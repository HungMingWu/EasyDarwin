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
	 File:       RTPBandwidthTracker.h

	 Contains:   Uses Karns Algorithm to measure round trip times. This also
				 tracks the current window size based on input from the caller.

	 ref:

	 Improving Round-Trip Time Estimates in Reliable Transport Protocols, ACM SIGCOMM, ???

	 Congestion Avoidance and Control - Van Jacobson, November 1988 -- Preoceedings of SIGCOMM '88

	 Internetworking with TCP/IP Comer and Stevens, Chap 14, prentice hall 1991
 */

#ifndef __RTP_BANDWIDTH_TRACKER_H__
#define __RTP_BANDWIDTH_TRACKER_H__

#include "OSHeaders.h"
#include <stdint.h>

class RTPBandwidthTracker
{
public:
	RTPBandwidthTracker(bool inUseSlowStart)
		: fRunningAverageMSecs(0),
		fRunningMeanDevationMSecs(0),
		fCurRetransmitTimeout(kMinRetransmitIntervalMSecs),
		fUnadjustedRTO(kMinRetransmitIntervalMSecs),
		fCongestionWindow(kMaximumSegmentSize),
		fSlowStartThreshold(0),
		fSlowStartByteCount(0),
		fClientWindow(0),
		fBytesInList(0),
		fAckTimeout(kMinAckTimeout),
		fUseSlowStart(inUseSlowStart),
		fMaxCongestionWindowSize(0),
		fMinCongestionWindowSize(1000000),
		fMaxRTO(0),
		fMinRTO(24000),
		fTotalCongestionWindowSize(0),
		fTotalRTO(0),
		fNumStatsSamples(0)
	{}

	~RTPBandwidthTracker() = default;

	//
	// Initialization - give the client's window size.
	void    SetWindowSize(int32_t clientWindowSize);

	//
	// Each RTT sample you get, let the tracker know what it is
	// so it can keep a good running average.
	void AddToRTTEstimate(int32_t rttSampleMSecs);

	//
	// Before sending new data, let the tracker know
	// how much data you are sending so it can adjust the window.
	void FillWindow(uint32_t inNumBytes)
	{
		fBytesInList += inNumBytes; fIsRetransmitting = false;
	}

	//
	// When data is acked, let the tracker know how much
	// data was acked so it can adjust the window
	void EmptyWindow(uint32_t inNumBytes, bool updateBytesInList = true);

	//
	// When retransmitting a packet, call this function so
	// the tracker can adjust the window sizes and back off.
	void AdjustWindowForRetransmit();

	//
	// ACCESSORS
	const bool ReadyForAckProcessing() { return (fClientWindow > 0 && fCongestionWindow > 0); } // see RTPBandwidthTracker::EmptyWindow for requirements
	const bool IsFlowControlled() { return ((int32_t)fBytesInList >= fCongestionWindow); }
	const int32_t ClientWindowSize() { return fClientWindow; }
	const uint32_t BytesInList() { return fBytesInList; }
	const int32_t CongestionWindow() { return fCongestionWindow; }
	const int32_t SlowStartThreshold() { return fSlowStartThreshold; }
	const int32_t RunningAverageMSecs() { return fRunningAverageMSecs / 8; }  // fRunningAverageMSecs is stored scaled up 8x
	const int32_t RunningMeanDevationMSecs() { return fRunningMeanDevationMSecs / 4; } // fRunningMeanDevationMSecs is stored scaled up 4x
	const int32_t CurRetransmitTimeout() { return fCurRetransmitTimeout; }
	const int32_t GetCurrentBandwidthInBps()
	{
		return (fUnadjustedRTO > 0) ? (fCongestionWindow * 1000) / fUnadjustedRTO : 0;
	}
	inline const uint32_t RecommendedClientAckTimeout() { return fAckTimeout; }
	void UpdateAckTimeout(uint32_t bitsSentInInterval, int64_t intervalLengthInMsec);
	void UpdateStats();

	//
	// Stats
	int32_t              GetMaxCongestionWindowSize() { return fMaxCongestionWindowSize; }
	int32_t              GetMinCongestionWindowSize() { return fMinCongestionWindowSize; }
	int32_t              GetAvgCongestionWindowSize() { return (int32_t)(fTotalCongestionWindowSize / (int64_t)fNumStatsSamples); }
	int32_t              GetMaxRTO() { return fMaxRTO; }
	int32_t              GetMinRTO() { return fMinRTO; }
	int32_t              GetAvgRTO() { return (int32_t)(fTotalRTO / (int64_t)fNumStatsSamples); }

	enum
	{
		kMaximumSegmentSize = 1466,  // enet - just a guess!

		//
		// Our algorithm for telling the client what the ack timeout
		// is currently not too sophisticated. This could probably be made
		// better. During slow start, we just use 20, and afterwards, just use 100
		kMinAckTimeout = 20,
		kMaxAckTimeout = 100
	};

private:

	//
	// For computing the round-trip estimate using Karn's algorithm
	int32_t  fRunningAverageMSecs;
	int32_t  fRunningMeanDevationMSecs;
	int32_t  fCurRetransmitTimeout;
	int32_t  fUnadjustedRTO;

	//
	// Tracking our window sizes
	int64_t              fLastCongestionAdjust;
	int32_t              fCongestionWindow;      // implentation of VJ congestion avoidance
	int32_t              fSlowStartThreshold;    // point at which we stop adding to the window for each ack, and add to the window for each window full of acks
	int32_t              fSlowStartByteCount;            // counts window a full of acks when past ss thresh
	int32_t              fClientWindow;          // max window size based on client UDP buffer
	uint32_t              fBytesInList;               // how many unacked bytes on this stream
	uint32_t              fAckTimeout;

	bool              fUseSlowStart;
	bool              fIsRetransmitting;      // are we in the re-transmit 'state' ( started resending, but have yet to send 'new' data

	//
	// Stats
	int32_t              fMaxCongestionWindowSize;
	int32_t              fMinCongestionWindowSize;
	int32_t              fMaxRTO;
	int32_t              fMinRTO;
	int64_t              fTotalCongestionWindowSize;
	int64_t              fTotalRTO;
	int32_t              fNumStatsSamples;

	enum
	{
		kMinRetransmitIntervalMSecs = 600,
		kMaxRetransmitIntervalMSecs = 24000
	};
};

#endif // __RTP_BANDWIDTH_TRACKER_H__
