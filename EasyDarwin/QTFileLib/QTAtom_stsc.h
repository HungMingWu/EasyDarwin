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
 // $Id: QTAtom_stsc.h,v 1.1 2006/01/05 13:20:36 murata Exp $
 //
 // QTAtom_stsc:
 //   The 'stsc' QTAtom class.

#ifndef QTAtom_stsc_H
#define QTAtom_stsc_H


//
// Includes
#include "QTFile.h"
#include "QTAtom.h"


//
// Class state cookie
class QTAtom_stsc_SampleTableControlBlock {

public:
	//
	// Constructor and destructor.
	QTAtom_stsc_SampleTableControlBlock();
	virtual             ~QTAtom_stsc_SampleTableControlBlock();

	//
	// Reset function
	void        Reset();

	//
	// Sample table cache
	uint32_t              fCurEntry;
	uint32_t              fCurSample;
	uint32_t              fLastFirstChunk, fLastSamplesPerChunk, fLastSampleDescription;


	uint32_t  fLastFirstChunk_GetChunkFirstLastSample;
	uint32_t  fLastSamplesPerChunk_GetChunkFirstLastSample;
	uint32_t  fLastTotalSamples_GetChunkFirstLastSample;

	uint32_t fCurEntry_GetChunkFirstLastSample;
	uint32_t chunkNumber_GetChunkFirstLastSample;
	uint32_t firstSample_GetChunkFirstLastSample;
	uint32_t lastSample_GetChunkFirstLastSample;


	uint32_t fFirstSampleNumber_SampleToChunkInfo;
	uint32_t fFirstSamplesPerChunk_SampleToChunkInfo;
	uint32_t fFirstChunkNumber_SampleToChunkInfo;
	uint32_t fFirstSampleDescriptionIndex_SampleToChunkInfo;
	uint32_t fFirstSampleOffsetInChunk_SampleToChunkInfo;

	uint32_t  fCurEntry_SampleToChunkInfo;
	uint32_t  fCurSample_SampleToChunkInfo;
	uint32_t  fLastFirstChunk_SampleToChunkInfo;
	uint32_t  fLastSamplesPerChunk_SampleToChunkInfo;
	uint32_t  fLastSampleDescription_SampleToChunkInfo;

	uint32_t  fGetSampleInfo_SampleNumber;
	uint32_t  fGetSampleInfo_Length;
	uint32_t  fGetSampleInfo_SampleDescriptionIndex;
	uint64_t  fGetSampleInfo_Offset;
	uint32_t  fGetSampleInfo_LastChunk;
	uint32_t  fGetSampleInfo_LastChunkOffset;

	uint32_t fGetSizeOfSamplesInChunk_chunkNumber;
	uint32_t fGetSizeOfSamplesInChunk_firstSample;
	uint32_t fGetSizeOfSamplesInChunk_lastSample;
	uint32_t fGetSizeOfSamplesInChunk_size;


};


//
// QTAtom class
class QTAtom_stsc : public QTAtom {

public:
	//
	// Constructors and destructor.
	QTAtom_stsc(QTFile * File, QTFile::AtomTOCEntry * Atom,
		bool Debug = false, bool DeepDebug = false);
	virtual             ~QTAtom_stsc();


	//
	// Initialization functions.
	virtual bool      Initialize();

	//
	// Accessors.

	bool             GetChunkFirstLastSample(uint32_t chunkNumber, uint32_t *firstSample, uint32_t *lastSample, QTAtom_stsc_SampleTableControlBlock *STCB);

	bool             SampleToChunkInfo(uint32_t SampleNumber,
		uint32_t *samplesPerChunk = NULL,
		uint32_t *ChunkNumber = NULL,
		uint32_t *SampleDescriptionIndex = NULL,
		uint32_t *SampleOffsetInChunk = NULL,
		QTAtom_stsc_SampleTableControlBlock * STCB = NULL);


	inline  bool      SampleNumberToChunkNumber(uint32_t SampleNumber, uint32_t *ChunkNumber = NULL, uint32_t *SampleDescriptionIndex = NULL, uint32_t *SampleOffsetInChunk = NULL,
		QTAtom_stsc_SampleTableControlBlock * STCB = NULL)
	{
		return SampleToChunkInfo(SampleNumber, NULL /*samplesPerChunk*/, ChunkNumber, SampleDescriptionIndex, SampleOffsetInChunk, STCB);
	}

	uint32_t  GetChunkFirstSample(uint32_t chunkNumber);
	//
	// Debugging functions.
	virtual void        DumpAtom();
	virtual void        DumpTable();


protected:
	//
	// Protected member variables.
	uint8_t       fVersion;
	uint32_t      fFlags; // 24 bits in the low 3 bytes

	uint32_t      fNumEntries;
	char        *fSampleToChunkTable;
	uint32_t      fTableSize;
};

#endif // QTAtom_stsc_H
