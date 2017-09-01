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

#include "QTSServerInterface.h"
#include "QTSServer.h"
#include <stdlib.h>

void select_startevents();

QTSServer* sServer = nullptr;
bool sHasPID = false;
uint64_t sLastStatusPackets = 0;
uint64_t sLastDebugPackets = 0;
int64_t sLastDebugTotalQuality = 0;
#ifdef __sgi__ 
#include <sched.h>
#endif

QTSS_ServerState StartServer(uint16_t inPortOverride, QTSS_ServerState inInitialState, bool inDontFork, const char* sAbsolutePath)
{
	//Mark when we are done starting up. If auto-restart is enabled, we want to make sure
	//to always exit with a status of 0 if we encountered a problem WHILE STARTING UP. This
	//will prevent infinite-auto-restart-loop type problems
	bool doneStartingUp = false;
	QTSS_ServerState theServerState = qtssStartingUpState;

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
	sServer = new QTSServer();

	bool createListeners = true;
	if (qtssShuttingDownState == inInitialState)
		createListeners = false;

	sServer->Initialize(inPortOverride, createListeners, sAbsolutePath);

	if (inInitialState == qtssShuttingDownState)
	{
		sServer->InitModules(inInitialState);
		return inInitialState;
	}

	if (sServer->GetServerState() != qtssFatalErrorState)
	{
		uint32_t numShortTaskThreads = 0;
		uint32_t numBlockingThreads = 0;
		uint32_t numThreads = 0;
		uint32_t numProcessors = 0;

		if (OS::ThreadSafe())
		{
			numProcessors = OS::GetNumProcessors();
			// 1 worker thread per processor, up to 2 threads.
			// Note: Limiting the number of worker threads to 2 on a MacOS X system with > 2 cores
			//     results in better performance on those systems, as of MacOS X 10.5.  Future
			//     improvements should make this limit unnecessary.
			if (numProcessors > 2)
				numShortTaskThreads = 2;
			else
				numShortTaskThreads = numProcessors;

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

void RunServer()
{
	bool restartServer = false;
	uint32_t loopCount = 0;
	uint32_t debugLevel = 0;

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

		if ((sServer->SigIntSet()) || (sServer->SigTermSet()))
		{
			//
			// start the shutdown process
			getSingleton()->SetServerState(qtssShuttingDownState);

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

	//Now, make sure that the server can't do any work
	TaskThreadPool::RemoveThreads();

	//now that the server is definitely stopped, it is safe to initate
	//the shutdown process
	delete sServer;

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
