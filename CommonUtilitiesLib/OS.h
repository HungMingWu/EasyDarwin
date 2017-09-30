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
	 File:       OS.h

	 Contains:   OS utility functions. Memory allocation, time, etc.



 */

#ifndef _OS_H_
#define _OS_H_

#include "OSHeaders.h"
#include "OSMutex.h"
#include <string.h>

class OS
{
public:

	//call this before calling anything else
	static void Initialize();

	//
	// Milliseconds always returns milliseconds since Jan 1, 1970 GMT.
	// This basically makes it the same as a POSIX time_t value, except
	// in msec, not seconds. To convert to a time_t, divide by 1000.
	static int64_t   Milliseconds();

	//because the OS doesn't seem to have these functions
	static int64_t   HostToNetworkSInt64(int64_t hostOrdered);
	static int64_t   NetworkToHostSInt64(int64_t networkOrdered);

	// Discovery of how many processors are on this machine
	static uint32_t   GetNumProcessors();

	static bool 	ThreadSafe();

private:
	static void setDivisor();

	static double sDivisor;
	static double sMicroDivisor;
	static int64_t sMsecSince1900;
	static int64_t sMsecSince1970;
	static int64_t sInitialMsec;
	static int32_t sMemoryErr;
	static int64_t sWrapTime;
	static int64_t sCompareWrap;
	static int64_t sLastTimeMilli;
	static OSMutex sStdLibOSMutex;
};

#endif
