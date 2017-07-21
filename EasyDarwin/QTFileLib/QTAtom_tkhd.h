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
 // $Id: QTAtom_tkhd.h,v 1.1 2006/01/05 13:20:36 murata Exp $
 //
 // QTAtom_tkhd:
 //   The 'tkhd' QTAtom class.

#ifndef QTAtom_tkhd_H
#define QTAtom_tkhd_H


//
// Includes
#include "OSHeaders.h"

#include "QTFile.h"
#include "QTAtom.h"


//
// QTAtom class
class QTAtom_tkhd : public QTAtom {
	//
	// Class constants
	enum {
		flagEnabled = 0x00000001,
		flagInMovie = 0x00000002,
		flagInPreview = 0x00000004,
		flagInPoster = 0x00000008
	};


public:
	//
	// Constructors and destructor.
	QTAtom_tkhd(QTFile * File, QTFile::AtomTOCEntry * Atom,
		bool Debug = false, bool DeepDebug = false);
	virtual             ~QTAtom_tkhd();


	//
	// Initialization functions.
	virtual bool      Initialize();

	//
	// Accessors.
	inline  uint32_t      GetTrackID() { return fTrackID; }
	inline  uint32_t      GetFlags() { return fFlags; }
	inline  uint64_t      GetCreationTime() { return fCreationTime; }
	inline  uint64_t      GetModificationTime() { return fModificationTime; }
	inline  uint64_t      GetDuration() { return fDuration; }


	//
	// Debugging functions.
	virtual void        DumpAtom();


protected:
	//
	// Protected member variables.
	uint8_t       fVersion;
	uint32_t      fFlags; // 24 bits in the low 3 bytes
	uint64_t      fCreationTime, fModificationTime;
	uint32_t      fTrackID;
	uint32_t      freserved1;
	uint64_t      fDuration;
	uint32_t      freserved2, freserved3;
	uint16_t      fLayer, fAlternateGroup;
	uint16_t      fVolume;
	uint16_t      freserved4;
	uint32_t      fa, fb, fu, fc, fd, fv, fx, fy, fw;
	uint32_t      fTrackWidth, fTrackHeight;
};

#endif // QTAtom_tkhd_H
