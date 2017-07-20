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
 // QTHintTrack:
 //   The central point of control for a hint track in a QTFile.

#ifndef QTHintTrack_H
#define QTHintTrack_H


//
// Includes
#include "QTTrack.h"
#include "QTAtom_hinf.h"
#include "QTAtom_tref.h"
#include "RTPMetaInfoPacket.h"
#include "MyAssert.h"


//
// External classes
class QTFile;
class QTAtom_stsc_SampleTableControlBlock;
class QTAtom_stts_SampleTableControlBlock;


class QTHintTrackRTPHeaderData {

public:
	uint16_t      rtpHeaderBits;
	uint16_t      rtpSequenceNumber;
	SInt32      relativePacketTransmissionTime;
	uint16_t      hintFlags;
	uint16_t      dataEntryCount;
	uint32_t      tlvSize;
	SInt32      tlvTimestampOffset; //'rtpo' TLV which is the timestamp offset for this packet
};

//
// Class state cookie
class QTHintTrack_HintTrackControlBlock {

public:
	//
	// Constructor and destructor.
	QTHintTrack_HintTrackControlBlock(QTFile_FileControlBlock * FCB = NULL);
	virtual             ~QTHintTrack_HintTrackControlBlock();

	//
	// If you are moving around randomly (seeking), you should call this to reset
	// caches
	void                Reset();

	//
	// If you want this HTCB to build RTP Meta Info packets,
	// tell it which fields to add, and also which IDs to assign, by passing
	// in an array of RTPMetaInfoPacket::kNumFields size, with all the right info
	void SetupRTPMetaInfo(RTPMetaInfoPacket::FieldID* inFieldArray, bool isVideo)
	{
		Assert(fRTPMetaInfoFieldArray == NULL); fRTPMetaInfoFieldArray = inFieldArray;
		fIsVideo = isVideo;
	}

	//
	// File control block
	QTFile_FileControlBlock *fFCB;

	//
	// Sample Table control blocks
	QTAtom_stsc_SampleTableControlBlock  fstscSTCB;
	QTAtom_stts_SampleTableControlBlock  fsttsSTCB;

	//
	// Sample cache
	uint32_t              fCachedSampleNumber;
	char *              fCachedSample;
	uint32_t              fCachedSampleSize, fCachedSampleLength;

	//
	// Sample (description) cache
	uint32_t              fCachedHintTrackSampleNumber, fCachedHintTrackSampleOffset;
	char *              fCachedHintTrackSample;
	uint32_t              fCachedHintTrackSampleLength;
	uint32_t              fCachedHintTrackBufferLength;

	uint16_t              fLastPacketNumberFetched;   // for optimizing Getting a packet from a cached sample
	char*               fPointerToNextPacket;       // after we get one, we point the next at this...

	//
	// To support RTP-Meta-Info payload
	RTPMetaInfoPacket::FieldID*         fRTPMetaInfoFieldArray;
	uint32_t                              fSyncSampleCursor; // Where are we in the sync sample table?
	bool                              fIsVideo; // so that we know what to do with the frame type field
	UInt64              fCurrentPacketNumber;
	UInt64              fCurrentPacketPosition;

	SInt32              fMediaTrackRefIndex;
	QTAtom_stsc_SampleTableControlBlock * fMediaTrackSTSC_STCB;

};


//
// QTHintTrack class
class QTHintTrack : public QTTrack {

public:
	//
	// Constructors and destructor.
	QTHintTrack(QTFile * File, QTFile::AtomTOCEntry * trakAtom,
		bool Debug = false, bool DeepDebug = false);
	virtual             ~QTHintTrack();


	//
	// Initialization functions.
	virtual ErrorCode   Initialize();

	bool              IsHintTrackInitialized() { return fHintTrackInitialized; }

	//
	// Accessors.
	ErrorCode   GetSDPFileLength(int * Length);
	char *      GetSDPFile(int * Length);

	inline  UInt64      GetTotalRTPBytes() { return fHintInfoAtom ? fHintInfoAtom->GetTotalRTPBytes() : 0; }
	inline  UInt64      GetTotalRTPPackets() { return fHintInfoAtom ? fHintInfoAtom->GetTotalRTPPackets() : 0; }

	inline  uint32_t      GetFirstRTPTimestamp() { return fFirstRTPTimestamp; }
	inline  void        SetAllowInvalidHintRefs(bool inAllowInvalidHintRefs) { fAllowInvalidHintRefs = inAllowInvalidHintRefs; }

	//
	// Sample functions
	bool      GetSamplePtr(uint32_t SampleNumber, char ** Buffer, uint32_t * Length,
		QTHintTrack_HintTrackControlBlock * HTCB);

	//
	// Packet functions
	inline  uint32_t      GetRTPTimescale() { return fRTPTimescale; }

	inline  uint32_t      GetRTPTimestampRandomOffset() { return fTimestampRandomOffset; }

	inline  uint16_t      GetRTPSequenceNumberRandomOffset() { return fSequenceNumberRandomOffset; }

	ErrorCode   GetNumPackets(uint32_t SampleNumber, uint16_t * NumPackets,
		QTHintTrack_HintTrackControlBlock * HTCB = NULL);

	//
	// This function will build an RTP-Meta-Info packet if the last argument
	// is non-NULL. Some caveats apply to maximize performance of this operation:
	//
	// 1.   If the "md" (media data) field is desired, please put it at the end.
	//
	// 2.   If you want to use compressed fields, pass in the field ID in the first
	//      byte of the TwoCharConst. Also set the high bit to indicate that this
	//      is a compressed field ID.
	//
	// Supported fields: tt, md, ft, pp, pn, sq
	ErrorCode   GetPacket(uint32_t SampleNumber, uint16_t PacketNumber,
		char * Buffer, uint32_t * Length,
		double * TransmitTime,
		bool dropBFrames,
		bool dropRepeatPackets = false,
		uint32_t SSRC = 0,
		QTHintTrack_HintTrackControlBlock * HTCB = NULL);

	inline ErrorCode    GetSampleData(QTHintTrack_HintTrackControlBlock * htcb, char **buffPtr, char **ppPacketBufOut, uint32_t sampleNumber, uint16_t packetNumber, uint32_t buffOutLen);

	//
	// Debugging functions.
	virtual void        DumpTrack();

	// only reliable after all of the packets have been played
	// any hint packet may reference another media track and we don't know until all have been played.
	inline int16_t GetHintTrackType() { return fHintType; }

protected:

	enum
	{
		kRepeatPacketMask = 0x0001,
		kBFrameBitMask = 0x0002
	};

	enum
	{
		kUnknown = 0,
		kOptimized = -1,
		kUnoptimized = 1
	};

	enum
	{
		kMaxHintTrackRefs = 1024
	};

	//
	// Protected member variables.
	QTAtom_hinf         *fHintInfoAtom;
	QTAtom_tref         *fHintTrackReferenceAtom;

	QTTrack             **fTrackRefs;

	uint32_t              fMaxPacketSize;
	uint32_t              fRTPTimescale, fFirstRTPTimestamp;
	uint32_t              fTimestampRandomOffset;
	uint16_t              fSequenceNumberRandomOffset;
	bool              fHintTrackInitialized;
	int16_t              fHintType;
	double  			fFirstTransmitTime;
	bool              fAllowInvalidHintRefs;
	//
	// Used by GetPacket for RTP-Meta-Info payload stuff
	void                WriteMetaInfoField(RTPMetaInfoPacket::FieldIndex inFieldIndex,
		RTPMetaInfoPacket::FieldID inFieldID,
		void* inFieldData, uint32_t inFieldLen, char** ioBuffer);

	inline QTTrack::ErrorCode   GetSamplePacketPtr(char ** samplePacketPtr, uint32_t sampleNumber, uint16_t packetNumber, QTHintTrackRTPHeaderData &hdrData, QTHintTrack_HintTrackControlBlock & htcb);
	inline void         GetSamplePacketHeaderVars(char *samplePacketPtr, char *maxBuffPtr, QTHintTrackRTPHeaderData &hdrData);
};

#endif // QTHintTrack_H
