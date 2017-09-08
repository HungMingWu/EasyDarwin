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
	void    ParseTransportOptionsHeader(boost::string_view header);
	void    ParseSessionHeader(boost::string_view header);
	void    ParseModeSubHeader(boost::string_view inModeSubHeader);
	bool    ParseNetworkModeSubHeader(boost::string_view inSubHeader);
	void 	ParseDynamicRateHeader(boost::string_view header);
	void    ParseBandwidthHeader(boost::string_view header);
};

class Content : public std::istream {
	friend class RTSPRequest1;
public:
	size_t size() noexcept {
		return streambuf.size();
	}
	/// Convenience function to return std::string. The stream buffer is consumed.
	std::string string() noexcept {
		try {
			std::stringstream ss;
			ss << rdbuf();
			return ss.str();
		}
		catch (...) {
			return std::string();
		}
	}

private:
	boost::asio::streambuf &streambuf;
	Content(boost::asio::streambuf &streambuf) noexcept : std::istream(&streambuf), streambuf(streambuf) {}
};

inline bool case_insensitive_equal(const std::string &str1, const std::string &str2) noexcept {
	return str1.size() == str2.size() &&
		std::equal(str1.begin(), str1.end(), str2.begin(), [](char a, char b) {
		return tolower(a) == tolower(b);
	});
}
class CaseInsensitiveEqual {
public:
	bool operator()(const std::string &str1, const std::string &str2) const noexcept {
		return case_insensitive_equal(str1, str2);
	}
};

// Based on https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x/2595226#2595226
class CaseInsensitiveHash {
public:
	size_t operator()(const std::string &str) const noexcept {
		size_t h = 0;
		std::hash<int> hash;
		for (auto c : str)
			h ^= hash(tolower(c)) + 0x9e3779b9 + (h << 6) + (h >> 2);
		return h;
	}
};

typedef std::unordered_map<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual> CaseInsensitiveMap;
class RTSPHeader {
public:
	/// Parse header fields
	static void parse(std::istream &stream, CaseInsensitiveMap &header) noexcept {
		std::string line;
		getline(stream, line);
		size_t param_end;
		while ((param_end = line.find(':')) != std::string::npos) {
			size_t value_start = param_end + 1;
			if (value_start < line.size()) {
				if (line[value_start] == ' ')
					value_start++;
				if (value_start < line.size())
					header.emplace(line.substr(0, param_end), line.substr(value_start, line.size() - value_start - 1));
			}

			getline(stream, line);
		}
	}
};

class RequestMessage;

class RTSPRequest1 {
	friend class RTSPServer;
	friend class RequestMessage;
	boost::asio::streambuf streambuf;
	Content content;
	std::string method, path, query_string, rtsp_version;
	std::string remote_endpoint_address;
	unsigned short remote_endpoint_port;
	CaseInsensitiveMap header;
	QTSS_RTPNetworkMode fNetworkMode;
	QTSS_RTPTransportType fTransportType;
	QTSS_RTPTransportMode fTransportMode;
public:
	RTSPRequest1(const std::string &remote_endpoint_address = std::string(), unsigned short remote_endpoint_port = 0) noexcept
		: content(streambuf), remote_endpoint_address(remote_endpoint_address), remote_endpoint_port(remote_endpoint_port) {}

	~RTSPRequest1() = default;
	bool IsPushRequest() { return (fTransportMode == qtssRTPTransportModeRecord) ? true : false; }
};

class RequestMessage {
	static bool ParseNetworkModeSubHeader(boost::string_view inModeSubHeader, RTSPRequest1& req);
	static bool ParseModeSubHeader(boost::string_view inModeSubHeader, RTSPRequest1& req);
	static bool parse_setup(RTSPRequest1& req);
public:
	/// Parse request line and header fields
	static bool parse(std::istream &stream, RTSPRequest1& req) noexcept {
		req.header.clear();
		std::string line;
		getline(stream, line);
		size_t method_end;
		if ((method_end = line.find(' ')) != std::string::npos) {
			req.method = line.substr(0, method_end);

			size_t query_start = std::string::npos;
			size_t path_and_query_string_end = std::string::npos;
			for (size_t i = method_end + 1; i < line.size(); ++i) {
				if (line[i] == '?' && (i + 1) < line.size())
					query_start = i + 1;
				else if (line[i] == ' ') {
					path_and_query_string_end = i;
					break;
				}
			}
			if (path_and_query_string_end != std::string::npos) {
				if (query_start != std::string::npos) {
					req.path = line.substr(method_end + 1, query_start - method_end - 2);
					req.query_string = line.substr(query_start, path_and_query_string_end - query_start);
				}
				else
					req.path = line.substr(method_end + 1, path_and_query_string_end - method_end - 1);

				size_t protocol_end;
				if ((protocol_end = line.find('/', path_and_query_string_end + 1)) != std::string::npos) {
					if (line.compare(path_and_query_string_end + 1, protocol_end - path_and_query_string_end - 1, "RTSP") != 0)
						return false;
					req.rtsp_version = line.substr(protocol_end + 1, line.size() - protocol_end - 2);
				}
				else
					return false;

				RTSPHeader::parse(stream, req.header);
				auto it = req.header.find("CSeq");
				if (it == req.header.end())
					return false;
				if (req.method == "SETUP")
					return parse_setup(req);
			}
			else
				return false;
		}
		else
			return false;
		return true;
	}
};



#endif // __RTSPREQUEST_H__

