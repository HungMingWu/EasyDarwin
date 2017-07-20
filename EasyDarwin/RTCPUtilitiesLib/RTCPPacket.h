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
	File:       RTCPPacket.h

	Contains:   RTCPReceiverPacket de-packetizing classes


*/

//#define DEBUG_RTCP_PACKETS 1


#ifndef _RTCPPACKET_H_
#define _RTCPPACKET_H_

#include <stdlib.h>
#include <stdint.h>
#ifndef __Win32__
#include <sys/types.h>
#include <netinet/in.h>
#endif

#include "OSHeaders.h"

class RTCPPacket
{
public:

	// Packet types
	enum
	{
		kReceiverPacketType = 201,  //uint32_t
		kSDESPacketType = 202,  //uint32_t
		kAPPPacketType = 204   //uint32_t
	};


	RTCPPacket() : fReceiverPacketBuffer(NULL) {}
	virtual ~RTCPPacket() {}

	//Call this before any accessor method. Returns true if successful, false otherwise
	bool ParsePacket(uint8_t* inPacketBuffer, uint32_t inPacketLen);

	inline int GetVersion();
	inline bool GetHasPadding();
	inline int GetReportCount();
	inline uint8_t GetPacketType();
	inline uint16_t GetPacketLength();    //in 32-bit words
	inline uint32_t GetPacketSSRC();
	inline int16_t GetHeader();
	uint8_t* GetPacketBuffer() { return fReceiverPacketBuffer; }

	//bool IsValidPacket();

	virtual void Dump();

	enum
	{
		kRTCPPacketSizeInBytes = 8,     //All are uint32_ts
		kRTCPHeaderSizeInBytes = 4
	};

protected:

	uint8_t* fReceiverPacketBuffer;

	enum
	{
		kVersionOffset = 0,
		kVersionMask = 0xC0000000UL,
		kVersionShift = 30,
		kHasPaddingOffset = 0,
		kHasPaddingMask = 0x20000000UL,
		kReportCountOffset = 0,
		kReportCountMask = 0x1F000000UL,
		kReportCountShift = 24,
		kPacketTypeOffset = 0,
		kPacketTypeMask = 0x00FF0000UL,
		kPacketTypeShift = 16,
		kPacketLengthOffset = 0,
		kPacketLengthMask = 0x0000FFFFUL,
		kPacketSourceIDOffset = 4,  //packet sender SSRC
		kPacketSourceIDSize = 4,    //
		kSupportedRTCPVersion = 2
	};

};




class SourceDescriptionPacket : public RTCPPacket

{

public:

	SourceDescriptionPacket() : RTCPPacket() {}

	bool ParseSourceDescription(uint8_t* inPacketBuffer, uint32_t inPacketLength)
	{
		return ParsePacket(inPacketBuffer, inPacketLength);
	}

private:
};




class RTCPReceiverPacket : public RTCPPacket
{
public:

	RTCPReceiverPacket() : RTCPPacket(), fRTCPReceiverReportArray(NULL) {}

	//Call this before any accessor method. Returns true if successful, false otherwise
	virtual bool ParseReport(uint8_t* inPacketBuffer, uint32_t inPacketLength);

	inline uint32_t GetReportSourceID(int inReportNum);
	uint8_t GetFractionLostPackets(int inReportNum);
	uint32_t GetTotalLostPackets(int inReportNum);
	inline uint32_t GetHighestSeqNumReceived(int inReportNum);
	inline uint32_t GetJitter(int inReportNum);
	inline uint32_t GetLastSenderReportTime(int inReportNum);
	inline uint32_t GetLastSenderReportDelay(int inReportNum);    //expressed in units of 1/65536 seconds

	uint32_t GetCumulativeFractionLostPackets();
	uint32_t GetCumulativeTotalLostPackets();
	uint32_t GetCumulativeJitter();

	//bool IsValidPacket();

	virtual void Dump(); //Override

protected:
	inline int RecordOffset(int inReportNum);

	uint8_t* fRTCPReceiverReportArray;    //points into fReceiverPacketBuffer

	enum
	{
		kReportBlockOffsetSizeInBytes = 24,     //All are uint32_ts

		kReportBlockOffset = kPacketSourceIDOffset + kPacketSourceIDSize,

		kReportSourceIDOffset = 0,  //SSRC for this report
		kFractionLostOffset = 4,
		kFractionLostMask = 0xFF000000UL,
		kFractionLostShift = 24,
		kTotalLostPacketsOffset = 4,
		kTotalLostPacketsMask = 0x00FFFFFFUL,
		kHighestSeqNumReceivedOffset = 8,
		kJitterOffset = 12,
		kLastSenderReportOffset = 16,
		kLastSenderReportDelayOffset = 20
	};
};

class RTCPSenderReportPacket : public RTCPReceiverPacket
{
public:
	bool ParseReport(uint8_t* inPacketBuffer, uint32_t inPacketLength);
	SInt64 GetNTPTimeStamp()
	{
		uint32_t* fieldPtr = (uint32_t*)&fReceiverPacketBuffer[kSRPacketNTPTimeStampMSW];
		SInt64 timestamp = ntohl(*fieldPtr);
		fieldPtr = (uint32_t*)&fReceiverPacketBuffer[kSRPacketNTPTimeStampLSW];
		return (timestamp << 32) | ntohl(*fieldPtr);
	}
	uint32_t GetRTPTimeStamp()
	{
		uint32_t* fieldPtr = (uint32_t*)&fReceiverPacketBuffer[kSRPacketRTPTimeStamp];
		return ntohl(*fieldPtr);
	}
protected:
	enum
	{
		kRTCPSRPacketSenderInfoInBytes = 20,
		kSRPacketNTPTimeStampMSW = 8,
		kSRPacketNTPTimeStampLSW = 12,
		kSRPacketRTPTimeStamp = 16
	};
};


/**************  RTCPPacket  inlines **************/
inline int RTCPPacket::GetVersion()
{
	uint32_t* theVersionPtr = (uint32_t*)&fReceiverPacketBuffer[kVersionOffset];
	uint32_t theVersion = ntohl(*theVersionPtr);
	return (int)((theVersion  & kVersionMask) >> kVersionShift);
}

inline bool RTCPPacket::GetHasPadding()
{
	uint32_t* theHasPaddingPtr = (uint32_t*)&fReceiverPacketBuffer[kHasPaddingOffset];
	uint32_t theHasPadding = ntohl(*theHasPaddingPtr);
	return (bool)(theHasPadding & kHasPaddingMask);
}

inline int RTCPPacket::GetReportCount()
{
	uint32_t* theReportCountPtr = (uint32_t*)&fReceiverPacketBuffer[kReportCountOffset];
	uint32_t theReportCount = ntohl(*theReportCountPtr);
	return (int)((theReportCount & kReportCountMask) >> kReportCountShift);
}

inline uint8_t RTCPPacket::GetPacketType()
{
	uint32_t* thePacketTypePtr = (uint32_t*)&fReceiverPacketBuffer[kPacketTypeOffset];
	uint32_t thePacketType = ntohl(*thePacketTypePtr);
	return (uint8_t)((thePacketType & kPacketTypeMask) >> kPacketTypeShift);
}

inline uint16_t RTCPPacket::GetPacketLength()
{
	uint32_t* fieldPtr = (uint32_t*)&fReceiverPacketBuffer[kPacketLengthOffset];
	uint32_t field = ntohl(*fieldPtr);
	return (uint16_t)(field & kPacketLengthMask);
}

inline uint32_t RTCPPacket::GetPacketSSRC()
{
	uint32_t* fieldPtr = (uint32_t*)&fReceiverPacketBuffer[kPacketSourceIDOffset];
	uint32_t field = ntohl(*fieldPtr);
	return field;
}

inline int16_t RTCPPacket::GetHeader() { return (int16_t)ntohs(*(int16_t*)&fReceiverPacketBuffer[0]); }

/**************  RTCPReceiverPacket  inlines **************/
inline int RTCPReceiverPacket::RecordOffset(int inReportNum)
{
	return inReportNum*kReportBlockOffsetSizeInBytes;
}


inline uint32_t RTCPReceiverPacket::GetReportSourceID(int inReportNum)
{
	return (uint32_t)ntohl(*(uint32_t*)&fRTCPReceiverReportArray[this->RecordOffset(inReportNum) + kReportSourceIDOffset]);
}

inline uint8_t RTCPReceiverPacket::GetFractionLostPackets(int inReportNum)
{
	return (uint8_t)((ntohl(*(uint32_t*)&fRTCPReceiverReportArray[this->RecordOffset(inReportNum) + kFractionLostOffset]) & kFractionLostMask) >> kFractionLostShift);
}


inline uint32_t RTCPReceiverPacket::GetTotalLostPackets(int inReportNum)
{
	return (ntohl(*(uint32_t*)&fRTCPReceiverReportArray[this->RecordOffset(inReportNum) + kTotalLostPacketsOffset]) & kTotalLostPacketsMask);
}


inline uint32_t RTCPReceiverPacket::GetHighestSeqNumReceived(int inReportNum)
{
	return (uint32_t)ntohl(*(uint32_t*)&fRTCPReceiverReportArray[this->RecordOffset(inReportNum) + kHighestSeqNumReceivedOffset]);
}

inline uint32_t RTCPReceiverPacket::GetJitter(int inReportNum)
{
	return (uint32_t)ntohl(*(uint32_t*)&fRTCPReceiverReportArray[this->RecordOffset(inReportNum) + kJitterOffset]);
}


inline uint32_t RTCPReceiverPacket::GetLastSenderReportTime(int inReportNum)
{
	return (uint32_t)ntohl(*(uint32_t*)&fRTCPReceiverReportArray[this->RecordOffset(inReportNum) + kLastSenderReportOffset]);
}


inline uint32_t RTCPReceiverPacket::GetLastSenderReportDelay(int inReportNum)
{
	return (uint32_t)ntohl(*(uint32_t*)&fRTCPReceiverReportArray[this->RecordOffset(inReportNum) + kLastSenderReportDelayOffset]);
}


/*
Receiver Report
---------------
 0                   1                   2                   3
 0 0 0 1 1 1 1 1
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|V=2|P|    RC   |   PT=RR=201   |             length            | header
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     SSRC of packet sender                     |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|                 SSRC_1 (SSRC of first source)                 | report
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
| fraction lost |       cumulative number of packets lost       |   1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           extended highest sequence number received           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      interarrival jitter                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         last SR (LSR)                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   delay since last SR (DLSR)                  |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|                 SSRC_2 (SSRC of second source)                | report
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
:                               ...                             :   2
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|                  profile-specific extensions                  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+



*/

#endif //_RTCPPACKET_H_
