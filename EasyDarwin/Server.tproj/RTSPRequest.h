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

#include <sstream>
#include <unordered_map>
#include <boost/utility/string_view.hpp>
#include <boost/asio/streambuf.hpp>
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

	boost::string_view GetQueryString() const { return queryString; }
	uint32_t GetContentLength() const { return fContentLength; }
	boost::string_view GetURI() const { return uriPath; }
	void SetRespMsg(boost::string_view msg) { respMsg = std::string(msg); }
	boost::string_view GetRespMsg() const { return respMsg; }
	QTSS_Error SendErrorResponse(QTSS_RTSPStatusCode inStatusCode);
	QTSS_Error SendErrorResponseWithMessage(QTSS_RTSPStatusCode inStatusCode);
	void SendDescribeResponse(iovec* describeData,
		uint32_t inNumVectors,
		uint32_t inTotalLength);
private:
	void ReqSendDescribeResponse();

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
	void    ParseRetransmitHeader(boost::string_view header);
	void    ParseContentLengthHeader(boost::string_view header);
	void    ParseSpeedHeader(boost::string_view header);
	void    ParsePrebufferHeader(boost::string_view header);
	void    ParseSessionHeader(boost::string_view header);
	void    ParseModeSubHeader(boost::string_view inModeSubHeader);
	bool    ParseNetworkModeSubHeader(boost::string_view inSubHeader);
	void    ParseBandwidthHeader(boost::string_view header);
};

#endif // __RTSPREQUEST_H__

