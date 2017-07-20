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
 // QTTrack:
 //   The central point of control for a track in a QTFile.

#ifndef QTTrack_H
#define QTTrack_H


//
// Includes
#include "QTAtom_dref.h"
#include "QTAtom_elst.h"
#include "QTAtom_mdhd.h"
#include "QTAtom_tkhd.h"
#include "QTAtom_stco.h"
#include "QTAtom_stsc.h"
#include "QTAtom_stsd.h"
#include "QTAtom_stss.h"
#include "QTAtom_stsz.h"
#include "QTAtom_stts.h"


//
// External classes
class QTFile;
class QTFile_FileControlBlock;
class QTAtom_stsc_SampleTableControlBlock;
class QTAtom_stts_SampleTableControlBlock;


//
// QTTrack class
class QTTrack {

public:
	//
	// Class error codes
	enum ErrorCode {
		errNoError = 0,
		errInvalidQuickTimeFile = 1,
		errParamError = 2,
		errIsSkippedPacket = 3,
		errInternalError = 100
	};


public:
	//
	// Constructors and destructor.
	QTTrack(QTFile * File, QTFile::AtomTOCEntry * trakAtom,
		bool Debug = false, bool DeepDebug = false);
	virtual             ~QTTrack();


	//
	// Initialization functions.
	virtual ErrorCode   Initialize();

	//
	// Accessors.
	inline  bool      IsInitialized() { return fIsInitialized; }

	inline  const char *GetTrackName() { return (fTrackName ? fTrackName : ""); }
	inline  uint32_t      GetTrackID() { return fTrackHeaderAtom->GetTrackID(); }
	inline  UInt64      GetCreationTime() { return fTrackHeaderAtom->GetCreationTime(); }
	inline  UInt64      GetModificationTime() { return fTrackHeaderAtom->GetModificationTime(); }
	inline  SInt64      GetDuration() { return (SInt64)fTrackHeaderAtom->GetDuration(); }
	inline  double     GetTimeScale() { return fMediaHeaderAtom->GetTimeScale(); }
	inline  double     GetTimeScaleRecip() { return fMediaHeaderAtom->GetTimeScaleRecip(); }
	inline  double     GetDurationInSeconds() { return GetDuration() / (double)GetTimeScale(); }
	inline  UInt64      GetFirstEditMovieTime(void)
	{
		if (fEditListAtom != NULL) return fEditListAtom->FirstEditMovieTime();
		else return 0;
	}
	inline  uint32_t      GetFirstEditMediaTime() { return fFirstEditMediaTime; }

	//
	// Sample functions
	bool              GetSizeOfSamplesInChunk(uint32_t chunkNumber, uint32_t * const sizePtr, uint32_t * const firstSampleNumPtr, uint32_t * const lastSampleNumPtr, QTAtom_stsc_SampleTableControlBlock * stcbPtr);

	inline  bool      GetChunkFirstLastSample(uint32_t chunkNumber, uint32_t *firstSample, uint32_t *lastSample,
		QTAtom_stsc_SampleTableControlBlock *STCB)
	{
		return fSampleToChunkAtom->GetChunkFirstLastSample(chunkNumber, firstSample, lastSample, STCB);
	}


	inline  bool      SampleToChunkInfo(uint32_t SampleNumber, uint32_t *samplesPerChunk, uint32_t *ChunkNumber, uint32_t *SampleDescriptionIndex, uint32_t *SampleOffsetInChunk,
		QTAtom_stsc_SampleTableControlBlock * STCB)
	{
		return fSampleToChunkAtom->SampleToChunkInfo(SampleNumber, samplesPerChunk, ChunkNumber, SampleDescriptionIndex, SampleOffsetInChunk, STCB);
	}


	inline  bool      SampleNumberToChunkNumber(uint32_t SampleNumber, uint32_t *ChunkNumber, uint32_t *SampleDescriptionIndex, uint32_t *SampleOffsetInChunk,
		QTAtom_stsc_SampleTableControlBlock * STCB)
	{
		return fSampleToChunkAtom->SampleNumberToChunkNumber(SampleNumber, ChunkNumber, SampleDescriptionIndex, SampleOffsetInChunk, STCB);
	}


	inline  uint32_t      GetChunkFirstSample(uint32_t chunkNumber)
	{
		return fSampleToChunkAtom->GetChunkFirstSample(chunkNumber);
	}

	inline  bool      ChunkOffset(uint32_t ChunkNumber, UInt64 *Offset = NULL)
	{
		return fChunkOffsetAtom->ChunkOffset(ChunkNumber, Offset);
	}

	inline  bool      SampleSize(uint32_t SampleNumber, uint32_t *Size = NULL)
	{
		return fSampleSizeAtom->SampleSize(SampleNumber, Size);
	}

	inline  bool      SampleRangeSize(uint32_t firstSample, uint32_t lastSample, uint32_t *sizePtr = NULL)
	{
		return fSampleSizeAtom->SampleRangeSize(firstSample, lastSample, sizePtr);
	}

	bool      GetSampleInfo(uint32_t SampleNumber, uint32_t * const Length, UInt64 * const Offset, uint32_t * const SampleDescriptionIndex,
		QTAtom_stsc_SampleTableControlBlock * STCB);

	bool      GetSample(uint32_t SampleNumber, char * Buffer, uint32_t * Length, QTFile_FileControlBlock * FCB,
		QTAtom_stsc_SampleTableControlBlock * STCB);

	inline  bool      GetSampleMediaTime(uint32_t SampleNumber, uint32_t * const MediaTime,
		QTAtom_stts_SampleTableControlBlock * STCB)
	{
		return fTimeToSampleAtom->SampleNumberToMediaTime(SampleNumber, MediaTime, STCB);
	}

	inline  bool      GetSampleNumberFromMediaTime(uint32_t MediaTime, uint32_t * const SampleNumber,
		QTAtom_stts_SampleTableControlBlock * STCB)
	{
		return fTimeToSampleAtom->MediaTimeToSampleNumber(MediaTime, SampleNumber, STCB);
	}


	inline  void        GetPreviousSyncSample(uint32_t SampleNumber, uint32_t * SyncSampleNumber)
	{
		if (fSyncSampleAtom != NULL) fSyncSampleAtom->PreviousSyncSample(SampleNumber, SyncSampleNumber);
		else *SyncSampleNumber = SampleNumber;
	}

	inline  void        GetNextSyncSample(uint32_t SampleNumber, uint32_t * SyncSampleNumber)
	{
		if (fSyncSampleAtom != NULL) fSyncSampleAtom->NextSyncSample(SampleNumber, SyncSampleNumber);
		else *SyncSampleNumber = SampleNumber + 1;
	}

	inline bool           IsSyncSample(uint32_t SampleNumber, uint32_t SyncSampleCursor)
	{
		if (fSyncSampleAtom != NULL) return fSyncSampleAtom->IsSyncSample(SampleNumber, SyncSampleCursor);
		else return true;
	}
	//
	// Read functions.
	inline  bool      Read(uint32_t SampleDescriptionID, UInt64 Offset, char * const Buffer, uint32_t Length,
		QTFile_FileControlBlock * FCB = NULL)
	{
		return fDataReferenceAtom->Read(fSampleDescriptionAtom->SampleDescriptionToDataReference(SampleDescriptionID), Offset, Buffer, Length, FCB);
	}

	inline bool       GetSampleMediaTimeOffset(uint32_t SampleNumber, uint32_t *mediaTimeOffset, QTAtom_ctts_SampleTableControlBlock * STCB)
	{
		if (fCompTimeToSampleAtom)
			return fCompTimeToSampleAtom->SampleNumberToMediaTimeOffset(SampleNumber, mediaTimeOffset, STCB);
		else
			return false;
	}
	//
	// Debugging functions.
	virtual void        DumpTrack();
	inline  void        DumpSampleToChunkTable() { fSampleToChunkAtom->DumpTable(); }
	inline  void        DumpChunkOffsetTable() { fChunkOffsetAtom->DumpTable(); }
	inline  void        DumpSampleSizeTable() { fSampleSizeAtom->DumpTable(); }
	inline  void        DumpTimeToSampleTable() { fTimeToSampleAtom->DumpTable(); }
	inline  void        DumpCompTimeToSampleTable() { if (fCompTimeToSampleAtom) fCompTimeToSampleAtom->DumpTable(); else printf("*** no ctts table ****\n"); }


protected:
	//
	// Protected member variables.
	bool              fDebug, fDeepDebug;
	QTFile              *fFile;
	QTFile::AtomTOCEntry fTOCEntry;

	bool              fIsInitialized;

	QTAtom_tkhd         *fTrackHeaderAtom;
	char                *fTrackName;

	QTAtom_mdhd         *fMediaHeaderAtom;

	QTAtom_elst         *fEditListAtom;
	QTAtom_dref         *fDataReferenceAtom;

	QTAtom_stts         *fTimeToSampleAtom;
	QTAtom_ctts         *fCompTimeToSampleAtom;
	QTAtom_stsc         *fSampleToChunkAtom;
	QTAtom_stsd         *fSampleDescriptionAtom;
	QTAtom_stco         *fChunkOffsetAtom;
	QTAtom_stsz         *fSampleSizeAtom;
	QTAtom_stss         *fSyncSampleAtom;

	uint32_t              fFirstEditMediaTime;
};

#endif // QTTrack_H
