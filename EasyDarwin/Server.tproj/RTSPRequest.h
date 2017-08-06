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
	 File:       RTSPRequest.h

	 Contains:   This class encapsulates a single RTSP request. It stores the meta data
				 associated with a request, and provides an interface (through its base
				 class) for modules to access request information

				 It divides the request into a series of states.

				 State 1: At first, when the object is constructed or right after
						  its Reset function is called, it is in an uninitialized state.
				 State 2: Parse() parses an RTSP header. Once this function returns,
						  most of the request-related query functions have been setup.
						  (unless an error occurs)
				 State 3: GetHandler() uses the request information to create the
						  proper request Handler object for this request. After that,
						  it is the Handler object's responsibilty to process the
						  request and send a response to the client.



 */

#ifndef __RTSPREQUEST_H__
#define __RTSPREQUEST_H__

#include <boost/utility/string_view.hpp>
#include "RTSPRequestInterface.h"
#include "RTSPSessionInterface.h"
#include "StringParser.h"

 //HTTPRequest class definition
class RTSPRequest : public RTSPRequestInterface
{
	// query stting (CGI parameters) passed to the server in the request URL, does not include the '?' separator
	boost::string_view queryString;
	//URI for this request
	std::string uri;
	//Challenge used by the server for Digest authentication
	std::string digestChallenge;
	//decoded Authentication information when provided by the RTSP request. See RTSPSessLastUserName.
	std::string userName;
public:

	//CONSTRUCTOR / DESTRUCTOR
	//these do very little. Just initialize / delete some member data.
	//
	//Arguments:        session: the session this request is on (massive cyclical dependency)
	RTSPRequest(RTSPSessionInterface* session)
		: RTSPRequestInterface(session) {}
	~RTSPRequest() override = default;

	//Parses the request. Returns an error handler if there was an error encountered
	//in parsing.
	QTSS_Error Parse();

	QTSS_Error ParseAuthHeader(void);
	// called by ParseAuthHeader
	QTSS_Error ParseBasicHeader(StringParser *inParsedAuthLinePtr);

	// called by ParseAuthHeader
	QTSS_Error ParseDigestHeader(StringParser *inParsedAuthLinePtr);

	void SetupAuthLocalPath(void);
	QTSS_Error SendBasicChallenge(void);
	QTSS_Error SendDigestChallenge(uint32_t qop, StrPtrLen *nonce, StrPtrLen* opaque);
	QTSS_Error SendForbiddenResponse(void);
	boost::string_view GetQueryString() const { return queryString; }
	uint32_t GetContentLength() const { return fContentLength; }
	boost::string_view GetURI() const { return uri; }
	void SetDigestChallenge(boost::string_view digest) { digestChallenge = std::string(digest); }
	void SetAuthUserName(boost::string_view name) { userName = std::string(name); }
	boost::string_view GetAuthUserName() const { return userName; }
private:

	//PARSING
	enum { kAuthNameAndPasswordBuffSize = 128, kAuthChallengeHeaderBufSize = 512 };

	//Parsing the URI line (first line of request
	QTSS_Error ParseFirstLine(StringParser &parser);

	//Utility functions called by above
	QTSS_Error ParseURI(StringParser &parser);

	//Parsing the rest of the headers
	//This assumes that the parser is at the beginning of the headers. It will parse
	//the headers, fill out the data & HTTPParameters object.
	//
	//Returns:      A handler object signifying that a fatal syntax error has occurred
	QTSS_Error ParseHeaders(StringParser& parser);


	//Functions to parse the contents of particuarly complicated headers (as a convienence
	//for modules)
	void    ParseRangeHeader(StrPtrLen &header);
	void    ParseTransportHeader(StrPtrLen &header);
	void    ParseIfModSinceHeader(StrPtrLen &header);
	void    ParseAddrSubHeader(StrPtrLen* inSubHeader, StrPtrLen* inHeaderName, uint32_t* outAddr);
	void    ParseRetransmitHeader(StrPtrLen &header);
	void    ParseContentLengthHeader(boost::string_view header);
	void    ParseSpeedHeader(boost::string_view header);
	void    ParsePrebufferHeader(boost::string_view header);
	void    ParseTransportOptionsHeader(StrPtrLen &header);
	void    ParseSessionHeader(boost::string_view header);
	void    ParseClientPortSubHeader(StrPtrLen* inClientPortSubHeader);
	void    ParseTimeToLiveSubHeader(StrPtrLen* inTimeToLiveSubHeader);
	void    ParseModeSubHeader(StrPtrLen* inModeSubHeader);
	bool    ParseNetworkModeSubHeader(StrPtrLen* inSubHeader);
	void 	ParseDynamicRateHeader(boost::string_view header);
	void	ParseRandomDataSizeHeader(boost::string_view header);
	void    ParseBandwidthHeader(boost::string_view header);

	static uint8_t    sURLStopConditions[];
};
#endif // __RTSPREQUEST_H__

