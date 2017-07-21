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
 // QTAtom_mvhd:
 //   The 'mvhd' QTAtom class.

#ifndef QTAtom_mvhd_H
#define QTAtom_mvhd_H


//
// Includes
#include "QTFile.h"
#include "QTAtom.h"


//
// QTAtom class
class QTAtom_mvhd : public QTAtom {

public:
	//
	// Constructors and destructor.
	QTAtom_mvhd(QTFile * File, QTFile::AtomTOCEntry * Atom,
		bool Debug = false, bool DeepDebug = false);
	virtual             ~QTAtom_mvhd();


	//
	// Initialization functions.
	virtual bool      Initialize();

	//
	// Accessors.
	inline  double     GetTimeScale() { return (double)fTimeScale; }
#if __Win32__

	// Win compiler can't convert uint64_t to double. It does support int64_t to double though.

	inline  double     GetDurationInSeconds() { if (fTimeScale != 0) { return (double)((int64_t)fDuration) / (double)((int64_t)fTimeScale); } else { return (double) 0.0; } }

#else

	inline  double     GetDurationInSeconds() { if (fTimeScale != 0) { return fDuration / (double)fTimeScale; } else { return (double) 0.0; } }
#endif

	//
	// Debugging functions.
	virtual void        DumpAtom();


protected:
	//
	// Protected member variables.
	uint8_t       fVersion;
	uint32_t      fFlags; // 24 bits in the low 3 bytes
	uint64_t      fCreationTime, fModificationTime;
	uint32_t      fTimeScale;
	uint64_t      fDuration;
	uint32_t      fPreferredRate;
	uint16_t      fPreferredVolume;
	uint32_t      fa, fb, fu, fc, fd, fv, fx, fy, fw;
	uint32_t      fPreviewTime, fPreviewDuration, fPosterTime;
	uint32_t      fSelectionTime, fSelectionDuration;
	uint32_t      fCurrentTime;
	uint32_t      fNextTrackID;
};

#endif // QTAtom_mvhd_H
