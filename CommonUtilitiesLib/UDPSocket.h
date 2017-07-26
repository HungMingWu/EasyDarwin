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
	 File:       UDPSocket.h

	 Contains:   Adds additional Socket functionality specific to UDP.




 */


#ifndef __UDPSOCKET_H__
#define __UDPSOCKET_H__

#ifndef __Win32__
#include <sys/socket.h>
#include <sys/uio.h>
#endif

#include "Socket.h"
#include "UDPDemuxer.h"


class UDPSocket : public Socket
{
public:

	//Another socket type flag (in addition to the ones defined in Socket.h).
	//The value of this can't conflict with those!
	enum
	{
		kWantsDemuxer = 0x0100 //uint32_t
	};

	UDPSocket(Task* inTask, uint32_t inSocketType);
	~UDPSocket() override { if (fDemuxer != nullptr) delete fDemuxer; }

	//Open
	OS_Error    Open() { return Socket::Open(SOCK_DGRAM); }

	OS_Error    JoinMulticast(uint32_t inRemoteAddr);
	OS_Error    LeaveMulticast(uint32_t inRemoteAddr);
	OS_Error    SetTtl(uint16_t timeToLive);
	OS_Error    SetMulticastInterface(uint32_t inLocalAddr);

	//returns an ERRNO
	OS_Error        SendTo(uint32_t inRemoteAddr, uint16_t inRemotePort,
		void* inBuffer, uint32_t inLength);

	OS_Error        RecvFrom(uint32_t* outRemoteAddr, uint16_t* outRemotePort,
		void* ioBuffer, uint32_t inBufLen, uint32_t* outRecvLen);

	//A UDP socket may or may not have a demuxer associated with it. The demuxer
	//is a data structure so the socket can associate incoming data with the proper
	//task to process that data (based on source IP addr & port)
	UDPDemuxer*         GetDemuxer() { return fDemuxer; }

private:

	UDPDemuxer* fDemuxer;
	struct sockaddr_in  fMsgAddr;
};
#endif // __UDPSOCKET_H__

