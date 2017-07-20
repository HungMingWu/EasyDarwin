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

	// Win compiler can't convert UInt64 to double. It does support SInt64 to double though.

	inline  double     GetDurationInSeconds() { if (fTimeScale != 0) { return (double)((SInt64)fDuration) / (double)((SInt64)fTimeScale); } else { return (double) 0.0; } }

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
	UInt32      fFlags; // 24 bits in the low 3 bytes
	UInt64      fCreationTime, fModificationTime;
	UInt32      fTimeScale;
	UInt64      fDuration;
	UInt32      fPreferredRate;
	uint16_t      fPreferredVolume;
	UInt32      fa, fb, fu, fc, fd, fv, fx, fy, fw;
	UInt32      fPreviewTime, fPreviewDuration, fPosterTime;
	UInt32      fSelectionTime, fSelectionDuration;
	UInt32      fCurrentTime;
	UInt32      fNextTrackID;
};

#endif // QTAtom_mvhd_H
