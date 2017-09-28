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
	 File:       QTSServer.cpp

	 Contains:   Implements object defined in QTSServer.h



 */


#ifndef __Win32__
#include <sys/types.h>
#include <dirent.h>
#endif

#ifndef __Win32__
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#endif

#include <algorithm>
#include <memory>

#include "QTSServer.h"

#include "SocketUtils.h"
#include "TCPListenerSocket.h"
#include "Task.h"

//Compile time modules
#include "QTSSReflectorModule.h"

#include "RTSPRequestInterface.h"
#include "RTSPSessionInterface.h"
#include "RTPSessionInterface.h"
#include "RTSPSession.h"

#include "RTPStream.h"
#include "ServerPrefs.h"

#ifdef _WIN32
#include "CreateDump.h"

LONG CrashHandler_EasyDarwin(EXCEPTION_POINTERS *pException)
{
	SYSTEMTIME	systemTime;
	GetLocalTime(&systemTime);

	char szFile[MAX_PATH] = { 0, };
	sprintf(szFile, TEXT("EasyDarwin_%04d%02d%02d %02d%02d%02d.dmp"), systemTime.wYear, systemTime.wMonth, systemTime.wDay, systemTime.wHour, systemTime.wMinute, systemTime.wSecond);
	CreateDumpFile(szFile, pException);

	return EXCEPTION_EXECUTE_HANDLER;		//·µ»ØÖµEXCEPTION_EXECUTE_HANDLER	EXCEPTION_CONTINUE_SEARCH	EXCEPTION_CONTINUE_EXECUTION
}
#endif

// CLASS DEFINITIONS

class RTSPListenerSocket : public TCPListenerSocket
{
public:

	RTSPListenerSocket() = default;
	~RTSPListenerSocket() override = default;

	//sole job of this object is to implement this function
	Task*   GetSessionTask(TCPSocket** outSocket) override;

	//check whether the Listener should be idling
	bool OverMaxConnections(uint32_t buffer);

};

class RTPSocketPool : public UDPSocketPool
{
public:

	// Pool of UDP sockets for use by the RTP server

	RTPSocketPool() = default;
	~RTPSocketPool() override = default;

	UDPSocketPair*  ConstructUDPSocketPair() override;
	void            DestructUDPSocketPair(UDPSocketPair* inPair) override;

	void            SetUDPSocketOptions(UDPSocketPair* inPair) override;
};



char*           QTSServer::sPortPrefString = "rtsp_port";

QTSServer::~QTSServer()
{
	delete fRTPMap;
	delete fReflectorSessionMap;

	delete fSocketPool;
}

bool QTSServer::Initialize(uint16_t inPortOverride, bool createListeners, const char*inAbsolutePath)
{
	static const uint32_t kRTPSessionMapSize = 2000;
	static const uint32_t kReflectorSessionMapSize = 2000;
	fServerState = qtssFatalErrorState;
	memset(sAbsolutePath, 0, MAX_PATH);
	strcpy(sAbsolutePath, inAbsolutePath);

	//
	// DICTIONARY INITIALIZATION

	RTSPRequestInterface::Initialize();

	//
	// CREATE GLOBAL OBJECTS
	fSocketPool = new RTPSocketPool();
	fRTPMap = new OSRefTable(kRTPSessionMapSize);
	fReflectorSessionMap = new OSRefTable(kReflectorSessionMapSize);

	//
	// DEFAULT IP ADDRESS & DNS NAME
	if (!this->SetDefaultIPAddr())
		return false;

	//
	// STARTUP TIME - record it
	fGMTOffset = OS::GetGMTOffset();

	//
	// BEGIN LISTENING
	if (createListeners)
		CreateListeners(false, inPortOverride);

	if (fNumListeners == 0)
		return false;

	fServerState = qtssStartingUpState;
	return true;
}

void QTSServer::InitModules(QTSS_ServerState inEndState)
{
	//
	// INVOKE INITIALIZE ROLE
	this->DoInitRole();

	if (fServerState != qtssFatalErrorState)
		fServerState = inEndState; // Server is done starting up!   
}

void QTSServer::StartTasks()
{
	//
	// Start listening
	for (uint32_t x = 0; x < fNumListeners; x++)
		fListeners[x]->RequestEvent(EV_RE);
}

bool QTSServer::SetDefaultIPAddr()
{
	fDefaultIPAddr = INADDR_ANY;
	return true;
}

/*
*
*	DESCRIBE:	Add HTTP Port Listening,Total Port Listening=RTSP Port listening + HTTP Port Listening
*	Author:		Babosa@easydarwin.org
*	Date:		2015/11/22
*
*/
bool QTSServer::CreateListeners(bool startListeningNow, uint16_t inPortOverride)
{
	struct PortTracking
	{
		PortTracking() {}

		uint16_t fPort{0};
		uint32_t fIPAddr{0};
		bool fNeedsCreating{true};
	};

	PortTracking* theRTSPPortTrackers = nullptr;
	uint32_t theTotalRTSPPortTrackers = 0;

	// Get the IP addresses from the pref
	uint32_t index = 0;

	// Stat Total Num of RTSP Port
	if (inPortOverride != 0)
	{
		theTotalRTSPPortTrackers = 1; // one port tracking struct for each IP addr
		theRTSPPortTrackers = new PortTracking[theTotalRTSPPortTrackers];
		for (index = 0; index < 1; index++)
		{
			theRTSPPortTrackers[index].fPort = inPortOverride;
			theRTSPPortTrackers[index].fIPAddr = INADDR_ANY;
		}
	}
	else
	{
		theTotalRTSPPortTrackers = 1;
		theRTSPPortTrackers = new PortTracking[theTotalRTSPPortTrackers];

		theRTSPPortTrackers[0].fPort = 554;
		theRTSPPortTrackers[0].fIPAddr = INADDR_ANY;
	}

	//
	// Now figure out which of these ports we are *already* listening on.
	// If we already are listening on that port, just move the pointer to the
	// listener over to the new array
	auto** newListenerArray = new TCPListenerSocket*[theTotalRTSPPortTrackers];
	uint32_t curPortIndex = 0;

	// RTSPPortTrackers check
	for (uint32_t count = 0; count < theTotalRTSPPortTrackers; count++)
	{
		for (uint32_t count2 = 0; count2 < fNumListeners; count2++)
		{
			if ((fListeners[count2]->GetLocalPort() == theRTSPPortTrackers[count].fPort) &&
				(fListeners[count2]->GetLocalAddr() == theRTSPPortTrackers[count].fIPAddr))
			{
				theRTSPPortTrackers[count].fNeedsCreating = false;
				newListenerArray[curPortIndex++] = fListeners[count2];
				Assert(curPortIndex <= theTotalRTSPPortTrackers);
				break;
			}
		}
	}

	// Create any new <RTSP> listeners we need
	for (uint32_t count3 = 0; count3 < theTotalRTSPPortTrackers; count3++)
	{
		if (theRTSPPortTrackers[count3].fNeedsCreating)
		{
			newListenerArray[curPortIndex] = new RTSPListenerSocket();
			QTSS_Error err = newListenerArray[curPortIndex]->Initialize(theRTSPPortTrackers[count3].fIPAddr, theRTSPPortTrackers[count3].fPort);

			//
			// If there was an error creating this listener, destroy it and log an error
			if ((startListeningNow) && (err != QTSS_NoErr))
				delete newListenerArray[curPortIndex];

			if (err == QTSS_NoErr)
			{
				//
				// This listener was successfully created.
				if (startListeningNow)
					newListenerArray[curPortIndex]->RequestEvent(EV_RE);
				curPortIndex++;
			}
		}
	}

	//
	// Kill any listeners that we no longer need
	for (uint32_t count4 = 0; count4 < fNumListeners; count4++)
	{
		bool deleteThisOne = true;

		for (uint32_t count5 = 0; count5 < curPortIndex; count5++)
		{
			if (newListenerArray[count5] == fListeners[count4])
				deleteThisOne = false;
		}

		if (deleteThisOne)
			fListeners[count4]->Signal(Task::kKillEvent);
	}

	//
	// Finally, make our server attributes and fListener privy to the new...
	fListeners = newListenerArray;
	fNumListeners = curPortIndex;

	delete[] theRTSPPortTrackers;
	return (fNumListeners > 0);
}

bool  QTSServer::SetupUDPSockets()
{
	//function finds all IP addresses on this machine, and binds 1 RTP / RTCP
	//socket pair to a port pair on each address.

	uint32_t theNumAllocatedPairs = 0;
	for (uint32_t theNumPairs = 0; theNumPairs < SocketUtils::GetNumIPAddrs(); theNumPairs++)
	{
		UDPSocketPair* thePair = fSocketPool->CreateUDPSocketPair(SocketUtils::GetIPAddr(theNumPairs), 0);
		if (thePair != nullptr)
		{
			theNumAllocatedPairs++;
			thePair->GetSocketA()->RequestEvent(EV_RE);
			thePair->GetSocketB()->RequestEvent(EV_RE);
		}
	}
	//only return an error if we couldn't allocate ANY pairs of sockets
	if (theNumAllocatedPairs == 0)
	{
		fServerState = qtssFatalErrorState; // also set the state to fatal error
		return false;
	}
	return true;
}

void QTSServer::DoInitRole()
{
	QTSS_Initialize_Params initParams;
	initParams.inServer = this;

	//
	// Add the OPTIONS method as the one method the server handles by default (it handles
	// it internally). Modules that handle other RTSP methods will add 
	supportMethods.push_back(qtssOptionsMethod);

	QTSS_Error theErr = ReflectionModule::Initialize(&initParams);
	this->SetupPublicHeader();

	OSThread::SetMainThreadData(nullptr);
}

void QTSServer::SetupPublicHeader()
{
	//
	// After the Init role, all the modules have reported the methods that they handle.
	// So, we can prune this attribute for duplicates, and construct a string to use in the
	// Public: header of the OPTIONS response
	std::sort(supportMethods.begin(), supportMethods.end());
	supportMethods.erase(std::unique(supportMethods.begin(), supportMethods.end()),
		supportMethods.end());

	for (size_t a = 0; a < supportMethods.size(); a++)
	{
		if (a) sPublicHeaderStr += ", ";
		sPublicHeaderStr += std::string(RTSPProtocol::GetMethodString(supportMethods[a]));
	}
}


Task*   RTSPListenerSocket::GetSessionTask(TCPSocket** outSocket)
{
	Assert(outSocket != nullptr);

	auto* theTask = new RTSPSession();
	*outSocket = theTask->GetSocket();  // out socket is not attached to a unix socket yet.

	if (this->OverMaxConnections(0))
		this->SlowDown();
	else
		this->RunNormal();

	return theTask;
}


bool RTSPListenerSocket::OverMaxConnections(uint32_t buffer)
{
	return false;
}

UDPSocketPair*  RTPSocketPool::ConstructUDPSocketPair()
{
	//construct a pair of UDP sockets, the lower one for RTP data (outgoing only, no demuxer
	//necessary), and one for RTCP data (incoming, so definitely need a demuxer).
	//These are nonblocking sockets that DON'T receive events (we are going to poll for data)
	// They do receive events - we don't poll from them anymore
	return new
		UDPSocketPair(new UDPSocket(nullptr, Socket::kNonBlockingSocketType),
			new UDPSocket(nullptr, Socket::kNonBlockingSocketType));
}

void RTPSocketPool::DestructUDPSocketPair(UDPSocketPair* inPair)
{
	delete inPair->GetSocketA();
	delete inPair->GetSocketB();
	delete inPair;
}

void RTPSocketPool::SetUDPSocketOptions(UDPSocketPair* inPair)
{
	// Apparently the socket buffer size matters even though this is UDP and being
	// used for sending... on UNIX typically the socket buffer size doesn't matter because the
	// packet goes right down to the driver. On Win32 and linux, unless this is really big, we get packet loss.
	inPair->GetSocketA()->SetSocketBufSize(256 * 1024);

	//
	// Always set the Rcv buf size for the RTCP sockets. This is important because the
	// server is going to be getting many many acks.
	uint32_t theRcvBufSize = ServerPrefs::GetRTCPSocketRcvBufSizeinK();

	//
	// In case the rcv buf size is too big for the system, retry, dividing the requested size by 2.
	// Until it works, or until some minimum value is reached.
	OS_Error theErr = EINVAL;
	while ((theErr != OS_NoErr) && (theRcvBufSize > 32))
	{
		theErr = inPair->GetSocketB()->SetSocketRcvBufSize(theRcvBufSize * 1024);
		if (theErr != OS_NoErr)
			theRcvBufSize >>= 1;
	}
}