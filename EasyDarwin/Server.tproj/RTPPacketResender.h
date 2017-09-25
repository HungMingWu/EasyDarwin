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
	 File:       RTPPacketResender.h

	 Contains:   RTPPacketResender class to buffer and track re-transmits of RTP packets.

	 the ctor copies the packet data, sets a timer for the packet's age limit and
	 another timer for it's possible re-transmission.
	 A duration timer is started to measure the RTT based on the client's ack.

 */

#ifndef __RTP_PACKET_RESENDER_H__
#define __RTP_PACKET_RESENDER_H__

#include <vector>
#include <atomic>

#include "RTPBandwidthTracker.h"
#include "UDPSocket.h"
#include "OSMutex.h"

class MyAckListLog;

class RTPResenderEntry
{
public:

	std::vector<char>     fPacket;
	int64_t              fExpireTime;
	int64_t              fAddedTime;
	int64_t              fOrigRetransTimeout;
	uint32_t              fNumResends;
	uint16_t              fSeqNum;
};


class RTPPacketResender
{
public:

	RTPPacketResender();
	~RTPPacketResender();

	//
	// These must be called before using the object
	void                SetDestination(UDPSocket* inOutputSocket, uint32_t inDestAddr, uint16_t inDestPort);
	void                SetBandwidthTracker(RTPBandwidthTracker* inTracker) { fBandwidthTracker = inTracker; }

	//
	// AddPacket adds a new packet to the resend queue. This will not send the packet.
	// AddPacket itself is not thread safe.
	void                AddPacket(void * rtpPacket, uint32_t packetSize, int32_t ageLimitInMsec);

	//
	// Acks a packet. Also not thread safe.
	void                AckPacket(uint16_t sequenceNumber, int64_t& inCurTimeInMsec);

	//
	// Resends outstanding packets in the queue. Guess what. Not thread safe.
	void                ResendDueEntries();

	//
	// Clear outstanding packets - if we no longer care about any of the
	// outstanding, unacked packets
	void                ClearOutstandingPackets();

	//
	// ACCESSORS
	bool              IsFlowControlled() { return fBandwidthTracker->IsFlowControlled(); }
	int32_t              GetMaxPacketsInList() { return fMaxPacketsInList; }
	int32_t              GetNumPacketsInList() { return fPacketsInList; }
	int32_t              GetNumResends() { return fNumResends; }

	static uint32_t       GetWastedBufferBytes() { return sNumWastedBytes; }

	void                SetLog(StrPtrLen * /*logname*/) {}

private:

	// Tracking the capacity of the network
	RTPBandwidthTracker* fBandwidthTracker{nullptr};

	// Who to send to
	UDPSocket*          fSocket{nullptr};
	uint32_t              fDestAddr{0};
	uint16_t              fDestPort{0};

	uint32_t              fMaxPacketsInList{0};
	uint32_t              fPacketsInList{0};
	uint32_t              fNumResends{0};                // how many total retransmitted packets
	uint32_t              fNumExpired{0};                // how many total packets dropped
	uint32_t              fNumAcksForMissingPackets{0};  // how many acks received in the case where the packet was not in the list
	uint32_t              fNumSent{0};                   // how many packets sent

	std::vector<RTPResenderEntry>   fPacketArray;
	uint16_t              fStartSeqNum;
	uint32_t              fPacketArrayMask{0};
	uint16_t              fHighestSeqNum{0};
	uint32_t              fLastUsed{0};
	OSMutex             fPacketQMutex;

	RTPResenderEntry*   GetEntryByIndex(uint16_t inIndex);
	RTPResenderEntry*   GetEntryBySeqNum(uint16_t inSeqNum);

	RTPResenderEntry*   GetEmptyEntry(uint16_t inSeqNum, uint32_t inPacketSize);
	void ReallocatePacketArray();
	void RemovePacket(uint32_t packetIndex, bool reuse = true);
	void RemovePacket(RTPResenderEntry* inEntry);

	static std::atomic_size_t sNumWastedBytes;

	void            UpdateCongestionWindow(int32_t bytesToOpenBy);
};

#endif //__RTP_PACKET_RESENDER_H__
