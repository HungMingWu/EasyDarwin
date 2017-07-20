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
	 File:       RTSPRequestInterface.h

	 Contains:   Provides a simple API for modules to access request information and
				 manipulate (and possibly send) the client response.

				 Implements the RTSP Request dictionary for QTSS API.


 */


#ifndef __RTSPREQUESTINTERFACE_H__
#define __RTSPREQUESTINTERFACE_H__

 //INCLUDES:
#include "QTSS.h"
#include "QTSSDictionary.h"

#include "StrPtrLen.h"
#include "RTSPSessionInterface.h"
#include "RTSPResponseStream.h"
#include "RTSPProtocol.h"
#include "QTSSUserProfile.h"

class RTSPRequestInterface : public QTSSDictionary
{
public:

	//Initialize
	//Call initialize before instantiating this class. For maximum performance, this class builds
	//any response header it can at startup time.
	static void         Initialize();
	void ReInit(RTSPSessionInterface *session);

	//CONSTRUCTOR:
	RTSPRequestInterface(RTSPSessionInterface *session);
	virtual ~RTSPRequestInterface()
	{
		if (fMovieFolderPtr != &fMovieFolderPath[0]) delete[] fMovieFolderPtr;
	}

	//FUNCTIONS FOR SENDING OUTPUT:

	//Adds a new header to this object's list of headers to be sent out.
	//Note that this is only needed for "special purpose" headers. The Server,
	//CSeq, SessionID, and Connection headers are taken care of automatically
	void    AppendHeader(QTSS_RTSPHeader inHeader, StrPtrLen* inValue);


	// The transport header constructed by this function mimics the one sent
	// by the client, with the addition of server port & interleaved sub headers
	void    AppendTransportHeader(StrPtrLen* serverPortA,
		StrPtrLen* serverPortB,
		StrPtrLen* channelA,
		StrPtrLen* channelB,
		StrPtrLen* serverIPAddr = NULL,
		StrPtrLen* ssrc = NULL);
	void    AppendContentBaseHeader(StrPtrLen* theURL);
	void    AppendRTPInfoHeader(QTSS_RTSPHeader inHeader,
		StrPtrLen* url, StrPtrLen* seqNumber,
		StrPtrLen* ssrc, StrPtrLen* rtpTime, bool lastRTPInfo);

	void    AppendContentLength(uint32_t contentLength);
	void    AppendDateAndExpires();
	void    AppendSessionHeaderWithTimeout(StrPtrLen* inSessionID, StrPtrLen* inTimeout);
	void    AppendRetransmitHeader(uint32_t inAckTimeout);

	// MODIFIERS

	void SetKeepAlive(bool newVal) { fResponseKeepAlive = newVal; }

	//SendHeader:
	//Sends the RTSP headers, in their current state, to the client.
	void SendHeader();

	// QTSS STREAM FUNCTIONS

	// THE FIRST ENTRY OF THE IOVEC MUST BE BLANK!!!
	virtual QTSS_Error WriteV(iovec* inVec, uint32_t inNumVectors, uint32_t inTotalLength, uint32_t* outLenWritten);

	//Write
	//A "buffered send" that can be used for sending small chunks of data at a time.
	virtual QTSS_Error Write(void* inBuffer, uint32_t inLength, uint32_t* outLenWritten, uint32_t inFlags);

	// Flushes all currently buffered data to the network. This either returns
	// QTSS_NoErr or EWOULDBLOCK. If it returns EWOULDBLOCK, you should wait for
	// a EV_WR on the socket, and call flush again.
	virtual QTSS_Error  Flush() { return fOutputStream->Flush(); }

	// Reads data off the stream. Same behavior as calling RTSPSessionInterface::Read
	virtual QTSS_Error Read(void* ioBuffer, uint32_t inLength, uint32_t* outLenRead)
	{
		return fSession->Read(ioBuffer, inLength, outLenRead);
	}

	// Requests an event. Same behavior as calling RTSPSessionInterface::RequestEvent
	virtual QTSS_Error RequestEvent(QTSS_EventType inEventMask)
	{
		return fSession->RequestEvent(inEventMask);
	}


	//ACCESS FUNCTIONS:

	// These functions are shortcuts that objects internal to the server
	// use to get access to RTSP request information. Pretty much all
	// of this stuff is also available as QTSS API attributes.

	QTSS_RTSPMethod             GetMethod() const { return fMethod; }
	QTSS_RTSPStatusCode         GetStatus() const { return fStatus; }
	bool                      GetResponseKeepAlive() const { return fResponseKeepAlive; }
	void                        SetResponseKeepAlive(bool keepAlive) { fResponseKeepAlive = keepAlive; }

	//will be -1 unless there was a Range header. May have one or two values
	double                     GetStartTime() { return fStartTime; }
	double                     GetStopTime() { return fStopTime; }

	//
	// Value of Speed: header in request
	float                     GetSpeed() { return fSpeed; }

	//
	// Value of late-tolerance field of x-RTP-Options header
	float                     GetLateToleranceInSec() { return fLateTolerance; }
	StrPtrLen*                  GetLateToleranceStr() { return &fLateToleranceStr; }

	// these get set if there is a transport header
	uint16_t                      GetClientPortA() { return fClientPortA; }
	uint16_t                      GetClientPortB() { return fClientPortB; }
	uint32_t                      GetDestAddr() { return fDestinationAddr; }
	uint32_t                      GetSourceAddr() { return fSourceAddr; }
	uint16_t                      GetTtl() { return fTtl; }
	QTSS_RTPTransportType       GetTransportType() { return fTransportType; }
	QTSS_RTPNetworkMode         GetNetworkMode() { return fNetworkMode; }
	uint32_t                      GetWindowSize() { return fWindowSize; }


	bool                      HasResponseBeenSent()
	{
		return fOutputStream->GetBytesWritten() > 0;
	}

	RTSPSessionInterface*       GetSession() { return fSession; }
	QTSSDictionary*             GetHeaderDictionary() { return &fHeaderDictionary; }

	bool                      GetAllowed() { return fAllowed; }
	void                        SetAllowed(bool allowed) { fAllowed = allowed; }

	bool                      GetHasUser() { return fHasUser; }
	void                        SetHasUser(bool hasUser) { fHasUser = hasUser; }

	bool                      GetAuthHandled() { return fAuthHandled; }
	void                        SetAuthHandled(bool handled) { fAuthHandled = handled; }

	QTSS_ActionFlags            GetAction() { return fAction; }
	void                        SetAction(QTSS_ActionFlags action) { fAction = action; }

	bool						IsPushRequest() { return (fTransportMode == qtssRTPTransportModeRecord) ? true : false; }
	uint16_t                      GetSetUpServerPort() { return fSetUpServerPort; }
	QTSS_RTPTransportMode       GetTransportMode() { return fTransportMode; }

	QTSS_AuthScheme             GetAuthScheme() { return fAuthScheme; }
	void                        SetAuthScheme(QTSS_AuthScheme scheme) { fAuthScheme = scheme; }
	StrPtrLen*                  GetAuthRealm() { return &fAuthRealm; }
	StrPtrLen*                  GetAuthNonce() { return &fAuthNonce; }
	StrPtrLen*                  GetAuthUri() { return &fAuthUri; }
	uint32_t                      GetAuthQop() { return fAuthQop; }
	StrPtrLen*                  GetAuthNonceCount() { return &fAuthNonceCount; }
	StrPtrLen*                  GetAuthCNonce() { return &fAuthCNonce; }
	StrPtrLen*                  GetAuthResponse() { return &fAuthResponse; }
	StrPtrLen*                  GetAuthOpaque() { return &fAuthOpaque; }
	QTSSUserProfile*            GetUserProfile() { return fUserProfilePtr; }

	bool                      GetStale() { return fStale; }
	void                        SetStale(bool stale) { fStale = stale; }

	bool                      SkipAuthorization() { return fSkipAuthorization; }

	int32_t                      GetDynamicRateState() { return fEnableDynamicRateState; }

	// DJM PROTOTYPE
	uint32_t						GetRandomDataSize() { return fRandomDataSize; }

	uint32_t                      GetBandwidthHeaderBits() { return fBandwidthBits; }

	StrPtrLen*                  GetRequestChallenge() { return &fAuthDigestChallenge; }


protected:

	//ALL THIS STUFF HERE IS SETUP BY RTSPREQUEST object (derived)

	//REQUEST HEADER DATA
	enum
	{
		kMovieFolderBufSizeInBytes = 256,   //uint32_t
		kMaxFilePathSizeInBytes = 256       //uint32_t
	};

	QTSS_RTSPMethod             fMethod;            //Method of this request
	QTSS_RTSPStatusCode         fStatus;            //Current status of this request
	uint32_t                      fRealStatusCode;    //Current RTSP status num of this request
	bool                      fRequestKeepAlive;  //Does the client want keep-alive?
	bool                      fResponseKeepAlive; //Are we going to keep-alive?
	RTSPProtocol::RTSPVersion   fVersion;

	double                     fStartTime;         //Range header info: start time
	double                     fStopTime;          //Range header info: stop time

	uint16_t                      fClientPortA;       //This is all info that comes out
	uint16_t                      fClientPortB;       //of the Transport: header
	uint16_t                      fTtl;
	uint32_t                      fDestinationAddr;
	uint32_t                      fSourceAddr;
	QTSS_RTPTransportType       fTransportType;
	QTSS_RTPNetworkMode         fNetworkMode;

	uint32_t                      fContentLength;
	SInt64                      fIfModSinceDate;
	float                     fSpeed;
	float                     fLateTolerance;
	StrPtrLen                   fLateToleranceStr;
	float                     fPrebufferAmt;

	StrPtrLen                   fFirstTransport;

	QTSS_StreamRef              fStreamRef;

	//
	// For reliable UDP
	uint32_t                      fWindowSize;
	StrPtrLen                   fWindowSizeStr;

	//Because of URL decoding issues, we need to make a copy of the file path.
	//Here is a buffer for it.
	char                        fFilePath[kMaxFilePathSizeInBytes];
	char                        fMovieFolderPath[kMovieFolderBufSizeInBytes];
	char*                       fMovieFolderPtr;

	QTSSDictionary              fHeaderDictionary;

	bool                      fAllowed;
	bool                      fHasUser;
	bool                      fAuthHandled;

	QTSS_RTPTransportMode       fTransportMode;
	uint16_t                      fSetUpServerPort;           //send this back as the server_port if is SETUP request

	QTSS_ActionFlags            fAction;    // The action that will be performed for this request
											// Set to a combination of QTSS_ActionFlags 

	QTSS_AuthScheme             fAuthScheme;
	StrPtrLen                   fAuthRealm;
	StrPtrLen                   fAuthNonce;
	StrPtrLen                   fAuthUri;
	uint32_t                      fAuthQop;
	StrPtrLen                   fAuthNonceCount;
	StrPtrLen                   fAuthCNonce;
	StrPtrLen                   fAuthResponse;
	StrPtrLen                   fAuthOpaque;
	QTSSUserProfile             fUserProfile;
	QTSSUserProfile*            fUserProfilePtr;
	bool                      fStale;

	bool                      fSkipAuthorization;

	int32_t                      fEnableDynamicRateState;

	// DJM PROTOTYPE
	uint32_t						fRandomDataSize;

	uint32_t                      fBandwidthBits;
	StrPtrLen                   fAuthDigestChallenge;
	StrPtrLen                   fAuthDigestResponse;
private:

	RTSPSessionInterface*   fSession;
	RTSPResponseStream*     fOutputStream;


	enum
	{
		kStaticHeaderSizeInBytes = 512  //uint32_t
	};

	bool                  fStandardHeadersWritten;

	void                    PutTransportStripped(StrPtrLen &outFirstTransport, StrPtrLen &outResultStr);
	void                    WriteStandardHeaders();
	static void             PutStatusLine(StringFormatter* putStream,
		QTSS_RTSPStatusCode status,
		RTSPProtocol::RTSPVersion version);

	//Individual param retrieval functions
	static void*        GetAbsTruncatedPath(QTSSDictionary* inRequest, uint32_t* outLen);
	static void*        GetTruncatedPath(QTSSDictionary* inRequest, uint32_t* outLen);
	static void*        GetFileName(QTSSDictionary* inRequest, uint32_t* outLen);
	static void*        GetFileDigit(QTSSDictionary* inRequest, uint32_t* outLen);
	static void*        GetRealStatusCode(QTSSDictionary* inRequest, uint32_t* outLen);
	static void*		GetLocalPath(QTSSDictionary* inRequest, uint32_t* outLen);
	static void* 		GetAuthDigestResponse(QTSSDictionary* inRequest, uint32_t* outLen);

	//optimized preformatted response header strings
	static char             sPremadeHeader[kStaticHeaderSizeInBytes];
	static StrPtrLen        sPremadeHeaderPtr;

	static char             sPremadeNoHeader[kStaticHeaderSizeInBytes];
	static StrPtrLen        sPremadeNoHeaderPtr;

	static StrPtrLen        sColonSpace;

	//Dictionary support
	static QTSSAttrInfoDict::AttrInfo   sAttributes[];
};
#endif // __RTSPREQUESTINTERFACE_H__

