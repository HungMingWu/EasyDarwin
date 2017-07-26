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

RTCPCompressedQTSSPacket::RTCPCompressedQTSSPacket(bool debug) :
	RTCPAPPPacket(debug),
	fOverbufferWindowSize(UINT32_MAX)
	//Proposed - are these there yet?
{
}

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

	APPEND_TO_DUMP_ARRAY("%s", "\n              RTCP APP QTSS Report ");

	char   printName[5];
	(void) this->GetAppPacketName(printName, sizeof(printName));
	APPEND_TO_DUMP_ARRAY("H_app_packet_name = %s, ", printName);
	APPEND_TO_DUMP_ARRAY("H_ssrc = %"   _U32BITARG_   ", ", this->GetPacketSSRC());
	APPEND_TO_DUMP_ARRAY("H_src_ID = %"   _U32BITARG_   ", ", this->GetQTSSReportSourceID());
	APPEND_TO_DUMP_ARRAY("H_vers=%d, ", this->GetQTSSPacketVersion());
	APPEND_TO_DUMP_ARRAY("H_packt_len=%d", this->GetQTSSPacketLength());

	uint8_t* qtssDataBuffer = this->GetPacketBuffer() + kQTSSDataOffset;

	//packet length is given in words
	uint32_t bytesRemaining = this->GetQTSSPacketLength() * 4;
	while (bytesRemaining >= 4) //items must be at least 32 bits
	{
		// DMS - There is no guarentee that qtssDataBuffer will be 4 byte aligned, because
		// individual APP packet fields can be 6 bytes or 4 bytes or 8 bytes. So we have to
		// use the 4-byte align protection functions. Sparc and MIPS processors will crash otherwise
		uint32_t theHeader = ntohl(OS::GetUInt32FromMemory((uint32_t*)&qtssDataBuffer[kQTSSItemTypeOffset]));
		auto itemType = (uint16_t)((theHeader & kQTSSItemTypeMask) >> kQTSSItemTypeShift);
		auto itemVersion = (uint8_t)((theHeader & kQTSSItemVersionMask) >> kQTSSItemVersionShift);
		auto itemLengthInBytes = (uint8_t)(theHeader & kQTSSItemLengthMask);

		APPEND_TO_DUMP_ARRAY("\n       h_type=%.2s(", (char*)&itemType);
		APPEND_TO_DUMP_ARRAY(", h_vers=%u", itemVersion);
		APPEND_TO_DUMP_ARRAY(", h_size=%u", itemLengthInBytes);

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
				fReceiverBitRate = ntohl(OS::GetUInt32FromMemory((uint32_t*)qtssDataBuffer));
				qtssDataBuffer += sizeof(fReceiverBitRate);
				APPEND_TO_DUMP_ARRAY(", rcvr_bit_rate=%"   _U32BITARG_   "", fReceiverBitRate);
			}
			break;

		case TW0_CHARS_TO_INT('l', 't'): //'lt':    //'late':
			{
				fAverageLateMilliseconds = ntohs(*(uint16_t*)qtssDataBuffer);
				qtssDataBuffer += sizeof(fAverageLateMilliseconds);
				APPEND_TO_DUMP_ARRAY(", avg_late=%u", fAverageLateMilliseconds);
			}
			break;

		case TW0_CHARS_TO_INT('l', 's'): // 'ls':   //'loss':
			{
				fPercentPacketsLost = ntohs(*(uint16_t*)qtssDataBuffer);
				qtssDataBuffer += sizeof(fPercentPacketsLost);
				APPEND_TO_DUMP_ARRAY(", percent_loss=%u", fPercentPacketsLost);
			}
			break;

		case TW0_CHARS_TO_INT('d', 'l'): //'dl':    //'bdly':
			{
				fAverageBufferDelayMilliseconds = ntohs(*(uint16_t*)qtssDataBuffer);
				qtssDataBuffer += sizeof(fAverageBufferDelayMilliseconds);
				APPEND_TO_DUMP_ARRAY(", avg_buf_delay=%u", fAverageBufferDelayMilliseconds);
			}
			break;

		case TW0_CHARS_TO_INT(':', ')'): //:)   
			{
				fIsGettingBetter = true;
				APPEND_TO_DUMP_ARRAY(", quality=%s", "better");
			}
			break;
		case TW0_CHARS_TO_INT(':', '('): // ':(': 
			{
				fIsGettingWorse = true;
				APPEND_TO_DUMP_ARRAY(", quality=%s", "worse");
			}
			break;

		case TW0_CHARS_TO_INT(':', '|'): // ':|': 
			{
				fIsGettingWorse = true;
				APPEND_TO_DUMP_ARRAY(", quality=%s", "same");
			}
			break;

		case TW0_CHARS_TO_INT('e', 'y'): //'ey':   //'eyes':
			{
				fNumEyes = ntohl(OS::GetUInt32FromMemory((uint32_t*)qtssDataBuffer));
				qtssDataBuffer += sizeof(fNumEyes);
				APPEND_TO_DUMP_ARRAY(", eyes=%"   _U32BITARG_   "", fNumEyes);

				if (itemLengthInBytes >= 2)
				{
					fNumEyesActive = ntohl(OS::GetUInt32FromMemory((uint32_t*)qtssDataBuffer));
					qtssDataBuffer += sizeof(fNumEyesActive);
					APPEND_TO_DUMP_ARRAY(", eyes_actv=%"   _U32BITARG_   "", fNumEyesActive);
				}
				if (itemLengthInBytes >= 3)
				{
					fNumEyesPaused = ntohl(OS::GetUInt32FromMemory((uint32_t*)qtssDataBuffer));
					qtssDataBuffer += sizeof(fNumEyesPaused);
					APPEND_TO_DUMP_ARRAY(", eyes_pausd=%"   _U32BITARG_   "", fNumEyesPaused);
				}
			}
			break;

		case TW0_CHARS_TO_INT('p', 'r'): // 'pr':  //'prcv':
			{
				fTotalPacketsReceived = ntohl(OS::GetUInt32FromMemory((uint32_t*)qtssDataBuffer));
				qtssDataBuffer += sizeof(fTotalPacketsReceived);
				APPEND_TO_DUMP_ARRAY(", pckts_rcvd=%"   _U32BITARG_   "", fTotalPacketsReceived);
			}
			break;

		case TW0_CHARS_TO_INT('p', 'd'): //'pd':    //'pdrp':
			{
				fTotalPacketsDropped = ntohs(*(uint16_t*)qtssDataBuffer);
				qtssDataBuffer += sizeof(fTotalPacketsDropped);
				APPEND_TO_DUMP_ARRAY(", pckts_drppd=%u", fTotalPacketsDropped);
			}
			break;

		case TW0_CHARS_TO_INT('p', 'l'): //'pl':    //'p???':
			{
				fTotalPacketsLost = ntohs(*(uint16_t*)qtssDataBuffer);
				qtssDataBuffer += sizeof(fTotalPacketsLost);
				APPEND_TO_DUMP_ARRAY(", ttl_pckts_lost=%u", fTotalPacketsLost);
			}
			break;


		case TW0_CHARS_TO_INT('b', 'l'): //'bl':    //'bufl':
			{
				fClientBufferFill = ntohs(*(uint16_t*)qtssDataBuffer);
				qtssDataBuffer += sizeof(fClientBufferFill);
				APPEND_TO_DUMP_ARRAY(", buffr_fill=%u", fClientBufferFill);
			}
			break;


		case TW0_CHARS_TO_INT('f', 'r'): //'fr':    //'frat':
			{
				fFrameRate = ntohs(*(uint16_t*)qtssDataBuffer);
				qtssDataBuffer += sizeof(fFrameRate);
				APPEND_TO_DUMP_ARRAY(", frame_rate=%u", fFrameRate);
			}
			break;


		case TW0_CHARS_TO_INT('x', 'r'): //'xr':    //'xrat':
			{
				fExpectedFrameRate = ntohs(*(uint16_t*)qtssDataBuffer);
				qtssDataBuffer += sizeof(fExpectedFrameRate);
				APPEND_TO_DUMP_ARRAY(", xpectd_frame_rate=%u", fExpectedFrameRate);
			}
			break;


		case TW0_CHARS_TO_INT('d', '#'): //'d#':    //'dry#':
			{
				fAudioDryCount = ntohs(*(uint16_t*)qtssDataBuffer);
				qtssDataBuffer += sizeof(fAudioDryCount);
				APPEND_TO_DUMP_ARRAY(", aud_dry_count=%u", fAudioDryCount);
			}
			break;

		case TW0_CHARS_TO_INT('o', 'b'): //'ob': // overbuffer window size
			{
				fOverbufferWindowSize = ntohl(OS::GetUInt32FromMemory((uint32_t*)qtssDataBuffer));
				qtssDataBuffer += sizeof(fOverbufferWindowSize);
				APPEND_TO_DUMP_ARRAY(", ovr_buffr_windw_siz=%"   _U32BITARG_   "", fOverbufferWindowSize);
			}
			break;

		default:
			{
				if (fDebug)
				{
					char s[12] = "";
					sprintf(s, "  [%.2s]", (char*)&itemType);
					WarnV(false, "Unknown APP('QTSS') item type");
					WarnV(false, s);
				}
			}

			break;
		}   //      switch (itemType)


		APPEND_TO_DUMP_ARRAY("%s", "),  ");

	}   //while ( bytesRemaining >= 4 )


	return true;
}

void RTCPCompressedQTSSPacket::Dump()//Override
{
	APPEND_TO_DUMP_ARRAY("%s", "\n");
	RTCPAPPPacket::Dump();

}


