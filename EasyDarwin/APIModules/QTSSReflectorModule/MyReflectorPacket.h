#pragma once
#include <chrono>
#include <cstdint>
#include <vector>
class ReflectorSender;
class ReflectorSocket;
class RTPSessionOutput;

class MyReflectorPacket
{
public:
	MyReflectorPacket(const char *data, size_t len) : fPacket(data, data + len) {}
	~MyReflectorPacket() = default;
	bool  IsRTCP() { return fIsRTCP; }
private:
	std::chrono::high_resolution_clock::time_point fTimeArrived;
	std::vector<char> fPacket;
	bool      fIsRTCP{ false };
	bool      fNeededByOutput{ false }; // is this packet still needed for output?
	uint64_t  fStreamCountID{ 0 };

	friend bool IsKeyFrameFirstPacket(const MyReflectorPacket &thePacket);
	friend class ReflectorSender;
	friend class MyReflectorSender;
	friend class ReflectorSocket;
	friend class RTPSessionOutput;
	friend class MyReflectorSocket;
};

static inline bool IsKeyFrameFirstPacket(const MyReflectorPacket &thePacket)
{
	if (thePacket.fPacket.size() < 20) return false;

	uint8_t csrc_count = thePacket.fPacket[0] & 0x0f;
	uint32_t rtp_head_size = /*sizeof(struct RTPHeader)*/12 + csrc_count * sizeof(uint32_t);
	uint8_t nal_unit_type = thePacket.fPacket[rtp_head_size + 0] & 0x1F;
	if (nal_unit_type == 24)//STAP-A
	{
		if (thePacket.fPacket.size() > rtp_head_size + 3)
			nal_unit_type = thePacket.fPacket[rtp_head_size + 3] & 0x1F;
	}
	else if (nal_unit_type == 25)//STAP-B
	{
		if (thePacket.fPacket.size() > rtp_head_size + 5)
			nal_unit_type = thePacket.fPacket[rtp_head_size + 5] & 0x1F;
	}
	else if (nal_unit_type == 26)//MTAP16
	{
		if (thePacket.fPacket.size() > rtp_head_size + 8)
			nal_unit_type = thePacket.fPacket[rtp_head_size + 8] & 0x1F;
	}
	else if (nal_unit_type == 27)//MTAP24
	{
		if (thePacket.fPacket.size() > rtp_head_size + 9)
			nal_unit_type = thePacket.fPacket[rtp_head_size + 9] & 0x1F;
	}
	else if ((nal_unit_type == 28) || (nal_unit_type == 29))//FU-A/B
	{
		if (thePacket.fPacket.size() > rtp_head_size + 1)
		{
			uint8_t startBit = thePacket.fPacket[rtp_head_size + 1] & 0x80;
			if (startBit)
				nal_unit_type = thePacket.fPacket[rtp_head_size + 1] & 0x1F;
		}
	}

	return nal_unit_type == 5 || nal_unit_type == 7 || nal_unit_type == 8;
}