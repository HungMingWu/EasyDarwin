/*
	Copyleft (c) 2012-2016 EasyDarwin.ORG.  All rights reserved.
	Github: https://github.com/EasyDarwin
	WEChat: EasyDarwin
	Website: http://www.EasyDarwin.org
*/
/*
	File:       HTTPSessionInterface.h
	Contains:
*/

#ifndef __HTTPSESSIONINTERFACE_H__
#define __HTTPSESSIONINTERFACE_H__

#include "RTSPRequestStream.h"
#include "RTSPResponseStream.h"
#include "Task.h"
#include "QTSS.h"
#include "QTSSDictionary.h"

class HTTPSessionInterface : public QTSSDictionary, public Task
{
public:

	static void	Initialize();

	HTTPSessionInterface();
	virtual ~HTTPSessionInterface();

	bool IsLiveSession() { return fSocket.IsConnected() && fLiveSession; }

	void RefreshTimeout() { fTimeoutTask.RefreshTimeout(); }

	void IncrementObjectHolderCount() { ++fObjectHolders; }
	void DecrementObjectHolderCount();

	RTSPRequestStream*  GetInputStream() { return &fInputStream; }
	RTSPResponseStream* GetOutputStream() { return &fOutputStream; }
	TCPSocket*          GetSocket() { return &fSocket; }
	OSMutex*            GetSessionMutex() { return &fSessionMutex; }

	uint32_t              GetSessionIndex() { return fSessionIndex; }

	void                SetRequestBodyLength(int32_t inLength) { fRequestBodyLen = inLength; }
	int32_t              GetRemainingReqBodyLen() { return fRequestBodyLen; }

	// QTSS STREAM FUNCTIONS

	// Allows non-buffered writes to the client. These will flow control.

	// THE FIRST ENTRY OF THE IOVEC MUST BE BLANK!!!
	virtual QTSS_Error WriteV(iovec* inVec, uint32_t inNumVectors, uint32_t inTotalLength, uint32_t* outLenWritten);
	virtual QTSS_Error Write(void* inBuffer, uint32_t inLength, uint32_t* outLenWritten, uint32_t inFlags);
	virtual QTSS_Error Read(void* ioBuffer, uint32_t inLength, uint32_t* outLenRead);
	virtual QTSS_Error RequestEvent(QTSS_EventType inEventMask);

	virtual QTSS_Error SendHTTPPacket(StrPtrLen* contentXML, bool connectionClose, bool decrement);

	enum
	{
		kMaxUserNameLen = 32,
		kMaxUserPasswordLen = 32
	};

protected:
	enum
	{
		kFirstHTTPSessionID = 1,    //uint32_t
	};

	//Each http session has a unique number that identifies it.

	char                fUserNameBuf[kMaxUserNameLen];
	char                fUserPasswordBuf[kMaxUserPasswordLen];

	TimeoutTask         fTimeoutTask;//allows the session to be timed out

	RTSPRequestStream   fInputStream;
	RTSPResponseStream  fOutputStream;

	// Any RTP session sending interleaved data on this RTSP session must
	// be prevented from writing while an RTSP request is in progress
	OSMutex             fSessionMutex;


	//+rt  socket we get from "accept()"
	TCPSocket           fSocket;
	TCPSocket*          fOutputSocketP;
	TCPSocket*          fInputSocketP;  // <-- usually same as fSocketP, unless we're HTTP Proxying

	void        SnarfInputSocket(HTTPSessionInterface* fromHTTPSession);

	bool              fLiveSession;
	unsigned int	fObjectHolders;

	uint32_t              fSessionIndex;
	uint32_t              fLocalAddr;
	uint32_t              fRemoteAddr;
	int32_t              fRequestBodyLen;

	uint16_t              fLocalPort;
	uint16_t              fRemotePort;

	bool				fAuthenticated;

	static unsigned int	sSessionIndexCounter;

	// Dictionary support Param retrieval function
	static void*        SetupParams(QTSSDictionary* inSession, uint32_t* outLen);

	static QTSSAttrInfoDict::AttrInfo   sAttributes[];
};
#endif // __HTTPSESSIONINTERFACE_H__

