#pragma once
#include <atomic>
#include "SDPSourceInfo.h"
#include "MyReflectorSender.h"
#include "UDPSocketPool.h"

class MyRTSPRequest;
class MyRTPSession;
class MyReflectorSession;
class MyReflectorSocket;

class MyReflectorStream {
	// All the necessary info about this stream
	StreamInfo  fStreamInfo;
	MyReflectorSession*	fMyReflectorSession;
	bool fEnableBuffer{ false };
	SocketPair<MyReflectorSocket>*      fSockets;
	QTSS_RTPTransportType fTransportType{ qtssRTPTransportTypeTCP };
	MyReflectorSender     fRTPSender;
	MyReflectorSender     fRTCPSender;
	std::atomic_size_t fBytesSentInThisInterval{ 0 };
	uint64_t                  fPacketCount;
	friend class MyReflectorSender;
	friend class MyReflectorSocket;
public:
	MyReflectorStream(StreamInfo* inInfo);
	~MyReflectorStream() = default;
	// Call this to initialize the reflector sockets. Uses the QTSS_RTSPRequestObject
	// if provided to report any errors that occur 
	// Passes the QTSS_ClientSessionObject to the socket so the socket can update the session if needed.
	QTSS_Error BindSockets(MyRTSPRequest &inRequest, MyRTPSession &inSession, uint32_t inReflectorSessionFlags, bool filterState, uint32_t timeout);
	void SetMyReflectorSession(MyReflectorSession* reflector) { fMyReflectorSession = reflector; }
	StreamInfo* GetStreamInfo() { return &fStreamInfo; }
	void SetEnableBuffer(bool enableBuffer) { fEnableBuffer = enableBuffer; }
	SocketPair<MyReflectorSocket>* GetSocketPair() { return fSockets; }
	void PushPacket(const char *packet, size_t packetLen, bool isRTCP);
	const StreamInfo& GetStreamInfo() const { return fStreamInfo; }
	MyReflectorSession* GetMyReflectorSession() { return fMyReflectorSession; }
};