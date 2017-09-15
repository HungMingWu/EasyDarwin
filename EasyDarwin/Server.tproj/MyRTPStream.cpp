#include "MyRTPStream.h"
#include "MyRTSPRequest.h"
#include "MyRTSPSession.h"
#include "MyRTPSession.h"
#include "ServerPrefs.h"

std::ostream& operator << (std::ostream& stream, const MyRTPStream& RTPStream)
{
	// This function appends a session header to the SETUP response, and
	// checks to see if it is a 304 Not Modified. If it is, it sends the entire
	// response and returns an error
#if 0
	if (ServerPrefs::GetRTSPTimeoutInSecs() > 0)  // adv the timeout
		inRequest->AppendSessionHeaderWithTimeout(RTPStream.fSession.fRTSPSessionID, std::to_string(ServerPrefs::GetRTSPTimeoutInSecs()));
	else
		inRequest->AppendSessionHeaderWithTimeout(RTPStream.fSession.fRTSPSessionID, {}); // no timeout in resp.
#endif

	std::string ssrcStr = (RTPStream.fEnableSSRC) ? std::to_string(RTPStream.fSsrc) : std::string();

	// We are either going to append the RTP / RTCP port numbers (UDP),
	// or the channel numbers (TCP, interleaved)
	if (!RTPStream.fIsTCP)
	{
#if 0
		//
		// With UDP retransmits its important the client starts sending RTCPs
		// to the right address right away. The sure-firest way to get the client
		// to do this is to put the src address in the transport. So now we do that always.
		//
		boost::string_view theSrcIPAddress = ServerPrefs::GetTransportSrcAddr();
		if (theSrcIPAddress.empty()) {
			//StrPtrLen *p = fSockets->GetSocketA()->GetLocalAddrStr();
			theSrcIPAddress = std::string("127.0.0.1");
		}


		if (request->IsPushRequest())
		{
			std::string rtpPort = std::to_string(request->GetSetUpServerPort());
			std::string rtcpPort = std::to_string(request->GetSetUpServerPort() + 1);
			request->AppendTransportHeader(rtpPort, rtcpPort, {}, {}, theSrcIPAddress, ssrcStr);
		}
		else
		{
			// Append UDP socket port numbers.
			UDPSocket* theRTPSocket = fSockets->GetSocketA();
			UDPSocket* theRTCPSocket = fSockets->GetSocketB();
			StrPtrLen *p = theRTPSocket->GetLocalPortStr();
			StrPtrLen *p1 = theRTCPSocket->GetLocalPortStr();
			request->AppendTransportHeader(std::string(p->Ptr, p->Len),
				std::string(p1->Ptr, p1->Len), {}, {}, theSrcIPAddress, ssrcStr);
		}
#endif
	}
	else
	{
		// If these channel numbers fall outside prebuilt range, we will have to call sprintf.
		std::string rtpChannel = std::to_string(RTPStream.fRTPChannel);
		std::string rtcpChannel = std::to_string(RTPStream.fRTCPChannel);
		//request->AppendTransportHeader({}, {}, rtpChannel, rtcpChannel, {}, ssrcStr);
	}
	return stream;
}

MyRTPStream::MyRTPStream(uint32_t inSSRC, MyRTPSession& inSession)
	: fSession(inSession),
	fSsrc(inSSRC)
{

}

void MyRTPStream::SetOverBufferState(MyRTSPRequest& request)
{
	int32_t requestedOverBufferState = request.GetDynamicRateState();
	bool enableOverBuffer = false;

	switch (fTransportType)
	{
		case qtssRTPTransportTypeReliableUDP:
		{
			enableOverBuffer = true; // default is on
			if (requestedOverBufferState == 0) // client specifically set to false
				enableOverBuffer = false;
		}
		break;

		case qtssRTPTransportTypeUDP:
		{
			enableOverBuffer = false; // always off
		}
		break;

		case qtssRTPTransportTypeTCP:
		{
			enableOverBuffer = true; // default is on same as 4.0 and earlier. Allows tcp to compensate for falling behind from congestion or slow-start. 
			if (requestedOverBufferState == 0) // client specifically set to false
				enableOverBuffer = false;
		}
		break;

	}

	//over buffering is enabled for the session by default
	//if any stream turns it off then it is off for all streams
	//a disable is from either the stream type default or a specific rtsp command to disable
	if (!enableOverBuffer)
		fSession.fOverbufferWindow.TurnOverbuffering(false);
}

QTSS_Error MyRTPStream::Setup(MyRTSPRequest& request, QTSS_AddStreamFlags inFlags)
{
	//Get the URL for this track
	fStreamURL = request.GetFileName();//just in case someone wants to use string routines

	//
	// Store the late-tolerance value that came out of hte x-RTP-Options header,
	// so that when it comes time to determine our thinning params (when we PLAY),
	// we will know this
	fLateToleranceInSec = request.GetLateToleranceInSec();
	if (fLateToleranceInSec == -1.0)
		fLateToleranceInSec = 1.5;

	//
	// Setup the transport type
	fTransportType = request.GetTransportType();
	fNetworkMode = request.GetNetworkMode();

	//
	// Check to see if caller is forcing raw UDP transport
	if ((fTransportType == qtssRTPTransportTypeReliableUDP) && (inFlags & qtssASFlagsForceUDPTransport))
		fTransportType = qtssRTPTransportTypeUDP;

	//
	// decide whether to overbuffer
	SetOverBufferState(request);

	// Check to see if this RTP stream should be sent over TCP.
	if (fTransportType == qtssRTPTransportTypeTCP)
	{
		fIsTCP = true;
		fSession.fOverbufferWindow.SetWindowSize(UINT32_MAX);

		// If it is, get 2 channel numbers from the RTSP session.
		fRTPChannel = request.GetSession().GetTwoChannelNumbers(fSession.fRTSPSessionID);
		fRTCPChannel = fRTPChannel + 1;

		// If we are interleaving, this is all we need to do to setup.
		return QTSS_NoErr;
	}
#if 0
	//
	// This track is not interleaved, so let the session know that all
	// tracks are not interleaved. This affects our scheduling of packets
	fSession->SetAllTracksInterleaved(false);

	//Get and store the remote addresses provided by the client. The remote addr is the
	//same as the RTSP client's IP address, unless an alternate was specified in the
	//transport header.
	fRemoteAddr = request->GetSession()->GetSocket()->GetRemoteAddr();
	if (request->GetDestAddr() != INADDR_ANY)
	{
		// Sending data to other addresses could be used in malicious ways, therefore
		// it is up to the module as to whether this sort of request might be allowed
		if (!(inFlags & qtssASFlagsAllowDestination))
			return request->SendErrorResponse(qtssClientBadRequest);
		fRemoteAddr = request->GetDestAddr();
	}
	fRemoteRTPPort = request->GetClientPortA();
	fRemoteRTCPPort = request->GetClientPortB();

	if ((fRemoteRTPPort == 0) || (fRemoteRTCPPort == 0))
		return request->SendErrorResponse(qtssClientBadRequest);

	//make sure that the client is advertising an even-numbered RTP port,
	//and that the RTCP port is actually one greater than the RTP port
	if ((fRemoteRTPPort & 1) != 0)
		return request->SendErrorResponse(qtssClientBadRequest);

	// comment out check below. This allows the rtcp port to be non-contiguous with the rtp port.
	//   if (fRemoteRTCPPort != (fRemoteRTPPort + 1))
	//       return QTSSModuleUtils::SendErrorResponse(request, qtssClientBadRequest, qtssMsgRTCPPortMustBeOneBigger);       

	// Find the right source address for this stream. If it isn't specified in the
	// RTSP request, assume it is the same interface as for the RTSP request.
	uint32_t sourceAddr = request->GetSession()->GetSocket()->GetLocalAddr();
	if ((request->GetSourceAddr() != INADDR_ANY) && (SocketUtils::IsLocalIPAddr(request->GetSourceAddr())))
		sourceAddr = request->GetSourceAddr();

	// if the transport is TCP or RUDP, then we only want one session quality level instead of a per stream one
	if (fTransportType != qtssRTPTransportTypeUDP)
	{
		this->SetQualityLevel(*(fSession->GetQualityLevelPtr()));
	}


	// If the destination address is multicast, we need to setup multicast socket options
	// on the sockets. Because these options may be different for each stream, we need
	// a dedicated set of sockets
	if (SocketUtils::IsMulticastIPAddr(fRemoteAddr))
	{
		fSockets = getSingleton()->GetSocketPool()->CreateUDPSocketPair(sourceAddr, 0);

		if (fSockets != nullptr)
		{
			//Set options on both sockets. Not really sure why we need to specify an
			//outgoing interface, because these sockets are already bound to an interface!
			QTSS_Error err = fSockets->GetSocketA()->SetTtl(request->GetTtl());
			if (err == QTSS_NoErr)
				err = fSockets->GetSocketB()->SetTtl(request->GetTtl());
			if (err == QTSS_NoErr)
				err = fSockets->GetSocketA()->SetMulticastInterface(fSockets->GetSocketA()->GetLocalAddr());
			if (err == QTSS_NoErr)
				err = fSockets->GetSocketB()->SetMulticastInterface(fSockets->GetSocketB()->GetLocalAddr());
			if (err != QTSS_NoErr)
				return request->SendErrorResponse(qtssServerInternal);
		}
	}
	else
		fSockets = getSingleton()->GetSocketPool()->GetUDPSocketPair(sourceAddr, 0, fRemoteAddr,
			fRemoteRTCPPort);

	if (fSockets == nullptr)
		return request->SendErrorResponse(qtssServerInternal);

	else if (fTransportType == qtssRTPTransportTypeReliableUDP)
	{
		//
		// FIXME - we probably want to get rid of this slow start flag in the API
		bool useSlowStart = !(inFlags & qtssASFlagsDontUseSlowStart);
		if (!ServerPrefs::IsSlowStartEnabled())
			useSlowStart = false;

		fTracker = fSession->GetBandwidthTracker();

		fResender.SetBandwidthTracker(fTracker);
		fResender.SetDestination(fSockets->GetSocketA(), fRemoteAddr, fRemoteRTPPort);

#if RTP_PACKET_RESENDER_DEBUGGING
		if (QTSServerInterface::GetServer()->GetPrefs()->IsAckLoggingEnabled())
		{
			char        url[256];
			char        logfile[256];
			sprintf(logfile, "resend_log_%"   _U32BITARG_   "", fSession->GetRTSPSession()->GetSessionID());
			StrPtrLen   logName(logfile);
			fResender.SetLog(&logName);

			StrPtrLen   *presoURL = fSession->GetValue(qtssCliSesPresentationURL);
			uint32_t      clientAddr = request->GetSession()->GetSocket()->GetRemoteAddr();
			memcpy(url, presoURL->Ptr, presoURL->Len);
			url[presoURL->Len] = 0;
			printf("RTPStream::Setup for %s will use ACKS, ip addr: %li.%li.%li.%li\n", url, (clientAddr & 0xff000000) >> 24
				, (clientAddr & 0x00ff0000) >> 16
				, (clientAddr & 0x0000ff00) >> 8
				, (clientAddr & 0x000000ff)
			);
		}
#endif
	}

	//
	// Record the Server RTP port
	fLocalRTPPort = fSockets->GetSocketA()->GetLocalPort();

	//finally, register with the demuxer to get RTCP packets from the proper address
	Assert(true == fSockets->GetSocketBDemux().RegisterTask({ fRemoteAddr, fRemoteRTCPPort }, this));
#endif
	return QTSS_NoErr;
}
