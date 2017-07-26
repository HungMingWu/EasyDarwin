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

#include "RTSPRequestInterface.h"
#include "RTSPSessionInterface.h"
#include "RTSPRequestStream.h"

#include "StringParser.h"
#include "OSThread.h"
#include "DateTranslator.h"
#include "QTSSDataConverter.h"
#include "OSArrayObjectDeleter.h"
#include "QTSServerInterface.h"

char        RTSPRequestInterface::sPremadeHeader[kStaticHeaderSizeInBytes];
StrPtrLen   RTSPRequestInterface::sPremadeHeaderPtr(sPremadeHeader, kStaticHeaderSizeInBytes);

char        RTSPRequestInterface::sPremadeNoHeader[kStaticHeaderSizeInBytes];
StrPtrLen   RTSPRequestInterface::sPremadeNoHeaderPtr(sPremadeNoHeader, kStaticHeaderSizeInBytes);


StrPtrLen   RTSPRequestInterface::sColonSpace(": ", 2);

QTSSAttrInfoDict::AttrInfo  RTSPRequestInterface::sAttributes[] =
{   /*fields:   fAttrName, fFuncPtr, fAttrDataType, fAttrPermission */
	/* 0 */ { "qtssRTSPReqFullRequest",         nullptr,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 1 */ { "qtssRTSPReqMethodStr",           nullptr,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 2 */ { "qtssRTSPReqFilePath",            nullptr,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModeWrite },
	/* 3 */ { "qtssRTSPReqURI",                 nullptr,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 4 */ { "qtssRTSPReqFilePathTrunc",       GetTruncatedPath,       qtssAttrDataTypeCharArray,  qtssAttrModeRead },
	/* 5 */ { "qtssRTSPReqFileName",            GetFileName,            qtssAttrDataTypeCharArray,  qtssAttrModeRead },
	/* 6 */ { "qtssRTSPReqFileDigit",           GetFileDigit,           qtssAttrDataTypeCharArray,  qtssAttrModeRead },
	/* 7 */ { "qtssRTSPReqAbsoluteURL",         nullptr,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 8 */ { "qtssRTSPReqTruncAbsoluteURL",    GetAbsTruncatedPath,    qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeCacheable },
	/* 9 */ { "qtssRTSPReqMethod",              nullptr,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 10 */ { "qtssRTSPReqStatusCode",         nullptr,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
	/* 11 */ { "qtssRTSPReqStartTime",          nullptr,                   qtssAttrDataTypeFloat64,    qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 12 */ { "qtssRTSPReqStopTime",           nullptr,                   qtssAttrDataTypeFloat64,    qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 13 */ { "qtssRTSPReqRespKeepAlive",      nullptr,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
	/* 14 */ { "qtssRTSPReqRootDir",            nullptr,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModeWrite },
	/* 15 */ { "qtssRTSPReqRealStatusCode",     GetRealStatusCode,      qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 16 */ { "qtssRTSPReqStreamRef",          nullptr,                   qtssAttrDataTypeQTSS_StreamRef, qtssAttrModeRead | qtssAttrModePreempSafe },

	/* 17 */ { "qtssRTSPReqUserName",           nullptr,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 18 */ { "qtssRTSPReqUserPassword",       nullptr,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 19 */ { "qtssRTSPReqUserAllowed",        nullptr,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
	/* 20 */ { "qtssRTSPReqURLRealm",           nullptr,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
	/* 21 */ { "qtssRTSPReqLocalPath",          GetLocalPath,			qtssAttrDataTypeCharArray,  qtssAttrModeRead },
	/* 22 */ { "qtssRTSPReqIfModSinceDate",     nullptr,                   qtssAttrDataTypeTimeVal,    qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 23 */ { "qtssRTSPReqQueryString",        nullptr,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 24 */ { "qtssRTSPReqRespMsg",            nullptr,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
	/* 25 */ { "qtssRTSPReqContentLen",         nullptr,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 26 */ { "qtssRTSPReqSpeed",              nullptr,                   qtssAttrDataTypeFloat32,    qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 27 */ { "qtssRTSPReqLateTolerance",      nullptr,                   qtssAttrDataTypeFloat32,    qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 28 */ { "qtssRTSPReqTransportType",      nullptr,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 29 */ { "qtssRTSPReqTransportMode",      nullptr,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 30 */ { "qtssRTSPReqSetUpServerPort",    nullptr,                   qtssAttrDataTypeUInt16,     qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite},
	/* 31 */ { "qtssRTSPReqAction",             nullptr,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
	/* 32 */ { "qtssRTSPReqUserProfile",        nullptr,                   qtssAttrDataTypeQTSS_Object, qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
	/* 33 */ { "qtssRTSPReqPrebufferMaxTime",   nullptr,                   qtssAttrDataTypeFloat32,    qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 34 */ { "qtssRTSPReqAuthScheme",         nullptr,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
	/* 35 */ { "qtssRTSPReqSkipAuthorization",  nullptr,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
	/* 36 */ { "qtssRTSPReqNetworkMode",		nullptr,					qtssAttrDataTypeUInt32,		qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 37 */ { "qtssRTSPReqDynamicRateValue",	nullptr,					qtssAttrDataTypeint32_t,		qtssAttrModeRead | qtssAttrModePreempSafe },

	/* 39 */ { "qtssRTSPReqBandwidthBits",	    nullptr,					qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 39 */ { "qtssRTSPReqUserFound",          nullptr,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
	/* 40 */ { "qtssRTSPReqAuthHandled",        nullptr,                   qtssAttrDataTypeBool16,     qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
	/* 41 */ { "qtssRTSPReqDigestChallenge",    nullptr,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 42 */ { "qtssRTSPReqDigestResponse",     GetAuthDigestResponse,  qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe }
};


void  RTSPRequestInterface::Initialize(void)
{
	//make a partially complete header
	StringFormatter headerFormatter(sPremadeHeaderPtr.Ptr, kStaticHeaderSizeInBytes);
	PutStatusLine(&headerFormatter, qtssSuccessOK, RTSPProtocol::k10Version);

	headerFormatter.Put(QTSServerInterface::GetServerHeader());
	headerFormatter.PutEOL();
	headerFormatter.Put(RTSPProtocol::GetHeaderString(qtssCSeqHeader));
	headerFormatter.Put(sColonSpace);
	sPremadeHeaderPtr.Len = headerFormatter.GetCurrentOffset();
	Assert(sPremadeHeaderPtr.Len < kStaticHeaderSizeInBytes);


	StringFormatter noServerInfoHeaderFormatter(sPremadeNoHeaderPtr.Ptr, kStaticHeaderSizeInBytes);
	PutStatusLine(&noServerInfoHeaderFormatter, qtssSuccessOK, RTSPProtocol::k10Version);
	noServerInfoHeaderFormatter.Put(RTSPProtocol::GetHeaderString(qtssCSeqHeader));
	noServerInfoHeaderFormatter.Put(sColonSpace);
	sPremadeNoHeaderPtr.Len = noServerInfoHeaderFormatter.GetCurrentOffset();
	Assert(sPremadeNoHeaderPtr.Len < kStaticHeaderSizeInBytes);

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
	this->SetVal(qtssRTSPReqFullRequest, input->GetRequestBuffer()->Ptr, input->GetRequestBuffer()->Len);

	// klaus(20170223):fix ffplay cant pull stream from easydarwin
	fHeaderDictionary.SetVal(qtssSessionHeader, nullptr, 0);
	fHeaderDictionary.SetNumValues(qtssSessionHeader, 0);
}

//CONSTRUCTOR / DESTRUCTOR: very simple stuff
RTSPRequestInterface::RTSPRequestInterface(RTSPSessionInterface *session)
	: QTSSDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kRTSPRequestDictIndex)),
	fMethod(qtssIllegalMethod),
	fStatus(qtssSuccessOK),
	fRealStatusCode(0),
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
	fHeaderDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kRTSPHeaderDictIndex)),
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
	//Setup QTSS parameters that can be setup now. These are typically the parameters that are actually
	//pointers to binary variable values. Because these variables are just member variables of this object,
	//we can properly initialize their pointers right off the bat.

	fStreamRef = this;
	RTSPRequestStream* input = session->GetInputStream();
	this->SetVal(qtssRTSPReqFullRequest, input->GetRequestBuffer()->Ptr, input->GetRequestBuffer()->Len);
	this->SetVal(qtssRTSPReqMethod, &fMethod, sizeof(fMethod));
	this->SetVal(qtssRTSPReqStatusCode, &fStatus, sizeof(fStatus));
	this->SetVal(qtssRTSPReqRespKeepAlive, &fResponseKeepAlive, sizeof(fResponseKeepAlive));
	this->SetVal(qtssRTSPReqStreamRef, &fStreamRef, sizeof(fStreamRef));
	this->SetVal(qtssRTSPReqContentLen, &fContentLength, sizeof(fContentLength));
	this->SetVal(qtssRTSPReqSpeed, &fSpeed, sizeof(fSpeed));
	this->SetVal(qtssRTSPReqLateTolerance, &fLateTolerance, sizeof(fLateTolerance));
	this->SetVal(qtssRTSPReqPrebufferMaxTime, &fPrebufferAmt, sizeof(fPrebufferAmt));

	// Get the default root directory from QTSSPrefs, and store that in the proper parameter
	// Note that the GetMovieFolderPath function may allocate memory, so we check for that
	// in this object's destructor and free that memory if necessary.
	//uint32_t pathLen = kMovieFolderBufSizeInBytes;
	//fMovieFolderPtr = QTSServerInterface::GetServer()->GetPrefs()->GetMovieFolder(fMovieFolderPtr, &pathLen);
	//this->SetValue(qtssRTSPReqRootDir, 0, fMovieFolderPtr, pathLen, QTSSDictionary::kDontObeyReadOnly);

	//There are actually other attributes that point to member variables that we COULD setup now, but they are attributes that
	//typically aren't set for every request, so we lazy initialize those when we parse the request

	this->SetVal(qtssRTSPReqUserAllowed, &fAllowed, sizeof(fAllowed));
	this->SetVal(qtssRTSPReqUserFound, &fHasUser, sizeof(fHasUser));
	this->SetVal(qtssRTSPReqAuthHandled, &fAuthHandled, sizeof(fAuthHandled));

	this->SetVal(qtssRTSPReqTransportType, &fTransportType, sizeof(fTransportType));
	this->SetVal(qtssRTSPReqTransportMode, &fTransportMode, sizeof(fTransportMode));
	this->SetVal(qtssRTSPReqSetUpServerPort, &fSetUpServerPort, sizeof(fSetUpServerPort));
	this->SetVal(qtssRTSPReqAction, &fAction, sizeof(fAction));
	this->SetVal(qtssRTSPReqUserProfile, &fUserProfilePtr, sizeof(fUserProfilePtr));
	this->SetVal(qtssRTSPReqAuthScheme, &fAuthScheme, sizeof(fAuthScheme));
	this->SetVal(qtssRTSPReqSkipAuthorization, &fSkipAuthorization, sizeof(fSkipAuthorization));

	this->SetVal(qtssRTSPReqDynamicRateState, &fEnableDynamicRateState, sizeof(fEnableDynamicRateState));

	this->SetVal(qtssRTSPReqBandwidthBits, &fBandwidthBits, sizeof(fBandwidthBits));

	this->SetVal(qtssRTSPReqDigestResponse, &fAuthDigestResponse, sizeof(fAuthDigestResponse));


}

void RTSPRequestInterface::AppendHeader(QTSS_RTSPHeader inHeader, StrPtrLen* inValue)
{
	if (!fStandardHeadersWritten)
		this->WriteStandardHeaders();

	fOutputStream->Put(RTSPProtocol::GetHeaderString(inHeader));
	fOutputStream->Put(sColonSpace);
	fOutputStream->Put(*inValue);
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

	char dataSize[10];
	dataSize[sizeof(dataSize) - 1] = 0;
	snprintf(dataSize, sizeof(dataSize) - 1, "%"   _U32BITARG_   "", contentLength);
	StrPtrLen contentLengthStr(dataSize);
	this->AppendHeader(qtssContentLengthHeader, &contentLengthStr);

}

void RTSPRequestInterface::AppendDateAndExpires()
{
	if (!fStandardHeadersWritten)
		this->WriteStandardHeaders();

	Assert(OSThread::GetCurrent() != nullptr);
	DateBuffer* theDateBuffer = OSThread::GetCurrent()->GetDateBuffer();
	theDateBuffer->InexactUpdate(); // Update the date buffer to the current date & time
	StrPtrLen theDate(theDateBuffer->GetDateBuffer(), DateBuffer::kDateBufferLen);

	// Append dates, and have this response expire immediately
	this->AppendHeader(qtssDateHeader, &theDate);
	this->AppendHeader(qtssExpiresHeader, &theDate);
}


void RTSPRequestInterface::AppendSessionHeaderWithTimeout(StrPtrLen* inSessionID, StrPtrLen* inTimeout)
{

	// Append a session header if there wasn't one already
	if (GetHeaderDictionary()->GetValue(qtssSessionHeader)->Len == 0)
	{
		if (!fStandardHeadersWritten)
			this->WriteStandardHeaders();

		static StrPtrLen    sTimeoutString(";timeout=");

		// Just write out the session header and session ID
		if (inSessionID != nullptr && inSessionID->Len > 0)
		{
			fOutputStream->Put(RTSPProtocol::GetHeaderString(qtssSessionHeader));
			fOutputStream->Put(sColonSpace);
			fOutputStream->Put(*inSessionID);


			if (inTimeout != nullptr && inTimeout->Len != 0)
			{
				fOutputStream->Put(sTimeoutString);
				fOutputStream->Put(*inTimeout);
			}


			fOutputStream->PutEOL();
		}
	}

}

void RTSPRequestInterface::PutTransportStripped(StrPtrLen &fullTransportHeader, StrPtrLen &fieldToStrip)
{

	// skip the fieldToStrip and echo the rest back
	uint32_t offset = (uint32_t)(fieldToStrip.Ptr - fullTransportHeader.Ptr);
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

void RTSPRequestInterface::AppendTransportHeader(StrPtrLen* serverPortA,
	StrPtrLen* serverPortB,
	StrPtrLen* channelA,
	StrPtrLen* channelB,
	StrPtrLen* serverIPAddr,
	StrPtrLen* ssrc)
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
	OSCharArrayDeleter outFirstTransportDeleter(outFirstTransport.Ptr);
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
	if (serverIPAddr != nullptr)
	{
		fOutputStream->Put(sSourceString);
		fOutputStream->Put(*serverIPAddr);
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
	if (serverPortA != nullptr)
	{
		fOutputStream->Put(sServerPortString);
		fOutputStream->Put(*serverPortA);
		fOutputStream->PutChar('-');
		fOutputStream->Put(*serverPortB);
	}

	// Append channel #'s, if provided
	if (channelA != nullptr)
	{
		fOutputStream->Put(sInterleavedString);
		fOutputStream->Put(*channelA);
		fOutputStream->PutChar('-');
		fOutputStream->Put(*channelB);
	}

	if (ssrc != nullptr && ssrc->Ptr != nullptr && ssrc->Len != 0 && fNetworkMode == qtssRTPNetworkModeUnicast && fTransportMode == qtssRTPTransportModePlay)
	{
		char* theCString = ssrc->GetAsCString();
		OSCharArrayDeleter cStrDeleter(theCString);

		uint32_t ssrcVal = 0;
		::sscanf(theCString, "%"   _U32BITARG_   "", &ssrcVal);
		ssrcVal = htonl(ssrcVal);

		StrPtrLen hexSSRC(QTSSDataConverter::ValueToString(&ssrcVal, sizeof(ssrcVal), qtssAttrDataTypeUnknown));
		OSCharArrayDeleter hexStrDeleter(hexSSRC.Ptr);

		fOutputStream->Put(sSSRC);
		fOutputStream->Put(hexSSRC);
	}

	fOutputStream->PutEOL();
}

void RTSPRequestInterface::AppendContentBaseHeader(StrPtrLen* theURL)
{
	if (!fStandardHeadersWritten)
		this->WriteStandardHeaders();

	fOutputStream->Put(RTSPProtocol::GetHeaderString(qtssContentBaseHeader));
	fOutputStream->Put(sColonSpace);
	fOutputStream->Put(*theURL);
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
	StrPtrLen* url, StrPtrLen* seqNumber,
	StrPtrLen* ssrc, StrPtrLen* rtpTime, bool lastRTPInfo)
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
	if ((url != nullptr) && (url->Len > 0))
	{
		fOutputStream->Put(sURL);

		if (true)
		{
			RTSPRequestInterface* theRequest = (RTSPRequestInterface*)this;
			StrPtrLen *path = (StrPtrLen *)theRequest->GetValue(qtssRTSPReqAbsoluteURL);

			if (path != nullptr && path->Len > 0)
			{
				fOutputStream->Put(*path);
				if (path->Ptr[path->Len - 1] != '/')
					fOutputStream->PutChar('/');
			}
		}

		fOutputStream->Put(*url);
	}
	if ((seqNumber != nullptr) && (seqNumber->Len > 0))
	{
		fOutputStream->Put(sSeq);
		fOutputStream->Put(*seqNumber);
	}
	if ((ssrc != nullptr) && (ssrc->Len > 0))
	{
		fOutputStream->Put(sSsrc);
		fOutputStream->Put(*ssrc);
	}
	if ((rtpTime != nullptr) && (rtpTime->Len > 0))
	{
		fOutputStream->Put(sRTPTime);
		fOutputStream->Put(*rtpTime);
	}

	if (lastRTPInfo)
		fOutputStream->PutEOL();
}



void RTSPRequestInterface::WriteStandardHeaders()
{
	static StrPtrLen    sCloseString("Close", 5);

	Assert(sPremadeHeader != nullptr);
	fStandardHeadersWritten = true; //must be done here to prevent recursive calls

	//if this is a "200 OK" response (most HTTP responses), we have some special
	//optmizations here
	bool sendServerInfo = QTSServerInterface::GetServer()->GetPrefs()->GetRTSPServerInfoEnabled();
	if (fStatus == qtssSuccessOK)
	{

		if (sendServerInfo)
		{
			fOutputStream->Put(sPremadeHeaderPtr);
		}
		else
		{
			fOutputStream->Put(sPremadeNoHeaderPtr);
		}
		StrPtrLen* cSeq = fHeaderDictionary.GetValue(qtssCSeqHeader);
		Assert(cSeq != nullptr);
		if (cSeq->Len > 1)
			fOutputStream->Put(*cSeq);
		else if (cSeq->Len == 1)
			fOutputStream->PutChar(*cSeq->Ptr);
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
		AppendHeader(qtssCSeqHeader, fHeaderDictionary.GetValue(qtssCSeqHeader));
	}

	//append sessionID header
	StrPtrLen* incomingID = fHeaderDictionary.GetValue(qtssSessionHeader);
	if ((incomingID != nullptr) && (incomingID->Len > 0))
		AppendHeader(qtssSessionHeader, incomingID);

	//follows the HTTP/1.1 convention: if server wants to close the connection, it
	//tags the response with the Connection: close header
	if (!fResponseKeepAlive)
		AppendHeader(qtssConnectionHeader, &sCloseString);
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
void* RTSPRequestInterface::GetAbsTruncatedPath(QTSSDictionary* inRequest, uint32_t* /*outLen*/)
{
	// This function gets called only once
	// if qtssRTSPReqAbsoluteURL = rtsp://www.easydarwin.org:554/live.sdp?channel=1&token=888888/trackID=1 
	// then qtssRTSPReqTruncAbsoluteURL = rtsp://www.easydarwin.org:554/live.sdp?channel=1&token=888888

	RTSPRequestInterface* theRequest = (RTSPRequestInterface*)inRequest;
	theRequest->SetVal(qtssRTSPReqTruncAbsoluteURL, theRequest->GetValue(qtssRTSPReqAbsoluteURL));

	//Adjust the length to truncate off the last file in the path

	StrPtrLen* theAbsTruncPathParam = theRequest->GetValue(qtssRTSPReqTruncAbsoluteURL);
	theAbsTruncPathParam->Len--;
	while (theAbsTruncPathParam->Ptr[theAbsTruncPathParam->Len] != kPathDelimiterChar)
		theAbsTruncPathParam->Len--;

	return nullptr;
}

void* RTSPRequestInterface::GetTruncatedPath(QTSSDictionary* inRequest, uint32_t* /*outLen*/)
{
	// This function always gets called

	RTSPRequestInterface* theRequest = (RTSPRequestInterface*)inRequest;
	theRequest->SetVal(qtssRTSPReqFilePathTrunc, theRequest->GetValue(qtssRTSPReqFilePath));

	//Adjust the length to truncate off the last file in the path
	StrPtrLen* theTruncPathParam = theRequest->GetValue(qtssRTSPReqFilePathTrunc);

	if (theTruncPathParam->Len > 0)
	{
		theTruncPathParam->Len--;
		while ((theTruncPathParam->Len != 0) && (theTruncPathParam->Ptr[theTruncPathParam->Len] != kPathDelimiterChar))
			theTruncPathParam->Len--;
	}

	return nullptr;
}

void* RTSPRequestInterface::GetFileName(QTSSDictionary* inRequest, uint32_t* /*outLen*/)
{
	// This function always gets called

	RTSPRequestInterface* theRequest = (RTSPRequestInterface*)inRequest;
	theRequest->SetVal(qtssRTSPReqFileName, theRequest->GetValue(qtssRTSPReqFilePath));

	StrPtrLen* theFileNameParam = theRequest->GetValue(qtssRTSPReqFileName);

	//paranoid check
	if (theFileNameParam->Len == 0)
		return theFileNameParam;

	if (theFileNameParam->Ptr[0] == kPathDelimiterChar)
	{
		theFileNameParam->Ptr++;
		theFileNameParam->Len--;
	}

	//walk back in the file name until we hit a /
	int32_t x = 0;
	int i = 0;
	for (; x < theFileNameParam->Len; x++ )
		if (theFileNameParam->Ptr[x] == kPathDelimiterChar)
			break;
	//once we do, make the tempPtr point to the next character after the slash,
	//and adjust the length accordingly
	theFileNameParam->Len = x;

	return nullptr;
}


void* RTSPRequestInterface::GetFileDigit(QTSSDictionary* inRequest, uint32_t* /*outLen*/)
{
	// This function always gets called

	RTSPRequestInterface* theRequest = (RTSPRequestInterface*)inRequest;
	theRequest->SetVal(qtssRTSPReqFileDigit, theRequest->GetValue(qtssRTSPReqAbsoluteURL));


	StrPtrLen* theFileDigit = theRequest->GetValue(qtssRTSPReqFileDigit);

	StrPtrLen theFilePath;
	(void)QTSS_GetValuePtr(inRequest, qtssRTSPReqTruncAbsoluteURL, 0, (void**)&theFilePath.Ptr, &theFilePath.Len);

	//uint32_t  theFilePathLen = theRequest->GetValue(qtssRTSPReqTruncAbsoluteURL)->Len;
	theFileDigit->Ptr += theFileDigit->Len -1;
	theFileDigit->Len = 0;
	while ((StringParser::sDigitMask[(unsigned int) *(*theFileDigit).Ptr] != '\0') &&
		(theFileDigit->Len <= theFilePath.Len))
	{
		theFileDigit->Ptr--;
		theFileDigit->Len++;
	}
	//termination condition means that we aren't actually on a digit right now.
	//Move pointer back onto the digit
	theFileDigit->Ptr++;

	return nullptr;
}

void* RTSPRequestInterface::GetRealStatusCode(QTSSDictionary* inRequest, uint32_t* outLen)
{
	// Set the fRealStatusCode variable based on the current fStatusCode.
	// This function always gets called
	RTSPRequestInterface* theReq = (RTSPRequestInterface*)inRequest;
	theReq->fRealStatusCode = RTSPProtocol::GetStatusCode(theReq->fStatus);
	*outLen = sizeof(uint32_t);
	return &theReq->fRealStatusCode;
}

void* RTSPRequestInterface::GetLocalPath(QTSSDictionary* inRequest, uint32_t* outLen)
{
	// This function always gets called	
	RTSPRequestInterface* theRequest = (RTSPRequestInterface*)inRequest;
	QTSS_AttributeID theID = qtssRTSPReqFilePath;

	// Get the truncated path on a setup, because setups have the trackID appended
	if (theRequest->GetMethod() == qtssSetupMethod)
	{
		theID = qtssRTSPReqFilePathTrunc;
		// invoke the param retrieval function here so that we can use the internal GetValue function later  
		RTSPRequestInterface::GetTruncatedPath(inRequest, outLen);
	}

	StrPtrLen* thePath = theRequest->GetValue(theID);
	StrPtrLen filePath(thePath->Ptr, thePath->Len);
	StrPtrLen* theRootDir = theRequest->GetValue(qtssRTSPReqRootDir);
	if (theRootDir->Len && theRootDir->Ptr[theRootDir->Len - 1] == kPathDelimiterChar
		&& thePath->Len  && thePath->Ptr[0] == kPathDelimiterChar)
	{
		char *thePathEnd = &(filePath.Ptr[filePath.Len]);
		while (filePath.Ptr != thePathEnd)
		{
			if (*filePath.Ptr != kPathDelimiterChar)
				break;

			filePath.Ptr++;
			filePath.Len--;
		}
	}

	char rootDir[512] = { 0 };
	::strncpy(rootDir, theRootDir->Ptr, theRootDir->Len);
	OS::RecursiveMakeDir(rootDir);

	uint32_t fullPathLen = filePath.Len + theRootDir->Len;
	char* theFullPath = new char[fullPathLen + 1];
	theFullPath[fullPathLen] = '\0';

	::memcpy(theFullPath, theRootDir->Ptr, theRootDir->Len);
	::memcpy(theFullPath + theRootDir->Len, filePath.Ptr, filePath.Len);

	(void)theRequest->SetValue(qtssRTSPReqLocalPath, 0, theFullPath, fullPathLen, QTSSDictionary::kDontObeyReadOnly);

	// delete our copy of the data
	delete[] theFullPath;
	*outLen = 0;

	return nullptr;
}

void* RTSPRequestInterface::GetAuthDigestResponse(QTSSDictionary* inRequest, uint32_t*)
{
	RTSPRequestInterface* theRequest = (RTSPRequestInterface*)inRequest;
	(void)theRequest->SetValue(qtssRTSPReqDigestResponse, 0, theRequest->fAuthDigestResponse.Ptr, theRequest->fAuthDigestResponse.Len, QTSSDictionary::kDontObeyReadOnly);
	return nullptr;
}

