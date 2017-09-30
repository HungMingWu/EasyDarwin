#pragma once
#include <chrono>
#include <cstdint>
#include <memory>
#include "MyReflectorPacket.h"
#include "UDPSocketPool.h"
class MyRTPSession;
class MyReflectorSender;
class MyReflectorSocket {
	using time_point = std::chrono::high_resolution_clock::time_point;
	bool  fFilterSSRCs{ true };
	uint32_t  fTimeoutSecs{ 30 };
	MyRTPSession* fBroadcasterClientSession{ nullptr };
	time_point fLastBroadcasterTimeOutRefresh;
	SyncUnorderMap<MyReflectorSender*> fDemuxer;
	std::list<MyReflectorSender*> fSenderQueue;
public:
	MyReflectorSocket() = default;
	~MyReflectorSocket() = default;
	void SetSSRCFilter(bool state, uint32_t timeoutSecs) { fFilterSSRCs = state; fTimeoutSecs = timeoutSecs; }
	void AddSender(MyReflectorSender* inSender);
	uint16_t GetLocalPort() const { return 0; }
	void AddBroadcasterSession(MyRTPSession* inSession) { fBroadcasterClientSession = inSession; }
	void RemoveBroadcasterSession() { fBroadcasterClientSession = nullptr; }
	bool ProcessPacket(time_point now, std::unique_ptr<MyReflectorPacket> thePacket, uint32_t theRemoteAddr, uint16_t theRemotePort);
};