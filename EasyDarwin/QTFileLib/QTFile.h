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
 // $Id: QTFile.h,v 1.1 2006/01/05 13:20:36 murata Exp $
 //
 // QTFile:
 //   The central point of control for a file in the QuickTime File Format.

#ifndef QTFile_H
#define QTFile_H


//
// Includes
#include "OSHeaders.h"
#include "OSFileSource.h"
#include "QTFile_FileControlBlock.h"
#include "DateTranslator.h"

//
// External classes
class OSMutex;

class QTAtom_mvhd;
class QTTrack;


//
// QTFile class
class QTFile {

public:
	//
	// Class constants


	//
	// Class error codes
	enum ErrorCode {
		errNoError = 0,
		errFileNotFound = 1,
		errInvalidQuickTimeFile = 2,
		errInternalError = 100
	};


	//
	// Class typedefs.
	struct AtomTOCEntry {
		// TOC id (used to compare TOCs)
		uint32_t          TOCID;

		// Atom information
		OSType          AtomType, beAtomType; // be = Big Endian

		UInt64          AtomDataPos;
		UInt64          AtomDataLength;
		uint32_t          AtomHeaderSize;

		// TOC pointers
		AtomTOCEntry    *NextOrdAtom;

		AtomTOCEntry    *PrevAtom, *NextAtom;
		AtomTOCEntry    *Parent, *FirstChild;
	};

	struct TrackListEntry {
		// Track information
		uint32_t          TrackID;
		QTTrack         *Track;
		bool          IsHintTrack;

		// List pointers
		TrackListEntry  *NextTrack;
	};


public:
	//
	// Constructors and destructor.
	QTFile(bool Debug = false, bool DeepDebug = false);
	virtual             ~QTFile();


	//
	// Open a movie file and generate the atom table of contents.
	ErrorCode   Open(const char * MoviePath);

	OSMutex*    GetMutex() { return fReadMutex; }

	//
	// Table of Contents functions.
	bool      FindTOCEntry(const char * AtomPath,
		AtomTOCEntry **TOCEntry,
		AtomTOCEntry *LastFoundTOCEntry = NULL);

	//
	// Track List functions
	inline  uint32_t      GetNumTracks() { return fNumTracks; }
	bool      NextTrack(QTTrack **Track, QTTrack *LastFoundTrack = NULL);
	bool      FindTrack(uint32_t TrackID, QTTrack **Track);
	bool      IsHintTrack(QTTrack *Track);

	//
	// Accessors
	inline  char *      GetMoviePath() { return fMoviePath; }
	double     GetTimeScale(void);
	double     GetDurationInSeconds();
	SInt64      GetModDate();
	// Returns the mod date as a RFC 1123 formatted string
	char*       GetModDateStr();
	//
	// Read functions.
	bool      Read(UInt64 Offset, char * const Buffer, uint32_t Length, QTFile_FileControlBlock * FCB = NULL);


	void        AllocateBuffers(uint32_t inUnitSizeInK, uint32_t inBufferInc, uint32_t inBufferSize, uint32_t inMaxBitRateBuffSizeInBlocks, uint32_t inBitrate);
#if DSS_USE_API_CALLBACKS
	void        IncBufferUserCount() { if (fOSFileSourceFD != NULL) fOSFileSourceFD->IncMaxBuffers(); }
	void        DecBufferUserCount() { if (fOSFileSourceFD != NULL) fOSFileSourceFD->DecMaxBuffers(); }
#else
	void        IncBufferUserCount() { fMovieFD.IncMaxBuffers(); }
	void        DecBufferUserCount() { fMovieFD.DecMaxBuffers(); }
#endif

	inline bool       ValidTOC();


	char*      MapFileToMem(UInt64 offset, uint32_t length);

	int         UnmapMem(char *memPtr, uint32_t length);

	//
	// Debugging functions.
	void        DumpAtomTOC();

protected:
	//
	// Protected member functions.
	bool      GenerateAtomTOC();

	//
	// Protected member variables.
	bool              fDebug, fDeepDebug;

	uint32_t              fNextTOCID;
#if DSS_USE_API_CALLBACKS
	QTSS_Object         fMovieFD;
	OSFileSource        *fOSFileSourceFD;
#else
	OSFileSource        fMovieFD;
#endif
	bool              fCacheBuffersSet;

	DateBuffer          fModDateBuffer;
	char                *fMoviePath;

	AtomTOCEntry        *fTOC, *fTOCOrdHead, *fTOCOrdTail;

	uint32_t              fNumTracks;
	TrackListEntry      *fFirstTrack, *fLastTrack;

	QTAtom_mvhd         *fMovieHeaderAtom;

	OSMutex             *fReadMutex;
	int                  fFile;

};

bool QTFile::ValidTOC()
{
	UInt64 theLength = 0;
	UInt64 thePos = 0;

#if DSS_USE_API_CALLBACKS
	uint32_t theDataLen = sizeof(UInt64);
	(void)QTSS_GetValue(fMovieFD, qtssFlObjLength, 0, (void*)&theLength, &theDataLen);
	(void)QTSS_GetValue(fMovieFD, qtssFlObjPosition, 0, (void*)&thePos, &theDataLen);
	//  qtss_printf("GenerateAtomTOC failed CurPos=%"_64BITARG_"u < Length=%"_64BITARG_"u\n", CurPos, theLength);
#else
	theLength = fMovieFD.GetLength();
	thePos = fMovieFD.GetCurOffset();
#endif

	if (thePos < theLength) // failure pos not at end of file
	{
		//      qtss_printf("GenerateAtomTOC failed CurPos=%"_64BITARG_"u < Length=%"_64BITARG_"u\n", CurPos, theLength);
		return false;
	}

	return true;
}

#endif // QTFile_H
