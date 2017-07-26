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
	 File:       RTCPAPPPacket.h

	 Contains:   RTCPAPPPacket de-packetizing classes



 */

#ifndef _RTCPAPPPACKET_H_
#define _RTCPAPPPACKET_H_

#include "RTCPPacket.h"
#include "StrPtrLen.h"
#include "ResizeableStringFormatter.h"

#define APPEND_TO_DUMP_ARRAY(f, v) {if (fDebug && mDumpArray != NULL) { (void)snprintf(mDumpArray,kmDumpArraySize, f, v); fDumpReport.Put(mDumpArray); }   }

class RTCPAPPPacket : public RTCPPacket
{

public:
	RTCPAPPPacket(bool debug = false);
	~RTCPAPPPacket() override {};
	void Dump() override;
	virtual bool ParseAPPPacket(uint8_t* inPacketBuffer, uint32_t inPacketLength); //default app header check
	virtual bool ParseAPPData(uint8_t* inPacketBuffer, uint32_t inPacketLength) { return false; }; //derived class implements
	inline FourCharCode GetAppPacketName(char *outName = NULL, uint32_t len = 0);
	inline uint32_t GetAppPacketSSRC();


	uint8_t* fRTCPAPPDataBuffer;  //points into RTCPPacket::fReceiverPacketBuffer should be set past the app header
	uint32_t fAPPDataBufferSize;

	enum
	{
		kAppSSRCOffset = 4,
		kAppNameOffset = 8, //byte offset to four char App identifier               //All are uint32_t

		kRTCPAPPHeaderSizeInBytes = 4, //
		kmDumpArraySize = 1024
	};

	char*           mDumpArray;
	StrPtrLenDel    mDumpArrayStrDeleter;
	ResizeableStringFormatter fDumpReport;
	bool fDebug;

private:
	virtual bool ParseAPPPacketHeader(uint8_t* inPacketBuffer, uint32_t inPacketLength);

};



/****************  RTCPAPPPacket inlines *******************************/

inline FourCharCode RTCPAPPPacket::GetAppPacketName(char *outName, uint32_t len)
{

	uint32_t packetName = (uint32_t)(*(uint32_t*)&(GetPacketBuffer()[kAppNameOffset]));

	if (outName)
	{
		if (len > 4)
		{
			*((uint32_t*)outName) = packetName;
			outName[4] = 0;
		}
		else if (len > 0)
			outName[0] = 0;
	}

	return ntohl(packetName);
}


inline uint32_t RTCPAPPPacket::GetAppPacketSSRC()
{
	return (uint32_t)ntohl(*(uint32_t*)&(GetPacketBuffer()[kAppSSRCOffset]));
}





/*
6.6 APP: Application-defined RTCP packet

	0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P| subtype |   PT=APP=204  |             length            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           SSRC/CSRC                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                          name (ASCII)                         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                   application-dependent data                  |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

 */

#endif //_RTCPAPPPACKET_H_
