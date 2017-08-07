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
	 File:       RTSPRequestInterface.cp

	 Contains:   Implementation of class defined in RTSPRequestInterface.h
 */


 //INCLUDES:
#ifndef __Win32__
#include <sys/types.h>
#include <sys/uio.h>
#endif

#include <memory>

#include "RTSPRequestInterface.h"
#include "RTSPSessionInterface.h"
#include "RTSPRequestStream.h"

#include "StringParser.h"
#include "OSThread.h"
#include "DateTranslator.h"
#include "QTSSDataConverter.h"
#include "QTSServerInterface.h"

std::string RTSPRequestInterface::sPremadeHeader;
std::string RTSPRequestInterface::sPremadeNoHeader;


StrPtrLen   RTSPRequestInterface::sColonSpace(": ", 2);

QTSSAttrInfoDict::AttrInfo  RTSPRequestInterface::sAttributes[] =
{   /*fields:   fAttrName, fFuncPtr, fAttrDataType, fAttrPermission */
	/* 0 */ {},
	/* 1 */ {},
	/* 2 */ {},
	/* 3 */ {},
	/* 4 */ {},
	/* 5 */ {},
	/* 6 */ {},
	/* 7 */ {},
	/* 8 */ {},
	/* 9 */ {},
	/* 10 */ {},
	/* 11 */ {},
	/* 12 */ {},
	/* 13 */ {},
	/* 14 */ {},
	/* 15 */ {},
	/* 16 */ {},

	/* 17 */ {},
	/* 18 */ {},
	/* 19 */ {},
	/* 20 */ {},
	/* 21 */ {},
	/* 22 */ {},
	/* 23 */ {},
	/* 24 */ {},
	/* 25 */ {},
	/* 26 */ {},
	/* 27 */ {},
	/* 28 */ {},
	/* 29 */ {},
	/* 30 */ {},
	/* 31 */ {},
	/* 32 */ {},
	/* 33 */ {},
	/* 34 */ {},
	/* 35 */ {},
	/* 36 */ {},
	/* 37 */ {},

	/* 39 */ {},
	/* 39 */ {},
	/* 40 */ {},
	/* 41 */ {},
	/* 42 */ {}
};

std::string PutStatusLine(QTSS_RTSPStatusCode status, RTSPProtocol::RTSPVersion version)
{
	std::string result;
	StrPtrLen temp;
	temp = RTSPProtocol::GetVersionString(version);
	result += std::string(temp.Ptr, temp.Len) + " ";
	temp = RTSPProtocol::GetStatusCodeAsString(status);
	result += std::string(temp.Ptr, temp.Len) + " ";
	temp = RTSPProtocol::GetStatusCodeString(status);
	result += std::string(temp.Ptr, temp.Len) + "\r\n";
	return result;
}
void  RTSPRequestInterface::Initialize(void)
{
	//make a partially complete header
	sPremadeHeader = ::PutStatusLine(qtssSuccessOK, RTSPProtocol::k10Version);

	StrPtrLen temp = QTSServerInterface::GetServerHeader();
	sPremadeHeader += std::string(temp.Ptr, temp.Len) + "\r\n";
	temp = RTSPProtocol::GetHeaderString(qtssCSeqHeader);
	sPremadeHeader += std::string(temp.Ptr, temp.Len) + ": ";

	sPremadeNoHeader = ::PutStatusLine(qtssSuccessOK, RTSPProtocol::k10Version);
	temp = RTSPProtocol::GetHeaderString(qtssCSeqHeader);
	sPremadeNoHeader += std::string(temp.Ptr, temp.Len) + ": ";

	//Setup all the dictionary stuff
	for (uint32_t x = 0; x < qtssRTSPReqNumParams; x++)
		QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kRTSPRequestDictIndex)->
		SetAttribute(x, sAttributes[x].fAttrName, sAttributes[x].fFuncPtr,
			sAttributes[x].fAttrDataType, sAttributes[x].fAttrPermission);

	QTSSDictionaryMap* theHeaderMap = QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kRTSPHeaderDictIndex);
	for (uint32_t y = 0; y < qtssNumHeaders; y++)
		theHeaderMap->SetAttribute(y, RTSPProtocol::GetHeaderString(y).Ptr, nullptr, qtssAttrDataTypeCharArray, qtssAttrModeRead | qtssAttrModePreempSafe);
}

void RTSPRequestInterface::ReInit(RTSPSessionInterface *session)
{
	//   fSession=session;
	//   fOutputStream=session->GetOutputStream();
	fStandardHeadersWritten = false; // private initializes after protected and public storage above

	RTSPRequestStream* input = session->GetInputStream();
	SetFullRequest({ input->GetRequestBuffer()->Ptr, input->GetRequestBuffer()->Len });

	// klaus(20170223):fix ffplay cant pull stream from easydarwin
	fHeaderDict.Set(qtssSessionHeader, "");
}

//CONSTRUCTOR / DESTRUCTOR: very simple stuff
RTSPRequestInterface::RTSPRequestInterface(RTSPSessionInterface *session)
	: QTSSDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kRTSPRequestDictIndex)),
	fMethod(qtssIllegalMethod),
	fStatus(qtssSuccessOK),
	fRequestKeepAlive(true),
	//fResponseKeepAlive(true), //parameter need not be set
	fVersion(RTSPProtocol::k10Version),
	fStartTime(-1),
	fStopTime(-1),
	fClientPortA(0),
	fClientPortB(0),
	fTtl(0),
	fDestinationAddr(0),
	fSourceAddr(0),
	fTransportType(qtssRTPTransportTypeTCP),
	fNetworkMode(qtssRTPNetworkModeDefault),
	fContentLength(0),
	fIfModSinceDate(0),
	fSpeed(0),
	fLateTolerance(-1),
	fPrebufferAmt(-1),
	fWindowSize(0),
	fMovieFolderPtr(&fMovieFolderPath[0]),
	fAllowed(true),
	fHasUser(false),
	fAuthHandled(false),
	fTransportMode(qtssRTPTransportModePlay),
	fSetUpServerPort(0),
	fAction(qtssActionFlagsNoFlags),
	fAuthScheme(qtssAuthNone),
	fAuthQop(RTSPSessionInterface::kNoQop),
	fUserProfile(),
	fUserProfilePtr(&fUserProfile),
	fStale(false),
	fSkipAuthorization(true),
	fEnableDynamicRateState(-1),// -1 undefined, 0 disabled, 1 enabled
	// DJM PROTOTYPE
	fRandomDataSize(0),
	fBandwidthBits(0),

	// private storage initializes after protected and public storage above
	fSession(session),
	fOutputStream(session->GetOutputStream()),
	fStandardHeadersWritten(false) // private initializes after protected and public storage above

{
}

void RTSPRequestInterface::AppendHeader(QTSS_RTSPHeader inHeader, boost::string_view inValue)
{
	if (!fStandardHeadersWritten)
		this->WriteStandardHeaders();

	fOutputStream->Put(RTSPProtocol::GetHeaderString(inHeader));
	fOutputStream->Put(sColonSpace);
	fOutputStream->Put(inValue);
	fOutputStream->PutEOL();
}

void RTSPRequestInterface::PutStatusLine(StringFormatter* putStream, QTSS_RTSPStatusCode status,
	RTSPProtocol::RTSPVersion version)
{
	putStream->Put(RTSPProtocol::GetVersionString(version));
	putStream->PutSpace();
	putStream->Put(RTSPProtocol::GetStatusCodeAsString(status));
	putStream->PutSpace();
	putStream->Put(RTSPProtocol::GetStatusCodeString(status));
	putStream->PutEOL();
}


void RTSPRequestInterface::AppendContentLength(uint32_t contentLength)
{
	if (!fStandardHeadersWritten)
		this->WriteStandardHeaders();

	this->AppendHeader(qtssContentLengthHeader, std::to_string(contentLength));
}

void RTSPRequestInterface::AppendDateAndExpires()
{
	if (!fStandardHeadersWritten)
		this->WriteStandardHeaders();

	Assert(OSThread::GetCurrent() != nullptr);
	DateBuffer* theDateBuffer = OSThread::GetCurrent()->GetDateBuffer();
	theDateBuffer->InexactUpdate(); // Update the date buffer to the current date & time
	std::string theDate(theDateBuffer->GetDateBuffer(), DateBuffer::kDateBufferLen);

	// Append dates, and have this response expire immediately
	this->AppendHeader(qtssDateHeader, theDate);
	this->AppendHeader(qtssExpiresHeader, theDate);
}


void RTSPRequestInterface::AppendSessionHeaderWithTimeout(boost::string_view inSessionID, boost::string_view inTimeout)
{
	// Append a session header if there wasn't one already
	if (GetHeaderDict().Get(qtssSessionHeader).empty())
	{
		if (!fStandardHeadersWritten)
			this->WriteStandardHeaders();

		static boost::string_view sTimeoutString(";timeout=");

		// Just write out the session header and session ID
		if (!inSessionID.empty())
		{
			fOutputStream->Put(RTSPProtocol::GetHeaderString(qtssSessionHeader));
			fOutputStream->Put(sColonSpace);
			fOutputStream->Put(inSessionID);

			if (!inTimeout.empty())
			{
				fOutputStream->Put(sTimeoutString);
				fOutputStream->Put(inTimeout);
			}

			fOutputStream->PutEOL();
		}
	}

}

void RTSPRequestInterface::PutTransportStripped(StrPtrLen &fullTransportHeader, StrPtrLen &fieldToStrip)
{

	// skip the fieldToStrip and echo the rest back
	auto offset = (uint32_t)(fieldToStrip.Ptr - fullTransportHeader.Ptr);
	StrPtrLen transportStart(fullTransportHeader.Ptr, offset);
	while (transportStart.Len > 0) // back up removing chars up to and including ;
	{
		transportStart.Len--;
		if (transportStart[transportStart.Len] == ';')
			break;
	}

	StrPtrLen transportRemainder(fieldToStrip.Ptr, fullTransportHeader.Len - offset);
	StringParser transportParser(&transportRemainder);
	transportParser.ConsumeUntil(&fieldToStrip, ';'); //remainder starts with ;       
	transportRemainder.Set(transportParser.GetCurrentPosition(), transportParser.GetDataRemaining());

	fOutputStream->Put(transportStart);
	fOutputStream->Put(transportRemainder);

}

void RTSPRequestInterface::AppendTransportHeader(boost::string_view serverPortA,
	boost::string_view serverPortB,
	boost::string_view channelA,
	boost::string_view channelB,
	boost::string_view serverIPAddr,
	boost::string_view ssrc)
{
	static StrPtrLen    sServerPortString(";server_port=");
	static StrPtrLen    sSourceString(";source=");
	static StrPtrLen    sInterleavedString(";interleaved=");
	static StrPtrLen    sSSRC(";ssrc=");
	static StrPtrLen    sInterLeaved("interleaved");//match the interleaved tag
	static StrPtrLen    sClientPort("client_port");
	static StrPtrLen    sClientPortString(";client_port=");

	if (!fStandardHeadersWritten)
		this->WriteStandardHeaders();

	// Just write out the same transport header the client sent to us.
	fOutputStream->Put(RTSPProtocol::GetHeaderString(qtssTransportHeader));
	fOutputStream->Put(sColonSpace);

	StrPtrLen outFirstTransport(fFirstTransport.GetAsCString());
	std::unique_ptr<char[]> outFirstTransportDeleter(outFirstTransport.Ptr);
	outFirstTransport.RemoveWhitespace();
	while (outFirstTransport[outFirstTransport.Len - 1] == ';')
		outFirstTransport.Len--;

	// see if it contains an interleaved field or client port field
	StrPtrLen stripClientPortStr;
	StrPtrLen stripInterleavedStr;
	(void)outFirstTransport.FindStringIgnoreCase(sClientPort, &stripClientPortStr);
	(void)outFirstTransport.FindStringIgnoreCase(sInterLeaved, &stripInterleavedStr);

	// echo back the transport without the interleaved or client ports fields we will add those in ourselves
	if (stripClientPortStr.Len != 0)
		PutTransportStripped(outFirstTransport, stripClientPortStr);
	else if (stripInterleavedStr.Len != 0)
		PutTransportStripped(outFirstTransport, stripInterleavedStr);
	else
		fOutputStream->Put(outFirstTransport);


	//The source IP addr is optional, only append it if it is provided
	if (!serverIPAddr.empty())
	{
		fOutputStream->Put(sSourceString);
		fOutputStream->Put(serverIPAddr);
	}

	// Append the client ports,
	if (stripClientPortStr.Len != 0)
	{
		fOutputStream->Put(sClientPortString);
		uint16_t portA = this->GetClientPortA();
		uint16_t portB = this->GetClientPortB();
		StrPtrLenDel clientPortA(QTSSDataConverter::ValueToString(&portA, sizeof(portA), qtssAttrDataTypeUInt16));
		StrPtrLenDel clientPortB(QTSSDataConverter::ValueToString(&portB, sizeof(portB), qtssAttrDataTypeUInt16));

		fOutputStream->Put(clientPortA);
		fOutputStream->PutChar('-');
		fOutputStream->Put(clientPortB);
	}

	// Append the server ports, if provided.
	if (!serverPortA.empty())
	{
		fOutputStream->Put(sServerPortString);
		fOutputStream->Put(serverPortA);
		fOutputStream->PutChar('-');
		fOutputStream->Put(serverPortB);
	}

	// Append channel #'s, if provided
	if (!channelA.empty())
	{
		fOutputStream->Put(sInterleavedString);
		fOutputStream->Put(channelA);
		fOutputStream->PutChar('-');
		fOutputStream->Put(channelB);
	}

	if (!ssrc.empty() && fNetworkMode == qtssRTPNetworkModeUnicast && fTransportMode == qtssRTPTransportModePlay)
	{
		std::string theCString(ssrc);

		uint32_t ssrcVal = 0;
		::sscanf(theCString.c_str(), "%"   _U32BITARG_   "", &ssrcVal);
		ssrcVal = htonl(ssrcVal);

		StrPtrLen hexSSRC(QTSSDataConverter::ValueToString(&ssrcVal, sizeof(ssrcVal), qtssAttrDataTypeUnknown));
		std::unique_ptr<char[]> hexStrDeleter(hexSSRC.Ptr);

		fOutputStream->Put(sSSRC);
		fOutputStream->Put(hexSSRC);
	}

	fOutputStream->PutEOL();
}

void RTSPRequestInterface::AppendContentBaseHeader(boost::string_view theURL)
{
	if (!fStandardHeadersWritten)
		this->WriteStandardHeaders();

	fOutputStream->Put(RTSPProtocol::GetHeaderString(qtssContentBaseHeader));
	fOutputStream->Put(sColonSpace);
	fOutputStream->Put(theURL);
	fOutputStream->PutChar('/');
	fOutputStream->PutEOL();
}

void RTSPRequestInterface::AppendRetransmitHeader(uint32_t inAckTimeout)
{
	static const StrPtrLen kAckTimeout("ack-timeout=");

	fOutputStream->Put(RTSPProtocol::GetHeaderString(qtssXRetransmitHeader));
	fOutputStream->Put(sColonSpace);
	fOutputStream->Put(RTSPProtocol::GetRetransmitProtocolName());
	fOutputStream->PutChar(';');
	fOutputStream->Put(kAckTimeout);
	fOutputStream->Put(inAckTimeout);

	if (fWindowSizeStr.Len > 0)
	{
		//
		// If the client provided a window size, append that as well.
		fOutputStream->PutChar(';');
		fOutputStream->Put(fWindowSizeStr);
	}

	fOutputStream->PutEOL();

}


void RTSPRequestInterface::AppendRTPInfoHeader(QTSS_RTSPHeader inHeader,
	boost::string_view url, boost::string_view seqNumber,
	boost::string_view ssrc, boost::string_view rtpTime, bool lastRTPInfo)
{
	static StrPtrLen sURL("url=", 4);
	static StrPtrLen sSeq(";seq=", 5);
	static StrPtrLen sSsrc(";ssrc=", 6);
	static StrPtrLen sRTPTime(";rtptime=", 9);

	if (!fStandardHeadersWritten)
		this->WriteStandardHeaders();

	fOutputStream->Put(RTSPProtocol::GetHeaderString(inHeader));
	if (inHeader != qtssSameAsLastHeader)
		fOutputStream->Put(sColonSpace);

	//Only append the various bits of RTP information if they actually have been
	//providied
	if (!url.empty())
	{
		fOutputStream->Put(sURL);

		if (true)
		{
			auto* theRequest = (RTSPRequestInterface*)this;
			boost::string_view path = theRequest->GetAbsoluteURL();

			if (!path.empty())
			{
				fOutputStream->Put(path);
				if (path.back() != '/')
					fOutputStream->PutChar('/');
			}
		}

		fOutputStream->Put(url);
	}
	if (!seqNumber.empty())
	{
		fOutputStream->Put(sSeq);
		fOutputStream->Put(seqNumber);
	}
	if (!ssrc.empty())
	{
		fOutputStream->Put(sSsrc);
		fOutputStream->Put(ssrc);
	}
	if (!rtpTime.empty())
	{
		fOutputStream->Put(sRTPTime);
		fOutputStream->Put(rtpTime);
	}

	if (lastRTPInfo)
		fOutputStream->PutEOL();
}



void RTSPRequestInterface::WriteStandardHeaders()
{
	static boost::string_view    sCloseString("Close");

	fStandardHeadersWritten = true; //must be done here to prevent recursive calls

	//if this is a "200 OK" response (most HTTP responses), we have some special
	//optmizations here
	bool sendServerInfo = QTSServerInterface::GetServer()->GetPrefs()->GetRTSPServerInfoEnabled();
	if (fStatus == qtssSuccessOK)
	{

		if (sendServerInfo)
		{
			fOutputStream->Put(sPremadeHeader);
		}
		else
		{
			fOutputStream->Put(sPremadeNoHeader);
		}
		boost::string_view cSeq = fHeaderDict.Get(qtssCSeqHeader);
		if (!cSeq.empty())
			fOutputStream->Put(cSeq);
		fOutputStream->PutEOL();
	}
	else
	{
#if 0
		// if you want the connection to stay alive when we don't grok
		// the specfied parameter than eneable this code. - [sfu]
		if (fStatus == qtssClientParameterNotUnderstood) {
			fResponseKeepAlive = true;
		}
#endif 
		//other status codes just get built on the fly
		PutStatusLine(fOutputStream, fStatus, RTSPProtocol::k10Version);
		if (sendServerInfo)
		{
			fOutputStream->Put(QTSServerInterface::GetServerHeader());
			fOutputStream->PutEOL();
		}
		AppendHeader(qtssCSeqHeader, fHeaderDict.Get(qtssCSeqHeader));
	}

	//append sessionID header
	boost::string_view incomingID = fHeaderDict.Get(qtssSessionHeader);
	if (!incomingID.empty())
		AppendHeader(qtssSessionHeader, incomingID);

	//follows the HTTP/1.1 convention: if server wants to close the connection, it
	//tags the response with the Connection: close header
	if (!fResponseKeepAlive)
		AppendHeader(qtssConnectionHeader, sCloseString);
}

void RTSPRequestInterface::SendHeader()
{
	if (!fStandardHeadersWritten)
		this->WriteStandardHeaders();
	fOutputStream->PutEOL();
}

QTSS_Error
RTSPRequestInterface::Write(void* inBuffer, uint32_t inLength, uint32_t* outLenWritten, uint32_t /*inFlags*/)
{
	//now just write whatever remains into the output buffer
	fOutputStream->Put((char*)inBuffer, inLength);

	if (outLenWritten != nullptr)
		*outLenWritten = inLength;

	return QTSS_NoErr;
}

QTSS_Error
RTSPRequestInterface::WriteV(iovec* inVec, uint32_t inNumVectors, uint32_t inTotalLength, uint32_t* outLenWritten)
{
	(void)fOutputStream->WriteV(inVec, inNumVectors, inTotalLength, nullptr,
		RTSPResponseStream::kAlwaysBuffer);
	if (outLenWritten != nullptr)
		*outLenWritten = inTotalLength;
	return QTSS_NoErr;
}

//param retrieval functions described in .h file
std::string RTSPRequestInterface::GetAbsTruncatedPath()
{
	// This function gets called only once
	// if qtssRTSPReqAbsoluteURL = rtsp://www.easydarwin.org:554/live.sdp?channel=1&token=888888/trackID=1 
	// then qtssRTSPReqTruncAbsoluteURL = rtsp://www.easydarwin.org:554/live.sdp?channel=1&token=888888
	std::string absTruncatedURL(GetAbsoluteURL());
	size_t pos = absTruncatedURL.rfind("/");
	if (pos == std::string::npos)
		return absTruncatedURL;
	else
		return absTruncatedURL.substr(0, pos);
}

std::string RTSPRequestInterface::GetTruncatedPath()
{
	// This function always gets called
	std::string absTruncatedPath(GetAbsolutePath());
	size_t pos = absTruncatedPath.rfind("/");
	if (pos == std::string::npos)
		return absTruncatedPath;
	else
		return absTruncatedPath.substr(0, pos);
}

std::string RTSPRequestInterface::GetFileName()
{
	// This function always gets called
	std::string str(GetAbsolutePath());
	if (str[0] == '/') str = str.substr(1);
	size_t nextDelimiter = str.find("/");
	if (nextDelimiter == std::string::npos)
		return str;
	else
		return str.substr(0, nextDelimiter);
}


std::string RTSPRequestInterface::GetFileDigit()
{
	std::string theFileDigitV(GetAbsoluteURL());
	StrPtrLen theFileDigit((char *)theFileDigitV.c_str());

	std::string theFilePathV = GetAbsTruncatedPath();
	StrPtrLen theFilePath((char *)theFilePathV.c_str());

	theFileDigit.Ptr += theFileDigit.Len -1;
	theFileDigit.Len = 0;
	while ((StringParser::sDigitMask[(unsigned int) *theFileDigit.Ptr] != '\0') &&
		(theFileDigit.Len <= theFilePath.Len))
	{
		theFileDigit.Ptr--;
		theFileDigit.Len++;
	}
	//termination condition means that we aren't actually on a digit right now.
	//Move pointer back onto the digit
	theFileDigit.Ptr++;

	return std::string(theFileDigit.Ptr, theFileDigit.Len);
}

uint32_t RTSPRequestInterface::GetRealStatusCode()
{
	// Current RTSP status num of this request
	// Set the fRealStatusCode variable based on the current fStatusCode.
	// This function always gets called
	return RTSPProtocol::GetStatusCode(fStatus);
}

boost::string_view RTSPRequestInterface::GetLocalPath() {

	if (!localPath.empty())
		return localPath;

	// This function always gets called	
	std::string filePath(GetAbsolutePath());

	// Get the truncated path on a setup, because setups have the trackID appended
	if (GetMethod() == qtssSetupMethod)
		filePath = GetTruncatedPath();

	boost::string_view theRootDir = GetRootDir();

	char rootDir[512] = { 0 };
	::strncpy(rootDir, theRootDir.data(), theRootDir.length());
	OS::RecursiveMakeDir(rootDir);

	uint32_t fullPathLen = filePath.length() + theRootDir.length();
	auto* theFullPath = new char[fullPathLen + 1];
	theFullPath[fullPathLen] = '\0';

	::memcpy(theFullPath, theRootDir.data(), theRootDir.length());
	::memcpy(theFullPath + theRootDir.length(), filePath.c_str(), filePath.length());

	SetLocalPath({ theFullPath, fullPathLen });

	// delete our copy of the data
	delete[] theFullPath;

	return localPath; 
}

boost::string_view RTSPRequestInterface::GetAuthDigestResponse()
{
	return fAuthDigestResponse;
}