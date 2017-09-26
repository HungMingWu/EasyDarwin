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
	 File:       RTCPSRPacket.h

	 Contains:   A class that writes a RTCP Sender Report

	 Change History (most recent first):


 */

#ifndef __RTCP_SR_PACKET__
#define __RTCP_SR_PACKET__

#include "OSHeaders.h"
#include "OS.h"
#include "MyAssert.h"

#ifndef __Win32__
#include <netinet/in.h> //definition of htonl
#endif

class RTCPSRPacket
{
public:

	enum
	{
		kSRPacketType = 200,    //uint32_t
		kByePacketType = 203
	};

	RTCPSRPacket();
	~RTCPSRPacket() = default;

	// ACCESSORS

	const char*   GetSRPacket() { return &fSenderReportBuffer[0]; }
	uint32_t  GetSRPacketLen() { return fSenderReportWithServerInfoSize; }
	uint32_t  GetSRWithByePacketLen() { return fSenderReportWithServerInfoSize + kByeSizeInBytes; }

	void*   GetServerInfoPacket() { return &fSenderReportBuffer[fSenderReportSize]; }
	uint32_t  GetServerInfoPacketLen() { return kServerInfoSizeInBytes; }

	//
	// MODIFIERS

	//
	// FOR SR
	inline void SetSSRC(uint32_t inSSRC);
	inline void SetClientSSRC(uint32_t inClientSSRC);

	inline void SetNTPTimestamp(int64_t inNTPTimestamp);
	inline void SetRTPTimestamp(uint32_t inRTPTimestamp);

	inline void SetPacketCount(uint32_t inPacketCount);
	inline void SetByteCount(uint32_t inByteCount);

	//
	// FOR SERVER INFO APP PACKET
	inline void SetAckTimeout(uint32_t inAckTimeoutInMsec);

	//RTCP support requires generating unique CNames for each session.
	//This function generates a proper cName and returns its length. The buffer
	//passed in must be at least kMaxCNameLen
	enum
	{
		kMaxCNameLen = 60   //uint32_t
	};
	static uint32_t           GetACName(char* ioCNameBuffer);

private:

	enum
	{
		kSenderReportSizeInBytes = 36,
		kServerInfoSizeInBytes = 28,
		kByeSizeInBytes = 8
	};
	char        fSenderReportBuffer[kSenderReportSizeInBytes + kMaxCNameLen + kServerInfoSizeInBytes + kByeSizeInBytes];
	uint32_t      fSenderReportSize;
	uint32_t      fSenderReportWithServerInfoSize;

};

inline void RTCPSRPacket::SetSSRC(uint32_t inSSRC)
{
	// Set SSRC in SR
	((uint32_t*)&fSenderReportBuffer)[1] = htonl(inSSRC);

	// Set SSRC in SDES
	((uint32_t*)&fSenderReportBuffer)[8] = htonl(inSSRC);

	// Set SSRC in SERVER INFO
	Assert((fSenderReportSize & 3) == 0);
	((uint32_t*)&fSenderReportBuffer)[(fSenderReportSize >> 2) + 1] = htonl(inSSRC);

	// Set SSRC in BYE
	Assert((fSenderReportWithServerInfoSize & 3) == 0);
	((uint32_t*)&fSenderReportBuffer)[(fSenderReportWithServerInfoSize >> 2) + 1] = htonl(inSSRC);
}

inline void RTCPSRPacket::SetClientSSRC(uint32_t inClientSSRC)
{
	//
	// Set Client SSRC in SERVER INFO
	((uint32_t*)&fSenderReportBuffer)[(fSenderReportSize >> 2) + 3] = htonl(inClientSSRC);
}

inline void RTCPSRPacket::SetNTPTimestamp(int64_t inNTPTimestamp)
{
#if ALLOW_NON_WORD_ALIGN_ACCESS
	((int64_t*)&fSenderReportBuffer)[1] = OS::HostToNetworkSInt64(inNTPTimestamp);
#else
	int64_t temp = OS::HostToNetworkint64_t(inNTPTimestamp);
	::memcpy(&((int64_t*)&fSenderReportBuffer)[1], &temp, sizeof(temp));
#endif
}

inline void RTCPSRPacket::SetRTPTimestamp(uint32_t inRTPTimestamp)
{
	((uint32_t*)&fSenderReportBuffer)[4] = htonl(inRTPTimestamp);
}

inline void RTCPSRPacket::SetPacketCount(uint32_t inPacketCount)
{
	((uint32_t*)&fSenderReportBuffer)[5] = htonl(inPacketCount);
}

inline void RTCPSRPacket::SetByteCount(uint32_t inByteCount)
{
	((uint32_t*)&fSenderReportBuffer)[6] = htonl(inByteCount);
}

inline void RTCPSRPacket::SetAckTimeout(uint32_t inAckTimeoutInMsec)
{
	((uint32_t*)&fSenderReportBuffer)[(fSenderReportWithServerInfoSize >> 2) - 1] = htonl(inAckTimeoutInMsec);
}

#endif //__RTCP_SR_PACKET__
