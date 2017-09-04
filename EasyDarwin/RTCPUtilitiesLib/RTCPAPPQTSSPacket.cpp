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
	 File:       RTCPAPPQTSSPacket.cpp

	 Contains:   RTCPAPPQTSSPacket de-packetizing classes


 */

#include <cstdint>
#include "RTCPAPPQTSSPacket.h"
#include "MyAssert.h"
#include "OS.h"

// use if you don't know what kind of packet this is
bool RTCPCompressedQTSSPacket::ParseCompressedQTSSPacket(uint8_t* inPacketBuffer, uint32_t inPacketLength)
{
	if (!this->ParseAPPPacket(inPacketBuffer, inPacketLength))
		return false;

	if (this->GetAppPacketName() != RTCPCompressedQTSSPacket::kCompressedQTSSPacketName)
		return false;

	if (inPacketLength < kQTSSDataOffset)
		return false;

	//figure out how many 32-bit words remain in the buffer
	uint32_t theMaxDataLen = inPacketLength - kQTSSDataOffset;
	theMaxDataLen /= 4;

	//if the number of 32 bit words reported in the packet is greater than the theoretical limit,
	//return an error
	if (this->GetQTSSPacketLength() > theMaxDataLen)
		return false;

	if (this->GetQTSSPacketVersion() != kSupportedCompressedQTSSVersion)
		return false;

	if (this->GetReportCount() > 0)
		return false;


	return true;
}



// You know the packet type and just want to parse it now
bool RTCPCompressedQTSSPacket::ParseAPPData(uint8_t* inPacketBuffer, uint32_t inPacketLength)
{

	if (!this->ParseCompressedQTSSPacket(inPacketBuffer, inPacketLength))
		return false;

	char   printName[5];
	(void) this->GetAppPacketName(printName, sizeof(printName));

	uint8_t* qtssDataBuffer = this->GetPacketBuffer() + kQTSSDataOffset;

	//packet length is given in words
	uint32_t bytesRemaining = this->GetQTSSPacketLength() * 4;
	while (bytesRemaining >= 4) //items must be at least 32 bits
	{
		// DMS - There is no guarentee that qtssDataBuffer will be 4 byte aligned, because
		// individual APP packet fields can be 6 bytes or 4 bytes or 8 bytes. So we have to
		// use the 4-byte align protection functions. Sparc and MIPS processors will crash otherwise
		uint32_t theHeader = ntohl(*(uint32_t*)&qtssDataBuffer[kQTSSItemTypeOffset]);
		auto itemType = (uint16_t)((theHeader & kQTSSItemTypeMask) >> kQTSSItemTypeShift);
		auto itemVersion = (uint8_t)((theHeader & kQTSSItemVersionMask) >> kQTSSItemVersionShift);
		auto itemLengthInBytes = (uint8_t)(theHeader & kQTSSItemLengthMask);

		qtssDataBuffer += sizeof(uint32_t);   //advance past the above uint16_t's & uint8_t's (point it at the actual item data)

		//Update bytesRemaining (move it past current item)
		//This itemLengthInBytes is part of the packet and could therefore be bogus.
		//Make sure not to overstep the end of the buffer!
		bytesRemaining -= sizeof(uint32_t);
		if (itemLengthInBytes > bytesRemaining)
			break; //don't walk off the end of the buffer
			//itemLengthInBytes = bytesRemaining;
		bytesRemaining -= itemLengthInBytes;

		switch (itemType)
		{
		case  TW0_CHARS_TO_INT('r', 'r'): //'rr': //'rrcv':
			{
				fReceiverBitRate = ntohl(*(uint32_t*)qtssDataBuffer);
				qtssDataBuffer += sizeof(fReceiverBitRate);
			}
			break;

		case TW0_CHARS_TO_INT('l', 't'): //'lt':    //'late':
			{
				fAverageLateMilliseconds = ntohs(*(uint16_t*)qtssDataBuffer);
				qtssDataBuffer += sizeof(fAverageLateMilliseconds);
			}
			break;

		case TW0_CHARS_TO_INT('l', 's'): // 'ls':   //'loss':
			{
				fPercentPacketsLost = ntohs(*(uint16_t*)qtssDataBuffer);
				qtssDataBuffer += sizeof(fPercentPacketsLost);
			}
			break;

		case TW0_CHARS_TO_INT('d', 'l'): //'dl':    //'bdly':
			{
				fAverageBufferDelayMilliseconds = ntohs(*(uint16_t*)qtssDataBuffer);
				qtssDataBuffer += sizeof(fAverageBufferDelayMilliseconds);
			}
			break;

		case TW0_CHARS_TO_INT(':', ')'): //:)   
			{
				fIsGettingBetter = true;
			}
			break;
		case TW0_CHARS_TO_INT(':', '('): // ':(': 
			{
				fIsGettingWorse = true;
			}
			break;

		case TW0_CHARS_TO_INT(':', '|'): // ':|': 
			{
				fIsGettingWorse = true;
			}
			break;

		case TW0_CHARS_TO_INT('e', 'y'): //'ey':   //'eyes':
			{
				fNumEyes = ntohl(*(uint32_t*)qtssDataBuffer);
				qtssDataBuffer += sizeof(fNumEyes);

				if (itemLengthInBytes >= 2)
				{
					fNumEyesActive = ntohl(*(uint32_t*)qtssDataBuffer);
					qtssDataBuffer += sizeof(fNumEyesActive);
				}
				if (itemLengthInBytes >= 3)
				{
					fNumEyesPaused = ntohl(*(uint32_t*)qtssDataBuffer);
					qtssDataBuffer += sizeof(fNumEyesPaused);
				}
			}
			break;

		case TW0_CHARS_TO_INT('p', 'r'): // 'pr':  //'prcv':
			{
				fTotalPacketsReceived = ntohl(*(uint32_t*)qtssDataBuffer);
				qtssDataBuffer += sizeof(fTotalPacketsReceived);
			}
			break;

		case TW0_CHARS_TO_INT('p', 'd'): //'pd':    //'pdrp':
			{
				fTotalPacketsDropped = ntohs(*(uint16_t*)qtssDataBuffer);
				qtssDataBuffer += sizeof(fTotalPacketsDropped);
			}
			break;

		case TW0_CHARS_TO_INT('p', 'l'): //'pl':    //'p???':
			{
				fTotalPacketsLost = ntohs(*(uint16_t*)qtssDataBuffer);
				qtssDataBuffer += sizeof(fTotalPacketsLost);
			}
			break;


		case TW0_CHARS_TO_INT('b', 'l'): //'bl':    //'bufl':
			{
				fClientBufferFill = ntohs(*(uint16_t*)qtssDataBuffer);
				qtssDataBuffer += sizeof(fClientBufferFill);
			}
			break;


		case TW0_CHARS_TO_INT('f', 'r'): //'fr':    //'frat':
			{
				fFrameRate = ntohs(*(uint16_t*)qtssDataBuffer);
				qtssDataBuffer += sizeof(fFrameRate);
			}
			break;


		case TW0_CHARS_TO_INT('x', 'r'): //'xr':    //'xrat':
			{
				fExpectedFrameRate = ntohs(*(uint16_t*)qtssDataBuffer);
				qtssDataBuffer += sizeof(fExpectedFrameRate);
			}
			break;


		case TW0_CHARS_TO_INT('d', '#'): //'d#':    //'dry#':
			{
				fAudioDryCount = ntohs(*(uint16_t*)qtssDataBuffer);
				qtssDataBuffer += sizeof(fAudioDryCount);
			}
			break;

		case TW0_CHARS_TO_INT('o', 'b'): //'ob': // overbuffer window size
			{
				fOverbufferWindowSize = ntohl(*(uint32_t*)qtssDataBuffer);
				qtssDataBuffer += sizeof(fOverbufferWindowSize);
			}
			break;

		default:
			{
			}

			break;
		}   //      switch (itemType)

	}   //while ( bytesRemaining >= 4 )


	return true;
}