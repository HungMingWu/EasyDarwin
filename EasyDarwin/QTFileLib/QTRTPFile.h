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
 // $Id: QTRTPFile.h,v 1.1 2006/01/05 13:20:36 murata Exp $
 //
 // QTRTPFile:
 //   An interface to QTFile for TimeShare.

#ifndef QTRTPFile_H
#define QTRTPFile_H


//
// Includes
#include "OSHeaders.h"
#include "RTPMetaInfoPacket.h"
#include "QTHintTrack.h"

#ifndef __Win32__
#include <sys/stat.h>
#endif

//
// Constants
#define QTRTPFILE_MAX_PACKET_LENGTH     2048


//
// QTRTPFile class
class OSMutex;

class QTFile;
class QTFile_FileControlBlock;
class QTHintTrack;
class QTHintTrack_HintTrackControlBlock;

class QTRTPFile {

public:
	//
	// Class error codes
	enum ErrorCode {
		errNoError = 0,
		errFileNotFound = 1,
		errInvalidQuickTimeFile = 2,
		errNoHintTracks = 3,
		errTrackIDNotFound = 4,
		errCallAgain = 5,
		errInternalError = 100
	};


	//
	// Class typedefs.
	struct RTPFileCacheEntry {
		//
		// Init mutex (do not use this entry until you have acquired and
		// released this.
		OSMutex     *InitMutex;

		//
		// File information
		char*       fFilename;
		QTFile      *File;

		//
		// Reference count for this cache entry
		int         ReferenceCount;

		//
		// List pointers
		RTPFileCacheEntry   *PrevEntry, *NextEntry;
	};

	struct RTPTrackListEntry {

		//
		// Track information
		uint32_t          TrackID;
		QTHintTrack     *HintTrack;
		QTHintTrack_HintTrackControlBlock   *HTCB;
		bool          IsTrackActive, IsPacketAvailable;
		uint32_t          QualityLevel;

		//
		// Server information
		void            *Cookie1;
		uint32_t          Cookie2;
		uint32_t          SSRC;
		uint16_t          FileSequenceNumberRandomOffset, BaseSequenceNumberRandomOffset,
			LastSequenceNumber;
		SInt32          SequenceNumberAdditive;
		uint32_t          FileTimestampRandomOffset, BaseTimestampRandomOffset;

		//
		// Sample/Packet information
		uint32_t          CurSampleNumber;
		uint32_t          ConsecutivePFramesSent;
		uint32_t          TargetPercentage;
		uint32_t          SampleToSeekTo;
		uint32_t          LastSyncSampleNumber;
		uint32_t          NextSyncSampleNumber;
		uint16_t          NumPacketsInThisSample, CurPacketNumber;

		double         CurPacketTime;
		char            CurPacket[QTRTPFILE_MAX_PACKET_LENGTH];
		uint32_t          CurPacketLength;

		//
		// List pointers
		RTPTrackListEntry   *NextTrack;
	};


public:
	//
	// Global initialize function; CALL THIS FIRST!
	static void         Initialize();

	//
	// Returns a static array of the RTP-Meta-Info fields supported by QTFileLib.
	// It also returns field IDs for the fields it recommends being compressed.
	static const RTPMetaInfoPacket::FieldID*        GetSupportedRTPMetaInfoFields() { return kMetaInfoFields; }

	//
	// Constructors and destructor.
	QTRTPFile(bool Debug = false, bool DeepDebug = false);

	virtual             ~QTRTPFile();


	//
	// Initialization functions.
	virtual ErrorCode   Initialize(const char * FilePath);

	void AllocateSharedBuffers(uint32_t inUnitSizeInK, uint32_t inBufferInc, uint32_t inBufferSizeUnits, uint32_t inMaxBitRateBuffSizeInBlocks)
	{
		fFile->AllocateBuffers(inUnitSizeInK, inBufferInc, inBufferSizeUnits, inMaxBitRateBuffSizeInBlocks, this->GetBytesPerSecond() * 8);
	}

	void AllocatePrivateBuffers(uint32_t inUnitSizeInK, uint32_t inNumBuffSizeUnits, uint32_t inMaxBitRateBuffSizeInBlocks);

	//
	// Accessors
	double     GetMovieDuration();
	UInt64      GetAddedTracksRTPBytes();
	char *      GetSDPFile(int * SDPFileLength);
	uint32_t      GetBytesPerSecond();

	char*       GetMoviePath();
	QTFile*     GetQTFile() { return fFile; }

	//
	// Track functions

			//
			// AddTrack
			//
			// If you would like this track to be an RTP-Meta-Info stream, pass in
			// the field names you would like to see
	ErrorCode   AddTrack(uint32_t TrackID, bool UseRandomOffset = true);


	double     GetTrackDuration(uint32_t TrackID);
	uint32_t      GetTrackTimeScale(uint32_t TrackID);

	void        SetTrackSSRC(uint32_t TrackID, uint32_t SSRC);
	void        SetTrackCookies(uint32_t TrackID, void * Cookie1, uint32_t Cookie2);
	void        SetAllowInvalidHintRefs(bool inAllowInvalidHintRefs) { fAllowInvalidHintRefs = inAllowInvalidHintRefs; }

	//
	// If you want QTRTPFile to output an RTP-Meta-Info packet instead
	// of a normal RTP packet for this track, call this function and
	// pass in a proper Field ID array (see RTPMetaInfoPacket.h) to
	// tell QTRTPFile which fields to include and which IDs to use with the fields.
	// You have to let this function know whether this is a video track or not.
	void        SetTrackRTPMetaInfo(uint32_t TrackID, RTPMetaInfoPacket::FieldID* inFieldArray, bool isVideo);

	//
	// What sort of packets do you want?
	enum
	{
		kAllPackets = 0,
		kNoBFrames = 1,
		k75PercentPFrames = 2,
		k50PercentPFrames = 3,
		k25PercentPFrames = 4,
		kKeyFramesOnly = 5,
		kKeyFramesPlusOneP = 6			//Special quality level with Key frames followed by 1 P frame
	};

	void SetTrackQualityLevel(RTPTrackListEntry* inEntry, uint32_t inNewLevel);
	//
	// Packet functions
	ErrorCode   Seek(double Time, double MaxBackupTime = 3.0);
	ErrorCode   SeekToPacketNumber(uint32_t inTrackID, UInt64 inPacketNumber);

	uint32_t      GetSeekTimestamp(uint32_t TrackID);
	double     GetRequestedSeekTime() { return fRequestedSeekTime; }
	double     GetActualSeekTime() { return fSeekTime; }
	double     GetFirstPacketTransmitTime();
	RTPTrackListEntry* GetLastPacketTrack() { return fLastPacketTrack; }
	uint32_t      GetNumSkippedSamples() { return fNumSkippedSamples; }

	uint16_t      GetNextTrackSequenceNumber(uint32_t TrackID);
	double     GetNextPacket(char ** Packet, int * PacketLength);

	SInt32      GetMovieHintType();
	bool      DropRepeatPackets() { return fDropRepeatPackets; }
	bool      SetDropRepeatPackets(bool allowRepeatPackets) { (!fHasRTPMetaInfoFieldArray) ? fDropRepeatPackets = allowRepeatPackets : fDropRepeatPackets = false; return fDropRepeatPackets; }

	ErrorCode   Error() { return fErr; };

	bool      FindTrackEntry(uint32_t TrackID, RTPTrackListEntry **TrackEntry);
protected:
	//
	// Protected cache functions and variables.
	static  OSMutex             *gFileCacheMutex, *gFileCacheAddMutex;
	static  RTPFileCacheEntry   *gFirstFileCacheEntry;

	static  ErrorCode   new_QTFile(const char * FilePath, QTFile ** File, bool Debug = false, bool DeepDebug = false);
	static  void        delete_QTFile(QTFile * File);

	static  void        AddFileToCache(const char *inFilename, QTRTPFile::RTPFileCacheEntry ** NewListEntry);
	static  bool      FindAndRefcountFileCacheEntry(const char *inFilename, QTRTPFile::RTPFileCacheEntry **CacheEntry);

	//
	// Protected member functions.
	bool      PrefetchNextPacket(RTPTrackListEntry * TrackEntry, bool doSeek = false);
	ErrorCode   ScanToCorrectSample();
	ErrorCode   ScanToCorrectPacketNumber(uint32_t inTrackID, UInt64 inPacketNumber);

	//
	// Protected member variables.
	bool              fDebug, fDeepDebug;

	QTFile              *fFile;
	QTFile_FileControlBlock *fFCB;

	uint32_t              fNumHintTracks;
	RTPTrackListEntry   *fFirstTrack, *fLastTrack, *fCurSeekTrack;

	char                *fSDPFile;
	uint32_t              fSDPFileLength;
	uint32_t              fNumSkippedSamples;

	double             fRequestedSeekTime, fSeekTime;

	RTPTrackListEntry   *fLastPacketTrack;

	uint32_t              fBytesPerSecond;

	bool              fHasRTPMetaInfoFieldArray;
	bool              fWasLastSeekASeekToPacketNumber;
	bool              fDropRepeatPackets;
	bool              fAllowInvalidHintRefs;
	ErrorCode           fErr;

	static const RTPMetaInfoPacket::FieldID kMetaInfoFields[];
};

#endif // QTRTPFile
