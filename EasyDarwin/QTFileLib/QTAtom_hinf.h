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
 // QTAtom_hinf:
 //   The 'hinf' QTAtom class.

#ifndef QTAtom_hinf_H
#define QTAtom_hinf_H


//
// Includes
#include "QTFile.h"
#include "QTAtom.h"


//
// QTAtom class
class QTAtom_hinf : public QTAtom {

public:
	//
	// Constructors and destructor.
	QTAtom_hinf(QTFile * File, QTFile::AtomTOCEntry * Atom,
		bool Debug = false, bool DeepDebug = false);
	virtual             ~QTAtom_hinf();


	//
	// Initialization functions.
	virtual bool      Initialize();

	//
	// Accessors.
	inline  uint64_t      GetTotalRTPBytes() { return  fTotalRTPBytes32 ? (uint64_t)fTotalRTPBytes32 : fTotalRTPBytes64; }
	inline  uint64_t      GetTotalRTPPackets() { return  fTotalRTPPackets32 ? (uint64_t)fTotalRTPPackets32 : fTotalRTPPackets64; }

	inline  uint64_t      GetTotalPayLoadBytes() { return  fTotalPayLoadBytes32 ? (uint64_t)fTotalPayLoadBytes32 : fTotalPayLoadBytes64; }

	inline  uint64_t      GetMaxDataRate() { return  fMaxDataRate64; }
	inline  uint64_t      GetTotalMediaBytes() { return  fTotalMediaBytes64; }
	inline  uint64_t      GetTotalImmediateBytes() { return  fTotalImmediateBytes64; }
	inline  uint64_t      GetRepeatBytes() { return  fTotalRepeatBytes64; }

	inline  uint32_t      GetMinTransTime() { return  fMinTransTime32; }
	inline  uint32_t      GetMaxTransTime() { return  fMaxTransTime32; }
	inline  uint32_t      GetMaxPacketSizeBytes() { return  fMaxPacketSizeBytes32; }
	inline  uint32_t      GetMaxPacketDuration() { return  fMaxPacketDuration32; }
	inline  uint32_t      GetPayLoadID() { return  fPayloadID; }
	inline  char*       GetPayLoadStr() { return  (char*)fPayloadStr; }

	//
	// Debugging functions.
	virtual void        DumpAtom();


protected:
	//
	// Protected member variables.
	uint32_t      fTotalRTPBytes32; //totl
	uint64_t      fTotalRTPBytes64; //trpy

	uint32_t      fTotalRTPPackets32; //nump
	uint64_t      fTotalRTPPackets64; //npck

	uint32_t      fTotalPayLoadBytes32; //tpay
	uint64_t      fTotalPayLoadBytes64; //tpyl
	uint64_t      fMaxDataRate64; //maxr
	uint64_t      fTotalMediaBytes64; //dmed
	uint64_t      fTotalImmediateBytes64; //dimm  
	uint64_t      fTotalRepeatBytes64; //drep

	uint32_t      fMinTransTime32; //tmin
	uint32_t      fMaxTransTime32; //tmax
	uint32_t      fMaxPacketSizeBytes32; //pmax
	uint32_t      fMaxPacketDuration32; //dmax
	uint32_t      fPayloadID;//payt
	char        fPayloadStr[262];//payt
};

#endif // QTAtom_hinf_H
