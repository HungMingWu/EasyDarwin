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
	 File:       ReflectorStream.h

	 Contains:   This object supports reflecting an RTP multicast stream to N
				 RTPStreams. It spaces out the packet send times in order to
				 maximize the randomness of the sending pattern and smooth
				 the stream.
 */

#ifndef _REFLECTOR_STREAM_H_
#define _REFLECTOR_STREAM_H_

#include <atomic>
#include <chrono>
#include "QTSS.h"

#include "IdleTask.h"
#include "SDPSourceInfo.h"

#include "UDPSocket.h"
#include "UDPSocketPool.h"
#include "SequenceNumberMap.h"

#include "OSMutex.h"
#include "OSQueue.h"
#include "OSRef.h"

#include "RTCPSRPacket.h"
#include "ReflectorOutput.h"

 /*fantasy add this*/
#include "keyframecache.h"

//This will add some printfs that are useful for checking the thinning
#define REFLECTOR_THINNING_DEBUGGING 0 
#define MAX_CACHE_SIZE  1024*1024*2

class ReflectorStream;
class ReflectorSession;
class ReflectorSender;

//Custom UDP socket classes for doing reflector packet retrieval, socket management
class ReflectorSocket : public IdleTask, public UDPSocket
{
	using time_point = std::chrono::high_resolution_clock::time_point;
public:

	ReflectorSocket();
	~ReflectorSocket() override = default;
	void    AddBroadcasterSession(RTPSession* inSession) { fBroadcasterClientSession = inSession; }
	void    RemoveBroadcasterSession() { fBroadcasterClientSession = nullptr; }
	void    AddSender(ReflectorSender* inSender);
	void    RemoveSender(ReflectorSender* inStreamElem);
	bool  HasSender() { return !fDemuxer.empty(); }
	bool  ProcessPacket(time_point now, std::unique_ptr<MyReflectorPacket> thePacket, uint32_t theRemoteAddr, uint16_t theRemotePort);
	int64_t      Run() override;
private:

	//virtual int64_t        Run();
	void    GetIncomingData(time_point now);

	//Number of packets to allocate when the socket is first created
	enum
	{
		kSSRCTimeOut = 30000 // milliseconds before clearing the SSRC if no new ssrcs have come in
	};
	RTPSession*                  fBroadcasterClientSession{nullptr};
	time_point                   fLastBroadcasterTimeOutRefresh;
	// Queue of senders
	std::list<ReflectorSender*> fSenderQueue;

	uint32_t  fValidSSRC{0};
	int64_t  fLastValidSSRCTime{0};

	bool  fHasReceiveTime{false};
	uint64_t  fFirstReceiveTime{0};
	int64_t  fFirstArrivalTime{0};
	uint32_t  fCurrentSSRC{0};
	SyncUnorderMap<ReflectorSender*> fDemuxer;
};


class ReflectorSocketPool
{
	enum
	{
		kLowestUDPPort = 6970,  //uint16_t
		kHighestUDPPort = 65535 //uint16_t
	};

	std::list<SocketPair<ReflectorSocket>*> fUDPQueue;
	std::mutex fMutex;
public:

	ReflectorSocketPool() = default;
	~ReflectorSocketPool() = default;

	const std::list<SocketPair<ReflectorSocket>*>& GetSocketQueue() { return fUDPQueue; }

	//Gets a UDP socket out of the pool. 
	//inIPAddr = IP address you'd like this pair to be bound to.
	//inPort = port you'd like this pair to be bound to, or 0 if you don't care
	//inSrcIPAddr = srcIP address of incoming packets for the demuxer.
	//inSrcPort = src port of incoming packets for the demuxer.
	//This may return NULL if no pair is available that meets the criteria.
	SocketPair<ReflectorSocket>*  GetUDPSocketPair(uint32_t inIPAddr, uint16_t inPort,
		uint32_t inSrcIPAddr, uint16_t inSrcPort);

	//When done using a UDP socket pair retrieved via GetUDPSocketPair, you must
	//call this function. Doing so tells the pool which UDP sockets are in use,
	//keeping the number of UDP sockets allocated at a minimum.
	void ReleaseUDPSocketPair(SocketPair<ReflectorSocket>* inPair);

	SocketPair<ReflectorSocket>*  CreateUDPSocketPair(uint32_t inAddr, uint16_t inPort);

	SocketPair<ReflectorSocket>*  ConstructUDPSocketPair();
	void            DestructUDPSocketPair(SocketPair<ReflectorSocket> *inPair);
	void            SetUDPSocketOptions(SocketPair<ReflectorSocket>* inPair);
};

class ReflectorSender
{
public:
	ReflectorSender(ReflectorStream* inStream, uint32_t inWriteFlag);
	~ReflectorSender() = default;

	int64_t  fSleepTime;

	//We want to make sure that ReflectPackets only gets invoked when there
	//is actually work to do, because it is an expensive function
	bool      ShouldReflectNow();

	//This function gets data from the multicast source and reflects.
	//Returns the time at which it next needs to be invoked
	void        ReflectPackets();

	MyReflectorPacket*    SendPacketsToOutput(ReflectorOutput* theOutput, MyReflectorPacket* currentPacket);

	void        RemoveOldPackets();
	MyReflectorPacket* GetClientBufferStartPacketOffset(std::chrono::seconds offset);

	MyReflectorPacket* NeedRelocateBookMark(MyReflectorPacket* thePacket);

	ReflectorStream*    fStream;
	uint32_t              fWriteFlag;

	std::list<std::unique_ptr<MyReflectorPacket>> fPacketQueue;
	MyReflectorPacket*	fKeyFrameStartPacketElementPointer{ nullptr };//最新关键帧指针

	//these serve as an optimization, keeping track of when this
	//sender needs to run so it doesn't run unnecessarily

	bool      fHasNewPackets{ false };

	std::chrono::high_resolution_clock::time_point fLastRRTime;
	void appendPacket(std::unique_ptr<MyReflectorPacket> thePacket);
	friend class ReflectorSocket;
	friend class ReflectorStream;
};

class ReflectorStream
{
	using time_point = std::chrono::high_resolution_clock::time_point;
public:

	enum
	{
		// A ReflectorStream is uniquely identified by the
		// destination IP address & destination port of the broadcast.
		// This ID simply contains that information.
		//
		// A unicast broadcast can also be identified by source IP address. If
		// you are attempting to demux by source IP, this ID will not guarentee
		// uniqueness and special care should be used.
		kStreamIDSize = sizeof(uint32_t) + sizeof(uint16_t)
	};

	ReflectorStream(StreamInfo* inInfo);
	~ReflectorStream();

	//
	// MODIFIERS

	// Call this to initialize the reflector sockets. Uses the QTSS_RTSPRequestObject
	// if provided to report any errors that occur 
	// Passes the QTSS_ClientSessionObject to the socket so the socket can update the session if needed.
	QTSS_Error BindSockets(RTSPRequest *inRequest, RTPSession* inSession, uint32_t inReflectorSessionFlags, bool filterState, uint32_t timeout);

	// This stream reflects packets from the broadcast to specific ReflectorOutputs.
	// You attach outputs to ReflectorStreams this way. You can force the ReflectorStream
	// to put this output into a certain bucket by passing in a certain bucket index.
	// Pass in -1 if you don't care. AddOutput returns the bucket index this output was
	// placed into, or -1 on an error.

	void  AddOutput(ReflectorOutput* inOutput);

	// Removes the specified output from this ReflectorStream.
	void    RemoveOutput(ReflectorOutput* inOutput); // Removes this output from all tracks

	void	TearDownAllOutputs(); // causes a tear down and then a remove

	// If the incoming data is RTSP interleaved, packets for this stream are identified
	// by channel numbers
	void	SetRTPChannelNum(int16_t inChannel) { fRTPChannel = inChannel; }
	void	SetRTCPChannelNum(int16_t inChannel) { fRTCPChannel = inChannel; }
	void	PushPacket(char *packet, size_t packetLen, bool isRTCP);

	//
	// ACCESSORS
	uint32_t                  GetBitRate() { return fCurrentBitRate; }
	StreamInfo* GetStreamInfo() { return &fStreamInfo; }
	OSMutex*                GetMutex() { return &fBucketMutex; }
	void*                   GetStreamCookie() { return this; }
	int16_t                  GetRTPChannel() { return fRTPChannel; }
	int16_t                  GetRTCPChannel() { return fRTCPChannel; }
	SocketPair<ReflectorSocket>*          GetSocketPair() { return fSockets; }
	ReflectorSender*        GetRTPSender() { return &fRTPSender; }
	ReflectorSender*        GetRTCPSender() { return &fRTCPSender; }

	uint64_t                  fPacketCount{ 0 };

	inline  void                    UpdateBitRate(time_point currentTime);

	void                    IncEyeCount() { OSMutexLocker locker(&fBucketMutex); fEyeCount++; }
	void                    DecEyeCount() { OSMutexLocker locker(&fBucketMutex); fEyeCount--; }
	uint32_t                  GetEyeCount() { OSMutexLocker locker(&fBucketMutex); return fEyeCount; }

	void					SetMyReflectorSession(ReflectorSession* reflector) { fMyReflectorSession = reflector; }
	ReflectorSession*		GetMyReflectorSession() { return fMyReflectorSession; }

private:

	//Sends an RTCP receiver report to the broadcast source
	void    SendReceiverReport();

	// Reflector sockets, retrieved from the socket pool
	SocketPair<ReflectorSocket>*      fSockets;

	QTSS_RTPTransportType fTransportType{ qtssRTPTransportTypeTCP };

	ReflectorSender     fRTPSender;
	ReflectorSender     fRTCPSender;
	SequenceNumberMap   fSequenceNumberMap; //for removing duplicate packets

	// All the necessary info about this stream
	StreamInfo  fStreamInfo;

	enum
	{
		kReceiverReportSize = 16,               //uint32_t
		kAppSize = 36,                          //uint32_t
		kMinNumBuckets = 16,                    //uint32_t
	};

	// BUCKET ARRAY
	//ReflectorOutputs are kept in a 1-dimensional array
	std::vector<ReflectorOutput*>     fOutputArray;

	//Bucket array can't be modified while we are sending packets.
	OSMutex     fBucketMutex;

	// RTCP RR information

	char        fReceiverReportBuffer[kReceiverReportSize + kAppSize +
		RTCPSRPacket::kMaxCNameLen];
	uint32_t*     fEyeLocation;//place in the buffer to write the eye information
	uint32_t      fReceiverReportSize;

	// This is the destination address & port for RTCP
	// receiver reports.
	uint32_t      fDestRTCPAddr;
	uint16_t      fDestRTCPPort;

	// Used for calculating average bit rate
	uint32_t              fCurrentBitRate;
	time_point            fLastBitRateSample;
	
	std::atomic_size_t   fBytesSentInThisInterval;// unsigned int because we need to atomic_add 

	// If incoming data is RTSP interleaved
	int16_t              fRTPChannel; //These will be -1 if not set to anything
	int16_t              fRTCPChannel;

	uint32_t              fEyeCount;

	ReflectorSession*	fMyReflectorSession;

	static uint32_t       sBucketSize;
	static uint32_t       sMaxFuturePacketSec;

	static uint32_t       sMaxFuturePacketMSec;
	static uint32_t       sOverBufferInSec;
	static uint32_t       sFirstPacketOffsetMsec;

	friend class ReflectorSocket;
	friend class ReflectorSender;

public:
	CKeyFrameCache*		pkeyFrameCache;
};


void    ReflectorStream::UpdateBitRate(time_point currentTime)
{
	static constexpr auto kBitRateAvgInterval = std::chrono::milliseconds(30000); // time between bitrate averages
	if (std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - fLastBitRateSample) > kBitRateAvgInterval)
	{
		unsigned int intervalBytes = fBytesSentInThisInterval;
		fBytesSentInThisInterval -= intervalBytes;

		// Multiply by 1000 to convert from milliseconds to seconds, and by 8 to convert from bytes to bits
		float bps = (float)(intervalBytes * 8 * 1000) / std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - fLastBitRateSample).count();
		fCurrentBitRate = (uint32_t)bps;

		// Don't check again for awhile!
		fLastBitRateSample = currentTime;
	}
}

#endif //_REFLECTOR_SESSION_H_

