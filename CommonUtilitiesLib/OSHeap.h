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
	 File:       OSHeap.h

	 Contains:   Implements a heap


 */

#ifndef _OSHEAP_H_
#define _OSHEAP_H_

#define _OSHEAP_TESTING_ 0

#include "OSCond.h"

class OSHeapElem;

class OSHeap
{
public:

	enum
	{
		kDefaultStartSize = 1024 //uint32_t
	};

	OSHeap(uint32_t inStartSize = kDefaultStartSize);
	~OSHeap() { if (fHeap != nullptr) delete fHeap; }

	//ACCESSORS
	uint32_t      CurrentHeapSize() { return fFreeIndex - 1; }
	OSHeapElem* PeekMin() { if (CurrentHeapSize() > 0) return fHeap[1]; return nullptr; }

	//MODIFIERS

	//These are the two primary operations supported by the heap
	//abstract data type. both run in log(n) time.
	void            Insert(OSHeapElem*  inElem);
	OSHeapElem*     ExtractMin() { return extract(1); }
	//removes specified element from the heap
	OSHeapElem*     Remove(OSHeapElem* elem);

#if _OSHEAP_TESTING_
	//returns true if it passed the test, false otherwise
	static bool       Test();
#endif

private:

	OSHeapElem*     extract(uint32_t index);

#if _OSHEAP_TESTING_
	//verifies that the heap is in fact a heap
	void            sanityCheck(uint32_t root);
#endif

	OSHeapElem**    fHeap;
	uint32_t          fFreeIndex{1};
	uint32_t          fArraySize;
};

class OSHeapElem
{
public:
	OSHeapElem(void* enclosingObject = nullptr)
		: fEnclosingObject(enclosingObject) {}
	~OSHeapElem() = default;

	//This data structure emphasizes performance over extensibility
	//If it were properly object-oriented, the compare routine would
	//be virtual. However, to avoid the use of v-functions in this data
	//structure, I am assuming that the objects are compared using a 64 bit number.
	//
	void    SetValue(int64_t newValue) { fValue = newValue; }
	int64_t  GetValue() { return fValue; }
	void*   GetEnclosingObject() { return fEnclosingObject; }
	void	SetEnclosingObject(void* obj) { fEnclosingObject = obj; }
	bool  IsMemberOfAnyHeap() { return fCurrentHeap != nullptr; }

private:

	int64_t  fValue{0};
	void* fEnclosingObject;
	OSHeap* fCurrentHeap{nullptr};

	friend class OSHeap;
};
#endif //_OSHEAP_H_
