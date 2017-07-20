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
	 File:       osfilesource.h

	 Contains:   simple file abstraction. This file abstraction is ONLY to be
				 used for files intended for serving


 */

#ifndef __OSFILE_H_
#define __OSFILE_H_

#include <stdio.h>

#include "OSHeaders.h"
#include "StrPtrLen.h"
#include "OSQueue.h"

#define READ_LOG 0

class FileBlockBuffer
{

public:
	FileBlockBuffer() : fArrayIndex(-1), fBufferSize(0), fBufferFillSize(0), fDataBuffer(nullptr), fDummy(0) {}
	~FileBlockBuffer();
	void AllocateBuffer(uint32_t buffSize);
	void TestBuffer();
	void CleanBuffer() const
	{ ::memset(fDataBuffer, 0, fBufferSize); }
	void SetFillSize(uint32_t fillSize) { fBufferFillSize = fillSize; }
	uint32_t GetFillSize() const
	{ return fBufferFillSize; }
	OSQueueElem* GetQElem() { return &fQElem; }
	SInt64              fArrayIndex;
	uint32_t              fBufferSize;
	uint32_t              fBufferFillSize;
	char*				fDataBuffer;
	OSQueueElem         fQElem;
	uint32_t              fDummy;
};

class FileBlockPool
{
	enum
	{
		kDataBufferUnitSizeExp = 15,// base 2 exponent
		kBufferUnitSize = (1 << kDataBufferUnitSizeExp) // 32Kbytes
	};

public:
	FileBlockPool() : fMaxBuffers(1), fNumCurrentBuffers(0), fBufferInc(0), fBufferUnitSizeBytes(kBufferUnitSize), fBufferDataSizeBytes(0)
	{}
	~FileBlockPool();

	void SetMaxBuffers(uint32_t maxBuffers) { if (maxBuffers > 0) fMaxBuffers = maxBuffers; }

	void SetBuffIncValue(uint32_t bufferInc) { if (bufferInc > 0) fBufferInc = bufferInc; }
	void IncMaxBuffers() { fMaxBuffers += fBufferInc; }
	void DecMaxBuffers() { if (fMaxBuffers > fBufferInc) fMaxBuffers -= fBufferInc; }
	void DecCurBuffers() { if (fNumCurrentBuffers > 0) fNumCurrentBuffers--; }

	void SetBufferUnitSize(uint32_t inUnitSizeInK) { fBufferUnitSizeBytes = inUnitSizeInK * 1024; }
	uint32_t GetBufferUnitSizeBytes() const
	{ return fBufferUnitSizeBytes; }
	uint32_t GetMaxBuffers() const
	{ return fMaxBuffers; }
	uint32_t GetIncBuffers() const
	{ return fBufferInc; }
	uint32_t GetNumCurrentBuffers() const
	{ return fNumCurrentBuffers; }
	void DeleteBlockPool();
	FileBlockBuffer* GetBufferElement(uint32_t bufferSizeBytes);
	void MarkUsed(FileBlockBuffer* inBuffPtr);

private:
	OSQueue fQueue;
	uint32_t  fMaxBuffers;
	uint32_t  fNumCurrentBuffers;
	uint32_t  fBufferInc;
	uint32_t  fBufferUnitSizeBytes;
	uint32_t  fBufferDataSizeBytes;

};

class FileMap
{

public:
	FileMap() :fFileMapArray(nullptr), fDataBufferSize(0), fMapArraySize(0), fNumBuffSizeUnits(0) {}
	~FileMap() { fFileMapArray = nullptr; }
	void    AllocateBufferMap(uint32_t inUnitSizeInK, uint32_t inNumBuffSizeUnits, uint32_t inBufferIncCount, uint32_t inMaxBitRateBuffSizeInBlocks, UInt64 fileLen, uint32_t inBitRate);
	char*   GetBuffer(SInt64 bufIndex, bool* outIsEmptyBuff);
	void    TestBuffer(SInt32 bufIndex) const
	{ Assert(bufIndex >= 0); fFileMapArray[bufIndex]->TestBuffer(); };
	void    SetIndexBuffFillSize(SInt32 bufIndex, uint32_t fillSize) const
	{ Assert(bufIndex >= 0); fFileMapArray[bufIndex]->SetFillSize(fillSize); }
	uint32_t  GetMaxBufSize() const
	{ return fDataBufferSize; }
	uint32_t  GetBuffSize(SInt64 bufIndex) const
	{ Assert(bufIndex >= 0); return fFileMapArray[bufIndex]->GetFillSize(); }
	uint32_t  GetIncBuffers() const
	{ return fBlockPool.GetIncBuffers(); }
	void    IncMaxBuffers() { fBlockPool.IncMaxBuffers(); }
	void    DecMaxBuffers() { fBlockPool.DecMaxBuffers(); }
	bool  Initialized() const
	{ return fFileMapArray == nullptr ? false : true; }
	void    Clean();
	void    DeleteMap();
	void    DeleteOldBuffs();
	SInt64  GetBuffIndex(UInt64 inPosition) const
	{ return inPosition / this->GetMaxBufSize(); }
	SInt64  GetMaxBuffIndex() const
	{ Assert(fMapArraySize > 0); return fMapArraySize - 1; }
	UInt64  GetBuffOffset(SInt64 bufIndex) const
	{ return static_cast<UInt64>(bufIndex * this->GetMaxBufSize()); }
	FileBlockPool fBlockPool;

	FileBlockBuffer**   fFileMapArray;

private:

	uint32_t              fDataBufferSize;
	SInt64              fMapArraySize;
	uint32_t              fNumBuffSizeUnits;

};

class OSFileSource
{
public:

	OSFileSource() : fFile(-1), fLength(0), fPosition(0), fReadPos(0), fShouldClose(true), fIsDir(false), fModDate(0), fCacheEnabled(false)
	{

#if READ_LOG 
		fFileLog = nullptr;
		fTrackID = 0;
		fFilePath[0] = 0;
#endif

	}

	OSFileSource(const char* inPath) : fFile(-1), fLength(0), fPosition(0), fReadPos(0), fShouldClose(true), fIsDir(false), fCacheEnabled(false)
	{
		Set(inPath);

#if READ_LOG 
		fFileLog = nullptr;
		fTrackID = 0;
		fFilePath[0] = 0;
#endif      

	}

	~OSFileSource() { Close();  fFileMap.DeleteMap(); }

	//Sets this object to reference this file
	void            Set(const char* inPath);

	// Call this if you don't want Close or the destructor to close the fd
	void            DontCloseFD() { fShouldClose = false; }

	//Advise: this advises the OS that we are going to be reading soon from the
	//following position in the file
	void            Advise(UInt64 advisePos, uint32_t adviseAmt);

	OS_Error    Read(void* inBuffer, uint32_t inLength, uint32_t* outRcvLen = nullptr)
	{
		return ReadFromDisk(inBuffer, inLength, outRcvLen);
	}

	OS_Error    Read(UInt64 inPosition, void* inBuffer, uint32_t inLength, uint32_t* outRcvLen = nullptr);
	OS_Error    ReadFromDisk(void* inBuffer, uint32_t inLength, uint32_t* outRcvLen = nullptr);
	OS_Error    ReadFromCache(UInt64 inPosition, void* inBuffer, uint32_t inLength, uint32_t* outRcvLen = nullptr);
	OS_Error    ReadFromPos(UInt64 inPosition, void* inBuffer, uint32_t inLength, uint32_t* outRcvLen = nullptr);
	void        EnableFileCache(bool enabled) { OSMutexLocker locker(&fMutex); fCacheEnabled = enabled; }
	bool      GetCacheEnabled() const
	{ return fCacheEnabled; }
	void        AllocateFileCache(uint32_t inUnitSizeInK = 32, uint32_t bufferSizeUnits = 0, uint32_t incBuffers = 1, uint32_t inMaxBitRateBuffSizeInBlocks = 8, uint32_t inBitRate = 32768)
	{
		fFileMap.AllocateBufferMap(inUnitSizeInK, bufferSizeUnits, incBuffers, inMaxBitRateBuffSizeInBlocks, fLength, inBitRate);
	}
	void        IncMaxBuffers() { OSMutexLocker locker(&fMutex); fFileMap.IncMaxBuffers(); }
	void        DecMaxBuffers() { OSMutexLocker locker(&fMutex); fFileMap.DecMaxBuffers(); }

	OS_Error    FillBuffer(char* ioBuffer, char* buffStart, SInt32 bufIndex);

	void            Close();
	time_t          GetModDate() const
	{ return fModDate; }
	UInt64          GetLength() const
	{ return fLength; }
	UInt64          GetCurOffset() const
	{ return fPosition; }
	void            Seek(SInt64 newPosition) { fPosition = newPosition; }
	bool IsValid() const
	{ return fFile != -1; }
	bool IsDir() const
	{ return fIsDir; }

	// For async I/O purposes
	int             GetFD() const
	{ return fFile; }
	void            SetTrackID(uint32_t trackID);
	// So that close won't do anything
	void ResetFD() { fFile = -1; }

	void SetLog(const char* inPath);

private:

	int     fFile;
	UInt64  fLength;
	UInt64  fPosition;
	UInt64  fReadPos;
	bool  fShouldClose;
	bool  fIsDir;
	time_t  fModDate;


	OSMutex fMutex;
	FileMap fFileMap;
	bool  fCacheEnabled;
#if READ_LOG
	FILE*               fFileLog;
	char                fFilePath[1024];
	uint32_t              fTrackID;
#endif

};

#endif //__OSFILE_H_
