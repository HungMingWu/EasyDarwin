#pragma once
#include <cstdint>
class MyRTPSession;
class MyReflectorSender;
class MyReflectorSocket {
	bool  fFilterSSRCs{ true };
	uint32_t  fTimeoutSecs{ 30 };
	MyRTPSession* fBroadcasterClientSession{ nullptr };
public:
	MyReflectorSocket() = default;
	~MyReflectorSocket() = default;
	void SetSSRCFilter(bool state, uint32_t timeoutSecs) { fFilterSSRCs = state; fTimeoutSecs = timeoutSecs; }
	void AddSender(MyReflectorSender* inSender);
	uint16_t GetLocalPort() const { return 0; }
	void AddBroadcasterSession(MyRTPSession* inSession) { fBroadcasterClientSession = inSession; }
	void RemoveBroadcasterSession() { fBroadcasterClientSession = nullptr; }
};