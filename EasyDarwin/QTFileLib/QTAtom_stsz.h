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
 // $Id: QTAtom_stsz.h,v 1.1 2006/01/05 13:20:36 murata Exp $
 //
 // QTAtom_stsz:
 //   The 'stsz' QTAtom class.

#ifndef QTAtom_stsz_H
#define QTAtom_stsz_H


//
// Includes
#include "QTFile.h"
#include "QTAtom.h"


//
// QTAtom class
class QTAtom_stsz : public QTAtom {

public:
	//
	// Constructors and destructor.
	QTAtom_stsz(QTFile * File, QTFile::AtomTOCEntry * Atom,
		bool Debug = false, bool DeepDebug = false);
	virtual             ~QTAtom_stsz();


	//
	// Initialization functions.
	virtual bool      Initialize();

	//
	// Accessors.
	inline  bool      SampleSize(uint32_t SampleNumber, uint32_t *Size = NULL) \
	{   if (fCommonSampleSize) {
		\
			if (Size != NULL) \
				*Size = fCommonSampleSize; \
				return true; \
	}
	else if (SampleNumber && (SampleNumber <= fNumEntries)) {
		\
			if (Size != NULL) \
				*Size = ntohl(fTable[SampleNumber - 1]); \
				return true; \
	}
	else \
		return false; \
	};

	bool      SampleRangeSize(uint32_t firstSampleNumber, uint32_t lastSampleNumber, uint32_t *sizePtr);

	//
	// Debugging functions.
	virtual void        DumpAtom();
	virtual void        DumpTable();

	inline  uint32_t      GetNumEntries() { return fNumEntries; }
	inline  uint32_t      GetCommonSampleSize() { return fCommonSampleSize; }

protected:
	//
	// Protected member variables.
	uint8_t       fVersion;
	uint32_t      fFlags; // 24 bits in the low 3 bytes
	uint32_t      fCommonSampleSize;
	uint32_t      fNumEntries;
	char        *fSampleSizeTable;
	uint32_t      *fTable; // longword-aligned version of the above
};

#endif // QTAtom_stsz_H
