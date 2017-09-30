#pragma once
#include <cstdint>
#include <atomic>
#include <iostream>
#include <string>
#include "QTSS.h"
class MyRTPSession;
class MyRTSPRequest;
class MyRTPStream {
	uint32_t      fSsrc;
	std::string   fStreamURL;
	bool      fIsTCP{ false };
	bool      fEnableSSRC{ false };
	QTSS_RTPTransportType   fTransportType{ qtssRTPTransportTypeTCP };
	QTSS_RTPNetworkMode     fNetworkMode{ qtssRTPNetworkModeDefault };
	MyRTPSession& fSession;
	// If we are interleaving RTP data over the TCP connection,
	// these are channel numbers to use for RTP & RTCP
	uint8_t   fRTPChannel{ 0 };
	uint8_t   fRTCPChannel{ 0 };
	void SetOverBufferState(MyRTSPRequest& request);
	friend std::ostream& operator << (std::ostream& stream, const MyRTPStream& RTPStream);
public:
	MyRTPStream(MyRTSPRequest& request, uint32_t inSSRC, MyRTPSession& inSession, QTSS_AddStreamFlags inFlags);
	~MyRTPStream() = default;
	uint32_t GetSSRC() const { return fSsrc; }

	void EnableSSRC() { fEnableSSRC = true; }
	uint8_t GetRTPChannelNum() const { return fRTPChannel; }
	uint8_t GetRTCPChannelNum() const { return fRTCPChannel; }
	std::string GetStreamURL() const { return fStreamURL; }
};