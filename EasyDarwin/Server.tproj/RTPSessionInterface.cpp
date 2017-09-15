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
	 File:       RTPSessionInterface.h

	 Contains:   Implementation of object defined in .h
 */

#include <memory>
#include <random>
#include "RTPSessionInterface.h"
#include "QTSServerInterface.h"
#include "RTSPRequestInterface.h"
#include "QTSS.h"
#include "OS.h"
#include "RTPStream.h"
#include "ServerPrefs.h"

unsigned int            RTPSessionInterface::sRTPSessionIDCounter = 0;

RTPSessionInterface::RTPSessionInterface()
	: Task(),
	// assume true until proven false!
	fTimeoutTask(nullptr, ServerPrefs::GetRTPSessionTimeoutInSecs() * 1000),
	fTracker(ServerPrefs::IsSlowStartEnabled()),
	fOverbufferWindow(ServerPrefs::GetSendIntervalInMsec(), UINT32_MAX, ServerPrefs::GetMaxSendAheadTimeInSecs(),
		ServerPrefs::GetOverbufferRate())
{
	//don't actually setup the fTimeoutTask until the session has been bound!
	//(we don't want to get timeouts before the session gets bound)

	fTimeoutTask.SetTask(this);
	fTimeout = ServerPrefs::GetRTPSessionTimeoutInSecs() * 1000;
	//fUniqueID = (uint32_t)atomic_add(&sRTPSessionIDCounter, 1);
	fUniqueID = ++sRTPSessionIDCounter;

	// fQualityUpdate is a counter the starting value is the unique ID so every session starts at a different position
	fQualityUpdate = fUniqueID;

	//mark the session create time
	fSessionCreateTime = OS::Milliseconds();
}

void RTPSessionInterface::UpdateRTSPSession(RTSPSessionInterface* inNewRTSPSession)
{
	if (inNewRTSPSession != fRTSPSession)
	{
		// If there was an old session, let it know that we are done
		if (fRTSPSession != nullptr)
			fRTSPSession->DecrementObjectHolderCount();

		// Increment this count to prevent the RTSP session from being deleted
		fRTSPSession = inNewRTSPSession;
		fRTSPSession->IncrementObjectHolderCount();
	}
}

void RTPSessionInterface::UpdateBitRateInternal(const int64_t& curTime)
{
	if (fState == qtssPausedState)
	{
		fMovieCurrentBitRate = 0;
		fLastBitRateUpdateTime = curTime;
		fLastBitRateBytes = fBytesSent;
	}
	else
	{
		uint32_t bitsInInterval = (fBytesSent - fLastBitRateBytes) * 8;
		int64_t updateTime = (curTime - fLastBitRateUpdateTime) / 1000;
		if (updateTime > 0) // leave Bit Rate the same if updateTime is 0 also don't divide by 0.
			fMovieCurrentBitRate = (uint32_t)(bitsInInterval / updateTime);
		fTracker.UpdateAckTimeout(bitsInInterval, curTime - fLastBitRateUpdateTime);
		fLastBitRateBytes = fBytesSent;
		fLastBitRateUpdateTime = curTime;
	}
	//printf("fMovieCurrentBitRate=%"   _U32BITARG_   "\n",fMovieCurrentBitRate);
	//printf("Cur bandwidth: %d. Cur ack timeout: %d.\n",fTracker.GetCurrentBandwidthInBps(), fTracker.RecommendedClientAckTimeout());
}

float RTPSessionInterface::GetPacketLossPercent()
{
	RTPStream* theStream = nullptr;
	uint32_t theLen = sizeof(theStream);

	int64_t packetsLost = 0;
	int64_t packetsSent = 0;

	for (auto theStream : fStreamBuffer)
	{
		uint32_t streamCurPacketsLost = theStream->GetPacketsLostInRTCPInterval();
		//printf("stream = %d streamCurPacketsLost = %"   _U32BITARG_   " \n",x, streamCurPacketsLost);

		uint32_t streamCurPackets = theStream->GetPacketCountInRTCPInterval();
		//printf("stream = %d streamCurPackets = %"   _U32BITARG_   " \n",x, streamCurPackets);

		packetsSent += (int64_t)streamCurPackets;
		packetsLost += (int64_t)streamCurPacketsLost;
		//printf("stream calculated loss = %f \n",x, (float) streamCurPacketsLost / (float) streamCurPackets);

	}

	//Assert(packetsLost <= packetsSent);
	if (packetsSent > 0)
	{
		if (packetsLost <= packetsSent)
			fPacketLossPercent = (float)((((float)packetsLost / (float)packetsSent) * 100.0));
		else
			fPacketLossPercent = 100.0;
	}
	else
		fPacketLossPercent = 0.0;

	return fPacketLossPercent;
}