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
	 File:       RTPSession.h

	 Contains:   RTPSession represents an, well, an RTP session. The server creates
				 one of these when a new client connects, and it lives for the duration
				 of an RTP presentation. It contains all the resources associated with
				 that presentation, such as RTPStream objects. It also provides an
				 API for manipulating a session (playing it, pausing it, stopping it, etc)

				 It is also the active element, ie. it is the object that gets scheduled
				 to send out & receive RTP & RTCP packets

	 Change History (most recent first):




 */


#ifndef _RTPSESSION_H_
#define _RTPSESSION_H_

#include "Attributes.h"
#include "RTPSessionInterface.h"
#include "RTSPRequestInterface.h"
#include "RTPStream.h"
#include "QTSSModule.h"


class RTPSession : public RTPSessionInterface
{
	// full Presentation URL for this session. Same as presentationURL, 
	// but includes rtsp://domain.com prefix
	std::string absoluteURL;
	// Presentation URL for this session. This URL is the "base" URL for the session. 
	// RTSP requests to this URL are assumed to affect all streams on the session.
	std::string presentationURL;
	// Query string from the request that creates this  client session
	std::string queryString;
	//requestes host name for s session. Just the "domain.com" portion
	std::string hostName;
	std::string userAgent;
	std::string respMsg;
public:

	RTPSession();
	~RTPSession() override;

	//
	//ACCESS FUNCTIONS

	bool      HasAnRTPStream() { return fHasAnRTPStream; }

	RTPStream*  FindRTPStreamForChannelNum(uint8_t inChannelNum);

	//
	// MODIFIERS

	//This puts this session into the session map (called by the server! not the module!)
	//If this function fails (it can return QTSS_DupName), it means that there is already
	//a session with this session ID in the map.
	QTSS_Error  Activate(boost::string_view inSessionID);

	//Once the session is bound, a module can add streams to it.
	//It must pass in a trackID that uniquely identifies this stream.
	//This call can only be made during an RTSP Setup request, and the
	//RTSPRequestInterface must be provided.
	//You may also opt to attach a codec name and type to this stream.
	QTSS_Error  AddStream(RTSPRequest* request, RTPStream** outStream,
		QTSS_AddStreamFlags inFlags);

	//Reset the thinning params for all streams using the late tolerance value
	void SetStreamThinningParams(float inLateTolerance);

	//Begins playing all streams. Currently must be associated with an RTSP Play
	//request, and the request interface must be provided.
	QTSS_Error  Play(RTSPRequestInterface* request, QTSS_PlayFlags inFlags);

	//Pauses all streams.
	void            Pause();

	// Tears down the session. This will cause QTSS_SessionClosing_Role to run
	void            Teardown();

	//Utility functions. Modules aren't required to use these, but can be useful
	void            SendPlayResponse(RTSPRequestInterface* request, uint32_t inFlags);

	int32_t          GetQualityLevel();
	void            SetQualityLevel(int32_t level);
	void            SetQueryString(boost::string_view query) {
		queryString = std::string(queryString);
	}
	boost::string_view GetQueryString() const { return queryString; }
	void            SetPresentationURL(boost::string_view url) {
		presentationURL = std::string(url);
	}
	boost::string_view GetPresentationURL() const { return presentationURL; }
	void            SetAbsoluteURL(boost::string_view url) {
		absoluteURL = std::string(url);
	}
	boost::string_view GetAbsoluteURL() const { return absoluteURL; }
	void            SetHost(boost::string_view url) {
		hostName = std::string(url);
	}
	boost::string_view GetHost() const { return hostName; }
	void            SetUserAgent(boost::string_view useragent) {
		userAgent = std::string(useragent);
	}
	boost::string_view GetUserAgent() const { return userAgent; }
	void SetRespMsg(boost::string_view msg) { respMsg = std::string(msg); }
	boost::string_view GetRespMsg() const { return respMsg; }
	void ResetTimeout(uint32_t timeout);
	inline void addAttribute(boost::string_view key, boost::any value) {
		attr.addAttribute(key, value);
	}
	inline boost::optional<boost::any> getAttribute(boost::string_view key) {
		return attr.getAttribute(key);
	}
	inline void removeAttribute(boost::string_view key) {
		attr.removeAttribute(key);
	}
private:
	Attributes attr;
	//where timeouts, deletion conditions get processed
	int64_t  Run() override;

	// Utility function used by Play
	uint32_t PowerOf2Floor(uint32_t inNumToFloor);

	//overbuffer logging function
	void LogOverbufferStats();

	enum
	{
		kRTPStreamArraySize = 20,
		kCantGetMutexIdleTime = 10
	};

	bool              fHasAnRTPStream{false};
	int32_t              fSessionQualityLevel;

	char        fRTPStreamArray[kRTPStreamArraySize];

	// Module invocation and module state.
	// This info keeps track of our current state so that
	// the state machine works properly.
	enum
	{
		kStart = 0,
		kSendingPackets = 1
	};

	uint32_t fCurrentModuleIndex{0};
	uint32_t fCurrentState{kStart};

	QTSS_ModuleState    fModuleState;
	QTSS_CliSesClosingReason fClosingReason{qtssCliSesCloseClientTeardown};

	uint32_t              fCurrentModule{0};
	// This is here to give the ability to the ClientSessionClosing role to
	// do asynchronous I/O
	bool              fModuleDoingAsyncStuff{false};

#if DEBUG
	bool fActivateCalled;
#endif
	int64_t              fLastBandwidthTrackerStatsUpdate{0};

};

#endif //_RTPSESSION_H_
