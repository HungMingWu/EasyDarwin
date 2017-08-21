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
	 File:       RTSPReflectorOutput.cpp

	 Contains:   Implementation of object in .h file


 */

#include "RTPSession.h"
#include "RTPSessionOutput.h"
#include "ReflectorStream.h"


#if DEBUG 
#define RTP_SESSION_DEBUGGING 0
#else
#define RTP_SESSION_DEBUGGING 0
#endif

static boost::string_view        sLastRTCPTransmit = "qtssReflectorStreamLastRTCPTransmit";
static boost::string_view        sNextSeqNum = "qtssNextSeqNum";
static boost::string_view        sSeqNumOffset = "qtssSeqNumOffset";
static boost::string_view        sLastQualityChange = "qtssLastQualityChange";
static boost::string_view        sLastRTPPacketID = "qtssReflectorStreamLastRTPPacketID";
static boost::string_view        sLastRTCPPacketID = "qtssReflectorStreamLastRTCPPacketID";
static boost::string_view        sFirstRTPCurrentTime = "qtssReflectorStreamStartRTPCurrent";
static boost::string_view        sFirstRTPArrivalTime = "qtssReflectorStreamStartRTPArrivalTime";
static boost::string_view        sFirstRTPTimeStamp = "qtssReflectorStreamStartRTPTimeStamp";
static boost::string_view        sBaseRTPTimeStamp = "qtssReflectorStreamBaseRTPTimeStamp";
static boost::string_view        sBaseArrivalTimeStamp = "qtssReflectorStreamBaseArrivalTime";
static boost::string_view        sStreamSSRC = "qtssReflectorStreamSSRC";
static boost::string_view        sStreamPacketCount = "qtssReflectorStreamPacketCount";
static boost::string_view        sStreamByteCount = "qtssReflectorStreamByteCount";
//static boost::string_view        sFirstRTCPArrivalTime = "qtssReflectorStreamStartRTCPArrivalTime";
//static boost::string_view        sLastRTPTimeStamp = "qtssReflectorStreamLastRTPTimeStamp";
//static boost::string_view        sFirstRTCPTimeStamp = "qtssReflectorStreamStartRTCPTimeStamp";
//static boost::string_view        sFirstRTCPCurrentTime = "qtssReflectorStreamStartRTCPCurrent";

RTPSessionOutput::RTPSessionOutput(RTPSession* inClientSession, ReflectorSession* inReflectorSession,
	QTSS_Object serverPrefs, boost::string_view inCookieAddrName)
	: fClientSession(inClientSession),
	fReflectorSession(inReflectorSession),
	fCookieAttrName(inCookieAddrName),
	fBufferDelayMSecs(ReflectorStream::sOverBufferInMsec),
	fBaseArrivalTime(0),
	fIsUDP(false),
	fTransportInitialized(false),
	fMustSynch(true),
	fPreFilter(true)
{
	// create a bookmark for each stream we'll reflect
	this->InititializeBookmarks(inReflectorSession->GetNumStreams());

}

bool RTPSessionOutput::IsPlaying()
{
	if (!fClientSession)
		return false;

	return fClientSession->GetSessionState() == qtssPlayingState;
}

void RTPSessionOutput::InitializeStreams()
{
	for (auto theStreamPtr : fClientSession->GetStreams())
		theStreamPtr->addAttribute(sStreamPacketCount, (uint32_t)0);
}



bool RTPSessionOutput::IsUDP()
{
	if (fTransportInitialized)
		return fIsUDP;


	uint32_t                  theLen = 0;
	if (fClientSession->GetSessionState() != qtssPlayingState);
		return true;

	QTSS_RTPTransportType *theTransportTypePtr = nullptr;
	for (auto theStreamPtr : fClientSession->GetStreams())
	{
		QTSS_RTPTransportType theTransportType = theStreamPtr->GetTransportType();
		if (theTransportType == qtssRTPTransportTypeUDP)
		{
			fIsUDP = true;
			break; // treat entire session UDP
		}
		else
		{
			fIsUDP = false;
		}
	}

	//if (fIsUDP) printf("RTPSessionOutput::RTPSessionOutput Standard UDP client\n");
	 //else printf("RTPSessionOutput::RTPSessionOutput Buffered Client\n");

	fTransportInitialized = true;
	return fIsUDP;
}


bool  RTPSessionOutput::FilterPacket(RTPStream *theStreamPtr, const std::vector<char> &inPacket)
{
	uint32_t theLen = 0;

	//see if we started sending and if so then just keep sending (reset on a play)
	boost::optional<boost::any> opt = theStreamPtr->getAttribute(sStreamPacketCount);
	if (opt && boost::any_cast<uint32_t>(opt.value()) > 0)
		return false;

	Assert(theStreamPtr);

	uint16_t seqnum = this->GetPacketSeqNumber(inPacket);
	uint16_t firstSeqNum = theStreamPtr->GetSeqNumber();

	if (seqnum < firstSeqNum)
	{
		//printf("RTPSessionOutput::FilterPacket don't send packet = %u < first=%lu\n", seqnum, firstSeqNum);
		return true;
	}

	//printf("RTPSessionOutput::FilterPacket found first packet = %u \n", firstSeqNum);

	fPreFilter = false;
	return fPreFilter;
}


bool  RTPSessionOutput::PacketAlreadySent(RTPStream *theStreamPtr, uint32_t inFlags, uint64_t* packetIDPtr)
{
	Assert(theStreamPtr);
	Assert(packetIDPtr);

	uint32_t theLen = 0;
	uint64_t *lastPacketIDPtr = nullptr;
	bool packetSent = false;

	if (inFlags & qtssWriteFlagsIsRTP)
	{
		boost::optional<boost::any> opt = theStreamPtr->getAttribute(sLastRTPPacketID);
		if (opt && *packetIDPtr <= boost::any_cast<uint64_t>(opt.value()))
		{
			//printf("RTPSessionOutput::WritePacket Don't send RTP packet id =%qu\n", *packetIDPtr);
			packetSent = true;
		}

	}
	else if (inFlags & qtssWriteFlagsIsRTCP)
	{
		boost::optional<boost::any> opt = theStreamPtr->getAttribute(sLastRTCPPacketID);
		if (opt && *packetIDPtr <= boost::any_cast<uint64_t>(opt.value()))
		{
			//printf("RTPSessionOutput::WritePacket Don't send RTP packet id =%qu\n", *packetIDPtr);
			packetSent = true;
		}
	}

	return packetSent;
}

bool  RTPSessionOutput::PacketReadyToSend(RTPStream *theStreamPtr, int64_t *currentTimePtr, uint32_t inFlags, uint64_t* packetIDPtr, int64_t* timeToSendThisPacketAgainPtr)
{
	return true;

}

QTSS_Error  RTPSessionOutput::TrackRTCPBaseTime(QTSS_RTPStreamObject *theStreamPtr, StrPtrLen* inPacketStrPtr, int64_t *currentTimePtr, uint32_t inFlags, int64_t* packetLatenessInMSec, int64_t* timeToSendThisPacketAgain, uint64_t* packetIDPtr, int64_t* arrivalTimeMSecPtr)
{
	bool haveBaseTime = false;
	bool haveAllFirstRTPs = true;

	RTPStream *dict = (RTPStream *)*theStreamPtr;
	uint32_t streamTimeScale = dict->GetTimeScale();

	boost::optional<boost::any> opt = dict->getAttribute(sBaseRTPTimeStamp);
	if (!fMustSynch || !opt) // we need a starting stream time that is synched 
	{
		haveBaseTime = true;
	}
	else
	{
		uint64_t earliestArrivalTime = ~(uint64_t)0; //max value

		opt = dict->getAttribute(sBaseRTPTimeStamp);
		if (fMustSynch || !opt)
		{   // we don't have a base arrival time for the session see if we can set one now.

			for (auto findStream : fClientSession->GetStreams())
			{
				opt = findStream->getAttribute(sFirstRTPArrivalTime);

				if (!opt)
				{
					// no packet on this stream yet 
					haveAllFirstRTPs = false; // not enough info to calc a base time
					break;
				}
				else
				{
					int64_t firstArrivalTimePtr = boost::any_cast<int64_t>(opt.value());
					// we have an arrival time see if it is the first for all streams
					if ((uint64_t)firstArrivalTimePtr < earliestArrivalTime)
					{
						earliestArrivalTime = firstArrivalTimePtr;
					}
				}

			}

			if (haveAllFirstRTPs) // we can now create a base arrival time and base stream time from that
			{
				dict->addAttribute(sBaseArrivalTimeStamp, earliestArrivalTime);
				fBaseArrivalTime = (int64_t)earliestArrivalTime;
			}
		}

		if (haveAllFirstRTPs)//sBaseRTPTimeStamp
		{   
			// we don't have a base stream time but we have a base session time so calculate the base stream time.
			RTPStream *dict = (RTPStream *)*theStreamPtr;
			boost::optional<boost::any> opt;
			opt = dict->getAttribute(sFirstRTPTimeStamp);
			if (!opt) return QTSS_NoErr;
			uint32_t firstStreamTime = boost::any_cast<uint32_t>(opt.value());

			opt = dict->getAttribute(sFirstRTPArrivalTime);
			if (!opt) return QTSS_NoErr;
			int64_t firstStreamArrivalTime = boost::any_cast<int64_t>(opt.value());

			int64_t arrivalTimeDiffMSecs = (firstStreamArrivalTime - fBaseArrivalTime);// + fBufferDelayMSecs;//add the buffer delay !! not sure about faster than real time arrival times....
			auto timeDiffStreamTime = (uint32_t)(((double)arrivalTimeDiffMSecs / (double) 1000.0) * (double)streamTimeScale);
			uint32_t baseTimeStamp = firstStreamTime - timeDiffStreamTime;
			dict->addAttribute(sBaseRTPTimeStamp, baseTimeStamp);
			haveBaseTime = true;

			((RTPStream *)*theStreamPtr)->SetTimeStamp(baseTimeStamp);

			fMustSynch = false;
			//printf("fBaseArrivalTime =%qd baseTimeStamp %"   _U32BITARG_   " streamStartTime=%qd diff =%qd\n", fBaseArrivalTime, baseTimeStamp, firstStreamArrivalTime, arrivalTimeDiffMSecs);
		}
	}

	return QTSS_NoErr;

}

QTSS_Error  RTPSessionOutput::RewriteRTCP(QTSS_RTPStreamObject *theStreamPtr, StrPtrLen* inPacketStrPtr, int64_t *currentTimePtr, uint32_t inFlags, int64_t* packetLatenessInMSec, int64_t* timeToSendThisPacketAgain, uint64_t* packetIDPtr, int64_t* arrivalTimeMSecPtr)
{
	uint32_t  theLen;

	boost::optional<boost::any> opt;

	RTPStream *dict = (RTPStream *)*theStreamPtr;
	opt = dict->getAttribute(sFirstRTPCurrentTime);
	int64_t firstRTPCurrentTime = boost::any_cast<int64_t>(opt.value());

	opt = dict->getAttribute(sFirstRTPArrivalTime);
	int64_t firstRTPArrivalTime = boost::any_cast<int64_t>(opt.value());

	opt = dict->getAttribute(sFirstRTPTimeStamp);
	uint32_t rtpTime = boost::any_cast<uint32_t>(opt.value());


	auto* theReport = (uint32_t*)inPacketStrPtr->Ptr;
	theReport += 2; // point to the NTP time stamp
	auto* theNTPTimestampP = (int64_t*)theReport;
	*theNTPTimestampP = OS::HostToNetworkSInt64(OS::TimeMilli_To_1900Fixed64Secs(*currentTimePtr)); // time now

	opt = dict->getAttribute(sBaseRTPTimeStamp);
	uint32_t baseTimeStamp = boost::any_cast<uint32_t>(opt.value());

	uint32_t streamTimeScale = dict->GetTimeScale();

	int64_t packetOffset = *currentTimePtr - fBaseArrivalTime; // real time that has passed
	packetOffset -= (firstRTPCurrentTime - firstRTPArrivalTime); // less the initial buffer delay for this stream
	if (packetOffset < 0)
		packetOffset = 0;

	double rtpTimeFromStart = (double)packetOffset / (double) 1000.0;
	auto rtpTimeFromStartInScale = (uint32_t)(double)((double)streamTimeScale * rtpTimeFromStart);
	//printf("rtptime offset time =%f in scale =%"   _U32BITARG_   "\n", rtpTimeFromStart, rtpTimeFromStartInScale );

	theReport += 2; // point to the rtp time stamp of "now" synched and scaled in stream time
	*theReport = htonl(baseTimeStamp + rtpTimeFromStartInScale);

	theReport += 1; // point to the rtp packets sent
	*theReport = htonl(ntohl(*theReport) * 2);

	theReport += 1; // point to the rtp payload bytes sent
	*theReport = htonl(ntohl(*theReport) * 2);

	return QTSS_NoErr;
}

QTSS_Error  RTPSessionOutput::TrackRTCPPackets(QTSS_RTPStreamObject *theStreamPtr, StrPtrLen* inPacketStrPtr, int64_t *currentTimePtr, uint32_t inFlags, int64_t* packetLatenessInMSec, int64_t* timeToSendThisPacketAgain, uint64_t* packetIDPtr, int64_t* arrivalTimeMSecPtr)
{
	QTSS_Error writeErr = QTSS_NoErr;

	Assert(inFlags & qtssWriteFlagsIsRTCP);

	if (!(inFlags & qtssWriteFlagsIsRTCP))
		return -1;

	this->TrackRTCPBaseTime(theStreamPtr, inPacketStrPtr, currentTimePtr, inFlags, packetLatenessInMSec, timeToSendThisPacketAgain, packetIDPtr, arrivalTimeMSecPtr);

	this->RewriteRTCP(theStreamPtr, inPacketStrPtr, currentTimePtr, inFlags, packetLatenessInMSec, timeToSendThisPacketAgain, packetIDPtr, arrivalTimeMSecPtr);


	return writeErr;
}

QTSS_Error  RTPSessionOutput::TrackRTPPackets(QTSS_RTPStreamObject *theStreamPtr, StrPtrLen* inPacketStrPtr, int64_t *currentTimePtr, uint32_t inFlags, int64_t* packetLatenessInMSec, int64_t* timeToSendThisPacketAgain, uint64_t* packetIDPtr, int64_t* arrivalTimeMSecPtr)
{
	QTSS_Error writeErr = QTSS_NoErr;

	Assert(inFlags & qtssWriteFlagsIsRTP);

	if (!(inFlags & qtssWriteFlagsIsRTP))
		return QTSS_NoErr;

	ReflectorPacket packetContainer;
	packetContainer.SetPacketData(inPacketStrPtr->Ptr, inPacketStrPtr->Len);
	packetContainer.fIsRTCP = false;
	int64_t *theTimePtr = nullptr;
	uint32_t theLen = 0;

	RTPStream *dict = (RTPStream *)*theStreamPtr;
	boost::optional<boost::any> opt = dict->getAttribute(sFirstRTPArrivalTime);
	if (!opt)
	{
		uint32_t theSSRC = packetContainer.GetSSRC(packetContainer.fIsRTCP);
		dict->addAttribute(sStreamSSRC, theSSRC);

		uint32_t rtpTime = packetContainer.GetPacketRTPTime();
		dict->addAttribute(sFirstRTPTimeStamp, rtpTime);
		dict->addAttribute(sFirstRTPArrivalTime, *arrivalTimeMSecPtr);
		dict->addAttribute(sFirstRTPCurrentTime, (int64_t)0);

		dict->addAttribute(sStreamByteCount, (uint32_t)0);

		//printf("first rtp on stream stream=%"   _U32BITARG_   " ssrc=%"   _U32BITARG_   " rtpTime=%"   _U32BITARG_   " arrivalTimeMSecPtr=%qd currentTime=%qd\n",(uint32_t) theStreamPtr, theSSRC, rtpTime, *arrivalTimeMSecPtr, *currentTimePtr);

	}
	else
	{
		opt = dict->getAttribute(sStreamByteCount);
		if (!opt) {
			uint32_t byteCount = boost::any_cast<uint32_t>(opt.value());
			dict->addAttribute(sStreamByteCount, byteCount + inPacketStrPtr->Len - 12); // 12 header bytes
		}


		opt = dict->getAttribute(sStreamSSRC);
		uint32_t theSSRCPtr = boost::any_cast<uint32_t>(opt.value());
		if (theSSRCPtr != packetContainer.GetSSRC(packetContainer.fIsRTCP))
		{


			dict->removeAttribute(sFirstRTPArrivalTime);
			dict->removeAttribute(sFirstRTPTimeStamp);
			dict->removeAttribute(sFirstRTPCurrentTime);
			dict->removeAttribute(sStreamPacketCount);
			dict->removeAttribute(sStreamByteCount);
			fMustSynch = true;

			//printf("found different ssrc =%"   _U32BITARG_   " packetssrc=%"   _U32BITARG_   "\n",*theSSRCPtr, packetContainer.GetSSRC(packetContainer.fIsRTCP));

		}



	}

	return writeErr;

}

QTSS_Error  RTPSessionOutput::TrackPackets(QTSS_RTPStreamObject *theStreamPtr, StrPtrLen* inPacketStrPtr, int64_t *currentTimePtr, uint32_t inFlags, int64_t* packetLatenessInMSec, int64_t* timeToSendThisPacketAgain, uint64_t* packetIDPtr, int64_t* arrivalTimeMSecPtr)
{
	if (this->IsUDP())
		return QTSS_NoErr;

	if (inFlags & qtssWriteFlagsIsRTCP)
		(void) this->TrackRTCPPackets(theStreamPtr, inPacketStrPtr, currentTimePtr, inFlags, packetLatenessInMSec, timeToSendThisPacketAgain, packetIDPtr, arrivalTimeMSecPtr);
	else if (inFlags & qtssWriteFlagsIsRTP)
		(void) this->TrackRTPPackets(theStreamPtr, inPacketStrPtr, currentTimePtr, inFlags, packetLatenessInMSec, timeToSendThisPacketAgain, packetIDPtr, arrivalTimeMSecPtr);

	return QTSS_NoErr;
}


QTSS_Error  RTPSessionOutput::WritePacket(const std::vector<char> &inPacket, void* inStreamCookie, uint32_t inFlags, int64_t packetLatenessInMSec, int64_t* timeToSendThisPacketAgain, uint64_t* packetIDPtr, int64_t* arrivalTimeMSecPtr, bool firstPacket)
{
	uint32_t                  theLen = 0;
	QTSS_Error              writeErr = QTSS_NoErr;
	int64_t                  currentTime = OS::Milliseconds();

	if (inPacket.empty())
		return QTSS_NoErr;

	if (fClientSession->GetSessionState() != qtssPlayingState)
		return QTSS_WouldBlock;

	//make sure all RTP streams with this ID see this packet
	QTSS_RTPStreamObject *theStreamPtr = nullptr;

	for (auto theStreamPtr : fClientSession->GetStreams())
	{
		if (this->PacketMatchesStream(inStreamCookie, theStreamPtr))
		{
			if ((inFlags & qtssWriteFlagsIsRTP) && FilterPacket(theStreamPtr, inPacket))
				return  QTSS_NoErr; // keep looking at packets

			if (PacketAlreadySent(theStreamPtr, inFlags, packetIDPtr))
				return QTSS_NoErr; // keep looking at packets

			if (!PacketReadyToSend(theStreamPtr, &currentTime, inFlags, packetIDPtr, timeToSendThisPacketAgain))
			{   //printf("QTSS_WouldBlock\n");
				return QTSS_WouldBlock; // stop not ready to send packets now
			}


			// TrackPackets below is for re-writing the rtcps we don't use it right now-- shouldn't need to    
			// (void) this->TrackPackets(theStreamPtr, inPacket, &currentTime,inFlags,  &packetLatenessInMSec, timeToSendThisPacketAgain, packetIDPtr,arrivalTimeMSecPtr);

			QTSS_PacketStruct thePacket;
			thePacket.packetData = (void *)&inPacket[0];
			int64_t delayMSecs = fBufferDelayMSecs - (currentTime - *arrivalTimeMSecPtr);
			thePacket.packetTransmitTime = (currentTime - packetLatenessInMSec);
			if (fBufferDelayMSecs > 0)
				thePacket.packetTransmitTime += delayMSecs; // add buffer time where oldest buffered packet as now == 0 and newest is entire buffer time in the future.

			writeErr = theStreamPtr->Write(&thePacket, inPacket.size(), nullptr, inFlags | qtssWriteFlagsWriteBurstBegin);
			if (writeErr == QTSS_WouldBlock)
			{
				//printf("QTSS_Write == QTSS_WouldBlock\n");
			   //
			   // We are flow controlled. See if we know when flow control will be lifted and report that
				*timeToSendThisPacketAgain = thePacket.suggestedWakeupTime;

				if (firstPacket)
				{
					fBufferDelayMSecs = (currentTime - *arrivalTimeMSecPtr);
					//printf("firstPacket fBufferDelayMSecs =%lu \n", fBufferDelayMSecs);
				}
			}
			else
			{
				fLastIntervalMilliSec = currentTime - fLastPacketTransmitTime;
				if (fLastIntervalMilliSec > 100) //reset interval maybe first packet or it has been blocked for awhile
					fLastIntervalMilliSec = 5;
				fLastPacketTransmitTime = currentTime;

				if (inFlags & qtssWriteFlagsIsRTP)
				{
					theStreamPtr->addAttribute(sLastRTPPacketID, *packetIDPtr);
				}
				else if (inFlags & qtssWriteFlagsIsRTCP)
				{
					theStreamPtr->addAttribute(sLastRTCPPacketID, *packetIDPtr);
					theStreamPtr->addAttribute(sLastRTCPTransmit, currentTime);
				}
			}
		}

		if (writeErr != QTSS_NoErr)
			break;
	}

	return writeErr;
}

uint16_t RTPSessionOutput::GetPacketSeqNumber(const std::vector<char> &inPacket)
{
	if (inPacket.size() < 4)
		return 0;

	//The RTP seq number is the second short of the packet
	auto* seqNumPtr = (uint16_t*)&inPacket[0];
	return ntohs(seqNumPtr[1]);
}

void RTPSessionOutput::SetPacketSeqNumber(const std::vector<char> &inPacket, uint16_t inSeqNumber)
{
	if (inPacket.size() < 4)
		return;

	//The RTP seq number is the second short of the packet
	auto* seqNumPtr = (uint16_t*)&inPacket[0];
	seqNumPtr[1] = htons(inSeqNumber);
}

// this routine is not used
bool RTPSessionOutput::PacketShouldBeThinned(QTSS_RTPStreamObject inStream, const std::vector<char> &inPacket)
{
	return false; // function is disabled.

	static uint16_t sZero = 0;
	boost::optional<uint16_t> nextSeqNum;
	boost::optional<uint16_t> theSeqNumOffset;
	boost::optional<int64_t> lastChangeTime;
	//This function determines whether the packet should be dropped.
	//It also adjusts the sequence number if necessary

	if (inPacket.size() < 4)
		return false;

	uint16_t curSeqNum = GetPacketSeqNumber(inPacket);
	
	uint32_t theLen = 0;
	RTPStream *dict = (RTPStream *)inStream;
	uint32_t curQualityLevel = dict->GetQualityLevel();

	boost::optional<boost::any> opt = dict->getAttribute(sNextSeqNum);
	if (!opt) {
		nextSeqNum = sZero;
		dict->addAttribute(sNextSeqNum, sZero);
	}
	opt = dict->getAttribute(sSeqNumOffset);
	if (!opt) {
		theSeqNumOffset = sZero;
		dict->addAttribute(sSeqNumOffset, sZero);
	}

	uint16_t newSeqNumOffset = theSeqNumOffset.value();

	opt = dict->getAttribute(sLastQualityChange);
	if (!opt) {
		static int64_t startTime = 0;
		lastChangeTime = startTime;
		dict->addAttribute(sLastQualityChange, startTime);
	}

	int64_t timeNow = OS::Milliseconds();
	if (*lastChangeTime == 0 || curQualityLevel == 0)
		lastChangeTime = timeNow;

	if (curQualityLevel > 0 && ((*lastChangeTime + 30000) < timeNow)) // 30 seconds between reductions
	{
		curQualityLevel -= 1; // reduce quality value.  If we quality doesn't change then we may have hit some steady state which we can't get out of without thinning or increasing the quality
		*lastChangeTime = timeNow;
		//printf("RTPSessionOutput set quality to %"   _U32BITARG_   "\n",*curQualityLevel);
	}

	//Check to see if we need to drop to audio only
	if ((curQualityLevel >= ReflectorSession::kAudioOnlyQuality) &&
		(*nextSeqNum == 0))
	{
#if REFLECTOR_THINNING_DEBUGGING || RTP_SESSION_DEBUGGING
		printf(" *** Reflector Dropping to audio only *** \n");
#endif
		//All we need to do in this case is mark the sequence number of the first dropped packet
		dict->addAttribute(sNextSeqNum, curSeqNum);
		*lastChangeTime = timeNow;
	}


	//Check to see if we can reinstate video
	if ((curQualityLevel == ReflectorSession::kNormalQuality) && (*nextSeqNum != 0))
	{
		//Compute the offset amount for each subsequent sequence number. This offset will
		//alter the sequence numbers so that they increment normally (providing the illusion to the
		//client that there are no missing packets)
		newSeqNumOffset = (*theSeqNumOffset) + (curSeqNum - (*nextSeqNum));
		dict->addAttribute(sSeqNumOffset, newSeqNumOffset);
		dict->addAttribute(sNextSeqNum, sZero);
	}

	//tell the caller whether to drop this packet or not.
	if (curQualityLevel >= ReflectorSession::kAudioOnlyQuality)
		return true;
	else
	{
		//Adjust the sequence number of the current packet based on the offset, if any
		curSeqNum -= newSeqNumOffset;
		SetPacketSeqNumber(inPacket, curSeqNum);
		return false;
	}
}

void RTPSessionOutput::TearDown()
{
	fClientSession->SetTeardownReason(qtssCliSesTearDownBroadcastEnded);
	fClientSession->Teardown();
}