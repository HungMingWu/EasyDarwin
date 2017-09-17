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
	return {};
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
		StreamInfo* theStreamInfo = broadcastSession->GetSourceInfo().GetStreamInfoByTrackID(theTrackID);
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

		MyRTPStream *newStream = nullptr;
		QTSS_Error theErr = fRTPSession->AddStream(*request, &newStream, qtssASFlagsForceUDPTransport);
		Assert(theErr == QTSS_NoErr);
		if (theErr != QTSS_NoErr)
		{
			// DeleteReflectorPushSession(inParams, theSession, foundSession);
			// return inParams.inRTSPRequest->SendErrorResponse(qtssClientBadRequest);
		}

		//send the setup response
		newStream->EnableSSRC();
		//newStream->SendSetupResponse(inRTSPRequest);

		broadcastSession->AddBroadcasterClientSession(fRTPSession.get());

#ifdef REFLECTORSESSION_DEBUG
		printf("QTSSReflectorModule.cpp:DoSetup Session =%p refcount=%"   _U32BITARG_   "\n", theSession->GetRef(), theSession->GetRef()->GetRefCount());
#endif
		return {};
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
