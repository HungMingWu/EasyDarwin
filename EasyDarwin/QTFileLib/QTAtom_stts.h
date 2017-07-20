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
 // $Id: QTAtom_stts.h,v 1.1 2006/01/05 13:20:36 murata Exp $
 //
 // QTAtom_stts:
 //   The 'stts' QTAtom class.

#ifndef QTAtom_stts_H
#define QTAtom_stts_H


//
// Includes
#include "QTFile.h"
#include "QTAtom.h"


//
// Class state cookie
class QTAtom_stts_SampleTableControlBlock {

public:
	//
	// Constructor and destructor.
	QTAtom_stts_SampleTableControlBlock();
	virtual             ~QTAtom_stts_SampleTableControlBlock();

	//
	// Reset function
	void        Reset();

	//
	// MT->SN Sample table cache
	uint32_t              fMTtSN_CurEntry;
	uint32_t              fMTtSN_CurMediaTime, fMTtSN_CurSample;

	//
	/// SN->MT Sample table cache
	uint32_t              fSNtMT_CurEntry;
	uint32_t              fSNtMT_CurMediaTime, fSNtMT_CurSample;

	uint32_t              fGetSampleMediaTime_SampleNumber;
	uint32_t              fGetSampleMediaTime_MediaTime;

};


//
// QTAtom class
class QTAtom_stts : public QTAtom {

public:
	//
	// Constructors and destructor.
	QTAtom_stts(QTFile * File, QTFile::AtomTOCEntry * Atom,
		bool Debug = false, bool DeepDebug = false);
	virtual             ~QTAtom_stts(void);


	//
	// Initialization functions.
	virtual bool      Initialize(void);

	//
	// Accessors.
	bool      MediaTimeToSampleNumber(uint32_t MediaTime, uint32_t * SampleNumber,
		QTAtom_stts_SampleTableControlBlock * STCB);
	bool      SampleNumberToMediaTime(uint32_t SampleNumber, uint32_t * MediaTime,
		QTAtom_stts_SampleTableControlBlock * STCB);


	//
	// Debugging functions.
	virtual void        DumpAtom(void);
	virtual void        DumpTable(void);

protected:
	//
	// Protected member variables.
	uint8_t       fVersion;
	uint32_t      fFlags; // 24 bits in the low 3 bytes

	uint32_t      fNumEntries;
	char        *fTimeToSampleTable;
	uint32_t      fTableSize;

};

//
// Class state cookie
class QTAtom_ctts_SampleTableControlBlock {

public:
	//
	// Constructor and destructor.
	QTAtom_ctts_SampleTableControlBlock(void);
	virtual             ~QTAtom_ctts_SampleTableControlBlock(void);

	//
	// Reset function
	void        Reset(void);

	//
	// MT->SN Sample table cache
	uint32_t              fMTtSN_CurEntry;
	uint32_t              fMTtSN_CurMediaTime, fMTtSN_CurSample;

	//
	/// SN->MT Sample table cache
	uint32_t              fSNtMT_CurEntry;
	uint32_t              fSNtMT_CurMediaTime, fSNtMT_CurSample;

	uint32_t              fGetSampleMediaTime_SampleNumber;
	uint32_t              fGetSampleMediaTime_MediaTime;

};


//
// QTAtom class
class QTAtom_ctts : public QTAtom {

public:
	//
	// Constructors and destructor.
	QTAtom_ctts(QTFile * File, QTFile::AtomTOCEntry * Atom,
		bool Debug = false, bool DeepDebug = false);
	virtual             ~QTAtom_ctts(void);


	//
	// Initialization functions.
	virtual bool      Initialize(void);

	//
	// Accessors.
	bool      MediaTimeToSampleNumber(uint32_t MediaTime, uint32_t * SampleNumber,
		QTAtom_ctts_SampleTableControlBlock * STCB);
	bool      SampleNumberToMediaTimeOffset(uint32_t SampleNumber, uint32_t * MediaTimeOffset,
		QTAtom_ctts_SampleTableControlBlock * STCB);


	//
	// Debugging functions.
	virtual void        DumpAtom(void);
	virtual void        DumpTable(void);

protected:
	//
	// Protected member variables.
	uint8_t       fVersion;
	uint32_t      fFlags; // 24 bits in the low 3 bytes

	uint32_t      fNumEntries;
	char        *fTimeToSampleTable;

};

#endif // QTAtom_stts_H
