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
 // QTAtom_elst:
 //   The 'elst' QTAtom class.

#ifndef QTAtom_elst_H
#define QTAtom_elst_H


//
// Includes
#include "QTFile.h"
#include "QTAtom.h"


//
// External classes
class QTFile_FileControlBlock;


//
// QTAtom class
class QTAtom_elst : public QTAtom {
	//
	// Class typedefs.
	struct EditListEntry {
		// Edit information
		uint64_t          EditDuration;
		int64_t          StartingMediaTime;
		uint32_t          EditMediaRate;
	};


public:
	//
	// Constructors and destructor.
	QTAtom_elst(QTFile * File, QTFile::AtomTOCEntry * Atom,
		bool Debug = false, bool DeepDebug = false);
	virtual             ~QTAtom_elst();


	//
	// Initialization functions.
	virtual bool      Initialize();

	//
	// Accessors.
	inline  uint64_t      FirstEditMovieTime() { return fFirstEditMovieTime; }


	//
	// Debugging functions.
	virtual void        DumpAtom();


protected:
	//
	// Protected member variables.
	uint8_t       fVersion;
	uint32_t      fFlags; // 24 bits in the low 3 bytes

	uint32_t          fNumEdits;
	EditListEntry   *fEdits;

	uint64_t      fFirstEditMovieTime;
};

#endif // QTAtom_elst_H
