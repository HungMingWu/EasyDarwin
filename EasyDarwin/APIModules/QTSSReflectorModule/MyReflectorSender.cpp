#include "MyReflectorSender.h"
#include "MyReflectorPacket.h"
#include "MyReflectorStream.h"
#include "MyReflectorSession.h"
#include "QTSS.h"

enum class KeyFrameType : char {
	Video,
	Audio,
	None
};

static KeyFrameType needToUpdateKeyFrame(MyReflectorStream* stream,
	const MyReflectorPacket &thePacket)
{
	const auto &info = stream->GetStreamInfo();
	if (info->fPayloadType == qtssVideoPayloadType && info->fPayloadName == "H264/90000"
		&& IsKeyFrameFirstPacket(thePacket))
		return KeyFrameType::Video;
	if (info->fPayloadType == qtssAudioPayloadType && stream->GetMyReflectorSession()->HasVideoKeyFrameUpdate())
		return KeyFrameType::Audio;
	return KeyFrameType::None;
}

MyReflectorSender::MyReflectorSender(MyReflectorStream* inStream, uint32_t inWriteFlag)
	: fStream(inStream),
	fWriteFlag(inWriteFlag)
{
}

void MyReflectorSender::appendPacket(std::unique_ptr<MyReflectorPacket> thePacket)
{
	if (!thePacket->IsRTCP())
	{
		auto type = needToUpdateKeyFrame(fStream, *thePacket);
		if (needToUpdateKeyFrame(fStream, *thePacket) != KeyFrameType::None)
		{
			if (fKeyFrameStartPacketElementPointer)
				fKeyFrameStartPacketElementPointer->fNeededByOutput = false;

			thePacket->fNeededByOutput = true;
			fKeyFrameStartPacketElementPointer = thePacket.get();
			if (type == KeyFrameType::Video)
				fStream->GetMyReflectorSession()->SetHasVideoKeyFrameUpdate(true);
			else
				fStream->GetMyReflectorSession()->SetHasVideoKeyFrameUpdate(false);
		}
	}

	fHasNewPackets = true;

	if (!thePacket->IsRTCP())
	{
		// don't check for duplicate packets, they may be needed to keep in sync.
		// Because this is an RTP packet make sure to atomic add this because
		// multiple sockets can be adding to this variable simultaneously
		fStream->fBytesSentInThisInterval += thePacket->fPacket.size();
	}

	fPacketQueue.push_back(std::move(thePacket));
}
