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
	 File:       Socket.h

	 Contains:   Provides a simple, object oriented socket abstraction, also
				 hides the details of socket event handling. Sockets can post
				 events (such as S_DATA, S_CONNECTIONCLOSED) to Tasks.



 */

#ifndef __SOCKET_H__
#define __SOCKET_H__

#ifndef __Win32__
#include <netinet/in.h>
#endif

#include "EventContext.h"

#define SOCKET_DEBUG 0

class Socket : public EventContext
{
public:

	enum
	{
		// Pass this in on socket constructors to specify whether the
		// socket should be non-blocking or blocking
		kNonBlockingSocketType = 1
	};

	// This class provides a global event thread.
	static void Initialize() { sEventThread = new EventThread(); }
	static void StartThread() { sEventThread->Start(); }
	static EventThread* GetEventThread() { return sEventThread; }

	//Binds the socket to the following address.
	//Returns: QTSS_FileNotOpen, QTSS_NoErr, or POSIX errorcode.
	OS_Error    Bind(uint32_t addr, uint16_t port, bool test = false);
	//The same. but in reverse
	void            Unbind();

	void            ReuseAddr();
	void            NoDelay();
	void            KeepAlive();
	void            SetSocketBufSize(uint32_t inNewSize);

	//
	// Returns an error if the socket buffer size is too big
	OS_Error        SetSocketRcvBufSize(uint32_t inNewSize);

	//Send
	//Returns: QTSS_FileNotOpen, QTSS_NoErr, or POSIX errorcode.
	OS_Error    Send(const char* inData, const uint32_t inLength, uint32_t* outLengthSent);

	//Read
	//Reads some data.
	//Returns: QTSS_FileNotOpen, QTSS_NoErr, or POSIX errorcode.
	OS_Error    Read(void *buffer, const uint32_t length, uint32_t *rcvLen);

	//WriteV: same as send, but takes an iovec
	//Returns: QTSS_FileNotOpen, QTSS_NoErr, or POSIX errorcode.
	OS_Error        WriteV(const struct iovec* iov, const uint32_t numIOvecs, uint32_t* outLengthSent);

	//You can query for the socket's state
	bool  IsConnected() { return (bool)(fState & kConnected); }
	bool  IsBound() { return (bool)(fState & kBound); }

	//If the socket is bound, you may find out to which addr it is bound
	uint32_t  GetLocalAddr() { return ntohl(fLocalAddr.sin_addr.s_addr); }
	uint16_t  GetLocalPort() { return ntohs(fLocalAddr.sin_port); }

	StrPtrLen*  GetLocalAddrStr();
	StrPtrLen*  GetLocalPortStr();
	StrPtrLen* GetLocalDNSStr();

	enum
	{
		kMaxNumSockets = 4096   //uint32_t
	};

protected:

	//TCPSocket takes an optional task object which will get notified when
	//certain events happen on this socket. Those events are:
	//
	//S_DATA:               Data is currently available on the socket.
	//S_CONNECTIONCLOSING:  Client is closing the connection. No longer necessary
	//                      to call Close or Disconnect, Snd & Rcv will fail.

	Socket(Task *notifytask, uint32_t inSocketType);
	~Socket() override {}

	//returns QTSS_NoErr, or appropriate posix error
	OS_Error    Open(int theType);

	uint32_t          fState;

	enum
	{
		kPortBufSizeInBytes = 8,    //uint32_t
		kMaxIPAddrSizeInBytes = 20  //uint32_t
	};

#if SOCKET_DEBUG
	StrPtrLen       fLocalAddrStr;
	char            fLocalAddrBuffer[kMaxIPAddrSizeInBytes];
#endif

	//address information (available if bound)
	//these are always stored in network order. Conver
	struct sockaddr_in  fLocalAddr;
	struct sockaddr_in  fDestAddr;

	StrPtrLen* fLocalAddrStrPtr;
	StrPtrLen* fLocalDNSStrPtr;
	char fPortBuffer[kPortBufSizeInBytes];
	StrPtrLen fPortStr;

	//State flags. Be careful when changing these values, as subclasses add their own
	enum
	{
		kBound = 0x0004,
		kConnected = 0x0008
	};

	static EventThread* sEventThread;

};

#endif // __SOCKET_H__

