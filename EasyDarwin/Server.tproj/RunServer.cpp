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
	 File:       main.cpp

	 Contains:   main function to drive streaming server.
 */

#include <memory>
#include "RunServer.h"
#include "OS.h"
#include "OSThread.h"
#include "Socket.h"
#include "SocketUtils.h"
#include "Task.h"
#include "IdleTask.h"
#include "TimeoutTask.h"
#include "QTSSRollingLog.h"

#ifndef __Win32__
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "QTSServerInterface.h"
#include "QTSServer.h"

#include <stdlib.h>
#include <QTSSModuleUtils.h>

void select_startevents();

QTSServer* sServer = nullptr;
int sStatusUpdateInterval = 0;
bool sHasPID = false;
uint64_t sLastStatusPackets = 0;
uint64_t sLastDebugPackets = 0;
int64_t sLastDebugTotalQuality = 0;
#ifdef __sgi__ 
#include <sched.h>
#endif

QTSS_ServerState StartServer(XMLPrefsParser* inPrefsSource, PrefsSource* inMessagesSource, uint16_t inPortOverride, int statsUpdateInterval, QTSS_ServerState inInitialState, bool inDontFork, uint32_t debugLevel, uint32_t debugOptions, const char* sAbsolutePath)
{
	//Mark when we are done starting up. If auto-restart is enabled, we want to make sure
	//to always exit with a status of 0 if we encountered a problem WHILE STARTING UP. This
	//will prevent infinite-auto-restart-loop type problems
	bool doneStartingUp = false;
	QTSS_ServerState theServerState = qtssStartingUpState;

	sStatusUpdateInterval = statsUpdateInterval;

	//Initialize utility classes
	OS::Initialize();
	OSThread::Initialize();

	Socket::Initialize();
	SocketUtils::Initialize(!inDontFork);

#if !MACOSXEVENTQUEUE

#ifndef __Win32__    
	::epollInit();
#else
	::select_startevents();//initialize the select() implementation of the event queue        
#endif

#endif

	//start the server
	QTSSDictionaryMap::Initialize();
	QTSServerInterface::Initialize();// this must be called before constructing the server object
	sServer = new QTSServer();
	sServer->SetDebugLevel(debugLevel);
	sServer->SetDebugOptions(debugOptions);

	// re-parse config file
	inPrefsSource->Parse();

	bool createListeners = true;
	if (qtssShuttingDownState == inInitialState)
		createListeners = false;

	sServer->Initialize(inPrefsSource, inMessagesSource, inPortOverride, createListeners, sAbsolutePath);

	if (inInitialState == qtssShuttingDownState)
	{
		sServer->InitModules(inInitialState);
		return inInitialState;
	}

	std::unique_ptr<char[]> runGroupName(sServer->GetPrefs()->GetRunGroupName());
	std::unique_ptr<char[]> runUserName(sServer->GetPrefs()->GetRunUserName());
	OSThread::SetPersonality(runUserName.get(), runGroupName.get());

	if (sServer->GetServerState() != qtssFatalErrorState)
	{
		uint32_t numShortTaskThreads = 0;
		uint32_t numBlockingThreads = 0;
		uint32_t numThreads = 0;
		uint32_t numProcessors = 0;

		if (OS::ThreadSafe())
		{
			numShortTaskThreads = sServer->GetPrefs()->GetNumThreads(); // whatever the prefs say
			if (numShortTaskThreads == 0) {
				numProcessors = OS::GetNumProcessors();
				// 1 worker thread per processor, up to 2 threads.
				// Note: Limiting the number of worker threads to 2 on a MacOS X system with > 2 cores
				//     results in better performance on those systems, as of MacOS X 10.5.  Future
				//     improvements should make this limit unnecessary.
				if (numProcessors > 2)
					numShortTaskThreads = 2;
				else
					numShortTaskThreads = numProcessors;
			}

			numBlockingThreads = sServer->GetPrefs()->GetNumBlockingThreads(); // whatever the prefs say
			if (numBlockingThreads == 0)
				numBlockingThreads = 1;

		}
		if (numShortTaskThreads == 0)
			numShortTaskThreads = 1;

		if (numBlockingThreads == 0)
			numBlockingThreads = 1;

		numThreads = numShortTaskThreads + numBlockingThreads;
		//printf("Add threads shortask=%lu blocking=%lu\n",numShortTaskThreads, numBlockingThreads);
		TaskThreadPool::SetNumShortTaskThreads(numShortTaskThreads);
		TaskThreadPool::SetNumBlockingTaskThreads(numBlockingThreads);
		TaskThreadPool::AddThreads(numThreads);
		sServer->InitNumThreads(numThreads);

#if DEBUG
		printf("Number of task threads: %"   _U32BITARG_   "\n", numThreads);
#endif

		// Start up the server's global tasks, and start listening
		TimeoutTask::Initialize();     // The TimeoutTask mechanism is task based,
									// we therefore must do this after adding task threads
									// this be done before starting the sockets and server tasks
	}

	//Make sure to do this stuff last. Because these are all the threads that
	//do work in the server, this ensures that no work can go on while the server
	//is in the process of staring up
	if (sServer->GetServerState() != qtssFatalErrorState)
	{
		IdleTask::Initialize();
		Socket::StartThread();
		OSThread::Sleep(1000);

		//
		// On Win32, in order to call modwatch the Socket EventQueue thread must be
		// created first. Modules call modwatch from their initializer, and we don't
		// want to prevent them from doing that, so module initialization is separated
		// out from other initialization, and we start the Socket EventQueue thread first.
		// The server is still prevented from doing anything as of yet, because there
		// aren't any TaskThreads yet.
		sServer->InitModules(inInitialState);
		sServer->StartTasks();
		sServer->SetupUDPSockets(); // udp sockets are set up after the rtcp task is instantiated
		theServerState = sServer->GetServerState();
	}

	if (theServerState != qtssFatalErrorState)
	{
		CleanPid(true);
		WritePid(!inDontFork);

		doneStartingUp = true;
		printf("Streaming Server done starting up\n");
		//OSMemory::SetMemoryError(ENOMEM);
	}


	// SWITCH TO RUN USER AND GROUP ID
	//if (!sServer->SwitchPersonality())
	//    theServerState = qtssFatalErrorState;

   //
	// Tell the caller whether the server started up or not
	return theServerState;
}

void WritePid(bool forked)
{
#ifndef __Win32__
	// WRITE PID TO FILE
	std::unique_ptr<char[]> thePidFileName(sServer->GetPrefs()->GetPidFilePath());
	FILE *thePidFile = fopen(thePidFileName.get(), "w");
	if (thePidFile)
	{
		if (!forked)
			fprintf(thePidFile, "%d\n", getpid());    // write own pid
		else
		{
			fprintf(thePidFile, "%d\n", getppid());    // write parent pid
			fprintf(thePidFile, "%d\n", getpid());    // and our own pid in the next line
		}
		fclose(thePidFile);
		sHasPID = true;
	}
#endif
}

void CleanPid(bool force)
{
#ifndef __Win32__
	if (sHasPID || force)
	{
		std::unique_ptr<char[]> thePidFileName(sServer->GetPrefs()->GetPidFilePath());
		unlink(thePidFileName.get());
	}
#endif
}

void print_status(FILE* file, FILE* console, char* format, char* theStr)
{
	if (file) fprintf(file, format, theStr);
	if (console) fprintf(console, format, theStr);

}

void DebugLevel_1(FILE*   statusFile, FILE*   stdOut, bool printHeader)
{
	char*  thePrefStr = nullptr;
	static char numStr[12] = "";
	static char dateStr[25] = "";
	uint32_t theLen = 0;

	if (printHeader)
	{

		print_status(statusFile, stdOut, "%s", "     RTP-Conns RTSP-Conns HTTP-Conns  kBits/Sec   Pkts/Sec   RTP-Playing   AvgDelay CurMaxDelay  MaxDelay  AvgQuality  NumThinned      Time\n");

	}

	(void)((QTSSDictionary*)sServer)->GetValueAsString(qtssRTPSvrCurConn, 0, &thePrefStr);
	print_status(statusFile, stdOut, "%11s", thePrefStr);

	delete[] thePrefStr; thePrefStr = nullptr;

	(void)((QTSSDictionary*)sServer)->GetValueAsString(qtssRTSPCurrentSessionCount, 0, &thePrefStr);
	print_status(statusFile, stdOut, "%11s", thePrefStr);
	delete[] thePrefStr; thePrefStr = nullptr;

	(void)((QTSSDictionary*)sServer)->GetValueAsString(qtssRTSPHTTPCurrentSessionCount, 0, &thePrefStr);
	print_status(statusFile, stdOut, "%11s", thePrefStr);
	delete[] thePrefStr; thePrefStr = nullptr;

	uint32_t curBandwidth = 0;
	theLen = sizeof(curBandwidth);
	(void)((QTSSDictionary*)sServer)->GetValue(qtssRTPSvrCurBandwidth, 0, &curBandwidth, &theLen);
	snprintf(numStr, 11, "%"   _U32BITARG_   "", curBandwidth / 1024);
	print_status(statusFile, stdOut, "%11s", numStr);

	(void)((QTSSDictionary*)sServer)->GetValueAsString(qtssRTPSvrCurPackets, 0, &thePrefStr);
	print_status(statusFile, stdOut, "%11s", thePrefStr);
	delete[] thePrefStr; thePrefStr = nullptr;


	uint32_t currentPlaying = sServer->GetNumRTPPlayingSessions();
	snprintf(numStr, sizeof(numStr) - 1, "%"   _U32BITARG_   "", currentPlaying);
	print_status(statusFile, stdOut, "%14s", numStr);


	//is the server keeping up with the streams?
	//what quality are the streams?
	int64_t totalRTPPaackets = sServer->GetTotalRTPPackets();
	int64_t deltaPackets = totalRTPPaackets - sLastDebugPackets;
	sLastDebugPackets = totalRTPPaackets;

	int64_t totalQuality = sServer->GetTotalQuality();
	int64_t deltaQuality = totalQuality - sLastDebugTotalQuality;
	sLastDebugTotalQuality = totalQuality;

	int64_t currentMaxLate = sServer->GetCurrentMaxLate();
	int64_t totalLate = sServer->GetTotalLate();

	sServer->ClearTotalLate();
	sServer->ClearCurrentMaxLate();
	sServer->ClearTotalQuality();

	::snprintf(numStr, sizeof(numStr) - 1, "%s", "0");
	if (deltaPackets > 0)
		snprintf(numStr, sizeof(numStr) - 1, "%" _S32BITARG_ "", (int32_t)((int64_t)totalLate / (int64_t)deltaPackets));
	print_status(statusFile, stdOut, "%11s", numStr);

	snprintf(numStr, sizeof(numStr) - 1, "%" _S32BITARG_ "", (int32_t)currentMaxLate);
	print_status(statusFile, stdOut, "%11s", numStr);

	snprintf(numStr, sizeof(numStr) - 1, "%" _S32BITARG_ "", (int32_t)sServer->GetMaxLate());
	print_status(statusFile, stdOut, "%11s", numStr);

	::snprintf(numStr, sizeof(numStr) - 1, "%s", "0");
	if (deltaPackets > 0)
		snprintf(numStr, sizeof(numStr) - 1, "%" _S32BITARG_ "", (int32_t)((int64_t)deltaQuality / (int64_t)deltaPackets));
	print_status(statusFile, stdOut, "%11s", numStr);

	snprintf(numStr, sizeof(numStr) - 1, "%" _S32BITARG_ "", (int32_t)sServer->GetNumThinned());
	print_status(statusFile, stdOut, "%11s", numStr);



	char theDateBuffer[QTSSRollingLog::kMaxDateBufferSizeInBytes];
	(void)QTSSRollingLog::FormatDate(theDateBuffer, false);

	snprintf(dateStr, sizeof(dateStr) - 1, "%s", theDateBuffer);
	print_status(statusFile, stdOut, "%24s\n", dateStr);
}

FILE* LogDebugEnabled()
{

	if (DebugLogOn(sServer))
	{
		static StrPtrLen statsFileNameStr("server_debug_status");

		StrPtrLenDel pathStr(sServer->GetPrefs()->GetErrorLogDir());
		ResizeableStringFormatter pathBuffer(nullptr, 0);
		pathBuffer.PutFilePath(&pathStr, &statsFileNameStr);
		pathBuffer.PutTerminator();

		char*   filePath = pathBuffer.GetBufPtr();
		return ::fopen(filePath, "a");
	}

	return nullptr;
}


FILE* DisplayDebugEnabled()
{
	return (DebugDisplayOn(sServer)) ? stdout : nullptr;
}


void DebugStatus(uint32_t debugLevel, bool printHeader)
{

	FILE*   statusFile = LogDebugEnabled();
	FILE*   stdOut = DisplayDebugEnabled();

	if (debugLevel > 0)
		DebugLevel_1(statusFile, stdOut, printHeader);

	if (statusFile)
		::fclose(statusFile);
}

void FormattedTotalBytesBuffer(char *outBuffer, int outBufferLen, uint64_t totalBytes)
{
	float displayBytes = 0.0;
	char  sizeStr[] = "B";
	char* format = nullptr;

	if (totalBytes > 1073741824) //GBytes
	{
		displayBytes = (float)((double)(int64_t)totalBytes / (double)(int64_t)1073741824);
		sizeStr[0] = 'G';
		format = "%.4f%s ";
	}
	else if (totalBytes > 1048576) //MBytes
	{
		displayBytes = (float)(int32_t)totalBytes / (float)(int32_t)1048576;
		sizeStr[0] = 'M';
		format = "%.3f%s ";
	}
	else if (totalBytes > 1024) //KBytes
	{
		displayBytes = (float)(int32_t)totalBytes / (float)(int32_t)1024;
		sizeStr[0] = 'K';
		format = "%.2f%s ";
	}
	else
	{
		displayBytes = (float)(int32_t)totalBytes;  //Bytes
		sizeStr[0] = 'B';
		format = "%4.0f%s ";
	}

	outBuffer[outBufferLen - 1] = 0;
	snprintf(outBuffer, outBufferLen - 1, format, displayBytes, sizeStr);
}

void PrintStatus(bool printHeader)
{
	char* thePrefStr = nullptr;
	uint32_t theLen = 0;

	if (printHeader)
	{
		printf("     RTP-Conns RTSP-Conns HTTP-Conns  kBits/Sec   Pkts/Sec    TotConn     TotBytes   TotPktsLost          Time\n");
	}

	(void)((QTSSDictionary*)sServer)->GetValueAsString(qtssRTPSvrCurConn, 0, &thePrefStr);
	printf("%11s", thePrefStr);
	delete[] thePrefStr; thePrefStr = nullptr;

	(void)((QTSSDictionary*)sServer)->GetValueAsString(qtssRTSPCurrentSessionCount, 0, &thePrefStr);
	printf("%11s", thePrefStr);
	delete[] thePrefStr; thePrefStr = nullptr;

	(void)((QTSSDictionary*)sServer)->GetValueAsString(qtssRTSPHTTPCurrentSessionCount, 0, &thePrefStr);
	printf("%11s", thePrefStr);
	delete[] thePrefStr; thePrefStr = nullptr;

	uint32_t curBandwidth = 0;
	theLen = sizeof(curBandwidth);
	((QTSSDictionary*)sServer)->GetValue(qtssRTPSvrCurBandwidth, 0, &curBandwidth, &theLen);
	printf("%11"   _U32BITARG_, curBandwidth / 1024);

	(void)((QTSSDictionary*)sServer)->GetValueAsString(qtssRTPSvrCurPackets, 0, &thePrefStr);
	printf("%11s", thePrefStr);
	delete[] thePrefStr; thePrefStr = nullptr;

	(void)((QTSSDictionary*)sServer)->GetValueAsString(qtssRTPSvrTotalConn, 0, &thePrefStr);
	printf("%11s", thePrefStr);
	delete[] thePrefStr; thePrefStr = nullptr;

	uint64_t totalBytes = sServer->GetTotalRTPBytes();
	char  displayBuff[32] = "";
	FormattedTotalBytesBuffer(displayBuff, sizeof(displayBuff), totalBytes);
	printf("%17s", displayBuff);

	printf("%11" _64BITARG_ "u", sServer->GetTotalRTPPacketsLost());

	char theDateBuffer[QTSSRollingLog::kMaxDateBufferSizeInBytes];
	(void)QTSSRollingLog::FormatDate(theDateBuffer, false);
	printf("%25s", theDateBuffer);

	printf("\n");

}

bool PrintHeader(uint32_t loopCount)
{
	return ((loopCount % (sStatusUpdateInterval * 10)) == 0) ? true : false;
}

bool PrintLine(uint32_t loopCount)
{
	return ((loopCount % sStatusUpdateInterval) == 0) ? true : false;
}


void RunServer()
{
	bool restartServer = false;
	uint32_t loopCount = 0;
	uint32_t debugLevel = 0;
	bool printHeader = false;
	bool printStatus = false;

	//just wait until someone stops the server or a fatal error occurs.
	QTSS_ServerState theServerState = sServer->GetServerState();
	while ((theServerState != qtssShuttingDownState) &&
		(theServerState != qtssFatalErrorState))
	{
#ifdef __sgi__
		OSThread::Sleep(999);
#else
		OSThread::Sleep(1000);
#endif

		if (sStatusUpdateInterval)
		{
			debugLevel = sServer->GetDebugLevel();
			printHeader = PrintHeader(loopCount);
			printStatus = PrintLine(loopCount);

			if (printStatus)
			{
				if (DebugOn(sServer)) // debug level display or logging is on
					DebugStatus(debugLevel, printHeader);

				if (!DebugDisplayOn(sServer))
					PrintStatus(printHeader); // default status output
			}


			loopCount++;

		}

		if ((sServer->SigIntSet()) || (sServer->SigTermSet()))
		{
			//
			// start the shutdown process
			theServerState = qtssShuttingDownState;
			QTSServerInterface::GetServer()->SetValue(qtssSvrState, 0, &theServerState, sizeof(theServerState));

			if (sServer->SigIntSet())
				restartServer = true;
		}

		theServerState = sServer->GetServerState();
		if (theServerState == qtssIdleState)
			sServer->KillAllRTPSessions();
	}

	//
	// Kill all the sessions and wait for them to die,
	// but don't wait more than 5 seconds
	sServer->KillAllRTPSessions();
	for (uint32_t shutdownWaitCount = 0; (sServer->GetNumRTPSessions() > 0) && (shutdownWaitCount < 5); shutdownWaitCount++)
		OSThread::Sleep(1000);

	//Now, make sure that the server can't do any work
	TaskThreadPool::RemoveThreads();

	//now that the server is definitely stopped, it is safe to initate
	//the shutdown process
	delete sServer;

	CleanPid(false);
	//ok, we're ready to exit. If we're quitting because of some fatal error
	//while running the server, make sure to let the parent process know by
	//exiting with a nonzero status. Otherwise, exit with a 0 status
	if (theServerState == qtssFatalErrorState || restartServer)
	{
#ifdef WIN32
		::ExitProcess(-2);
#else
		::exit(-2);
#endif //WIN32		
	}
}
