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

#include "QTSSModuleUtils.h"

 //Compile time modules
#include "QTSSReflectorModule.h"

#include "RTSPRequestInterface.h"
#include "RTSPSessionInterface.h"
#include "RTPSessionInterface.h"
#include "RTSPSession.h"

#include "RTPStream.h"
#include "RTCPTask.h"

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
XMLPrefsParser* QTSServer::sPrefsSource = nullptr;
PrefsSource*    QTSServer::sMessagesSource = nullptr;

QTSServer::~QTSServer()
{
	//
	// Grab the server mutex. This is to make sure all gets & set values on this
	// object complete before we start deleting stuff
	auto* serverlocker = new OSMutexLocker(this->GetServerObjectMutex());

	//
	// Grab the prefs mutex. This is to make sure we can't reread prefs
	// WHILE shutting down, which would cause some weirdness for QTSS API
	// (some modules could get QTSS_RereadPrefs_Role after QTSS_Shutdown, which would be bad)
	auto* locker = new OSMutexLocker(this->GetPrefs()->GetMutex());

	ReflectionModule::Shutdown();

	OSThread::SetMainThreadData(nullptr);

	delete fRTPMap;
	delete fReflectorSessionMap;

	delete fSocketPool;
	delete fSrvrMessages;
	delete locker;
	delete serverlocker;
	delete fSrvrPrefs;
}

bool QTSServer::Initialize(XMLPrefsParser* inPrefsSource, PrefsSource* inMessagesSource, uint16_t inPortOverride, bool createListeners, const char*inAbsolutePath)
{
	static const uint32_t kRTPSessionMapSize = 2000;
	static const uint32_t kReflectorSessionMapSize = 2000;
	fServerState = qtssFatalErrorState;
	sPrefsSource = inPrefsSource;
	sMessagesSource = inMessagesSource;
	memset(sAbsolutePath, 0, MAX_PATH);
	strcpy(sAbsolutePath, inAbsolutePath);

	//
	// DICTIONARY INITIALIZATION

	QTSServerPrefs::Initialize();
	QTSSMessages::Initialize();
	RTSPRequestInterface::Initialize();

	RTSPSessionInterface::Initialize();
	RTSPSession::Initialize();
	QTSSUserProfile::Initialize();

	//
	// STUB SERVER INITIALIZATION
	//
	// Construct stub versions of the prefs and messages dictionaries. We need
	// both of these to initialize the server, but they have to be stubs because
	// their QTSSDictionaryMaps will presumably be modified when modules get loaded.

	fSrvrPrefs = new QTSServerPrefs(inPrefsSource, false); // First time, don't write changes to the prefs file
	fSrvrMessages = new QTSSMessages(inMessagesSource);
	QTSSModuleUtils::Initialize(fSrvrMessages, this, QTSServerInterface::GetErrorLogStream());

	//
	// SETUP ASSERT BEHAVIOR
	//
	// Depending on the server preference, we will either break when we hit an
	// assert, or log the assert to the error log
	if (!fSrvrPrefs->ShouldServerBreakOnAssert())
		SetAssertLogger(this->GetErrorLogStream());// the error log stream is our assert logger

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
	fStartupTime_UnixMilli = OS::Milliseconds();
	fGMTOffset = OS::GetGMTOffset();

	//
	// BEGIN LISTENING
	if (createListeners)
	{
		if (!this->CreateListeners(false, fSrvrPrefs, inPortOverride))
			QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgSomePortsFailed, 0);
	}

	if (fNumListeners == 0)
	{
		if (createListeners)
			QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgNoPortsSucceeded, 0);
		return false;
	}

	fServerState = qtssStartingUpState;
	return true;
}

void QTSServer::InitModules(QTSS_ServerState inEndState)
{
	//
	// LOAD AND INITIALIZE ALL MODULES

	// temporarily set the verbosity on missing prefs when starting up to debug level
	// This keeps all the pref messages being written to the config file from being logged.
	// don't exit until the verbosity level is reset back to the initial prefs.

	fSrvrPrefs->SetErrorLogVerbosity(qtssWarningVerbosity); // turn off info messages while initializing compiled in modules.
   //
	// CREATE MODULE OBJECTS AND READ IN MODULE PREFS

	// Finish setting up modules. Create our final prefs & messages objects,
	// register all global dictionaries, and invoke the modules in their Init roles.
	fStubSrvrPrefs = fSrvrPrefs;
	fStubSrvrMessages = fSrvrMessages;

	fSrvrPrefs = new QTSServerPrefs(sPrefsSource, true); // Now write changes to the prefs file. First time, we don't because the error messages won't get printed.
	QTSS_ErrorVerbosity serverLevel = fSrvrPrefs->GetErrorLogVerbosity(); // get the real prefs verbosity and save it.
	fSrvrPrefs->SetErrorLogVerbosity(qtssWarningVerbosity); // turn off info messages while loading dynamic modules


	fSrvrMessages = new QTSSMessages(sMessagesSource);
	QTSSModuleUtils::Initialize(fSrvrMessages, this, QTSServerInterface::GetErrorLogStream());

	this->SetVal(qtssSvrMessages, &fSrvrMessages, sizeof(fSrvrMessages));
	this->SetVal(qtssSvrPreferences, &fSrvrPrefs, sizeof(fSrvrPrefs));

	//
	// ADD REREAD PREFERENCES SERVICE
	(void)QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kServiceDictIndex)->
		AddAttribute(QTSS_REREAD_PREFS_SERVICE, (QTSS_AttrFunctionPtr)QTSServer::RereadPrefsService, qtssAttrDataTypeUnknown, qtssAttrModeRead);

	//
	// INVOKE INITIALIZE ROLE
	this->DoInitRole();

	if (fServerState != qtssFatalErrorState)
		fServerState = inEndState; // Server is done starting up!   


	fSrvrPrefs->SetErrorLogVerbosity(serverLevel); // reset the server's verbosity back to the original prefs level.
}

void QTSServer::StartTasks()
{
	fRTCPTask = new RTCPTask();
	fStatsTask = new RTPStatsUpdaterTask();

	//
	// Start listening
	for (uint32_t x = 0; x < fNumListeners; x++)
		fListeners[x]->RequestEvent(EV_RE);
}

bool QTSServer::SetDefaultIPAddr()
{
	//check to make sure there is an available ip interface
	if (SocketUtils::GetNumIPAddrs() == 0)
	{
		QTSSModuleUtils::LogError(qtssFatalVerbosity, qtssMsgNotConfiguredForIP, 0);
		return false;
	}

	//find out what our default IP addr is & dns name
	uint32_t theNumAddrs = 0;
	uint32_t* theIPAddrs = this->GetRTSPIPAddrs(fSrvrPrefs, &theNumAddrs);
	if (theNumAddrs == 1)
		fDefaultIPAddr = SocketUtils::GetIPAddr(0);
	else
		fDefaultIPAddr = theIPAddrs[0];
	delete[] theIPAddrs;

	for (uint32_t ipAddrIter = 0; ipAddrIter < SocketUtils::GetNumIPAddrs(); ipAddrIter++)
	{
		if (SocketUtils::GetIPAddr(ipAddrIter) == fDefaultIPAddr)
		{
			this->SetVal(qtssSvrDefaultDNSName, SocketUtils::GetDNSNameStr(ipAddrIter));
			Assert(this->GetValue(qtssSvrDefaultDNSName)->Ptr != nullptr);
			this->SetVal(qtssSvrDefaultIPAddrStr, SocketUtils::GetIPAddrStr(ipAddrIter));
			Assert(this->GetValue(qtssSvrDefaultDNSName)->Ptr != nullptr);
			break;
		}
	}
	if (this->GetValue(qtssSvrDefaultDNSName)->Ptr == nullptr)
	{
		//If we've gotten here, what has probably happened is the IP address (explicitly
		//entered as a preference) doesn't exist
		QTSSModuleUtils::LogError(qtssFatalVerbosity, qtssMsgDefaultRTSPAddrUnavail, 0);
		return false;
	}
	return true;
}

/*
*
*	DESCRIBE:	Add HTTP Port Listening,Total Port Listening=RTSP Port listening + HTTP Port Listening
*	Author:		Babosa@easydarwin.org
*	Date:		2015/11/22
*
*/
bool QTSServer::CreateListeners(bool startListeningNow, QTSServerPrefs* inPrefs, uint16_t inPortOverride)
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
	uint32_t theNumAddrs = 0;
	uint32_t* theIPAddrs = this->GetRTSPIPAddrs(inPrefs, &theNumAddrs);
	uint32_t index = 0;

	// Stat Total Num of RTSP Port
	if (inPortOverride != 0)
	{
		theTotalRTSPPortTrackers = theNumAddrs; // one port tracking struct for each IP addr
		theRTSPPortTrackers = new PortTracking[theTotalRTSPPortTrackers];
		for (index = 0; index < theNumAddrs; index++)
		{
			theRTSPPortTrackers[index].fPort = inPortOverride;
			theRTSPPortTrackers[index].fIPAddr = theIPAddrs[index];
		}
	}
	else
	{
		uint32_t theNumPorts = 0;
		uint16_t* thePorts = GetRTSPPorts(inPrefs, &theNumPorts);
		theTotalRTSPPortTrackers = theNumAddrs * theNumPorts;
		theRTSPPortTrackers = new PortTracking[theTotalRTSPPortTrackers];

		uint32_t currentIndex = 0;

		for (index = 0; index < theNumAddrs; index++)
		{
			for (uint32_t portIndex = 0; portIndex < theNumPorts; portIndex++)
			{
				currentIndex = (theNumPorts * index) + portIndex;

				theRTSPPortTrackers[currentIndex].fPort = thePorts[portIndex];
				theRTSPPortTrackers[currentIndex].fIPAddr = theIPAddrs[index];
			}
		}

		delete[] thePorts;
	}

	delete[] theIPAddrs;
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

			char thePortStr[20];
			sprintf(thePortStr, "%hu", theRTSPPortTrackers[count3].fPort);

			//
			// If there was an error creating this listener, destroy it and log an error
			if ((startListeningNow) && (err != QTSS_NoErr))
				delete newListenerArray[curPortIndex];

			if (err == EADDRINUSE)
				QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssListenPortInUse, 0, thePortStr);
			else if (err == EACCES)
				QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssListenPortAccessDenied, 0, thePortStr);
			else if (err != QTSS_NoErr)
				QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssListenPortError, 0, thePortStr);
			else
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
	uint32_t portIndex = 0;

	for (uint32_t count6 = 0; count6 < fNumListeners; count6++)
	{
		if (fListeners[count6]->GetLocalAddr() != INADDR_LOOPBACK)
		{
			uint16_t thePort = fListeners[count6]->GetLocalPort();
			(void)this->SetValue(qtssSvrRTSPPorts, portIndex, &thePort, sizeof(thePort), QTSSDictionary::kDontObeyReadOnly);
			portIndex++;
		}
	}
	this->SetNumValues(qtssSvrRTSPPorts, portIndex);

	delete[] theRTSPPortTrackers;
	return (fNumListeners > 0);
}

uint32_t* QTSServer::GetRTSPIPAddrs(QTSServerPrefs* inPrefs, uint32_t* outNumAddrsPtr)
{
	uint32_t numAddrs = inPrefs->GetNumValues(qtssPrefsRTSPIPAddr);
	uint32_t* theIPAddrArray = nullptr;

	if (numAddrs == 0)
	{
		*outNumAddrsPtr = 1;
		theIPAddrArray = new uint32_t[1];
		theIPAddrArray[0] = INADDR_ANY;
	}
	else
	{
		theIPAddrArray = new uint32_t[numAddrs + 1];
		uint32_t arrIndex = 0;

		for (uint32_t theIndex = 0; theIndex < numAddrs; theIndex++)
		{
			// Get the ip addr out of the prefs dictionary
			QTSS_Error theErr = QTSS_NoErr;

			char* theIPAddrStr = nullptr;
			theErr = inPrefs->GetValueAsString(qtssPrefsRTSPIPAddr, theIndex, &theIPAddrStr);
			if (theErr != QTSS_NoErr)
			{
				delete[] theIPAddrStr;
				break;
			}


			uint32_t theIPAddr = 0;
			if (theIPAddrStr != nullptr)
			{
				theIPAddr = SocketUtils::ConvertStringToAddr(theIPAddrStr);
				delete[] theIPAddrStr;

				if (theIPAddr != 0)
					theIPAddrArray[arrIndex++] = theIPAddr;
			}
		}

		if ((numAddrs == 1) && (arrIndex == 0))
			theIPAddrArray[arrIndex++] = INADDR_ANY;
		else
			theIPAddrArray[arrIndex++] = INADDR_LOOPBACK;

		*outNumAddrsPtr = arrIndex;
	}

	return theIPAddrArray;
}

uint16_t* QTSServer::GetRTSPPorts(QTSServerPrefs* inPrefs, uint32_t* outNumPortsPtr)
{
	*outNumPortsPtr = inPrefs->GetNumValues(qtssPrefsRTSPPorts);

	if (*outNumPortsPtr == 0)
		return nullptr;

	auto* thePortArray = new uint16_t[*outNumPortsPtr];

	for (uint32_t theIndex = 0; theIndex < *outNumPortsPtr; theIndex++)
	{
		// Get the ip addr out of the prefs dictionary
		uint32_t theLen = sizeof(uint16_t);
		QTSS_Error theErr = QTSS_NoErr;
		theErr = inPrefs->GetValue(qtssPrefsRTSPPorts, theIndex, &thePortArray[theIndex], &theLen);
		Assert(theErr == QTSS_NoErr);
	}

	return thePortArray;
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

bool  QTSServer::SwitchPersonality()
{
#ifndef __Win32__  //not supported
	std::unique_ptr<char[]> runGroupName(fSrvrPrefs->GetRunGroupName());
	std::unique_ptr<char[]> runUserName(fSrvrPrefs->GetRunUserName());

	int groupID = 0;

	if (::strlen(runGroupName.get()) > 0)
	{
		struct group* gr = ::getgrnam(runGroupName.get());
		if (gr == nullptr || ::setgid(gr->gr_gid) == -1)
		{
#define kErrorStrSize 256
			char buffer[kErrorStrSize];

			::strncpy(buffer, ::strerror(OSThread::GetErrno()), kErrorStrSize);
			buffer[kErrorStrSize - 1] = 0;  //make sure it is null terminated even if truncated.
			QTSSModuleUtils::LogError(qtssFatalVerbosity, qtssMsgCannotSetRunGroup, 0,
				runGroupName.get(), buffer);
			return false;
		}
		groupID = gr->gr_gid;
	}

	if (::strlen(runUserName.get()) > 0)
	{
		struct passwd* pw = ::getpwnam(runUserName.get());

#if __MacOSX__
		if (pw != nullptr && groupID != 0) //call initgroups before doing a setuid
			(void) initgroups(runUserName.get(), groupID);
#endif  

		if (pw == nullptr || ::setuid(pw->pw_uid) == -1)
		{
			QTSSModuleUtils::LogError(qtssFatalVerbosity, qtssMsgCannotSetRunUser, 0,
				runUserName.get(), strerror(OSThread::GetErrno()));
			return false;
		}
	}

#endif  
	return true;
}

void QTSServer::DoInitRole()
{
	QTSS_Initialize_Params initParams;
	initParams.inServer = this;
	initParams.inPrefs = fSrvrPrefs;
	initParams.inMessages = fSrvrMessages;
	initParams.inErrorLogStream = &sErrorLogStream;


	//
	// Add the OPTIONS method as the one method the server handles by default (it handles
	// it internally). Modules that handle other RTSP methods will add 
	supportMethods.push_back(qtssOptionsMethod);

	initParams.inModule = nullptr;
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
	QTSServerInterface* theServer = QTSServerInterface::GetServer();
	int32_t maxConns = theServer->GetPrefs()->GetMaxConnections();
	bool overLimit = false;

	if (maxConns > -1) // limit connections
	{
		maxConns += buffer;
		if ((theServer->GetNumRTPSessions() > (uint32_t)maxConns)
			||
			(theServer->GetNumRTSPSessions() + theServer->GetNumRTSPHTTPSessions() > (uint32_t)maxConns)
			)
		{
			overLimit = true;
		}
	}
	return overLimit;

}

UDPSocketPair*  RTPSocketPool::ConstructUDPSocketPair()
{
	Task* theTask = ((QTSServer*)QTSServerInterface::GetServer())->fRTCPTask;

	//construct a pair of UDP sockets, the lower one for RTP data (outgoing only, no demuxer
	//necessary), and one for RTCP data (incoming, so definitely need a demuxer).
	//These are nonblocking sockets that DON'T receive events (we are going to poll for data)
	// They do receive events - we don't poll from them anymore
	return new
		UDPSocketPair(new UDPSocket(theTask, Socket::kNonBlockingSocketType),
			new UDPSocket(theTask, UDPSocket::kWantsDemuxer | Socket::kNonBlockingSocketType));
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
	uint32_t theRcvBufSize = QTSServerInterface::GetServer()->GetPrefs()->GetRTCPSocketRcvBufSizeinK();

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

	//
	// Report an error if we couldn't set the socket buffer size the user requested
	if (theRcvBufSize != QTSServerInterface::GetServer()->GetPrefs()->GetRTCPSocketRcvBufSizeinK())
	{
		char theRcvBufSizeStr[20];
		sprintf(theRcvBufSizeStr, "%"   _U32BITARG_   "", theRcvBufSize);
		//
		// For now, do not log an error, though we should enable this in the future.
		//QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgSockBufSizesTooLarge, theRcvBufSizeStr);
	}
}



QTSS_Error QTSServer::RereadPrefsService(QTSS_ServiceFunctionArgsPtr /*inArgs*/)
{
	//
	// This function can only be called safely when the server is completely running.
	// Ensuring this is a bit complicated because of preemption. Here's how it's done...

	QTSServerInterface* theServer = QTSServerInterface::GetServer();

	// This is to make sure this function isn't being called before the server is
	// completely started up.
	if ((theServer == nullptr) || (theServer->GetServerState() != qtssRunningState))
		return QTSS_OutOfState;

	// Because the server must have started up, and because this object always stays
	// around (until the process dies), we can now safely get this object.
	QTSServerPrefs* thePrefs = theServer->GetPrefs();

	// Grab the prefs mutex. We want to make sure that calls to RereadPrefsService
	// are serialized. This also prevents the server from shutting down while in
	// this function, because the QTSServer destructor grabs this mutex as well.
	OSMutexLocker locker(thePrefs->GetMutex());

	// Finally, check the server state again. The state may have changed
	// to qtssShuttingDownState or qtssFatalErrorState in this time, though
	// at this point we have the prefs mutex, so we are guarenteed that the
	// server can't actually shut down anymore
	if (theServer->GetServerState() != qtssRunningState)
		return QTSS_OutOfState;

	// Ok, we're ready to reread preferences now.

	//
	// Reread preferences
	sPrefsSource->Parse();
	thePrefs->RereadServerPreferences(true);

	{
		//
		// Update listeners, ports, and IP addrs.
		OSMutexLocker locker(theServer->GetServerObjectMutex());
		(void)((QTSServer*)theServer)->SetDefaultIPAddr();
		(void)((QTSServer*)theServer)->CreateListeners(true, thePrefs, 0);
	}

	// Delete all the streams
	uint32_t theLen = 0;

	//
	// Go through each module's prefs object and have those reread as well

	//
	// Now that we are done rereading the prefs, invoke all modules in the RereadPrefs
	// role so they can update their internal prefs caches.
	ReflectionModule::RereadPrefs();
	return QTSS_NoErr;
}


