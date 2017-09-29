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
	 File:       RTSPRequestStream.cpp

	 Contains:   Implementation of RTSPRequestStream class.
 */


#include "RTSPRequestStream.h"
#include "StringParser.h"
#include "OS.h"

#include <errno.h>

#define READ_DEBUGGING 0

RTSPRequestStream::RTSPRequestStream(TCPSocket* sock)
	: fSocket(sock),
	fRetreatBytes(0),
	fRetreatBytesRead(0),
	fCurOffset(0),
	fRequest(fRequestBuffer, 0),
	fRequestPtr(nullptr)
{}

QTSS_Error RTSPRequestStream::ReadRequest()
{
	while (true)
	{
		uint32_t newOffset = 0;

		//If this is the case, we already HAVE a request on this session, and we now are done
		//with the request and want to move onto the next one. The first thing we should do
		//is check whether there is any lingering data in the stream. If there is, the parent
		//session believes that is part of a new request
		if (fRequestPtr != nullptr)
		{
			fRequestPtr = nullptr;//flag that we no longer have a complete request

			// Take all the retreated leftover data and move it to the beginning of the buffer
			if ((fRetreatBytes > 0) && (fRequest.Len > 0))
				::memmove(fRequest.Ptr, fRequest.Ptr + fRequest.Len + fRetreatBytesRead, fRetreatBytes);

			fCurOffset = fRetreatBytes;

			newOffset = fRequest.Len = fRetreatBytes;
			fRetreatBytes = fRetreatBytesRead = 0;
		}

		// We don't have any new data, so try and get some
		if (newOffset == 0)
		{
			if (fRetreatBytes > 0)
			{
				// This will be true if we've just snarfed another input stream, in which case the encoded data
				// is copied into our request buffer, and its length is tracked in fRetreatBytes.
				// If this is true, just fall through and decode the data.
				newOffset = fRetreatBytes;
				fRetreatBytes = 0;
			}
			else
			{
				// We don't have any new data, get some from the socket...
				QTSS_Error sockErr = fSocket->Read(&fRequestBuffer[fCurOffset],
					(kRequestBufferSizeInBytes - fCurOffset) - 1, &newOffset);
				//assume the client is dead if we get an error back
				if (sockErr == EAGAIN)
					return QTSS_NoErr;
				if (sockErr != QTSS_NoErr)
				{
					Assert(!fSocket->IsConnected());
					return sockErr;
				}
			}

			fRequest.Len += newOffset;
			Assert(fRequest.Len < kRequestBufferSizeInBytes);
			fCurOffset += newOffset;
		}
		Assert(newOffset > 0);

		// See if this is an interleaved data packet
		if ('$' == *(fRequest.Ptr))
		{
			if (fRequest.Len < 4)
				continue;
			auto* dataLenP = (uint16_t*)fRequest.Ptr;
			uint32_t interleavedPacketLen = ntohs(dataLenP[1]) + 4;
			if (interleavedPacketLen > fRequest.Len)
				continue;

			//put back any data that is not part of the header
			fRetreatBytes += fRequest.Len - interleavedPacketLen;
			fRequest.Len = interleavedPacketLen;

			fRequestPtr = &fRequest;
			fIsDataPacket = true;
			return QTSS_RequestArrived;
		}
		fIsDataPacket = false;

		//use a StringParser object to search for a double EOL, which signifies the end of
		//the header.
		bool weAreDone = false;
		StringParser headerParser(&fRequest);

		uint16_t lcount = 0;
		while (headerParser.GetThruEOL(nullptr))
		{
			lcount++;
			if (headerParser.ExpectEOL())
			{
				//The legal end-of-header sequences are \r\r, \r\n\r\n, & \n\n. NOT \r\n\r!
				//If the packets arrive just a certain way, we could get here with the latter
				//combo, and not wait for a final \n.
				if ((headerParser.GetDataParsedLen() > 2) &&
					(memcmp(headerParser.GetCurrentPosition() - 3, "\r\n\r", 3) == 0))
					continue;
				weAreDone = true;
				break;
			}
			else if (lcount == 1) {
				// if this request is actually a ShoutCast password it will be 
				// in the form of "xxxxxx\r" where "xxxxx" is the password.
				// If we get a 1st request line ending in \r with no blanks we will
				// assume that this is the end of the request.
				uint16_t flag = 0;
				uint16_t i = 0;
				for (i = 0; i < fRequest.Len; i++)
				{
					if (fRequest.Ptr[i] == ' ')
						flag++;
				}
				if (flag == 0)
				{
					weAreDone = true;
					break;
				}
			}
		}

		//weAreDone means we have gotten a full request
		if (weAreDone)
		{
			//put back any data that is not part of the header
			fRequest.Len -= headerParser.GetDataRemaining();
			fRetreatBytes += headerParser.GetDataRemaining();

			fRequestPtr = &fRequest;
			return QTSS_RequestArrived;
		}

		//check for a full buffer
		if (fCurOffset == kRequestBufferSizeInBytes - 1)
		{
			fRequestPtr = &fRequest;
			return E2BIG;
		}
	}
}

QTSS_Error RTSPRequestStream::Read(void* ioBuffer, uint32_t inBufLen, uint32_t* outLengthRead)
{
	uint32_t theLengthRead = 0;
	auto* theIoBuffer = (uint8_t*)ioBuffer;

	//
	// If there are retreat bytes available, read them first.
	if (fRetreatBytes > 0)
	{
		theLengthRead = fRetreatBytes;
		if (inBufLen < theLengthRead)
			theLengthRead = inBufLen;

		::memcpy(theIoBuffer, fRequest.Ptr + fRequest.Len + fRetreatBytesRead, theLengthRead);

		//
		// We should not update fRequest.Len even though we've read some of the retreat bytes.
		// fRequest.Len always refers to the length of the request header. Instead, we
		// have a separate variable, fRetreatBytesRead
		fRetreatBytes -= theLengthRead;
		fRetreatBytesRead += theLengthRead;
#if READ_DEBUGGING
		printf("In RTSPRequestStream::Read: Got %d Retreat Bytes\n", theLengthRead);
#endif  
	}

	//
	// If there is still space available in ioBuffer, continue. Otherwise, we can return now
	if (theLengthRead == inBufLen)
	{
		if (outLengthRead != nullptr)
			*outLengthRead = theLengthRead;
		return QTSS_NoErr;
	}

	//
	// Read data directly from the socket and place it in our buffer
	uint32_t theNewOffset = 0;
	QTSS_Error theErr = fSocket->Read(&theIoBuffer[theLengthRead], inBufLen - theLengthRead, &theNewOffset);
#if READ_DEBUGGING
	printf("In RTSPRequestStream::Read: Got %d bytes off Socket\n", theNewOffset);
#endif  
	if (outLengthRead != nullptr)
		*outLengthRead = theNewOffset + theLengthRead;

	return theErr;
}