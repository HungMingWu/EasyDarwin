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
	 File:       OSQueue.h

	 Contains:   implements OSQueue class


 */

#ifndef _OSQUEUE_H_
#define _OSQUEUE_H_

#include "MyAssert.h"
#include "OSHeaders.h"
#include "OSMutex.h"
#include "OSCond.h"
#include "OSThread.h"

#define OSQUEUETESTING 0

class OSQueue;

class OSQueueElem {
public:
	OSQueueElem(void* enclosingObject = nullptr) :
		fEnclosingObject(enclosingObject) {}
	virtual ~OSQueueElem() { Assert(fQueue == nullptr); }

	bool IsMember(const OSQueue& queue) { return (&queue == fQueue); }
	bool IsMemberOfAnyQueue() { return fQueue != nullptr; }
	void* GetEnclosingObject() { return fEnclosingObject; }
	void SetEnclosingObject(void* obj) { fEnclosingObject = obj; }

	OSQueueElem* Next() { return fNext; }
	OSQueueElem* Prev() { return fPrev; }
	OSQueue* InQueue() { return fQueue; }
	inline void Remove();

private:

	OSQueueElem*    fNext{nullptr};
	OSQueueElem*    fPrev{nullptr};
	OSQueue *       fQueue{nullptr};
	void*           fEnclosingObject;

	friend class    OSQueue;
};

class OSQueue {
public:
	OSQueue();
	~OSQueue() = default;

	void            EnQueue(OSQueueElem* object);
	OSQueueElem*    DeQueue();

	OSQueueElem*    GetHead() { if (fLength > 0) return fSentinel.fPrev; return nullptr; }
	OSQueueElem*    GetTail() { if (fLength > 0) return fSentinel.fNext; return nullptr; }
	uint32_t          GetLength() { return fLength; }

	void            Remove(OSQueueElem* object);

#if OSQUEUETESTING
	static bool       Test();
#endif

protected:
	OSMutex 			fMutex;

	OSQueueElem     fSentinel;
	uint32_t          fLength{0};
};

class OSQueueIter
{
public:
	OSQueueIter(OSQueue* inQueue) : fQueueP(inQueue), fCurrentElemP(inQueue->GetHead()) {}
	OSQueueIter(OSQueue* inQueue, OSQueueElem* startElemP) : fQueueP(inQueue)
	{
		if (startElemP)
		{
			Assert(startElemP->IsMember(*inQueue));
			fCurrentElemP = startElemP;

		}
		else
			fCurrentElemP = nullptr;
	}
	~OSQueueIter() = default;

	void            Reset() { fCurrentElemP = fQueueP->GetHead(); }

	OSQueueElem*    GetCurrent() { return fCurrentElemP; }
	void            Next();

	bool          IsDone() { return fCurrentElemP == nullptr; }

private:

	OSQueue*        fQueueP;
	OSQueueElem*    fCurrentElemP;
};

class OSQueue_Blocking
{
public:
	OSQueue_Blocking() = default;
	~OSQueue_Blocking() = default;

	OSQueueElem*    DeQueueBlocking(OSThread* inCurThread, int32_t inTimeoutInMilSecs);
	OSQueueElem*    DeQueue();//will not block
	void            EnQueue(OSQueueElem* obj);

	OSCond*         GetCond() { return &fCond; }
	OSQueue*        GetQueue() { return &fQueue; }

private:

	OSCond              fCond;
	OSMutex             fMutex;
	OSQueue             fQueue;
};


void OSQueueElem::Remove()
{
	if (fQueue != nullptr)
		fQueue->Remove(this);
}
#endif //_OSQUEUE_H_
