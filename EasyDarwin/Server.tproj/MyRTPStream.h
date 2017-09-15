#pragma once
#include <stdint.h>
#include <string>
#include "QTSS.h"
class MyRTPSession;
class MyRTSPRequest;
class MyRTPStream {
	uint32_t      fSsrc;
	std::string   fStreamURL;
	float     fLateToleranceInSec{ 0 };
	bool      fIsTCP{ false };
	QTSS_RTPTransportType   fTransportType{ qtssRTPTransportTypeTCP };
	QTSS_RTPNetworkMode     fNetworkMode{ qtssRTPNetworkModeDefault };
	MyRTPSession& fSession;

	// If we are interleaving RTP data over the TCP connection,
	// these are channel numbers to use for RTP & RTCP
	uint8_t   fRTPChannel{ 0 };
	uint8_t   fRTCPChannel{ 0 };
	void SetOverBufferState(MyRTSPRequest* request);
public:
	MyRTPStream(uint32_t inSSRC, MyRTPSession& inSession);
	~MyRTPStream() = default;
	uint32_t GetSSRC() const { return fSsrc; }

	// Setup uses the info in the RTSPRequestInterface to associate
	// all the necessary resources, ports, sockets, etc, etc, with this
	// stream.
	QTSS_Error Setup(MyRTSPRequest* request, QTSS_AddStreamFlags inFlags);
};