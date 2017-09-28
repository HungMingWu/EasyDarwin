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
	Contains:   API interface for objects to use to get access to attributes,
				data items, whatever, specific to RTP sessions (see RTPSession.h
				for more details on what that object is). This object
				implements the RTP Session Dictionary.



*/


#ifndef _RTPSESSIONINTERFACE_H_
#define _RTPSESSIONINTERFACE_H_

#include <vector>

#include "RTSPSessionInterface.h"
#include "TimeoutTask.h"
#include "Task.h"
#include "QTSServerInterface.h"
#include "OSMutex.h"
#include "RTPStream.h"

class RTSPRequestInterface;


class RTPSessionInterface : public Task
{
public:

	//
	// CONSTRUCTOR / DESTRUCTOR

	RTPSessionInterface();
	~RTPSessionInterface() override
	{
		if (fRTSPSession != nullptr)
			fRTSPSession->DecrementObjectHolderCount();
	}

	//Timeouts. This allows clients to refresh the timeout on this session
	void    RefreshTimeout() { fTimeoutTask.RefreshTimeout(); }
	void    RefreshRTSPTimeout() { if (fRTSPSession != nullptr) fRTSPSession->RefreshTimeout(); }
	void    RefreshTimeouts() { RefreshTimeout(); RefreshRTSPTimeout(); }

	//
	// ACCESSORS
	//Time (msec) most recent play, adjusted for start time of the movie
	//ex: PlayTime() == 20,000. Client said start 10 sec into the movie,
	//so AdjustedPlayTime() == 10,000
	QTSS_PlayFlags GetPlayFlags() { return fPlayFlags; }
	OSMutex*        GetSessionMutex() { return &fSessionMutex; }
	OSMutex*		GetRTSPSessionMutex() { return  &fRTSPSessionMutex; }

	OSRef*      GetRef() { return &fRTPMapElem; }
	RTSPSessionInterface* GetRTSPSession() { return fRTSPSession; }
	QTSS_RTPSessionState    GetSessionState() { return fState; }

	// This object has a current RTSP session. This may change over the
	// life of the RTSPSession, so update it. It keeps an RTSP session in
	// case interleaved data or commands need to be sent back to the client. 
	void            UpdateRTSPSession(RTSPSessionInterface* inNewRTSPSession);

	boost::string_view GetSessionID() const { return fRTSPSessionID; }
	std::vector<RTPStream*> GetStreams()  { return fStreamBuffer; }
protected:
	// These variables are setup by the derived RTPSession object when
	// Play and Pause get called

	//Some stream related information that is shared amongst all streams
	int64_t      fNextSendPacketsTime{0};

	//keeps track of whether we are playing or not
	QTSS_RTPSessionState fState{qtssPausedState};

	// If we are playing, this are the play flags that were set on play
	QTSS_PlayFlags  fPlayFlags{0};

	//Session mutex. This mutex should be grabbed before invoking the module
	//responsible for managing this session. This allows the module to be
	//non-preemptive-safe with respect to a session
	OSMutex     fSessionMutex;
	OSMutex		fRTSPSessionMutex;

	//Stores the session ID
	OSRef               fRTPMapElem;
	//The RTSP session ID that refers to this client session
	std::string         fRTSPSessionID;
	StrPtrLen           fRTSPSessionIDV;

	// In order to facilitate sending data over the RTSP channel from
	// an RTP session, we store a pointer to the RTSP session used in
	// the last RTSP request.
	RTSPSessionInterface* fRTSPSession{nullptr};

	std::vector<RTPStream*>       fStreamBuffer;
	TimeoutTask fTimeoutTask;
private:
};

#endif //_RTPSESSIONINTERFACE_H_
