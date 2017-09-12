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
	 File:       QTSServerInterface.cpp
	 Contains:   Implementation of object defined in QTSServerInterface.h
 */

 //INCLUDES:

#include <boost/asio/io_service.hpp>

#include "QTSServerInterface.h"

#include "RTPSessionInterface.h"
#include "OSRef.h"
#include "UDPSocketPool.h"
#include "RTSPProtocol.h"
#include "RTPPacketResender.h"
#include "ServerPrefs.h"

// STATIC DATA

static QTSServerInterface* sServer = nullptr;

boost::string_view      QTSServerInterface::sServerBuildDateStr(__DATE__ ", " __TIME__);

std::string             QTSServerInterface::sPublicHeaderStr;

QTSServerInterface::QTSServerInterface()
{
	sServer = this;
}

QTSServerInterface* getSingleton()
{
	return sServer;
}

void QTSServerInterface::KillAllRTPSessions()
{
	OSMutexLocker locker(fRTPMap->GetMutex());
	for (OSRefHashTableIter theIter(fRTPMap->GetHashTable()); !theIter.IsDone(); theIter.Next())
	{
		OSRef* theRef = theIter.GetCurrent();
		auto* theSession = (RTPSessionInterface*)theRef->GetObject();
		theSession->Signal(Task::kKillEvent);
	}
}

extern boost::asio::io_service io_service;
RTPStatsUpdaterTask::RTPStatsUpdaterTask()
	: timer(io_service)
{
	timer.async_wait(std::bind(&RTPStatsUpdaterTask::Run, this, std::placeholders::_1));
}

float RTPStatsUpdaterTask::GetCPUTimeInSeconds()
{
	// This function returns the total number of seconds that the
	// process running RTPStatsUpdaterTask() has been executing as
	// a user process.
	float cpuTimeInSec = 0.0;
#ifdef __Win32__
	// The Win32 way of getting the time for this process
	HANDLE hProcess = GetCurrentProcess();
	int64_t createTime, exitTime, kernelTime, userTime;
	if (GetProcessTimes(hProcess, (LPFILETIME)&createTime, (LPFILETIME)&exitTime, (LPFILETIME)&kernelTime, (LPFILETIME)&userTime))
	{
		// userTime is in 10**-7 seconds since Jan.1, 1607.
		// (What type of computers did they use in 1607?)
		cpuTimeInSec = (float)(userTime / 10000000.0);
	}
	else
	{
		// This should never happen!!!
		Assert(0);
		cpuTimeInSec = 0.0;
	}
#else
	// The UNIX way of getting the time for this process
	clock_t cpuTime = clock();
	cpuTimeInSec = (float)cpuTime / CLOCKS_PER_SEC;
#endif
	return cpuTimeInSec;
}

void RTPStatsUpdaterTask::Run(const boost::system::error_code &ec)
{
	if (ec == boost::asio::error::operation_aborted) {
		printf("RTPStatsUpdaterTask timer canceled\n");
		return;
	}

	QTSServerInterface* theServer = getSingleton();

	// All of this must happen atomically wrt dictionary values we are manipulating
	OSMutexLocker locker(&theServer->fMutex);

	//First update total bytes. This must be done because total bytes is a 64 bit number,
	//so no atomic functions can apply.
	//
	// NOTE: The line below is not thread safe on non-PowerPC platforms. This is
	// because the fPeriodicRTPBytes variable is being manipulated from within an
	// atomic_add. On PowerPC, assignments are atomic, so the assignment below is ok.
	// On a non-PowerPC platform, the following would be thread safe:
	//unsigned int periodicBytes = atomic_add(&theServer->fPeriodicRTPBytes, 0);-----------
	size_t periodicBytes = theServer->fPeriodicRTPBytes;
	theServer->fPeriodicRTPBytes -= periodicBytes;
	theServer->fTotalRTPBytes += periodicBytes;

	// Same deal for packet totals
	size_t periodicPackets = theServer->fPeriodicRTPPackets;
	theServer->fPeriodicRTPPackets -= periodicPackets;
	theServer->fTotalRTPPackets += periodicPackets;

	// ..and for lost packet totals
	size_t periodicPacketsLost = theServer->fPeriodicRTPPacketsLost;
	theServer->fPeriodicRTPPacketsLost -= periodicPacketsLost;

	theServer->fTotalRTPPacketsLost += periodicPacketsLost;

	int64_t curTime = OS::Milliseconds();

	//for cpu percent
	float cpuTimeInSec = GetCPUTimeInSeconds();

	//also update current bandwidth statistic
	if (fLastBandwidthTime != 0)
	{
		Assert(curTime > fLastBandwidthTime);
		auto delta = (uint32_t)(curTime - fLastBandwidthTime);
		// Prevent divide by zero errror
		if (delta < 1000) {
			WarnV(delta >= 1000, "delta < 1000");
			timer.expires_from_now(std::chrono::seconds(ServerPrefs::GetTotalBytesUpdateTimeInSecs()));
			timer.async_wait(std::bind(&RTPStatsUpdaterTask::Run, this, std::placeholders::_1));
			return;
		}

		uint32_t packetsPerSecond = periodicPackets;
		uint32_t theTime = delta / 1000;

		packetsPerSecond /= theTime;
		Assert(packetsPerSecond >= 0);
		theServer->fRTPPacketsPerSecond = packetsPerSecond;
		uint32_t additionalBytes = 28 * packetsPerSecond; // IP headers = 20 + UDP headers = 8
		uint32_t headerBits = 8 * additionalBytes;
		headerBits /= theTime;

		float bits = periodicBytes * 8;
		bits /= theTime;
		theServer->fCurrentRTPBandwidthInBits = (uint32_t)(bits + headerBits);

		//do the computation for cpu percent
		float diffTime = cpuTimeInSec - theServer->fCPUTimeUsedInSec;
		theServer->fCPUPercent = (diffTime / theTime) * 100;

		uint32_t numProcessors = OS::GetNumProcessors();

		if (numProcessors > 1)
			theServer->fCPUPercent /= numProcessors;
	}

	//for cpu percent
	theServer->fCPUTimeUsedInSec = cpuTimeInSec;

	//also compute average bandwidth, a much more smooth value. This is done with
	//the fLastBandwidthAvg, a timestamp of the last time we did an average, and
	//fLastBytesSent, the number of bytes sent when we last did an average.
	if ((fLastBandwidthAvg != 0) && (curTime > (fLastBandwidthAvg +
		(ServerPrefs::GetAvgBandwidthUpdateTimeInSecs() * 1000))))
	{
		auto delta = (uint32_t)(curTime - fLastBandwidthAvg);
		int64_t bytesSent = theServer->fTotalRTPBytes - fLastBytesSent;
		Assert(bytesSent >= 0);

		//do the bandwidth computation using floating point divides
		//for accuracy and speed.
		auto bits = (float)(bytesSent * 8);
		auto theAvgTime = (float)delta;
		theAvgTime /= 1000;
		bits /= theAvgTime;
		Assert(bits >= 0);
		theServer->fAvgRTPBandwidthInBits = (uint32_t)bits;

		fLastBandwidthAvg = curTime;
		fLastBytesSent = theServer->fTotalRTPBytes;

		//if the bandwidth is above the bandwidth setting, disconnect 1 user by sending them
		//a BYE RTCP packet.
		int32_t maxKBits = ServerPrefs::GetMaxKBitsBandwidth();
		if ((maxKBits > -1) && (theServer->fAvgRTPBandwidthInBits > ((uint32_t)maxKBits * 1024)))
		{
			//we need to make sure that all of this happens atomically wrt the session map
			OSMutexLocker locker(theServer->GetRTPSessionMap()->GetMutex());
			RTPSessionInterface* theSession = this->GetNewestSession(theServer->fRTPMap);
			if (theSession != nullptr)
				if ((curTime - theSession->GetSessionCreateTime()) <
					ServerPrefs::GetSafePlayDurationInSecs() * 1000)
					theSession->Signal(Task::kKillEvent);
		}
	}
	else if (fLastBandwidthAvg == 0)
	{
		fLastBandwidthAvg = curTime;
		fLastBytesSent = theServer->fTotalRTPBytes;
	}

	timer.expires_from_now(std::chrono::seconds(ServerPrefs::GetTotalBytesUpdateTimeInSecs()));
	timer.async_wait(std::bind(&RTPStatsUpdaterTask::Run, this, std::placeholders::_1));
}

RTPSessionInterface* RTPStatsUpdaterTask::GetNewestSession(OSRefTable* inRTPSessionMap)
{
	//Caller must lock down the RTP session map
	int64_t theNewestPlayTime = 0;
	RTPSessionInterface* theNewestSession = nullptr;

	//use the session map to iterate through all the sessions, finding the most
	//recently connected client
	for (OSRefHashTableIter theIter(inRTPSessionMap->GetHashTable()); !theIter.IsDone(); theIter.Next())
	{
		OSRef* theRef = theIter.GetCurrent();
		auto* theSession = (RTPSessionInterface*)theRef->GetObject();
		Assert(theSession->GetSessionCreateTime() > 0);
		if (theSession->GetSessionCreateTime() > theNewestPlayTime)
		{
			theNewestPlayTime = theSession->GetSessionCreateTime();
			theNewestSession = theSession;
		}
	}
	return theNewestSession;
}