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
#include "SDPSourceInfo.h"
#include "MyRTSPRequest.h"

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
uint32_t                          ReflectorStream::sMaxFuturePacketMSec = 60000; // max packet future time

uint32_t                          ReflectorStream::sMaxFuturePacketSec = 60; // max packet future time
uint32_t                          ReflectorStream::sOverBufferInSec = 10;
uint32_t                          ReflectorStream::sFirstPacketOffsetMsec = 500;

bool IsKeyFrameFirstPacket(const ReflectorPacket &thePacket)
{
	if (thePacket.fPacket.size() < 20) return false;

	uint8_t csrc_count = thePacket.fPacket[0] & 0x0f;
	uint32_t rtp_head_size = /*sizeof(struct RTPHeader)*/12 + csrc_count * sizeof(uint32_t);
	uint8_t nal_unit_type = thePacket.fPacket[rtp_head_size + 0] & 0x1F;
	if (nal_unit_type == 24)//STAP-A
	{
		if (thePacket.fPacket.size() > rtp_head_size + 3)
			nal_unit_type = thePacket.fPacket[rtp_head_size + 3] & 0x1F;
	}
	else if (nal_unit_type == 25)//STAP-B
	{
		if (thePacket.fPacket.size() > rtp_head_size + 5)
			nal_unit_type = thePacket.fPacket[rtp_head_size + 5] & 0x1F;
	}
	else if (nal_unit_type == 26)//MTAP16
	{
		if (thePacket.fPacket.size() > rtp_head_size + 8)
			nal_unit_type = thePacket.fPacket[rtp_head_size + 8] & 0x1F;
	}
	else if (nal_unit_type == 27)//MTAP24
	{
		if (thePacket.fPacket.size() > rtp_head_size + 9)
			nal_unit_type = thePacket.fPacket[rtp_head_size + 9] & 0x1F;
	}
	else if ((nal_unit_type == 28) || (nal_unit_type == 29))//FU-A/B
	{
		if (thePacket.fPacket.size() > rtp_head_size + 1)
		{
			uint8_t startBit = thePacket.fPacket[rtp_head_size + 1] & 0x80;
			if (startBit)
				nal_unit_type = thePacket.fPacket[rtp_head_size + 1] & 0x1F;
		}
	}

	return nal_unit_type == 5 || nal_unit_type == 7 || nal_unit_type == 8;
}

enum class KeyFrameType : char {
	Video,
	Audio,
	None
};

static KeyFrameType needToUpdateKeyFrame(ReflectorStream* stream,
	const ReflectorPacket &thePacket)
{
	StreamInfo* info = stream->GetStreamInfo();
	if (info->fPayloadType == qtssVideoPayloadType && info->fPayloadName == "H264/90000"
		&& IsKeyFrameFirstPacket(thePacket))
		return KeyFrameType::Video;
	if (info->fPayloadType == qtssAudioPayloadType && stream->GetMyReflectorSession()->HasVideoKeyFrameUpdate())
		return KeyFrameType::Audio;
	return KeyFrameType::None;
}

ReflectorStream::ReflectorStream(StreamInfo* inInfo)
	: fPacketCount(0),
	fSockets(nullptr),
	fRTPSender(this, qtssWriteFlagsIsRTP),
	fRTCPSender(this, qtssWriteFlagsIsRTCP),
	fBucketMutex(),

	fDestRTCPAddr(0),
	fDestRTCPPort(0),

	fCurrentBitRate(0),
	fLastBitRateSample(OS::Milliseconds()), // don't calculate our first bit rate until kBitRateAvgIntervalInMilSecs has passed!
	fBytesSentInThisInterval(0),

	fRTPChannel(-1),
	fRTCPChannel(-1),
	fEyeCount(0),
	fMyReflectorSession(nullptr),
	fStreamInfo(*inInfo)
{

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
	if (fSockets != nullptr)
	{
		//first things first, let's take this stream off the socket's queue
		//of streams. This will basically ensure that no reflecting activity
		//can happen on this stream.
		fSockets->GetSocketA()->RemoveSender(&fRTPSender);
		fSockets->GetSocketB()->RemoveSender(&fRTCPSender);

		//leave the multicast group. Because this socket is shared amongst several
		//potential multicasts, we don't want to remain a member of a stale multicast
		if (SocketUtils::IsMulticastIPAddr(fStreamInfo.fDestIPAddr))
		{
			fSockets->GetSocketA()->LeaveMulticast(fStreamInfo.fDestIPAddr);
			fSockets->GetSocketB()->LeaveMulticast(fStreamInfo.fDestIPAddr);
		}
		//now release the socket pair
		if (qtssRTPTransportTypeTCP == fTransportType)
			sSocketPool.DestructUDPSocketPair(fSockets);
	}

	// 释放关键帧缓冲区
	if (pkeyFrameCache)
	{
		delete pkeyFrameCache;
		pkeyFrameCache = nullptr;
	}
}

void ReflectorStream::AddOutput(ReflectorOutput* inOutput)
{
	OSMutexLocker locker(&fBucketMutex);
	fOutputArray.push_back(inOutput);
}

void  ReflectorStream::RemoveOutput(ReflectorOutput* inOutput)
{
	OSMutexLocker locker(&fBucketMutex);
	auto it = std::find(begin(fOutputArray), end(fOutputArray), inOutput);
	if (it != end(fOutputArray))
		fOutputArray.erase(it);
}

void  ReflectorStream::TearDownAllOutputs()
{
	OSMutexLocker locker(&fBucketMutex);

	//look at all the indexes in the array
	for (auto &theOutputPtr : fOutputArray)
	{
		theOutputPtr->TearDown();
#if REFLECTOR_STREAM_DEBUGGING  
		printf("TearDownAllOutputs Removing output from bucket %" _S32BITARG_ ", index %" _S32BITARG_ "\n", x, y);
#endif
	}
}


QTSS_Error ReflectorStream::BindSockets(RTSPRequest *inRequest, RTPSession *inSession, uint32_t inReflectorSessionFlags, bool filterState, uint32_t timeout)
{
	// If the incoming data is RTSP interleaved, we don't need to do anything here
	if (inReflectorSessionFlags & ReflectorSession::kIsPushSession)
		fStreamInfo.fSetupToReceive = true;

	// Set the transport Type a Broadcaster
	fTransportType = inRequest->GetTransportType();

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

	if (fSockets == nullptr)
		return inRequest->SendErrorResponse(qtssServerInternal);

	// If we know the source IP address of this broadcast, we can demux incoming traffic
	// on the same port by that source IP address. If we don't know the source IP addr,
	// it is impossible for us to demux, and therefore we shouldn't allow multiple
	// broadcasts on the same port.
	if (fSockets->GetSocketA()->HasSender() && (fStreamInfo.fSrcIPAddr == 0))
		return inRequest->SendErrorResponse(qtssServerInternal);

	//also put this stream onto the socket's queue of streams
	fSockets->GetSocketA()->AddSender(&fRTPSender);
	fSockets->GetSocketB()->AddSender(&fRTCPSender);

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
	std::vector<char> temp(fReceiverReportBuffer, fReceiverReportBuffer + fReceiverReportSize);
	(void)fSockets->GetSocketB()->SendTo(fDestRTCPAddr, fDestRTCPPort, temp);
}

void ReflectorStream::PushPacket(char *packet, size_t packetLen, bool isRTCP)
{
	if (packetLen > 0)
	{
		auto thePacket = std::make_unique<ReflectorPacket>(packet, packetLen);
		if (isRTCP)
		{
			//printf("ReflectorStream::PushPacket RTCP packetlen = %"   _U32BITARG_   "\n",packetLen);
			fSockets->GetSocketB()->ProcessPacket(OS::Milliseconds(), std::move(thePacket), 0, 0);
			fSockets->GetSocketB()->Signal(Task::kIdleEvent);
		}
		else
		{
			fSockets->GetSocketA()->ProcessPacket(OS::Milliseconds(), std::move(thePacket), 0, 0);
			fSockets->GetSocketA()->Signal(Task::kIdleEvent);
		}
	}
}

ReflectorSender::ReflectorSender(ReflectorStream* inStream, uint32_t inWriteFlag)
	: fStream(inStream),
	fWriteFlag(inWriteFlag),
	fKeyFrameStartPacketElementPointer(nullptr)
{
}

bool ReflectorSender::ShouldReflectNow()
{
	//check to make sure there actually is work to do for this stream.
	if (!fHasNewPackets)
		return false;
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

void ReflectorSender::ReflectPackets()
{
	int64_t currentTime = OS::Milliseconds();

	//make sure to reset these state variables
	fHasNewPackets = false;

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

	ReflectorPacket *fFirstPacketInQueueForNewOutput = 
		fKeyFrameStartPacketElementPointer ? fKeyFrameStartPacketElementPointer : 
		GetClientBufferStartPacketOffset(std::chrono::seconds(0));

	for (auto &theOutput : fStream->fOutputArray)
	{
		if (false == theOutput->IsPlaying()) continue;
		OSMutexLocker locker(&theOutput->fMutex);
		ReflectorPacket* packetElem = theOutput->GetBookMarkedPacket(fPacketQueue);
		if (packetElem == nullptr) // should only be a new output
			packetElem = fFirstPacketInQueueForNewOutput; // everybody starts at the oldest packet in the buffer delay or uses a bookmark

		packetElem = SendPacketsToOutput(theOutput, packetElem, currentTime);
		if (packetElem)
		{
			ReflectorPacket* thePacket = NeedRelocateBookMark(packetElem);

			thePacket->fNeededByOutput = true; 				// flag to prevent removal in RemoveOldPackets
			theOutput->SetBookMarkPacket(thePacket); 	// store a reference to the packet
		}
	}

	RemoveOldPackets();
}

ReflectorPacket*    ReflectorSender::SendPacketsToOutput(ReflectorOutput* theOutput, ReflectorPacket* currentPacket, int64_t currentTime)
{
	auto it = std::find_if(begin(fPacketQueue), end(fPacketQueue),
		[currentPacket](const std::unique_ptr<ReflectorPacket> &pkt) {
		return pkt.get() == currentPacket;
	});

	QTSS_Error err = QTSS_NoErr;
	for (; it != fPacketQueue.end(); ++it)
	{
		const auto &thePacket = *it;

		//printf("packetLateness %qd, seq# %li\n", packetLateness, (int32_t) DGetPacketSeqNumber( &thePacket->fPacketPtr ) );          

		err = theOutput->WritePacket(thePacket->fPacket, fStream, fWriteFlag, 
			thePacket->fStreamCountID);

		if (err == QTSS_WouldBlock)
		{ 
			break;
		}
	}

	return fPacketQueue.back().get();
}


ReflectorPacket* ReflectorSender::GetClientBufferStartPacketOffset(std::chrono::seconds offset)
{
	auto theCurrentTime = std::chrono::high_resolution_clock::now();

	// more or less what the client over buffer will be
	static constexpr auto sOverBufferInSec = std::chrono::seconds(10); 

	if (offset > sOverBufferInSec)
		offset = sOverBufferInSec;

	for (const auto &thePacket : fPacketQueue)
	{
		auto packetDelay = theCurrentTime - thePacket->fTimeArrived;
		if (packetDelay <= sOverBufferInSec - offset)
			return thePacket.get();
	}

	return nullptr;
}

void    ReflectorSender::RemoveOldPackets()
{
	// Iterate through the senders queue to clear out packets
	// Start at the oldest packet and walk forward to the newest packet
	// 
	auto theCurrentTime = std::chrono::high_resolution_clock::now();
	static constexpr auto sMaxPacketAge = std::chrono::seconds(20);

	for (auto it = fPacketQueue.begin(); it != fPacketQueue.end(); )
	{
		const auto &thePacket = *it;

		auto packetDelay = std::chrono::duration_cast<std::chrono::milliseconds>(theCurrentTime - thePacket->fTimeArrived);

		// walk q and remove packets that are too old
		if (!thePacket->fNeededByOutput && packetDelay > sMaxPacketAge) // delete based on late tolerance and whether a client is blocked on the packet
		{
			printf("erase packet\n");
			// not needed and older than our required buffer
			it = fPacketQueue.erase(it);
		}
		else
		{
			// we want to keep all of these but we should reset the es that should be aged out unless marked
			// as need the next time through reflect packets.
			++it;

			if (fKeyFrameStartPacketElementPointer == thePacket.get())
					break;

			thePacket->fNeededByOutput = false; //mark not needed.. will be set next time through reflect packets
			if (packetDelay <= sMaxPacketAge)  // this packet is going to be kept around as well as the ones that follow.
				break;
		}
	}
}

// if current packet over max packetAgeTime, we need relocate the BookMark to
// the new fKeyFrameStartPacketElementPointer
ReflectorPacket* ReflectorSender::NeedRelocateBookMark(ReflectorPacket* thePacket)
{
	Assert(thePacket);

	auto packetDelay = 
		std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - thePacket->fTimeArrived);

	static constexpr auto sRelocatePacketAge = std::chrono::seconds(1);
	if (packetDelay > sRelocatePacketAge)
	{
		if (fKeyFrameStartPacketElementPointer && fKeyFrameStartPacketElementPointer->fTimeArrived > thePacket->fTimeArrived)
		{
			fStream->GetMyReflectorSession()->SetHasVideoKeyFrameUpdate(true);
			return fKeyFrameStartPacketElementPointer;
		}
	}

	return thePacket;
}

void ReflectorSender::appendPacket(std::unique_ptr<ReflectorPacket> thePacket)
{
	if (!thePacket->IsRTCP())
	{
		auto type = needToUpdateKeyFrame(fStream, *thePacket);
		if (needToUpdateKeyFrame(fStream, *thePacket) != KeyFrameType::None)
		{
			if (fKeyFrameStartPacketElementPointer)
				fKeyFrameStartPacketElementPointer->fNeededByOutput = false;

			thePacket->fNeededByOutput = true;
			fKeyFrameStartPacketElementPointer = thePacket.get();
			if (type == KeyFrameType::Video) 
				fStream->GetMyReflectorSession()->SetHasVideoKeyFrameUpdate(true);
			else 
				fStream->GetMyReflectorSession()->SetHasVideoKeyFrameUpdate(false);
		}
	}

	fHasNewPackets = true;

	if (!(thePacket->IsRTCP()))
	{
		// don't check for duplicate packets, they may be needed to keep in sync.
		// Because this is an RTP packet make sure to atomic add this because
		// multiple sockets can be adding to this variable simultaneously
		fStream->fBytesSentInThisInterval += thePacket->fPacket.size();
	}

	fPacketQueue.push_back(std::move(thePacket));
}

void ReflectorSocketPool::SetUDPSocketOptions(SocketPair<ReflectorSocket>* inPair)
{
	// Fix add ReuseAddr for compatibility with MPEG4IP broadcaster which likes to use the same
	//sockets.  

	//Make sure this works with PlaylistBroadcaster 
	//inPair->GetSocketA()->ReuseAddr();
	//inPair->GetSocketA()->ReuseAddr();

}


SocketPair<ReflectorSocket>* ReflectorSocketPool::GetUDPSocketPair(uint32_t inIPAddr, uint16_t inPort,
	uint32_t inSrcIPAddr, uint16_t inSrcPort)
{
	std::lock_guard<std::mutex> locker(fMutex);
	if ((inSrcIPAddr != 0) || (inSrcPort != 0))
	{
		for (const auto &theElem : fUDPQueue)
		{
			//If we find a pair that is a) on the right IP address, and b) doesn't
			//have this source IP & port in the demuxer already, we can return this pair
			if ((theElem->GetSocketA()->GetLocalAddr() == inIPAddr) &&
				((inPort == 0) || (theElem->GetSocketA()->GetLocalPort() == inPort)))
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

SocketPair<ReflectorSocket>*  ReflectorSocketPool::CreateUDPSocketPair(uint32_t inAddr, uint16_t inPort)
{
	//try to find an open pair of ports to bind these suckers tooo
	std::lock_guard<std::mutex> locker(fMutex);
	SocketPair<ReflectorSocket>* theElem = nullptr;
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
		if (theElem->GetSocketA()->Open() != OS_NoErr)
		{
			this->DestructUDPSocketPair(theElem);
			return nullptr;
		}
		if (theElem->GetSocketB()->Open() != OS_NoErr)
		{
			this->DestructUDPSocketPair(theElem);
			return nullptr;
		}

		// Set socket options on these new sockets
		this->SetUDPSocketOptions(theElem);

		OS_Error theErr = theElem->GetSocketA()->Bind(inAddr, curPort);
		if (theErr == OS_NoErr)
		{   //printf("fSocketA->Bind ok on port%u\n", curPort);
			theErr = theElem->GetSocketB()->Bind(inAddr, socketBPort);
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

void ReflectorSocketPool::ReleaseUDPSocketPair(SocketPair<ReflectorSocket>* inPair)
{
	std::lock_guard<std::mutex> locker(fMutex);
	inPair->fRefCount--;
	if (inPair->fRefCount == 0)
	{
		auto it = std::find(fUDPQueue.begin(), fUDPQueue.end(), inPair);
		if (it != fUDPQueue.end())
			fUDPQueue.erase(it);
		DestructUDPSocketPair(inPair);
	}
}

SocketPair<ReflectorSocket>* ReflectorSocketPool::ConstructUDPSocketPair()
{
	return new SocketPair<ReflectorSocket>();
}

void ReflectorSocketPool::DestructUDPSocketPair(SocketPair<ReflectorSocket> *inPair)
{
	delete inPair;
}

ReflectorSocket::ReflectorSocket()
	: IdleTask(),
	UDPSocket(nullptr, Socket::kNonBlockingSocketType)
{
	//construct all the preallocated packets
	this->SetTaskName("ReflectorSocket");
	this->SetTask(this);
}

void    ReflectorSocket::AddSender(ReflectorSender* inSender)
{
	Assert(true == fDemuxer.RegisterTask(
	{ inSender->fStream->fStreamInfo.fSrcIPAddr, 0 }, inSender));
	fSenderQueue.push_back(inSender);
}

void    ReflectorSocket::RemoveSender(ReflectorSender* inSender)
{
	auto it = std::find(fSenderQueue.begin(), fSenderQueue.end(), inSender);
	if (it != fSenderQueue.end())
		fSenderQueue.erase(it);
	fDemuxer.UnregisterTask({ inSender->fStream->fStreamInfo.fSrcIPAddr, 0 });
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

	int64_t theMilliseconds = OS::Milliseconds();

	//Only check for data on the socket if we've actually been notified to that effect
	if (theEvents & Task::kReadEvent)
		this->GetIncomingData(theMilliseconds);

	//Now that we've gotten all available packets, have the streams reflect
	for (const auto &theSender2 : fSenderQueue)
		if (theSender2 != nullptr && theSender2->ShouldReflectNow())
			theSender2->ReflectPackets();

	//For smoothing purposes, the streams can mark when they want to wakeup.
	this->SetIdleTimer(1000);

	return 0;
}

bool ReflectorSocket::ProcessPacket(int64_t inMilliseconds, std::unique_ptr<ReflectorPacket> thePacket, uint32_t theRemoteAddr, uint16_t theRemotePort)
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
				done = true;
				break;
			}
		}

		// Find the appropriate ReflectorSender for this packet.
		ReflectorSender* theSender = fDemuxer.GetTask({ theRemoteAddr, 0 });
		// If there is a generic sender for this socket, use it.
		if (theSender == nullptr)
			theSender = fDemuxer.GetTask({ 0, 0 });

		if (theSender == nullptr)
		{
			//uint16_t* theSeqNumberP = (uint16_t*)thePacket->fPacketPtr.Ptr;
			//printf("ReflectorSocket::ProcessPacket no sender found for packet! sequence number=%d\n",ntohs(theSeqNumberP[1]));
			done = true;
			break;
		}

		Assert(theSender != nullptr); // at this point we have a sender

		thePacket->fStreamCountID = ++(theSender->fStream->fPacketCount);
		thePacket->fTimeArrived = std::chrono::high_resolution_clock::now();
		theSender->appendPacket(std::move(thePacket));
	} while (false);

	return done;
}


void ReflectorSocket::GetIncomingData(int64_t inMilliseconds)
{
	uint32_t theRemoteAddr = 0;
	uint16_t theRemotePort = 0;
	//get all the outstanding packets for this socket
	while (true)
	{
		//get a packet off the free queue.
		
		static const size_t kMaxReflectorPacketSize = 2060;
		static char fPacketPtr[kMaxReflectorPacketSize];
		size_t fPacketLen;
		(void)this->RecvFrom(&theRemoteAddr, &theRemotePort, fPacketPtr,
			kMaxReflectorPacketSize, &fPacketLen);

		auto thePacket = std::make_unique<ReflectorPacket>(fPacketPtr, fPacketLen);
		if (this->ProcessPacket(inMilliseconds, std::move(thePacket), theRemoteAddr, theRemotePort))
			break;

		//printf("ReflectorSocket::GetIncomingData \n");
	}

}
