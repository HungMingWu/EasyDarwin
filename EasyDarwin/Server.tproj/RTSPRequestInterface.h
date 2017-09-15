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
#include <map>
#include <boost/utility/string_view.hpp>
#include "QTSS.h"

#include "StrPtrLen.h"
#include "RTSPSessionInterface.h"
#include "RTSPResponseStream.h"
#include "RTSPProtocol.h"

class HeaderDict {
	std::map<int, std::string> infos;
public:
	void Set(int type, const std::string &session) {
		infos[type] = session;
	}
	boost::string_view Get(int type) const { 
		auto it = infos.find(type);
		if (it == end(infos)) return {};
		return it->second;
	}
	void clear() { infos.clear(); }
};

class RTSPRequestInterface
{
protected:
	//The full local path to the file. This Attribute is first set after the Routing Role has run and before any other role is called. 
	std::string fullRequest;
	std::string absoluteURL;
	std::string absolutePath;
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
	}

	//FUNCTIONS FOR SENDING OUTPUT:

	//Adds a new header to this object's list of headers to be sent out.
	//Note that this is only needed for "special purpose" headers. The Server,
	//CSeq, SessionID, and Connection headers are taken care of automatically
	void    AppendHeader(QTSS_RTSPHeader inHeader, boost::string_view inValue);


	// The transport header constructed by this function mimics the one sent
	// by the client, with the addition of server port & interleaved sub headers
	void    AppendTransportHeader(boost::string_view serverPortA,
		boost::string_view serverPortB,
		boost::string_view channelA,
		boost::string_view channelB,
		boost::string_view serverIPAddr = {},
		boost::string_view ssrc = {});
	void    AppendContentBaseHeader(boost::string_view theURL);
	void    AppendRTPInfoHeader(QTSS_RTSPHeader inHeader,
		boost::string_view url, boost::string_view seqNumber,
		boost::string_view ssrc, boost::string_view rtpTime, bool lastRTPInfo);

	void    AppendContentLength(uint32_t contentLength);
	void    AppendSessionHeaderWithTimeout(boost::string_view inSessionID, boost::string_view inTimeout);
	void    AppendRetransmitHeader(uint32_t inAckTimeout);

	// MODIFIERS

	void SetKeepAlive(bool newVal) { fResponseKeepAlive = newVal; }

	//SendHeader:
	//Sends the RTSP headers, in their current state, to the client.
	void SendHeader();

	// QTSS STREAM FUNCTIONS

	// THE FIRST ENTRY OF THE IOVEC MUST BE BLANK!!!
	QTSS_Error WriteV(iovec* inVec, uint32_t inNumVectors, uint32_t inTotalLength, uint32_t* outLenWritten);

	//Write
	//A "buffered send" that can be used for sending small chunks of data at a time.
	QTSS_Error Write(void* inBuffer, uint32_t inLength, uint32_t* outLenWritten, uint32_t inFlags);

	// Flushes all currently buffered data to the network. This either returns
	// QTSS_NoErr or EWOULDBLOCK. If it returns EWOULDBLOCK, you should wait for
	// a EV_WR on the socket, and call flush again.
	QTSS_Error  Flush() { return fOutputStream->Flush(); }

	// Reads data off the stream. Same behavior as calling RTSPSessionInterface::Read
	QTSS_Error Read(void* ioBuffer, uint32_t inLength, uint32_t* outLenRead)
	{
		return fSession->Read(ioBuffer, inLength, outLenRead);
	}

	// Requests an event. Same behavior as calling RTSPSessionInterface::RequestEvent
	QTSS_Error RequestEvent(QTSS_EventType inEventMask)
	{
		return fSession->RequestEvent(inEventMask);
	}


	//ACCESS FUNCTIONS:

	// These functions are shortcuts that objects internal to the server
	// use to get access to RTSP request information. Pretty much all
	// of this stuff is also available as QTSS API attributes.

	QTSS_RTSPMethod             GetMethod() const { return fMethod; }
	void                        SetStatus(QTSS_RTSPStatusCode status) { fStatus = status; }
	QTSS_RTSPStatusCode         GetStatus() const { return fStatus; }
	bool                        GetResponseKeepAlive() const { return fResponseKeepAlive; }
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
	boost::string_view        GetLateToleranceStr() { return fLateToleranceStr; }

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
	const HeaderDict&           GetHeaderDict() const { return fHeaderDict; }

	bool						IsPushRequest() { return (fTransportMode == qtssRTPTransportModeRecord) ? true : false; }
	uint16_t                      GetSetUpServerPort() { return fSetUpServerPort; }
	QTSS_RTPTransportMode       GetTransportMode() { return fTransportMode; }

	bool                      GetStale() { return fStale; }
	void                        SetStale(bool stale) { fStale = stale; }

	int32_t                      GetDynamicRateState() { return fEnableDynamicRateState; }

	// DJM PROTOTYPE
	uint32_t						GetRandomDataSize() { return fRandomDataSize; }

	uint32_t                      GetBandwidthHeaderBits() { return fBandwidthBits; }

	//If the URI ends with one or more digits, this points to those.
	std::string         GetFileDigit();
	void SetUpServerPort(uint16_t port) { fSetUpServerPort = port; }
	//Everything after the last path separator in the file system path
	std::string GetFileName();
	void SetFullRequest(boost::string_view req) { fullRequest = std::string(req); }
	boost::string_view GetFullRequest() const { return fullRequest; }
	void SetAbsoluteURL(boost::string_view url) { absoluteURL = std::string(url); }
	boost::string_view GetAbsoluteURL() const { return absoluteURL; }
protected:

	//ALL THIS STUFF HERE IS SETUP BY RTSPREQUEST object (derived)

	//REQUEST HEADER DATA
	enum
	{
		kMovieFolderBufSizeInBytes = 256,   //uint32_t
	};

	QTSS_RTSPMethod             fMethod;            //Method of this request
	QTSS_RTSPStatusCode         fStatus;            //Current status of this request
   
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

	// Content length of incoming RTSP request body
	uint32_t                      fContentLength;

	float                     fSpeed;
	float                     fLateTolerance{ -1 };
	std::string               fLateToleranceStr;
	float                     fPrebufferAmt;

	std::string               fFirstTransport;

	//
	// For reliable UDP
	uint32_t                      fWindowSize;
	std::string                   fWindowSizeStr;

	//Because of URL decoding issues, we need to make a copy of the file path.
	//Here is a buffer for it.
	HeaderDict                  fHeaderDict;

	// A setup request from the client.
	QTSS_RTPTransportMode       fTransportMode;

	uint16_t                      fSetUpServerPort;           //send this back as the server_port if is SETUP request

	bool                      fStale;

	// -1 not in request, 0 off, 1 on
	int32_t                      fEnableDynamicRateState;

	// DJM PROTOTYPE
	uint32_t						fRandomDataSize;

	uint32_t                      fBandwidthBits;
private:

	RTSPSessionInterface*   fSession;
	RTSPResponseStream*     fOutputStream;

	bool                  fStandardHeadersWritten;

	void                    WriteStandardHeaders();
	static void             PutStatusLine(RTSPResponseStream* putStream,
		QTSS_RTSPStatusCode status,
		RTSPProtocol::RTSPVersion version);

	//optimized preformatted response header strings
	static std::string      sPremadeHeader;
};
#endif // __RTSPREQUESTINTERFACE_H__

