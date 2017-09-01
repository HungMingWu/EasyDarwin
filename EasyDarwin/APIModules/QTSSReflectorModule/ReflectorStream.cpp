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
	 File:       ReflectorStream.cpp

	 Contains:   Implementation of object defined in ReflectorStream.h.
 */

#include "RTPSession.h"
#include "ReflectorStream.h"
#include "SocketUtils.h"
#include "RTCPPacket.h"
#include "ReflectorSession.h"
#include "RTSPRequest.h"

#if DEBUG
#define REFLECTOR_STREAM_DEBUGGING 0
#else
#define REFLECTOR_STREAM_DEBUGGING 0
#endif


static ReflectorSocketPool  sSocketPool;

// PREFS
static uint32_t                   sDefaultOverBufferInSec = 1;
static uint32_t					sDefaultRTPReflectorThresholdMsec = 2000;

static uint32_t                   sDefaultBucketDelayInMsec = 73;
static bool						sDefaultUsePacketReceiveTime = false;
static uint32_t                   sDefaultMaxFuturePacketTimeSec = 60;
static uint32_t                   sDefaultFirstPacketOffsetMsec = 500;

uint32_t                          ReflectorStream::sBucketSize = 16;
uint32_t                          ReflectorStream::sOverBufferInMsec = 10000; // more or less what the client over buffer will be
uint32_t                          ReflectorStream::sMaxFuturePacketMSec = 60000; // max packet future time
uint32_t                          ReflectorStream::sMaxPacketAgeMSec = 20000;

uint32_t                          ReflectorStream::sMaxFuturePacketSec = 60; // max packet future time
uint32_t                          ReflectorStream::sOverBufferInSec = 10;
uint32_t                          ReflectorStream::sBucketDelayInMsec = 73;
bool							ReflectorStream::sUsePacketReceiveTime = false;
uint32_t                          ReflectorStream::sFirstPacketOffsetMsec = 500;

uint32_t                          ReflectorStream::sRelocatePacketAgeMSec = 1000;

void ReflectorStream::GenerateSourceID(SourceInfo::StreamInfo* inInfo, char* ioBuffer)
{

	::memcpy(ioBuffer, &inInfo->fSrcIPAddr, sizeof(inInfo->fSrcIPAddr));
	::memcpy(&ioBuffer[sizeof(inInfo->fSrcIPAddr)], &inInfo->fPort, sizeof(inInfo->fPort));
}


ReflectorStream::ReflectorStream(SourceInfo::StreamInfo* inInfo)
	: fPacketCount(0),
	fSockets(nullptr),
	fRTPSender(nullptr, qtssWriteFlagsIsRTP),
	fRTCPSender(nullptr, qtssWriteFlagsIsRTCP),
	fNumElements(0),
	fBucketMutex(),

	fDestRTCPAddr(0),
	fDestRTCPPort(0),

	fCurrentBitRate(0),
	fLastBitRateSample(OS::Milliseconds()), // don't calculate our first bit rate until kBitRateAvgIntervalInMilSecs has passed!
	fBytesSentInThisInterval(0),

	fRTPChannel(-1),
	fRTCPChannel(-1),
	fHasFirstRTCPPacket(false),
	fHasFirstRTPPacket(false),
	fEnableBuffer(false),
	fEyeCount(0),
	fFirst_RTCP_RTP_Time(0),
	fFirst_RTCP_Arrival_Time(0),
	fTransportType(qtssRTPTransportTypeUDP),
	fMyReflectorSession(nullptr),
	fStreamInfo(*inInfo)
{

	fRTPSender.fStream = this;
	fRTCPSender.fStream = this;

	// ALLOCATE BUCKET ARRAY
	this->AllocateBucketArray(kMinNumBuckets);

	// WRITE RTCP PACKET

	//write as much of the RTCP RR as is possible right now (most of it never changes)
	auto theSsrc = (uint32_t)::rand();
	char theTempCName[RTCPSRPacket::kMaxCNameLen];
	uint32_t cNameLen = RTCPSRPacket::GetACName(theTempCName);

	//write the RR (just header + ssrc)
	auto* theRRWriter = (uint32_t*)&fReceiverReportBuffer[0];
	*theRRWriter = htonl(0x80c90001);
	theRRWriter++;
	*theRRWriter = htonl(theSsrc);
	theRRWriter++;

	//SDES length is the length of the CName, plus 2 32bit words, minus 1
	*theRRWriter = htonl(0x81ca0000 + (cNameLen >> 2) + 1);
	theRRWriter++;
	*theRRWriter = htonl(theSsrc);
	theRRWriter++;
	::memcpy(theRRWriter, theTempCName, cNameLen);
	theRRWriter += cNameLen >> 2;

	//APP packet format, QTSS specific stuff
	*theRRWriter = htonl(0x80cc0008);
	theRRWriter++;
	*theRRWriter = htonl(theSsrc);
	theRRWriter++;
	*theRRWriter = htonl(FOUR_CHARS_TO_INT('Q', 'T', 'S', 'S'));
	theRRWriter++;
	*theRRWriter = htonl(0);
	theRRWriter++;
	*theRRWriter = htonl(0x00000004);
	theRRWriter++;
	*theRRWriter = htonl(0x6579000c);
	theRRWriter++;

	fEyeLocation = theRRWriter;
	fReceiverReportSize = kReceiverReportSize + kAppSize + cNameLen;

	// If the source is a multicast, we should send our receiver reports
	// to the multicast address
	if (SocketUtils::IsMulticastIPAddr(fStreamInfo.fDestIPAddr))
	{
		fDestRTCPAddr = fStreamInfo.fDestIPAddr;
		fDestRTCPPort = fStreamInfo.fPort + 1;
	}

	pkeyFrameCache = new CKeyFrameCache(MAX_CACHE_SIZE);

}


ReflectorStream::~ReflectorStream()
{
	Assert(fNumElements == 0);

	if (fSockets != nullptr)
	{
		//first things first, let's take this stream off the socket's queue
		//of streams. This will basically ensure that no reflecting activity
		//can happen on this stream.
		((ReflectorSocket*)fSockets->GetSocketA())->RemoveSender(&fRTPSender);
		((ReflectorSocket*)fSockets->GetSocketB())->RemoveSender(&fRTCPSender);

		//leave the multicast group. Because this socket is shared amongst several
		//potential multicasts, we don't want to remain a member of a stale multicast
		if (SocketUtils::IsMulticastIPAddr(fStreamInfo.fDestIPAddr))
		{
			fSockets->GetSocketA()->LeaveMulticast(fStreamInfo.fDestIPAddr);
			fSockets->GetSocketB()->LeaveMulticast(fStreamInfo.fDestIPAddr);
		}
		//now release the socket pair
		if (qtssRTPTransportTypeUDP == fTransportType)
			sSocketPool.ReleaseUDPSocketPair(fSockets);
		else if (qtssRTPTransportTypeTCP == fTransportType)
			sSocketPool.DestructUDPSocketPair(fSockets);
	}

	// 释放关键帧缓冲区
	if (pkeyFrameCache)
	{
		delete pkeyFrameCache;
		pkeyFrameCache = nullptr;
	}
}

void ReflectorStream::AllocateBucketArray(uint32_t inNumBuckets)
{
	//allocate the 2-dimensional array
	fOutputArray.resize(inNumBuckets);
	for (uint32_t x = 0; x < inNumBuckets; x++)
		fOutputArray[x].resize(sBucketSize);
}


int32_t ReflectorStream::AddOutput(ReflectorOutput* inOutput, int32_t putInThisBucket)
{
	OSMutexLocker locker(&fBucketMutex);

#if DEBUG
	// We should never be adding an output twice to a stream
	for (uint32_t dOne = 0; dOne < fNumBuckets; dOne++)
		for (uint32_t dTwo = 0; dTwo < sBucketSize; dTwo++)
			Assert(fOutputArray[dOne][dTwo] != inOutput);
#endif
	if (inOutput)
	{
		inOutput->setNewFlag(true);
	}

	// If caller didn't specify a bucket, find a bucket
	if (putInThisBucket < 0)
		putInThisBucket = this->FindBucket();

	Assert(putInThisBucket >= 0);

	if (fOutputArray.size() <= (uint32_t)putInThisBucket)
		AllocateBucketArray(putInThisBucket * 2);

	for (uint32_t y = 0; y < sBucketSize; y++)
	{
		if (fOutputArray[putInThisBucket][y] == nullptr)
		{
			fOutputArray[putInThisBucket][y] = inOutput;
#if REFLECTOR_STREAM_DEBUGGING 
			printf("Adding new output (0x%lx) to bucket %" _S32BITARG_ ", index %" _S32BITARG_ ",\nnum buckets %li bucketSize: %li \n", (int32_t)inOutput, putInThisBucket, y, (int32_t)fNumBuckets, (int32_t)sBucketSize);
#endif
			fNumElements++;
			return putInThisBucket;
		}
	}
	// There was no empty spot in the specified bucket. Return an error
	return -1;
}

int32_t ReflectorStream::FindBucket()
{
	// If we need more buckets, allocate them.
	if (fNumElements == (sBucketSize * fOutputArray.size()))
		AllocateBucketArray(fOutputArray.size() * 2);

	//find the first open spot in the array
	for (int32_t putInThisBucket = 0; (uint32_t)putInThisBucket < fOutputArray.size(); putInThisBucket++)
	{
		for (uint32_t y = 0; y < sBucketSize; y++)
			if (fOutputArray[putInThisBucket][y] == nullptr)
				return putInThisBucket;
	}
	Assert(0);
	return 0;
}

void  ReflectorStream::RemoveOutput(ReflectorOutput* inOutput)
{
	OSMutexLocker locker(&fBucketMutex);
	Assert(fNumElements > 0);

	//look at all the indexes in the array
	for (uint32_t x = 0; x < fOutputArray.size(); x++)
	{
		for (uint32_t y = 0; y < sBucketSize; y++)
		{
			//The array may have blank spaces!
			if (fOutputArray[x][y] == inOutput)
			{
				fOutputArray[x][y] = nullptr;//just clear out the pointer

#if REFLECTOR_STREAM_DEBUGGING  
				printf("Removing output %x from bucket %" _S32BITARG_ ", index %" _S32BITARG_ "\n", inOutput, x, y);
#endif
				fNumElements--;
				return;
			}
		}
	}
	Assert(0);
}

void  ReflectorStream::TearDownAllOutputs()
{

	OSMutexLocker locker(&fBucketMutex);

	//look at all the indexes in the array
	for (uint32_t x = 0; x < fOutputArray.size(); x++)
	{
		for (uint32_t y = 0; y < sBucketSize; y++)
		{
			ReflectorOutput* theOutputPtr = fOutputArray[x][y];
			//The array may have blank spaces!
			if (theOutputPtr != nullptr)
			{
				theOutputPtr->TearDown();
#if REFLECTOR_STREAM_DEBUGGING  
				printf("TearDownAllOutputs Removing output from bucket %" _S32BITARG_ ", index %" _S32BITARG_ "\n", x, y);
#endif
			}
		}
	}
}


QTSS_Error ReflectorStream::BindSockets(QTSS_StandardRTSP_Params* inParams, uint32_t inReflectorSessionFlags, bool filterState, uint32_t timeout)
{
	// If the incoming data is RTSP interleaved, we don't need to do anything here
	if (inReflectorSessionFlags & ReflectorSession::kIsPushSession)
		fStreamInfo.fSetupToReceive = true;

	RTSPRequest* inRequest = nullptr;
	if (inParams != nullptr)
		inRequest = inParams->inRTSPRequest;

	// Set the transport Type a Broadcaster
	if (inParams != nullptr)
		fTransportType = inParams->inRTSPRequest->GetTransportType();

	// get a pair of sockets. The socket must be bound on INADDR_ANY because we don't know
	// which interface has access to this broadcast. If there is a source IP address
	// specified by the source info, we can use that to demultiplex separate broadcasts on
	// the same port. If the src IP addr is 0, we cannot do this and must dedicate 1 port per
	// broadcast

	// changing INADDR_ANY to fStreamInfo.fDestIPAddr to deal with NATs (need to track this change though)
	// change submitted by denis@berlin.ccc.de

	bool isMulticastDest = (SocketUtils::IsMulticastIPAddr(fStreamInfo.fDestIPAddr));

	// Babosa修改:当RTSP TCP推送的时候,直接创建SocketPair,不经过UDPSocketPool
	if (qtssRTPTransportTypeTCP == fTransportType)
	{
		fSockets = sSocketPool.ConstructUDPSocketPair();
	}
	else
	{
		if (isMulticastDest)
		{
			fSockets = sSocketPool.GetUDPSocketPair(INADDR_ANY, fStreamInfo.fPort, fStreamInfo.fSrcIPAddr, 0);
		}
		else
		{
			fSockets = sSocketPool.GetUDPSocketPair(fStreamInfo.fDestIPAddr, fStreamInfo.fPort, fStreamInfo.fSrcIPAddr, 0);
		}

		if ((fSockets == nullptr) && fStreamInfo.fSetupToReceive)
		{
			fStreamInfo.fPort = 0;
			if (isMulticastDest)
			{
				fSockets = sSocketPool.GetUDPSocketPair(INADDR_ANY, fStreamInfo.fPort, fStreamInfo.fSrcIPAddr, 0);
			}
			else
			{
				fSockets = sSocketPool.GetUDPSocketPair(fStreamInfo.fDestIPAddr, fStreamInfo.fPort, fStreamInfo.fSrcIPAddr, 0);
			}
		}
	}

	if (fSockets == nullptr)
		return inRequest->SendErrorResponse(qtssServerInternal);

	// If we know the source IP address of this broadcast, we can demux incoming traffic
	// on the same port by that source IP address. If we don't know the source IP addr,
	// it is impossible for us to demux, and therefore we shouldn't allow multiple
	// broadcasts on the same port.
	if (((ReflectorSocket*)fSockets->GetSocketA())->HasSender() && (fStreamInfo.fSrcIPAddr == 0))
		return inRequest->SendErrorResponse(qtssServerInternal);

	//also put this stream onto the socket's queue of streams
	((ReflectorSocket*)fSockets->GetSocketA())->AddSender(&fRTPSender);
	((ReflectorSocket*)fSockets->GetSocketB())->AddSender(&fRTCPSender);

	// A broadcaster is setting up a UDP session so let the sockets update the session
	if (fStreamInfo.fSetupToReceive &&  qtssRTPTransportTypeUDP == fTransportType && inParams != nullptr)
	{
		((ReflectorSocket*)fSockets->GetSocketA())->AddBroadcasterSession(inParams->inClientSession);
		((ReflectorSocket*)fSockets->GetSocketB())->AddBroadcasterSession(inParams->inClientSession);
	}

	((ReflectorSocket*)fSockets->GetSocketA())->SetSSRCFilter(filterState, timeout);
	((ReflectorSocket*)fSockets->GetSocketB())->SetSSRCFilter(filterState, timeout);

#if 1 
	// Always set the Rcv buf size for the sockets. This is important because the
	// server is going to be getting many packets on these sockets.
	if (qtssRTPTransportTypeUDP == fTransportType)
	{
		fSockets->GetSocketA()->SetSocketRcvBufSize(1024 * 1024);
		fSockets->GetSocketB()->SetSocketRcvBufSize(1024 * 1024);
	}
#endif

	//If the broadcaster is sending RTP directly to us, we don't
	//need to join a multicast group because we're not using multicast
	if (isMulticastDest)
	{
		QTSS_Error err = fSockets->GetSocketA()->JoinMulticast(fStreamInfo.fDestIPAddr);
		if (err == QTSS_NoErr)
			err = fSockets->GetSocketB()->JoinMulticast(fStreamInfo.fDestIPAddr);
		// If we get an error when setting the TTL, this isn't too important (TTL on
		// these sockets is only useful for RTCP RRs.
		if (err == QTSS_NoErr)
			(void)fSockets->GetSocketA()->SetTtl(fStreamInfo.fTimeToLive);
		if (err == QTSS_NoErr)
			(void)fSockets->GetSocketB()->SetTtl(fStreamInfo.fTimeToLive);
		if (err != QTSS_NoErr)
			return inRequest->SendErrorResponse(qtssServerInternal);
	}

	// If the port is 0, update the port to be the actual port value
	fStreamInfo.fPort = fSockets->GetSocketA()->GetLocalPort();

	//finally, register these sockets for events
	if (qtssRTPTransportTypeUDP == fTransportType)
	{
		fSockets->GetSocketA()->RequestEvent(EV_RE);
		fSockets->GetSocketB()->RequestEvent(EV_RE);
	}

	return QTSS_NoErr;
}

void ReflectorStream::SendReceiverReport()
{
	// Check to see if our destination RTCP addr & port are setup. They may
	// not be if the source is unicast and we haven't gotten any incoming packets yet
	if (fDestRTCPAddr == 0)
		return;

	uint32_t theEyeCount = this->GetEyeCount();
	uint32_t* theEyeWriter = fEyeLocation;
	*theEyeWriter = htonl(theEyeCount) & 0x7fffffff;//no idea why we do this!
	theEyeWriter++;
	*theEyeWriter = htonl(theEyeCount) & 0x7fffffff;
	theEyeWriter++;
	*theEyeWriter = htonl(0) & 0x7fffffff;

	//send the packet to the multicast RTCP addr & port for this stream
	(void)fSockets->GetSocketB()->SendTo(fDestRTCPAddr, fDestRTCPPort, fReceiverReportBuffer, fReceiverReportSize);
}

void ReflectorStream::PushPacket(char *packet, uint32_t packetLen, bool isRTCP)
{
	auto *head = (FU_Head*)&packet[13];

	if (packetLen > 0)
	{
		ReflectorPacket* thePacket = nullptr;
		if (isRTCP)
		{
			//printf("ReflectorStream::PushPacket RTCP packetlen = %"   _U32BITARG_   "\n",packetLen);
			thePacket = ((ReflectorSocket*)fSockets->GetSocketB())->GetPacket();
			if (thePacket == nullptr)
			{
				//printf("ReflectorStream::PushPacket RTCP GetPacket() is NULL\n");
				return;
			}

			OSMutexLocker locker(((ReflectorSocket*)(fSockets->GetSocketB()))->GetDemuxer()->GetMutex());
			thePacket->SetPacketData(packet, packetLen);
			((ReflectorSocket*)fSockets->GetSocketB())->ProcessPacket(OS::Milliseconds(), thePacket, 0, 0);
			((ReflectorSocket*)fSockets->GetSocketB())->Signal(Task::kIdleEvent);
		}
		else
		{
			//printf("ReflectorStream::PushPacket RTP packetlen = %"   _U32BITARG_   "\n",packetLen);
			thePacket = ((ReflectorSocket*)fSockets->GetSocketA())->GetPacket();
			if (thePacket == nullptr)
			{
				//printf("ReflectorStream::PushPacket GetPacket() is NULL\n");
				return;
			}

			OSMutexLocker locker(((ReflectorSocket*)(fSockets->GetSocketA()))->GetDemuxer()->GetMutex());
			thePacket->SetPacketData(packet, packetLen);

			//if(this->fStreamInfo.fPayloadName.Equal("H264/90000"))
			//{
			//	if(head->nalu_type != 0)
			//	{
			//		pkeyFrameCache->PutOnePacket(packet,packetLen,head->nalu_type,head->s);
			//	}
			//}

			((ReflectorSocket*)fSockets->GetSocketA())->ProcessPacket(OS::Milliseconds(), thePacket, 0, 0);
			((ReflectorSocket*)fSockets->GetSocketA())->Signal(Task::kIdleEvent);
		}
	}
}

ReflectorSender::ReflectorSender(ReflectorStream* inStream, uint32_t inWriteFlag)
	: fStream(inStream),
	fWriteFlag(inWriteFlag),
	fKeyFrameStartPacketElementPointer(nullptr)
{
}

bool ReflectorSender::ShouldReflectNow(const int64_t& inCurrentTime, int64_t* ioWakeupTime)
{
	Assert(ioWakeupTime != nullptr);
	//check to make sure there actually is work to do for this stream.
	if ((!fHasNewPackets) && ((fNextTimeToRun == 0) || (inCurrentTime < fNextTimeToRun)))
	{
		//We don't need to do work right now, but
		//this stream must still communicate when it needs to be woken up next
		int64_t theWakeupTime = fNextTimeToRun + inCurrentTime;
		//printf("ReflectorSender::ShouldReflectNow theWakeupTime=%qd newWakeUpTime=%qd  ioWakepTime=%qd\n", theWakeupTime, fNextTimeToRun + inCurrentTime,*ioWakeupTime);
		if ((fNextTimeToRun > 0) && (theWakeupTime < *ioWakeupTime))
			*ioWakeupTime = theWakeupTime;
		return false;
	}
	return true;
}

uint32_t ReflectorSender::GetOldestPacketRTPTime(bool *foundPtr)
{
	if (foundPtr != nullptr)
		*foundPtr = false;
	OSMutexLocker locker(&fStream->fBucketMutex);
	ReflectorPacket* thePacket = GetClientBufferStartPacket();
	if (thePacket == nullptr)
		return 0;

	if (foundPtr != nullptr)
		*foundPtr = true;

	return thePacket->GetPacketRTPTime();
}

uint16_t ReflectorSender::GetFirstPacketRTPSeqNum(bool *foundPtr)
{
	if (foundPtr != nullptr)
		*foundPtr = false;

	uint16_t resultSeqNum = 0;
	OSMutexLocker locker(&fStream->fBucketMutex);
	ReflectorPacket* thePacket = GetClientBufferStartPacket();

	if (thePacket == nullptr)
		return 0;

	if (foundPtr != nullptr)
		*foundPtr = true;

	resultSeqNum = thePacket->GetPacketRTPSeqNum();

	return resultSeqNum;
}

ReflectorPacket*    ReflectorSender::GetClientBufferNextPacketTime(uint32_t inRTPTime)
{
	if (fPacketQueue.front().get() == nullptr)
		return nullptr;

	for (const auto &thePacket : fPacketQueue) // start at oldest packet in q
		if (thePacket->GetPacketRTPTime() > inRTPTime)
			return thePacket.get(); // return the first packet we have that has a later time

	return nullptr;
}

bool ReflectorSender::GetFirstRTPTimePacket(uint16_t* outSeqNumPtr, uint32_t* outRTPTimePtr, int64_t* outArrivalTimePtr)
{
	OSMutexLocker locker(&fStream->fBucketMutex);
	ReflectorPacket* thePacket = GetClientBufferStartPacketOffset(ReflectorStream::sFirstPacketOffsetMsec);

	if (thePacket == nullptr)
		return false;

	thePacket = GetClientBufferNextPacketTime(thePacket->GetPacketRTPTime());
	if (thePacket == nullptr)
		return false;

	if (outSeqNumPtr)
		*outSeqNumPtr = thePacket->GetPacketRTPSeqNum();

	if (outRTPTimePtr)
		*outRTPTimePtr = thePacket->GetPacketRTPTime();

	if (outArrivalTimePtr)
		*outArrivalTimePtr = thePacket->fTimeArrived;

	return true;
}

bool ReflectorSender::GetFirstPacketInfo(uint16_t* outSeqNumPtr, uint32_t* outRTPTimePtr, int64_t* outArrivalTimePtr)
{
	OSMutexLocker locker(&fStream->fBucketMutex);
	ReflectorPacket* thePacket = GetClientBufferStartPacketOffset(ReflectorStream::sFirstPacketOffsetMsec);

	if (thePacket == nullptr)
		return false;

	if (outSeqNumPtr)
		*outSeqNumPtr = thePacket->GetPacketRTPSeqNum();

	if (outRTPTimePtr)
		*outRTPTimePtr = thePacket->GetPacketRTPTime();

	if (outArrivalTimePtr)
		*outArrivalTimePtr = thePacket->fTimeArrived;

	thePacket->fNeededByOutput = true;

	return true;
}


#if REFLECTOR_STREAM_DEBUGGING
static uint16_t DGetPacketSeqNumber(StrPtrLen* inPacket)
{
	if (inPacket->Len < 4)
		return 0;

	//The RTP seq number is the second short of the packet
	uint16_t* seqNumPtr = (uint16_t*)inPacket->Ptr;
	return ntohs(seqNumPtr[1]);
}



#endif


void ReflectorSender::ReflectRelayPackets(int64_t* ioWakeupTime)
{
	//Most of this code is useless i.e. buckets and bookmarks. This code will get cleaned up eventually

	//printf("ReflectorSender::ReflectPackets %qd %qd\n",*ioWakeupTime,fNextTimeToRun);
#if DEBUG
	Assert(ioWakeupTime != NULL);
#endif
#if REFLECTOR_STREAM_DEBUGGING > 2
	bool	printQueueLenOnExit = false;
#endif	

	int64_t currentTime = OS::Milliseconds();

	//make sure to reset these state variables
	fHasNewPackets = false;
	fNextTimeToRun = 1000;	// init to 1 secs

	//determine if we need to send a receiver report to the multicast source
	if ((fWriteFlag == qtssWriteFlagsIsRTCP) && (currentTime > (fLastRRTime + kRRInterval)))
	{
		fLastRRTime = currentTime;
		fStream->SendReceiverReport();
#if REFLECTOR_STREAM_DEBUGGING > 2
		printQueueLenOnExit = true;
		printf("fPacketQueue len %li\n", (int32_t)fPacketQueue.GetLength());
#endif	
	}

	//the rest of this function must be atomic wrt the ReflectorSession, because
	//it involves iterating through the RTPSession array, which isn't thread safe
	OSMutexLocker locker(&fStream->fBucketMutex);

	// Check to see if we should update the session's bitrate average
	if ((fStream->fLastBitRateSample + ReflectorStream::kBitRateAvgIntervalInMilSecs) < currentTime)
	{
		unsigned int intervalBytes = fStream->fBytesSentInThisInterval;
		(void)atomic_sub(&fStream->fBytesSentInThisInterval, intervalBytes);

		// Multiply by 1000 to convert from milliseconds to seconds, and by 8 to convert from bytes to bits
		float bps = (float)(intervalBytes * 8) / (float)(currentTime - fStream->fLastBitRateSample);
		bps *= 1000;
		fStream->fCurrentBitRate = (uint32_t)bps;

		// Don't check again for awhile!
		fStream->fLastBitRateSample = currentTime;
	}

	for (uint32_t bucketIndex = 0; bucketIndex < fStream->fOutputArray.size(); bucketIndex++)
	{
		for (uint32_t bucketMemberIndex = 0; bucketMemberIndex < fStream->sBucketSize; bucketMemberIndex++)
		{
			ReflectorOutput* theOutput = fStream->fOutputArray[bucketIndex][bucketMemberIndex];


			if (theOutput != nullptr)
			{
				int32_t			availBookmarksPosition = -1;	// -1 == invalid position
				ReflectorPacket*	packetElem = nullptr;
				uint32_t			curBookmark = 0;

				Assert(curBookmark < theOutput->fBookmarkedPacketsElemsArray.size());

				// see if we've bookmarked a held packet for this Sender in this Output
				while (curBookmark < theOutput->fBookmarkedPacketsElemsArray.size())
				{
					ReflectorPacket* 	bookmarkedElem = theOutput->fBookmarkedPacketsElemsArray[curBookmark];

					if (bookmarkedElem)	// there may be holes in this array
					{
						using namespace std;
						auto it = find_if(begin(fPacketQueue), end(fPacketQueue), 
							[bookmarkedElem](const unique_ptr<ReflectorPacket> &pkt) {
							return pkt.get() == bookmarkedElem;
						});
						if (it != end(fPacketQueue))
						{
							// this packet was previously bookmarked for this specific queue
							// remove if from the bookmark list and use it
							// to jump ahead into the Sender's over all packet queue						
							theOutput->fBookmarkedPacketsElemsArray[curBookmark] = nullptr;
							availBookmarksPosition = curBookmark;
							packetElem = bookmarkedElem;
							break;
						}
					}
					else
					{
						availBookmarksPosition = curBookmark;
					}

					curBookmark++;

				}

				Assert(availBookmarksPosition != -1);

#if REFLECTOR_STREAM_DEBUGGING > 1
				if (packetElem)	// show 'em what we got johnny
				{
					ReflectorPacket* 	thePacket = (ReflectorPacket*)packetElem->GetEnclosingObject();
					printf("Bookmarked packet time: %li, packetSeq %i\n", (int32_t)thePacket->fTimeArrived, DGetPacketSeqNumber(&thePacket->fPacketPtr));
				}
#endif

				// the output did not have a bookmarked packet if it's own
				// so show it the first new packet we have in this sender.
				// ( since TCP flow control may delay the sending of packets, this may not
				// be the same as the first packet in the queue
				if (packetElem == nullptr)
				{
					packetElem = fFirstNewPacketInQueue;

#if REFLECTOR_STREAM_DEBUGGING > 1
					if (packetElem)	// show 'em what we got johnny
					{
						ReflectorPacket* 	thePacket = (ReflectorPacket*)packetElem->GetEnclosingObject();
						printf("1st new packet from Sender sess 0x%lx time: %li, packetSeq %i\n", (int32_t)theOutput, (int32_t)thePacket->fTimeArrived, DGetPacketSeqNumber(&thePacket->fPacketPtr));
					}
					else
						printf("no new packets\n");
#endif
				}

				auto it = std::find_if(begin(fPacketQueue), end(fPacketQueue),
					[packetElem](const std::unique_ptr<ReflectorPacket> &pkt) {
					return pkt.get() == packetElem;
				});

				bool			dodBookmarkPacket = false;

				for (; it != fPacketQueue.end(); ++it)
				{
					const auto &thePacket = *it;
					QTSS_Error			err = QTSS_NoErr;

#if REFLECTOR_STREAM_DEBUGGING > 2
					printf("packet time: %li, packetSeq %i\n", (int32_t)thePacket->fTimeArrived, DGetPacketSeqNumber(&thePacket->fPacketPtr));
#endif

					// once we see a packet we cant' send, we need to stop trying
					// during this pass mark remaining as still needed
					if (!dodBookmarkPacket)
					{
						int64_t  packetLateness = currentTime - thePacket->fTimeArrived - (ReflectorStream::sBucketDelayInMsec * (int64_t)bucketIndex);
						// packetLateness measures how late this packet it after being corrected for the bucket delay

#if REFLECTOR_STREAM_DEBUGGING > 2
						printf("packetLateness %li, seq# %li\n", (int32_t)packetLateness, (int32_t)DGetPacketSeqNumber(&thePacket->fPacketPtr));
#endif

						int64_t timeToSendPacket = -1;
						err = theOutput->WritePacket(thePacket->fPacket, fStream, fWriteFlag, packetLateness, &timeToSendPacket, nullptr, nullptr, false);

						if (err == QTSS_WouldBlock)
						{
#if REFLECTOR_STREAM_DEBUGGING > 2
							printf("EAGAIN bookmark: %li, packetSeq %i\n", (int32_t)packetLateness, DGetPacketSeqNumber(&thePacket->fPacketPtr));
#endif
							// tag it and bookmark it
							thePacket->fNeededByOutput = true;

							Assert(availBookmarksPosition != -1);
							if (availBookmarksPosition != -1)
								theOutput->fBookmarkedPacketsElemsArray[availBookmarksPosition] = packetElem;

							dodBookmarkPacket = true;

							// call us again in # ms to retry on an EAGAIN
							if ((timeToSendPacket > 0) && (fNextTimeToRun > timeToSendPacket))
								fNextTimeToRun = timeToSendPacket;
							if (timeToSendPacket == -1)
								this->SetNextTimeToRun(5); // keep in synch with delay on would block for on-demand lower is better for high-bit rate movies.

						}
					}
					else
					{
						if (thePacket->fNeededByOutput)	// optimization: if the packet is already marked, another Output has been through this already
							break;
						thePacket->fNeededByOutput = true;
					}
				}

			}
		}
	}

	// reset our first new packet bookmark
	fFirstNewPacketInQueue = nullptr;

	// iterate one more through the senders queue to clear out
	// the unneeded packets
	for (auto it = fPacketQueue.begin(); it != fPacketQueue.end(); )
	{
		const auto &thePacket = *it;

		//at this point, move onto the next queue element, because we may be altering
		//the queue itself in the code below

		if (thePacket->fNeededByOutput == false)
		{
			thePacket->fNeededByOutput = true;
			it = fPacketQueue.erase(it);
		}
		else	// reset for next call to ReflectPackets
		{
			thePacket->fNeededByOutput = false;
			++it;
		}
	}

	//Don't forget that the caller also wants to know when we next want to run
	if (*ioWakeupTime == 0)
		*ioWakeupTime = fNextTimeToRun;
	else if ((fNextTimeToRun > 0) && (*ioWakeupTime > fNextTimeToRun))
		*ioWakeupTime = fNextTimeToRun;
	// exit with fNextTimeToRun in real time, not relative time.
	fNextTimeToRun += currentTime;

#if REFLECTOR_STREAM_DEBUGGING > 2
	if (printQueueLenOnExit)
		printf("EXIT fPacketQueue len %li\n", (int32_t)fPacketQueue.GetLength());
#endif
}

/***********************************************************************************************
/   ReflectorSender::ReflectPackets
/
/   There are n ReflectorSender's for n output streams per presentation.
/
/   Each sender is associated with an array of ReflectorOutput's.  Each
/   output represents a client connection.  Each output has # RTPStream's.
/
/   When we write a packet to the ReflectorOutput he matches it's payload
/   to one of his streams and sends it there.
/
/   To smooth the bandwitdth (server, not user) requirements of the reflected streams, the Sender
/   groups the ReflectorOutput's into buckets.  The input streams are reflected to
/   each bucket progressively later in time.  So rather than send a single packet
/   to say 1000 clients all at once, we send it to just the first 16, then then next 16
/   100 ms later and so on.
/
/
/   intputs     ioWakeupTime - relative time to call us again in MSec
/               inFreeQueue - queue of free packets.
*/

void ReflectorSender::ReflectPackets(int64_t* ioWakeupTime)
{
	if (!fStream->BufferEnabled()) // Call old routine for relays; they don't want buffering.
	{
		this->ReflectRelayPackets(ioWakeupTime);
		return;
	}

	int64_t currentTime = OS::Milliseconds();

	//make sure to reset these state variables
	fHasNewPackets = false;

	fNextTimeToRun = 1000;	// init to 1 secs

	if (fWriteFlag == qtssWriteFlagsIsRTCP)
		fNextTimeToRun = 1000;

	//determine if we need to send a receiver report to the multicast source
	if ((fWriteFlag == qtssWriteFlagsIsRTCP) && (currentTime > (fLastRRTime + kRRInterval)))
	{
		fLastRRTime = currentTime;
		fStream->SendReceiverReport();
	}

	//the rest of this function must be atomic wrt the ReflectorSession, because
	//it involves iterating through the RTPSession array, which isn't thread safe
	OSMutexLocker locker(&fStream->fBucketMutex);

	// Check to see if we should update the session's bitrate average
	fStream->UpdateBitRate(currentTime);

	// 视频数据流，最好直接定位到第一个关键帧起始包这样出视频的时间会更快一些

	fFirstPacketInQueueForNewOutput = nullptr;

	if (fKeyFrameStartPacketElementPointer)
	{
		fFirstPacketInQueueForNewOutput = fKeyFrameStartPacketElementPointer;
	}

	if (fFirstPacketInQueueForNewOutput == nullptr)
	{
		// where to start new clients in the q
		fFirstPacketInQueueForNewOutput = this->GetClientBufferStartPacketOffset(0);
	}

#if (0) //test code 
	if (NULL != fFirstPacketInQueueForNewOutput)
		printf("ReflectorSender::ReflectPackets SET first packet fFirstPacketInQueueForNewOutput %d \n", DGetPacketSeqNumber(&((ReflectorPacket*)(fFirstPacketInQueueForNewOutput->GetEnclosingObject()))->fPacketPtr));

	ReflectorPacket* thePacket = NULL;
	if (fFirstPacketInQueueForNewOutput != NULL)
		thePacket = (ReflectorPacket*)fFirstPacketInQueueForNewOutput->GetEnclosingObject();
	if (thePacket == NULL)
	{
		printf("fFirstPacketInQueueForNewOutput is NULL \n");

	}

#endif

	bool firstPacket = false;

	for (uint32_t bucketIndex = 0; bucketIndex < fStream->fOutputArray.size(); bucketIndex++)
	{
		for (uint32_t bucketMemberIndex = 0; bucketMemberIndex < fStream->sBucketSize; bucketMemberIndex++)
		{
			ReflectorOutput* theOutput = fStream->fOutputArray[bucketIndex][bucketMemberIndex];
			if (theOutput == nullptr || false == theOutput->IsPlaying()) continue;
			OSMutexLocker locker(&theOutput->fMutex);
			ReflectorPacket* packetElem = theOutput->GetBookMarkedPacket(fPacketQueue);
			if (packetElem == nullptr) // should only be a new output
			{
				packetElem = fFirstPacketInQueueForNewOutput; // everybody starts at the oldest packet in the buffer delay or uses a bookmark
				firstPacket = true;
				theOutput->setNewFlag(false);
			}

			int64_t  bucketDelay = ReflectorStream::sBucketDelayInMsec * (int64_t)bucketIndex;
			packetElem = SendPacketsToOutput(theOutput, packetElem, currentTime, bucketDelay, firstPacket);
			if (packetElem)
			{
				ReflectorPacket* thePacket = NeedRelocateBookMark(packetElem);

				thePacket->fNeededByOutput = true; 				// flag to prevent removal in RemoveOldPackets
				(void)theOutput->SetBookMarkPacket(thePacket); 	// store a reference to the packet
			}
		}
	}

	RemoveOldPackets();
	fFirstNewPacketInQueue = nullptr;

	//Don't forget that the caller also wants to know when we next want to run
	if (*ioWakeupTime == 0)
		*ioWakeupTime = fNextTimeToRun;
	else if ((fNextTimeToRun > 0) && (*ioWakeupTime > fNextTimeToRun))
		*ioWakeupTime = fNextTimeToRun;
	// exit with fNextTimeToRun in real time, not relative time.
	fNextTimeToRun += currentTime;

	// printf("SetNextTimeToRun fNextTimeToRun=%qd + currentTime=%qd\n", fNextTimeToRun, currentTime);
	// printf("ReflectorSender::ReflectPackets *ioWakeupTime = %qd\n", *ioWakeupTime);

}

ReflectorPacket*    ReflectorSender::SendPacketsToOutput(ReflectorOutput* theOutput, ReflectorPacket* currentPacket, int64_t currentTime, int64_t  bucketDelay, bool firstPacket)
{
	ReflectorPacket* lastPacket = currentPacket;
	auto it = std::find_if(begin(fPacketQueue), end(fPacketQueue),
		[currentPacket](const std::unique_ptr<ReflectorPacket> &pkt) {
		return pkt.get() == currentPacket;
	});

	uint32_t count = 0;
	QTSS_Error err = QTSS_NoErr;
	for (; it != fPacketQueue.end(); ++it)
	{
		const auto &thePacket = *it;
		lastPacket = thePacket.get();
		int64_t  packetLateness = bucketDelay;
		int64_t timeToSendPacket = -1;

		//printf("packetLateness %qd, seq# %li\n", packetLateness, (int32_t) DGetPacketSeqNumber( &thePacket->fPacketPtr ) );          

		err = theOutput->WritePacket(thePacket->fPacket, fStream, fWriteFlag, packetLateness, &timeToSendPacket, &thePacket->fStreamCountID, &thePacket->fTimeArrived, firstPacket);

		if (err == QTSS_WouldBlock)
		{ // call us again in # ms to retry on an EAGAIN

			if ((timeToSendPacket > 0) && ((fNextTimeToRun + currentTime) > timeToSendPacket)) // blocked but we are scheduled to wake up later
				fNextTimeToRun = timeToSendPacket - currentTime;

			if (theOutput->fLastIntervalMilliSec < 5)
				theOutput->fLastIntervalMilliSec = 5;

			if (timeToSendPacket < 0) // blocked and we are behind
			{    //printf("fNextTimeToRun = theOutput->fLastIntervalMilliSec=%qd;\n", theOutput->fLastIntervalMilliSec); // Use the last packet interval 
				this->SetNextTimeToRun(theOutput->fLastIntervalMilliSec);
			}

			if (fNextTimeToRun > 100) //don't wait that long
			{    //printf("fNextTimeToRun = %qd now 100;\n", fNextTimeToRun);
				this->SetNextTimeToRun(100);
			}

			if (fNextTimeToRun < 5) //wait longer
			{    //printf("fNextTimeToRun = 5;\n");
				this->SetNextTimeToRun(5);
			}

			if (theOutput->fLastIntervalMilliSec >= 100) // allow up to 1 second max -- allow some time for the socket to clear and don't go into a tight loop if the client is gone.
				theOutput->fLastIntervalMilliSec = 100;
			else
				theOutput->fLastIntervalMilliSec *= 2; // scale upwards over time

			//printf ( "Blocked ReflectorSender::SendPacketsToOutput timeToSendPacket=%qd fLastIntervalMilliSec=%qd fNextTimeToRun=%qd \n", timeToSendPacket, theOutput->fLastIntervalMilliSec, fNextTimeToRun);

			break;
		}

		count++;
	}

	return lastPacket;
}


ReflectorPacket* ReflectorSender::GetClientBufferStartPacketOffset(int64_t offsetMsec, bool needKeyFrameFirstPacket)
{
	int64_t theCurrentTime = OS::Milliseconds();
	int64_t packetDelay = 0;

	if (offsetMsec > ReflectorStream::sOverBufferInMsec)
		offsetMsec = ReflectorStream::sOverBufferInMsec;

	for (const auto &thePacket : fPacketQueue)
	{
		packetDelay = theCurrentTime - thePacket->fTimeArrived;
		if (packetDelay <= ReflectorStream::sOverBufferInMsec - offsetMsec)
			return thePacket.get();
	}

	return nullptr;
}

void    ReflectorSender::RemoveOldPackets()
{

	// Iterate through the senders queue to clear out packets
	// Start at the oldest packet and walk forward to the newest packet
	// 
	int64_t theCurrentTime = OS::Milliseconds();
	int64_t packetDelay = 0;
	int64_t currentMaxPacketDelay = ReflectorStream::sMaxPacketAgeMSec;


	for (auto it = fPacketQueue.begin(); it != fPacketQueue.end(); )
	{
		const auto &thePacket = *it;
		Assert(thePacket.get());
		//printf("ReflectorSender::RemoveOldPackets Packet %d in queue is %qd milliseconds old\n", DGetPacketSeqNumber( &thePacket->fPacketPtr ) ,theCurrentTime - thePacket->fTimeArrived);


		packetDelay = theCurrentTime - thePacket->fTimeArrived;

		// walk q and remove packets that are too old
		if (!thePacket->fNeededByOutput && packetDelay > currentMaxPacketDelay) // delete based on late tolerance and whether a client is blocked on the packet
		{
			// not needed and older than our required buffer
			it = fPacketQueue.erase(it);
		}
		else
		{   // we want to keep all of these but we should reset the ones that should be aged out unless marked
			// as need the next time through reflect packets.
			++it;

			if (fKeyFrameStartPacketElementPointer == thePacket.get())
					break;

			//if(IsKeyFrameFirstPacket(thePacket))
			//	break;

			thePacket->fNeededByOutput = false; //mark not needed.. will be set next time through reflect packets
			if (packetDelay <= currentMaxPacketDelay)  // this packet is going to be kept around as well as the ones that follow.
				break;
		}
	}


}

// if current packet over max packetAgeTime, we need relocate the BookMark to
// the new fKeyFrameStartPacketElementPointer
ReflectorPacket* ReflectorSender::NeedRelocateBookMark(ReflectorPacket* thePacket)
{
	//1、判断当前Packet是否已经超过了最大缓冲周期时间(不判断音/视频、I/P帧)
	//2、当时间超过了阀值,查找最新的fKeyFrameStartPacketElementPointer
	//3、返回最新的fKeyFrameStartPacketElementPointer做为最新的BookMark

	int64_t theCurrentTime = OS::Milliseconds();
	int64_t packetDelay = 0;
	int64_t currentMaxPacketDelay = ReflectorStream::sRelocatePacketAgeMSec;

	Assert(thePacket);

	packetDelay = theCurrentTime - thePacket->fTimeArrived;

	if ((packetDelay > currentMaxPacketDelay) /*&& IsFrameFirstPacket(thePacket)*/)//or IsFrameFirstPacket
	{
		if (fKeyFrameStartPacketElementPointer)
		{
			if (fKeyFrameStartPacketElementPointer->fTimeArrived > thePacket->fTimeArrived)
			{
				this->fStream->GetMyReflectorSession()->SetHasVideoKeyFrameUpdate(true);
				return fKeyFrameStartPacketElementPointer;
			}
		}
	}

	return thePacket;
}

ReflectorPacket*    ReflectorSender::GetNewestKeyFrameFirstPacket(ReflectorPacket* currentElem, int64_t offsetMsec)
{
	//printf("[geyijun] GetNewestKeyFrameFirstPacket---------------->1\n");
	int64_t theCurrentTime = OS::Milliseconds();
	int64_t packetDelay = 0;
	ReflectorPacket* requestedPacket = nullptr;
	auto it = std::find_if(begin(fPacketQueue), end(fPacketQueue),
		[currentElem](const std::unique_ptr<ReflectorPacket> &pkt) {
		return pkt.get() == currentElem;
	});
	for (; it != fPacketQueue.end(); ++it) // start at oldest packet in q
	{
		const auto &thePacket = *it;
		if (thePacket == nullptr)
		{
			break;
		}
		if (IsKeyFrameFirstPacket(thePacket.get()))
		{
			requestedPacket = thePacket.get();
			//printf("[geyijun]Maybe,GetNewestKeyFrameFirstPacket --->[%#x]\n",requestedPacket);	

			//
			packetDelay = theCurrentTime - thePacket->fTimeArrived;
			if (offsetMsec > ReflectorStream::sOverBufferInMsec)
				offsetMsec = ReflectorStream::sOverBufferInMsec;
			if (packetDelay <= (ReflectorStream::sOverBufferInMsec - offsetMsec))
			{
				break;
			}
		}
	}
	if (requestedPacket == nullptr)
	{
		printf("[geyijun]GetNewestKeyFrameFirstPacket --->[NotFound]\n");
	}
	else
	{
		printf("[geyijun]Final,GetNewestKeyFrameFirstPacket --->[%#x]\n", requestedPacket);
	}
	return requestedPacket;
}

/*
	判断当前RTP包是否为H.264 I关键帧的第一个RTP包
	下面的写法可能不太严谨(仅针对H264 的情况，未考虑其他格式)
*/
bool ReflectorSender::IsKeyFrameFirstPacket(ReflectorPacket* thePacket)
{
#if 0
	char const* nal_unit_type_description_h264[32] = {
	  "Unspecified", //0
	  "Coded slice of a non-IDR picture", //1
	  "Coded slice data partition A", //2
	  "Coded slice data partition B", //3
	  "Coded slice data partition C", //4
	  "Coded slice of an IDR picture", //5
	  "Supplemental enhancement information (SEI)", //6
	  "Sequence parameter set", //7
	  "Picture parameter set", //8
	  "Access unit delimiter", //9
	  "End of sequence", //10
	  "End of stream", //11
	  "Filler data", //12
	  "Sequence parameter set extension", //13
	  "Prefix NAL unit", //14
	  "Subset sequence parameter set", //15
	  "Reserved", //16
	  "Reserved", //17
	  "Reserved", //18
	  "Coded slice of an auxiliary coded picture without partitioning", //19
	  "Coded slice extension", //20
	  "Reserved", //21
	  "Reserved", //22
	  "Reserved", //23
	  "Unspecified", //24
	  "Unspecified", //25
	  "Unspecified", //26
	  "Unspecified", //27
	  "Unspecified", //28
	  "Unspecified", //29
	  "Unspecified", //30
	  "Unspecified" //31
	};
	struct RTPHeader {
		//unsigned int version:2;          /* protocol version */
		//unsigned int p:1;                /* padding flag */
		//unsigned int x:1;                /* header extension flag */
		//unsigned int cc:4;               /* CSRC count */
		//unsigned int m:1;                /* marker bit */
		//unsigned int pt:7;               /* payload type */
		uint16_t rtpheader;
		uint16_t seq;				        /* sequence number */
		uint32_t ts;                       /* timestamp */
		uint32_t ssrc;                     /* synchronization source */
		//uint32_t csrc[1];                /* optional CSRC list */
	};
#endif
	//printf("[geyijun] IsKeyFrameFirstPacket--->RtpSeq[%d]\n",thePacket->GetPacketRTPSeqNum());	
	Assert(thePacket);
	if (thePacket->fPacket.size() >= 20)
	{
		uint8_t csrc_count = thePacket->fPacket[0] & 0x0f;
		uint32_t rtp_head_size = /*sizeof(struct RTPHeader)*/12 + csrc_count * sizeof(uint32_t);
		uint8_t nal_unit_type = thePacket->fPacket[rtp_head_size + 0] & 0x1F;
		//printf("[geyijun] IsKeyFrameFirstPacket 111--->nal_unit_type[%d]\n",nal_unit_type);
		if ((nal_unit_type >= 1) && (nal_unit_type <= 23))	//单一包
		{
			;//不需要转换
		}
		else if (nal_unit_type == 24)//STAP-A
		{
			if (thePacket->fPacket.size() > rtp_head_size + 3)
				nal_unit_type = thePacket->fPacket[rtp_head_size + 3] & 0x1F;
		}
		else if (nal_unit_type == 25)//STAP-B
		{
			if (thePacket->fPacket.size() > rtp_head_size + 5)
				nal_unit_type = thePacket->fPacket[rtp_head_size + 5] & 0x1F;
		}
		else if (nal_unit_type == 26)//MTAP16
		{
			if (thePacket->fPacket.size() > rtp_head_size + 8)
				nal_unit_type = thePacket->fPacket[rtp_head_size + 8] & 0x1F;
		}
		else if (nal_unit_type == 27)//MTAP24
		{
			if (thePacket->fPacket.size() > rtp_head_size + 9)
				nal_unit_type = thePacket->fPacket[rtp_head_size + 9] & 0x1F;
		}
		else if ((nal_unit_type == 28) || (nal_unit_type == 29))//FU-A/B
		{
			//printf("[geyijun] IsKeyFrameFirstPacket fPacketPtr.Len[%d] rtp_head_size[%d]\n",thePacket->fPacketPtr.Len,rtp_head_size);	
			if (thePacket->fPacket.size() > rtp_head_size + 1)
			{

				uint8_t startBit = thePacket->fPacket[rtp_head_size + 1] & 0x80;
				if (startBit)
				{
					//printf("[geyijun] IsKeyFrameFirstPacket AAA--->[%x:%x:%x:%x:]\n",
					//	(unsigned char )thePacket->fPacketPtr.Ptr[rtp_head_size+0],
					//	(unsigned char )thePacket->fPacketPtr.Ptr[rtp_head_size+1],
					//	(unsigned char )thePacket->fPacketPtr.Ptr[rtp_head_size+2],
					//	(unsigned char )thePacket->fPacketPtr.Ptr[rtp_head_size+3]);
					nal_unit_type = thePacket->fPacket[rtp_head_size + 1] & 0x1F;
					//printf("[geyijun] IsKeyFrameFirstPacket BBB--->[%#x]\n",nal_unit_type);
				}
			}
		}
		//printf("[geyijun] IsKeyFrameFirstPacket 222--->nal_unit_type[%d]\n",nal_unit_type);	
		if ((nal_unit_type == 5) || (nal_unit_type == 7) || (nal_unit_type == 8))
		{
			//printf("[geyijun] IsKeyFrameFirstPacket 333--->nal_unit_type[%d]\n",nal_unit_type);	
			return true;
		}
	}
	return false;
}

bool ReflectorSender::IsFrameFirstPacket(ReflectorPacket* thePacket)
{
	//printf("[geyijun] IsKeyFrameFirstPacket--->RtpSeq[%d]\n",thePacket->GetPacketRTPSeqNum());	
	Assert(thePacket);
	if (thePacket->fPacket.size() >= 20)
	{
		uint8_t csrc_count = thePacket->fPacket[0] & 0x0f;
		uint32_t rtp_head_size = /*sizeof(struct RTPHeader)*/12 + csrc_count * sizeof(uint32_t);
		uint8_t nal_unit_type = thePacket->fPacket[rtp_head_size + 0] & 0x1F;
		if ((nal_unit_type >= 1) && (nal_unit_type <= 23))	//单一包
		{
			return true;
		}
		else if (nal_unit_type == 24)//STAP-A
		{
			return true;
		}
		else if (nal_unit_type == 25)//STAP-B
		{
			return true;
		}
		else if (nal_unit_type == 26)//MTAP16
		{
			return true;
		}
		else if (nal_unit_type == 27)//MTAP24
		{
			return true;
		}
		else if ((nal_unit_type == 28) || (nal_unit_type == 29))//FU-A/B
		{
			if (thePacket->fPacket.size() > rtp_head_size + 1)
			{
				uint8_t startBit = thePacket->fPacket[rtp_head_size + 1] & 0x80;
				if (startBit)
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool ReflectorSender::IsFrameLastPacket(ReflectorPacket* thePacket)
{
	//printf("[geyijun] IsFrameLastPacket--->RtpSeq[%d]\n",thePacket->GetPacketRTPSeqNum());	
	Assert(thePacket);
	if (thePacket->fPacket.size() >= 20)
	{
		uint8_t markBit = thePacket->fPacket[1] & 0x80;
		if (markBit)
		{
			//printf("[geyijun] IsFrameLastPacket --->OK\n");	
			return true;
		}
	}
	return false;
}

void ReflectorSocketPool::SetUDPSocketOptions(UDPSocketPair* inPair)
{
	// Fix add ReuseAddr for compatibility with MPEG4IP broadcaster which likes to use the same
	//sockets.  

	//Make sure this works with PlaylistBroadcaster 
	//inPair->GetSocketA()->ReuseAddr();
	//inPair->GetSocketA()->ReuseAddr();

}


UDPSocketPair* ReflectorSocketPool::ConstructUDPSocketPair()
{
	return new UDPSocketPair
	(new ReflectorSocket(), new ReflectorSocket());
}

void ReflectorSocketPool::DestructUDPSocket(ReflectorSocket* socket)
{
	if (socket) // allocated
	{
		//if (socket->GetLocalPort() > 0) // bound and active
		//{   //The socket's run function may be executing RIGHT NOW! So we can't
			//just delete the thing, we need to send the sockets kill events.
			//printf("ReflectorSocketPool::DestructUDPSocketPair Signal kKillEvent socket=%p\n",socket);
		socket->Signal(Task::kKillEvent);
		//}
		//else // not bound ok to delete
		//{   //printf("ReflectorSocketPool::DestructUDPSocketPair delete socket=%p\n",socket);
		//    delete socket;
		//}  
	}
}

void ReflectorSocketPool::DestructUDPSocketPair(UDPSocketPair *inPair)
{
	//printf("ReflectorSocketPool::DestructUDPSocketPair inPair=%p socketA=%p\n", inPair,(ReflectorSocket*)inPair->GetSocketA());
	this->DestructUDPSocket((ReflectorSocket*)inPair->GetSocketA());

	//printf("ReflectorSocketPool::DestructUDPSocketPair inPair=%p socketB=%p\n", inPair,(ReflectorSocket*)inPair->GetSocketB());
	this->DestructUDPSocket((ReflectorSocket*)inPair->GetSocketB());

	delete inPair;
}

ReflectorSocket::ReflectorSocket()
	: IdleTask(),
	UDPSocket(nullptr, Socket::kNonBlockingSocketType | UDPSocket::kWantsDemuxer)
{
	//construct all the preallocated packets
	this->SetTaskName("ReflectorSocket");
	this->SetTask(this);
}

void    ReflectorSocket::AddSender(ReflectorSender* inSender)
{
	OSMutexLocker locker(this->GetDemuxer()->GetMutex());
	QTSS_Error err = this->GetDemuxer()->RegisterTask(inSender->fStream->fStreamInfo.fSrcIPAddr, 0, inSender);
	Assert(err == QTSS_NoErr);
	fSenderQueue.push_back(inSender);
}

void    ReflectorSocket::RemoveSender(ReflectorSender* inSender)
{
	OSMutexLocker locker(this->GetDemuxer()->GetMutex());
	auto it = std::find(fSenderQueue.begin(), fSenderQueue.end(), inSender);
	if (it != fSenderQueue.end())
		fSenderQueue.erase(it);
	QTSS_Error err = this->GetDemuxer()->UnregisterTask(inSender->fStream->fStreamInfo.fSrcIPAddr, 0, inSender);
	Assert(err == QTSS_NoErr);
}

int64_t ReflectorSocket::Run()
{
	//We want to make sure we can't get idle events WHILE we are inside
	//this function. That will cause us to run the queues unnecessarily
	//and just get all confused.
	this->CancelTimeout();

	Task::EventFlags theEvents = this->GetEvents();
	//if we have been told to delete ourselves, do so.
	if (theEvents & Task::kKillEvent)
		return -1;

	OSMutexLocker locker(this->GetDemuxer()->GetMutex());
	int64_t theMilliseconds = OS::Milliseconds();

	//Only check for data on the socket if we've actually been notified to that effect
	if (theEvents & Task::kReadEvent)
		this->GetIncomingData(theMilliseconds);

#if DEBUG
	//make sure that we haven't gotten here prematurely! This wouldn't mess
	//anything up, but it would waste CPU.
	if (theEvents & Task::kIdleEvent)
	{
		int32_t temp = (int32_t)(fSleepTime - theMilliseconds);
		char tempBuf[20];
		sprintf(tempBuf, "%" _S32BITARG_ "", temp);
		WarnV(fSleepTime <= theMilliseconds, tempBuf);
	}
#endif

	fSleepTime = 0;
	//Now that we've gotten all available packets, have the streams reflect
	for (const auto &theSender2 : fSenderQueue)
		if (theSender2 != nullptr && theSender2->ShouldReflectNow(theMilliseconds, &fSleepTime))
			theSender2->ReflectPackets(&fSleepTime);

#if DEBUG
	theMilliseconds = OS::Milliseconds();
#endif

	//For smoothing purposes, the streams can mark when they want to wakeup.
	if (fSleepTime > 0)
		this->SetIdleTimer(fSleepTime);
#if DEBUG
	//The debugging check above expects real time.
	fSleepTime += theMilliseconds;
#endif

	return 0;
}


void ReflectorSocket::FilterInvalidSSRCs(ReflectorPacket* thePacket, bool isRTCP)
{   // assume the first SSRC we see is valid and all others are to be ignored.
	if (thePacket->fPacket.size() > 0) do
	{
		int64_t currentTime = OS::Milliseconds() / 1000;
		if (0 == fValidSSRC)
		{
			fValidSSRC = thePacket->GetSSRC(isRTCP); // SSRC of 0 is allowed
			fLastValidSSRCTime = currentTime;
			//printf("socket=%"   _U32BITARG_   " FIRST PACKET fValidSSRC=%"   _U32BITARG_   " \n", (uint32_t) this,fValidSSRC);
			break;
		}

		uint32_t packetSSRC = thePacket->GetSSRC(isRTCP);
		if (packetSSRC != 0)
		{
			if (packetSSRC == fValidSSRC)
			{
				fLastValidSSRCTime = currentTime;
				//printf("socket=%"   _U32BITARG_   " good packet\n", (uint32_t) this );
				break;
			}

			//printf("socket=%"   _U32BITARG_   " bad packet packetSSRC= %"   _U32BITARG_   " fValidSSRC=%"   _U32BITARG_   " \n", (uint32_t) this,packetSSRC,fValidSSRC);
			thePacket->fPacket.clear(); // ignore this packet wrong SSRC
		}

		// this executes whenever an invalid SSRC is found -- maybe the original stream ended and a new one is now active
		if ((fLastValidSSRCTime + fTimeoutSecs) < currentTime) // fValidSSRC timed out --no packets with this SSRC seen for awhile
		{
			fValidSSRC = 0; // reset the valid SSRC with the next packet's SSRC
		 //printf("RESET fValidSSRC\n");
		}

	} while (false);
}

bool ReflectorSocket::ProcessPacket(const int64_t& inMilliseconds, ReflectorPacket* thePacket, uint32_t theRemoteAddr, uint16_t theRemotePort)
{
	bool done = false; // stop when result is true
	if (thePacket != nullptr) do
	{
		if (GetLocalPort() & 1)
			thePacket->fIsRTCP = true;
		else
			thePacket->fIsRTCP = false;

		if (fBroadcasterClientSession != nullptr) // alway refresh timeout even if we are filtering.
		{
			if ((inMilliseconds - fLastBroadcasterTimeOutRefresh) > kRefreshBroadcastSessionIntervalMilliSecs)
			{
				fBroadcasterClientSession->RefreshTimeouts();
				fLastBroadcasterTimeOutRefresh = inMilliseconds;
			}
		}

		if (thePacket->fPacket.empty())
		{
			//put the packet back on the free queue, because we didn't actually
			//get any data here.
			delete thePacket;
			this->RequestEvent(EV_RE);
			done = true;
			//printf("ReflectorSocket::ProcessPacket no more packets on this socket!\n");
			break;//no more packets on this socket!
		}

		if (thePacket->IsRTCP())
		{
			//if this is a new RTCP packet, check to see if it is a sender report.
			//We should only reflect sender reports. Because RTCP packets can't have both
			//an SR & an RR, and because the SR & the RR must be the first packet in a
			//compound RTCP packet, all we have to do to determine this is look at the
			//packet type of the first packet in the compound packet.
			RTCPPacket theRTCPPacket;
			if ((!theRTCPPacket.ParsePacket((uint8_t*)&thePacket->fPacket[0], thePacket->fPacket.size())) ||
				(theRTCPPacket.GetPacketType() != RTCPSRPacket::kSRPacketType))
			{
				//pretend as if we never got this packet
				delete thePacket;
				done = true;
				break;
			}
		}

		// Only reflect one SSRC stream at a time.
		// Pass the packet and whether it is an RTCP or RTP packet based on the port number.
		if (fFilterSSRCs)
			this->FilterInvalidSSRCs(thePacket, GetLocalPort() & 1);// thePacket->fPacketPtr.Len is set to 0 for invalid SSRCs.

		// Find the appropriate ReflectorSender for this packet.
		auto* theSender = (ReflectorSender*)this->GetDemuxer()->GetTask(theRemoteAddr, 0);
		// If there is a generic sender for this socket, use it.
		if (theSender == nullptr)
			theSender = (ReflectorSender*)this->GetDemuxer()->GetTask(0, 0);

		if (theSender == nullptr)
		{
			//uint16_t* theSeqNumberP = (uint16_t*)thePacket->fPacketPtr.Ptr;
			//printf("ReflectorSocket::ProcessPacket no sender found for packet! sequence number=%d\n",ntohs(theSeqNumberP[1]));
			delete thePacket; // don't process the packet
			done = true;
			break;
		}

		Assert(theSender != nullptr); // at this point we have a sender

		const uint32_t maxQSize = 4000;

		// Check to see if we need to set the remote RTCP address
	   // for this stream. This will be necessary if the source is unicast.
#ifdef NAT_WORKAROUND
		if ((theRemoteAddr != 0) && ((theSender->fStream->fDestRTCPAddr == 0) || (thePacket->IsRTCP()))) // Submitted fix from  denis@berlin.ccc.de
		{
			Assert(!SocketUtils::IsMulticastIPAddr(theSender->fStream->fStreamInfo.fDestIPAddr));
			Assert(theRemotePort != 0);
			theSender->fStream->fDestRTCPAddr = theRemoteAddr;
			theSender->fStream->fDestRTCPPort = theRemotePort;

			// RTCPs are always on odd ports, so check to see if this port is an
			// RTP port, and if so, just add 1.
			if (!(thePacket->IsRTCP()) && !(theRemotePort & 1))
				theSender->fStream->fDestRTCPPort++;
		}
#else
		if ((theRemoteAddr != 0) && (theSender->fStream->fDestRTCPAddr == 0))
		{
			// If the source is multicast, this shouldn't be necessary
			Assert(!SocketUtils::IsMulticastIPAddr(theSender->fStream->fStreamInfo.fDestIPAddr));
			Assert(theRemotePort != 0);
			theSender->fStream->fDestRTCPAddr = theRemoteAddr;
			theSender->fStream->fDestRTCPPort = theRemotePort;

			// RTCPs are always on odd ports, so check to see if this port is an
			// RTP port, and if so, just add 1.
			if (!(theRemotePort & 1))
				theSender->fStream->fDestRTCPPort++;
		}
#endif //NAT_WORKAROUND
		thePacket->fStreamCountID = ++(theSender->fStream->fPacketCount);
		thePacket->fBucketsSeenThisPacket = 0;
		thePacket->fTimeArrived = inMilliseconds;
		theSender->fPacketQueue.emplace_back(thePacket);

		// TODO:A、对H264视频RTP包进行关键帧过滤，保存最新关键帧首个RTP包指针
		// 1、判断是否为视频H.264 RTP
		SourceInfo::StreamInfo* streamInfo = theSender->fStream->GetStreamInfo();
		if (!(thePacket->IsRTCP()) && (streamInfo->fPayloadType == qtssVideoPayloadType) && (streamInfo->fPayloadName =="H264/90000"))
		{
			// 2、在这里判断上面插入的thePacket是否为关键帧起始RTP包，如果是，这记录thePacket->fQueueElem
			if (theSender->IsKeyFrameFirstPacket(thePacket))
			{
				//printf("\nI");
				//3、取消原来的fKeyFrameStartPacketElementPointer
				if (theSender->fKeyFrameStartPacketElementPointer)
					theSender->fKeyFrameStartPacketElementPointer->fNeededByOutput = false;

				//4、设置最新的fKeyFrameStartPacketElementPointer
				{
					//锁定关键帧不会被Remove
					thePacket->fNeededByOutput = true;
					//更新最新的关键帧开始包
					theSender->fKeyFrameStartPacketElementPointer = thePacket;
				}

				//5、设置ReflectorSession标志位，Notify有新视频关键帧，提醒音频队列更新
				{
					theSender->fStream->GetMyReflectorSession()->SetHasVideoKeyFrameUpdate(true);
				}
			}
			//else if(theSender->IsFrameFirstPacket(thePacket))
			//{
			//	printf("P");
			//}
		}

		// TODO:B、如果视频关键帧有更新，音频也根据视频的更新立即进行更新
		// 1、判断是否为音频RTP且有视频关键帧更新Notify
		if (!(thePacket->IsRTCP()) && (streamInfo->fPayloadType == qtssAudioPayloadType) && (theSender->fStream->GetMyReflectorSession()->HasVideoKeyFrameUpdate()))
		{
			//2、取消原来音频的fKeyFrameStartPacketElementPointer
			if (theSender->fKeyFrameStartPacketElementPointer)
				theSender->fKeyFrameStartPacketElementPointer->fNeededByOutput = false;

			//4、设置最新的音频fKeyFrameStartPacketElementPointer
			{
				//锁定关键帧不会被Remove
				thePacket->fNeededByOutput = true;
				//更新最新的关键帧开始包
				theSender->fKeyFrameStartPacketElementPointer = thePacket;
			}

			//5、设置ReflectorSession标志位，Notify有新视频关键帧，提醒音频队列更新
			{
				theSender->fStream->GetMyReflectorSession()->SetHasVideoKeyFrameUpdate(false);
			}
		}



		if (theSender->fFirstNewPacketInQueue == nullptr)
			theSender->fFirstNewPacketInQueue = thePacket;
		theSender->fHasNewPackets = true;

		if (!(thePacket->IsRTCP()))
		{
			// don't check for duplicate packets, they may be needed to keep in sync.
			// Because this is an RTP packet make sure to atomic add this because
			// multiple sockets can be adding to this variable simultaneously
			(void)atomic_add(&theSender->fStream->fBytesSentInThisInterval, thePacket->fPacket.size());
			//printf("ReflectorSocket::ProcessPacket received RTP id=%qu\n", thePacket->fStreamCountID); 
			theSender->fStream->SetHasFirstRTP(true);
		}
		else
		{
			//printf("ReflectorSocket::ProcessPacket received RTCP id=%qu\n", thePacket->fStreamCountID); 
			theSender->fStream->SetHasFirstRTCP(true);
			theSender->fStream->SetFirst_RTCP_RTP_Time(thePacket->GetPacketRTPTime());
			theSender->fStream->SetFirst_RTCP_Arrival_Time(thePacket->fTimeArrived);
		}


		if (ReflectorStream::sUsePacketReceiveTime && thePacket->fPacket.size() > 12)
		{
			uint32_t offset = thePacket->fPacket.size();
			char* theTag = ((char*)thePacket->fPacket.data() + offset) - 12;
			auto* theValue = (uint64_t*)((char*)((char*)thePacket->fPacket.data() + offset) - 8);

			if (0 == ::strncmp(theTag, "aktt", 4))
			{
				uint64_t theReceiveTime = OS::NetworkToHostSInt64(*theValue);
				uint32_t theSSRC = thePacket->GetSSRC(theRemotePort & 1); // use to check if broadcast has restarted so we can reset

				if (!this->fHasReceiveTime || (this->fCurrentSSRC != theSSRC))
				{
					this->fCurrentSSRC = theSSRC;
					this->fFirstArrivalTime = thePacket->fTimeArrived;
					this->fFirstReceiveTime = theReceiveTime;
					this->fHasReceiveTime = true;
				}


				int64_t packetOffsetFromStart = theReceiveTime - this->fFirstReceiveTime; // packets arrive at time 0 and fill forward into the future
				thePacket->fTimeArrived = this->fFirstArrivalTime + packetOffsetFromStart; // offset starts negative by over buffer amount
				thePacket->fPacket.erase(thePacket->fPacket.end() - 12, thePacket->fPacket.end());

				int64_t arrivalTimeOffset = thePacket->fTimeArrived - inMilliseconds;
				if (arrivalTimeOffset > ReflectorStream::sMaxFuturePacketMSec) // way out in the future.
					thePacket->fTimeArrived = inMilliseconds + ReflectorStream::sMaxFuturePacketMSec; //keep it but only for sMaxFuturePacketMSec =  (sMaxPacketAgeMSec <-- current --> sMaxFuturePacketMSec)

				// if it was in the past we leave it alone because it will be deleted after processing.


				//printf("ReflectorSocket::ProcessPacket packetOffsetFromStart=%f\n", (float) packetOffsetFromStart / 1000);
			}

		}

		//printf("ReflectorSocket::GetIncomingData has packet from time=%qd src addr=%"   _U32BITARG_   " src port=%u packetlen=%"   _U32BITARG_   "\n",inMilliseconds, theRemoteAddr,theRemotePort,thePacket->fPacketPtr.Len);
		if (0) //turn on / off buffer size checking --  pref can go here if we find we need to adjust this
			if (theSender->fPacketQueue.size() > maxQSize) //don't grow memory too big
			{
				char outMessage[256];
				sprintf(outMessage, "Packet Queue for port=%d qsize = %" _S32BITARG_ " hit max qSize=%"   _U32BITARG_   "", theRemotePort, theSender->fPacketQueue.size(), maxQSize);
				WarnV(false, outMessage);
			}



	} while (false);

	return done;
}


void ReflectorSocket::GetIncomingData(const int64_t& inMilliseconds)
{
	OSMutexLocker locker(this->GetDemuxer()->GetMutex());
	uint32_t theRemoteAddr = 0;
	uint16_t theRemotePort = 0;
	//get all the outstanding packets for this socket
	while (true)
	{
		//get a packet off the free queue.
		ReflectorPacket* thePacket = this->GetPacket();
		static const size_t kMaxReflectorPacketSize = 2060;
		static char fPacketPtr[kMaxReflectorPacketSize];
		size_t fPacketLen;
		(void)this->RecvFrom(&theRemoteAddr, &theRemotePort, fPacketPtr,
			kMaxReflectorPacketSize, &fPacketLen);

		thePacket->SetPacketData(fPacketPtr, fPacketLen);
		if (this->ProcessPacket(inMilliseconds, thePacket, theRemoteAddr, theRemotePort))
			break;

		//printf("ReflectorSocket::GetIncomingData \n");
	}

}

ReflectorPacket* ReflectorSocket::GetPacket()
{
	OSMutexLocker locker(this->GetDemuxer()->GetMutex());
	return new ReflectorPacket();
}
