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
	 File:       RTPOverbufferWindow.cpp

	 Contains:   Implementation of the class

	 Written By: Denis Serenyi

 */

#include "RTPOverbufferWindow.h"

RTPOverbufferWindow::RTPOverbufferWindow(uint32_t inSendInterval, uint32_t inInitialWindowSize, uint32_t inMaxSendAheadTimeInSec,
	float inOverbufferRate)
	: fWindowSize(inInitialWindowSize),
	fBytesSentSinceLastReport(0),
	fSendInterval(inSendInterval),
	fBytesDuringLastSecond(0),
	fLastSecondStart(-1),
	fBytesDuringPreviousSecond(0),
	fPreviousSecondStart(-1),
	fBytesDuringBucket(0),
	fBucketBegin(0),
	fBucketTimeAhead(0),
	fPreviousBucketTimeAhead(0),
	fMaxSendAheadTime(inMaxSendAheadTimeInSec * 1000),
	fWriteBurstBeginning(false),
	fOverbufferRate(inOverbufferRate),
	fSendAheadDurationInMsec(1000),
	fOverbufferWindowBegin(-1),
	fPreviousBucketBegin(0)
{
	if (fSendInterval == 0)
	{
		fSendInterval = 200;
	}

	if (fOverbufferRate < 1.0)
		fOverbufferRate = 1.0;

}

void RTPOverbufferWindow::ResetOverBufferWindow()
{
	fBytesDuringLastSecond = 0;
	fLastSecondStart = -1;
	fBytesDuringPreviousSecond = 0;
	fPreviousSecondStart = -1;
	fBytesDuringBucket = 0;
	fBucketBegin = 0;
	fBucketTimeAhead = 0;
	fPreviousBucketTimeAhead = 0;
	fOverbufferWindowBegin = -1;
}

void RTPOverbufferWindow::AddPacketToWindow(int32_t inPacketSize)
{
	fBytesDuringBucket += inPacketSize;
	fBytesDuringLastSecond += inPacketSize;
	fBytesSentSinceLastReport += inPacketSize;
}

void RTPOverbufferWindow::SetWindowSize(uint32_t inWindowSizeInBytes)
{
	fWindowSize = inWindowSizeInBytes;
	fBytesSentSinceLastReport = 0;
}
