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
	 File:       RTCPAPPQTSSPacket.h

	 Contains:   RTCPAPPQTSSPacket de-packetizing classes



 */

#ifndef _RTCPAPPQTSSPACKET_H_
#define _RTCPAPPQTSSPACKET_H_

#include "RTCPAPPPacket.h"
#include "StrPtrLen.h"

 /****** RTCPCompressedQTSSPacket is the packet type that the client actually sends ******/
class RTCPCompressedQTSSPacket : public RTCPAPPPacket
{
public:

	RTCPCompressedQTSSPacket(bool debug = false);
	~RTCPCompressedQTSSPacket() override {}

	//Call this before any accessor method. Returns true if successful, false otherwise
	bool ParseAPPData(uint8_t* inPacketBuffer, uint32_t inPacketLength) override;

	// Call to parse if you don't know what kind of packet this is
	bool ParseCompressedQTSSPacket(uint8_t* inPacketBuffer, uint32_t inPacketLength);
	inline uint32_t GetQTSSReportSourceID();
	inline uint16_t GetQTSSPacketVersion();
	inline uint16_t GetQTSSPacketLength(); //In 'uint32_t's


	inline uint32_t GetReceiverBitRate() { return fReceiverBitRate; }
	inline uint16_t GetAverageLateMilliseconds() { return fAverageLateMilliseconds; }
	inline uint16_t GetPercentPacketsLost() { return fPercentPacketsLost; }
	inline uint16_t GetAverageBufferDelayMilliseconds() { return fAverageBufferDelayMilliseconds; }
	inline bool GetIsGettingBetter() { return fIsGettingBetter; }
	inline bool GetIsGettingWorse() { return fIsGettingWorse; }
	inline uint32_t GetNumEyes() { return fNumEyes; }
	inline uint32_t GetNumEyesActive() { return fNumEyesActive; }
	inline uint32_t GetNumEyesPaused() { return fNumEyesPaused; }
	inline uint32_t GetOverbufferWindowSize() { return fOverbufferWindowSize; }

	//Proposed - are these there yet?
	inline uint32_t GetTotalPacketReceived() { return fTotalPacketsReceived; }
	inline uint16_t GetTotalPacketsDropped() { return fTotalPacketsDropped; }
	inline uint16_t GetTotalPacketsLost() { return fTotalPacketsLost; }
	inline uint16_t GetClientBufferFill() { return fClientBufferFill; }
	inline uint16_t GetFrameRate() { return fFrameRate; }
	inline uint16_t GetExpectedFrameRate() { return fExpectedFrameRate; }
	inline uint16_t GetAudioDryCount() { return fAudioDryCount; }

	void Dump() override; //Override

	static void GetTestPacket(StrPtrLen* resultPtr) {}

	uint32_t fReceiverBitRate;
	uint16_t fAverageLateMilliseconds;
	uint16_t fPercentPacketsLost;
	uint16_t fAverageBufferDelayMilliseconds;
	bool fIsGettingBetter;
	bool fIsGettingWorse;
	uint32_t fNumEyes;
	uint32_t fNumEyesActive;
	uint32_t fNumEyesPaused;
	uint32_t fOverbufferWindowSize;

	//Proposed - are these there yet?
	uint32_t fTotalPacketsReceived;
	uint16_t fTotalPacketsDropped;
	uint16_t fTotalPacketsLost;
	uint16_t fClientBufferFill;
	uint16_t fFrameRate;
	uint16_t fExpectedFrameRate;
	uint16_t fAudioDryCount;

	enum // QTSS App Header offsets
	{

		kQTSSDataOffset = 20, // in bytes from packet start
		kQTSSReportSourceIDOffset = 3,  //in 32 bit words SSRC for this report
		kQTSSPacketVersionOffset = 4, // in 32bit words
		kQTSSPacketVersionMask = 0xFFFF0000UL,
		kQTSSPacketVersionShift = 16,
		kQTSSPacketLengthOffset = 4, // in 32bit words
		kQTSSPacketLengthMask = 0x0000FFFFUL,

	};

	enum // QTSS App Data Offsets
	{

		//Individual item offsets/masks
		kQTSSItemTypeOffset = 0,    //SSRC for this report
		kQTSSItemTypeMask = 0xFFFF0000UL,
		kQTSSItemTypeShift = 16,
		kQTSSItemVersionOffset = 0,
		kQTSSItemVersionMask = 0x0000FF00UL,
		kQTSSItemVersionShift = 8,
		kQTSSItemLengthOffset = 0,
		kQTSSItemLengthMask = 0x000000FFUL,
		kQTSSItemDataOffset = 4,

		//version we support currently
		kSupportedCompressedQTSSVersion = 0
	};

	enum //The 4 character name in the APP packet
	{
		kCompressedQTSSPacketName = FOUR_CHARS_TO_INT('Q', 'T', 'S', 'S') //QTSS
	};


private:
	void ParseAndStore();

};

inline uint32_t RTCPCompressedQTSSPacket::GetQTSSReportSourceID()
{
	return (uint32_t)ntohl(((uint32_t*)this->GetPacketBuffer())[kQTSSReportSourceIDOffset]);
}


inline uint16_t RTCPCompressedQTSSPacket::GetQTSSPacketVersion()
{
	uint32_t field = ((uint32_t*)this->GetPacketBuffer())[kQTSSPacketVersionOffset];
	auto vers = (uint16_t)((ntohl(field) & kQTSSPacketVersionMask) >> kQTSSPacketVersionShift);
	return vers;
}

inline uint16_t RTCPCompressedQTSSPacket::GetQTSSPacketLength()
{
	uint32_t  field = ((uint32_t*)this->GetPacketBuffer())[kQTSSPacketLengthOffset];
	return (uint16_t)((uint32_t)ntohl(field)  & kQTSSPacketLengthMask);
}

/*
QTSS APP: QTSS Application-defined RTCP packet

	0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P| subtype |  PT=APP=204  |             length             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           SSRC/CSRC                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                          name (ASCII)                         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ <---- app data start
   |                           SSRC/CSRC                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |        version                |             length            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |      field name='ob' other    |   version=0   |   length=4    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |               Over-buffer window size in bytes                |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

fieldnames = rr, lt, ls, dl, :), :|, :(, ey, pr, pd, pl, bl, fr, xr, d#, ob

 */





 /****** RTCPqtssPacket is apparently no longer sent by the client ******/
class RTCPqtssPacket : public RTCPAPPPacket
{
public:

	RTCPqtssPacket() : RTCPAPPPacket() {}
	~RTCPqtssPacket() override {}

	//Call this before any accessor method. Returns true if successful, false otherwise
	bool ParseAPPData(uint8_t* inPacketBuffer, uint32_t inPacketLength) override;

	//Call this before any accessor method. Returns true if successful, false otherwise
	bool ParseQTSSPacket(uint8_t* inPacketBuffer, uint32_t inPacketLength);


	inline uint32_t GetReceiverBitRate() { return fReceiverBitRate; }
	inline uint32_t GetAverageLateMilliseconds() { return fAverageLateMilliseconds; }
	inline uint32_t GetPercentPacketsLost() { return fPercentPacketsLost; }
	inline uint32_t GetAverageBufferDelayMilliseconds() { return fAverageBufferDelayMilliseconds; }
	inline bool GetIsGettingBetter() { return fIsGettingBetter; }
	inline bool GetIsGettingWorse() { return fIsGettingWorse; }
	inline uint32_t GetNumEyes() { return fNumEyes; }
	inline uint32_t GetNumEyesActive() { return fNumEyesActive; }
	inline uint32_t GetNumEyesPaused() { return fNumEyesPaused; }

	//Proposed - are these there yet?
	inline uint32_t GetTotalPacketReceived() { return fTotalPacketsReceived; }
	inline uint32_t GetTotalPacketsDropped() { return fTotalPacketsDropped; }
	inline uint32_t GetClientBufferFill() { return fClientBufferFill; }
	inline uint32_t GetFrameRate() { return fFrameRate; }
	inline uint32_t GetExpectedFrameRate() { return fExpectedFrameRate; }
	inline uint32_t GetAudioDryCount() { return fAudioDryCount; }



private:

	void ParseAndStore();

	uint32_t fReportSourceID;
	uint16_t fAppPacketVersion;
	uint16_t fAppPacketLength;    //In 'uint32_t's

	uint32_t fReceiverBitRate;
	uint32_t fAverageLateMilliseconds;
	uint32_t fPercentPacketsLost;
	uint32_t fAverageBufferDelayMilliseconds;
	bool fIsGettingBetter;
	bool fIsGettingWorse;
	uint32_t fNumEyes;
	uint32_t fNumEyesActive;
	uint32_t fNumEyesPaused;

	//Proposed - are these there yet?
	uint32_t fTotalPacketsReceived;
	uint32_t fTotalPacketsDropped;
	uint32_t fClientBufferFill;
	uint32_t fFrameRate;
	uint32_t fExpectedFrameRate;
	uint32_t fAudioDryCount;

	enum
	{
		//Individual item offsets/masks
		kQTSSItemTypeOffset = 0,    //SSRC for this report
		kQTSSItemVersionOffset = 4,
		kQTSSItemVersionMask = 0xFFFF0000UL,
		kQTSSItemVersionShift = 16,
		kQTSSItemLengthOffset = 4,
		kQTSSItemLengthMask = 0x0000FFFFUL,
		kQTSSItemDataOffset = 8,

		//version we support currently
		kSupportedQTSSVersion = 0
	};


};


#endif //_RTCPAPPQTSSPACKET_H_
