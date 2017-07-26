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
	 File:       RTCPAckPacket.cpp

	 Contains:   RTCPAckPacket de-packetizing class


 */


#include "RTCPAckPacket.h"
#include "RTCPPacket.h"
#include "MyAssert.h"
#include <stdio.h>
#include <memory>

 // use if you don't know what kind of packet this is
bool RTCPAckPacket::ParseAckPacket(uint8_t* inPacketBuffer, uint32_t inPacketLength)
{

	if (!this->ParseAPPPacket(inPacketBuffer, inPacketLength))
		return false;

	if (this->GetAppPacketName() == RTCPAckPacket::kAckPacketName)
		return true;

	if (this->GetAppPacketName() == RTCPAckPacket::kAckPacketAlternateName)
		return true;

	return false;

}


bool RTCPAckPacket::ParseAPPData(uint8_t* inPacketBuffer, uint32_t inPacketLength)
{
	if (!this->ParseAckPacket(inPacketBuffer, inPacketLength))
		return false;


	fRTCPAckBuffer = inPacketBuffer;

	//
	// Check whether this is an ack packet or not.
	if ((inPacketLength < kAckMaskOffset) || (!this->IsAckPacketType()))
		return false;

	Assert(inPacketLength == (uint32_t)((this->GetPacketLength() * 4)) + RTCPPacket::kRTCPHeaderSizeInBytes);
	fAckMaskSize = inPacketLength - kAckMaskOffset;

	return true;
}

bool RTCPAckPacket::IsAckPacketType()
{
	// While we are moving to a new type, check for both
	uint32_t theAppType = this->GetAppPacketName();

	//  if ( theAppType == kAckPacketAlternateName ) printf("ack\n"); 
	//  if ( theAppType == kAckPacketName ) printf("qtack\n");

	return this->IsAckType(theAppType);
}

void   RTCPAckPacket::Dump()
{
	uint16_t theSeqNum = this->GetAckSeqNum();
	uint16_t thePacketLen = this->GetPacketLength();
	uint32_t theAckMaskSizeInBits = this->GetAckMaskSizeInBits();

	char name[5];
	name[4] = 0;

	::memcpy(name, &fRTCPAckBuffer[kAppPacketTypeOffset], 4);
	auto numBufferBytes = (uint16_t)((7 * theAckMaskSizeInBits) + 1);
	auto *maskBytesBuffer = new char[numBufferBytes];
	std::unique_ptr<char[]> deleter(maskBytesBuffer);
	maskBytesBuffer[0] = 0;
	maskBytesBuffer[numBufferBytes - 1] = 0;
	for (uint32_t maskCount = 0; maskCount < theAckMaskSizeInBits; maskCount++)
	{
		if (this->IsNthBitEnabled(maskCount))
		{
			sprintf(&maskBytesBuffer[::strlen(maskBytesBuffer)], "%"   _U32BITARG_   ", ", theSeqNum + 1 + maskCount);
		}
	}
	Assert(::strlen(maskBytesBuffer) < numBufferBytes);
	printf(" H_name=%s H_seq=%u H_len=%u mask_size=%"   _U32BITARG_   " seq_nums_bit_set=%s\n",
		name, theSeqNum, thePacketLen, theAckMaskSizeInBits, maskBytesBuffer);

}

