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
	 File:       RTSPSessionInterface.cpp

	 Contains:   Implementation of RTSPSessionInterface object.
 */

#include <boost/algorithm/string/predicate.hpp>

#include "RTSPSessionInterface.h"
#include "QTSServerInterface.h"
#include "RTSPProtocol.h"
#include <errno.h>


#if DEBUG
#define RTSP_SESSION_INTERFACE_DEBUGGING 1
#else
#define RTSP_SESSION_INTERFACE_DEBUGGING 0
#endif

unsigned int            RTSPSessionInterface::sSessionIDCounter = kFirstRTSPSessionID;
bool                  RTSPSessionInterface::sDoBase64Decoding = true;
uint32_t					RTSPSessionInterface::sOptionsRequestBody[kMaxRandomDataSize / sizeof(uint32_t)];

void    RTSPSessionInterface::Initialize()
{
	// DJM PROTOTYPE
	::srand((unsigned int)OS::Microseconds());
	for (unsigned int i = 0; i < kMaxRandomDataSize / sizeof(uint32_t); i++)
		RTSPSessionInterface::sOptionsRequestBody[i] = ::rand();
	((char *)RTSPSessionInterface::sOptionsRequestBody)[0] = 0; //always set first byte so it doesn't hit any client parser bugs for \r or \n.

}


RTSPSessionInterface::RTSPSessionInterface()
	: Task(),
	fTimeoutTask(nullptr, QTSServerInterface::GetServer()->GetPrefs()->GetRTSPSessionTimeoutInSecs() * 1000),
	fInputStream(&fSocket),
	fOutputStream(&fSocket, &fTimeoutTask),
	fSessionMutex(),
	fSocket(nullptr, Socket::kNonBlockingSocketType),
	fOutputSocketP(&fSocket),
	fInputSocketP(&fSocket)
{
	fTimeoutTask.SetTask(this);
	fSocket.SetTask(this);

	//fSessionID = (uint32_t)atomic_add(&sSessionIDCounter, 1);
	fSessionID = ++sSessionIDCounter;

	fInputStream.ShowRTSP(QTSServerInterface::GetServer()->GetPrefs()->GetRTSPDebugPrintfs());
	fOutputStream.ShowRTSP(QTSServerInterface::GetServer()->GetPrefs()->GetRTSPDebugPrintfs());
}


RTSPSessionInterface::~RTSPSessionInterface()
{
	// If the input socket is != output socket, the input socket was created dynamically
	if (fInputSocketP != fOutputSocketP)
		delete fInputSocketP;

	delete[] fTCPCoalesceBuffer;
}

void RTSPSessionInterface::DecrementObjectHolderCount()
{

//#if __Win32__
//	//maybe don't need this special case but for now on Win32 we do it the old way since the killEvent code hasn't been verified on Windows.
//	this->Signal(Task::kReadEvent);//have the object wakeup in case it can go away.
//	//atomic_sub(&fObjectHolders, 1);
//	--fObjectHolders;
//#else
//	if (0 == --fObjectHolders)
//		this->Signal(Task::kKillEvent);
//#endif

	if (0 == --fObjectHolders)
		this->Signal(Task::kKillEvent);
}

QTSS_Error RTSPSessionInterface::Write(void* inBuffer, uint32_t inLength,
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

QTSS_Error RTSPSessionInterface::WriteV(iovec* inVec, uint32_t inNumVectors, uint32_t inTotalLength, uint32_t* outLenWritten)
{
	return fOutputStream.WriteV(inVec, inNumVectors, inTotalLength, outLenWritten, RTSPResponseStream::kDontBuffer);
}

QTSS_Error RTSPSessionInterface::Read(void* ioBuffer, uint32_t inLength, uint32_t* outLenRead)
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

QTSS_Error RTSPSessionInterface::RequestEvent(QTSS_EventType inEventMask)
{
	if (inEventMask & QTSS_ReadableEvent)
		fInputSocketP->RequestEvent(EV_RE);
	if (inEventMask & QTSS_WriteableEvent)
		fOutputSocketP->RequestEvent(EV_WR);

	return QTSS_NoErr;
}

uint8_t RTSPSessionInterface::GetTwoChannelNumbers(boost::string_view inRTSPSessionID)
{
	//
	// Allocate a TCP coalesce buffer if still needed
	if (fTCPCoalesceBuffer != nullptr)
		fTCPCoalesceBuffer = new char[kTCPCoalesceBufferSize];

	//
	// Allocate 2 channel numbers
	uint8_t theChannelNum = fCurChannelNum;
	fCurChannelNum += 2;

	//
	// Put this sessionID to the proper place in the map
	fChNumToSessIDMap.emplace_back(inRTSPSessionID.data(), inRTSPSessionID.length());

	return theChannelNum;
}

boost::string_view  RTSPSessionInterface::GetSessionIDForChannelNum(uint8_t inChannelNum)
{
	if (inChannelNum < fCurChannelNum)
		return fChNumToSessIDMap[inChannelNum >> 1];
	else
		return {};
}

/*********************************
/
/   InterleavedWrite
/
/   Write the given RTP packet out on the RTSP channel in interleaved format.
/
*/

QTSS_Error RTSPSessionInterface::InterleavedWrite(void* inBuffer, uint32_t inLen, uint32_t* outLenWritten, unsigned char channel)
{

	if (inLen == 0 && fNumInCoalesceBuffer == 0)
	{
		if (outLenWritten != nullptr)
			*outLenWritten = 0;
		return QTSS_NoErr;
	}

	// First attempt to grab the RTSPSession mutex. This is to prevent writing data to
	// the connection at the same time an RTSPRequest is being processed. We cannot
	// wait for this mutex to be freed (there would be a deadlock possibility), so
	// just try to grab it, and if we can't, then just report it as an EAGAIN
	if (this->GetSessionMutex()->TryLock() == false)
	{
		return EAGAIN;
	}

	// DMS - this struct should be packed.
	//rt todo -- is this struct more portable (byte alignment could be a problem)?
	struct  RTPInterleaveHeader
	{
		unsigned char header;
		unsigned char channel;
		uint16_t      len;
	};

	struct  iovec               iov[3];
	QTSS_Error                  err = QTSS_NoErr;



	// flush rules
	if ((inLen > kTCPCoalesceDirectWriteSize || inLen == 0) && fNumInCoalesceBuffer > 0
		|| (inLen + fNumInCoalesceBuffer + kInteleaveHeaderSize > kTCPCoalesceBufferSize) && fNumInCoalesceBuffer > 0
		)
	{
		uint32_t      buffLenWritten;

		// skip iov[0], WriteV uses it
		iov[1].iov_base = fTCPCoalesceBuffer;
		iov[1].iov_len = fNumInCoalesceBuffer;

		err = this->GetOutputStream()->WriteV(iov, 2, fNumInCoalesceBuffer, &buffLenWritten, RTSPResponseStream::kAllOrNothing);

#if RTSP_SESSION_INTERFACE_DEBUGGING 
		printf("InterleavedWrite: flushing %li\n", fNumInCoalesceBuffer);
#endif

		if (err == QTSS_NoErr)
			fNumInCoalesceBuffer = 0;
	}



	if (err == QTSS_NoErr)
	{

		if (inLen > kTCPCoalesceDirectWriteSize)
		{
			struct RTPInterleaveHeader  rih;

			// write direct to stream
			rih.header = '$';
			rih.channel = channel;
			rih.len = htons((uint16_t)inLen);

			iov[1].iov_base = (char*)&rih;
			iov[1].iov_len = sizeof(rih);

			iov[2].iov_base = (char*)inBuffer;
			iov[2].iov_len = inLen;

			err = this->GetOutputStream()->WriteV(iov, 3, inLen + sizeof(rih), outLenWritten, RTSPResponseStream::kAllOrNothing);

#if RTSP_SESSION_INTERFACE_DEBUGGING 
			printf("InterleavedWrite: bypass %li\n", inLen);
#endif

		}
		else
		{
			// coalesce with other small writes

			fTCPCoalesceBuffer[fNumInCoalesceBuffer] = '$';
			fNumInCoalesceBuffer++;;

			fTCPCoalesceBuffer[fNumInCoalesceBuffer] = channel;
			fNumInCoalesceBuffer++;

			//*((short*)&fTCPCoalesceBuffer[fNumInCoalesceBuffer]) = htons(inLen);
			// if we ever turn TCPCoalesce back on, this should be optimized
			// for processors w/o alignment restrictions as above.

			int16_t  pcketLen = htons((uint16_t)inLen);
			::memcpy(&fTCPCoalesceBuffer[fNumInCoalesceBuffer], &pcketLen, 2);
			fNumInCoalesceBuffer += 2;

			::memcpy(&fTCPCoalesceBuffer[fNumInCoalesceBuffer], inBuffer, inLen);
			fNumInCoalesceBuffer += inLen;

#if RTSP_SESSION_INTERFACE_DEBUGGING 
			printf("InterleavedWrite: coalesce %li, total bufff %li\n", inLen, fNumInCoalesceBuffer);
#endif
		}
	}

	if (err == QTSS_NoErr)
	{
		/*  if no error sure to correct outLenWritten, cuz WriteV above includes the interleave header count

			 GetOutputStream()->WriteV guarantees all or nothing for writes
			 if no error, then all was written.
		*/
		if (outLenWritten != nullptr)
			*outLenWritten = inLen;
	}

	this->GetSessionMutex()->Unlock();


	return err;

}

/*
	take the TCP socket away from a RTSP session that's
	waiting to be snarfed.

*/

void    RTSPSessionInterface::SnarfInputSocket(RTSPSessionInterface* fromRTSPSession)
{
	Assert(fromRTSPSession != nullptr);
	Assert(fromRTSPSession->fOutputSocketP != nullptr);

	// grab the unused, but already read fromsocket data
	// this should be the first RTSP request
	if (sDoBase64Decoding)
		fInputStream.IsBase64Encoded(true); // client sends all data base64 encoded
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

std::string RTSPSessionInterface::GetLocalAddr()
{
	StrPtrLen* theLocalAddrStr = fSocket.GetLocalAddrStr();
	return std::string(theLocalAddrStr->Ptr, theLocalAddrStr->Len);
}

std::string RTSPSessionInterface::GetLocalDNS()
{
	StrPtrLen* theLocalDNSStr = fSocket.GetLocalDNSStr();
	return std::string(theLocalDNSStr->Ptr, theLocalDNSStr->Len);
}

std::string RTSPSessionInterface::GetRemoteAddr()
{
	StrPtrLen* theRemoteAddrStr = fSocket.GetRemoteAddrStr();
	return std::string(theRemoteAddrStr->Ptr, theRemoteAddrStr->Len);
}

void RTSPSessionInterface::SaveOutputStream()
{
	Assert(fOldOutputStreamBuffer.Ptr == nullptr);
	fOldOutputStreamBuffer.Ptr = new char[fOutputStream.GetBytesWritten()];
	fOldOutputStreamBuffer.Len = fOutputStream.GetBytesWritten();
	::memcpy(fOldOutputStreamBuffer.Ptr, fOutputStream.GetBufPtr(), fOldOutputStreamBuffer.Len);
}

void RTSPSessionInterface::RevertOutputStream()
{
	Assert(fOldOutputStreamBuffer.Ptr != nullptr);
	Assert(fOldOutputStreamBuffer.Len != 0);
	static StrPtrLen theRTTStr(";rtt=", 5);

	if (fOldOutputStreamBuffer.Ptr != nullptr)
	{
		//fOutputStream.Put(fOldOutputStreamBuffer);		
		StringParser theStreamParser(&fOldOutputStreamBuffer);
		StrPtrLen theHeader;
		StrPtrLen theEOL;
		StrPtrLen theField;
		StrPtrLen theValue;
		while (theStreamParser.GetDataRemaining() != 0)
		{
			theStreamParser.ConsumeUntil(&theHeader, StringParser::sEOLMask);
			if (theHeader.Len != 0)
			{
				fOutputStream.Put(theHeader);

				StringParser theHeaderParser(&theHeader);
				theHeaderParser.ConsumeUntil(&theField, ':');
				if (theHeaderParser.PeekFast() == ':')
				{
					boost::string_view theFieldV(theField.Ptr, theField.Len);
					if (boost::iequals(theFieldV, RTSPProtocol::GetHeaderString(qtssXDynamicRateHeader)))
					{
						fOutputStream.Put(theRTTStr);
						fOutputStream.Put(fRoundTripTime);
					}
				}
			}
			theStreamParser.ConsumeEOL(&theEOL);
			fOutputStream.Put(theEOL);
		}

		fOldOutputStreamBuffer.Delete();
	}
}

void RTSPSessionInterface::SendOptionsRequest()
{
	static StrPtrLen	sOptionsRequestHeader("OPTIONS * RTSP/1.0\r\nContent-Type: application/x-random-data\r\nContent-Length: 1400\r\n\r\n");

	fOutputStream.Put(sOptionsRequestHeader);
	fOutputStream.Put((char*)(RTSPSessionInterface::sOptionsRequestBody), 1400);

	fOptionsRequestSendTime = OS::Milliseconds();
	fSentOptionsRequest = true;
	fRoundTripTimeCalculation = false;
}
