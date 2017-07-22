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
	 File:       SocketUtils.h

	 Contains:   Some static routines for dealing with networking


 */

#ifndef __SOCKETUTILS_H__
#define __SOCKETUTILS_H__

#ifndef __Win32__
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#endif

#include "OSHeaders.h"
#include "MyAssert.h"
#include "StrPtrLen.h"
#include "OSMutex.h"

#ifdef __solaris__
#ifndef INADDR_NONE
#define INADDR_NONE     0xffffffff      /* -1 return from inet_addr */
#endif
#endif

class SocketUtils
{
public:

	// Call initialize before using any socket functions.
	// (pass true for lookupDNSName if you want the hostname
	// looked up via DNS during initialization -- %%sfu)
	static void Initialize(bool lookupDNSName = true);

	//static utility routines
	static bool   IsMulticastIPAddr(uint32_t inAddress);
	static bool   IsLocalIPAddr(uint32_t inAddress);

	//This function converts an integer IP address to a dotted-decimal string.
	//This function is NOT THREAD SAFE!!!
	static void ConvertAddrToString(const struct in_addr& theAddr, StrPtrLen* outAddr);

	// This function converts a dotted-decimal string IP address to a uint32_t
	static uint32_t ConvertStringToAddr(const char* inAddr);

	//You can get at all the IP addrs and DNS names on this machine this way
	static uint32_t       GetNumIPAddrs() { return sNumIPAddrs; }
	static inline uint32_t        GetIPAddr(uint32_t inAddrIndex);
	static inline StrPtrLen*    GetIPAddrStr(uint32_t inAddrIndex);
	static inline StrPtrLen*    GetDNSNameStr(uint32_t inDNSIndex);

private:

	//Utility function used by Initialize
#ifndef __Win32__
	static bool IncrementIfReqIter(char** inIfReqIter, ifreq* ifr);
#endif
	//For storing relevent information about each IP interface
	struct IPAddrInfo
	{
		uint32_t      fIPAddr;
		StrPtrLen   fIPAddrStr;
		StrPtrLen   fDNSNameStr;
	};

	static IPAddrInfo*              sIPAddrInfoArray;
	static uint32_t                   sNumIPAddrs;
	static OSMutex                  sMutex;
};

inline uint32_t SocketUtils::GetIPAddr(uint32_t inAddrIndex)
{
	Assert(sIPAddrInfoArray != nullptr);
	Assert(inAddrIndex < sNumIPAddrs);
	return sIPAddrInfoArray[inAddrIndex].fIPAddr;
}

inline StrPtrLen* SocketUtils::GetIPAddrStr(uint32_t inAddrIndex)
{
	Assert(sIPAddrInfoArray != nullptr);
	Assert(inAddrIndex < sNumIPAddrs);
	return &sIPAddrInfoArray[inAddrIndex].fIPAddrStr;
}

inline StrPtrLen* SocketUtils::GetDNSNameStr(uint32_t inDNSIndex)
{
	Assert(sIPAddrInfoArray != nullptr);
	Assert(inDNSIndex < sNumIPAddrs);
	return &sIPAddrInfoArray[inDNSIndex].fDNSNameStr;
}

#endif // __SOCKETUTILS_H__

