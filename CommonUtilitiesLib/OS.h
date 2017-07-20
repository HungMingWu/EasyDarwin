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

	static int32_t Min(int32_t a, int32_t b) { if (a < b) return a; return b; }

	//
	// Milliseconds always returns milliseconds since Jan 1, 1970 GMT.
	// This basically makes it the same as a POSIX time_t value, except
	// in msec, not seconds. To convert to a time_t, divide by 1000.
	static int64_t   Milliseconds();

	static int64_t   Microseconds();

	// Some processors (MIPS, Sparc) cannot handle non word aligned memory
	// accesses. So, we need to provide functions to safely get at non-word
	// aligned memory.
	static inline uint32_t    GetUInt32FromMemory(uint32_t* inP);

	//because the OS doesn't seem to have these functions
	static int64_t   HostToNetworkSInt64(int64_t hostOrdered);
	static int64_t   NetworkToHostSInt64(int64_t networkOrdered);

	static int64_t	TimeMilli_To_Fixed64Secs(int64_t inMilliseconds); //new CISCO provided implementation
	//disable: calculates integer value only                { return (int64_t) ( (double) inMilliseconds / 1000) * ((int64_t) 1 << 32 ) ; }
	static int64_t	Fixed64Secs_To_TimeMilli(int64_t inFixed64Secs)
	{
		UInt64 value = (UInt64)inFixed64Secs; return (value >> 32) * 1000 + (((value % ((UInt64)1 << 32)) * 1000) >> 32);
	}

	//This converts the local time (from OS::Milliseconds) to NTP time.
	static int64_t	TimeMilli_To_1900Fixed64Secs(int64_t inMilliseconds)
	{
		return TimeMilli_To_Fixed64Secs(sMsecSince1900) + TimeMilli_To_Fixed64Secs(inMilliseconds);
	}

	static int64_t	TimeMilli_To_UnixTimeMilli(int64_t inMilliseconds)
	{
		return inMilliseconds;
	}

	static time_t	TimeMilli_To_UnixTimeSecs(int64_t inMilliseconds)
	{
		return (time_t)((int64_t)TimeMilli_To_UnixTimeMilli(inMilliseconds) / (int64_t)1000);
	}

	static time_t 	UnixTime_Secs(void) // Seconds since 1970
	{
		return TimeMilli_To_UnixTimeSecs(Milliseconds());
	}

	static time_t   Time1900Fixed64Secs_To_UnixTimeSecs(int64_t in1900Fixed64Secs)
	{
		return (time_t)((int64_t)((int64_t)(in1900Fixed64Secs - TimeMilli_To_Fixed64Secs(sMsecSince1900)) / ((int64_t)1 << 32)));
	}

	static int64_t   Time1900Fixed64Secs_To_TimeMilli(int64_t in1900Fixed64Secs)
	{
		return   ((int64_t)((double)((int64_t)in1900Fixed64Secs - (int64_t)TimeMilli_To_Fixed64Secs(sMsecSince1900)) / (double)((int64_t)1 << 32)) * 1000);
	}

	// Returns the offset in hours between local time and GMT (or UTC) time.
	static int32_t   GetGMTOffset();

	//Both these functions return QTSS_NoErr, QTSS_FileExists, or POSIX errorcode
	//Makes whatever directories in this path that don't exist yet 
	static OS_Error RecursiveMakeDir(char *inPath);
	//Makes the directory at the end of this path
	static OS_Error MakeDir(char *inPath);

	// Discovery of how many processors are on this machine
	static uint32_t   GetNumProcessors();

	// CPU Load
	static float  GetCurrentCPULoadPercent();

	// Mutex for StdLib calls
	static OSMutex* GetStdLibMutex() { return &sStdLibOSMutex; }

	static int64_t   InitialMSec() { return sInitialMsec; }
	static float  StartTimeMilli_Float() { return (float)((double)((int64_t)OS::Milliseconds() - (int64_t)OS::InitialMSec()) / (double) 1000.0); }
	static int64_t   StartTimeMilli_Int() { return (OS::Milliseconds() - OS::InitialMSec()); }

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

inline uint32_t OS::GetUInt32FromMemory(uint32_t* inP)
{
#if ALLOW_NON_WORD_ALIGN_ACCESS
	return *inP;
#else
	char* tempPtr = (char*)inP;
	uint32_t temp = 0;
	::memcpy(&temp, tempPtr, sizeof(uint32_t));
	return temp;
#endif
}


#endif
