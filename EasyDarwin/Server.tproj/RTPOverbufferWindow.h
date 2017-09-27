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
	 File:       RTPOverbufferWindow.h

	 Contains:   Class that tracks packets that are part of the "overbuffer". That is,
				 packets that are being sent ahead of time. This class can be used
				 to make sure the server isn't overflowing the client's overbuffer size.

	 Written By: Denis Serenyi

 */

#ifndef __RTP_OVERBUFFER_WINDOW_H__
#define __RTP_OVERBUFFER_WINDOW_H__

#include "OSHeaders.h"
#include <stdint.h>

class RTPOverbufferWindow
{
public:

	RTPOverbufferWindow(uint32_t inSendInterval, uint32_t inInitialWindowSize, uint32_t inMaxSendAheadTimeInSec,
		float inOverbufferRate);
	~RTPOverbufferWindow() = default;

	void ResetOverBufferWindow();

	//
	// ACCESSORS

	uint32_t  GetSendInterval() { return fSendInterval; }

	// This may be negative!
	int32_t  AvailableSpaceInWindow() { return fWindowSize - fBytesSentSinceLastReport; }


	//
	// The window size may be changed at any time
	void	SetWindowSize(uint32_t inWindowSizeInBytes);

	//
	// Remembers that this packet has been sent
	void AddPacketToWindow(int32_t inPacketSize);

	//
	// MarkBeginningOfWriteBurst
	// Call this on the first write of a write burst for a client. This
	// allows the overbuffer window to track whether the bitrate of the movie
	// is above the play rate.
	void MarkBeginningOfWriteBurst() { fWriteBurstBeginning = true; }

private:

	int32_t fWindowSize;
	int32_t fBytesSentSinceLastReport;
	int32_t fSendInterval;

	int32_t fBytesDuringLastSecond;
	int64_t fLastSecondStart;

	int32_t fBytesDuringPreviousSecond;
	int64_t fPreviousSecondStart;

	int32_t fBytesDuringBucket;
	int64_t fBucketBegin;
	int64_t fPreviousBucketBegin;

	int64_t fBucketTimeAhead;
	int64_t fPreviousBucketTimeAhead;

	uint32_t fMaxSendAheadTime;

	bool fWriteBurstBeginning;

	float fOverbufferRate;
	uint32_t fSendAheadDurationInMsec;

	int64_t fOverbufferWindowBegin;

};


#endif // __RTP_OVERBUFFER_TRACKER_H__


