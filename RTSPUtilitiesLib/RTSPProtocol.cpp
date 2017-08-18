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
	 File:       RTSPProtocol.cpp

	 Contains:   Implementation of class defined in RTSPProtocol.h
 */

#include "RTSPProtocol.h"
#include <boost/algorithm/string/predicate.hpp>

boost::string_view RTSPProtocol::sRetrProtName("our-retransmit");

boost::string_view  RTSPProtocol::sMethods[] =
{
	"DESCRIBE",
	"SETUP",
	"TEARDOWN",
	"PLAY",
	"PAUSE",
	"OPTIONS",
	"ANNOUNCE",
	"GET_PARAMETER",
	"SET_PARAMETER",
	"REDIRECT",
	"RECORD"
};

QTSS_RTSPMethod RTSPProtocol::GetMethod(boost::string_view inMethodStr)
{
	//chances are this is one of our selected "VIP" methods. so check for this.
	QTSS_RTSPMethod theMethod = qtssIllegalMethod;

	switch (inMethodStr[0])
	{
	case 'S':   case 's':   theMethod = qtssSetupMethod;    break;
	case 'D':   case 'd':   theMethod = qtssDescribeMethod; break;
	case 'T':   case 't':   theMethod = qtssTeardownMethod; break;
	case 'O':   case 'o':   theMethod = qtssOptionsMethod;  break;
	case 'A':   case 'a':   theMethod = qtssAnnounceMethod; break;
	}

	if ((theMethod != qtssIllegalMethod) &&
		boost::iequals(inMethodStr, sMethods[theMethod]))
		return theMethod;

	for (int32_t x = qtssNumVIPMethods; x < qtssIllegalMethod; x++)
		if (boost::iequals(inMethodStr, sMethods[x]))
			return x;
	return qtssIllegalMethod;
}

boost::string_view RTSPProtocol::sHeaders[] =
{
	"Accept",
	"Cseq",
	"User-Agent",
	"Transport",
	"Session",
	"Range",

	"Accept-Encoding",
	"Accept-Language",
	"Authorization",
	"Bandwidth",
	"Blocksize",
	"Cache-Control",
	"Conference",
	"Connection",
	"Content-Base",
	"Content-Encoding",
	"Content-Language",
	"Content-length",
	"Content-Location",
	"Content-Type",
	"Date",
	"Expires",
	"From",
	"Host",
	"If-Match",
	"If-Modified-Since",
	"Last-Modified",
	"Location",
	"Proxy-Authenticate",
	"Proxy-Require",
	"Referer",
	"Retry-After",
	"Require",
	"RTP-Info",
	"Scale",
	"Speed",
	"Timestamp",
	"Vary",
	"Via",
	"Allow",
	"Public",
	"Server",
	"Unsupported",
	"WWW-Authenticate",
	",",
	"x-Retransmit",
	"x-Accept-Retransmit",
	"x-RTP-Meta-Info",
	"x-Transport-Options",
	"x-Packet-Range",
	"x-Prebuffer",
	"x-Dynamic-Rate",
	"x-Accept-Dynamic-Rate",
	// DJM PROTOTYPE
	"x-Random-Data-Size",
};

QTSS_RTSPHeader RTSPProtocol::GetRequestHeader(boost::string_view inHeaderStr)
{
	if (inHeaderStr.empty())
		return qtssIllegalHeader;

	QTSS_RTSPHeader theHeader = qtssIllegalHeader;

	//chances are this is one of our selected "VIP" headers. so check for this.
	switch (inHeaderStr[0])
	{
	case 'C':   case 'c':   theHeader = qtssCSeqHeader;         break;
	case 'S':   case 's':   theHeader = qtssSessionHeader;      break;
	case 'U':   case 'u':   theHeader = qtssUserAgentHeader;    break;
	case 'A':   case 'a':   theHeader = qtssAcceptHeader;       break;
	case 'T':   case 't':   theHeader = qtssTransportHeader;    break;
	case 'R':   case 'r':   theHeader = qtssRangeHeader;        break;
	case 'X':   case 'x':   theHeader = qtssExtensionHeaders;   break;
	}

	//
	// Check to see whether this is one of our extension headers. These
	// are very likely to appear in requests.
	if (theHeader == qtssExtensionHeaders)
		for (int32_t y = qtssExtensionHeaders; y < qtssNumHeaders; y++)
			if (boost::iequals(inHeaderStr, sHeaders[y]))
				return y;

	//
	// It's not one of our extension headers, check to see if this is one of
	// our normal VIP headers
	if (theHeader != qtssIllegalHeader && boost::iequals(inHeaderStr, sHeaders[theHeader]))
		return theHeader;

	//
	//If this isn't one of our VIP headers, go through the remaining request headers, trying
	//to find the right one.
	for (int32_t x = qtssNumVIPHeaders; x < qtssNumHeaders; x++)
		if (boost::iequals(inHeaderStr, sHeaders[x]))
			return x;
	return qtssIllegalHeader;
}



boost::string_view RTSPProtocol::sStatusCodeStrings[] =
{
	"Continue",                              //kContinue
	"OK",                                    //kSuccessOK
	"Created",                               //kSuccessCreated
	"Accepted",                              //kSuccessAccepted
	"No Content",                            //kSuccessNoContent
	"Partial Content",                       //kSuccessPartialContent
	"Low on Storage Space",                  //kSuccessLowOnStorage
	"Multiple Choices",                      //kMultipleChoices
	"Moved Permanently",                     //kRedirectPermMoved
	"Found",                                 //kRedirectTempMoved
	"See Other",                             //kRedirectSeeOther
	"Not Modified",                          //kRedirectNotModified
	"Use Proxy",                             //kUseProxy
	"Bad Request",                           //kClientBadRequest
	"Unauthorized",                          //kClientUnAuthorized
	"Payment Required",                      //kPaymentRequired
	"Forbidden",                             //kClientForbidden
	"Not Found",                             //kClientNotFound
	"Method Not Allowed",                    //kClientMethodNotAllowed
	"Not Acceptable",                        //kNotAcceptable
	"Proxy Authentication Required",         //kProxyAuthenticationRequired
	"Request Time-out",                      //kRequestTimeout
	"Conflict",                              //kClientConflict
	"Gone",                                  //kGone
	"Length Required",                       //kLengthRequired
	"Precondition Failed",                   //kPreconditionFailed
	"Request Entity Too Large",              //kRequestEntityTooLarge
	"Request-URI Too Large",                 //kRequestURITooLarge
	"Unsupported Media Type",                //kUnsupportedMediaType
	"Parameter Not Understood",              //kClientParameterNotUnderstood
	"Conference Not Found",                  //kClientConferenceNotFound
	"Not Enough Bandwidth",                  //kClientNotEnoughBandwidth
	"Session Not Found",                     //kClientSessionNotFound
	"Method Not Valid in this State",        //kClientMethodNotValidInState
	"Header Field Not Valid For Resource",   //kClientHeaderFieldNotValid
	"Invalid Range",                         //kClientInvalidRange
	"Parameter Is Read-Only",                //kClientReadOnlyParameter
	"Aggregate Option Not Allowed",          //kClientAggregateOptionNotAllowed
	"Only Aggregate Option Allowed",         //kClientAggregateOptionAllowed
	"Unsupported Transport",                 //kClientUnsupportedTransport
	"Destination Unreachable",               //kClientDestinationUnreachable
	"Internal Server Error",                 //kServerInternal
	"Not Implemented",                       //kServerNotImplemented
	"Bad Gateway",                           //kServerBadGateway
	"Service Unavailable",                   //kServerUnavailable
	"Gateway Timeout",                       //kServerGatewayTimeout
	"RTSP Version not supported",            //kRTSPVersionNotSupported
	"Option Not Supported"                   //kServerOptionNotSupported
};

int32_t RTSPProtocol::sStatusCodes[] =
{
	100,        //kContinue
	200,        //kSuccessOK
	201,        //kSuccessCreated
	202,        //kSuccessAccepted
	204,        //kSuccessNoContent
	206,        //kSuccessPartialContent
	250,        //kSuccessLowOnStorage
	300,        //kMultipleChoices
	301,        //kRedirectPermMoved
	302,        //kRedirectTempMoved
	303,        //kRedirectSeeOther
	304,        //kRedirectNotModified
	305,        //kUseProxy
	400,        //kClientBadRequest
	401,        //kClientUnAuthorized
	402,        //kPaymentRequired
	403,        //kClientForbidden
	404,        //kClientNotFound
	405,        //kClientMethodNotAllowed
	406,        //kNotAcceptable
	407,        //kProxyAuthenticationRequired
	408,        //kRequestTimeout
	409,        //kClientConflict
	410,        //kGone
	411,        //kLengthRequired
	412,        //kPreconditionFailed
	413,        //kRequestEntityTooLarge
	414,        //kRequestURITooLarge
	415,        //kUnsupportedMediaType
	451,        //kClientParameterNotUnderstood
	452,        //kClientConferenceNotFound
	453,        //kClientNotEnoughBandwidth
	454,        //kClientSessionNotFound
	455,        //kClientMethodNotValidInState
	456,        //kClientHeaderFieldNotValid
	457,        //kClientInvalidRange
	458,        //kClientReadOnlyParameter
	459,        //kClientAggregateOptionNotAllowed
	460,        //kClientAggregateOptionAllowed
	461,        //kClientUnsupportedTransport
	462,        //kClientDestinationUnreachable
	500,        //kServerInternal
	501,        //kServerNotImplemented
	502,        //kServerBadGateway
	503,        //kServerUnavailable
	504,        //kServerGatewayTimeout
	505,        //kRTSPVersionNotSupported
	551         //kServerOptionNotSupported
};

boost::string_view RTSPProtocol::sStatusCodeAsStrings[] =
{
	"100",       //kContinue
	"200",       //kSuccessOK
	"201",       //kSuccessCreated
	"202",       //kSuccessAccepted
	"204",       //kSuccessNoContent
	"206",       //kSuccessPartialContent
	"250",       //kSuccessLowOnStorage
	"300",       //kMultipleChoices
	"301",       //kRedirectPermMoved
	"302",       //kRedirectTempMoved
	"303",       //kRedirectSeeOther
	"304",       //kRedirectNotModified
	"305",       //kUseProxy
	"400",       //kClientBadRequest
	"401",       //kClientUnAuthorized
	"402",       //kPaymentRequired
	"403",       //kClientForbidden
	"404",       //kClientNotFound
	"405",       //kClientMethodNotAllowed
	"406",       //kNotAcceptable
	"407",       //kProxyAuthenticationRequired
	"408",       //kRequestTimeout
	"409",       //kClientConflict
	"410",       //kGone
	"411",       //kLengthRequired
	"412",       //kPreconditionFailed
	"413",       //kRequestEntityTooLarge
	"414",       //kRequestURITooLarge
	"415",       //kUnsupportedMediaType
	"451",       //kClientParameterNotUnderstood
	"452",       //kClientConferenceNotFound
	"453",       //kClientNotEnoughBandwidth
	"454",       //kClientSessionNotFound
	"455",       //kClientMethodNotValidInState
	"456",       //kClientHeaderFieldNotValid
	"457",       //kClientInvalidRange
	"458",       //kClientReadOnlyParameter
	"459",       //kClientAggregateOptionNotAllowed
	"460",       //kClientAggregateOptionAllowed
	"461",       //kClientUnsupportedTransport
	"462",       //kClientDestinationUnreachable
	"500",       //kServerInternal
	"501",       //kServerNotImplemented
	"502",       //kServerBadGateway
	"503",       //kServerUnavailable
	"504",       //kServerGatewayTimeout
	"505",       //kRTSPVersionNotSupported
	"551"        //kServerOptionNotSupported
};

boost::string_view RTSPProtocol::sVersionString[] =
{
	"RTSP/1.0"
};

RTSPProtocol::RTSPVersion RTSPProtocol::GetVersion(boost::string_view versionStr)
{
	if (versionStr.length() != 8)
		return kIllegalVersion;
	else
		return k10Version;
}

static void copyUsernameOrPasswordStringFromURL(char* dest, char const* src, unsigned len) {
	// Normally, we just copy from the source to the destination.  However, if the source contains
	// %-encoded characters, then we decode them while doing the copy:
	while (len > 0) {
		int nBefore = 0;
		int nAfter = 0;

		if (*src == '%' && len >= 3 && sscanf(src + 1, "%n%2hhx%n", &nBefore, dest, &nAfter) == 1) {
			unsigned codeSize = nAfter - nBefore; // should be 1 or 2

			++dest;
			src += (1 + codeSize);
			len -= (1 + codeSize);
		}
		else {
			*dest++ = *src++;
			--len;
		}
	}
	*dest = '\0';
}

// Parse the URL as "rtsp://[<username>[:<password>]@]<server-address-or-name>[:<port>][/<stream-name>]"
bool RTSPProtocol::ParseRTSPURL(char const* url, char* username, char* password, char* ip, uint16_t* port, char const** urlSuffix)
{
	do {
		
		char const* prefix = "rtsp://";
		unsigned const prefixLength = 7;
#ifdef WIN32
		if (_strnicmp(url, prefix, prefixLength) != 0) {
#else
		if (strncasecmp(url, prefix, prefixLength) != 0) {
#endif
			printf("URL is not of the form rtsp://\n");
			break;
		}

		unsigned const parseBufferSize = 100;
		char parseBuffer[parseBufferSize];
		char const* from = &url[prefixLength];

		// Check whether "<username>[:<password>]@" occurs next.
		// We do this by checking whether '@' appears before the end of the URL, or before the first '/'.
		char const* colonPasswordStart = nullptr;
		char const* p;
		for (p = from; *p != '\0' && *p != '/'; ++p) {
			if (*p == ':' && colonPasswordStart == nullptr) {
				colonPasswordStart = p;
			}
			else if (*p == '@') {
				// We found <username> (and perhaps <password>).  Copy them into newly-allocated result strings:
				if (colonPasswordStart == nullptr) colonPasswordStart = p;

				char const* usernameStart = from;
				unsigned usernameLen = colonPasswordStart - usernameStart;
				if(username)
					copyUsernameOrPasswordStringFromURL(username, usernameStart, usernameLen);

				char const* passwordStart = colonPasswordStart;
				if (passwordStart < p) ++passwordStart; // skip over the ':'
				unsigned passwordLen = p - passwordStart;
				if(password)
					copyUsernameOrPasswordStringFromURL(password, passwordStart, passwordLen);

				from = p + 1; // skip over the '@'
				break;
			}
		}

		// Next, parse <server-address-or-name>
		char* to = &parseBuffer[0];
		unsigned i;
		for (i = 0; i < parseBufferSize; ++i) {
			if (*from == '\0' || *from == ':' || *from == '/') {
				// We've completed parsing the address
				*to = '\0';
				break;
			}
			*to++ = *from++;
		}
		if (i == parseBufferSize) {
			printf("URL is too long");
			break;
		}

		if (ip)
			strncpy(ip, parseBuffer, i);

		*port = 554; // default value
		char nextChar = *from;
		if (nextChar == ':') {
			int portNumInt;
			if (sscanf(++from, "%d", &portNumInt) != 1) {
				printf("No port number follows ':'");
				break;
			}
			if (portNumInt < 1 || portNumInt > 65535) {
				printf("Bad port number");
				break;
			}
			*port = portNumInt;
			while (*from >= '0' && *from <= '9') ++from; // skip over port number
		}

		// The remainder of the URL is the suffix:
		if (urlSuffix != nullptr) *urlSuffix = from;

		return true;
	} while (0);

	return false;
}
