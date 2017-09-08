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
	 File:       OSThread.h

	 Contains:   A thread abstraction



 */

 // OSThread.h
#ifndef __OSTHREAD__
#define __OSTHREAD__

#include "OSHeaders.h"

class OSThread
{

public:
	//
	// Call before calling any other OSThread function
	static void     Initialize();

	OSThread();
	virtual                     ~OSThread();

	//
	// Derived classes must implement their own entry function
	virtual     void            Entry() = 0;
	void            Start();

	static void     ThreadYield();
	static void     Sleep(uint32_t inMsec);

	void            Join();
	void            SendStopRequest() { fStopRequested = true; }
	bool          IsStopRequested() { return fStopRequested; }
	void            StopAndWaitForThread();

	void*           GetThreadData() { return fThreadData; }
	void            SetThreadData(void* inThreadData) { fThreadData = inThreadData; }

	static void*    GetMainThreadData() { return sMainThreadData; }
	static void     SetMainThreadData(void* inData) { sMainThreadData = inData; }
	bool          SwitchPersonality();
#if DEBUG
	uint32_t          GetNumLocksHeld() { return 0; }
	void            IncrementLocksHeld() {}
	void            DecrementLocksHeld() {}
#endif

#if __linux__ ||  __MacOSX__
	static void     WrapSleep(bool wrapSleep) { sWrapSleep = wrapSleep; }
#endif

#ifdef __Win32__
	static int          GetErrno();
	static DWORD        GetCurrentThreadID() { return ::GetCurrentThreadId(); }
#elif __PTHREADS__
	static  int         GetErrno() { return errno; }
	static  pthread_t   GetCurrentThreadID() { return ::pthread_self(); }
#else
	static  int         GetErrno() { return cthread_errno(); }
	static  cthread_t   GetCurrentThreadID() { return cthread_self(); }
#endif

	static  OSThread*   GetCurrent();

private:

#ifdef __Win32__
	static DWORD    sThreadStorageIndex;
#elif __PTHREADS__
	static pthread_key_t    gMainKey;
#ifdef _POSIX_THREAD_PRIORITY_SCHEDULING
	static pthread_attr_t sThreadAttr;
#endif
#endif

	static char sUser[128];
	static char sGroup[128];


	bool fStopRequested{false};
	bool fJoined{false};

#ifdef __Win32__
	HANDLE          fThreadID;
#elif __PTHREADS__
	pthread_t       fThreadID;
#else
	uint32_t          fThreadID;
#endif
	void*           fThreadData{nullptr};

	static void*    sMainThreadData;
#ifdef __Win32__
	static unsigned int WINAPI _Entry(LPVOID inThread);
#else
	static void*    _Entry(void* inThread);
#endif

#if __linux__ || __MacOSX__
	static bool sWrapSleep;
#endif


};

class OSThreadDataSetter
{
public:

	OSThreadDataSetter(void* inInitialValue, void* inFinalValue) : fFinalValue(inFinalValue)
	{
		OSThread::GetCurrent()->SetThreadData(inInitialValue);
	}

	~OSThreadDataSetter() { OSThread::GetCurrent()->SetThreadData(fFinalValue); }

private:

	void*   fFinalValue;
};


#endif

