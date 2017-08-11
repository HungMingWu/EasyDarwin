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
	 File:       RTSPRequest.cpp

	 Contains:   Implementation of RTSPRequest class.
 */

#include <map>
#include <boost/utility/string_view.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/fusion/include/std_pair.hpp>

#include "RTSPRequest.h"
#include "RTSPProtocol.h"
#include "QTSServerInterface.h"

#include "RTSPSession.h"
#include "RTSPSessionInterface.h"
#include "StringParser.h"
#include "StringTranslator.h"
#include "QTSS.h"
#include "QTSSModuleUtils.h"
#include "base64.h"
#include "DateTranslator.h"
#include "SocketUtils.h"

static boost::string_view    sDefaultRealm("Streaming Server");
static StrPtrLen    sAuthBasicStr("Basic", 5);
static StrPtrLen    sAuthDigestStr("Digest", 6);
static StrPtrLen    sUsernameStr("username", 8);
static StrPtrLen    sRealmStr("realm", 5);
static StrPtrLen    sNonceStr("nonce", 5);
static StrPtrLen    sUriStr("uri", 3);
static StrPtrLen    sQopStr("qop", 3);
static StrPtrLen    sQopAuthStr("auth", 4);
static StrPtrLen    sQopAuthIntStr("auth-int", 8);
static StrPtrLen    sNonceCountStr("nc", 2);
static StrPtrLen    sResponseStr("response", 8);
static StrPtrLen    sOpaqueStr("opaque", 6);
static StrPtrLen    sEqualQuote("=\"", 2);
static StrPtrLen    sQuoteCommaSpace("\", ", 3);
static StrPtrLen    sStaleTrue("stale=\"true\", ", 14);

typedef std::map<std::string, std::string> header_fields_t;

struct RTSPRequestHeader
{
	std::string method;
	std::string uri;
	std::string rtsp_version;

	header_fields_t header_fields;
};

namespace qi = boost::spirit::qi;

BOOST_FUSION_ADAPT_STRUCT(RTSPRequestHeader, method, uri, rtsp_version, header_fields)

template <typename Iterator, typename Skipper = qi::ascii::blank_type>
struct RTSPHeaderGrammar : qi::grammar <Iterator, RTSPRequestHeader(), Skipper> {
	RTSPHeaderGrammar() : RTSPHeaderGrammar::base_type(rtsp_header, "RTSPHeaderGrammar Grammar") {
		method = +qi::alpha;
		uri = +qi::graph;
		rtsp_ver = "RTSP/" >> +qi::char_("0-9.");

		field_key = +qi::char_("0-9a-zA-Z-");
		field_value = +~qi::char_("\r\n");

		fields = *(field_key >> ':' >> field_value >> qi::lexeme["\r\n"]);

		rtsp_header = method >> uri >> rtsp_ver >> qi::lexeme["\r\n"] >> fields >> qi::lexeme["\r\n"];
		BOOST_SPIRIT_DEBUG_NODES((method)(uri)(rtsp_ver)(fields)(rtsp_header))
	}
private:
	qi::rule<Iterator, std::map<std::string, std::string>(), Skipper> fields;
	qi::rule<Iterator, RTSPRequestHeader(), Skipper> rtsp_header;
	// lexemes
	qi::rule<Iterator, std::string()> method, uri, rtsp_ver;
	qi::rule<Iterator, std::string()> field_key, field_value;
};

//Parses the request
QTSS_Error RTSPRequest::Parse()
{
	fHeaderDict.clear();
	boost::string_view requestHeader = GetFullRequest();
	typedef boost::string_view::const_iterator It;
	RTSPHeaderGrammar<It> rtspGrammar;
	It iter = requestHeader.begin(), end = requestHeader.end();
	RTSPRequestHeader rtspHeader;
	bool r = phrase_parse(iter, end, rtspGrammar, qi::ascii::blank, rtspHeader);

	if (r && iter == end) {
		std::cout << "Parsing succeeded\n";
		//check the version
		//fVersion = RTSPProtocol::GetVersion(rtspHeader.rtsp_version);
		//if (fVersion != RTSPProtocol::RTSPVersion::k10Version)
			//return QTSSModuleUtils::SendErrorResponse(this, qtssClientBadRequest, qtssMsgBadRTSPRequest, nullptr);
	}
	else {
		std::cout << "Parsing failed\n";
		std::cout << "stopped at: \"" << std::string(iter, end) << "\"\n";
		//return QTSSModuleUtils::SendErrorResponse(this, qtssClientBadRequest, qtssMsgBadRTSPRequest, nullptr);
	}

	//parse status line.
	QTSS_Error error = ParseFirstLine(rtspHeader.method, rtspHeader.uri, rtspHeader.rtsp_version);

	//handle any errors that come up    
	if (error != QTSS_NoErr)
		return error;

	error = this->ParseHeaders(rtspHeader.header_fields);
	if (error != QTSS_NoErr)
		return error;

	//Response headers should set themselves up to reflect what's in the request headers
	fResponseKeepAlive = fRequestKeepAlive;

	//Make sure that there was some path that was extracted from this request. If not, there is no way
	//we can process the request, so generate an error
	if (GetAbsolutePath().empty())
		return QTSSModuleUtils::SendErrorResponse(this, qtssClientBadRequest, qtssMsgNoURLInRequest, nullptr);

	return QTSS_NoErr;
}

//returns: StatusLineTooLong, SyntaxError, BadMethod
QTSS_Error RTSPRequest::ParseFirstLine(boost::string_view method, boost::string_view fulluri, boost::string_view ver)
{
	fMethod = RTSPProtocol::GetMethod(method);
	if (fMethod == qtssIllegalMethod)
		return QTSSModuleUtils::SendErrorResponse(this, qtssClientBadRequest, qtssMsgBadRTSPMethod, nullptr);

	//now parse the uri,for example rtsp://www.easydarwin.org:554/live.sdp?channel=1&token=888888
	QTSS_Error err = ParseURI(fulluri);
	if (err != QTSS_NoErr)
		return err;

	//check the version
	fVersion = RTSPProtocol::GetVersion(ver);

	return QTSS_NoErr;
}

//returns: SyntaxError if there was an error in the uri. Or InternalServerError
QTSS_Error RTSPRequest::ParseURI(boost::string_view fulluri)
{
	//for example: rtsp://www.easydarwin.org:554/live.sdp?channel=1&token=888888
	//read in the complete URL, set it to be the qtssAbsoluteURLParam
	//qtssRTSPReqAbsoluteURL = rtsp://www.easydarwin.org:554/live.sdp?channel=1&token=888888
	SetAbsoluteURL(fulluri);
	//theAbsURL = rtsp://www.easydarwin.org:554/live.sdp
	std::string theAbsURL;
	bool r = qi::phrase_parse(fulluri.cbegin(), fulluri.cend(),
		+(qi::char_ - "?") >> -("?" >> +(qi::char_)), qi::ascii::blank, theAbsURL, queryString);
	//we always should have a slash before the uri.
	//If not, that indicates this is a full URI. Also, this could be a '*' OPTIONS request
	if (theAbsURL[0] != '/' && theAbsURL[0] != '*')
	{
		std::string theHost;
		bool r = qi::phrase_parse(theAbsURL.cbegin(), theAbsURL.cend(),
			qi::no_case["RTSP://"] >> *(qi::char_ - "/") >> (qi::eoi | *(qi::char_)),  
			qi::ascii::blank, theHost, uriPath);

		fHeaderDict.Set(qtssHostHeader, theHost);
		if (uriPath.empty()) {
			static char* sSlashURI = "/";
			uriPath = sSlashURI;
		}
	}

	// don't allow non-aggregate operations indicated by a url/media track=id
	// might need this for rate adapt   if (qtssSetupMethod != fMethod && qtssOptionsMethod != fMethod && qtssSetParameterMethod != fMethod) // any method not a setup, options, or setparameter is not allowed to have a "/trackID=" in the url.
	if (qtssSetupMethod != fMethod) // any method not a setup is not allowed to have a "/trackID=" in the url.
	{
		if (theAbsURL.find("/trackID=") != std::string::npos) // check for non-aggregate method and return error
			return QTSSModuleUtils::SendErrorResponse(this, qtssClientAggregateOptionAllowed, qtssMsgBadRTSPMethod, nullptr);
	}

	// don't allow non-aggregate operations like a setup on a playing session
	if (qtssSetupMethod == fMethod) // if it is a setup but we are playing don't allow it
	{
		auto*  theSession = (RTSPSession*)this->GetSession();
		if (theSession != nullptr && theSession->IsPlaying())
			return QTSSModuleUtils::SendErrorResponse(this, qtssClientAggregateOptionAllowed, qtssMsgBadRTSPMethod, nullptr);
	}

	//
	// If the is a '*', return right now because '*' is not a path
	// so the below functions don't make any sense.
	if (theAbsURL == "*")
	{
		SetAbsolutePath(theAbsURL);

		return QTSS_NoErr;
	}

	//path strings are statically allocated. Therefore, if they are longer than
	//this length we won't be able to handle the request.
	boost::string_view theURLParam = GetURI();
	StrPtrLen theURLParamV((char *)theURLParam.data(), theURLParam.length());
	if (theURLParam.length() > RTSPRequestInterface::kMaxFilePathSizeInBytes)
		return QTSSModuleUtils::SendErrorResponse(this, qtssClientBadRequest, qtssMsgURLTooLong, &theURLParamV);

	//decode the URL, put the result in the separate buffer for the file path,
	//set the file path StrPtrLen to the proper value
	int32_t theBytesWritten = StringTranslator::DecodeURL((char *)theURLParam.data(), theURLParam.length(),
		fFilePath, RTSPRequestInterface::kMaxFilePathSizeInBytes);
	//if negative, an error occurred, reported as an QTSS_Error
	//we also need to leave room for a terminator.
	if ((theBytesWritten < 0) || (theBytesWritten == RTSPRequestInterface::kMaxFilePathSizeInBytes))
	{
		return QTSSModuleUtils::SendErrorResponse(this, qtssClientBadRequest, qtssMsgURLInBadFormat, &theURLParamV);
	}

	// Convert from a / delimited path to a local file system path
	StringTranslator::DecodePath(fFilePath, theBytesWritten);

	//setup the proper QTSS param
	fFilePath[theBytesWritten] = '\0';
	//this->SetVal(qtssRTSPReqFilePath, fFilePath, theBytesWritten);
	SetAbsolutePath({ (char *)fFilePath, (size_t)theBytesWritten });

	return QTSS_NoErr;
}


//throws eHTTPNoMoreData and eHTTPOutOfBuffer
QTSS_Error RTSPRequest::ParseHeaders(const std::map<std::string, std::string>& headers)
{
	for (const auto pairs : headers)
	{
		//Look up the proper header enumeration based on the header string.
		//Use the enumeration to look up the dictionary ID of this header,
		//and set that dictionary attribute to be whatever is in the body of the header
		boost::string_view theKeyWord = pairs.first;
		uint32_t theHeader = RTSPProtocol::GetRequestHeader(theKeyWord);
		boost::string_view theHeaderVal = pairs.second;

		// If this is an unknown header, ignore it. Otherwise, set the proper
		// dictionary attribute
		if (theHeader != qtssIllegalHeader)
		{
			Assert(theHeader < qtssNumHeaders);
			fHeaderDict.Set(theHeader, std::string(theHeaderVal));
		}

		//some headers require some special processing. If this code begins
		//to get out of control, we made need to come up with a function pointer table
		switch (theHeader)
		{
		case qtssSessionHeader:             ParseSessionHeader(theHeaderVal); break;
		case qtssTransportHeader:           ParseTransportHeader(theHeaderVal); break;
		case qtssRangeHeader:               ParseRangeHeader(theHeaderVal);     break;
		case qtssIfModifiedSinceHeader:     ParseIfModSinceHeader(theHeaderVal); break;
		case qtssXRetransmitHeader:         ParseRetransmitHeader(theHeaderVal); break;
		case qtssContentLengthHeader:       ParseContentLengthHeader(theHeaderVal); break;
		case qtssSpeedHeader:               ParseSpeedHeader(theHeaderVal);     break;
		case qtssXTransportOptionsHeader:   ParseTransportOptionsHeader(theHeaderVal); break;
		case qtssXPreBufferHeader:          ParsePrebufferHeader(theHeaderVal); break;
		case qtssXDynamicRateHeader:		ParseDynamicRateHeader(theHeaderVal); break;
		case qtssXRandomDataSizeHeader:		ParseRandomDataSizeHeader(theHeaderVal); break;
		case qtssBandwidthHeader:           ParseBandwidthHeader(theHeaderVal); break;
		default:    break;
		}
	}

	// Tell the session what the request body length is for this request
	// so that it can prevent people from reading past the end of the request.
	std::string theContentLengthBody(fHeaderDict.Get(qtssContentLengthHeader));
	if (!theContentLengthBody.empty())
	{
		this->GetSession()->SetRequestBodyLength(std::stoi(theContentLengthBody));
	}

	return QTSS_NoErr;
}

void RTSPRequest::ParseSessionHeader(boost::string_view header)
{
	std::string theSessionID;
	auto iter = header.cbegin(), end = header.cend();
	bool r = qi::phrase_parse(iter, end, *(qi::digit), qi::lit(";") | qi::ascii::blank, theSessionID);
	if (r && iter == end) {
		fHeaderDict.Set(qtssSessionHeader, theSessionID);
	}
}

bool RTSPRequest::ParseNetworkModeSubHeader(boost::string_view inSubHeader)
{
	static boost::string_view sUnicast("unicast");
	static boost::string_view sMulticast("multiicast");

	if (boost::iequals(inSubHeader, sUnicast))
	{
		fNetworkMode = qtssRTPNetworkModeUnicast;
		return true;
	}

	if (boost::iequals(inSubHeader, sMulticast))
	{
		fNetworkMode = qtssRTPNetworkModeMulticast;
		return true;
	}

	return false;
}

template <typename S>
static std::vector<std::string> spirit_direct(const S& input, char const* delimiter)
{
	std::vector<std::string> result;
	if (!qi::parse(input.begin(), input.end(), 
		qi::raw[*(qi::char_ - qi::char_(delimiter))] % qi::char_(delimiter), result))
		result.push_back(std::string(input));
	return result;
}

void RTSPRequest::ParseTransportHeader(boost::string_view header)
{
	static boost::string_view sRTPAVPTransportStr = "RTP/AVP";

	//transport header from client: Transport: RTP/AVP;unicast;client_port=5000-5001\r\n
	//                              Transport: RTP/AVP;multicast;ttl=15;destination=229.41.244.93;client_port=5000-5002\r\n
	//                              Transport: RTP/AVP/TCP;unicast\r\n

	//
	// A client may send multiple transports to the server, comma separated.
	// In this case, the server should just pick one and use that. 
	std::vector<std::string> transports = spirit_direct(header, ",");
	for (const auto &transport : transports)
		if (boost::istarts_with(transport, sRTPAVPTransportStr)) {
			fFirstTransport = transport;
			break;
		}

	std::vector<std::string> SubHeaders = spirit_direct(fFirstTransport, ";");
	for (const auto &subHeader : SubHeaders)
	{
		// Extract the relevent information from the relevent subheader.
		// So far we care about 3 sub-headers

		if (!ParseNetworkModeSubHeader(subHeader))
		{
			switch (subHeader[0])
			{
			case 'r':	// rtp/avp/??? Is this tcp or udp?
			case 'R':   // RTP/AVP/??? Is this TCP or UDP?
				{
					if (boost::iequals(subHeader, "RTP/AVP/TCP"))
						fTransportType = qtssRTPTransportTypeTCP;
					break;
				}
			case 'c':   //client_port sub-header
			case 'C':   //client_port sub-header
				{
					ParseClientPortSubHeader(subHeader);
					break;
				}
			case 'd':   //destination sub-header
			case 'D':   //destination sub-header
				{
					static boost::string_view sDestinationSubHeader("destination");

					//Parse the header, extract the destination address
					ParseAddrSubHeader(subHeader, sDestinationSubHeader, &fDestinationAddr);
					break;
				}
			case 's':   //source sub-header
			case 'S':   //source sub-header
				{
					//Same as above code
					static boost::string_view sSourceSubHeader("source");
					ParseAddrSubHeader(subHeader, sSourceSubHeader, &fSourceAddr);
					break;
				}
			case 't':   //time-to-live sub-header
			case 'T':   //time-to-live sub-header
				{
					ParseTimeToLiveSubHeader(subHeader);
					break;
				}
			case 'm':   //mode sub-header
			case 'M':   //mode sub-header
				{
					ParseModeSubHeader(subHeader);
					break;
				}
			}
		}
	}
}

void  RTSPRequest::ParseRangeHeader(boost::string_view header)
{
	StrPtrLen t((char *)header.data(), header.length());
	StringParser theRangeParser(&t);

	theRangeParser.GetThru(nullptr, '=');//consume "npt="
	theRangeParser.ConsumeWhitespace();
	fStartTime = (double)theRangeParser.ConsumeNPT();
	//see if there is a stop time as well.
	if (theRangeParser.GetDataRemaining() > 1)
	{
		theRangeParser.GetThru(nullptr, '-');
		theRangeParser.ConsumeWhitespace();
		fStopTime = (double)theRangeParser.ConsumeNPT();
	}
}

void  RTSPRequest::ParseRetransmitHeader(boost::string_view header)
{
	StrPtrLen t2((char *)header.data(), header.length());
	StringParser theRetransmitParser(&t2);
	StrPtrLen theProtName;
	bool foundRetransmitProt = false;

	do
	{
		theRetransmitParser.ConsumeWhitespace();
		theRetransmitParser.ConsumeWord(&theProtName);
		theProtName.TrimTrailingWhitespace();
		foundRetransmitProt = theProtName.EqualIgnoreCase(RTSPProtocol::GetRetransmitProtocolName());
	} while ((!foundRetransmitProt) &&
		(theRetransmitParser.GetThru(nullptr, ',')));

	if (!foundRetransmitProt)
		return;

	//
	// We are using Reliable RTP as the transport for this stream,
	// but if there was a previous transport header that indicated TCP,
	// do not set the transport to be reliable UDP
	if (fTransportType == qtssRTPTransportTypeUDP)
		fTransportType = qtssRTPTransportTypeReliableUDP;

	StrPtrLen theProtArg;
	while (theRetransmitParser.GetThru(&theProtArg, '='))
	{
		//
		// Parse out params
		static const StrPtrLen kWindow("window");

		theProtArg.TrimWhitespace();
		if (theProtArg.EqualIgnoreCase(kWindow))
		{
			theRetransmitParser.ConsumeWhitespace();
			fWindowSize = theRetransmitParser.ConsumeInteger(nullptr);

			// Save out the window size argument as a string so we
			// can easily put it into the response
			// (we never muck with this header)
			fWindowSizeStr.Ptr = theProtArg.Ptr;
			fWindowSizeStr.Len = theRetransmitParser.GetCurrentPosition() - theProtArg.Ptr;
		}

		theRetransmitParser.GetThru(nullptr, ';'); //Skip past ';'
	}
}

void  RTSPRequest::ParseContentLengthHeader(boost::string_view header)
{
	auto iter = header.cbegin(), end = header.cend();
	bool r = qi::phrase_parse(iter, end, qi::uint_, qi::ascii::blank, fContentLength);
}

void  RTSPRequest::ParsePrebufferHeader(boost::string_view header)
{
	std::vector<std::string> splitTokens = spirit_direct(header, ";");
	for (const auto &token : splitTokens) {
		std::string name;
		float temp;
		bool r = qi::phrase_parse(token.cbegin(), token.cend(), *(qi::alpha - "=") >> "=" >> qi::float_, qi::ascii::blank,
			name, temp);
		if (r && boost::iequals(name, "maxtime"))
			fPrebufferAmt = temp;
	}
}

void  RTSPRequest::ParseDynamicRateHeader(boost::string_view header)
{
	auto iter = header.cbegin(), end = header.cend();
	int32_t value = 0;
	bool r = qi::phrase_parse(iter, end, qi::int_, qi::ascii::blank, value);

	// fEnableDynamicRate: < 0 undefined, 0 disable, > 0 enable
	if (value > 0)
		fEnableDynamicRateState = 1;
	else
		fEnableDynamicRateState = 0;
}

void  RTSPRequest::ParseIfModSinceHeader(boost::string_view header)
{
	StrPtrLen t((char *)header.data(), header.length());
	fIfModSinceDate = DateTranslator::ParseDate(&t);
}

void RTSPRequest::ParseSpeedHeader(boost::string_view header)
{
	auto iter = header.cbegin(), end = header.cend();
	bool r = qi::phrase_parse(iter, end, qi::float_, qi::ascii::blank, fSpeed);
}

void RTSPRequest::ParseTransportOptionsHeader(boost::string_view header)
{
	StrPtrLen t((char *)header.data(), header.length());
	StringParser theRTPOptionsParser(&t);
	StrPtrLen theRTPOptionsSubHeader;

	do
	{
		static StrPtrLen sLateTolerance("late-tolerance");

		if (theRTPOptionsSubHeader.NumEqualIgnoreCase(sLateTolerance.Ptr, sLateTolerance.Len))
		{
			StringParser theLateTolParser(&theRTPOptionsSubHeader);
			theLateTolParser.GetThru(nullptr, '=');
			theLateTolParser.ConsumeWhitespace();
			fLateTolerance = theLateTolParser.ConsumeFloat();
			fLateToleranceStr = std::string(theRTPOptionsSubHeader.Ptr, theRTPOptionsSubHeader.Len);
		}

		(void)theRTPOptionsParser.GetThru(&theRTPOptionsSubHeader, ';');

	} while (theRTPOptionsSubHeader.Len > 0);
}


void RTSPRequest::ParseAddrSubHeader(boost::string_view inSubHeader, boost::string_view inHeaderName, uint32_t* outAddr)
{
	if (inSubHeader.empty() || inHeaderName.empty() || !outAddr)
		return;

	std::string name, theAddr;
	bool r = qi::phrase_parse(inSubHeader.cbegin(), inSubHeader.cend(),
		*(qi::alpha - "=") >> "=" >> *(qi::char_), qi::ascii::blank,
		name, theAddr);

	// First make sure this is the proper subheader
	if (!boost::iequals(name, inHeaderName))
		return;

	*outAddr = SocketUtils::ConvertStringToAddr(theAddr.c_str());
}

void RTSPRequest::ParseModeSubHeader(boost::string_view inModeSubHeader)
{
	static boost::string_view sModeSubHeader("mode");
	static boost::string_view sReceiveMode("receive");
	static boost::string_view sRecordMode("record");

	std::string name, mode;
	bool r = qi::phrase_parse(inModeSubHeader.cbegin(), inModeSubHeader.cend(),
		*(qi::alpha - "=") >> "=" >> *(qi::char_), qi::ascii::blank,
		name, mode);
	if (r && boost::iequals(name, sModeSubHeader)) {
		if (boost::iequals(mode, sReceiveMode) || boost::iequals(mode, sRecordMode))
			fTransportMode = qtssRTPTransportModeRecord;
	}
}

void RTSPRequest::ParseClientPortSubHeader(boost::string_view inClientPortSubHeader)
{
	static boost::string_view sClientPortSubHeader("client_port");
	static boost::string_view sErrorMessage("Received invalid client_port field: ");
	uint16_t portA, portB;
	std::string name;
	bool r = qi::phrase_parse(inClientPortSubHeader.cbegin(), inClientPortSubHeader.cend(),
		*(qi::alpha - "=") >> "=" >> qi::ushort_ >> "-" >> qi::ushort_, qi::ascii::blank,
		name, portA, portB);
	if (r && boost::iequals(name, sClientPortSubHeader)) {
		fClientPortA = portA;
		fClientPortB = portB;
	}
	if (fClientPortB != fClientPortA + 1) // an error in the port values
	{
		// The following to setup and log the error as a message level 2.
		boost::string_view userAgent = fHeaderDict.Get(qtssUserAgentHeader);
		ResizeableStringFormatter errorPortMessage;
		errorPortMessage.Put(sErrorMessage);
		if (!userAgent.empty())
			errorPortMessage.Put(userAgent);
		errorPortMessage.PutSpace();
		errorPortMessage.Put(inClientPortSubHeader);
		errorPortMessage.PutTerminator();
		QTSSModuleUtils::LogError(qtssMessageVerbosity, qtssMsgNoMessage, 0, errorPortMessage.GetBufPtr(), nullptr);


		//fix the rtcp port and hope it works.
		fClientPortB = fClientPortA + 1;
	}
}

void RTSPRequest::ParseTimeToLiveSubHeader(boost::string_view inTimeToLiveSubHeader)
{
	static boost::string_view sTimeToLiveSubHeader("ttl");

	std::string name;
	uint16_t temp;
	bool r = qi::phrase_parse(inTimeToLiveSubHeader.cbegin(), inTimeToLiveSubHeader.cend(), *(qi::alpha - "=") >> "=" >> qi::ushort_, qi::ascii::blank,
		name, temp);
	if (r && boost::iequals(name, sTimeToLiveSubHeader))
		fTtl = temp;
}

// DJM PROTOTYPE
void  RTSPRequest::ParseRandomDataSizeHeader(boost::string_view header)
{
	auto iter = header.cbegin(), end = header.cend();
	bool r = qi::phrase_parse(iter, end, qi::uint_, qi::ascii::blank, fRandomDataSize);

	if (fRandomDataSize > RTSPSessionInterface::kMaxRandomDataSize) {
		fRandomDataSize = RTSPSessionInterface::kMaxRandomDataSize;
	}
}

void  RTSPRequest::ParseBandwidthHeader(boost::string_view header)
{
	auto iter = header.cbegin(), end = header.cend();
	bool r = qi::phrase_parse(iter, end, qi::uint_, qi::ascii::blank, fBandwidthBits);
}



QTSS_Error RTSPRequest::ParseBasicHeader(StringParser *inParsedAuthLinePtr)
{
	QTSS_Error  theErr = QTSS_NoErr;
	fAuthScheme = qtssAuthBasic;

	StrPtrLen authWord;

	inParsedAuthLinePtr->ConsumeWhitespace();
	inParsedAuthLinePtr->ConsumeUntilWhitespace(&authWord);
	if (0 == authWord.Len)
		return theErr;

	char* encodedStr = authWord.GetAsCString();
	std::unique_ptr<char[]> encodedStrDeleter(encodedStr);

	auto *decodedAuthWord = new char[Base64decode_len(encodedStr) + 1];
	std::unique_ptr<char[]> decodedAuthWordDeleter(decodedAuthWord);

	(void)Base64decode(decodedAuthWord, encodedStr);

	boost::string_view nameAndPassword(decodedAuthWord, ::strlen(decodedAuthWord));
	std::string  name, password;

	bool r = qi::phrase_parse(nameAndPassword.cbegin(), nameAndPassword.cend(),
		*(qi::char_ - ":") >> ":" >> *(qi::char_), qi::ascii::blank,
		name, password);

	SetAuthUserName(name);
	SetPassWord(password);

	// Also set the qtssUserName attribute in the qtssRTSPReqUserProfile object attribute of the Request Object
	(void)fUserProfile.SetValue(qtssUserName, 0, name.c_str(), name.length(), QTSSDictionary::kDontObeyReadOnly);

	return theErr;
}

QTSS_Error RTSPRequest::ParseDigestHeader(StringParser *inParsedAuthLinePtr)
{
	QTSS_Error  theErr = QTSS_NoErr;
	fAuthScheme = qtssAuthDigest;

	inParsedAuthLinePtr->ConsumeWhitespace();
	StrPtrLen   *authLine = inParsedAuthLinePtr->GetStream();
	if (nullptr != authLine)
	{
		StringParser digestAuthLine(authLine);
		digestAuthLine.GetThru(nullptr, '=');
		digestAuthLine.ConsumeWhitespace();

		fAuthDigestResponse = std::string(authLine->Ptr, authLine->Len);
	}

	while (inParsedAuthLinePtr->GetDataRemaining() != 0)
	{
		StrPtrLen fieldNameAndValue("");
		inParsedAuthLinePtr->GetThru(&fieldNameAndValue, ',');
		StringParser parsedNameAndValue(&fieldNameAndValue);
		StrPtrLen fieldName("");
		StrPtrLen fieldValue("");

		//Parse name="value" pair fields in the auth line
		parsedNameAndValue.ConsumeUntil(&fieldName, '=');
		parsedNameAndValue.ConsumeLength(nullptr, 1);
		parsedNameAndValue.GetThruEOL(&fieldValue);
		StringParser::UnQuote(&fieldValue);

		// fieldValue.Ptr below is a pointer to a part of the qtssAuthorizationHeader 
		// as GetValue returns a pointer
		// Since the header attribute remains for the entire time the request is alive
		// we don't need to make copies of the values of each field into the request
		// object, and can just keep pointers to the values
		// Thus, no need to delete memory for the following fields when the request is deleted:
		// fAuthRealm, fAuthNonce, fAuthUri, fAuthNonceCount, fAuthResponse, fAuthOpaque
		if (fieldName.Equal(sUsernameStr)) {
			// Set the qtssRTSPReqUserName attribute in the Request object
			SetAuthUserName({ fieldValue.Ptr, fieldValue.Len });
			// Also set the qtssUserName attribute in the qtssRTSPReqUserProfile object attribute of the Request Object
			(void)fUserProfile.SetValue(qtssUserName, 0, fieldValue.Ptr, fieldValue.Len, QTSSDictionary::kDontObeyReadOnly);
		}
		else if (fieldName.Equal(sRealmStr)) {
			fAuthRealm = std::string(fieldValue.Ptr, fieldValue.Len);
		}
		else if (fieldName.Equal(sNonceStr)) {
			fAuthNonce = std::string(fieldValue.Ptr, fieldValue.Len);
		}
		else if (fieldName.Equal(sUriStr)) {
			fAuthUri = std::string(fieldValue.Ptr, fieldValue.Len);
		}
		else if (fieldName.Equal(sQopStr)) {
			if (fieldValue.Equal(sQopAuthStr))
				fAuthQop = RTSPSessionInterface::kAuthQop;
			else if (fieldValue.Equal(sQopAuthIntStr))
				fAuthQop = RTSPSessionInterface::kAuthIntQop;
		}
		else if (fieldName.Equal(sNonceCountStr)) {
			fAuthNonceCount = std::string(fieldValue.Ptr, fieldValue.Len);
		}
		else if (fieldName.Equal(sResponseStr)) {
			fAuthResponse = std::string(fieldValue.Ptr, fieldValue.Len);
		}
		else if (fieldName.Equal(sOpaqueStr)) {
			fAuthOpaque = std::string(fieldValue.Ptr, fieldValue.Len);
		}

		inParsedAuthLinePtr->ConsumeWhitespace();
	}

	return theErr;
}

QTSS_Error RTSPRequest::ParseAuthHeader(void)
{
	QTSS_Error  theErr = QTSS_NoErr;
	boost::string_view authLine = GetHeaderDict().Get(qtssAuthorizationHeader);
	if (authLine.empty())
		return theErr;

	StrPtrLen   authWord("");
	StrPtrLen authLineV((char *)authLine.data(), authLine.length());
	StringParser parsedAuthLine(&authLineV);
	parsedAuthLine.ConsumeUntilWhitespace(&authWord);

	if (authWord.EqualIgnoreCase(sAuthBasicStr.Ptr, sAuthBasicStr.Len))
		return ParseBasicHeader(&parsedAuthLine);

	if (authWord.EqualIgnoreCase(sAuthDigestStr.Ptr, sAuthDigestStr.Len))
		return ParseDigestHeader(&parsedAuthLine);

	return theErr;
}

void RTSPRequest::SetupAuthLocalPath(void)
{
	std::string theFilePath(GetAbsolutePath());

	//
	// Get the truncated path on a setup, because setups have the trackID appended
	if (qtssSetupMethod == fMethod)
		theFilePath = GetTruncatedPath();

	SetLocalPath(std::string(GetRootDir()) + theFilePath);
}

QTSS_Error RTSPRequest::SendDigestChallenge(uint32_t qop, boost::string_view nonce, boost::string_view opaque)
{
	QTSS_Error theErr = QTSS_NoErr;

	char challengeBuf[kAuthChallengeHeaderBufSize];
	ResizeableStringFormatter challengeFormatter(challengeBuf, kAuthChallengeHeaderBufSize);

	boost::string_view realm;
	char *prefRealmPtr = nullptr;
	boost::string_view realmPtr = GetURLRealm();              // Get auth realm set by the module
	if (!realmPtr.empty()) {
		realm = realmPtr;
	}
	else {                                                                  // If module hasn't set the realm
		QTSServerInterface* theServer = QTSServerInterface::GetServer();    // get the realm from prefs
		prefRealmPtr = theServer->GetPrefs()->GetAuthorizationRealm();      // allocates memory
		Assert(prefRealmPtr != nullptr);
		if (prefRealmPtr != nullptr) {
			realm = boost::string_view(prefRealmPtr, strlen(prefRealmPtr));
		}
		else {
			realm = sDefaultRealm;
		}
	}

	// Creating the Challenge header
	challengeFormatter.Put(sAuthDigestStr);             // [Digest]
	challengeFormatter.PutSpace();                      // [Digest ] 
	challengeFormatter.Put(sRealmStr);                  // [Digest realm]
	challengeFormatter.Put(sEqualQuote);                // [Digest realm="]
	challengeFormatter.Put(realm);                      // [Digest realm="somerealm]
	challengeFormatter.Put(sQuoteCommaSpace);           // [Digest realm="somerealm", ]
	if (this->GetStale()) {
		challengeFormatter.Put(sStaleTrue);             // [Digest realm="somerealm", stale="true", ]
	}
	challengeFormatter.Put(sNonceStr);                  // [Digest realm="somerealm", nonce]
	challengeFormatter.Put(sEqualQuote);                // [Digest realm="somerealm", nonce="]
	challengeFormatter.Put(nonce);                     // [Digest realm="somerealm", nonce="19723343a9fd75e019723343a9fd75e0]
	challengeFormatter.PutChar('"');                    // [Digest realm="somerealm", nonce="19723343a9fd75e019723343a9fd75e0"]
	challengeFormatter.PutTerminator();                 // [Digest realm="somerealm", nonce="19723343a9fd75e019723343a9fd75e0"\0]

	std::string challengePtr(challengeFormatter.GetBufPtr(), challengeFormatter.GetBytesWritten() - 1);

	SetDigestChallenge(challengePtr);
	RTSPSessionInterface* thisRTSPSession = this->GetSession();
	if (thisRTSPSession)
	{
		thisRTSPSession->SetDigestChallenge(challengePtr);
	}

	fStatus = qtssClientUnAuthorized;
	this->SetResponseKeepAlive(true);
	this->AppendHeader(qtssWWWAuthenticateHeader, challengePtr);
	this->SendHeader();

	// deleting the memory that was allocated in GetPrefs call above
	if (prefRealmPtr != nullptr)
	{
		delete[] prefRealmPtr;
	}

	return theErr;
}

QTSS_Error RTSPRequest::SendBasicChallenge(void)
{
	QTSS_Error theErr = QTSS_NoErr;
	char *prefRealmPtr = nullptr;

	do
	{
		std::string challenge("Basic realm=\"");
		boost::string_view whichRealm;

		// Get the module's realm
		boost::string_view moduleRealm = GetURLRealm();
		if (!moduleRealm.empty())
		{
			whichRealm = moduleRealm;
		}
		else
		{
			theErr = QTSS_NoErr;
			// Get the default realm from the config file or use the static default if config realm is not found
			QTSServerInterface* theServer = QTSServerInterface::GetServer();
			prefRealmPtr = theServer->GetPrefs()->GetAuthorizationRealm(); // allocates memory
			Assert(prefRealmPtr != nullptr);
			if (prefRealmPtr != nullptr)
			{
				whichRealm = boost::string_view(prefRealmPtr, strlen(prefRealmPtr));
			}
			else
			{
				whichRealm = sDefaultRealm;
			}
		}

		challenge += std::string(whichRealm) + "\"";

#if (0)
		{  // test code
			char test[256];

			memcpy(test, sDefaultRealm.Ptr, sDefaultRealm.Len);
			test[sDefaultRealm.Len] = 0;
			printf("the static realm =%s \n", test);

			std::unique_ptr<char[]> prefDeleter(QTSServerInterface::GetServer()->GetPrefs()->GetAuthorizationRealm());
			memcpy(test, prefDeleter.GetObject(), strlen(prefDeleter.GetObject()));
			test[strlen(prefDeleter.GetObject())] = 0;
			printf("the Pref realm =%s \n", test);

			memcpy(test, moduleRealm.Ptr, moduleRealm.Len);
			test[moduleRealm.Len] = 0;
			printf("the moduleRealm  =%s \n", test);

			memcpy(test, whichRealm.Ptr, whichRealm.Len);
			test[whichRealm.Len] = 0;
			printf("the challenge realm  =%s \n", test);

			memcpy(test, challenge.Ptr, challenge.Len);
			test[challenge.Len] = 0;
			printf("the challenge string  =%s len = %" _S32BITARG_ "\n", test, challenge.Len);
		}
#endif

		fStatus = qtssClientUnAuthorized;
		this->SetResponseKeepAlive(true);
		this->AppendHeader(qtssWWWAuthenticateHeader, challenge);
		this->SendHeader();


	} while (false);

	if (prefRealmPtr != nullptr)
	{
		delete[] prefRealmPtr;
	}

	return theErr;
}

QTSS_Error RTSPRequest::SendForbiddenResponse(void)
{
	fStatus = qtssClientForbidden;
	this->SetResponseKeepAlive(false);
	this->SendHeader();

	return QTSS_NoErr;
}
