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
	 File:       UDPDemuxer.h

	 Contains:   Provides a "Listener" socket for UDP. Blocks on a local IP & port,
				 waiting for data. When it gets data, it passes it off to a UDPDemuxerTask
				 object depending on where it came from.


 */

#ifndef __UDPDEMUXER_H__
#define __UDPDEMUXER_H__

#include "OSHashTable.h"
#include "OSMutex.h"
#include "StrPtrLen.h"

class Task;
class UDPDemuxerKey;

//IMPLEMENTATION ONLY:
//HASH TABLE CLASSES USED ONLY IN IMPLEMENTATION


class UDPDemuxerUtils
{
private:

	static uint32_t ComputeHashValue(uint32_t inRemoteAddr, uint16_t inRemotePort)
	{
		return ((inRemoteAddr << 16) + inRemotePort);
	}

	friend class UDPDemuxerTask;
	friend class UDPDemuxerKey;
};

class UDPDemuxerTask
{
public:

	UDPDemuxerTask()
		: fRemoteAddr(0), fRemotePort(0),
		fHashValue(0), fNextHashEntry(NULL) {}
	virtual ~UDPDemuxerTask() {}

	uint32_t  GetRemoteAddr() { return fRemoteAddr; }

private:

	void set(uint32_t inRemoteAddr, uint16_t inRemotePort)
	{
		fRemoteAddr = inRemoteAddr; fRemotePort = inRemotePort;
		fHashValue = UDPDemuxerUtils::ComputeHashValue(fRemoteAddr, fRemotePort);
	}

	//key values
	uint32_t fRemoteAddr;
	uint16_t fRemotePort;

	//precomputed for performance
	uint32_t fHashValue;

	UDPDemuxerTask  *fNextHashEntry;

	friend class UDPDemuxerKey;
	friend class UDPDemuxer;
	friend class OSHashTable<UDPDemuxerTask, UDPDemuxerKey>;
};



class UDPDemuxerKey
{
private:

	//CONSTRUCTOR / DESTRUCTOR:
	UDPDemuxerKey(uint32_t inRemoteAddr, uint16_t inRemotePort)
		: fRemoteAddr(inRemoteAddr), fRemotePort(inRemotePort)
	{
		fHashValue = UDPDemuxerUtils::ComputeHashValue(inRemoteAddr, inRemotePort);
	}

	~UDPDemuxerKey() {}


private:

	//PRIVATE ACCESSORS:    
	uint32_t      GetHashKey() { return fHashValue; }

	//these functions are only used by the hash table itself. This constructor
	//will break the "Set" functions.
	UDPDemuxerKey(UDPDemuxerTask *elem) : fRemoteAddr(elem->fRemoteAddr),
		fRemotePort(elem->fRemotePort),
		fHashValue(elem->fHashValue) {}

	friend int operator ==(const UDPDemuxerKey &key1, const UDPDemuxerKey &key2) {
		if ((key1.fRemoteAddr == key2.fRemoteAddr) &&
			(key1.fRemotePort == key2.fRemotePort))
			return true;
		return false;
	}

	//data:
	uint32_t fRemoteAddr;
	uint16_t fRemotePort;
	uint32_t  fHashValue;

	friend class OSHashTable<UDPDemuxerTask, UDPDemuxerKey>;
	friend class UDPDemuxer;
};

//CLASSES USED ONLY IN IMPLEMENTATION
typedef OSHashTable<UDPDemuxerTask, UDPDemuxerKey> UDPDemuxerHashTable;

class UDPDemuxer
{
public:

	UDPDemuxer() : fHashTable(kMaxHashTableSize), fMutex() {}
	~UDPDemuxer() {}

	//These functions grab the mutex and are therefore premptive safe

	// Return values: OS_NoErr, or EPERM if there is already a task registered
	// with this address combination
	OS_Error RegisterTask(uint32_t inRemoteAddr, uint16_t inRemotePort,
		UDPDemuxerTask *inTaskP);

	// Return values: OS_NoErr, or EPERM if this task / address combination
	// is not registered
	OS_Error UnregisterTask(uint32_t inRemoteAddr, uint16_t inRemotePort,
		UDPDemuxerTask *inTaskP);

	//Assumes that parent has grabbed the mutex!
	UDPDemuxerTask* GetTask(uint32_t inRemoteAddr, uint16_t inRemotePort);

	bool  AddrInMap(uint32_t inRemoteAddr, uint16_t inRemotePort)
	{
		return (this->GetTask(inRemoteAddr, inRemotePort) != NULL);
	}

	OSMutex*                GetMutex() { return &fMutex; }
	UDPDemuxerHashTable*    GetHashTable() { return &fHashTable; }

private:

	enum
	{
		kMaxHashTableSize = 2747//is this prime? it should be... //uint32_t
	};
	UDPDemuxerHashTable fHashTable;
	OSMutex             fMutex;//this data structure is shared!
};

#endif // __UDPDEMUXER_H__


