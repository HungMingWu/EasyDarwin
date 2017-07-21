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
 // QTAtom:
 //   The base-class for atoms in a QuickTime file.

#ifndef QTAtom_H
#define QTAtom_H


//
// Includes
#include "OSHeaders.h"

#include "QTFile.h"


//
// QTAtom class
class QTAtom {

public:
	//
	// Constructors and destructor.
	QTAtom(QTFile * File, QTFile::AtomTOCEntry * Atom,
		bool Debug = false, bool DeepDebug = false);
	virtual             ~QTAtom();


	//
	// Initialization functions.
	virtual bool      Initialize() { return true; }

	static int64_t   NTOH64(int64_t networkOrdered)
	{
#if BIGENDIAN
		return networkOrdered;
#else
		return (int64_t)((uint64_t)(networkOrdered << 56) | (uint64_t)(((uint64_t)0x00ff0000 << 32) & (networkOrdered << 40))
			| (uint64_t)(((uint64_t)0x0000ff00 << 32) & (networkOrdered << 24)) | (uint64_t)(((uint64_t)0x000000ff << 32) & (networkOrdered << 8))
			| (uint64_t)(((uint64_t)0x00ff0000 << 8) & (networkOrdered >> 8)) | (uint64_t)((uint64_t)0x00ff0000 & (networkOrdered >> 24))
			| (uint64_t)((uint64_t)0x0000ff00 & (networkOrdered >> 40)) | (uint64_t)((uint64_t)0x00ff & (networkOrdered >> 56)));
#endif
	}

	//
	// Read functions.
	bool      ReadBytes(uint64_t Offset, char* Buffer, uint32_t Length);
	bool      ReadInt8(uint64_t Offset, uint8_t* Datum);
	bool      ReadInt16(uint64_t Offset, uint16_t* Datum);
	bool      ReadInt32(uint64_t Offset, uint32_t* Datum);
	bool      ReadInt64(uint64_t Offset, uint64_t* Datum);
	bool      ReadInt32To64(uint64_t Offset, uint64_t* Datum);
	bool		ReadInt32To64Signed(uint64_t Offset, int64_t* Datum);

	bool      ReadSubAtomBytes(const char* AtomPath, char* Buffer, uint32_t Length);
	bool      ReadSubAtomInt8(const char* AtomPath, uint8_t* Datum);
	bool      ReadSubAtomInt16(const char* AtomPath, uint16_t* Datum);
	bool      ReadSubAtomInt32(const char* AtomPath, uint32_t* Datum);
	bool      ReadSubAtomInt64(const char* AtomPath, uint64_t* Datum);

	char*       MemMap(uint64_t Offset, uint32_t Length);
	bool      UnMap(char* memPtr, uint32_t Length);
	//
	// Debugging functions.
	virtual void        DumpAtom() {}


protected:
	//
	// Protected member variables.
	bool              fDebug;
	bool				fDeepDebug;
	QTFile*				fFile;

	QTFile::AtomTOCEntry fTOCEntry;
};

#endif // QTAtom_H
