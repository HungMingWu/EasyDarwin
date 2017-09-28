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

MyRTPStream::MyRTPStream(MyRTSPRequest& request, uint32_t inSSRC, MyRTPSession& inSession, QTSS_AddStreamFlags inFlags)
	: fStreamURL(request.GetFileName()),
	fTransportType(request.GetTransportType()),
	fNetworkMode(request.GetNetworkMode()),
	fSession(inSession),
	fSsrc(inSSRC)
{
	//
	// decide whether to overbuffer
	SetOverBufferState(request);

	// Check to see if this RTP stream should be sent over TCP.
	if (fTransportType == qtssRTPTransportTypeTCP)
	{
		fIsTCP = true;

		// If it is, get 2 channel numbers from the RTSP session.
		fRTPChannel = request.GetSession().GetTwoChannelNumbers(fSession.fRTSPSessionID);
		fRTCPChannel = fRTPChannel + 1;
	}
}

void MyRTPStream::SetOverBufferState(MyRTSPRequest& request)
{
	int32_t requestedOverBufferState = request.GetDynamicRateState();
	bool enableOverBuffer = false;

	switch (fTransportType)
	{
		case qtssRTPTransportTypeTCP:
		{
			enableOverBuffer = true; // default is on same as 4.0 and earlier. Allows tcp to compensate for falling behind from congestion or slow-start. 
			if (requestedOverBufferState == 0) // client specifically set to false
				enableOverBuffer = false;
		}
		break;

	}
}