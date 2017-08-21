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
 /*
	 File:       RTSPReflectorOutput.h

	 Contains:   Derived from ReflectorOutput, this implements the WritePacket
				 method in terms of the QTSS API (that is, it writes to a client
				 using the QTSS_RTPSessionObject



 */

#ifndef __RTSP_REFLECTOR_OUTPUT_H__
#define __RTSP_REFLECTOR_OUTPUT_H__

#include "ReflectorOutput.h"
#include "ReflectorSession.h"
#include "QTSS.h"

class RTPSessionOutput : public ReflectorOutput
{
public:
	RTPSessionOutput(RTPSession* inRTPSession, ReflectorSession* inReflectorSession,
		QTSS_Object serverPrefs, boost::string_view inCookieAddrName);
	~RTPSessionOutput() override = default;

	ReflectorSession* GetReflectorSession() { return fReflectorSession; }
	void InitializeStreams();

	// This writes the packet out to the proper QTSS_RTPStreamObject.
	// If this function returns QTSS_WouldBlock, timeToSendThisPacketAgain will
	// be set to # of msec in which the packet can be sent, or -1 if unknown
	QTSS_Error  WritePacket(const std::vector<char> &inPacketData, void* inStreamCookie, uint32_t inFlags, int64_t packetLatenessInMSec, int64_t* timeToSendThisPacketAgain, uint64_t* packetIDPtr, int64_t* arrivalTimeMSec, bool firstPacket) override;
	void TearDown() override;

	int64_t                  GetReflectorSessionInitTime() { return fReflectorSession->GetInitTimeMS(); }

	bool  IsUDP() override;

	bool  IsPlaying() override;

	void SetBufferDelay(uint32_t delay) { fBufferDelayMSecs = delay; }

private:

	RTPSession*             fClientSession;
	ReflectorSession*       fReflectorSession;
	std::string             fCookieAttrName;
	uint32_t                  fBufferDelayMSecs;
	int64_t                  fBaseArrivalTime;
	bool                  fIsUDP;
	bool                  fTransportInitialized;
	bool                  fMustSynch;
	bool                  fPreFilter;

	uint16_t GetPacketSeqNumber(const std::vector<char> &inPacket);
	void SetPacketSeqNumber(const std::vector<char> &inPacket, uint16_t inSeqNumber);
	bool PacketShouldBeThinned(QTSS_RTPStreamObject inStream, const std::vector<char> &inPacket);
	bool  FilterPacket(RTPStream *theStreamPtr, const std::vector<char> &inPacket);

	uint32_t GetPacketRTPTime(StrPtrLen* packetStrPtr);
	inline  bool PacketMatchesStream(void* inStreamCookie, RTPStream *theStreamPtr);
	bool PacketReadyToSend(RTPStream *theStreamPtr, int64_t *currentTimePtr, uint32_t inFlags, uint64_t* packetIDPtr, int64_t* timeToSendThisPacketAgainPtr);
	bool PacketAlreadySent(RTPStream *theStreamPtr, uint32_t inFlags, uint64_t* packetIDPtr);
	QTSS_Error TrackRTCPBaseTime(QTSS_RTPStreamObject *theStreamPtr, StrPtrLen* inPacketStrPtr, int64_t *currentTimePtr, uint32_t inFlags, int64_t *packetLatenessInMSec, int64_t* timeToSendThisPacketAgain, uint64_t* packetIDPtr, int64_t* arrivalTimeMSecPtr);
	QTSS_Error RewriteRTCP(QTSS_RTPStreamObject *theStreamPtr, StrPtrLen* inPacketStrPtr, int64_t *currentTimePtr, uint32_t inFlags, int64_t *packetLatenessInMSec, int64_t* timeToSendThisPacketAgain, uint64_t* packetIDPtr, int64_t* arrivalTimeMSecPtr);
	QTSS_Error TrackRTPPackets(QTSS_RTPStreamObject *theStreamPtr, StrPtrLen* inPacketStrPtr, int64_t *currentTimePtr, uint32_t inFlags, int64_t *packetLatenessInMSec, int64_t* timeToSendThisPacketAgain, uint64_t* packetIDPtr, int64_t* arrivalTimeMSecPtr);
	QTSS_Error TrackRTCPPackets(QTSS_RTPStreamObject *theStreamPtr, StrPtrLen* inPacketStrPtr, int64_t *currentTimePtr, uint32_t inFlags, int64_t *packetLatenessInMSec, int64_t* timeToSendThisPacketAgain, uint64_t* packetIDPtr, int64_t* arrivalTimeMSecPtr);
	QTSS_Error TrackPackets(QTSS_RTPStreamObject *theStreamPtr, StrPtrLen* inPacketStrPtr, int64_t *currentTimePtr, uint32_t inFlags, int64_t *packetLatenessInMSec, int64_t* timeToSendThisPacketAgain, uint64_t* packetIDPtr, int64_t* arrivalTimeMSecPtr);
};


bool RTPSessionOutput::PacketMatchesStream(void* inStreamCookie, RTPStream *theStreamPtr)
{
	boost::optional<boost::any> opt = theStreamPtr->getAttribute(fCookieAttrName);

	if (opt && boost::any_cast<void *>(opt.value()) == inStreamCookie)
		return true;

	return false;
}
#endif //__RTSP_REFLECTOR_OUTPUT_H__
