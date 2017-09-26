/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2008 Apple Inc.  All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 *
 */
 /*
	 File:       RTSPSessionInterface.h

	 Contains:   Presents an API for session-wide resources for modules to use.
				 Implements the RTSP Session dictionary for QTSS API.


 */

#ifndef __RTSPSESSIONINTERFACE_H__
#define __RTSPSESSIONINTERFACE_H__

#include <vector>
#include <string>
#include "RTSPRequestStream.h"
#include "RTSPResponseStream.h"
#include "Task.h"
#include "QTSS.h"
#include "atomic.h"

class RTSPSessionInterface : public Task
{
public:
	RTSPSessionInterface();
	~RTSPSessionInterface() override = default;

	//Is this session alive? If this returns false, clean up and begone as
	//fast as possible
	bool IsLiveSession() { return fSocket.IsConnected() && fLiveSession; }

	// Allows clients to refresh the timeout
	void RefreshTimeout() { fTimeoutTask.RefreshTimeout(); }

	// In order to facilitate sending out of band data on the RTSP connection,
	// other objects need to have direct pointer access to this object. But,
	// because this object is a task object it can go away at any time. If # of
	// object holders is > 0, the RTSPSession will NEVER go away. However,
	// the object managing the session should be aware that if IsLiveSession returns
	// false it may be wise to relinquish control of the session
	void IncrementObjectHolderCount() { /*(void)atomic_add(&fObjectHolders, 1);*/ ++fObjectHolders; }
	void DecrementObjectHolderCount();

	// If RTP data is interleaved into the RTSP connection, we need to associate
	// 2 unique channel numbers with each RTP stream, one for RTP and one for RTCP.
	// This function allocates 2 channel numbers, returns the lower one. The other one
	// is implicitly 1 greater.
	//
	// Pass in the RTSP Session ID of the Client session to which these channel numbers will
	// belong.
	uint8_t               GetTwoChannelNumbers(boost::string_view inRTSPSessionID);

	//
	// Given a channel number, returns the RTSP Session ID to which this channel number refers
	boost::string_view  GetSessionIDForChannelNum(uint8_t inChannelNum);

	//Two main things are persistent through the course of a session, not
	//associated with any one request. The RequestStream (which can be used for
	//getting data from the client), and the socket. OOps, and the ResponseStream
	RTSPRequestStream*  GetInputStream() { return &fInputStream; }
	RTSPResponseStream* GetOutputStream() { return &fOutputStream; }
	TCPSocket*          GetSocket() { return &fSocket; }
	OSMutex*            GetSessionMutex() { return &fSessionMutex; }

	uint32_t              GetSessionID() { return fSessionID; }

	// Request Body Length
	// This object can enforce a length of the request body to prevent callers
	// of Read() from overrunning the request body and going into the next request.
	// -1 is an unknown request body length. If the body length is unknown,
	// this object will do no length enforcement. 
	void                SetRequestBodyLength(int32_t inLength) { fRequestBodyLen = inLength; }
	int32_t              GetRemainingReqBodyLen() { return fRequestBodyLen; }

	// QTSS STREAM FUNCTIONS

	// Allows non-buffered writes to the client. These will flow control.

	// THE FIRST ENTRY OF THE IOVEC MUST BE BLANK!!!
	QTSS_Error WriteV(iovec* inVec, uint32_t inNumVectors, uint32_t inTotalLength, uint32_t* outLenWritten);
	QTSS_Error Write(void* inBuffer, uint32_t inLength, uint32_t* outLenWritten, uint32_t inFlags);
	QTSS_Error Read(void* ioBuffer, uint32_t inLength, uint32_t* outLenRead);
	QTSS_Error RequestEvent(QTSS_EventType inEventMask);

	// performs RTP over RTSP
	QTSS_Error  InterleavedWrite(const std::vector<char> &inBuffer, uint32_t* outLenWritten, unsigned char channel);

	std::string GetRemoteAddr();
protected:
	enum
	{
		kFirstRTSPSessionID = 1,    //uint32_t
	};
	//Each RTSP session has a unique number that identifies it.

	TimeoutTask         fTimeoutTask;//allows the session to be timed out

	RTSPRequestStream   fInputStream;
	RTSPResponseStream  fOutputStream;

	// Any RTP session sending interleaved data on this RTSP session must
	// be prevented from writing while an RTSP request is in progress
	OSMutex             fSessionMutex;

	// for coalescing small interleaved writes into a single TCP frame
	enum
	{
		kTCPCoalesceBufferSize = 1450 //1450 is the max data space in an TCP segment over ent
		, kTCPCoalesceDirectWriteSize = 0 // if > this # bytes bypass coalescing and make a direct write
		, kInteleaveHeaderSize = 4  // '$ '+ 1 byte ch ID + 2 bytes length
	};
	std::vector<char>       fTCPCoalesceBuffer;


	//+rt  socket we get from "accept()"
	TCPSocket           fSocket;

	// What session type are we?
	QTSS_RTSPSessionType    fSessionType{qtssRTSPSession};
	bool              fLiveSession{true};
	unsigned int        fObjectHolders{0};

	std::vector<std::string> fChNumToSessIDMap;

	uint32_t              fSessionID;
	int32_t              fRequestBodyLen{-1};

	static unsigned int sSessionIDCounter;
};
#endif // __RTSPSESSIONINTERFACE_H__

