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
	}

	//also put this stream onto the socket's queue of streams
	fSockets->GetSocketA()->AddSender(&fRTPSender);
	fSockets->GetSocketB()->AddSender(&fRTCPSender);

	fSockets->GetSocketA()->SetSSRCFilter(filterState, timeout);
	fSockets->GetSocketB()->SetSSRCFilter(filterState, timeout);

	//If the broadcaster is sending RTP directly to us, we don't
	//need to join a multicast group because we're not using multicast

	// If the port is 0, update the port to be the actual port value
	fStreamInfo.fPort = fSockets->GetSocketA()->GetLocalPort();

	return QTSS_NoErr;
}

void MyReflectorStream::PushPacket(const char *packet, size_t packetLen, bool isRTCP)
{
	if (packetLen > 0)
	{
		auto thePacket = std::make_unique<MyReflectorPacket>(packet, packetLen);
		if (isRTCP)
		{
			//printf("ReflectorStream::PushPacket RTCP packetlen = %"   _U32BITARG_   "\n",packetLen);
			fSockets->GetSocketB()->ProcessPacket(std::chrono::high_resolution_clock::now(), std::move(thePacket), 0, 0);
		}
		else
		{
			fSockets->GetSocketA()->ProcessPacket(std::chrono::high_resolution_clock::now(), std::move(thePacket), 0, 0);
		}
	}
}