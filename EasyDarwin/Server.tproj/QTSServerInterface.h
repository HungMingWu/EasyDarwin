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
	 File:       QTSServerInterface.h

	 Contains:   This object defines an interface for getting and setting server-wide
				 attributes, and storing global server resources.

				 There can be only one of these objects per process, so there
				 is a static accessor.


 */


#ifndef __QTSSERVERINTERFACE_H__
#define __QTSSERVERINTERFACE_H__

#include <array>
#include <atomic>
#include <list>
#include <vector>
#include <boost/utility/string_view.hpp>
#include <boost/asio/steady_timer.hpp>

#include "QTSS.h"

#include "OSMutex.h"
#include "Task.h"
#include "TCPListenerSocket.h"



 // OSRefTable;
class UDPSocketPool;
class QTSServerPrefs;
class QTSSMessages;
//class RTPStatsUpdaterTask;
class RTPSessionInterface;

class QTSServerInterface
{
protected:
	std::vector<QTSS_RTSPMethod> supportMethods;
public:
	void AppendSupportMehod(const std::vector<QTSS_RTSPMethod>& methods) { 
		std::copy(begin(methods), end(methods), back_inserter(supportMethods)); 
	}
	std::vector<QTSS_RTSPMethod> GetSupportMehod() const { return supportMethods; }

	//
	// CONSTRUCTOR / DESTRUCTOR

	QTSServerInterface();
	virtual ~QTSServerInterface() = default;

	//
	//
	// STATISTICS MANIPULATION
	// These functions are how the server keeps its statistics current

	//total rtp bytes sent by the server
	void            IncrementTotalRTPBytes(size_t bytes)
	{
		fPeriodicRTPBytes += bytes;
	}
	//total rtp packets sent by the server
	void            IncrementTotalPackets()
	{
		//(void)atomic_add(&fPeriodicRTPPackets, 1);
		++fPeriodicRTPPackets;
	}
	//total rtp bytes reported as lost by the clients
	void            IncrementTotalRTPPacketsLost(size_t packets)
	{
		fPeriodicRTPPacketsLost -= packets;
	}

	void            IncrementTotalLate(int64_t milliseconds)
	{
		OSMutexLocker locker(&fMutex);
		fTotalLate += milliseconds;
		if (milliseconds > fCurrentMaxLate) fCurrentMaxLate = milliseconds;
		if (milliseconds > fMaxLate) fMaxLate = milliseconds;
	}

	void            IncrementTotalQuality(int32_t level)
	{
		OSMutexLocker locker(&fMutex); fTotalQuality += level;
	}


	void            IncrementNumThinned(int32_t inDifference)
	{
		OSMutexLocker locker(&fMutex); fNumThinned += inDifference;
	}

	void            ClearTotalLate()
	{
		OSMutexLocker locker(&fMutex); fTotalLate = 0;
	}
	void            ClearCurrentMaxLate()
	{
		OSMutexLocker locker(&fMutex); fCurrentMaxLate = 0;
	}
	void            ClearTotalQuality()
	{
		OSMutexLocker locker(&fMutex); fTotalQuality = 0;
	}


	void 			InitNumThreads(uint32_t numThreads) { fNumThreads = numThreads; }
	//
	// ACCESSORS

	QTSS_ServerState    GetServerState() { return fServerState; }
	void                SetServerState(QTSS_ServerState state) { fServerState = state; }

	uint32_t              GetCurBandwidthInBits() { return fCurrentRTPBandwidthInBits; }
	uint32_t              GetAvgBandwidthInBits() { return fAvgRTPBandwidthInBits; }
	uint32_t              GetRTPPacketsPerSec() { return fRTPPacketsPerSecond; }
	uint64_t              GetTotalRTPBytes() { return fTotalRTPBytes; }
	uint64_t              GetTotalRTPPacketsLost() { return fTotalRTPPacketsLost; }
	uint64_t              GetTotalRTPPackets() { return fTotalRTPPackets; }
	float             GetCPUPercent() { return fCPUPercent; }
	bool              SigIntSet() { return fSigInt; }
	bool				SigTermSet() { return fSigTerm; }

	int64_t				GetMaxLate() { return fMaxLate; };
	int64_t				GetTotalLate() { return fTotalLate; };
	int64_t				GetCurrentMaxLate() { return fCurrentMaxLate; };
	int64_t				GetTotalQuality() { return fTotalQuality; };
	int32_t				GetNumThinned() { return fNumThinned; };
	uint32_t				GetNumThreads() { return fNumThreads; };

	//Allows you to map RTP session IDs (strings) to actual RTP session objects
	OSRefTable*         GetRTPSessionMap() { return fRTPMap; }
	OSRefTable*			GetReflectorSessionMap() { return fReflectorSessionMap; }

	//Server provides a statically created & bound UDPSocket / Demuxer pair
	//for each IP address setup to serve RTP. You access those pairs through
	//this function. This returns a pair pre-bound to the IPAddr specified.
	UDPSocketPool*      GetSocketPool() { return fSocketPool; }

	//
	//
	// SERVER NAME & VERSION

	static boost::string_view GetServerBuildDate() { return sServerBuildDateStr; }

	//
	// PUBLIC HEADER
	static boost::string_view   GetPublicHeader() { return sPublicHeaderStr; }

	//
	// KILL ALL
	void                KillAllRTPSessions();

	//
	// SIGINT - to interrupt the server, set this flag and the server will shut down
	void                SetSigInt() { fSigInt = true; }

	// SIGTERM - to kill the server, set this flag and the server will shut down
	void                SetSigTerm() { fSigTerm = true; }

	//
	// LOCKING DOWN THE SERVER OBJECT
	OSMutex*        GetServerObjectMutex() { return &fMutex; }



protected:

	// Setup by the derived RTSPServer object

	//Sockets are allocated global to the server, and arbitrated through this pool here.
	//RTCP data is processed completely within the following task.
	UDPSocketPool*              fSocketPool{nullptr};

	// All RTP sessions are put into this map
	OSRefTable*                 fRTPMap{nullptr};
	OSRefTable*					fReflectorSessionMap{nullptr};

	QTSS_ServerState            fServerState{qtssStartingUpState};
	uint32_t                      fDefaultIPAddr{0};

	// Array of pointers to TCPListenerSockets.
	TCPListenerSocket**         fListeners{nullptr};
	uint32_t                      fNumListeners{0}; // Number of elements in the array

	int32_t						fGMTOffset{0};

	static std::string                  sPublicHeaderStr;

private:

	enum
	{
		kMaxServerHeaderLen = 1000
	};

	static boost::string_view    sServerBuildDateStr;

	OSMutex             fMutex;

	//stores the total number of bytes served since startup
	uint64_t              fTotalRTPBytes{0};
	//total number of rtp packets sent since startup
	uint64_t              fTotalRTPPackets{0};
	//stores the total number of bytes lost (as reported by clients) since startup
	uint64_t              fTotalRTPPacketsLost{0};

	//because there is no 64 bit atomic add (for obvious reasons), we efficiently
	//implement total byte counting by atomic adding to this variable, then every
	//once in awhile updating the sTotalBytes.
	
	std::atomic_size_t         fPeriodicRTPBytes{0};

	std::atomic_size_t         fPeriodicRTPPacketsLost{0};

	std::atomic_size_t         fPeriodicRTPPackets{0};

	//stores the current served bandwidth in BITS per second
	uint32_t              fCurrentRTPBandwidthInBits{0};
	uint32_t              fAvgRTPBandwidthInBits{0};
	uint32_t              fRTPPacketsPerSecond{0};

	float             fCPUPercent{0};
	float             fCPUTimeUsedInSec{0};

	// Stats for UDP retransmits
	uint32_t              fUDPWastageInBytes{0};
	uint32_t              fNumUDPBuffers{0};

	bool              fSigInt{false};
	bool              fSigTerm{false};

	int64_t          fMaxLate{0};
	int64_t          fTotalLate{0};
	int64_t          fCurrentMaxLate{0};
	int64_t          fTotalQuality{0};
	int32_t          fNumThinned{0};
	uint32_t          fNumThreads{0};

	friend class RTPStatsUpdaterTask;
};


class RTPStatsUpdaterTask
{
public:

	// This class runs periodically to compute current totals & averages
	RTPStatsUpdaterTask();
	~RTPStatsUpdaterTask() = default;

private:
	boost::asio::steady_timer timer;
	void Run(const boost::system::error_code &ec);
	RTPSessionInterface* GetNewestSession(OSRefTable* inRTPSessionMap);
	float GetCPUTimeInSeconds();

	int64_t fLastBandwidthTime{0};
	int64_t fLastBandwidthAvg{0};
	int64_t fLastBytesSent{0};
};
QTSServerInterface* getSingleton();
#endif // __QTSSERVERINTERFACE_H__