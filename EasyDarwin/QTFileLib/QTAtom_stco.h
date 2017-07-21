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
 // QTAtom_stco:
 //   The 'stco' QTAtom class.

#ifndef QTAtom_stco_H
#define QTAtom_stco_H


//
// Includes
#ifndef __Win32__
#include <netinet/in.h>
#endif
#include "QTFile.h"
#include "QTAtom.h"


//
// QTAtom class
class QTAtom_stco : public QTAtom {

public:
	//
	// Constructors and destructor.
	QTAtom_stco(QTFile * File, QTFile::AtomTOCEntry * Atom,
		uint16_t offSetSize = 4, bool Debug = false, bool DeepDebug = false);
	virtual             ~QTAtom_stco();


	//
	// Initialization functions.
	virtual bool      Initialize();

	//
	// Accessors.

	inline  bool      ChunkOffset(uint32_t ChunkNumber, uint64_t *Offset = NULL)
	{
		if (Offset && ChunkNumber && (ChunkNumber <= fNumEntries))
		{
			if (4 == fOffSetSize)
				*Offset = (uint64_t)ntohl(((uint32_t *)fTable)[ChunkNumber - 1]);
			else
				*Offset = (uint64_t)QTAtom::NTOH64(((uint64_t *)fTable)[ChunkNumber - 1]);

			return true;
		}

		return false;
	}


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
	uint16_t      fOffSetSize;
	char        *fChunkOffsetTable;
	void        *fTable; // longword-aligned version of the above
};

#endif // QTAtom_stco_H
