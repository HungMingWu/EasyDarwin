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
	std::string queryString;
	//URI for this request
	std::string uriPath;
	//Challenge used by the server for Digest authentication
	std::string digestChallenge;
	//decoded Authentication information when provided by the RTSP request. See RTSPSessLastUserName.
	std::string userName;
	std::string passWord;
	std::string uRLRealm;
	std::string respMsg;
public:

	//CONSTRUCTOR / DESTRUCTOR
	//these do very little. Just initialize / delete some member data.
	//
	//Arguments:        session: the session this request is on (massive cyclical dependency)
	RTSPRequest(RTSPSessionInterface* session)
		: RTSPRequestInterface(session) {}
	~RTSPRequest() = default;

	//Parses the request. Returns an error handler if there was an error encountered
	//in parsing.
	QTSS_Error Parse();

	QTSS_Error ParseAuthHeader(void);
	// called by ParseAuthHeader
	QTSS_Error ParseBasicHeader(boost::string_view inParsedAuthLinePtr);

	// called by ParseAuthHeader
	QTSS_Error ParseDigestHeader(boost::string_view inParsedAuthLine);

	void SetupAuthLocalPath(void);
	QTSS_Error SendBasicChallenge(void);
	QTSS_Error SendDigestChallenge(uint32_t qop, boost::string_view nonce, boost::string_view opaque);
	QTSS_Error SendForbiddenResponse(void);
	boost::string_view GetQueryString() const { return queryString; }
	uint32_t GetContentLength() const { return fContentLength; }
	boost::string_view GetURI() const { return uriPath; }
	void SetDigestChallenge(boost::string_view digest) { digestChallenge = std::string(digest); }
	void SetAuthUserName(boost::string_view name) { userName = std::string(name); }
	boost::string_view GetAuthUserName() const { return userName; }
	void SetPassWord(boost::string_view password) { passWord = std::string(password); }
	boost::string_view GetPassWord() const { return passWord; }
	void SetURLRealm(boost::string_view realm) { uRLRealm = std::string(realm); }
	boost::string_view GetURLRealm() const { return uRLRealm; }
	void SetRespMsg(boost::string_view msg) { respMsg = std::string(msg); }
	boost::string_view GetRespMsg() const { return respMsg; }
	QTSS_Error SendErrorResponse(QTSS_RTSPStatusCode inStatusCode);
	QTSS_Error SendErrorResponseWithMessage(QTSS_RTSPStatusCode inStatusCode);
	void SendDescribeResponse(iovec* describeData,
		uint32_t inNumVectors,
		uint32_t inTotalLength);
private:
	void ReqSendDescribeResponse();
	//PARSING
	enum { kAuthChallengeHeaderBufSize = 512 };

	//Parsing the URI line (first line of request
	QTSS_Error ParseFirstLine(boost::string_view method, boost::string_view fulluri, boost::string_view ver);

	//Utility functions called by above
	QTSS_Error ParseURI(boost::string_view fulluri);

	//Parsing the rest of the headers
	//This assumes that the parser is at the beginning of the headers. It will parse
	//the headers, fill out the data & HTTPParameters object.
	//
	//Returns:      A handler object signifying that a fatal syntax error has occurred
	QTSS_Error ParseHeaders(const std::map<std::string, std::string> &headers);


	//Functions to parse the contents of particuarly complicated headers (as a convienence
	//for modules)
	void    ParseRangeHeader(boost::string_view header);
	void    ParseTransportHeader(boost::string_view header);
	void    ParseIfModSinceHeader(boost::string_view header);
	void    ParseAddrSubHeader(boost::string_view inSubHeader, boost::string_view inHeaderName, uint32_t* outAddr);
	void    ParseRetransmitHeader(boost::string_view header);
	void    ParseContentLengthHeader(boost::string_view header);
	void    ParseSpeedHeader(boost::string_view header);
	void    ParsePrebufferHeader(boost::string_view header);
	void    ParseTransportOptionsHeader(boost::string_view header);
	void    ParseSessionHeader(boost::string_view header);
	void    ParseClientPortSubHeader(boost::string_view inClientPortSubHeader);
	void    ParseTimeToLiveSubHeader(boost::string_view inTimeToLiveSubHeader);
	void    ParseModeSubHeader(boost::string_view inModeSubHeader);
	bool    ParseNetworkModeSubHeader(boost::string_view inSubHeader);
	void 	ParseDynamicRateHeader(boost::string_view header);
	void	ParseRandomDataSizeHeader(boost::string_view header);
	void    ParseBandwidthHeader(boost::string_view header);
};
#endif // __RTSPREQUEST_H__

