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
	 File:       RTCPAckPacket.h

	 Contains:   RTCPAckPacket de-packetizing class



 */

#ifndef _RTCPACKPACKET_H_
#define _RTCPACKPACKET_H_

#include "OSHeaders.h"
#include "RTCPAPPPacket.h"
#include <stdlib.h>
#ifndef __Win32__
#include <netinet/in.h>
#endif

class RTCPAckPacket : public RTCPAPPPacket
{
public:

	/*

			RTCP app ACK packet

			# bytes   description
			-------   -----------
			4         rtcp header
			4         SSRC of receiver
			4         app type ('qtak')
			2         reserved (set to 0)
			2         seqNum

	*/

	//
	// This class is not derived from RTCPPacket as a performance optimization.
	// Instead, it is assumed that the RTCP packet validation has already been
	// done.
	RTCPAckPacket() : fRTCPAckBuffer(nullptr), fAckMaskSize(0) {}
	~RTCPAckPacket() override {}

	// Call to parse if you don't know what kind of packet this is
	// Returns true if this is an Ack packet, false otherwise.
	// Assumes that inPacketBuffer is a pointer to a valid RTCP packet header.
	bool  ParseAckPacket(uint8_t* inPacketBuffer, uint32_t inPacketLength);

	bool ParseAPPData(uint8_t* inPacketBuffer, uint32_t inPacketLength) override;

	inline uint16_t GetAckSeqNum();
	inline uint32_t GetAckMaskSizeInBits() { return fAckMaskSize * 8; }
	inline bool IsNthBitEnabled(uint32_t inBitNumber);
	inline uint16_t GetPacketLength();
	void   Dump() override;
	static void GetTestPacket(StrPtrLen* resultPtr) {} //todo

	enum
	{
		kAckPacketName = FOUR_CHARS_TO_INT('q', 't', 'a', 'k'), // 'qtak'  documented Apple reliable UDP packet type
		kAckPacketAlternateName = FOUR_CHARS_TO_INT('a', 'c', 'k', ' '), // 'ack ' required by QT 5 and earlier
	};
private:

	uint8_t* fRTCPAckBuffer;
	uint32_t fAckMaskSize;

	bool IsAckPacketType();

	enum
	{
		kAppPacketTypeOffset = 8,
		kAckSeqNumOffset = 16,
		kAckMaskOffset = 20,
		kPacketLengthMask = 0x0000FFFFUL,
	};



	inline bool IsAckType(uint32_t theAppType) { return ((theAppType == kAckPacketAlternateName) || (theAppType == kAckPacketName)); }
};


bool RTCPAckPacket::IsNthBitEnabled(uint32_t inBitNumber)
{
	// Don't need to do endian conversion because we're dealing with 8-bit numbers
	uint8_t bitMask = 128;
	return *(fRTCPAckBuffer + kAckMaskOffset + (inBitNumber >> 3)) & (bitMask >>= inBitNumber & 7);
}

uint16_t RTCPAckPacket::GetAckSeqNum()
{
	return (uint16_t)(ntohl(*(uint32_t*)&fRTCPAckBuffer[kAckSeqNumOffset]));
}

inline uint16_t RTCPAckPacket::GetPacketLength()
{
	return (uint16_t)(ntohl(*(uint32_t*)fRTCPAckBuffer) & kPacketLengthMask);
}




/*
6.6 Ack Packet format

	0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P| subtype |   PT=APP=204  |             length            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           SSRC/CSRC                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                          name (ASCII)  = 'qtak'               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           SSRC/CSRC                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          Reserved             |          Seq num              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Mask...                                |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

 */

#endif //_RTCPAPPPACKET_H_
