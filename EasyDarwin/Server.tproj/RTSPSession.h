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
	 File:       RTSPSession.h

	 Contains:   Represents an RTSP session (duh), which I define as a complete TCP connection
				 lifetime, from connection to FIN or RESET termination. This object is
				 the active element that gets scheduled and gets work done. It creates requests
				 and processes them when data arrives. When it is time to close the connection
				 it takes care of that.
 */

#ifndef __RTSPSESSION_H__
#define __RTSPSESSION_H__

#include "Attributes.h"
#include "RTSPSessionInterface.h"
#include "RTSPRequestStream.h"
#include "RTSPRequest.h"
#include "RTPSession.h"

class RTSPSession : public RTSPSessionInterface
{
public:

	RTSPSession();
	~RTSPSession() override;

	bool IsPlaying() { if (fRTPSession == nullptr) return false; if (fRTPSession->GetSessionState() == qtssPlayingState) return true; return false; }
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

	int64_t Run() override;

	// Gets & creates RTP session for this request.
	RTPSession*  FindRTPSession();
	QTSS_Error  CreateNewRTPSession();
	void        SetupClientSessionAttrs();

	// Does request prep & request cleanup, respectively
	void SetupRequest();
	void CleanupRequest();

	bool ParseOptionsResponse();

	// Fancy random number generator
	uint32_t GenerateNewSessionID(char* ioBuffer);

	// Sends an error response & returns error if not ok.
	QTSS_Error IsOkToAddNewRTPSession();

	// Checks authentication parameters
	void CheckAuthentication();

	// test current connections handled by this object against server pref connection limit
	bool OverMaxConnections(uint32_t buffer);

	char                fLastRTPSessionID[QTSS_MAX_SESSION_ID_LENGTH];
	StrPtrLen           fLastRTPSessionIDPtr;

	RTSPRequest*        fRequest{ nullptr };
	RTPSession*         fRTPSession{ nullptr };

	OSMutex             fReadMutex;
	void                HandleIncomingDataPacket();

	// Module invocation and module state.
	// This info keeps track of our current state so that
	// the state machine works properly.
	enum
	{
		kReadingRequest = 0,
		kFilteringRequest = 1,
		kRoutingRequest = 2,
		kAuthenticatingRequest = 3,
		kAuthorizingRequest = 4,
		kPreprocessingRequest = 5,
		kProcessingRequest = 6,
		kSendingResponse = 7,
		kPostProcessingRequest = 8,
		kCleaningUp = 9,

		// states that RTSP sessions that setup RTSP
		// through HTTP tunnels pass through
		kReadingFirstRequest = 13,                      // initial state - the only time we look for an HTTP tunnel
		kHaveNonTunnelMessage = 14                  // we've looked at the message, and its not an HTTP tunnle message
	};

	uint32_t fCurrentModule{ 0 };
	uint32_t fState{ kReadingFirstRequest };



	QTSS_StandardRTSP_Params     rtspParams;//module param blocks for roles.

	QTSS_Error SetupAuthLocalPath(RTSPRequest *theRTSPRequest);


	void SaveRequestAuthorizationParams(RTSPRequest *theRTSPRequest);
	QTSS_Error DumpRequestData();

	uint64_t fMsgCount{ 0 };
	Attributes attr;
    //friend class RTSPSessionHandler;

};

#endif // __RTSPSESSION_H__

