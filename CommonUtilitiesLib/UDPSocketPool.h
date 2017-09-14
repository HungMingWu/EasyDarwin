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
	 File:       UDPSocketPool.h

	 Contains:   Object that creates & maintains UDP socket pairs in a pool.



 */

#ifndef __UDPSOCKETPOOL_H__
#define __UDPSOCKETPOOL_H__

#include <list>
#include <mutex>
#include "SyncUnorderMap.h"
#include "UDPSocket.h"
#include "OSMutex.h"

class RTPStream;
class UDPSocketPair
{
public:

	UDPSocketPair(UDPSocket* inSocketA, UDPSocket* inSocketB)
		: fSocketA(inSocketA), fSocketB(inSocketB), fRefCount(0) {
	}
	~UDPSocketPair() = default;

	UDPSocket*  GetSocketA() { return fSocketA; }
	UDPSocket*  GetSocketB() { return fSocketB; }
	SyncUnorderMap<RTPStream*>& GetSocketADemux() { return socketADemux; }
	SyncUnorderMap<RTPStream*>& GetSocketBDemux() { return socketBDemux; }
private:
	SyncUnorderMap<RTPStream*> socketADemux;
	SyncUnorderMap<RTPStream*> socketBDemux;
	UDPSocket*  fSocketA;
	UDPSocket*  fSocketB;
	uint32_t      fRefCount;

	friend class UDPSocketPool;
};

template <typename T>
class SocketPair
{
public:

	SocketPair() = default;
	~SocketPair() = default;

	std::unique_ptr<T>& GetSocketA() { return fSocketA; }
	std::unique_ptr<T>& GetSocketB() { return fSocketB; }
	SyncUnorderMap<RTPStream*>& GetSocketADemux() { return socketADemux; }
	SyncUnorderMap<RTPStream*>& GetSocketBDemux() { return socketBDemux; }
	uint32_t      fRefCount{ 0 };
private:
	SyncUnorderMap<RTPStream*> socketADemux;
	SyncUnorderMap<RTPStream*> socketBDemux;
	std::unique_ptr<T>  fSocketA{ std::make_unique<T>() };
	std::unique_ptr<T>  fSocketB{ std::make_unique<T>() };
};

class UDPSocketPool
{
public:

	UDPSocketPool() : fMutex() {}
	virtual ~UDPSocketPool() = default;

	//Skanky access to member data
	OSMutex*    GetMutex() { return &fMutex; }
	const std::list<UDPSocketPair*>&    GetSocketQueue() { return fUDPQueue; }

	//Gets a UDP socket out of the pool. 
	//inIPAddr = IP address you'd like this pair to be bound to.
	//inPort = port you'd like this pair to be bound to, or 0 if you don't care
	//inSrcIPAddr = srcIP address of incoming packets for the demuxer.
	//inSrcPort = src port of incoming packets for the demuxer.
	//This may return NULL if no pair is available that meets the criteria.
	UDPSocketPair*  GetUDPSocketPair(uint32_t inIPAddr, uint16_t inPort,
		uint32_t inSrcIPAddr, uint16_t inSrcPort);

	//When done using a UDP socket pair retrieved via GetUDPSocketPair, you must
	//call this function. Doing so tells the pool which UDP sockets are in use,
	//keeping the number of UDP sockets allocated at a minimum.
	void ReleaseUDPSocketPair(UDPSocketPair* inPair);

	UDPSocketPair*  CreateUDPSocketPair(uint32_t inAddr, uint16_t inPort);

protected:

	//Because UDPSocket is a base class, and this pool class is intended to be
	//a general purpose class for all types of UDP sockets (reflector, standard),
	//there must be a virtual fuction for actually constructing the derived UDP sockets
	virtual UDPSocketPair*  ConstructUDPSocketPair() = 0;
	virtual void            DestructUDPSocketPair(UDPSocketPair* inPair) = 0;

	virtual void            SetUDPSocketOptions(UDPSocketPair* /*inPair*/) {}

private:

	enum
	{
		kLowestUDPPort = 6970,  //uint16_t
		kHighestUDPPort = 65535 //uint16_t
	};

	std::list<UDPSocketPair*> fUDPQueue;
	OSMutex fMutex;
};

#endif // __UDPSOCKETPOOL_H__

