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
	 File:       RTSPReflectorOutput.cpp

	 Contains:   Implementation of object in .h file


 */

#include "RTPSession.h"
#include "RTPSessionOutput.h"
#include "ReflectorStream.h"

static boost::string_view        sLastRTPPacketID = "qtssReflectorStreamLastRTPPacketID";
static boost::string_view        sLastRTCPPacketID = "qtssReflectorStreamLastRTCPPacketID";

RTPSessionOutput::RTPSessionOutput(RTPSession* inClientSession, ReflectorSession* inReflectorSession,
	boost::string_view inCookieAddrName)
	: ReflectorOutput(inReflectorSession->GetNumStreams()), // create a bookmark for each stream we'll reflect
	fClientSession(inClientSession),
	fReflectorSession(inReflectorSession),
	fCookieAttrName(inCookieAddrName)
{
}

bool RTPSessionOutput::IsPlaying()
{
	if (!fClientSession)
		return false;

	return fClientSession->GetSessionState() == qtssPlayingState;
}

bool RTPSessionOutput::IsUDP()
{
	if (fTransportInitialized)
		return fIsUDP;

	if (fClientSession->GetSessionState() != qtssPlayingState);
		return true;

	fIsUDP = true;
	fTransportInitialized = true;
	return fIsUDP;
}

bool  RTPSessionOutput::PacketAlreadySent(RTPStream *theStreamPtr, uint32_t inFlags, uint64_t* packetIDPtr)
{
	Assert(theStreamPtr);
	Assert(packetIDPtr);

	uint32_t theLen = 0;
	uint64_t *lastPacketIDPtr = nullptr;
	bool packetSent = false;

	if (inFlags & qtssWriteFlagsIsRTP)
	{
		boost::optional<boost::any> opt = theStreamPtr->getAttribute(sLastRTPPacketID);
		if (opt && *packetIDPtr <= boost::any_cast<uint64_t>(opt.value()))
		{
			//printf("RTPSessionOutput::WritePacket Don't send RTP packet id =%qu\n", *packetIDPtr);
			packetSent = true;
		}

	}
	else if (inFlags & qtssWriteFlagsIsRTCP)
	{
		boost::optional<boost::any> opt = theStreamPtr->getAttribute(sLastRTCPPacketID);
		if (opt && *packetIDPtr <= boost::any_cast<uint64_t>(opt.value()))
		{
			//printf("RTPSessionOutput::WritePacket Don't send RTP packet id =%qu\n", *packetIDPtr);
			packetSent = true;
		}
	}

	return packetSent;
}

QTSS_Error  RTPSessionOutput::WritePacket(const std::vector<char> &inPacket, void* inStreamCookie, 
	uint32_t inFlags, int64_t packetLatenessInMSec,
	uint64_t* packetIDPtr, int64_t* arrivalTimeMSecPtr, bool firstPacket)
{
	QTSS_Error              writeErr = QTSS_NoErr;
	int64_t                  currentTime = OS::Milliseconds();

	if (inPacket.empty())
		return QTSS_NoErr;

	if (fClientSession->GetSessionState() != qtssPlayingState)
		return QTSS_WouldBlock;

	//make sure all RTP streams with this ID see this packet

	for (auto theStreamPtr : fClientSession->GetStreams())
	{
		if (this->PacketMatchesStream(inStreamCookie, theStreamPtr))
		{
			if (PacketAlreadySent(theStreamPtr, inFlags, packetIDPtr))
				return QTSS_NoErr; // keep looking at packets

			// TrackPackets below is for re-writing the rtcps we don't use it right now-- shouldn't need to    
			// (void) this->TrackPackets(theStreamPtr, inPacket, &currentTime,inFlags,  &packetLatenessInMSec, timeToSendThisPacketAgain, packetIDPtr,arrivalTimeMSecPtr);

			writeErr = theStreamPtr->Write(inPacket, nullptr, inFlags);
			if (writeErr == QTSS_WouldBlock)
			{
				//printf("QTSS_Write == QTSS_WouldBlock\n");
			   //
			   // We are flow controlled. See if we know when flow control will be lifted and report that

				if (firstPacket)
				{
					fBufferDelayMSecs = (currentTime - *arrivalTimeMSecPtr);
					//printf("firstPacket fBufferDelayMSecs =%lu \n", fBufferDelayMSecs);
				}
			}
			else
			{
				fLastPacketTransmitTime = currentTime;

				if (inFlags & qtssWriteFlagsIsRTP)
				{
					theStreamPtr->addAttribute(sLastRTPPacketID, *packetIDPtr);
				}
				else if (inFlags & qtssWriteFlagsIsRTCP)
				{
					theStreamPtr->addAttribute(sLastRTCPPacketID, *packetIDPtr);
				}
			}
		}

		if (writeErr != QTSS_NoErr)
			break;
	}

	return writeErr;
}

void RTPSessionOutput::TearDown()
{
	//fClientSession->SetTeardownReason(qtssCliSesTearDownBroadcastEnded);
	fClientSession->Teardown();
}