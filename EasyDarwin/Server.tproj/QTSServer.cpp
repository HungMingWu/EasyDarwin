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

#include <memory>

#include "QTSServer.h"
#include "Format.h"

#include "SocketUtils.h"
#include "TCPListenerSocket.h"
#include "Task.h"

#include "QTSS_Private.h"
#include "QTSSCallbacks.h"
#include "QTSSModuleUtils.h"

 //Compile time modules
#include "QTSSErrorLogModule.h"
#include "QTSSAccessLogModule.h"
#include "QTSSFlowControlModule.h"
#include "QTSSReflectorModule.h"
#include "EasyCMSModule.h"
#include "EasyRedisModule.h"
#ifdef PROXYSERVER
#include "QTSSProxyModule.h"
#endif
#include "QTSSPosixFileSysModule.h"
#include "QTSSAccessModule.h"
#if MEMORY_DEBUGGING
#include "QTSSWebDebugModule.h"
#endif

#include "RTSPRequestInterface.h"
#include "RTSPSessionInterface.h"
#include "RTPSessionInterface.h"
#include "RTSPSession.h"
#include "HTTPSession.h"

#include "RTPStream.h"
#include "RTCPTask.h"
#include "QTSSFile.h"

#ifdef _WIN32
#include "CreateDump.h"

LONG CrashHandler_EasyDarwin(EXCEPTION_POINTERS *pException)
{
	SYSTEMTIME	systemTime;
	GetLocalTime(&systemTime);

	char szFile[MAX_PATH] = { 0, };
	sprintf(szFile, TEXT("EasyDarwin_%04d%02d%02d %02d%02d%02d.dmp"), systemTime.wYear, systemTime.wMonth, systemTime.wDay, systemTime.wHour, systemTime.wMinute, systemTime.wSecond);
	CreateDumpFile(szFile, pException);

	return EXCEPTION_EXECUTE_HANDLER;		//����ֵEXCEPTION_EXECUTE_HANDLER	EXCEPTION_CONTINUE_SEARCH	EXCEPTION_CONTINUE_EXECUTION
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

class HTTPListenerSocket : public TCPListenerSocket
{
public:

	HTTPListenerSocket() = default;
	~HTTPListenerSocket() override = default;

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
QTSS_Callbacks  QTSServer::sCallbacks;
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

	QTSS_ModuleState theModuleState;
	theModuleState.curRole = QTSS_Shutdown_Role;
	theModuleState.curTask = nullptr;
	OSThread::SetMainThreadData(&theModuleState);

	for (uint32_t x = 0; x < QTSServerInterface::GetNumModulesInRole(QTSSModule::kShutdownRole); x++)
		(void)QTSServerInterface::GetModule(QTSSModule::kShutdownRole, x)->CallDispatch(QTSS_Shutdown_Role, nullptr);

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
	this->InitCallbacks();

	//
	// DICTIONARY INITIALIZATION

	QTSSModule::Initialize();
	QTSServerPrefs::Initialize();
	QTSSMessages::Initialize();
	RTSPRequestInterface::Initialize();
	HTTPSessionInterface::Initialize();

	RTSPSessionInterface::Initialize();
	RTPSessionInterface::Initialize();
	RTPStream::Initialize();
	RTSPSession::Initialize();
	QTSSFile::Initialize();
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
	// Load ERROR LOG module only. This is good in case there is a startup error.

	auto* theLoggingModule = new QTSSModule("QTSSErrorLogModule");
	(void)theLoggingModule->SetupModule(&sCallbacks, &QTSSErrorLogModule_Main);
	(void)AddModule(theLoggingModule);
	this->BuildModuleRoleArrays();

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

	LoadModules(fSrvrPrefs);
	LoadCompiledInModules();
	this->BuildModuleRoleArrays();

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

	PortTracking* theHTTPPortTrackers = nullptr;
	uint32_t theTotalHTTPPortTrackers = 0;

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

	// Stat Total Num of HTTP Port
	{
		theTotalHTTPPortTrackers = theNumAddrs;
		theHTTPPortTrackers = new PortTracking[theTotalHTTPPortTrackers];

		uint16_t theHTTPPort = inPrefs->GetServiceLanPort();
		uint32_t currentIndex = 0;

		for (index = 0; index < theNumAddrs; index++)
		{
			theHTTPPortTrackers[index].fPort = theHTTPPort;
			theHTTPPortTrackers[index].fIPAddr = theIPAddrs[index];
		}
	}

	delete[] theIPAddrs;
	//
	// Now figure out which of these ports we are *already* listening on.
	// If we already are listening on that port, just move the pointer to the
	// listener over to the new array
	auto** newListenerArray = new TCPListenerSocket*[theTotalRTSPPortTrackers + theTotalHTTPPortTrackers];
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

	// HTTPPortTrackers check
	for (uint32_t count = 0; count < theTotalHTTPPortTrackers; count++)
	{
		for (uint32_t count2 = 0; count2 < fNumListeners; count2++)
		{
			if ((fListeners[count2]->GetLocalPort() == theHTTPPortTrackers[count].fPort) &&
				(fListeners[count2]->GetLocalAddr() == theHTTPPortTrackers[count].fIPAddr))
			{
				theHTTPPortTrackers[count].fNeedsCreating = false;
				newListenerArray[curPortIndex++] = fListeners[count2];
				Assert(curPortIndex <= theTotalRTSPPortTrackers + theTotalHTTPPortTrackers);
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

	// Create any new <HTTP> listeners we need
	for (uint32_t count3 = 0; count3 < theTotalHTTPPortTrackers; count3++)
	{
		if (theHTTPPortTrackers[count3].fNeedsCreating)
		{
			newListenerArray[curPortIndex] = new HTTPListenerSocket();
			QTSS_Error err = newListenerArray[curPortIndex]->Initialize(theHTTPPortTrackers[count3].fIPAddr, theHTTPPortTrackers[count3].fPort);

			char thePortStr[20];
			sprintf(thePortStr, "%hu", theHTTPPortTrackers[count3].fPort);

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
	delete[] theHTTPPortTrackers;
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

void    QTSServer::LoadCompiledInModules()
{
#ifndef DSS_DYNAMIC_MODULES_ONLY
	// MODULE DEVELOPERS SHOULD ADD THE FOLLOWING THREE LINES OF CODE TO THIS
	// FUNCTION IF THEIR MODULE IS BEING COMPILED INTO THE SERVER.
	//
	// QTSSModule* myModule = new QTSSModule("__MODULE_NAME__");
	// (void)myModule->Initialize(&sCallbacks, &__MODULE_MAIN_ROUTINE__);
	// (void)AddModule(myModule);

	auto* theReflectorModule = new QTSSModule("QTSSReflectorModule");
	(void)theReflectorModule->SetupModule(&sCallbacks, &QTSSReflectorModule_Main);
	(void)AddModule(theReflectorModule);

	auto* theAccessLog = new QTSSModule("QTSSAccessLogModule");
	(void)theAccessLog->SetupModule(&sCallbacks, &QTSSAccessLogModule_Main);
	(void)AddModule(theAccessLog);

	auto* theFlowControl = new QTSSModule("QTSSFlowControlModule");
	(void)theFlowControl->SetupModule(&sCallbacks, &QTSSFlowControlModule_Main);
	(void)AddModule(theFlowControl);

	auto* theFileSysModule = new QTSSModule("QTSSPosixFileSysModule");
	(void)theFileSysModule->SetupModule(&sCallbacks, &QTSSPosixFileSysModule_Main);
	(void)AddModule(theFileSysModule);

	if (this->GetPrefs()->CloudPlatformEnabled())
	{
		auto* theCMSModule = new QTSSModule("EasyCMSModule");
		(void)theCMSModule->SetupModule(&sCallbacks, &EasyCMSModule_Main);
		(void)AddModule(theCMSModule);

		auto* theRedisModule = new QTSSModule("EasyRedisModule");
		(void)theRedisModule->SetupModule(&sCallbacks, &EasyRedisModule_Main);
		(void)AddModule(theRedisModule);
	}

#if MEMORY_DEBUGGING
	QTSSModule* theWebDebug = new QTSSModule("QTSSWebDebugModule");
	(void)theWebDebug->SetupModule(&sCallbacks, &QTSSWebDebugModule_Main);
	(void)AddModule(theWebDebug);
#endif

#ifdef __MacOSX__
	QTSSModule* theQTSSDSAuthModule = new QTSSModule("QTSSDSAuthModule");
	(void)theQTSSDSAuthModule->SetupModule(&sCallbacks, &QTSSDSAuthModule_Main);
	(void)AddModule(theQTSSDSAuthModule);
#endif

	auto* theQTACCESSmodule = new QTSSModule("QTSSAccessModule");
	(void)theQTACCESSmodule->SetupModule(&sCallbacks, &QTSSAccessModule_Main);
	(void)AddModule(theQTACCESSmodule);

#endif //DSS_DYNAMIC_MODULES_ONLY

#ifdef PROXYSERVER
	QTSSModule* theProxyModule = new QTSSModule("QTSSProxyModule");
	(void)theProxyModule->SetupModule(&sCallbacks, &QTSSProxyModule_Main);
	(void)AddModule(theProxyModule);
#endif

#ifdef _WIN32
	SetUnhandledExceptionFilter(reinterpret_cast<LPTOP_LEVEL_EXCEPTION_FILTER>(CrashHandler_EasyDarwin));
#endif

}

void    QTSServer::InitCallbacks()
{
	sCallbacks.addr[kConvertToUnixTimeCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_ConvertToUnixTime;

	sCallbacks.addr[kAddRoleCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_AddRole;
	sCallbacks.addr[kIDForTagCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_IDForAttr;

	sCallbacks.addr[kSetAttributeByIDCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_SetValue;
	sCallbacks.addr[kGetNumValuesCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_GetNumValues;

	sCallbacks.addr[kWriteCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_Write;
	sCallbacks.addr[kWriteVCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_WriteV;
	sCallbacks.addr[kFlushCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_Flush;
	sCallbacks.addr[kReadCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_Read;

	sCallbacks.addr[kAddServiceCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_AddService;
	sCallbacks.addr[kIDForServiceCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_IDForService;
	sCallbacks.addr[kDoServiceCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_DoService;

	sCallbacks.addr[kSendRTSPHeadersCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_SendRTSPHeaders;
	sCallbacks.addr[kAppendRTSPHeadersCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_AppendRTSPHeader;

	sCallbacks.addr[kRequestEventCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_RequestEvent;
	sCallbacks.addr[kSetIdleTimerCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_SetIdleTimer;
	sCallbacks.addr[kSignalStreamCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_SignalStream;

	sCallbacks.addr[kOpenFileObjectCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_OpenFileObject;
	sCallbacks.addr[kCloseFileObjectCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_CloseFileObject;

	sCallbacks.addr[kAddStaticAttributeCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_AddStaticAttribute;
	sCallbacks.addr[kAddInstanceAttributeCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_AddInstanceAttribute;
	sCallbacks.addr[kRemoveInstanceAttributeCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_RemoveInstanceAttribute;

	sCallbacks.addr[kGetAttrInfoByIndexCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_GetAttrInfoByIndex;
	sCallbacks.addr[kGetAttrInfoByNameCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_GetAttrInfoByName;
	sCallbacks.addr[kGetAttrInfoByIDCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_GetAttrInfoByID;
	sCallbacks.addr[kGetNumAttributesCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_GetNumAttributes;

	sCallbacks.addr[kRemoveValueCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_RemoveValue;

	sCallbacks.addr[kRequestGlobalLockCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_RequestLockedCallback;
	sCallbacks.addr[kIsGlobalLockedCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_IsGlobalLocked;

	sCallbacks.addr[kAuthenticateCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_Authenticate;
	sCallbacks.addr[kAuthorizeCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_Authorize;

	sCallbacks.addr[kLockObjectCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_LockObject;
	sCallbacks.addr[kUnlockObjectCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_UnlockObject;
	sCallbacks.addr[kSetAttributePtrCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_SetValuePtr;

	sCallbacks.addr[kSetIntervalRoleTimerCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::QTSS_SetIdleRoleTimer;

	sCallbacks.addr[kGetRTSPPushSessionsCallback] = (QTSS_CallbackProcPtr)QTSSCallbacks::Easy_GetRTSPPushSessions;

}

void QTSServer::LoadModules(QTSServerPrefs* inPrefs)
{
	// Fetch the name of the module directory and open it.
	std::unique_ptr<char[]> theModDirName(inPrefs->GetModuleDirectory());

#ifdef __Win32__
	// NT doesn't seem to have support for the POSIX directory parsing APIs.
	std::unique_ptr<char[]> theLargeModDirName(new char[::strlen(theModDirName.get()) + 3]);
	::strcpy(theLargeModDirName.get(), theModDirName.get());
	::strcat(theLargeModDirName.get(), "\\*");

	WIN32_FIND_DATA theFindData;
	HANDLE theSearchHandle = ::FindFirstFile(theLargeModDirName.get(), &theFindData);

	if (theSearchHandle == INVALID_HANDLE_VALUE)
	{
		QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgNoModuleFolder, 0);
		return;
	}

	while (theSearchHandle != INVALID_HANDLE_VALUE)
	{
		this->CreateModule(theModDirName.get(), theFindData.cFileName);

		if (!::FindNextFile(theSearchHandle, &theFindData))
		{
			::FindClose(theSearchHandle);
			theSearchHandle = INVALID_HANDLE_VALUE;
		}
	}
#else       

	// POSIX version
	// opendir mallocs memory for DIR* so call closedir to free the allocated memory
	DIR* theDir = ::opendir(theModDirName.get());
	if (theDir == nullptr)
	{
		QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgNoModuleFolder, 0);
		return;
	}

	while (true)
	{
		// Iterate over each file in the directory, attempting to construct
		// a module object from that file.

		struct dirent* theFile = ::readdir(theDir);
		if (theFile == nullptr)
			break;

		this->CreateModule(theModDirName.get(), theFile->d_name);
	}

	(void)::closedir(theDir);

#endif
}

void    QTSServer::CreateModule(char* inModuleFolderPath, char* inModuleName)
{
	// Ignore these silly directory names

	if (::strcmp(inModuleName, ".") == 0)
		return;
	if (::strcmp(inModuleName, "..") == 0)
		return;
	if (::strlen(inModuleName) == 0)
		return;
	if (*inModuleName == '.')
		return; // Fix 2572248. Do not attempt to load '.' files as modules at all 

	//
	// Construct a full path to this module
	uint32_t totPathLen = ::strlen(inModuleFolderPath) + ::strlen(inModuleName);
	std::unique_ptr<char[]> theModPath(new char[totPathLen + 4]);
	::strcpy(theModPath.get(), inModuleFolderPath);
	::strcat(theModPath.get(), kPathDelimiterString);
	::strcat(theModPath.get(), inModuleName);

	//
	// Construct a QTSSModule object, and attempt to initialize the module
	auto* theNewModule = new QTSSModule(inModuleName, theModPath.get());
	QTSS_Error theErr = theNewModule->SetupModule(&sCallbacks);

	if (theErr != QTSS_NoErr)
	{
		QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgBadModule, theErr,
			inModuleName);
		delete theNewModule;
	}
	//
	// If the module was successfully initialized, add it to our module queue
	else if (!this->AddModule(theNewModule))
	{
		QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgRegFailed, theErr,
			inModuleName);
		delete theNewModule;
	}
}

bool QTSServer::AddModule(QTSSModule* inModule)
{
	Assert(inModule->IsInitialized());

	// Prepare to invoke the module's Register role. Setup the Register param block
	QTSS_ModuleState theModuleState;

	theModuleState.curModule = inModule;
	theModuleState.curRole = QTSS_Register_Role;
	theModuleState.curTask = nullptr;
	OSThread::SetMainThreadData(&theModuleState);

	// Currently we do nothing with the module name
	QTSS_RoleParams theRegParams;
	theRegParams.regParams.outModuleName[0] = 0;

	// If the module returns an error from the QTSS_Register role, don't put it anywhere
	if (inModule->CallDispatch(QTSS_Register_Role, &theRegParams) != QTSS_NoErr)
	{
		//Log 
		char msgStr[2048];
		char* moduleName = nullptr;
		(void)inModule->GetValueAsString(qtssModName, 0, &moduleName);
		snprintf(msgStr, sizeof(msgStr), "Loading Module [%s] Failed!", moduleName);
		delete moduleName;
		QTSServerInterface::LogError(qtssMessageVerbosity, msgStr);
		return false;
	}

	OSThread::SetMainThreadData(nullptr);

	//
	// Update the module name to reflect what was returned from the register role
	theRegParams.regParams.outModuleName[QTSS_MAX_MODULE_NAME_LENGTH - 1] = 0;
	if (theRegParams.regParams.outModuleName[0] != 0)
		inModule->SetValue(qtssModName, 0, theRegParams.regParams.outModuleName, ::strlen(theRegParams.regParams.outModuleName), false);

	//
	// Give the module object a prefs dictionary. Instance attributes are allowed for these objects.
	auto* thePrefs = new QTSSPrefs(sPrefsSource, inModule->GetValue(qtssModName), QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kModulePrefsDictIndex), true);
	thePrefs->RereadPreferences();
	inModule->SetPrefsDict(thePrefs);

	//
	// Add this module to the array of module (dictionaries)
	uint32_t theNumModules = this->GetNumValues(qtssSvrModuleObjects);
	QTSS_Error theErr = this->SetValue(qtssSvrModuleObjects, theNumModules, &inModule, sizeof(QTSSModule*), QTSSDictionary::kDontObeyReadOnly);
	Assert(theErr == QTSS_NoErr);

	//
	// Add this module to the module queue
	sModuleQueue.EnQueue(inModule->GetQueueElem());

	return true;
}

void QTSServer::BuildModuleRoleArrays()
{
	OSQueueIter theIter(&sModuleQueue);
	QTSSModule* theModule = nullptr;

	// Make sure these variables are cleaned out in case they've already been inited.

	DestroyModuleRoleArrays();

	// Loop through all the roles of all the modules, recording the number of
	// modules in each role, and also recording which modules are doing what.

	for (uint32_t x = 0; x < QTSSModule::kNumRoles; x++)
	{
		sNumModulesInRole[x] = 0;
		for (theIter.Reset(); !theIter.IsDone(); theIter.Next())
		{
			theModule = (QTSSModule*)theIter.GetCurrent()->GetEnclosingObject();
			if (theModule->RunsInRole(x))
				sNumModulesInRole[x] += 1;
		}

		if (sNumModulesInRole[x] > 0)
		{
			uint32_t moduleIndex = 0;
			sModuleArray[x] = new QTSSModule*[sNumModulesInRole[x] + 1];
			for (theIter.Reset(); !theIter.IsDone(); theIter.Next())
			{
				theModule = (QTSSModule*)theIter.GetCurrent()->GetEnclosingObject();
				if (theModule->RunsInRole(x))
				{
					sModuleArray[x][moduleIndex] = theModule;
					moduleIndex++;
				}
			}
		}
	}
}

void QTSServer::DestroyModuleRoleArrays()
{
	for (uint32_t x = 0; x < QTSSModule::kNumRoles; x++)
	{
		sNumModulesInRole[x] = 0;
		if (sModuleArray[x] != nullptr)
			delete[] sModuleArray[x];
		sModuleArray[x] = nullptr;
	}
}

void QTSServer::DoInitRole()
{
	QTSS_RoleParams theInitParams;
	theInitParams.initParams.inServer = this;
	theInitParams.initParams.inPrefs = fSrvrPrefs;
	theInitParams.initParams.inMessages = fSrvrMessages;
	theInitParams.initParams.inErrorLogStream = &sErrorLogStream;

	QTSS_ModuleState theModuleState;
	theModuleState.curRole = QTSS_Initialize_Role;
	theModuleState.curTask = nullptr;
	OSThread::SetMainThreadData(&theModuleState);

	//
	// Add the OPTIONS method as the one method the server handles by default (it handles
	// it internally). Modules that handle other RTSP methods will add 
	QTSS_RTSPMethod theOptionsMethod = qtssOptionsMethod;
	(void)this->SetValue(qtssSvrHandledMethods, 0, &theOptionsMethod, sizeof(theOptionsMethod));


	// For now just disable the SetParameter to be compatible with Real.  It should really be removed only for clients that have problems with their SetParameter implementations like (Real Players).
	// At the moment it isn't necesary to add the option.
	//   QTSS_RTSPMethod	theSetParameterMethod = qtssSetParameterMethod;
	//    (void)this->SetValue(qtssSvrHandledMethods, 0, &theSetParameterMethod, sizeof(theSetParameterMethod));

	for (uint32_t x = 0; x < QTSServerInterface::GetNumModulesInRole(QTSSModule::kInitializeRole); x++)
	{
		QTSSModule* theModule = QTSServerInterface::GetModule(QTSSModule::kInitializeRole, x);
		theInitParams.initParams.inModule = theModule;
		theModuleState.curModule = theModule;
		QTSS_Error theErr = theModule->CallDispatch(QTSS_Initialize_Role, &theInitParams);

		if (theErr != QTSS_NoErr)
		{
			// If the module reports an error when initializing itself,
			// delete the module and pretend it was never there.
			QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgInitFailed, theErr,
				theModule->GetValue(qtssModName)->Ptr);

			sModuleQueue.Remove(theModule->GetQueueElem());
			delete theModule;
		}
	}
	this->SetupPublicHeader();

	OSThread::SetMainThreadData(nullptr);
}

void QTSServer::SetupPublicHeader()
{
	//
	// After the Init role, all the modules have reported the methods that they handle.
	// So, we can prune this attribute for duplicates, and construct a string to use in the
	// Public: header of the OPTIONS response
	QTSS_RTSPMethod* theMethod = nullptr;
	uint32_t theLen = 0;

	bool theUniqueMethods[qtssNumMethods + 1];
	::memset(theUniqueMethods, 0, sizeof(theUniqueMethods));

	for (uint32_t y = 0; this->GetValuePtr(qtssSvrHandledMethods, y, (void**)&theMethod, &theLen) == QTSS_NoErr; y++)
		theUniqueMethods[*theMethod] = true;

	// Rewrite the qtssSvrHandledMethods, eliminating any duplicates that modules may have introduced
	uint32_t uniqueMethodCount = 0;
	for (QTSS_RTSPMethod z = 0; z < qtssNumMethods; z++)
	{
		if (theUniqueMethods[z])
			this->SetValue(qtssSvrHandledMethods, uniqueMethodCount++, &z, sizeof(QTSS_RTSPMethod));
	}
	this->SetNumValues(qtssSvrHandledMethods, uniqueMethodCount);

	// Format a text string for the Public: header
	ResizeableStringFormatter theFormatter(nullptr, 0);

	for (uint32_t a = 0; this->GetValuePtr(qtssSvrHandledMethods, a, (void**)&theMethod, &theLen) == QTSS_NoErr; a++)
	{
		sPublicHeaderFormatter.Put(RTSPProtocol::GetMethodString(*theMethod));
		sPublicHeaderFormatter.Put(", ");
	}
	sPublicHeaderStr.Ptr = sPublicHeaderFormatter.GetBufPtr();
	sPublicHeaderStr.Len = sPublicHeaderFormatter.GetBytesWritten() - 2; //trunc the last ", "
}


Task*   RTSPListenerSocket::GetSessionTask(TCPSocket** outSocket)
{
	Assert(outSocket != nullptr);

	// when the server is behing a round robin DNS, the client needs to knwo the IP address ot the server
	// so that it can direct the "POST" half of the connection to the same machine when tunnelling RTSP thru HTTP
	bool  doReportHTTPConnectionAddress = QTSServerInterface::GetServer()->GetPrefs()->GetDoReportHTTPConnectionAddress();

	auto* theTask = new RTSPSession(doReportHTTPConnectionAddress);
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

Task*   HTTPListenerSocket::GetSessionTask(TCPSocket** outSocket)
{
	Assert(outSocket != nullptr);

	auto* theTask = new HTTPSession();
	*outSocket = theTask->GetSocket();  // out socket is not attached to a unix socket yet.

	if (this->OverMaxConnections(0))
		this->SlowDown();
	else
		this->RunNormal();

	return theTask;
}


bool HTTPListenerSocket::OverMaxConnections(uint32_t buffer)
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
	QTSSModule** theModule = nullptr;
	uint32_t theLen = 0;

	for (int y = 0; QTSServerInterface::GetServer()->GetValuePtr(qtssSvrModuleObjects, y, (void**)&theModule, &theLen) == QTSS_NoErr; y++)
	{
		Assert(theModule != nullptr);
		Assert(theLen == sizeof(QTSSModule*));

		(*theModule)->GetPrefsDict()->RereadPreferences();

#if DEBUG
		theModule = nullptr;
		theLen = 0;
#endif
	}

	//
	// Go through each module's prefs object and have those reread as well

	//
	// Now that we are done rereading the prefs, invoke all modules in the RereadPrefs
	// role so they can update their internal prefs caches.
	for (uint32_t x = 0; x < QTSServerInterface::GetNumModulesInRole(QTSSModule::kRereadPrefsRole); x++)
	{
		QTSSModule* theModule = QTSServerInterface::GetModule(QTSSModule::kRereadPrefsRole, x);
		(void)theModule->CallDispatch(QTSS_RereadPrefs_Role, nullptr);
	}
	return QTSS_NoErr;
}


