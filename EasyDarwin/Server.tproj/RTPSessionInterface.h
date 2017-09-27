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

#include "RTCPSRPacket.h"
#include "RTSPSessionInterface.h"
#include "TimeoutTask.h"
#include "Task.h"
#include "RTPBandwidthTracker.h"
#include "RTPOverbufferWindow.h"
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
		if (GetQualityLevel() != 0)
			getSingleton()->IncrementNumThinned(-1);
		if (fRTSPSession != nullptr)
			fRTSPSession->DecrementObjectHolderCount();
	}

	//Timeouts. This allows clients to refresh the timeout on this session
	void    RefreshTimeout() { fTimeoutTask.RefreshTimeout(); }
	void    RefreshRTSPTimeout() { if (fRTSPSession != nullptr) fRTSPSession->RefreshTimeout(); }
	void    RefreshTimeouts() { RefreshTimeout(); RefreshRTSPTimeout(); }

	//
	// ACCESSORS

	bool  IsFirstPlay() { return fIsFirstPlay; }
	int64_t  GetFirstPlayTime() { return fFirstPlayTime; }
	//Time (msec) most recent play was issued
	int64_t  GetPlayTime() { return fPlayTime; }
	int64_t  GetNTPPlayTime() { return fNTPPlayTime; }
	int64_t  GetSessionCreateTime() { return fSessionCreateTime; }
	//Time (msec) most recent play, adjusted for start time of the movie
	//ex: PlayTime() == 20,000. Client said start 10 sec into the movie,
	//so AdjustedPlayTime() == 10,000
	QTSS_PlayFlags GetPlayFlags() { return fPlayFlags; }
	OSMutex*        GetSessionMutex() { return &fSessionMutex; }
	OSMutex*		GetRTSPSessionMutex() { return  &fRTSPSessionMutex; }

	OSRef*      GetRef() { return &fRTPMapElem; }
	RTSPSessionInterface* GetRTSPSession() { return fRTSPSession; }
	uint32_t      GetMovieAvgBitrate() { return fMovieAverageBitRate; }
	void          SetMovieAvgBitrate(uint32_t rate) { fMovieAverageBitRate = rate; }
	QTSS_CliSesTeardownReason GetTeardownReason() { return fTeardownReason; }
	void SetTeardownReason(QTSS_CliSesTeardownReason reason) { fTeardownReason = reason; }
	QTSS_RTPSessionState    GetSessionState() { return fState; }
	void    SetUniqueID(uint32_t theID) { fUniqueID = theID; }
	uint32_t  GetUniqueID() { return fUniqueID; }
	RTPBandwidthTracker* GetBandwidthTracker() { return &fTracker; }
	RTPOverbufferWindow* GetOverbufferWindow() { return &fOverbufferWindow; }
	uint32_t  GetFramesSkipped() { return fFramesSkipped; }

	//
	// MEMORY FOR RTCP PACKETS

	//
	// Class for easily building a standard RTCP SR
	RTCPSRPacket*   GetSRPacket() { return &fRTCPSRPacket; }

	//
	// STATISTICS UPDATING

	//The session tracks the total number of bytes sent during the session.
	//Streams can update that value by calling this function

	void            SetAllTracksInterleaved(bool newValue) { fAllTracksInterleaved = newValue; }

	//
	// RTSP SESSION

	// This object has a current RTSP session. This may change over the
	// life of the RTSPSession, so update it. It keeps an RTSP session in
	// case interleaved data or commands need to be sent back to the client. 
	void            UpdateRTSPSession(RTSPSessionInterface* inNewRTSPSession);

	int32_t          GetQualityLevel() { return fSessionQualityLevel; }
	int32_t*         GetQualityLevelPtr() { return &fSessionQualityLevel; }
	void            SetQualityLevel(int32_t level) {
		if (fSessionQualityLevel == 0 && level != 0)
			getSingleton()->IncrementNumThinned(1);
		else if (fSessionQualityLevel != 0 && level == 0)
			getSingleton()->IncrementNumThinned(-1);
		fSessionQualityLevel = level;
	}

	// Used by RTPStream to increment the RTCP packet and byte counts.
	void            IncrTotalRTCPPacketsRecv() { fTotalRTCPPacketsRecv++; }
	uint32_t          GetTotalRTCPPacketsRecv() { return fTotalRTCPPacketsRecv; }
	void            IncrTotalRTCPBytesRecv(uint16_t cnt) { fTotalRTCPBytesRecv += cnt; }
	uint32_t          GetTotalRTCPBytesRecv() { return fTotalRTCPBytesRecv; }

	uint32_t          GetLastRTSPBandwithBits() { return fLastRTSPBandwidthHeaderBits; }
	void              SetLastRTSPBandwithBits(uint32_t bandwidth) { fLastRTSPBandwidthHeaderBits = bandwidth; }

	uint32_t          GetMaxBandwidthBits() { uint32_t maxRTSP = GetLastRTSPBandwithBits();  return  maxRTSP; }
	boost::string_view GetSessionID() const { return fRTSPSessionID; }
	std::vector<RTPStream*> GetStreams()  { return fStreamBuffer; }
	float GetPacketLossPercent();
	uint64_t GetMovieSizeInBytes() const { return fMovieSizeInBytes; }
	double GetMovieDuration() const { return fMovieDuration; }
protected:
	// These variables are setup by the derived RTPSession object when
	// Play and Pause get called

	//Some stream related information that is shared amongst all streams
	bool      fIsFirstPlay{true};
	bool      fAllTracksInterleaved{true};
	int64_t      fFirstPlayTime{0};//in milliseconds
	int64_t      fPlayTime{0};
	int64_t      fAdjustedPlayTime{0};
	int64_t      fNTPPlayTime{0};
	int64_t      fNextSendPacketsTime{0};

	int32_t      fSessionQualityLevel{0};

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
	//for timing out this session

	uint32_t      fTimeout;

	// Time when this session got created
	int64_t      fSessionCreateTime;

	//Packet priority levels. Each stream has a current level, and
	//the module that owns this session sets what the number of levels is.
	uint32_t      fNumQualityLevels{0};

	//Statistics
	float fPacketLossPercent{0.0};
	int64_t fTimeConnected{0};
	uint32_t fTotalRTCPPacketsRecv{0};
	uint32_t fTotalRTCPBytesRecv{0};
	// Movie size & movie duration. It may not be so good to associate these
	// statistics with the movie, for a session MAY have multiple movies...
	// however, the 1 movie assumption is in too many subsystems at this point
	double     fMovieDuration{0};
	uint64_t      fMovieSizeInBytes{0};
	uint32_t      fMovieAverageBitRate{0};

	QTSS_CliSesTeardownReason fTeardownReason{0};
	// So the streams can send sender reports
	uint32_t      fUniqueID{0};

	RTCPSRPacket        fRTCPSRPacket;

	RTPBandwidthTracker fTracker;
	RTPOverbufferWindow fOverbufferWindow;

	static unsigned int sRTPSessionIDCounter;

	uint32_t                      fQualityUpdate;

	uint32_t                      fFramesSkipped{0};

	uint32_t                  fLastRTSPBandwidthHeaderBits{0};
};

#endif //_RTPSESSIONINTERFACE_H_
