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
	 File:       TCPListenerSocket.h

	 Contains:   A TCP listener socket. When a new connection comes in, the listener
				 attempts to assign the new connection to a socket object and a Task
				 object. Derived classes must implement a method of getting new
				 Task & socket objects


 */


#ifndef __TCPLISTENERSOCKET_H__
#define __TCPLISTENERSOCKET_H__

#include "TCPSocket.h"
#include "IdleTask.h"

class TCPListenerSocket : public TCPSocket, public IdleTask
{
public:

	TCPListenerSocket() : TCPSocket(nullptr, Socket::kNonBlockingSocketType), IdleTask()
	{
		this->SetTaskName("TCPListenerSocket");
	}
	~TCPListenerSocket() override = default;

	//
	// Send a TCPListenerObject a Kill event to delete it.

	//addr = listening address. port = listening port. Automatically
	//starts listening
	OS_Error        Initialize(uint32_t addr, uint16_t port);

	//You can query the listener to see if it is failing to accept
	//connections because the OS is out of descriptors.
	bool      IsOutOfDescriptors() { return fOutOfDescriptors; }

	void        SlowDown() { fSleepBetweenAccepts = true; }
	void        RunNormal() { fSleepBetweenAccepts = false; }
	//derived object must implement a way of getting tasks & sockets to this object 
	virtual Task*   GetSessionTask(TCPSocket** outSocket) = 0;

	int64_t  Run() override;

private:

	enum
	{
		kTimeBetweenAcceptsInMsec = 1000,   //uint32_t
		kListenQueueLength = 128            //uint32_t
	};

	void ProcessEvent(int eventBits) override;
	OS_Error    listen(uint32_t queueLength);

	uint32_t          fAddr{0};
	uint16_t          fPort{0};

	bool          fOutOfDescriptors{false};
	bool          fSleepBetweenAccepts{false};
};
#endif // __TCPLISTENERSOCKET_H__

