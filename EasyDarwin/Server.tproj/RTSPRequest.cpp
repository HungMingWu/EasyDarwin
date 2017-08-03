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

uint8_t
RTSPRequest::sURLStopConditions[] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 1, //0-9      //'\t' is a stop condition
	1, 0, 0, 1, 0, 0, 0, 0, 0, 0, //10-19    //'\r' & '\n' are stop conditions
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //20-29
	0, 0, 1, 0, 0, 0, 0, 0, 0, 0, //30-39    //' '
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //40-49
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //50-59
	0, 0, 0, 1, 0, 0, 0, 0, 0, 0, //60-69   //'?' 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //70-79
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //80-89
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //90-99
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //100-109
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //110-119
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //120-129
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //130-139
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //140-149
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //150-159
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //160-169
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //170-179
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //180-189
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //190-199
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //200-209
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //210-219
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //220-229
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //230-239
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //240-249
	0, 0, 0, 0, 0, 0             //250-255
};

static StrPtrLen    sDefaultRealm("Streaming Server", 16);
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
	StringParser parser(this->GetValue(qtssRTSPReqFullRequest));
	Assert(this->GetValue(qtssRTSPReqFullRequest)->Ptr != nullptr);
	fHeaderDict.clear();
	boost::string_view requestHeader(this->GetValue(qtssRTSPReqFullRequest)->Ptr, this->GetValue(qtssRTSPReqFullRequest)->Len);
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
	QTSS_Error error = ParseFirstLine(parser);

	//handle any errors that come up    
	if (error != QTSS_NoErr)
		return error;

	error = this->ParseHeaders(parser);
	if (error != QTSS_NoErr)
		return error;

	//Response headers should set themselves up to reflect what's in the request headers
	fResponseKeepAlive = fRequestKeepAlive;

	//Make sure that there was some path that was extracted from this request. If not, there is no way
	//we can process the request, so generate an error
	if (this->GetValue(qtssRTSPReqFilePath)->Len == 0)
		return QTSSModuleUtils::SendErrorResponse(this, qtssClientBadRequest, qtssMsgNoURLInRequest, this->GetValue(qtssRTSPReqFullRequest));

	return QTSS_NoErr;
}

//returns: StatusLineTooLong, SyntaxError, BadMethod
QTSS_Error RTSPRequest::ParseFirstLine(StringParser &parser)
{
	//first get the method
	StrPtrLen theParsedData;
	parser.ConsumeWord(&theParsedData);

	//THIS WORKS UNDER THE ASSUMPTION THAT:
	//valid HTTP/1.1 headers are: GET, HEAD, POST, PUT, OPTIONS, DELETE, TRACE
	fMethod = RTSPProtocol::GetMethod(theParsedData);
	if (fMethod == qtssIllegalMethod)
		return QTSSModuleUtils::SendErrorResponse(this, qtssClientBadRequest, qtssMsgBadRTSPMethod, &theParsedData);

	//no longer assume this is a space... instead, just consume whitespace
	parser.ConsumeWhitespace();

	//now parse the uri,for example rtsp://www.easydarwin.org:554/live.sdp?channel=1&token=888888
	QTSS_Error err = ParseURI(parser);
	if (err != QTSS_NoErr)
		return err;

	//no longer assume this is a space... instead, just consume whitespace
	parser.ConsumeWhitespace();

	//if there is a version, consume the version string
	StrPtrLen versionStr;
	parser.ConsumeUntil(&versionStr, StringParser::sEOLMask);

	//check the version
	if (versionStr.Len > 0)
		fVersion = RTSPProtocol::GetVersion(versionStr);

	//go past the end of line
	if (!parser.ExpectEOL())
		return QTSSModuleUtils::SendErrorResponse(this, qtssClientBadRequest, qtssMsgNoRTSPVersion, &theParsedData);
	return QTSS_NoErr;
}

//returns: SyntaxError if there was an error in the uri. Or InternalServerError
QTSS_Error RTSPRequest::ParseURI(StringParser &parser)
{
	//for example: rtsp://www.easydarwin.org:554/live.sdp?channel=1&token=888888
	//read in the complete URL, set it to be the qtssAbsoluteURLParam
	StrPtrLen theURL;
	parser.ConsumeUntilWhitespace(&theURL);
	//qtssRTSPReqAbsoluteURL = rtsp://www.easydarwin.org:554/live.sdp?channel=1&token=888888
	this->SetVal(qtssRTSPReqAbsoluteURL, &theURL);
	StringParser absParser(&theURL);
	StrPtrLen theAbsURL;
	//theAbsURL = rtsp://www.easydarwin.org:554/live.sdp
	absParser.ConsumeUntil(&theAbsURL, sURLStopConditions);

	//we always should have a slash before the uri.
	//If not, that indicates this is a full URI. Also, this could be a '*' OPTIONS request
	if ((*theAbsURL.Ptr != '/') && (*theAbsURL.Ptr != '*'))
	{
		std::string theAbsURLV(theAbsURL.Ptr, theAbsURL.Len), theHost, uriPath;
		bool r = qi::phrase_parse(theAbsURLV.cbegin(), theAbsURLV.cend(),
			qi::no_case["RTSP://"] >> *(qi::char_ - "/") >> (qi::eoi | *(qi::char_)),  
			qi::ascii::blank, theHost, uriPath);

		fHeaderDict.Set(qtssHostHeader, theHost);
		if (!uriPath.empty())
			// qtssRTSPReqURI = /live.sdp
			uri = uriPath;
		else {
			//
			// This might happen if there is nothing after the host at all, not even
			// a '/'. This is legal (RFC 2326, Sec 3.2). If so, just pretend that there
			// is a '/'
			static char* sSlashURI = "/";
			uri = sSlashURI;
		}
	}

	// don't allow non-aggregate operations indicated by a url/media track=id
// might need this for rate adapt   if (qtssSetupMethod != fMethod && qtssOptionsMethod != fMethod && qtssSetParameterMethod != fMethod) // any method not a setup, options, or setparameter is not allowed to have a "/trackID=" in the url.
	if (qtssSetupMethod != fMethod) // any method not a setup is not allowed to have a "/trackID=" in the url.
	{
		StrPtrLenDel tempCStr(theAbsURL.GetAsCString());
		StrPtrLen nonaggregate(tempCStr.FindString("/trackID="));
		if (nonaggregate.Len > 0) // check for non-aggregate method and return error
			return QTSSModuleUtils::SendErrorResponse(this, qtssClientAggregateOptionAllowed, qtssMsgBadRTSPMethod, &theAbsURL);
	}

	// don't allow non-aggregate operations like a setup on a playing session
	if (qtssSetupMethod == fMethod) // if it is a setup but we are playing don't allow it
	{
		auto*  theSession = (RTSPSession*)this->GetSession();
		if (theSession != nullptr && theSession->IsPlaying())
			return QTSSModuleUtils::SendErrorResponse(this, qtssClientAggregateOptionAllowed, qtssMsgBadRTSPMethod, &theAbsURL);
	}

	// parse the query string from the url if present.
	// init qtssRTSPReqQueryString dictionary to an empty string
	// qtssRTSPReqQueryString = channel=1&token=888888

	if (absParser.GetDataRemaining() > 0)
	{
		if (absParser.PeekFast() == '?')
		{
			// we've got some CGI param
			StrPtrLen queryString1;
			absParser.ConsumeLength(&queryString1, 1); // toss '?'

			// consume the rest of the line..
			absParser.ConsumeUntilWhitespace(&queryString1);

			queryString = boost::string_view(queryString1.Ptr, queryString1.Len);
		}
	}


	//
	// If the is a '*', return right now because '*' is not a path
	// so the below functions don't make any sense.
	if ((*theAbsURL.Ptr == '*') && (theAbsURL.Len == 1))
	{
		this->SetValue(qtssRTSPReqFilePath, 0, theAbsURL.Ptr, theAbsURL.Len, QTSSDictionary::kDontObeyReadOnly);

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
	this->SetValue(qtssRTSPReqFilePath, 0, fFilePath, theBytesWritten, QTSSDictionary::kDontObeyReadOnly);



	return QTSS_NoErr;
}


//throws eHTTPNoMoreData and eHTTPOutOfBuffer
QTSS_Error RTSPRequest::ParseHeaders(StringParser& parser)
{
	StrPtrLen theKeyWord;
	bool isStreamOK;

	//Repeat until we get a \r\n\r\n, which signals the end of the headers

	while ((parser.PeekFast() != '\r') && (parser.PeekFast() != '\n'))
	{
		//First get the header identifier

		isStreamOK = parser.GetThru(&theKeyWord, ':');
		if (!isStreamOK)
			return QTSSModuleUtils::SendErrorResponse(this, qtssClientBadRequest, qtssMsgNoColonAfterHeader, this->GetValue(qtssRTSPReqFullRequest));

		theKeyWord.TrimWhitespace();

		//Look up the proper header enumeration based on the header string.
		//Use the enumeration to look up the dictionary ID of this header,
		//and set that dictionary attribute to be whatever is in the body of the header

		uint32_t theHeader = RTSPProtocol::GetRequestHeader(theKeyWord);
		StrPtrLen theHeaderVal;
		parser.ConsumeUntil(&theHeaderVal, StringParser::sEOLMask);

		StrPtrLen theEOL;
		if ((parser.PeekFast() == '\r') || (parser.PeekFast() == '\n'))
		{
			isStreamOK = true;
			parser.ConsumeEOL(&theEOL);
		}
		else
			isStreamOK = false;

		while ((parser.PeekFast() == ' ') || (parser.PeekFast() == '\t'))
		{
			theHeaderVal.Len += theEOL.Len;
			StrPtrLen temp;
			parser.ConsumeUntil(&temp, StringParser::sEOLMask);
			theHeaderVal.Len += temp.Len;

			if ((parser.PeekFast() == '\r') || (parser.PeekFast() == '\n'))
			{
				isStreamOK = true;
				parser.ConsumeEOL(&theEOL);
			}
			else
				isStreamOK = false;
		}

		// If this is an unknown header, ignore it. Otherwise, set the proper
		// dictionary attribute
		if (theHeader != qtssIllegalHeader)
		{
			Assert(theHeader < qtssNumHeaders);
			theHeaderVal.TrimWhitespace();
			fHeaderDict.Set(theHeader, std::string(theHeaderVal.Ptr, theHeaderVal.Len));
		}
		if (!isStreamOK)
			return QTSSModuleUtils::SendErrorResponse(this, qtssClientBadRequest, qtssMsgNoEOLAfterHeader);

		//some headers require some special processing. If this code begins
		//to get out of control, we made need to come up with a function pointer table
		boost::string_view theHeaderValV(theHeaderVal.Ptr, theHeaderVal.Len);
		switch (theHeader)
		{
		case qtssSessionHeader:             ParseSessionHeader(theHeaderValV); break;
		case qtssTransportHeader:           ParseTransportHeader(theHeaderVal); break;
		case qtssRangeHeader:               ParseRangeHeader(theHeaderVal);     break;
		case qtssIfModifiedSinceHeader:     ParseIfModSinceHeader(theHeaderVal); break;
		case qtssXRetransmitHeader:         ParseRetransmitHeader(theHeaderVal); break;
		case qtssContentLengthHeader:       ParseContentLengthHeader(theHeaderValV); break;
		case qtssSpeedHeader:               ParseSpeedHeader(theHeaderValV);     break;
		case qtssXTransportOptionsHeader:   ParseTransportOptionsHeader(theHeaderVal); break;
		case qtssXPreBufferHeader:          ParsePrebufferHeader(theHeaderValV); break;
		case qtssXDynamicRateHeader:		ParseDynamicRateHeader(theHeaderValV); break;
		case qtssXRandomDataSizeHeader:		ParseRandomDataSizeHeader(theHeaderValV); break;
		case qtssBandwidthHeader:           ParseBandwidthHeader(theHeaderValV); break;
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

	isStreamOK = parser.ExpectEOL();
	Assert(isStreamOK);
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

bool RTSPRequest::ParseNetworkModeSubHeader(StrPtrLen* inSubHeader)
{
	static StrPtrLen sUnicast("unicast");
	static StrPtrLen sMulticast("multiicast");
	bool result = false; // true means header was found

	if (!result && inSubHeader->EqualIgnoreCase(sUnicast))
	{
		fNetworkMode = qtssRTPNetworkModeUnicast;
		result = true;
	}

	if (!result && inSubHeader->EqualIgnoreCase(sMulticast))
	{
		fNetworkMode = qtssRTPNetworkModeMulticast;
		result = true;
	}

	return result;
}

template <typename S>
std::vector<std::string> spirit_direct(const S& input, char const* delimiter)
{
	std::vector<std::string> result;
	if (!qi::parse(input.begin(), input.end(), 
		qi::raw[*(qi::char_ - qi::char_(delimiter))] % qi::char_(delimiter), result))
		result.push_back(std::string(input));
	return result;
}

void RTSPRequest::ParseTransportHeader(StrPtrLen &header)
{
	static char* sRTPAVPTransportStr = "RTP/AVP";

	StringParser theTransParser(&header);

	//transport header from client: Transport: RTP/AVP;unicast;client_port=5000-5001\r\n
	//                              Transport: RTP/AVP;multicast;ttl=15;destination=229.41.244.93;client_port=5000-5002\r\n
	//                              Transport: RTP/AVP/TCP;unicast\r\n

	//
	// A client may send multiple transports to the server, comma separated.
	// In this case, the server should just pick one and use that. 

	while (theTransParser.GetDataRemaining() > 0)
	{
		(void)theTransParser.ConsumeWhitespace();
		(void)theTransParser.ConsumeUntil(&fFirstTransport, ',');

		if (fFirstTransport.NumEqualIgnoreCase(sRTPAVPTransportStr, ::strlen(sRTPAVPTransportStr)))
			break;

		if (theTransParser.PeekFast() == ',')
			theTransParser.Expect(',');
	}

	StringParser theFirstTransportParser(&fFirstTransport);

	StrPtrLen theTransportSubHeader;
	(void)theFirstTransportParser.GetThru(&theTransportSubHeader, ';');

	while (theTransportSubHeader.Len > 0)
	{

		// Extract the relevent information from the relevent subheader.
		// So far we care about 3 sub-headers

		if (!this->ParseNetworkModeSubHeader(&theTransportSubHeader))
		{
			theTransportSubHeader.TrimWhitespace();

			switch (*theTransportSubHeader.Ptr)
			{
			case 'r':	// rtp/avp/??? Is this tcp or udp?
			case 'R':   // RTP/AVP/??? Is this TCP or UDP?
				{
					if (theTransportSubHeader.EqualIgnoreCase("RTP/AVP/TCP"))
						fTransportType = qtssRTPTransportTypeTCP;
					break;
				}
			case 'c':   //client_port sub-header
			case 'C':   //client_port sub-header
				{
					this->ParseClientPortSubHeader(&theTransportSubHeader);
					break;
				}
			case 'd':   //destination sub-header
			case 'D':   //destination sub-header
				{
					static StrPtrLen sDestinationSubHeader("destination");

					//Parse the header, extract the destination address
					this->ParseAddrSubHeader(&theTransportSubHeader, &sDestinationSubHeader, &fDestinationAddr);
					break;
				}
			case 's':   //source sub-header
			case 'S':   //source sub-header
				{
					//Same as above code
					static StrPtrLen sSourceSubHeader("source");
					this->ParseAddrSubHeader(&theTransportSubHeader, &sSourceSubHeader, &fSourceAddr);
					break;
				}
			case 't':   //time-to-live sub-header
			case 'T':   //time-to-live sub-header
				{
					this->ParseTimeToLiveSubHeader(&theTransportSubHeader);
					break;
				}
			case 'm':   //mode sub-header
			case 'M':   //mode sub-header
				{
					this->ParseModeSubHeader(&theTransportSubHeader);
					break;
				}
			}
		}

		// Move onto the next parameter
		(void)theFirstTransportParser.GetThru(&theTransportSubHeader, ';');
	}
}

void  RTSPRequest::ParseRangeHeader(StrPtrLen &header)
{
	StringParser theRangeParser(&header);

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

void  RTSPRequest::ParseRetransmitHeader(StrPtrLen &header)
{
	boost::string_view t1(fHeaderDict.Get(qtssXRetransmitHeader));
	StrPtrLen t2((char *)t1.data(), t1.length());
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

void  RTSPRequest::ParseIfModSinceHeader(StrPtrLen &header)
{
	fIfModSinceDate = DateTranslator::ParseDate(&header);

	// Only set the param if this is a legal date
	if (fIfModSinceDate != 0)
		this->SetVal(qtssRTSPReqIfModSinceDate, &fIfModSinceDate, sizeof(fIfModSinceDate));
}

void RTSPRequest::ParseSpeedHeader(boost::string_view header)
{
	auto iter = header.cbegin(), end = header.cend();
	bool r = qi::phrase_parse(iter, end, qi::float_, qi::ascii::blank, fSpeed);
}

void RTSPRequest::ParseTransportOptionsHeader(StrPtrLen &header)
{
	StringParser theRTPOptionsParser(&header);
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
			fLateToleranceStr = theRTPOptionsSubHeader;
		}

		(void)theRTPOptionsParser.GetThru(&theRTPOptionsSubHeader, ';');

	} while (theRTPOptionsSubHeader.Len > 0);
}


void RTSPRequest::ParseAddrSubHeader(StrPtrLen* inSubHeader, StrPtrLen* inHeaderName, uint32_t* outAddr)
{
	if (!inSubHeader || !inHeaderName || !outAddr)
		return;

	StringParser theSubHeaderParser(inSubHeader);

	// Skip over to the value
	StrPtrLen theFirstBit;
	theSubHeaderParser.GetThru(&theFirstBit, '=');
	theFirstBit.TrimWhitespace();

	// First make sure this is the proper subheader
	if (!theFirstBit.EqualIgnoreCase(*inHeaderName))
		return;

	//Find the IP address
	theSubHeaderParser.ConsumeUntilDigit();

	//Set the addr string param.
	StrPtrLen theAddr(theSubHeaderParser.GetCurrentPosition(), theSubHeaderParser.GetDataRemaining());

	//Convert the string to a uint32_t IP address
	char theTerminator = theAddr.Ptr[theAddr.Len];
	theAddr.Ptr[theAddr.Len] = '\0';

	*outAddr = SocketUtils::ConvertStringToAddr(theAddr.Ptr);

	theAddr.Ptr[theAddr.Len] = theTerminator;

}

void RTSPRequest::ParseModeSubHeader(StrPtrLen* inModeSubHeader)
{
	static StrPtrLen sModeSubHeader("mode");
	static StrPtrLen sReceiveMode("receive");
	static StrPtrLen sRecordMode("record");
	StringParser theSubHeaderParser(inModeSubHeader);

	// Skip over to the first port
	StrPtrLen theFirstBit;
	theSubHeaderParser.GetThru(&theFirstBit, '=');
	theFirstBit.TrimWhitespace();

	// Make sure this is the client port subheader
	if (theFirstBit.EqualIgnoreCase(sModeSubHeader)) do
	{
		theSubHeaderParser.ConsumeWhitespace();

		StrPtrLen theMode;
		theSubHeaderParser.ConsumeWord(&theMode);

		if (theMode.EqualIgnoreCase(sReceiveMode) || theMode.EqualIgnoreCase(sRecordMode))
		{
			fTransportMode = qtssRTPTransportModeRecord;
			break;
		}

	} while (false);

}

void RTSPRequest::ParseClientPortSubHeader(StrPtrLen* inClientPortSubHeader)
{
	static StrPtrLen sClientPortSubHeader("client_port");
	static StrPtrLen sErrorMessage("Received invalid client_port field: ");
	StringParser theSubHeaderParser(inClientPortSubHeader);

	// Skip over to the first port
	StrPtrLen theFirstBit;
	theSubHeaderParser.GetThru(&theFirstBit, '=');
	theFirstBit.TrimWhitespace();

	// Make sure this is the client port subheader
	if (!theFirstBit.EqualIgnoreCase(sClientPortSubHeader))
		return;

	// Store the two client ports as integers
	theSubHeaderParser.ConsumeWhitespace();
	fClientPortA = (uint16_t)theSubHeaderParser.ConsumeInteger(nullptr);
	theSubHeaderParser.GetThru(nullptr, '-');
	theSubHeaderParser.ConsumeWhitespace();
	fClientPortB = (uint16_t)theSubHeaderParser.ConsumeInteger(nullptr);
	if (fClientPortB != fClientPortA + 1) // an error in the port values
	{
		// The following to setup and log the error as a message level 2.
		boost::string_view userAgent = fHeaderDict.Get(qtssUserAgentHeader);
		ResizeableStringFormatter errorPortMessage;
		errorPortMessage.Put(sErrorMessage);
		if (!userAgent.empty())
			errorPortMessage.Put((char *)userAgent.data(), userAgent.length());
		errorPortMessage.PutSpace();
		errorPortMessage.Put(*inClientPortSubHeader);
		errorPortMessage.PutTerminator();
		QTSSModuleUtils::LogError(qtssMessageVerbosity, qtssMsgNoMessage, 0, errorPortMessage.GetBufPtr(), nullptr);


		//fix the rtcp port and hope it works.
		fClientPortB = fClientPortA + 1;
	}
}

void RTSPRequest::ParseTimeToLiveSubHeader(StrPtrLen* inTimeToLiveSubHeader)
{
	static StrPtrLen sTimeToLiveSubHeader("ttl");

	StringParser theSubHeaderParser(inTimeToLiveSubHeader);

	// Skip over to the first part
	StrPtrLen theFirstBit;
	theSubHeaderParser.GetThru(&theFirstBit, '=');
	theFirstBit.TrimWhitespace();
	// Make sure this is the ttl subheader
	if (!theFirstBit.EqualIgnoreCase(sTimeToLiveSubHeader))
		return;

	// Parse out the time to live...
	theSubHeaderParser.ConsumeWhitespace();
	fTtl = (uint16_t)theSubHeaderParser.ConsumeInteger(nullptr);
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

	StrPtrLen   nameAndPassword;
	nameAndPassword.Set(decodedAuthWord, ::strlen(decodedAuthWord));

	StrPtrLen   name("");
	StrPtrLen   password("");
	StringParser parsedNameAndPassword(&nameAndPassword);

	parsedNameAndPassword.ConsumeUntil(&name, ':');
	parsedNameAndPassword.ConsumeLength(nullptr, 1);
	parsedNameAndPassword.GetThruEOL(&password);


	// Set the qtssRTSPReqUserName and qtssRTSPReqUserPassword attributes in the Request object
	(void) this->SetValue(qtssRTSPReqUserName, 0, name.Ptr, name.Len, QTSSDictionary::kDontObeyReadOnly);
	(void) this->SetValue(qtssRTSPReqUserPassword, 0, password.Ptr, password.Len, QTSSDictionary::kDontObeyReadOnly);

	// Also set the qtssUserName attribute in the qtssRTSPReqUserProfile object attribute of the Request Object
	(void)fUserProfile.SetValue(qtssUserName, 0, name.Ptr, name.Len, QTSSDictionary::kDontObeyReadOnly);

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

		fAuthDigestResponse.Set(authLine->Ptr, authLine->Len);
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
			(void) this->SetValue(qtssRTSPReqUserName, 0, fieldValue.Ptr, fieldValue.Len, QTSSDictionary::kDontObeyReadOnly);
			// Also set the qtssUserName attribute in the qtssRTSPReqUserProfile object attribute of the Request Object
			(void)fUserProfile.SetValue(qtssUserName, 0, fieldValue.Ptr, fieldValue.Len, QTSSDictionary::kDontObeyReadOnly);
		}
		else if (fieldName.Equal(sRealmStr)) {
			fAuthRealm.Set(fieldValue.Ptr, fieldValue.Len);
		}
		else if (fieldName.Equal(sNonceStr)) {
			fAuthNonce.Set(fieldValue.Ptr, fieldValue.Len);
		}
		else if (fieldName.Equal(sUriStr)) {
			fAuthUri.Set(fieldValue.Ptr, fieldValue.Len);
		}
		else if (fieldName.Equal(sQopStr)) {
			if (fieldValue.Equal(sQopAuthStr))
				fAuthQop = RTSPSessionInterface::kAuthQop;
			else if (fieldValue.Equal(sQopAuthIntStr))
				fAuthQop = RTSPSessionInterface::kAuthIntQop;
		}
		else if (fieldName.Equal(sNonceCountStr)) {
			fAuthNonceCount.Set(fieldValue.Ptr, fieldValue.Len);
		}
		else if (fieldName.Equal(sResponseStr)) {
			fAuthResponse.Set(fieldValue.Ptr, fieldValue.Len);
		}
		else if (fieldName.Equal(sOpaqueStr)) {
			fAuthOpaque.Set(fieldValue.Ptr, fieldValue.Len);
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
	QTSS_AttributeID theID = qtssRTSPReqFilePath;

	//
	// Get the truncated path on a setup, because setups have the trackID appended
	if (qtssSetupMethod == fMethod)
		theID = qtssRTSPReqFilePathTrunc;

	uint32_t theLen = 0;
	char* theFullPath = QTSSModuleUtils::GetFullPath(this, theID, &theLen, nullptr);
	SetLocalPath({ theFullPath, theLen });
	delete[] theFullPath;
}

QTSS_Error RTSPRequest::SendDigestChallenge(uint32_t qop, StrPtrLen *nonce, StrPtrLen* opaque)
{
	QTSS_Error theErr = QTSS_NoErr;

	char challengeBuf[kAuthChallengeHeaderBufSize];
	ResizeableStringFormatter challengeFormatter(challengeBuf, kAuthChallengeHeaderBufSize);

	StrPtrLen realm;
	char *prefRealmPtr = nullptr;
	StrPtrLen *realmPtr = this->GetValue(qtssRTSPReqURLRealm);              // Get auth realm set by the module
	if (realmPtr->Len > 0) {
		realm = *realmPtr;
	}
	else {                                                                  // If module hasn't set the realm
		QTSServerInterface* theServer = QTSServerInterface::GetServer();    // get the realm from prefs
		prefRealmPtr = theServer->GetPrefs()->GetAuthorizationRealm();      // allocates memory
		Assert(prefRealmPtr != nullptr);
		if (prefRealmPtr != nullptr) {
			realm.Set(prefRealmPtr, strlen(prefRealmPtr));
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
	challengeFormatter.Put(*nonce);                     // [Digest realm="somerealm", nonce="19723343a9fd75e019723343a9fd75e0]
	challengeFormatter.PutChar('"');                    // [Digest realm="somerealm", nonce="19723343a9fd75e019723343a9fd75e0"]
	challengeFormatter.PutTerminator();                 // [Digest realm="somerealm", nonce="19723343a9fd75e019723343a9fd75e0"\0]

	std::string challengePtr(challengeFormatter.GetBufPtr(), challengeFormatter.GetBytesWritten() - 1);

	this->SetValue(qtssRTSPReqDigestChallenge, 0, challengePtr.c_str(), challengePtr.length(), QTSSDictionary::kDontObeyReadOnly);
	RTSPSessionInterface* thisRTSPSession = this->GetSession();
	if (thisRTSPSession)
	{
		(void)thisRTSPSession->SetValue(qtssRTSPSesLastDigestChallenge, 0, challengePtr.c_str(), challengePtr.length(), QTSSDictionary::kDontObeyReadOnly);
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
		StrPtrLen whichRealm;

		// Get the module's realm
		StrPtrLen moduleRealm;
		theErr = this->GetValuePtr(qtssRTSPReqURLRealm, 0, (void **)&moduleRealm.Ptr, &moduleRealm.Len);
		if ((QTSS_NoErr == theErr) && (moduleRealm.Len > 0))
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
				whichRealm.Set(prefRealmPtr, strlen(prefRealmPtr));
			}
			else
			{
				whichRealm = sDefaultRealm;
			}
		}

		challenge += std::string(whichRealm.Ptr, whichRealm.Len) + "\"";

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
