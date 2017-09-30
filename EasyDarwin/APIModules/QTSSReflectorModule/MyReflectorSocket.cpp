#include "MyReflectorSocket.h"
#include "MyReflectorPacket.h"
#include "MyReflectorSender.h"
#include "MyReflectorStream.h"
#include "MyRTPSession.h"
#include "RTCPPacket.h"
#include "RTCPSRPacket.h"
#include "MyAssert.h"

void MyReflectorSocket::AddSender(MyReflectorSender* inSender)
{
	Assert(true == fDemuxer.RegisterTask(
	{ inSender->fStream->fStreamInfo.fSrcIPAddr, 0 }, inSender));
	fSenderQueue.push_back(inSender);
}

bool MyReflectorSocket::ProcessPacket(time_point now, std::unique_ptr<MyReflectorPacket> thePacket, uint32_t theRemoteAddr, uint16_t theRemotePort)
{
	thePacket->fIsRTCP = false;

	static constexpr auto kRefreshBroadcastSessionInterval = std::chrono::milliseconds(10000);
	if (fBroadcasterClientSession != nullptr) // alway refresh timeout even if we are filtering.
	{
		if (std::chrono::duration_cast<std::chrono::milliseconds>(now - fLastBroadcasterTimeOutRefresh) > kRefreshBroadcastSessionInterval)
		{
			fBroadcasterClientSession->RefreshTimeouts();
			fLastBroadcasterTimeOutRefresh = now;
		}
	}

	if (thePacket->fPacket.empty())
		return false;

	if (thePacket->IsRTCP())
	{
		//if this is a new RTCP packet, check to see if it is a sender report.
		//We should only reflect sender reports. Because RTCP packets can't have both
		//an SR & an RR, and because the SR & the RR must be the first packet in a
		//compound RTCP packet, all we have to do to determine this is look at the
		//packet type of the first packet in the compound packet.
		RTCPPacket theRTCPPacket;
		if ((!theRTCPPacket.ParsePacket((uint8_t*)&thePacket->fPacket[0], thePacket->fPacket.size())) ||
			(theRTCPPacket.GetPacketType() != RTCPSRPacket::kSRPacketType))
		{
			return true;
		}
	}

	// Find the appropriate ReflectorSender for this packet.
	MyReflectorSender* theSender = fDemuxer.GetTask({ theRemoteAddr, 0 });
	// If there is a generic sender for this socket, use it.
	if (theSender == nullptr)
		theSender = fDemuxer.GetTask({ 0, 0 });

	if (theSender == nullptr)
		return true;

	Assert(theSender != nullptr); // at this point we have a sender

	thePacket->fStreamCountID = ++(theSender->fStream->fPacketCount);
	thePacket->fTimeArrived = std::chrono::high_resolution_clock::now();
	theSender->appendPacket(std::move(thePacket));

	return false;
}