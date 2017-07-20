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
 //
 // QTFile_FileControlBlock:
 //   All the per-client stuff for QTFile.


#ifndef _QTFILE_FILECONTROLBLOCK_H_
#define _QTFILE_FILECONTROLBLOCK_H_

//
// Includes
#include "OSHeaders.h"

#if DSS_USE_API_CALLBACKS
#include "QTSS.h"
#endif

#if DSS_USE_API_CALLBACKS
#define FILE_SOURCE QTSS_Object
#else
#define FILE_SOURCE OSFileSource
#endif

//
// Class state cookie
class QTFile_FileControlBlock {

public:
	//
	// Constructor and destructor.
	QTFile_FileControlBlock();
	virtual ~QTFile_FileControlBlock();

	//Sets this object to reference this file
	void Set(char *inPath);

	//Advise: this advises the OS that we are going to be reading soon from the
	//following position in the file
	// void Advise(OSFileSource *dflt, UInt64 advisePos, uint32_t adviseAmt);

	bool Read(FILE_SOURCE *dflt, UInt64 inPosition, void* inBuffer, uint32_t inLength);

	bool ReadInternal(FILE_SOURCE *dataFD, UInt64 inPosition, void* inBuffer, uint32_t inLength, uint32_t *inReadLenPtr = NULL);

	//
	// Buffer management functions
	void AdjustDataBufferBitRate(uint32_t inUnitSizeInK = 32, uint32_t inFileBitRate = 32768, uint32_t inNumBuffSizeUnits = 0, uint32_t inMaxBitRateBuffSizeInBlocks = 8);
	void AdjustDataBuffers(uint32_t inBlockSizeKBits = 32, uint32_t inBlockCountPerBuff = 1);
	void EnableCacheBuffers(bool enabled) { fCacheEnabled = enabled; }

	// QTSS_ErrorCode Close();

	bool IsValid()
	{
#if DSS_USE_API_CALLBACKS
		return fDataFD != NULL;
#else       
		return fDataFD.IsValid();
#endif
	}


private:
	//
	// File descriptor for this control block
	FILE_SOURCE fDataFD;

	enum
	{
		kMaxDefaultBlocks = 8,
		kDataBufferUnitSizeExp = 15,   // 32Kbytes
		kBlockByteSize = (1 << kDataBufferUnitSizeExp)
	};
	//
	// Data buffer cache
	char                *fDataBufferPool;

	uint32_t              fDataBufferSize;
	UInt64              fDataBufferPosStart, fDataBufferPosEnd;

	char                *fCurrentDataBuffer, *fPreviousDataBuffer;
	uint32_t              fCurrentDataBufferLength, fPreviousDataBufferLength;

	uint32_t              fNumBlocksPerBuff;
	uint32_t              fNumBuffs;
	bool              fCacheEnabled;
};

#endif //_QTFILE_FILECONTROLBLOCK_H_
