#include <random>
#include "MyRTSPSession.h"
#include "RTSPServer.h"
#include "sdpCache.h"
#include "SDPSourceInfo.h"
#include "MyRTSPRequest.h"
#include "MyRTPSession.h"
#include "MyRTPStream.h"
#include "MyReflectorSession.h"
#include "MyAssert.h"

static std::string GenerateNewSessionID()
{
	std::random_device rd;
	std::mt19937 mt(rd());
	std::uniform_int_distribution<int64_t> dist;

	return std::to_string(dist(mt));
}

MyRTSPSession::MyRTSPSession(RTSPServer &server, std::shared_ptr<Connection> connection) noexcept
	: mServer(server), connection(std::move(connection))
{
	try {
		//auto remote_endpoint = connection->socket.lowest_layer().remote_endpoint();
		//request = std::make_shared<RTSPRequest1>(remote_endpoint.address().to_string(), remote_endpoint.port());
		request = std::make_shared<MyRTSPRequest>(*this);
	}
	catch (...) {
		request = std::make_shared<MyRTSPRequest>(*this);
	}
}

std::shared_ptr<MyReflectorSession> MyRTSPSession::CreateSession(boost::string_view sessionName)
{
	std::lock_guard<std::mutex> lock(mServer.session_mutex);
	auto it = mServer.sessionMap.find(std::string(sessionName));
	if (it == mServer.sessionMap.end()) {
		boost::string_view theFileData = CSdpCache::GetInstance()->getSdpMap(sessionName);

		if (theFileData.empty())
			return nullptr;

		SDPSourceInfo theInfo(theFileData); // will make a copy

		if (!theInfo.IsReflectable())
		{
			return nullptr;
		}

		//
		// Setup a ReflectorSession and bind the sockets. If we are negotiating,
		// make sure to let the session know that this is a Push Session so
		// ports may be modified.
		uint32_t theSetupFlag = MyReflectorSession::kMarkSetup | MyReflectorSession::kIsPushSession;

		auto theSession = std::make_shared<MyReflectorSession>(sessionName, theInfo);

		QTSS_Error theErr = theSession->SetupReflectorSession(*request, *fRTPSession, theSetupFlag);
		if (theErr != QTSS_NoErr)
		{
			//delete theSession;
			//CSdpCache::GetInstance()->eraseSdpMap(theSession->GetStreamName());
			//theSession->StopTimer();
			return nullptr;
		}
		mServer.sessionMap.insert(std::make_pair(std::string(sessionName), theSession));
		return theSession;
	}
	return it->second;
}

std::error_code MyRTSPSession::do_setup()
{
	bool isPush = request->IsPushRequest();
	if (!rtp_OutputSession)
	{
		if (!isPush)
		{
#if 0
			theSession = DoSessionSetup(inParams, isPush);
			if (theSession == nullptr)
				return QTSS_RequestFailed;
			auto* theNewOutput = new RTPSessionOutput(inParams->inClientSession, theSession, sStreamCookieName);
			theSession->AddOutput(theNewOutput, true);
			inParams->inClientSession->addAttribute(sOutputName, theNewOutput);
#endif
		}
		else
		{
			if (!broadcastSession)
			{
				broadcastSession = CreateSession(request->GetFileName());
				//if (theSession == nullptr)
				//					return QTSS_RequestFailed;
			}
		}
	}
	else
	{
		//auto theSession = outputSession.value().GetReflectorSession();
	}

	//unless there is a digit at the end of this path (representing trackID), don't
	//even bother with the request
	std::string theDigitStr = request->GetFileDigit();
	if (theDigitStr.empty())
	{
		//if (isPush)
			//DeleteReflectorPushSession(inParams, theSession, foundSession);
		//return inParams.inRTSPRequest->SendErrorResponse(qtssClientBadRequest);
	}

	uint32_t theTrackID = std::stoi(theDigitStr);

	if (isPush)
	{
		//printf("QTSSReflectorModule.cpp:DoSetup is push setup\n");
		// Get info about this trackID
		const StreamInfo* theStreamInfo = broadcastSession->GetSourceInfo().GetStreamInfoByTrackID(theTrackID);
		// If theStreamInfo is NULL, we don't have a legit track, so return an error
		if (theStreamInfo == nullptr)
		{
			//DeleteReflectorPushSession(inParams, theSession, foundSession);
			//return inParams.inRTSPRequest->SendErrorResponse(qtssClientBadRequest);
		}

		if (theStreamInfo->fSetupToReceive)
		{
			//DeleteReflectorPushSession(inParams, theSession, foundSession);
			//return inParams.inRTSPRequest->SendErrorResponse(qtssPreconditionFailed);
		}

		request->SetUpServerPort(theStreamInfo->fPort);

		fRTPSession->AddStream(*request, qtssASFlagsForceUDPTransport);

		auto &newStream = fRTPSession->GetStreams().back();
		//send the setup response
		newStream.EnableSSRC();
		//newStream->SendSetupResponse(inRTSPRequest);

		broadcastSession->AddBroadcasterClientSession(fRTPSession.get());

#ifdef REFLECTORSESSION_DEBUG
		printf("QTSSReflectorModule.cpp:DoSetup Session =%p refcount=%"   _U32BITARG_   "\n", theSession->GetRef(), theSession->GetRef()->GetRefCount());
#endif
		return {};
	}
	return {};
}

std::error_code MyRTSPSession::do_play(MyReflectorSession *session)
{
	if (session == nullptr) {
		if (!broadcastSession) return {};
		session = broadcastSession.get();
	}
	else {

	}
	return {};
}

std::error_code MyRTSPSession::process_rtppacket(const char *packetData, size_t length)
{
	const SDPSourceInfo& theSoureInfo = broadcastSession->GetSourceInfo();
	uint32_t  numStreams = theSoureInfo.GetNumStreams();
	//printf("QTSSReflectorModule.cpp:ProcessRTPData numStreams=%"   _U32BITARG_   "\n",numStreams);

	/*
	  Stream data such as RTP packets is encapsulated by an ASCII dollar
	  sign (24 hexadecimal), followed by a one-byte channel identifier,
	  followed by the length of the encapsulated binary data as a binary,
	  two-byte integer in network byte order. The stream data follows
	  immediately afterwards, without a CRLF, but including the upper-layer
	  protocol headers. Each $ block contains exactly one upper-layer
	  protocol data unit, e.g., one RTP packet.
	*/
	uint8_t packetChannel = (uint8_t)packetData[1];

	uint16_t  packetDataLen;
	memcpy(&packetDataLen, &packetData[2], 2);
	packetDataLen = ntohs(packetDataLen);

	char*   rtpPacket = (char *)&packetData[4];

	uint32_t inIndex = packetChannel / 2; // one stream per every 2 channels rtcp channel handled below
	if (inIndex < numStreams)
	{
		MyReflectorStream& theStream = broadcastSession->GetStreamByIndex(inIndex);
		//if (theStream == nullptr) return QTSS_Unimplemented;

		StreamInfo* theStreamInfo = theStream.GetStreamInfo();
		uint16_t serverReceivePort = theStreamInfo->fPort;

		bool isRTCP = false;
		if (packetChannel & 1)
		{
			serverReceivePort++;
			isRTCP = true;
		}
		theStream.PushPacket(rtpPacket, packetDataLen, isRTCP);
	}

	return {};
}

void MyRTSPSession::FindOrCreateRTPSession()
{
	// This function attempts to locate the appropriate RTP session for this RTSP
	// Request. It uses an RTSP session ID as a key to finding the correct RTP session,
	// and it looks for this session ID in two places. First, the RTSP session ID header
	// in the RTSP request, and if there isn't one there, in the RTSP session object itself.
	auto it = request->header.find("Session");
	if (it != end(request->header)) {
		std::string theSessionID = it->second;
		std::lock_guard<std::mutex> lock(mServer.rtp_mutex);
		fRTPSession = mServer.rtpMap[theSessionID];
	}
	else {
		fRTPSession = std::make_shared<MyRTPSession>();
		std::lock_guard<std::mutex> lock(mServer.rtp_mutex);
		while (true)
		{
			fSessionID = GenerateNewSessionID();
			auto it = mServer.rtpMap.find(fSessionID);
			if (it == end(mServer.rtpMap)) break;
		}
		mServer.rtpMap[fSessionID] = fRTPSession;
	}
	return;
}

uint8_t MyRTSPSession::GetTwoChannelNumbers(boost::string_view inRTSPSessionID)
{
	// Allocate 2 channel numbers
	// Put this sessionID to the proper place in the map
	fChNumToSessIDMap.emplace_back(inRTSPSessionID);
	return (fChNumToSessIDMap.size() - 1) * 2;
}
