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
	 File:       UDPSocket.cpp

	 Contains:   Implementation of object defined in UDPSocket.h.



 */

#ifndef __Win32__
#include <sys/types.h>
#include <sys/socket.h>

#if __solaris__
#include "SocketUtils.h"
#endif

#if NEED_SOCKETBITS
#if __GLIBC__ >= 2
#include <bits/socket.h>
#else
#include <socketbits.h>
#endif
#endif
#endif

#include <errno.h>
#include "UDPSocket.h"

#ifdef USE_NETLOG
#include <netlog.h>
#endif

UDPSocket::UDPSocket(Task* inTask, uint32_t inSocketType)
	: Socket(inTask, inSocketType)
{
	//setup msghdr
	::memset(&fMsgAddr, 0, sizeof(fMsgAddr));
}


OS_Error
UDPSocket::SendTo(uint32_t inRemoteAddr, uint16_t inRemotePort, const std::vector<char> &inBuffer)
{
	Assert(!inBuffer.empty());

	struct sockaddr_in  theRemoteAddr;
	theRemoteAddr.sin_family = AF_INET;
	theRemoteAddr.sin_port = htons(inRemotePort);
	theRemoteAddr.sin_addr.s_addr = htonl(inRemoteAddr);

	int theErr = ::sendto(fFileDesc, &inBuffer[0], inBuffer.size(), 0, (sockaddr*)&theRemoteAddr, sizeof(theRemoteAddr));

	if (theErr == -1)
		return (OS_Error)OSThread::GetErrno();
	return OS_NoErr;
}

OS_Error UDPSocket::RecvFrom(uint32_t* outRemoteAddr, uint16_t* outRemotePort,
	void* ioBuffer, size_t inBufLen, size_t* outRecvLen)
{
	Assert(outRecvLen != nullptr);
	Assert(outRemoteAddr != nullptr);
	Assert(outRemotePort != nullptr);

#if __Win32__ || __osf__  || __sgi__ || __hpux__
	int addrLen = sizeof(fMsgAddr);
#else
	socklen_t addrLen = sizeof(fMsgAddr);
#endif

#ifdef __sgi__
	int32_t theRecvLen = ::recvfrom(fFileDesc, ioBuffer, inBufLen, 0, (sockaddr*)&fMsgAddr, &addrLen);
#else
	// Win32 says that ioBuffer is a char*
	int32_t theRecvLen = ::recvfrom(fFileDesc, (char*)ioBuffer, inBufLen, 0, (sockaddr*)&fMsgAddr, &addrLen);
#endif

	if (theRecvLen == -1)
		return (OS_Error)OSThread::GetErrno();

	*outRemoteAddr = ntohl(fMsgAddr.sin_addr.s_addr);
	*outRemotePort = ntohs(fMsgAddr.sin_port);
	Assert(theRecvLen >= 0);
	*outRecvLen = (uint32_t)theRecvLen;
	return OS_NoErr;
}

OS_Error UDPSocket::JoinMulticast(uint32_t inRemoteAddr)
{
	struct ip_mreq  theMulti;
	uint32_t localAddr = fLocalAddr.sin_addr.s_addr; // Already in network byte order

#if __solaris__
	if (localAddr == htonl(INADDR_ANY))
		localAddr = htonl(SocketUtils::GetIPAddr(0));
#endif
	theMulti.imr_multiaddr.s_addr = htonl(inRemoteAddr);
	theMulti.imr_interface.s_addr = localAddr;
	int err = setsockopt(fFileDesc, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&theMulti, sizeof(theMulti));
	//AssertV(err == 0, OSThread::GetErrno());
	if (err == -1)
		return (OS_Error)OSThread::GetErrno();
	else
		return OS_NoErr;
}

OS_Error UDPSocket::SetTtl(uint16_t timeToLive)
{
	// set the ttl
	auto  nOptVal = (u_char)timeToLive;//cms - stevens pp. 496. bsd implementations barf
											//unless this is a u_char
	int err = setsockopt(fFileDesc, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&nOptVal, sizeof(nOptVal));
	if (err == -1)
		return (OS_Error)OSThread::GetErrno();
	else
		return OS_NoErr;
}

OS_Error UDPSocket::SetMulticastInterface(uint32_t inLocalAddr)
{
	// set the outgoing interface for multicast datagrams on this socket
	in_addr theLocalAddr;
	theLocalAddr.s_addr = inLocalAddr;
	int err = setsockopt(fFileDesc, IPPROTO_IP, IP_MULTICAST_IF, (char*)&theLocalAddr, sizeof(theLocalAddr));
	AssertV(err == 0, OSThread::GetErrno());
	if (err == -1)
		return (OS_Error)OSThread::GetErrno();
	else
		return OS_NoErr;
}

OS_Error UDPSocket::LeaveMulticast(uint32_t inRemoteAddr)
{
	struct ip_mreq  theMulti;
	theMulti.imr_multiaddr.s_addr = htonl(inRemoteAddr);
	theMulti.imr_interface.s_addr = htonl(fLocalAddr.sin_addr.s_addr);
	int err = setsockopt(fFileDesc, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&theMulti, sizeof(theMulti));
	if (err == -1)
		return (OS_Error)OSThread::GetErrno();
	else
		return OS_NoErr;
}
