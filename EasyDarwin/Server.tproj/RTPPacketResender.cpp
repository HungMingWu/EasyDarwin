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
	 File:       RTPPacketResender.cpp

	 Contains:   RTPPacketResender class to buffer and track re-transmits of RTP packets.


 */

#include <stdio.h>

#include "RTPPacketResender.h"
#include "RTPStream.h"
#include "OSMutex.h"

static const uint32_t kPacketArrayIncreaseInterval = 32;// must be multiple of 2
static const uint32_t kInitialPacketArraySize = 64;// must be multiple of kPacketArrayIncreaseInterval (Turns out this is as big as we typically need)
//static const uint32_t kMaxPacketArraySize = 512;// must be multiple of kPacketArrayIncreaseInterval it would have to be a 3 mbit or more

static const uint32_t kMaxDataBufferSize = 1600;
std::atomic_size_t  RTPPacketResender::sNumWastedBytes{ 0 };

RTPPacketResender::RTPPacketResender()
	: fPacketArray(kInitialPacketArraySize),
	fPacketQMutex()
{
}

RTPPacketResender::~RTPPacketResender()
{
	for (const auto & packet : fPacketArray)
	{
		if (packet.fPacket.size() > 0)
			sNumWastedBytes -= kMaxDataBufferSize - packet.fPacket.size();
	}
}

void RTPPacketResender::SetDestination(UDPSocket* inOutputSocket, uint32_t inDestAddr, uint16_t inDestPort)
{
	fSocket = inOutputSocket;
	fDestAddr = inDestAddr;
	fDestPort = inDestPort;
}

RTPResenderEntry*   RTPPacketResender::GetEmptyEntry(uint16_t inSeqNum, uint32_t inPacketSize)
{

	RTPResenderEntry* theEntry = nullptr;

	for (uint32_t packetIndex = 0; packetIndex < fPacketsInList; packetIndex++) // see if packet is already in the array
	{
		if (inSeqNum == fPacketArray[packetIndex].fSeqNum)
		{
			return nullptr;
		}
	}

	if (fPacketsInList == fPacketArray.size()) // allocate a new array
		fPacketArray.resize(fPacketArray.size() + kPacketArrayIncreaseInterval);

	if (fPacketsInList < fPacketArray.size()) // have an open spot
	{
		theEntry = &fPacketArray[fPacketsInList];
		fPacketsInList++;

		if (fPacketsInList < fPacketArray.size())
			fLastUsed = fPacketsInList;
		else
			fLastUsed = fPacketArray.size();
	}
	else
	{
		// nothing open so re-use 
		if (fLastUsed < fPacketArray.size() - 1)
			fLastUsed++;
		else
			fLastUsed = 0;

		//printf("array is full = %"   _U32BITARG_   " reusing index=%"   _U32BITARG_   "\n",fPacketsInList,fLastUsed); 
		theEntry = &fPacketArray[fLastUsed];
		RemovePacket(fLastUsed, false); // delete packet in place don't fill we will use the spot
	}

	//
	// Check to see if this packet is too big for the buffer. If it is, then
	// we need to specially allocate a special buffer
	theEntry->fPacket.resize(inPacketSize);

	return theEntry;
}


void RTPPacketResender::ClearOutstandingPackets()
{
	//OSMutexLocker packetQLocker(&fPacketQMutex);
	//for (uint16_t packetIndex = 0; packetIndex < fPacketArraySize; packetIndex++) //Testing purposes
	for (uint16_t packetIndex = 0; packetIndex < fPacketsInList; packetIndex++)
	{
		this->RemovePacket(packetIndex, false);// don't move packets delete in place
		Assert(fPacketArray[packetIndex].fPacket.size() == 0);
	}
	if (fBandwidthTracker != nullptr)
		fBandwidthTracker->EmptyWindow(fBandwidthTracker->BytesInList()); //clean it out
	fPacketsInList = 0; // deleting in place doesn't decrement

	Assert(fPacketsInList == 0);
}

void RTPPacketResender::AddPacket(void * inRTPPacket, uint32_t packetSize, int32_t ageLimit)
{
	//OSMutexLocker packetQLocker(&fPacketQMutex);
	// the caller needs to adjust the overall age limit by reducing it
	// by the current packet lateness.

	// we compute a re-transmit timeout based on the Karns RTT esmitate

	auto* theSeqNumP = (uint16_t*)inRTPPacket;
	uint16_t theSeqNum = ntohs(theSeqNumP[1]);

	if (ageLimit > 0)
	{
		RTPResenderEntry* theEntry = this->GetEmptyEntry(theSeqNum, packetSize);

		//
		// This may happen if this sequence number has already been added.
		// That may happen if we have repeat packets in the stream.
		if (theEntry == nullptr || theEntry->fPacket.size() > 0)
			return;

		//
		// Reset all the information in the RTPResenderEntry
		theEntry->fPacket = std::vector<char>((char *)inRTPPacket, (char *)inRTPPacket + packetSize);
		theEntry->fAddedTime = OS::Milliseconds();
		theEntry->fOrigRetransTimeout = fBandwidthTracker->CurRetransmitTimeout();
		theEntry->fExpireTime = theEntry->fAddedTime + ageLimit;
		theEntry->fNumResends = 0;
		theEntry->fSeqNum = theSeqNum;

		//
		// Track the number of wasted bytes we have
		sNumWastedBytes += kMaxDataBufferSize - packetSize;

		//PLDoubleLinkedListNode<RTPResenderEntry> * listNode = new PLDoubleLinkedListNode<RTPResenderEntry>( new RTPResenderEntry(inRTPPacket, packetSize, ageLimit, fRTTEstimator.CurRetransmitTimeout() ) );
		//fAckList.AddNodeToTail(listNode);
		fBandwidthTracker->FillWindow(packetSize);
	}
	else
	{
		fNumExpired++;
	}
	fNumSent++;
}

void RTPPacketResender::AckPacket(uint16_t inSeqNum, int64_t& inCurTimeInMsec)
{
	//OSMutexLocker packetQLocker(&fPacketQMutex);

	int32_t foundIndex = -1;
	for (uint32_t packetIndex = 0; packetIndex < fPacketsInList; packetIndex++)
	{
		if (inSeqNum == fPacketArray[packetIndex].fSeqNum)
		{
			foundIndex = packetIndex;
			break;
		}
	}

	RTPResenderEntry* theEntry = nullptr;
	if (foundIndex != -1)
		theEntry = &fPacketArray[foundIndex];


	if (theEntry == nullptr || theEntry->fPacket.size() == 0)
	{
		/*
			we got an ack for a packet that has already expired or
			for a packet whose re-transmit crossed with it's original ack
		*/
		fNumAcksForMissingPackets++;
		//printf("Ack for missing packet: %d\n", inSeqNum);

		 // hmm.. we -should not have- closed down the window in this case
		 // so reopen it a bit as we normally would.
		 // ?? who know's what it really was, just use kMaximumSegmentSize
		fBandwidthTracker->EmptyWindow(RTPBandwidthTracker::kMaximumSegmentSize, false);

		// when we don't add an estimate from re-transmitted segments we're actually *underestimating* 
		// both the variation and srtt since we're throwing away ALL estimates above the current RTO!
		// therefore it's difficult for us to rapidly adapt to increases in RTT, as well as RTT that
		// are higher than our original RTO estimate.

		// for duplicate acks, use 1.5x the cur RTO as the RTT sample
		//fRTTEstimator.AddToEstimate( fRTTEstimator.CurRetransmitTimeout() * 3 / 2 );
		/// this results in some very very big RTO's since the dupes come in batches of maybe 10 or more!

//      printf("Got ack for expired packet %d\n", inSeqNum);
	}
	else
	{

		fBandwidthTracker->EmptyWindow(theEntry->fPacket.size());
		if (theEntry->fNumResends == 0)
		{
			// add RTT sample...        
			// only use rtt from packets acked after their initial send, do not use
			// estimates gatherered from re-trasnmitted packets.
			//fRTTEstimator.AddToEstimate( theEntry->fPacketRTTDuration.DurationInMilliseconds() );
			fBandwidthTracker->AddToRTTEstimate((int32_t)(inCurTimeInMsec - theEntry->fAddedTime));

			//          printf("Got ack for packet %d RTT = %qd\n", inSeqNum, inCurTimeInMsec - theEntry->fAddedTime);
		}

		this->RemovePacket(foundIndex);
	}
}

void RTPPacketResender::RemovePacket(uint32_t packetIndex, bool reuseIndex)
{
	//OSMutexLocker packetQLocker(&fPacketQMutex);

	Assert(packetIndex < fPacketArray.size());
	if (packetIndex >= fPacketArray.size())
		return;

	if (fPacketsInList == 0)
		return;

	RTPResenderEntry* theEntry = &fPacketArray[packetIndex];
	if (theEntry->fPacket.size() == 0)
		return;

	//
	// Track the number of wasted bytes we have
	sNumWastedBytes -= kMaxDataBufferSize - theEntry->fPacket.size();

	Assert(theEntry->fPacket.size() > 0);

	//
	// Update our list information
	Assert(fPacketsInList > 0);

	theEntry->fPacket.clear();

	if (reuseIndex) // we are re-using the space so keep array contiguous
	{
		fPacketArray[packetIndex] = fPacketArray[fPacketsInList - 1];
		::memset(&fPacketArray[fPacketsInList - 1], 0, sizeof(RTPResenderEntry));
		fPacketsInList--;

	}
	else    // the array is full
	{
		fBandwidthTracker->EmptyWindow(theEntry->fPacket.size(), false); // keep window available
		::memset(theEntry, 0, sizeof(RTPResenderEntry));
	}

}

void RTPPacketResender::ResendDueEntries()
{
	if (fPacketsInList <= 0)
		return;

	//OSMutexLocker packetQLocker(&fPacketQMutex);
	//
	int32_t numResends = 0;
	RTPResenderEntry* theEntry = nullptr;
	int64_t curTime = OS::Milliseconds();
	for (int32_t packetIndex = fPacketsInList - 1; packetIndex >= 0; packetIndex--) // walk backwards because remove packet moves array members forward
	{
		theEntry = &fPacketArray[packetIndex];

		if (theEntry->fPacket.empty())
			continue;

		if ((curTime - theEntry->fAddedTime) > fBandwidthTracker->CurRetransmitTimeout())
		{
			// Change:  Only expire packets after they were due to be resent. This gives the client
			// a chance to ack them and improves congestion avoidance and RTT calculation
			if (curTime > theEntry->fExpireTime)
			{
				//
				// This packet is expired
				fNumExpired++;
				//printf("Packet expired: %d\n", ((uint16_t*)thePacket)[1]);
				fBandwidthTracker->EmptyWindow(theEntry->fPacket.size());
				this->RemovePacket(packetIndex);
				//              printf("Expired packet %d\n", theEntry->fSeqNum);
				continue;
			}

			// Resend this packet
			fSocket->SendTo(fDestAddr, fDestPort, theEntry->fPacket.data(), theEntry->fPacket.size());
			//printf("Packet resent: %d\n", ((uint16_t*)theEntry->fPacketData)[1]);

			theEntry->fNumResends++;
			fNumResends++;

			numResends++;
			//printf("resend loop numResends=%" _S32BITARG_ " packet theEntry->fNumResends=%" _S32BITARG_ " stream fNumResends=\n",numResends,theEntry->fNumResends++, fNumResends);

			// ok -- lets try this.. add 1.5x of the INITIAL duration since the last send to the rto estimator
			// since we won't get an ack on this packet
			// this should keep us from exponentially increasing due o a one time increase
			// in the actuall rtt, only AddToEstimate on the first resend ( assume that it's a dupe )
			// if it's not a dupe, but rather an actual loss, the subseqnuent actuals wil bring down the average quickly

			if (theEntry->fNumResends == 1)
				fBandwidthTracker->AddToRTTEstimate((int32_t)((theEntry->fOrigRetransTimeout * 3) / 2));

			//          printf("Retransmitted packet %d\n", theEntry->fSeqNum);
			theEntry->fAddedTime = curTime;
			fBandwidthTracker->AdjustWindowForRetransmit();
			continue;
		}

	}
}
void RTPPacketResender::RemovePacket(RTPResenderEntry* inEntry) { Assert(0); }
