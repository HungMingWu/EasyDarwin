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

#include <random>
#include <boost/algorithm/string/predicate.hpp>

#include "RTSPSessionInterface.h"
#include "QTSServerInterface.h"
#include "RTSPProtocol.h"
#include <errno.h>
#include "ServerPrefs.h"
#include "StringParser.h"

#if DEBUG
#define RTSP_SESSION_INTERFACE_DEBUGGING 1
#else
#define RTSP_SESSION_INTERFACE_DEBUGGING 0
#endif

unsigned int            RTSPSessionInterface::sSessionIDCounter = kFirstRTSPSessionID;

RTSPSessionInterface::RTSPSessionInterface()
	: Task(),
	fTimeoutTask(nullptr, ServerPrefs::GetRTSPSessionTimeoutInSecs() * 1000),
	fInputStream(&fSocket),
	fOutputStream(&fSocket, &fTimeoutTask),
	fSessionMutex(),
	fSocket(nullptr, Socket::kNonBlockingSocketType)
{
	fTimeoutTask.SetTask(this);
	fSocket.SetTask(this);

	//fSessionID = (uint32_t)atomic_add(&sSessionIDCounter, 1);
	fSessionID = ++sSessionIDCounter;
}

void RTSPSessionInterface::DecrementObjectHolderCount()
{
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
		fSocket.RequestEvent(EV_RE);

	return QTSS_NoErr;
}

uint8_t RTSPSessionInterface::GetTwoChannelNumbers(boost::string_view inRTSPSessionID)
{
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

	if (inLen == 0 && fTCPCoalesceBuffer.empty())
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
	if ((inLen > kTCPCoalesceDirectWriteSize || inLen == 0) && fTCPCoalesceBuffer.size() > 0
		|| (inLen + fTCPCoalesceBuffer.size() + kInteleaveHeaderSize > kTCPCoalesceBufferSize) && fTCPCoalesceBuffer.size() > 0
		)
	{
		uint32_t      buffLenWritten;

		// skip iov[0], WriteV uses it
		iov[1].iov_base = &fTCPCoalesceBuffer[0];
		iov[1].iov_len = fTCPCoalesceBuffer.size();

		err = this->GetOutputStream()->WriteV(iov, 2, fTCPCoalesceBuffer.size(), &buffLenWritten, RTSPResponseStream::kAllOrNothing);

#if RTSP_SESSION_INTERFACE_DEBUGGING 
		printf("InterleavedWrite: flushing %li\n", fTCPCoalesceBuffer.size());
#endif

		if (err == QTSS_NoErr)
			fTCPCoalesceBuffer.clear();
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

			fTCPCoalesceBuffer.push_back('$');
			fTCPCoalesceBuffer.push_back(channel);

			// if we ever turn TCPCoalesce back on, this should be optimized
			// for processors w/o alignment restrictions as above.

			int16_t  pcketLen = htons((uint16_t)inLen);
			std::copy(&pcketLen, &pcketLen + 2, std::back_inserter(fTCPCoalesceBuffer));

			std::copy((const char *)inBuffer, (const char *)inBuffer + inLen, std::back_inserter(fTCPCoalesceBuffer));

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

std::string RTSPSessionInterface::GetRemoteAddr()
{
	StrPtrLen* theRemoteAddrStr = fSocket.GetRemoteAddrStr();
	return std::string(theRemoteAddrStr->Ptr, theRemoteAddrStr->Len);
}