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

#include "QTSS.h"

#include "IdleTask.h"
#include "SourceInfo.h"

#include "UDPSocket.h"
#include "UDPSocketPool.h"
#include "UDPDemuxer.h"
#include "SequenceNumberMap.h"

#include "OSMutex.h"
#include "OSQueue.h"
#include "OSRef.h"

#include "RTCPSRPacket.h"
#include "ReflectorOutput.h"
#include "atomic.h"

 /*fantasy add this*/
#include "keyframecache.h"

//This will add some printfs that are useful for checking the thinning
#define REFLECTOR_THINNING_DEBUGGING 0 
#define MAX_CACHE_SIZE  1024*1024*2

//Define to use new potential workaround for NAT problems
#define NAT_WORKAROUND 1

typedef struct FU_Indicator_tag
{
	unsigned char F : 1;
	unsigned char nRI : 2;
	unsigned char type : 5;//
}FAU_Indicator;

typedef struct FU_Head_tag
{
	unsigned char nalu_type : 5;//little 5 bit
	unsigned char r : 1;
	unsigned char e : 1;
	unsigned char s : 1;//high bit    
}FU_Head;


class ReflectorPacket;
class ReflectorSender;
class ReflectorStream;
class RTPSessionOutput;
class ReflectorSession;

class ReflectorPacket
{
public:

	ReflectorPacket() : fQueueElem() { fQueueElem.SetEnclosingObject(this); this->Reset(); }
	void Reset() { // make packet ready to reuse fQueueElem is always in use
		fBucketsSeenThisPacket = 0;
		fTimeArrived = 0;
		//fQueueElem -- should be set to this
		fPacketPtr.Set(fPacketData, 0);
		fIsRTCP = false;
		fStreamCountID = 0;
		fNeededByOutput = false;
	}

	~ReflectorPacket() {}

	void    SetPacketData(char *data, uint32_t len)
	{
		Assert(kMaxReflectorPacketSize > len);

		if (len > kMaxReflectorPacketSize)
			len = kMaxReflectorPacketSize;

		if (len > 0)
			memcpy(this->fPacketPtr.Ptr, data, len);
		this->fPacketPtr.Len = len;
	}

	bool  IsRTCP() { return fIsRTCP; }
	inline  uint32_t  GetPacketRTPTime();
	inline  uint16_t  GetPacketRTPSeqNum();
	inline  uint32_t  GetSSRC(bool isRTCP);
	inline  int64_t  GetPacketNTPTime();

private:

	enum
	{
		kMaxReflectorPacketSize = 2060    //jm 5/02 increased from 2048 by 12 bytes for test bytes appended to packets
	};

	uint32_t      fBucketsSeenThisPacket;
	int64_t      fTimeArrived;
	OSQueueElem fQueueElem;
	char        fPacketData[kMaxReflectorPacketSize];
	StrPtrLen   fPacketPtr;
	bool      fIsRTCP;
	bool      fNeededByOutput; // is this packet still needed for output?
	uint64_t      fStreamCountID;

	friend class ReflectorSender;
	friend class ReflectorSocket;
	friend class RTPSessionOutput;


};

uint32_t ReflectorPacket::GetSSRC(bool isRTCP)
{
	if (fPacketPtr.Ptr == nullptr || fPacketPtr.Len < 8)
		return 0;

	uint32_t* theSsrcPtr = (uint32_t*)fPacketPtr.Ptr;
	if (isRTCP)// RTCP 
		return ntohl(theSsrcPtr[1]);

	if (fPacketPtr.Len < 12)
		return 0;

	return ntohl(theSsrcPtr[2]);  // RTP SSRC
}

uint32_t ReflectorPacket::GetPacketRTPTime()
{

	uint32_t timestamp = 0;
	if (!fIsRTCP)
	{
		//The RTP timestamp number is the second long of the packet
		if (fPacketPtr.Ptr == nullptr || fPacketPtr.Len < 8)
			return 0;
		timestamp = ntohl(((uint32_t*)fPacketPtr.Ptr)[1]);
	}
	else
	{
		if (fPacketPtr.Ptr == nullptr || fPacketPtr.Len < 20)
			return 0;
		timestamp = ntohl(((uint32_t*)fPacketPtr.Ptr)[4]);
	}
	return timestamp;
}

uint16_t ReflectorPacket::GetPacketRTPSeqNum()
{
	Assert(!fIsRTCP); // not a supported type

	if (fPacketPtr.Ptr == nullptr || fPacketPtr.Len < 4 || fIsRTCP)
		return 0;

	uint16_t sequence = ntohs(((uint16_t*)fPacketPtr.Ptr)[1]); //The RTP sequenc number is the second short of the packet
	return sequence;
}


int64_t  ReflectorPacket::GetPacketNTPTime()
{
	Assert(fIsRTCP); // not a supported type
	if (fPacketPtr.Ptr == nullptr || fPacketPtr.Len < 16 || !fIsRTCP)
		return 0;

	uint32_t* theReport = (uint32_t*)fPacketPtr.Ptr;
	theReport += 2;
	int64_t ntp = 0;
	::memcpy(&ntp, theReport, sizeof(int64_t));

	return OS::Time1900Fixed64Secs_To_TimeMilli(OS::NetworkToHostSInt64(ntp));


}


//Custom UDP socket classes for doing reflector packet retrieval, socket management
class ReflectorSocket : public IdleTask, public UDPSocket
{
public:

	ReflectorSocket();
	~ReflectorSocket() override;
	void    AddBroadcasterSession(QTSS_ClientSessionObject inSession) { OSMutexLocker locker(this->GetDemuxer()->GetMutex()); fBroadcasterClientSession = inSession; }
	void    RemoveBroadcasterSession(QTSS_ClientSessionObject inSession) { OSMutexLocker locker(this->GetDemuxer()->GetMutex()); if (inSession == fBroadcasterClientSession) fBroadcasterClientSession = nullptr; }
	void    AddSender(ReflectorSender* inSender);
	void    RemoveSender(ReflectorSender* inStreamElem);
	bool  HasSender() { return (this->GetDemuxer()->GetHashTable()->GetNumEntries() > 0); }
	bool  ProcessPacket(const int64_t& inMilliseconds, ReflectorPacket* thePacket, uint32_t theRemoteAddr, uint16_t theRemotePort);
	ReflectorPacket*    GetPacket();
	int64_t      Run() override;
	void    SetSSRCFilter(bool state, uint32_t timeoutSecs) { fFilterSSRCs = state; fTimeoutSecs = timeoutSecs; }
private:

	//virtual int64_t        Run();
	void    GetIncomingData(const int64_t& inMilliseconds);
	void    FilterInvalidSSRCs(ReflectorPacket* thePacket, bool isRTCP);

	//Number of packets to allocate when the socket is first created
	enum
	{
		kNumPreallocatedPackets = 20,   //uint32_t
		kRefreshBroadcastSessionIntervalMilliSecs = 10000,
		kSSRCTimeOut = 30000 // milliseconds before clearing the SSRC if no new ssrcs have come in
	};
	QTSS_ClientSessionObject    fBroadcasterClientSession;
	int64_t                      fLastBroadcasterTimeOutRefresh;
	// Queue of available ReflectorPackets
	OSQueue fFreeQueue;
	// Queue of senders
	OSQueue fSenderQueue;
	int64_t  fSleepTime;

	uint32_t  fValidSSRC;
	int64_t  fLastValidSSRCTime;
	bool  fFilterSSRCs;
	uint32_t  fTimeoutSecs;

	bool  fHasReceiveTime;
	uint64_t  fFirstReceiveTime;
	int64_t  fFirstArrivalTime;
	uint32_t  fCurrentSSRC;

};


class ReflectorSocketPool : public UDPSocketPool
{
public:

	ReflectorSocketPool() {}
	~ReflectorSocketPool() override {}

	UDPSocketPair*  ConstructUDPSocketPair() override;
	void            DestructUDPSocketPair(UDPSocketPair *inPair) override;
	void            SetUDPSocketOptions(UDPSocketPair* inPair) override;
	void                    DestructUDPSocket(ReflectorSocket* socket);


};

class ReflectorSender : public UDPDemuxerTask
{
public:
	ReflectorSender(ReflectorStream* inStream, uint32_t inWriteFlag);
	~ReflectorSender() override;
	// Queue of senders
	OSQueue fSenderQueue;
	int64_t  fSleepTime;

	//Used for adjusting sequence numbers in light of thinning
	uint16_t      GetPacketSeqNumber(const StrPtrLen& inPacket);
	void        SetPacketSeqNumber(const StrPtrLen& inPacket, uint16_t inSeqNumber);
	bool      PacketShouldBeThinned(QTSS_RTPStreamObject inStream, const StrPtrLen& inPacket);

	//We want to make sure that ReflectPackets only gets invoked when there
	//is actually work to do, because it is an expensive function
	bool      ShouldReflectNow(const int64_t& inCurrentTime, int64_t* ioWakeupTime);

	//This function gets data from the multicast source and reflects.
	//Returns the time at which it next needs to be invoked
	void        ReflectPackets(int64_t* ioWakeupTime, OSQueue* inFreeQueue);

	//this is the old way of doing reflect packets. It is only here until the relay code can be cleaned up.
	void        ReflectRelayPackets(int64_t* ioWakeupTime, OSQueue* inFreeQueue);

	OSQueueElem*    SendPacketsToOutput(ReflectorOutput* theOutput, OSQueueElem* currentPacket, int64_t currentTime, int64_t  bucketDelay, bool firstPacket);

	uint32_t      GetOldestPacketRTPTime(bool *foundPtr);
	uint16_t      GetFirstPacketRTPSeqNum(bool *foundPtr);
	bool      GetFirstPacketInfo(uint16_t* outSeqNumPtr, uint32_t* outRTPTimePtr, int64_t* outArrivalTimePtr);

	OSQueueElem*GetClientBufferNextPacketTime(uint32_t inRTPTime);
	bool      GetFirstRTPTimePacket(uint16_t* outSeqNumPtr, uint32_t* outRTPTimePtr, int64_t* outArrivalTimePtr);

	void        RemoveOldPackets(OSQueue* inFreeQueue);
	OSQueueElem* GetClientBufferStartPacketOffset(int64_t offsetMsec, bool needKeyFrameFirstPacket = false);
	OSQueueElem* GetClientBufferStartPacket() { return this->GetClientBufferStartPacketOffset(0); };

	// ->geyijyn@20150427
	// 关键帧索引及丢帧方案
	OSQueueElem* NeedRelocateBookMark(OSQueueElem* currentElem);
	OSQueueElem* GetNewestKeyFrameFirstPacket(OSQueueElem* currentElem, int64_t offsetMsec);
	bool IsKeyFrameFirstPacket(ReflectorPacket* thePacket);
	bool IsFrameFirstPacket(ReflectorPacket* thePacket);
	bool IsFrameLastPacket(ReflectorPacket* thePacket);

	ReflectorStream*    fStream;
	uint32_t              fWriteFlag;

	OSQueue         fPacketQueue;
	OSQueueElem*    fFirstNewPacketInQueue;
	OSQueueElem*    fFirstPacketInQueueForNewOutput;
	OSQueueElem*	fKeyFrameStartPacketElementPointer;//最新关键帧指针

	//these serve as an optimization, keeping track of when this
	//sender needs to run so it doesn't run unnecessarily

	inline void SetNextTimeToRun(int64_t nextTime)
	{
		fNextTimeToRun = nextTime;
		//printf("SetNextTimeToRun =%"_64BITARG_"d\n", fNextTimeToRun);
	}

	bool      fHasNewPackets;
	int64_t      fNextTimeToRun;

	//how often to send RRs to the source
	enum
	{
		kRRInterval = 5000      //int64_t (every 5 seconds)
	};

	int64_t      fLastRRTime;
	OSQueueElem fSocketQueueElem;

	friend class ReflectorSocket;
	friend class ReflectorStream;
};

class ReflectorStream
{
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

	// Uses a StreamInfo to generate a unique ID
	static void GenerateSourceID(SourceInfo::StreamInfo* inInfo, char* ioBuffer);

	ReflectorStream(SourceInfo::StreamInfo* inInfo);
	~ReflectorStream();

	//
	// SETUP
	//
	// Call Register from the Register role, as this object has some QTSS API
	// attributes to setup
	static void Register();
	static void Initialize(QTSS_ModulePrefsObject inPrefs);

	//
	// MODIFIERS

	// Call this to initialize the reflector sockets. Uses the QTSS_RTSPRequestObject
	// if provided to report any errors that occur 
	// Passes the QTSS_ClientSessionObject to the socket so the socket can update the session if needed.
	QTSS_Error BindSockets(QTSS_StandardRTSP_Params* inParams, uint32_t inReflectorSessionFlags, bool filterState, uint32_t timeout);

	// This stream reflects packets from the broadcast to specific ReflectorOutputs.
	// You attach outputs to ReflectorStreams this way. You can force the ReflectorStream
	// to put this output into a certain bucket by passing in a certain bucket index.
	// Pass in -1 if you don't care. AddOutput returns the bucket index this output was
	// placed into, or -1 on an error.

	int32_t  AddOutput(ReflectorOutput* inOutput, int32_t putInThisBucket);

	// Removes the specified output from this ReflectorStream.
	void    RemoveOutput(ReflectorOutput* inOutput); // Removes this output from all tracks

	void	TearDownAllOutputs(); // causes a tear down and then a remove

	// If the incoming data is RTSP interleaved, packets for this stream are identified
	// by channel numbers
	void	SetRTPChannelNum(int16_t inChannel) { fRTPChannel = inChannel; }
	void	SetRTCPChannelNum(int16_t inChannel) { fRTCPChannel = inChannel; }
	void	PushPacket(char *packet, uint32_t packetLen, bool isRTCP);

	//
	// ACCESSORS
	uint32_t                  GetBitRate() { return fCurrentBitRate; }
	SourceInfo::StreamInfo* GetStreamInfo() { return &fStreamInfo; }
	OSMutex*                GetMutex() { return &fBucketMutex; }
	void*                   GetStreamCookie() { return this; }
	int16_t                  GetRTPChannel() { return fRTPChannel; }
	int16_t                  GetRTCPChannel() { return fRTCPChannel; }
	UDPSocketPair*          GetSocketPair() { return fSockets; }
	ReflectorSender*        GetRTPSender() { return &fRTPSender; }
	ReflectorSender*        GetRTCPSender() { return &fRTCPSender; }

	void                    SetHasFirstRTCP(bool hasPacket) { fHasFirstRTCPPacket = hasPacket; }
	bool                  HasFirstRTCP() { return fHasFirstRTCPPacket; }

	void                    SetFirst_RTCP_RTP_Time(uint32_t time) { fFirst_RTCP_RTP_Time = time; }
	uint32_t                  GetFirst_RTCP_RTP_Time() { return fFirst_RTCP_RTP_Time; }

	void                    SetFirst_RTCP_Arrival_Time(int64_t time) { fFirst_RTCP_Arrival_Time = time; }
	int64_t                  GetFirst_RTCP_Arrival_Time() { return fFirst_RTCP_Arrival_Time; }


	void                    SetHasFirstRTP(bool hasPacket) { fHasFirstRTPPacket = hasPacket; }
	bool                  HasFirstRTP() { return fHasFirstRTPPacket; }

	uint32_t                  GetBufferDelay() { return ReflectorStream::sOverBufferInMsec; }
	uint32_t                  GetTimeScale() { return fStreamInfo.fTimeScale; }
	uint64_t                  fPacketCount;

	void                    SetEnableBuffer(bool enableBuffer) { fEnableBuffer = enableBuffer; }
	bool                  BufferEnabled() { return fEnableBuffer; }
	inline  void                    UpdateBitRate(int64_t currentTime);
	static uint32_t           sOverBufferInMsec;

	void                    IncEyeCount() { OSMutexLocker locker(&fBucketMutex); fEyeCount++; }
	void                    DecEyeCount() { OSMutexLocker locker(&fBucketMutex); fEyeCount--; }
	uint32_t                  GetEyeCount() { OSMutexLocker locker(&fBucketMutex); return fEyeCount; }

	void					SetMyReflectorSession(ReflectorSession* reflector) { fMyReflectorSession = reflector; }
	ReflectorSession*		GetMyReflectorSession() { return fMyReflectorSession; }

private:

	//Sends an RTCP receiver report to the broadcast source
	void    SendReceiverReport();
	void    AllocateBucketArray(uint32_t inNumBuckets);
	int32_t  FindBucket();

	// Reflector sockets, retrieved from the socket pool
	UDPSocketPair*      fSockets;

	QTSS_RTPTransportType fTransportType;

	ReflectorSender     fRTPSender;
	ReflectorSender     fRTCPSender;
	SequenceNumberMap   fSequenceNumberMap; //for removing duplicate packets

	// All the necessary info about this stream
	SourceInfo::StreamInfo  fStreamInfo;

	enum
	{
		kReceiverReportSize = 16,               //uint32_t
		kAppSize = 36,                          //uint32_t
		kMinNumBuckets = 16,                    //uint32_t
		kBitRateAvgIntervalInMilSecs = 30000 // time between bitrate averages
	};

	// BUCKET ARRAY
	//ReflectorOutputs are kept in a 2-dimensional array, "Buckets"
	typedef ReflectorOutput** Bucket;
	Bucket*     fOutputArray;

	uint32_t      fNumBuckets;        //Number of buckets currently
	uint32_t      fNumElements;       //Number of reflector outputs in the array

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
	int64_t              fLastBitRateSample;
	
	unsigned int        fBytesSentInThisInterval;// unsigned int because we need to atomic_add 

	// If incoming data is RTSP interleaved
	int16_t              fRTPChannel; //These will be -1 if not set to anything
	int16_t              fRTCPChannel;

	bool              fHasFirstRTCPPacket;
	bool              fHasFirstRTPPacket;

	bool              fEnableBuffer;
	uint32_t              fEyeCount;

	uint32_t              fFirst_RTCP_RTP_Time;
	int64_t              fFirst_RTCP_Arrival_Time;

	ReflectorSession*	fMyReflectorSession;

	static uint32_t       sBucketSize;
	static uint32_t       sMaxPacketAgeMSec;
	static uint32_t       sMaxFuturePacketSec;

	static uint32_t       sMaxFuturePacketMSec;
	static uint32_t       sOverBufferInSec;
	static uint32_t       sBucketDelayInMsec;
	static bool       sUsePacketReceiveTime;
	static uint32_t       sFirstPacketOffsetMsec;

	static uint32_t       sRelocatePacketAgeMSec;

	friend class ReflectorSocket;
	friend class ReflectorSender;

public:
	CKeyFrameCache*		pkeyFrameCache;
};


void    ReflectorStream::UpdateBitRate(int64_t currentTime)
{
	if ((fLastBitRateSample + ReflectorStream::kBitRateAvgIntervalInMilSecs) < currentTime)
	{
		unsigned int intervalBytes = fBytesSentInThisInterval;
		(void)atomic_sub(&fBytesSentInThisInterval, intervalBytes);

		// Multiply by 1000 to convert from milliseconds to seconds, and by 8 to convert from bytes to bits
		float bps = (float)(intervalBytes * 8) / (float)(currentTime - fLastBitRateSample);
		bps *= 1000;
		fCurrentBitRate = (uint32_t)bps;

		// Don't check again for awhile!
		fLastBitRateSample = currentTime;
	}
}
#endif //_REFLECTOR_SESSION_H_

