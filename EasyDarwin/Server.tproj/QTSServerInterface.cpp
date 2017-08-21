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

#ifndef kVersionString
#include "revision.h"
#endif
#include <boost/asio/io_service.hpp>

#include "QTSServerInterface.h"

#include "RTPSessionInterface.h"
#include "OSRef.h"
#include "UDPSocketPool.h"
#include "RTSPProtocol.h"
#include "RTPPacketResender.h"
#include "revision.h"
#include "EasyUtil.h"

// STATIC DATA

uint32_t                  QTSServerInterface::sServerAPIVersion = QTSS_API_VERSION;
QTSServerInterface*     QTSServerInterface::sServer = nullptr;
#if __MacOSX__
StrPtrLen               QTSServerInterface::sServerNameStr("EasyDarwin");
#else
StrPtrLen               QTSServerInterface::sServerNameStr("EasyDarwin");
#endif

// kVersionString from revision.h, include with -i at project level
StrPtrLen               QTSServerInterface::sServerVersionStr(kVersionString);
StrPtrLen               QTSServerInterface::sServerBuildStr(kBuildString);
StrPtrLen               QTSServerInterface::sServerCommentStr(kCommentString);

StrPtrLen               QTSServerInterface::sServerPlatformStr(kPlatformNameString);
StrPtrLen               QTSServerInterface::sServerBuildDateStr(__DATE__ ", " __TIME__);
std::string             QTSServerInterface::sServerHeader;

std::string             QTSServerInterface::sPublicHeaderStr;

std::array<std::vector<QTSSModule*>, QTSSModule::kNumRoles> QTSServerInterface::sModuleArray{};
std::list<QTSSModule*>               QTSServerInterface::sModuleQueue;
QTSSErrorLogStream      QTSServerInterface::sErrorLogStream;


QTSSAttrInfoDict::AttrInfo  QTSServerInterface::sConnectedUserAttributes[] =
{   /*fields:   fAttrName, fFuncPtr, fAttrDataType, fAttrPermission */
	/* 0  */ { "qtssConnectionType",                    nullptr,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
	/* 1  */ { "qtssConnectionCreateTimeInMsec",        nullptr,   qtssAttrDataTypeTimeVal,        qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
	/* 2  */ { "qtssConnectionTimeConnectedInMsec",     TimeConnected,  qtssAttrDataTypeTimeVal,        qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 3  */ { "qtssConnectionBytesSent",               nullptr,   qtssAttrDataTypeUInt32,         qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
	/* 4  */ { "qtssConnectionMountPoint",              nullptr,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
	/* 5  */ { "qtssConnectionHostName",                nullptr,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe } ,

	/* 6  */ { "qtssConnectionSessRemoteAddrStr",       nullptr,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
	/* 7  */ { "qtssConnectionSessLocalAddrStr",        nullptr,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },

	/* 8  */ { "qtssConnectionCurrentBitRate",          nullptr,   qtssAttrDataTypeUInt32,         qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
	/* 9  */ { "qtssConnectionPacketLossPercent",       nullptr,   qtssAttrDataTypeFloat32,        qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
	// this last parameter is a workaround for the current dictionary implementation.  For qtssConnectionTimeConnectedInMsec above we have a param
	// retrieval function.  This needs storage to keep the value returned, but if it sets its own param then the function no longer gets called.
	/* 10 */ { "qtssConnectionTimeStorage",             nullptr,   qtssAttrDataTypeTimeVal,        qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
};


QTSSAttrInfoDict::AttrInfo  QTSServerInterface::sAttributes[] =
{   /*fields:   fAttrName, fFuncPtr, fAttrDataType, fAttrPermission */
	/* 0  */ { "qtssServerAPIVersion",          nullptr,   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 1  */ { "qtssSvrDefaultDNSName",         nullptr,   qtssAttrDataTypeCharArray,  qtssAttrModeRead },
	/* 2  */ { "qtssSvrDefaultIPAddr",          nullptr,   qtssAttrDataTypeUInt32,     qtssAttrModeRead },
	/* 3  */ { "qtssSvrServerName",             nullptr,   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 4  */ { "qtssRTSPSvrServerVersion",      nullptr,   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 5  */ { "qtssRTSPSvrServerBuildDate",    nullptr,   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 6  */ { "qtssSvrRTSPPorts",              nullptr,   qtssAttrDataTypeUInt16,     qtssAttrModeRead },
	/* 7  */ { "qtssSvrRTSPServerHeader",       nullptr,   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 8  */ { "qtssSvrState",              nullptr,   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite  },
	/* 9  */ { "qtssSvrIsOutOfDescriptors",     IsOutOfDescriptors,     qtssAttrDataTypeBool16, qtssAttrModeRead },
	/* 10 */ { "qtssRTSPCurrentSessionCount",   nullptr,   qtssAttrDataTypeUInt32,     qtssAttrModeRead },
	/* 11 */ { "qtssRTSPHTTPCurrentSessionCount",nullptr,  qtssAttrDataTypeUInt32,     qtssAttrModeRead },
	/* 12 */ { "qtssRTPSvrNumUDPSockets",       GetTotalUDPSockets,     qtssAttrDataTypeUInt32, qtssAttrModeRead },
	/* 13 */ { "qtssRTPSvrCurConn",             nullptr,   qtssAttrDataTypeUInt32,     qtssAttrModeRead },
	/* 14 */ { "qtssRTPSvrTotalConn",           nullptr,   qtssAttrDataTypeUInt32,     qtssAttrModeRead },
	/* 15 */ { "qtssRTPSvrCurBandwidth",        nullptr,   qtssAttrDataTypeUInt32,     qtssAttrModeRead },
	/* 16 */ { "qtssRTPSvrTotalBytes",          nullptr,   qtssAttrDataTypeuint64_t,     qtssAttrModeRead },
	/* 17 */ { "qtssRTPSvrAvgBandwidth",        nullptr,   qtssAttrDataTypeUInt32,     qtssAttrModeRead },
	/* 18 */ { "qtssRTPSvrCurPackets",          nullptr,   qtssAttrDataTypeUInt32,     qtssAttrModeRead },
	/* 19 */ { "qtssRTPSvrTotalPackets",        nullptr,   qtssAttrDataTypeuint64_t,     qtssAttrModeRead },
	/* 20 */ { "qtssSvrHandledMethods",         nullptr,   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe  },
	/* 21 */ { "qtssSvrModuleObjects",          nullptr,   qtssAttrDataTypeQTSS_Object,qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 22 */ { "qtssSvrStartupTime",            nullptr,   qtssAttrDataTypeTimeVal,    qtssAttrModeRead },
	/* 23 */ { "qtssSvrGMTOffsetInHrs",         nullptr,   qtssAttrDataTypeint32_t,     qtssAttrModeRead },
	/* 24 */ { "qtssSvrDefaultIPAddrStr",       nullptr,   qtssAttrDataTypeCharArray,  qtssAttrModeRead },
	/* 25 */ { "qtssSvrPreferences",            nullptr,   qtssAttrDataTypeQTSS_Object,qtssAttrModeRead | qtssAttrModeInstanceAttrAllowed},
	/* 26 */ { "qtssSvrMessages",               nullptr,   qtssAttrDataTypeQTSS_Object,qtssAttrModeRead },
	/* 27 */ { "qtssSvrClientSessions",         nullptr,   qtssAttrDataTypeQTSS_Object,qtssAttrModeRead },
	/* 28 */ { "qtssSvrCurrentTimeMilliseconds",CurrentUnixTimeMilli,   qtssAttrDataTypeTimeVal,qtssAttrModeRead},
	/* 29 */ { "qtssSvrCPULoadPercent",         nullptr,   qtssAttrDataTypeFloat32,    qtssAttrModeRead},
	/* 30 */ {},
	/* 31 */ { "qtssSvrReliableUDPWastageInBytes",GetNumWastedBytes, qtssAttrDataTypeUInt32,        qtssAttrModeRead },
	/* 32 */ { "qtssSvrConnectedUsers",         nullptr, qtssAttrDataTypeQTSS_Object,      qtssAttrModeRead | qtssAttrModeWrite },
	/* 33  */ { "qtssSvrServerBuild",           nullptr,   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 34  */ { "qtssSvrServerPlatform",        nullptr,   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 35  */ { "qtssSvrRTSPServerComment",     nullptr,   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 36  */ { "qtssSvrNumThinned",            nullptr,   qtssAttrDataTypeint32_t,     qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 37  */ { "qtssSvrNumThreads",            nullptr,   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe }
};

void    QTSServerInterface::Initialize()
{
	for (uint32_t x = 0; x < qtssSvrNumParams; x++)
		QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kServerDictIndex)->
		SetAttribute(x, sAttributes[x].fAttrName, sAttributes[x].fFuncPtr,
			sAttributes[x].fAttrDataType, sAttributes[x].fAttrPermission);

	for (uint32_t y = 0; y < qtssConnectionNumParams; y++)
		QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kQTSSConnectedUserDictIndex)->
		SetAttribute(y, sConnectedUserAttributes[y].fAttrName, sConnectedUserAttributes[y].fFuncPtr,
			sConnectedUserAttributes[y].fAttrDataType, sConnectedUserAttributes[y].fAttrPermission);

	//Write out a premade server header
	sServerHeader = std::string(RTSPProtocol::GetHeaderString(qtssServerHeader))
		          + ": " + std::string(sServerNameStr.Ptr, sServerNameStr.Len)
				  + "/" + std::string(sServerVersionStr.Ptr, sServerVersionStr.Len)
		          + " (Build/" + std::string(sServerBuildStr.Ptr, sServerBuildStr.Len)
		          + "; Platform/" + std::string(sServerPlatformStr.Ptr, sServerPlatformStr.Len)
		          + ";";

	if (sServerCommentStr.Len > 0)
	{
		sServerHeader += " ";
		sServerHeader += std::string(sServerCommentStr.Ptr, sServerCommentStr.Len);
	}
	sServerHeader += ")";
}

QTSServerInterface::QTSServerInterface()
	: QTSSDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kServerDictIndex), &fMutex)
{
	this->SetVal(qtssSvrState, &fServerState, sizeof(fServerState));
	this->SetVal(qtssServerAPIVersion, &sServerAPIVersion, sizeof(sServerAPIVersion));
	this->SetVal(qtssSvrDefaultIPAddr, &fDefaultIPAddr, sizeof(fDefaultIPAddr));
	this->SetVal(qtssSvrServerName, sServerNameStr.Ptr, sServerNameStr.Len);
	this->SetVal(qtssSvrServerVersion, sServerVersionStr.Ptr, sServerVersionStr.Len);
	this->SetVal(qtssSvrServerBuildDate, sServerBuildDateStr.Ptr, sServerBuildDateStr.Len);
	this->SetVal(qtssRTSPCurrentSessionCount, &fNumRTSPSessions, sizeof(fNumRTSPSessions));
	this->SetVal(qtssRTSPHTTPCurrentSessionCount, &fNumRTSPHTTPSessions, sizeof(fNumRTSPHTTPSessions));
	this->SetVal(qtssRTPSvrCurConn, &fNumRTPSessions, sizeof(fNumRTPSessions));
	this->SetVal(qtssRTPSvrTotalConn, &fTotalRTPSessions, sizeof(fTotalRTPSessions));
	this->SetVal(qtssRTPSvrCurBandwidth, &fCurrentRTPBandwidthInBits, sizeof(fCurrentRTPBandwidthInBits));
	this->SetVal(qtssRTPSvrTotalBytes, &fTotalRTPBytes, sizeof(fTotalRTPBytes));
	this->SetVal(qtssRTPSvrAvgBandwidth, &fAvgRTPBandwidthInBits, sizeof(fAvgRTPBandwidthInBits));
	this->SetVal(qtssRTPSvrCurPackets, &fRTPPacketsPerSecond, sizeof(fRTPPacketsPerSecond));
	this->SetVal(qtssRTPSvrTotalPackets, &fTotalRTPPackets, sizeof(fTotalRTPPackets));
	this->SetVal(qtssSvrStartupTime, &fStartupTime_UnixMilli, sizeof(fStartupTime_UnixMilli));
	this->SetVal(qtssSvrGMTOffsetInHrs, &fGMTOffset, sizeof(fGMTOffset));
	this->SetVal(qtssSvrCPULoadPercent, &fCPUPercent, sizeof(fCPUPercent));

	this->SetVal(qtssSvrServerBuild, sServerBuildStr.Ptr, sServerBuildStr.Len);
	this->SetVal(qtssSvrRTSPServerComment, sServerCommentStr.Ptr, sServerCommentStr.Len);
	this->SetVal(qtssSvrServerPlatform, sServerPlatformStr.Ptr, sServerPlatformStr.Len);

	this->SetVal(qtssSvrNumThinned, &fNumThinned, sizeof(fNumThinned));
	this->SetVal(qtssSvrNumThreads, &fNumThreads, sizeof(fNumThreads));

	sprintf(fCloudServiceNodeID, "%s", EasyUtil::GetUUID().c_str());

	sServer = this;
}


void QTSServerInterface::LogError(QTSS_ErrorVerbosity inVerbosity, char* inBuffer)
{
	QTSS_RoleParams theParams;
	theParams.errorParams.inVerbosity = inVerbosity;
	theParams.errorParams.inBuffer = inBuffer;

	for (const auto &theModule : QTSServerInterface::GetModule(QTSSModule::kErrorLogRole))
		theModule->CallDispatch(QTSS_ErrorLog_Role, &theParams);

	// If this is a fatal error, set the proper attribute in the RTSPServer dictionary
	if ((inVerbosity == qtssFatalVerbosity) && (sServer != nullptr))
	{
		QTSS_ServerState theState = qtssFatalErrorState;
		(void)sServer->SetValue(qtssSvrState, 0, &theState, sizeof(theState));
	}
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

void QTSServerInterface::SetValueComplete(uint32_t inAttrIndex, QTSSDictionaryMap* inMap,
	uint32_t inValueIndex, void* inNewValue, uint32_t inNewValueLen)
{
	if (inAttrIndex == qtssSvrState)
	{
		Assert(inNewValueLen == sizeof(QTSS_ServerState));

		//
		// Invoke the server state change role
		QTSS_RoleParams theParams;
		theParams.stateChangeParams.inNewState = *(QTSS_ServerState*)inNewValue;

		static QTSS_ModuleState sStateChangeState = { nullptr, 0, nullptr, false };
		if (OSThread::GetCurrent() == nullptr)
			OSThread::SetMainThreadData(&sStateChangeState);
		else
			OSThread::GetCurrent()->SetThreadData(&sStateChangeState);

		for (const auto &theModule : QTSServerInterface::GetModule(QTSSModule::kStateChangeRole))
			theModule->CallDispatch(QTSS_StateChange_Role, &theParams);

		//
		// Make sure to clear out the thread data
		if (OSThread::GetCurrent() == nullptr)
			OSThread::SetMainThreadData(nullptr);
		else
			OSThread::GetCurrent()->SetThreadData(nullptr);
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

	QTSServerInterface* theServer = QTSServerInterface::sServer;

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
	unsigned int periodicBytes = theServer->fPeriodicRTPBytes;
	(void)atomic_sub(&theServer->fPeriodicRTPBytes, periodicBytes);
	theServer->fTotalRTPBytes += periodicBytes;

	// Same deal for packet totals
	unsigned int periodicPackets = theServer->fPeriodicRTPPackets;
	(void)atomic_sub(&theServer->fPeriodicRTPPackets, periodicPackets);
	theServer->fTotalRTPPackets += periodicPackets;

	// ..and for lost packet totals
	unsigned int periodicPacketsLost = theServer->fPeriodicRTPPacketsLost;
	(void)atomic_sub(&theServer->fPeriodicRTPPacketsLost, periodicPacketsLost);

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
			timer.expires_from_now(std::chrono::seconds(theServer->GetPrefs()->GetTotalBytesUpdateTimeInSecs()));
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
		(theServer->GetPrefs()->GetAvgBandwidthUpdateTimeInSecs() * 1000))))
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
		int32_t maxKBits = theServer->GetPrefs()->GetMaxKBitsBandwidth();
		if ((maxKBits > -1) && (theServer->fAvgRTPBandwidthInBits > ((uint32_t)maxKBits * 1024)))
		{
			//we need to make sure that all of this happens atomically wrt the session map
			OSMutexLocker locker(theServer->GetRTPSessionMap()->GetMutex());
			RTPSessionInterface* theSession = this->GetNewestSession(theServer->fRTPMap);
			if (theSession != nullptr)
				if ((curTime - theSession->GetSessionCreateTime()) <
					theServer->GetPrefs()->GetSafePlayDurationInSecs() * 1000)
					theSession->Signal(Task::kKillEvent);
		}
	}
	else if (fLastBandwidthAvg == 0)
	{
		fLastBandwidthAvg = curTime;
		fLastBytesSent = theServer->fTotalRTPBytes;
	}

	timer.expires_from_now(std::chrono::seconds(theServer->GetPrefs()->GetTotalBytesUpdateTimeInSecs()));
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



void* QTSServerInterface::CurrentUnixTimeMilli(QTSSDictionary* inServer, uint32_t* outLen)
{
	auto* theServer = (QTSServerInterface*)inServer;
	theServer->fCurrentTime_UnixMilli = OS::TimeMilli_To_UnixTimeMilli(OS::Milliseconds());

	// Return the result
	*outLen = sizeof(theServer->fCurrentTime_UnixMilli);
	return &theServer->fCurrentTime_UnixMilli;
}

void* QTSServerInterface::GetTotalUDPSockets(QTSSDictionary* inServer, uint32_t* outLen)
{
	auto* theServer = (QTSServerInterface*)inServer;
	// Multiply by 2 because this is returning the number of socket *pairs*
	theServer->fTotalUDPSockets = theServer->fSocketPool->GetSocketQueue().size() * 2;

	// Return the result
	*outLen = sizeof(theServer->fTotalUDPSockets);
	return &theServer->fTotalUDPSockets;
}

void* QTSServerInterface::IsOutOfDescriptors(QTSSDictionary* inServer, uint32_t* outLen)
{
	auto* theServer = (QTSServerInterface*)inServer;

	theServer->fIsOutOfDescriptors = false;
	for (uint32_t x = 0; x < theServer->fNumListeners; x++)
	{
		if (theServer->fListeners[x]->IsOutOfDescriptors())
		{
			theServer->fIsOutOfDescriptors = true;
			break;
		}
	}
	// Return the result
	*outLen = sizeof(theServer->fIsOutOfDescriptors);
	return &theServer->fIsOutOfDescriptors;
}

void* QTSServerInterface::GetNumWastedBytes(QTSSDictionary* inServer, uint32_t* outLen)
{
	// This param retrieval function must be invoked each time it is called,
	// because whether we are out of descriptors or not is continually changing
	auto* theServer = (QTSServerInterface*)inServer;

	theServer->fUDPWastageInBytes = RTPPacketResender::GetWastedBufferBytes();

	// Return the result
	*outLen = sizeof(theServer->fUDPWastageInBytes);
	return &theServer->fUDPWastageInBytes;
}

void* QTSServerInterface::TimeConnected(QTSSDictionary* inConnection, uint32_t* outLen)
{
	int64_t connectTime;
	void* result;
	uint32_t len = sizeof(connectTime);
	inConnection->GetValue(qtssConnectionCreateTimeInMsec, 0, &connectTime, &len);
	int64_t timeConnected = OS::Milliseconds() - connectTime;
	*outLen = sizeof(timeConnected);
	inConnection->SetValue(qtssConnectionTimeStorage, 0, &timeConnected, sizeof(connectTime));
	inConnection->GetValuePtr(qtssConnectionTimeStorage, 0, &result, outLen);

	// Return the result
	return result;
}


QTSS_Error  QTSSErrorLogStream::Write(void* inBuffer, uint32_t inLen, uint32_t* outLenWritten, uint32_t inFlags)
{
	// For the error log stream, the flags are considered to be the verbosity
	// of the error.
	if (inFlags >= qtssIllegalVerbosity)
		inFlags = qtssMessageVerbosity;

	QTSServerInterface::LogError(inFlags, (char*)inBuffer);
	if (outLenWritten != nullptr)
		*outLenWritten = inLen;

	return QTSS_NoErr;
}

void QTSSErrorLogStream::LogAssert(char* inMessage)
{
	QTSServerInterface::LogError(qtssAssertVerbosity, inMessage);
}
