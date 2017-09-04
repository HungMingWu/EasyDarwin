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