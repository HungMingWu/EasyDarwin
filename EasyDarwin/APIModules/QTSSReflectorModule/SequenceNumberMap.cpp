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
	 File:       SequenceNumberMap.cpp

	 Contains:   Implements object defined in SequenceNumberMap.h.
 */

#include <string.h>
#include "MyAssert.h"

#include "SequenceNumberMap.h"

SequenceNumberMap::SequenceNumberMap(uint32_t inSlidingWindowSize)
	: fSlidingWindow(NULL),
	fWindowSize((int32_t)inSlidingWindowSize),
	fNegativeWindowSize((int32_t)inSlidingWindowSize - (int32_t)(2 * inSlidingWindowSize)),
	fHighestSeqIndex(0),
	fHighestSeqNumber(0)
{
	Assert(fNegativeWindowSize < 0);
	Assert(fWindowSize < 32768);//AddSequenceNumber makes this assumption

}

bool SequenceNumberMap::AddSequenceNumber(uint16_t inSeqNumber)
{
	// Returns whether sequence number has already been added.

	//Check to see if object has been initialized
	if (fSlidingWindow == NULL)
	{
		fSlidingWindow = new bool[fWindowSize + 1];
		::memset(fSlidingWindow, 0, fWindowSize * sizeof(bool));
		fHighestSeqIndex = 0;
		fHighestSeqNumber = inSeqNumber;
	}

	// First check to see if this sequence number is so far below the highest sequence number
	// we can't even put it in the sliding window.

	int16_t theWindowOffset = inSeqNumber - fHighestSeqNumber;

	if (theWindowOffset < fNegativeWindowSize)
		return false;//We don't know, but for safety, assume we haven't seen it.

	// If this seq # is higher thn the highest previous, set the highest to be this
	// new sequence number, and zero out our sliding window as we go.

	while (theWindowOffset > 0)
	{
		fHighestSeqNumber++;

		fHighestSeqIndex++;
		if (fHighestSeqIndex == fWindowSize)
			fHighestSeqIndex = 0;
		fSlidingWindow[fHighestSeqIndex] = false;

		theWindowOffset--;
	}

	// Find the right entry in the sliding window for this sequence number, taking
	// into account that we may need to wrap.

	int32_t theWindowIndex = fHighestSeqIndex + theWindowOffset;
	if (theWindowIndex < 0)
		theWindowIndex += fWindowSize;

	Assert(theWindowIndex >= 0);
	Assert(theWindowIndex < fWindowSize);

	// Turn this index on, return whether it was already turned on.
	bool alreadyAdded = fSlidingWindow[theWindowIndex];
	fSlidingWindow[theWindowIndex] = true;
#if SEQUENCENUMBERMAPTESTING
	//if (alreadyAdded)
	//  qtss_printf("Found a duplicate seq num. Num = %d\n", inSeqNumber);
#endif
	return alreadyAdded;
}

#if SEQUENCENUMBERMAPTESTING
void SequenceNumberMap::Test()
{
	SequenceNumberMap theMap1;
	bool retval = theMap1.AddSequenceNumber(64674);
	Assert(retval == false);

	retval = theMap1.AddSequenceNumber(64582);
	Assert(retval == false);

	retval = theMap1.AddSequenceNumber(64777);
	Assert(retval == false);

	retval = theMap1.AddSequenceNumber(64582);
	Assert(retval == true);

	retval = theMap1.AddSequenceNumber(64674);
	Assert(retval == true);

	retval = theMap1.AddSequenceNumber(1);
	Assert(retval == false);

	retval = theMap1.AddSequenceNumber(65500);
	Assert(retval == false);

	retval = theMap1.AddSequenceNumber(65500);
	Assert(retval == false);

	retval = theMap1.AddSequenceNumber(32768);
	Assert(retval == false);

	retval = theMap1.AddSequenceNumber(1024);
	Assert(retval == false);

	retval = theMap1.AddSequenceNumber(32757);
	Assert(retval == false);

	retval = theMap1.AddSequenceNumber(32799);
	Assert(retval == false);

	retval = theMap1.AddSequenceNumber(32768);
	Assert(retval == false);

}
#endif
