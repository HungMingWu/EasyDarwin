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
	 Copyleft (c) 2012-2016 EasyDarwin.ORG.  All rights reserved.
	 Github: https://github.com/EasyDarwin
	 WEChat: EasyDarwin
	 Website: http://www.EasyDarwin.org
 */
 /*
	 File:       HTTPSessionInterface.cpp
	 Contains:   Implementation of HTTPSessionInterface object.
 */

#include "HTTPSessionInterface.h"
#include "QTSServerInterface.h"
#include <errno.h>


#if DEBUG
#define HTTP_SESSION_INTERFACE_DEBUGGING 1
#else
#define HTTP_SESSION_INTERFACE_DEBUGGING 0
#endif

unsigned int	HTTPSessionInterface::sSessionIndexCounter = kFirstHTTPSessionID;

QTSSAttrInfoDict::AttrInfo  HTTPSessionInterface::sAttributes[] =
{   /*fields:   fAttrName, fFuncPtr, fAttrDataType, fAttrPermission */
	/* 0 */ { "easyHTTPSesID",				nullptr,          qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 1 */ { "easyHTTPSesLocalAddr",       SetupParams,    qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeCacheable },
	/* 2 */ { "easyHTTPSesLocalAddrStr",    SetupParams,    qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeCacheable },
	/* 3 */ { "easyHTTPSesLocalDNS",        SetupParams,    qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeCacheable },
	/* 4 */ { "easyHTTPSesRemoteAddr",      SetupParams,    qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeCacheable },
	/* 5 */ { "easyHTTPSesRemoteAddrStr",   SetupParams,    qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeCacheable },
	/* 6 */ { "easyHTTPSesEventCntxt",      nullptr,           qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 7 */ { "easyHTTPSesLastUserName",    nullptr,           qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe  },
	/* 8 */	{ "easyHTTPSesLastUserPassword",nullptr,           qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe  },
	/* 9 */	{ "easyHTTPSesLastURLRealm",	nullptr,			qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe  },
	/* 10 */{ "easyHTTPSesLocalPort",       SetupParams,    qtssAttrDataTypeUInt16,     qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeCacheable },
	/* 11 */{ "easyHTTPSesRemotePort",      SetupParams,    qtssAttrDataTypeUInt16,     qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeCacheable },
	/* 12 */{ "easyHTTPSesLastToken",		nullptr,			qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
	/* 13 */{ "easyHTTPSesContentBody",		nullptr,           qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
	/* 14 */{ "easyHTTPSesContentBodyOffset",nullptr,          qtssAttrDataTypeUInt32,  qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite }
};


void    HTTPSessionInterface::Initialize()
{
	for (uint32_t x = 0; x < easyHTTPSesNumParams; x++)
		QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kHTTPSessionDictIndex)->
		SetAttribute(x, sAttributes[x].fAttrName, sAttributes[x].fFuncPtr, sAttributes[x].fAttrDataType, sAttributes[x].fAttrPermission);
}

HTTPSessionInterface::HTTPSessionInterface()
	: QTSSDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kHTTPSessionDictIndex)),
	Task(),
	fTimeoutTask(nullptr, QTSServerInterface::GetServer()->GetPrefs()->GetRTSPSessionTimeoutInSecs() * 1000),
	fInputStream(&fSocket),
	fOutputStream(&fSocket, &fTimeoutTask),
	fSessionMutex(),
	fSocket(nullptr, Socket::kNonBlockingSocketType),
	fOutputSocketP(&fSocket),
	fInputSocketP(&fSocket),
	fLiveSession(true),
	fObjectHolders(0),
	fRequestBodyLen(-1),
	fAuthenticated(false)
{

	fTimeoutTask.SetTask(this);
	fSocket.SetTask(this);

	//fSessionIndex = (uint32_t)atomic_add(&sSessionIndexCounter, 1);
	fSessionIndex = ++sSessionIndexCounter;
	this->SetVal(easyHTTPSesID, &fSessionIndex, sizeof(fSessionIndex));

	this->SetVal(easyHTTPSesEventCntxt, &fOutputSocketP, sizeof(fOutputSocketP));

	this->SetEmptyVal(easyHTTPSesLastUserName, &fUserNameBuf[0], kMaxUserNameLen);
	this->SetEmptyVal(easyHTTPSesLastUserPassword, &fUserPasswordBuf[0], kMaxUserPasswordLen);

	fInputStream.ShowRTSP(QTSServerInterface::GetServer()->GetPrefs()->GetRTSPDebugPrintfs());
	fOutputStream.ShowRTSP(QTSServerInterface::GetServer()->GetPrefs()->GetRTSPDebugPrintfs());
}


HTTPSessionInterface::~HTTPSessionInterface()
{
	// If the input socket is != output socket, the input socket was created dynamically
	if (fInputSocketP != fOutputSocketP)
		delete fInputSocketP;
}

void HTTPSessionInterface::DecrementObjectHolderCount()
{

	//#if __Win32__
	//maybe don't need this special case but for now on Win32 we do it the old way since the killEvent code hasn't been verified on Windows.
	this->Signal(Task::kReadEvent);//have the object wakeup in case it can go away.
	//atomic_sub(&fObjectHolders, 1);
	--fObjectHolders;
	//#else
	//    if (0 == atomic_sub(&fObjectHolders, 1))
	//        this->Signal(Task::kKillEvent);
	//#endif

}

QTSS_Error HTTPSessionInterface::Write(void* inBuffer, uint32_t inLength,
	uint32_t* outLenWritten, uint32_t inFlags)
{
	uint32_t sendType = RTSPResponseStream::kDontBuffer;
	if ((inFlags & qtssWriteFlagsBufferData) != 0)
		sendType = RTSPResponseStream::kAlwaysBuffer;

	iovec theVec[2];
	theVec[1].iov_base = (char*)inBuffer;
	theVec[1].iov_len = inLength;
	return fOutputStream.WriteV(theVec, 2, inLength, outLenWritten, sendType);
}

QTSS_Error HTTPSessionInterface::WriteV(iovec* inVec, uint32_t inNumVectors, uint32_t inTotalLength, uint32_t* outLenWritten)
{
	return fOutputStream.WriteV(inVec, inNumVectors, inTotalLength, outLenWritten, RTSPResponseStream::kDontBuffer);
}

QTSS_Error HTTPSessionInterface::Read(void* ioBuffer, uint32_t inLength, uint32_t* outLenRead)
{
	//
	// Don't let callers of this function accidently creep past the end of the
	// request body.  If the request body size isn't known, fRequestBodyLen will be -1

	if (fRequestBodyLen == 0)
		return QTSS_NoMoreData;

	if ((fRequestBodyLen > 0) && ((int32_t)inLength > fRequestBodyLen))
		inLength = fRequestBodyLen;

	uint32_t theLenRead = 0;
	QTSS_Error theErr = fInputStream.Read(ioBuffer, inLength, &theLenRead);

	if (fRequestBodyLen >= 0)
		fRequestBodyLen -= theLenRead;

	if (outLenRead != nullptr)
		*outLenRead = theLenRead;

	return theErr;
}

QTSS_Error HTTPSessionInterface::RequestEvent(QTSS_EventType inEventMask)
{
	if (inEventMask & QTSS_ReadableEvent)
		fInputSocketP->RequestEvent(EV_RE);
	if (inEventMask & QTSS_WriteableEvent)
		fOutputSocketP->RequestEvent(EV_WR);

	return QTSS_NoErr;
}

/*
	take the TCP socket away from a RTSP session that's
	waiting to be snarfed.
*/

void    HTTPSessionInterface::SnarfInputSocket(HTTPSessionInterface* fromRTSPSession)
{
	Assert(fromRTSPSession != nullptr);
	Assert(fromRTSPSession->fOutputSocketP != nullptr);

	fInputStream.SnarfRetreat(fromRTSPSession->fInputStream);

	if (fInputSocketP == fOutputSocketP)
		fInputSocketP = new TCPSocket(this, Socket::kNonBlockingSocketType);
	else
		fInputSocketP->Cleanup();   // if this is a socket replacing an old socket, we need
									// to make sure the file descriptor gets closed
	fInputSocketP->SnarfSocket(fromRTSPSession->fSocket);

	// fInputStream, meet your new input socket
	fInputStream.AttachToSocket(fInputSocketP);
}


void* HTTPSessionInterface::SetupParams(QTSSDictionary* inSession, uint32_t* /*outLen*/)
{
	HTTPSessionInterface* theSession = (HTTPSessionInterface*)inSession;

	theSession->fLocalAddr = theSession->fSocket.GetLocalAddr();
	theSession->fRemoteAddr = theSession->fSocket.GetRemoteAddr();

	theSession->fLocalPort = theSession->fSocket.GetLocalPort();
	theSession->fRemotePort = theSession->fSocket.GetRemotePort();

	StrPtrLen* theLocalAddrStr = theSession->fSocket.GetLocalAddrStr();
	StrPtrLen* theLocalDNSStr = theSession->fSocket.GetLocalDNSStr();
	StrPtrLen* theRemoteAddrStr = theSession->fSocket.GetRemoteAddrStr();
	if (theLocalAddrStr == nullptr || theLocalDNSStr == nullptr || theRemoteAddrStr == nullptr)
	{    //the socket is bad most likely values are all 0. If the socket had an error we shouldn't even be here.
		 //theLocalDNSStr is set to localAddr if it is unavailable, so it should be present at this point as well.
		Assert(0);   //for debugging
		return nullptr; //nothing to set
	}
	theSession->SetVal(easyHTTPSesLocalAddr, &theSession->fLocalAddr, sizeof(theSession->fLocalAddr));
	theSession->SetVal(easyHTTPSesLocalAddrStr, theLocalAddrStr->Ptr, theLocalAddrStr->Len);
	theSession->SetVal(easyHTTPSesLocalDNS, theLocalDNSStr->Ptr, theLocalDNSStr->Len);
	theSession->SetVal(easyHTTPSesRemoteAddr, &theSession->fRemoteAddr, sizeof(theSession->fRemoteAddr));
	theSession->SetVal(easyHTTPSesRemoteAddrStr, theRemoteAddrStr->Ptr, theRemoteAddrStr->Len);

	theSession->SetVal(easyHTTPSesLocalPort, &theSession->fLocalPort, sizeof(theSession->fLocalPort));
	theSession->SetVal(easyHTTPSesRemotePort, &theSession->fRemotePort, sizeof(theSession->fRemotePort));
	return nullptr;
}

QTSS_Error HTTPSessionInterface::SendHTTPPacket(StrPtrLen* contentXML, bool connectionClose, bool decrement)
{
	return QTSS_NoErr;
}