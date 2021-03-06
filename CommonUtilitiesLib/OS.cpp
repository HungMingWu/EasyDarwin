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
	 File:       OS.cpp

	 Contains:   OS utility functions
 */

#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <errno.h>

#include <time.h>
#include <math.h>

#ifndef __Win32__
#include <sys/time.h>
#include "easy_gettimeofday.h"
#endif

#ifdef __sgi__ 
#include <unistd.h>
#endif

#include "OS.h"
#include "OSThread.h"
#include "MyAssert.h"

#if __MacOSX__

#ifndef __COREFOUNDATION__
#include <CoreFoundation/CoreFoundation.h>
 //extern "C" { void Microseconds (UnsignedWide *microTickCount); }
#endif

#endif


#if (__FreeBSD__ ||  __MacOSX__)
#include <sys/sysctl.h>
#endif

#if (__solaris__ || __linux__ || __linuxppc__)
#include "StringParser.h"
#endif

#if __sgi__
#include <sys/systeminfo.h>
#endif

double  OS::sDivisor = 0;
double  OS::sMicroDivisor = 0;
int64_t  OS::sMsecSince1970 = 0;
int64_t  OS::sMsecSince1900 = 0;
int64_t  OS::sInitialMsec = 0;
int64_t  OS::sWrapTime = 0;
int64_t  OS::sCompareWrap = 0;
int64_t  OS::sLastTimeMilli = 0;
OSMutex OS::sStdLibOSMutex;

#if DEBUG || __Win32__
#include "OSMutex.h"
static OSMutex* sLastMillisMutex = NULL;
#endif

void OS::Initialize()
{
	Assert(sInitialMsec == 0);  // do only once
	if (sInitialMsec != 0) return;
	::tzset();

	//setup t0 value for msec since 1900

	//t.tv_sec is number of seconds since Jan 1, 1970. Convert to seconds since 1900    
	int64_t the1900Sec = (int64_t)(24 * 60 * 60) * (int64_t)((70 * 365) + 17);
	sMsecSince1900 = the1900Sec * 1000;

	sWrapTime = (int64_t)0x00000001 << 32;
	sCompareWrap = (int64_t)0xffffffff << 32;
	sLastTimeMilli = 0;

	sInitialMsec = OS::Milliseconds(); //Milliseconds uses sInitialMsec so this assignment is valid only once.

	sMsecSince1970 = ::time(nullptr);  // POSIX time always returns seconds since 1970
	sMsecSince1970 *= 1000;         // Convert to msec


#if DEBUG || __Win32__ 
	sLastMillisMutex = new OSMutex();
#endif
}

int64_t OS::Milliseconds()
{
	/*
	#if __MacOSX__

	#if DEBUG
		OSMutexLocker locker(sLastMillisMutex);
	#endif

	   UnsignedWide theMicros;
		::Microseconds(&theMicros);
		int64_t scalarMicros = theMicros.hi;
		scalarMicros <<= 32;
		scalarMicros += theMicros.lo;
		scalarMicros = ((scalarMicros / 1000) - sInitialMsec) + sMsecSince1970; // convert to msec

	#if DEBUG
		static int64_t sLastMillis = 0;
		//Assert(scalarMicros >= sLastMillis); // currently this fails on dual processor machines
		sLastMillis = scalarMicros;
	#endif
		return scalarMicros;
	*/
#if __Win32__
	OSMutexLocker locker(sLastMillisMutex);
	// curTimeMilli = timeGetTime() + ((sLastTimeMilli/ 2^32) * 2^32)
	// using binary & to reduce it to one operation from two
	// sCompareWrap and sWrapTime are constants that are never changed
	// sLastTimeMilli is updated with the curTimeMilli after each call to this function
	int64_t curTimeMilli = (uint32_t) ::timeGetTime() + (sLastTimeMilli & sCompareWrap);
	if ((curTimeMilli - sLastTimeMilli) < 0)
	{
		curTimeMilli += sWrapTime;
	}
	sLastTimeMilli = curTimeMilli;

	// For debugging purposes
	//int64_t tempCurMsec = (curTimeMilli - sInitialMsec) + sMsecSince1970;
	//int32_t tempCurSec = tempCurMsec / 1000;
	//char buffer[kTimeStrSize];
	//printf("OS::MilliSeconds current time = %s\n", std::ctime(&tempCurSec));

	return (curTimeMilli - sInitialMsec) + sMsecSince1970; // convert to application time
#else
	struct timeval t;
#if !defined(EASY_DEVICE)
	int theErr = easy_gettimeofday(&t);
#else
	int theErr = ::gettimeofday(&t, NULL);
#endif
	Assert(theErr == 0);

	int64_t curTime;
	curTime = t.tv_sec;
	curTime *= 1000;                // sec -> msec
	curTime += t.tv_usec / 1000;    // usec -> msec

	return (curTime - sInitialMsec) + sMsecSince1970;
#endif

}

int64_t OS::HostToNetworkSInt64(int64_t hostOrdered)
{
#if BIGENDIAN
	return hostOrdered;
#else
	return (int64_t)((uint64_t)(hostOrdered << 56) | (uint64_t)(((uint64_t)0x00ff0000 << 32) & (hostOrdered << 40))
		| (uint64_t)(((uint64_t)0x0000ff00 << 32) & (hostOrdered << 24)) | (uint64_t)(((uint64_t)0x000000ff << 32) & (hostOrdered << 8))
		| (uint64_t)(((uint64_t)0x00ff0000 << 8) & (hostOrdered >> 8)) | (uint64_t)((uint64_t)0x00ff0000 & (hostOrdered >> 24))
		| (uint64_t)((uint64_t)0x0000ff00 & (hostOrdered >> 40)) | (uint64_t)((uint64_t)0x00ff & (hostOrdered >> 56)));
#endif
}

int64_t OS::NetworkToHostSInt64(int64_t networkOrdered)
{
#if BIGENDIAN
	return networkOrdered;
#else
	return (int64_t)((uint64_t)(networkOrdered << 56) | (uint64_t)(((uint64_t)0x00ff0000 << 32) & (networkOrdered << 40))
		| (uint64_t)(((uint64_t)0x0000ff00 << 32) & (networkOrdered << 24)) | (uint64_t)(((uint64_t)0x000000ff << 32) & (networkOrdered << 8))
		| (uint64_t)(((uint64_t)0x00ff0000 << 8) & (networkOrdered >> 8)) | (uint64_t)((uint64_t)0x00ff0000 & (networkOrdered >> 24))
		| (uint64_t)((uint64_t)0x0000ff00 & (networkOrdered >> 40)) | (uint64_t)((uint64_t)0x00ff & (networkOrdered >> 56)));
#endif
}

bool OS::ThreadSafe()
{

#if (__MacOSX__) // check for version 7 or greater for thread safe stdlib
	char releaseStr[32] = "";
	size_t strLen = sizeof(releaseStr);
	int mib[2];
	mib[0] = CTL_KERN;
	mib[1] = KERN_OSRELEASE;

	uint32_t majorVers = 0;
	int err = sysctl(mib, 2, releaseStr, &strLen, NULL, 0);
	if (err == 0)
	{
		StrPtrLen rStr(releaseStr, strLen);
		char* endMajor = rStr.FindString(".");
		if (endMajor != NULL) // truncate to an int value.
			*endMajor = 0;

		if (::strlen(releaseStr) > 0) //convert to an int
			::sscanf(releaseStr, "%"   _U32BITARG_   "", &majorVers);
	}
	if (majorVers < 7) // less than OS X Panther 10.3 
		return false; // force 1 worker thread because < 10.3 means std c lib is not thread safe.

#endif

	return true;

}


uint32_t OS::GetNumProcessors()
{
#if (__Win32__)
	SYSTEM_INFO theSystemInfo;
	::GetSystemInfo(&theSystemInfo);

	return (uint32_t)theSystemInfo.dwNumberOfProcessors;
#endif

#if (__MacOSX__ || __FreeBSD__)
	int numCPUs = 1;
	size_t len = sizeof(numCPUs);
	int mib[2];
	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	(void) ::sysctl(mib, 2, &numCPUs, &len, NULL, 0);
	if (numCPUs < 1)
		numCPUs = 1;
	return (uint32_t)numCPUs;
#endif

#if(__linux__ || __linuxppc__)

	char cpuBuffer[8192] = "";
	StrPtrLen cpuInfoBuf(cpuBuffer, sizeof(cpuBuffer));
	FILE    *cpuFile = ::fopen("/proc/cpuinfo", "r");
	if (cpuFile)
	{
		cpuInfoBuf.Len = ::fread(cpuInfoBuf.Ptr, sizeof(char), cpuInfoBuf.Len, cpuFile);
		::fclose(cpuFile);
	}

	StringParser cpuInfoFileParser(&cpuInfoBuf);
	StrPtrLen line;
	StrPtrLen word;
	uint32_t numCPUs = 0;

	while (cpuInfoFileParser.GetDataRemaining() != 0)
	{
		cpuInfoFileParser.GetThruEOL(&line);    // Read each line   
		StringParser lineParser(&line);
		lineParser.ConsumeWhitespace();         //skip over leading whitespace

		if (lineParser.GetDataRemaining() == 0) // must be an empty line
			continue;

		lineParser.ConsumeUntilWhitespace(&word);

		if (word.Equal("processor")) // found a processor as first word in line
		{
			numCPUs++;
		}
	}

	if (numCPUs == 0)
		numCPUs = 1;

	return numCPUs;
#endif

#if(__solaris__)
	{
		uint32_t numCPUs = 0;
		char linebuff[512] = "";
		StrPtrLen line(linebuff, sizeof(linebuff));
		StrPtrLen word;

		FILE *p = ::popen("uname -X", "r");
		while ((::fgets(linebuff, sizeof(linebuff - 1), p)) > 0)
		{
			StringParser lineParser(&line);
			lineParser.ConsumeWhitespace(); //skip over leading whitespace

			if (lineParser.GetDataRemaining() == 0) // must be an empty line
				continue;

			lineParser.ConsumeUntilWhitespace(&word);

			if (word.Equal("NumCPU")) // found a tag as first word in line
			{
				lineParser.GetThru(NULL, '=');
				lineParser.ConsumeWhitespace();  //skip over leading whitespace
				lineParser.ConsumeUntilWhitespace(&word); //read the number of cpus
				if (word.Len > 0)
					::sscanf(word.Ptr, "%"   _U32BITARG_   "", &numCPUs);

				break;
			}
		}
		if (numCPUs == 0)
			numCPUs = 1;

		::pclose(p);

		return numCPUs;
	}
#endif

#if(__sgi__) 
	uint32_t numCPUs = 0;

	numCPUs = sysconf(_SC_NPROC_ONLN);

	return numCPUs;
#endif		


	return 1;
}