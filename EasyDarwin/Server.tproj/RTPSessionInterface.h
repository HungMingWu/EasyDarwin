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
#include "QTSSDictionary.h"

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


class RTPSessionInterface : public QTSSDictionary, public Task
{
public:

	// Initializes dictionary resources
	static void Initialize();

	//
	// CONSTRUCTOR / DESTRUCTOR

	RTPSessionInterface();
	~RTPSessionInterface() override
	{
		if (GetQualityLevel() != 0)
			QTSServerInterface::GetServer()->IncrementNumThinned(-1);
		if (fRTSPSession != nullptr)
			fRTSPSession->DecrementObjectHolderCount();
		delete[] fSRBuffer.Ptr;
		delete[] fAuthNonce.Ptr;
		delete[] fAuthOpaque.Ptr;
	}

	void SetValueComplete(uint32_t inAttrIndex, QTSSDictionaryMap* inMap,
		uint32_t inValueIndex, void* inNewValue, uint32_t inNewValueLen) override;

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

	uint32_t          GetPacketsSent() { return fPacketsSent; }
	uint32_t          GetBytesSent() { return fBytesSent; }
	OSRef*      GetRef() { return &fRTPMapElem; }
	RTSPSessionInterface* GetRTSPSession() { return fRTSPSession; }
	uint32_t      GetMovieAvgBitrate() { return fMovieAverageBitRate; }
	void SetMovieAvgBitrate(uint32_t average) { fMovieAverageBitRate = average; }
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
	// Memory if you want to build your own
	char*           GetSRBuffer(uint32_t inSRLen);

	//
	// STATISTICS UPDATING

	//The session tracks the total number of bytes sent during the session.
	//Streams can update that value by calling this function
	void            UpdateBytesSent(uint32_t bytesSent) { fBytesSent += bytesSent; }

	//The session tracks the total number of packets sent during the session.
	//Streams can update that value by calling this function                
	void            UpdatePacketsSent(uint32_t packetsSent) { fPacketsSent += packetsSent; }

	void            UpdateCurrentBitRate(const int64_t& curTime)
	{
		if (curTime > (fLastBitRateUpdateTime + 3000)) this->UpdateBitRateInternal(curTime);
	}

	void            SetAllTracksInterleaved(bool newValue) { fAllTracksInterleaved = newValue; }
	//
	// RTSP RESPONSES

	// This function appends a session header to the SETUP response, and
	// checks to see if it is a 304 Not Modified. If it is, it sends the entire
	// response and returns an error
	QTSS_Error DoSessionSetupResponse(RTSPRequestInterface* inRequest);

	//
	// RTSP SESSION

	// This object has a current RTSP session. This may change over the
	// life of the RTSPSession, so update it. It keeps an RTSP session in
	// case interleaved data or commands need to be sent back to the client. 
	void            UpdateRTSPSession(RTSPSessionInterface* inNewRTSPSession);

	// let's RTSP Session pass along it's query string
	void            SetQueryString(StrPtrLen* queryString);

	// SETTERS and ACCESSORS for auth information
	// Authentication information that needs to be kept around
	// for the duration of the session      
	QTSS_AuthScheme GetAuthScheme() { return fAuthScheme; }
	StrPtrLen*      GetAuthNonce() { return &fAuthNonce; }
	uint32_t          GetAuthQop() { return fAuthQop; }
	uint32_t          GetAuthNonceCount() { return fAuthNonceCount; }
	StrPtrLen*      GetAuthOpaque() { return &fAuthOpaque; }
	void            SetAuthScheme(QTSS_AuthScheme scheme) { fAuthScheme = scheme; }
	// Use this if the auth scheme or the qop has to be changed from the defaults 
	// of scheme = Digest, and qop = auth
	void            SetChallengeParams(QTSS_AuthScheme scheme, uint32_t qop, bool newNonce, bool createOpaque);
	// Use this otherwise...if newNonce == true, it will create a new nonce
	// and reset nonce count. If newNonce == false but nonce was never created earlier
	// a nonce will be created. If newNonce == false, and there is an existing nonce,
	// the nounce count will be incremented.
	void            UpdateDigestAuthChallengeParams(bool newNonce, bool createOpaque, uint32_t qop);

	float* GetPacketLossPercent() { uint32_t outLen; return  (float*) this->PacketLossPercent(this, &outLen); }

	int32_t          GetQualityLevel() { return fSessionQualityLevel; }
	int32_t*         GetQualityLevelPtr() { return &fSessionQualityLevel; }
	void            SetQualityLevel(int32_t level) {
		if (fSessionQualityLevel == 0 && level != 0)
			QTSServerInterface::GetServer()->IncrementNumThinned(1);
		else if (fSessionQualityLevel != 0 && level == 0)
			QTSServerInterface::GetServer()->IncrementNumThinned(-1);
		fSessionQualityLevel = level;
	}
	int64_t          fLastQualityCheckTime{0};
	int64_t			fLastQualityCheckMediaTime{0};
	bool			fStartedThinning{false};

	// Used by RTPStream to increment the RTCP packet and byte counts.
	void            IncrTotalRTCPPacketsRecv() { fTotalRTCPPacketsRecv++; }
	uint32_t          GetTotalRTCPPacketsRecv() { return fTotalRTCPPacketsRecv; }
	void            IncrTotalRTCPBytesRecv(uint16_t cnt) { fTotalRTCPBytesRecv += cnt; }
	uint32_t          GetTotalRTCPBytesRecv() { return fTotalRTCPBytesRecv; }
	uint64_t      GetMovieSizeInBytes() const { return fMovieSizeInBytes; }
	void          SetLastRTSPBandwithBits(uint32_t value) { fLastRTSPBandwidthHeaderBits = value; }
	uint32_t          GetCurrentMovieBitRate() { return fMovieCurrentBitRate; }

	uint32_t          GetMaxBandwidthBits() { uint32_t maxRTSP = fLastRTSPBandwidthHeaderBits;  return  maxRTSP; }
	boost::string_view GetSessionID() const { return fRTSPSessionID; }
	std::vector<RTPStream*> GetStreams()  { return fStreamBuffer; }
	void SetUserName(boost::string_view name) { fUserName = std::string(name); }
	boost::string_view GetUserName() const { return fUserName; }
	void SetLocalDNS(boost::string_view local) { fRTSPSessLocalDNS = std::string(local); }
	boost::string_view GetLocalDNS() const { return fRTSPSessLocalDNS; }
	void SetLocalAddr(boost::string_view local) { fRTSPSessLocalAddrStr = std::string(local); }
	boost::string_view GetLocalAddr() const { return fRTSPSessLocalAddrStr; }
	void SetRemoteAddr(boost::string_view remote) {	fRTSPSessRemoteAddrStr = std::string(fRTSPSessRemoteAddrStr); }
	boost::string_view GetRemoteAddr() const { return fRTSPSessRemoteAddrStr; }
	void SetStatusCode(uint32_t code) { fLastRTSPReqRealStatusCode = code; }
	uint32_t GetStatusCode() const { return fLastRTSPReqRealStatusCode; }
	double GetMovieDuration() const { return fMovieDuration; }
	void SetOverBufferEnabled(bool turned) { GetOverbufferWindow()->TurnOverbuffering(turned); }
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
	uint32_t      fLastBitRateBytes{0};
	int64_t      fLastBitRateUpdateTime{0};
	uint32_t      fMovieCurrentBitRate{0};

	// In order to facilitate sending data over the RTSP channel from
	// an RTP session, we store a pointer to the RTSP session used in
	// the last RTSP request.
	RTSPSessionInterface* fRTSPSession{nullptr};

	std::vector<RTPStream*>       fStreamBuffer;
	std::string fUserName;

private:

	//
	// Utility function for calculating current bit rate
	void UpdateBitRateInternal(const int64_t& curTime);

	static void* PacketLossPercent(QTSSDictionary* inSession, uint32_t* outLen);
	static void* TimeConnected(QTSSDictionary* inSession, uint32_t* outLen);
	static void* CurrentBitRate(QTSSDictionary* inSession, uint32_t* outLen);

	// Create nonce
	void CreateDigestAuthenticationNonce();

	// One of the RTP session attributes is an iterated list of all streams.
	// As an optimization, we preallocate a "large" buffer of stream pointers,
	// even though we don't know how many streams we need at first.
	enum
	{
		kFullRequestURLBufferSize = 256,

		kIPAddrStrBufSize = 20,

		kAuthNonceBufSize = 32,
		kAuthOpaqueBufSize = 32,

	};




	// theses are dictionary items picked up by the RTSPSession
	// but we need to store copies of them for logging purposes.

	std::string        fRTSPSessRemoteAddrStr;
	std::string        fRTSPSessLocalDNS;
	std::string        fRTSPSessLocalAddrStr;

	
	char        fUserPasswordBuf[RTSPSessionInterface::kMaxUserPasswordLen];
	char        fUserRealmBuf[RTSPSessionInterface::kMaxUserRealmLen];
	uint32_t      fLastRTSPReqRealStatusCode{200};

	//for timing out this session
	TimeoutTask fTimeoutTask;
	uint32_t      fTimeout;

	// Time when this session got created
	int64_t      fSessionCreateTime;

	//Packet priority levels. Each stream has a current level, and
	//the module that owns this session sets what the number of levels is.
	uint32_t      fNumQualityLevels{0};

	//Statistics
	uint32_t fBytesSent{0};
	uint32_t fPacketsSent{0};
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
	StrPtrLen           fSRBuffer;

	RTPBandwidthTracker fTracker;
	RTPOverbufferWindow fOverbufferWindow;

	// Built in dictionary attributes
	static QTSSAttrInfoDict::AttrInfo   sAttributes[];
	static unsigned int sRTPSessionIDCounter;

	// Authentication information that needs to be kept around
	// for the duration of the session      
	QTSS_AuthScheme             fAuthScheme;
	StrPtrLen                   fAuthNonce;
	uint32_t                      fAuthQop{RTSPSessionInterface::kNoQop};
	uint32_t                      fAuthNonceCount{0};
	StrPtrLen                   fAuthOpaque;
	uint32_t                      fQualityUpdate;

	uint32_t                      fFramesSkipped{0};

	uint32_t                  fLastRTSPBandwidthHeaderBits{0};
};

#endif //_RTPSESSIONINTERFACE_H_
