#include "MyReflectorStream.h"
#include "MyRTSPRequest.h"
#include "MyReflectorSession.h"
#include "ReflectorStream.h"
#include "MyReflectorSocket.h"

MyReflectorStream::MyReflectorStream(StreamInfo* inInfo)
	: fStreamInfo(*inInfo),
	fRTPSender(this, qtssWriteFlagsIsRTP),
	fRTCPSender(this, qtssWriteFlagsIsRTCP)
{
}

QTSS_Error MyReflectorStream::BindSockets(MyRTSPRequest &inRequest, MyRTPSession &inSession, uint32_t inReflectorSessionFlags, bool filterState, uint32_t timeout)
{
	// If the incoming data is RTSP interleaved, we don't need to do anything here
	if (inReflectorSessionFlags & MyReflectorSession::kIsPushSession)
		fStreamInfo.fSetupToReceive = true;

	// Set the transport Type a Broadcaster
	fTransportType = inRequest.GetTransportType();

	// get a pair of sockets. The socket must be bound on INADDR_ANY because we don't know
	// which interface has access to this broadcast. If there is a source IP address
	// specified by the source info, we can use that to demultiplex separate broadcasts on
	// the same port. If the src IP addr is 0, we cannot do this and must dedicate 1 port per
	// broadcast

	if (qtssRTPTransportTypeTCP == fTransportType)
	{
		fSockets = new SocketPair<MyReflectorSocket>();
	}
	else
	{
#if 0
		// changing INADDR_ANY to fStreamInfo.fDestIPAddr to deal with NATs (need to track this change though)
		// change submitted by denis@berlin.ccc.de

		bool isMulticastDest = (SocketUtils::IsMulticastIPAddr(fStreamInfo.fDestIPAddr));

		if (isMulticastDest)
		{
			fSockets = sSocketPool.GetUDPSocketPair(INADDR_ANY, fStreamInfo.fPort, fStreamInfo.fSrcIPAddr, 0);
		}
		else
		{
			fSockets = sSocketPool.GetUDPSocketPair(fStreamInfo.fDestIPAddr, fStreamInfo.fPort, fStreamInfo.fSrcIPAddr, 0);
		}

		if ((fSockets == nullptr) && fStreamInfo.fSetupToReceive)
		{
			fStreamInfo.fPort = 0;
			if (isMulticastDest)
			{
				fSockets = sSocketPool.GetUDPSocketPair(INADDR_ANY, fStreamInfo.fPort, fStreamInfo.fSrcIPAddr, 0);
			}
			else
			{
				fSockets = sSocketPool.GetUDPSocketPair(fStreamInfo.fDestIPAddr, fStreamInfo.fPort, fStreamInfo.fSrcIPAddr, 0);
			}
		}
#endif
	}

#if 0
	if (fSockets == nullptr)
		return inRequest->SendErrorResponse(qtssServerInternal);

	// If we know the source IP address of this broadcast, we can demux incoming traffic
	// on the same port by that source IP address. If we don't know the source IP addr,
	// it is impossible for us to demux, and therefore we shouldn't allow multiple
	// broadcasts on the same port.
	if (fSockets->GetSocketA()->HasSender() && (fStreamInfo.fSrcIPAddr == 0))
		return inRequest->SendErrorResponse(qtssServerInternal);
#endif

	//also put this stream onto the socket's queue of streams
	fSockets->GetSocketA()->AddSender(&fRTPSender);
	fSockets->GetSocketB()->AddSender(&fRTCPSender);

	// A broadcaster is setting up a UDP session so let the sockets update the session
	if (fStreamInfo.fSetupToReceive &&  qtssRTPTransportTypeUDP == fTransportType)
	{
		//fSockets->GetSocketA()->AddBroadcasterSession(inSession);
		//fSockets->GetSocketB()->AddBroadcasterSession(inSession);
	}

	fSockets->GetSocketA()->SetSSRCFilter(filterState, timeout);
	fSockets->GetSocketB()->SetSSRCFilter(filterState, timeout);

#if 0
	// Always set the Rcv buf size for the sockets. This is important because the
	// server is going to be getting many packets on these sockets.
	if (qtssRTPTransportTypeUDP == fTransportType)
	{
		fSockets->GetSocketA()->SetSocketRcvBufSize(1024 * 1024);
		fSockets->GetSocketB()->SetSocketRcvBufSize(1024 * 1024);
	}
#endif

	//If the broadcaster is sending RTP directly to us, we don't
	//need to join a multicast group because we're not using multicast
#if 0
	if (isMulticastDest)
	{
		QTSS_Error err = fSockets->GetSocketA()->JoinMulticast(fStreamInfo.fDestIPAddr);
		if (err == QTSS_NoErr)
			err = fSockets->GetSocketB()->JoinMulticast(fStreamInfo.fDestIPAddr);
		// If we get an error when setting the TTL, this isn't too important (TTL on
		// these sockets is only useful for RTCP RRs.
		if (err == QTSS_NoErr)
			(void)fSockets->GetSocketA()->SetTtl(fStreamInfo.fTimeToLive);
		if (err == QTSS_NoErr)
			(void)fSockets->GetSocketB()->SetTtl(fStreamInfo.fTimeToLive);

		if (err != QTSS_NoErr)
			return inRequest->SendErrorResponse(qtssServerInternal);
	}
#endif

	// If the port is 0, update the port to be the actual port value
	fStreamInfo.fPort = fSockets->GetSocketA()->GetLocalPort();

#if 0
	//finally, register these sockets for events
	if (qtssRTPTransportTypeUDP == fTransportType)
	{
		fSockets->GetSocketA()->RequestEvent(EV_RE);
		fSockets->GetSocketB()->RequestEvent(EV_RE);
	}
#endif

	return QTSS_NoErr;
}

void MyReflectorStream::PushPacket(const char *packet, size_t packetLen, bool isRTCP)
{

}