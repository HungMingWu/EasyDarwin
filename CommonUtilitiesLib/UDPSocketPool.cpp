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
	 File:       UDPSocketPool.cpp

	 Contains:   Object that creates & maintains UDP socket pairs in a pool.


 */

#include <algorithm>
#include "UDPSocketPool.h"

UDPSocketPair* UDPSocketPool::GetUDPSocketPair(uint32_t inIPAddr, uint16_t inPort,
	uint32_t inSrcIPAddr, uint16_t inSrcPort)
{
	OSMutexLocker locker(&fMutex);
	if ((inSrcIPAddr != 0) || (inSrcPort != 0))
	{
		for (const auto &theElem : fUDPQueue)
		{
			//If we find a pair that is a) on the right IP address, and b) doesn't
			//have this source IP & port in the demuxer already, we can return this pair
			if ((theElem->fSocketA->GetLocalAddr() == inIPAddr) &&
				((inPort == 0) || (theElem->fSocketA->GetLocalPort() == inPort)))
			{
				//check to make sure this source IP & port is not already in the demuxer.
				//If not, we can return this socket pair.
				if (((!theElem->GetSocketBDemux().AddrInMap({ 0, 0 })) &&
					(!theElem->GetSocketBDemux().AddrInMap({ inSrcIPAddr, inSrcPort }))))
				{
					theElem->fRefCount++;
					return theElem;
				}
				//If port is specified, there is NO WAY a socket pair can exist that matches
				//the criteria (because caller wants a specific ip & port combination)
				else if (inPort != 0)
					return nullptr;
			}
		}
	}
	//if we get here, there is no pair already in the pool that matches the specified
	//criteria, so we have to create a new pair.
	return this->CreateUDPSocketPair(inIPAddr, inPort);
}

void UDPSocketPool::ReleaseUDPSocketPair(UDPSocketPair* inPair)
{
	OSMutexLocker locker(&fMutex);
	inPair->fRefCount--;
	if (inPair->fRefCount == 0)
	{
		auto it = std::find(fUDPQueue.begin(), fUDPQueue.end(), inPair);
		if (it != fUDPQueue.end())
			fUDPQueue.erase(it);
		this->DestructUDPSocketPair(inPair);
	}
}

UDPSocketPair*  UDPSocketPool::CreateUDPSocketPair(uint32_t inAddr, uint16_t inPort)
{
	//try to find an open pair of ports to bind these suckers tooo
	OSMutexLocker locker(&fMutex);
	UDPSocketPair* theElem = nullptr;
	bool foundPair = false;
	uint16_t curPort = kLowestUDPPort;
	uint16_t stopPort = kHighestUDPPort - 1; // prevent roll over when iterating over port nums
	uint16_t socketBPort = kLowestUDPPort + 1;

	//If port is 0, then the caller doesn't care what port # we bind this socket to.
	//Otherwise, ONLY attempt to bind this socket to the specified port
	if (inPort != 0)
		curPort = inPort;
	if (inPort != 0)
		stopPort = inPort;


	while ((!foundPair) && (curPort < kHighestUDPPort))
	{
		socketBPort = curPort + 1; // make socket pairs adjacent to one another

		theElem = ConstructUDPSocketPair();
		Assert(theElem != nullptr);
		if (theElem->fSocketA->Open() != OS_NoErr)
		{
			this->DestructUDPSocketPair(theElem);
			return nullptr;
		}
		if (theElem->fSocketB->Open() != OS_NoErr)
		{
			this->DestructUDPSocketPair(theElem);
			return nullptr;
		}

		// Set socket options on these new sockets
		this->SetUDPSocketOptions(theElem);

		OS_Error theErr = theElem->fSocketA->Bind(inAddr, curPort);
		if (theErr == OS_NoErr)
		{   //printf("fSocketA->Bind ok on port%u\n", curPort);
			theErr = theElem->fSocketB->Bind(inAddr, socketBPort);
			if (theErr == OS_NoErr)
			{   //printf("fSocketB->Bind ok on port%u\n", socketBPort);
				foundPair = true;
				fUDPQueue.push_back(theElem);
				theElem->fRefCount++;
				return theElem;
			}
			//else printf("fSocketB->Bind failed on port%u\n", socketBPort);
		}
		//else printf("fSocketA->Bind failed on port%u\n", curPort);

	   //If we are looking to bind to a specific port set, and we couldn't then
	   //just break here.
		if (inPort != 0)
			break;

		if (curPort >= stopPort) //test for stop condition
			break;

		curPort += 2; //try a higher port pair

		this->DestructUDPSocketPair(theElem); //a bind failure
		theElem = nullptr;
	}
	//if we couldn't find a pair of sockets, make sure to clean up our mess
	if (theElem != nullptr)
		this->DestructUDPSocketPair(theElem);

	return nullptr;
}


