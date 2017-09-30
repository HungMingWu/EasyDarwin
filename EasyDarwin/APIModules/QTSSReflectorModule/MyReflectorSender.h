#pragma once
#include <cstdint>
#include <list>
#include <memory>
#include "MyReflectorPacket.h"
class MyReflectorStream;
class MyReflectorPacket;
class MyReflectorSender
{
	MyReflectorStream*    fStream;
	uint32_t              fWriteFlag;
	std::list<std::unique_ptr<MyReflectorPacket>> fPacketQueue;
	MyReflectorPacket*	fKeyFrameStartPacketElementPointer{ nullptr };
	bool fHasNewPackets{ false };
	friend class MyReflectorSocket;
public:
	MyReflectorSender(MyReflectorStream* inStream, uint32_t inWriteFlag);
	~MyReflectorSender() = default;
	void appendPacket(std::unique_ptr<MyReflectorPacket> thePacket);
};