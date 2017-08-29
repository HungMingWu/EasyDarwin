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
	Contains:   Object store for RTSP server preferences.



*/

#ifndef __QTSSERVERPREFS_H__
#define __QTSSERVERPREFS_H__

#include "StrPtrLen.h"
#include "QTSSPrefs.h"
#include "XMLPrefsParser.h"

class QTSServerPrefs : public QTSSPrefs
{
public:

	// INITIALIZE
	//
	// This function sets up the dictionary map. Must be called before instantiating
	// the first RTSPPrefs object.

	static void Initialize();

	QTSServerPrefs(XMLPrefsParser* inPrefsSource, bool inWriteMissingPrefs);
	~QTSServerPrefs() override = default;

	//This is callable at any time, and is thread safe wrt to the accessors.
	//Pass in true if you want this function to update the prefs file if
	//any defaults need to be used. False otherwise
	void RereadServerPreferences(bool inWriteMissingPrefs);

	//Individual accessor methods for preferences.

	//Amount of idle time after which respective protocol sessions are timed out
	//(stored in seconds)

	//This is the value we advertise to clients (lower than the real one)
	uint32_t  GetRTSPTimeoutInSecs() { return fRTSPTimeoutInSecs; }
	uint32_t  GetRTPSessionTimeoutInSecs() { return fRTPSessionTimeoutInSecs; }
	boost::string_view  GetRTSPTimeoutAsString() { return fRTSPTimeoutString; }

	//This is the real timeout
	uint32_t  GetRTSPSessionTimeoutInSecs() { return fRTSPSessionTimeoutInSecs; }

	//-1 means unlimited
	int32_t  GetMaxConnections() { return fMaximumConnections; }
	int32_t  GetMaxKBitsBandwidth() { return fMaxBandwidthInKBits; }

	// Thinning algorithm parameters
	int32_t  GetDropAllPacketsTimeInMsec() { return fDropAllPacketsTimeInMsec; }
	int32_t  GetDropAllVideoPacketsTimeInMsec() { return fDropAllVideoPacketsTimeInMsec; }
	int32_t  GetThinAllTheWayTimeInMsec() { return fThinAllTheWayTimeInMsec; }
	int32_t  GetAlwaysThinTimeInMsec() { return fAlwaysThinTimeInMsec; }
	int32_t  GetStartThinningTimeInMsec() { return fStartThinningTimeInMsec; }
	int32_t  GetStartThickingTimeInMsec() { return fStartThickingTimeInMsec; }
	int32_t  GetThickAllTheWayTimeInMsec() { return fThickAllTheWayTimeInMsec; }
	uint32_t  GetQualityCheckIntervalInMsec() { return fQualityCheckIntervalInMsec; }

	// for tcp buffer size scaling
	uint32_t  GetMinTCPBufferSizeInBytes() { return fMinTCPBufferSizeInBytes; }
	uint32_t  GetMaxTCPBufferSizeInBytes() { return fMaxTCPBufferSizeInBytes; }
	float GetTCPSecondsToBuffer() { return fTCPSecondsToBuffer; }

	//for debugging, mainly
	bool      ShouldServerBreakOnAssert() { return fBreakOnAssert; }
	bool      IsAutoRestartEnabled() { return fAutoRestart; }

	uint32_t      GetTotalBytesUpdateTimeInSecs() { return fTBUpdateTimeInSecs; }
	uint32_t      GetAvgBandwidthUpdateTimeInSecs() { return fABUpdateTimeInSecs; }
	uint32_t      GetSafePlayDurationInSecs() { return fSafePlayDurationInSecs; }

	// For the compiled-in error logging module

	bool  IsErrorLogEnabled() { return fErrorLogEnabled; }
	bool  IsScreenLoggingEnabled() { return fScreenLoggingEnabled; }

	uint32_t  GetMaxErrorLogBytes() { return fErrorLogBytes; }
	uint32_t  GetErrorRollIntervalInDays() { return fErrorRollIntervalInDays; }
	bool  GetAppendSrcAddrInTransport() { return fAppendSrcAddrInTransport; }

	//
	// For UDP retransmits
	uint32_t  IsReliableUDPEnabled() { return fReliableUDP; }
	uint32_t  GetMaxRetransmitDelayInMsec() { return fMaxRetransDelayInMsec; }
	bool  IsAckLoggingEnabled() { return fIsAckLoggingEnabled; }
	uint32_t  GetRTCPPollIntervalInMsec() { return fRTCPPollIntervalInMsec; }
	uint32_t  GetRTCPSocketRcvBufSizeinK() { return fRTCPSocketRcvBufSizeInK; }
	uint32_t  GetSendIntervalInMsec() { return fSendIntervalInMsec; }
	uint32_t  GetMaxSendAheadTimeInSecs() { return fMaxSendAheadTimeInSecs; }
	bool  IsSlowStartEnabled() { return fIsSlowStartEnabled; }
	bool  GetReliableUDPPrintfsEnabled() { return fReliableUDPPrintfs; }
	bool  GetRTSPDebugPrintfs() { return fEnableRTSPDebugPrintfs; }
	bool  GetRTSPServerInfoEnabled() { return fEnableRTSPServerInfo; }

	float    GetOverbufferRate() { return fOverbufferRate; }

	// RUDP window size
	uint32_t  GetSmallWindowSizeInK() { return fSmallWindowSizeInK; }
	uint32_t    GetMediumWindowSizeInK() { return fMediumWindowSizeInK; }
	uint32_t  GetLargeWindowSizeInK() { return fLargeWindowSizeInK; }
	uint32_t  GetWindowSizeThreshold() { return fWindowSizeThreshold; }
	uint32_t    GetWindowSizeMaxThreshold() { return fWindowSizeMaxThreshold; }

	//
	// force logs to close after each write (true or false)
	bool  GetCloseLogsOnWrite() { return fCloseLogsOnWrite; }
	void    SetCloseLogsOnWrite(bool closeLogsOnWrite);

	//
	// Optionally require that reliable UDP content be in certain folders
	bool IsPathInsideReliableUDPDir(StrPtrLen* inPath);

	// Movie folder pref. If the path fits inside the buffer provided,
	// the path is copied into that buffer. Otherwise, a new buffer is allocated
	// and returned.
	//char*   GetMovieFolder(char* inBuffer, uint32_t* ioLen);

	//
	// Transport addr pref. Caller must provide a buffer big enough for an IP addr
	std::string    GetTransportSrcAddr();

	// String preferences. Note that the pointers returned here is allocated
	// memory that you must delete!

	char*   GetErrorLogDir()
	{
		return this->GetStringPref(qtssPrefsErrorLogDir);
	}
	char*   GetErrorLogName()
	{
		return this->GetStringPref(qtssPrefsErrorLogName);
	}

	char*   GetModuleDirectory()
	{
		return this->GetStringPref(qtssPrefsModuleFolder);
	}

	char*   GetAuthorizationRealm()
	{
		return this->GetStringPref(qtssPrefsDefaultAuthorizationRealm);
	}

	char*   GetRunUserName()
	{
		return this->GetStringPref(qtssPrefsRunUserName);
	}
	char*   GetRunGroupName()
	{
		return this->GetStringPref(qtssPrefsRunGroupName);
	}

	char*   GetPidFilePath()
	{
		return this->GetStringPref(qtssPrefsPidFile);
	}

	char*   GetStatsMonitorFileName()
	{
		return this->GetStringPref(qtssPrefsMonitorStatsFileName);
	}

	bool ServerStatFileEnabled() { return fEnableMonitorStatsFile; }
	uint32_t GetStatFileIntervalSec() { return fStatsFileIntervalSeconds; }
	bool CloudPlatformEnabled() { return fCloudPlatformEnabled; }
	QTSS_AuthScheme GetAuthScheme() { return fAuthScheme; }

	bool PacketHeaderPrintfsEnabled() { return fEnablePacketHeaderPrintfs; }
	bool PrintRTPHeaders() { return (bool)(fPacketHeaderPrintfOptions & kRTPALL); }
	bool PrintSRHeaders() { return (bool)(fPacketHeaderPrintfOptions & kRTCPSR); }
	bool PrintRRHeaders() { return (bool)(fPacketHeaderPrintfOptions & kRTCPRR); }
	bool PrintAPPHeaders() { return (bool)(fPacketHeaderPrintfOptions & kRTCPAPP); }
	bool PrintACKHeaders() { return (bool)(fPacketHeaderPrintfOptions & kRTCPACK); }

	uint32_t DeleteSDPFilesInterval() { return fsdp_file_delete_interval_seconds; }

	uint32_t  GetNumThreads() { return fNumThreads; } //short tasks threads
	uint32_t  GetNumBlockingThreads() { return fNumRTSPThreads; } //return the number of threads that long tasks will be scheduled on -- RTSP processing for example.

	bool  GetDisableThinning() { return fDisableThinning; }

	uint16_t  GetDefaultStreamQuality() { return fDefaultStreamQuality; }
	bool  GetUDPMonitorEnabled() { return fUDPMonitorEnabled; }
	uint16_t  GetUDPMonitorVideoPort() { return fUDPMonitorVideoPort; }
	uint16_t  GetUDPMonitorAudioPort() { return fUDPMonitorAudioPort; }

	char* GetMonitorDestIP() { return this->GetStringPref(qtssPrefsUDPMonitorDestIPAddr); }

	char* GetMonitorSrcIP() { return this->GetStringPref(qtssPrefsUDPMonitorSourceIPAddr); }

	bool GetAllowGuestDefault() { return fAllowGuestAuthorizeDefault; }

	uint16_t GetServiceLanPort() { return fServiceLANPort; }
	uint16_t GetServiceWanPort() { return fServiceWANPort; }

	char* GetServiceWANIP() { return this->GetStringPref(easyPrefsServiceWANIPAddr); }
	uint16_t GetRTSPWANPort() const {	return fRTSPWANPort; }

	char* GetMovieFolder() { return this->GetStringPref(qtssPrefsMovieFolder); }

private:

	uint32_t      fRTSPTimeoutInSecs;
	std::string   fRTSPTimeoutString;
	uint32_t      fRTSPSessionTimeoutInSecs;
	uint32_t      fRTPSessionTimeoutInSecs;

	int32_t  fMaximumConnections;
	int32_t  fMaxBandwidthInKBits;

	bool  fBreakOnAssert;
	bool  fAutoRestart;
	uint32_t  fTBUpdateTimeInSecs;
	uint32_t  fABUpdateTimeInSecs;
	uint32_t  fSafePlayDurationInSecs;

	uint32_t  fErrorRollIntervalInDays;
	uint32_t  fErrorLogBytes;
	bool  fScreenLoggingEnabled;
	bool  fErrorLogEnabled;

	int32_t  fDropAllPacketsTimeInMsec;
	int32_t  fDropAllVideoPacketsTimeInMsec;
	int32_t  fThinAllTheWayTimeInMsec;
	int32_t  fAlwaysThinTimeInMsec;
	int32_t  fStartThinningTimeInMsec;
	int32_t  fStartThickingTimeInMsec;
	int32_t  fThickAllTheWayTimeInMsec;
	uint32_t  fQualityCheckIntervalInMsec;

	uint32_t  fMinTCPBufferSizeInBytes;
	uint32_t  fMaxTCPBufferSizeInBytes;
	float fTCPSecondsToBuffer;

	bool  fAppendSrcAddrInTransport;

	uint32_t  fSmallWindowSizeInK;
	uint32_t  fMediumWindowSizeInK;
	uint32_t  fLargeWindowSizeInK;
	uint32_t  fWindowSizeThreshold;
	uint32_t  fWindowSizeMaxThreshold;

	uint32_t  fMaxRetransDelayInMsec;
	bool  fIsAckLoggingEnabled;
	uint32_t  fRTCPPollIntervalInMsec;
	uint32_t  fRTCPSocketRcvBufSizeInK;
	bool  fIsSlowStartEnabled;
	uint32_t  fSendIntervalInMsec;
	uint32_t  fMaxSendAheadTimeInSecs;

	bool  fCloudPlatformEnabled;

	QTSS_AuthScheme fAuthScheme;
	uint32_t  fsdp_file_delete_interval_seconds;
	bool  fAutoStart;
	bool  fReliableUDP;
	bool  fReliableUDPPrintfs;
	bool  fEnableRTSPDebugPrintfs;
	bool  fEnableRTSPServerInfo;
	uint32_t  fNumThreads;
	uint32_t  fNumRTSPThreads;

	uint16_t	fServiceLANPort;
	uint16_t	fServiceWANPort;

	bool  fEnableMonitorStatsFile;
	uint32_t  fStatsFileIntervalSeconds;

	float	fOverbufferRate;

	bool   fEnablePacketHeaderPrintfs;
	uint32_t fPacketHeaderPrintfOptions;
	bool   fCloseLogsOnWrite;

	bool   fDisableThinning;
	uint16_t fDefaultStreamQuality;
	bool   fUDPMonitorEnabled;
	uint16_t fUDPMonitorVideoPort;
	uint16_t fUDPMonitorAudioPort;
	char   fUDPMonitorDestAddr[20];
	char   fUDPMonitorSrcAddr[20];
	bool   fAllowGuestAuthorizeDefault;

	char   fRTSPWANAddr[20];
	uint16_t fRTSPWANPort;

	enum //fPacketHeaderPrintfOptions
	{
		kRTPALL = 1 << 0,
		kRTCPSR = 1 << 1,
		kRTCPRR = 1 << 2,
		kRTCPAPP = 1 << 3,
		kRTCPACK = 1 << 4
	};

	enum
	{
		kAllowMultipleValues = 1,
		kDontAllowMultipleValues = 0
	};

	struct PrefInfo
	{
		uint32_t  fAllowMultipleValues;
		char*   fDefaultValue;
		char**  fAdditionalDefVals; // For prefs with multiple default values
	};

	void SetupAttributes();
	void UpdateAuthScheme();
	void UpdatePrintfOptions();
	//
	// Returns the string preference with the specified ID. If there
	// was any problem, this will return an empty string.
	char* GetStringPref(QTSS_AttributeID inAttrID);

	static QTSSAttrInfoDict::AttrInfo   sAttributes[];
	static PrefInfo sPrefInfo[];

	// Prefs that have multiple default values (rtsp_ports) have
	// to be dealt with specially
	static char*    sAdditionalDefaultPorts[];

	// Player prefs
	static char*    sRTP_Header_Players[];
	static char*    sAdjust_Bandwidth_Players[];
	static char*    sNo_Adjust_Pause_Time_Players[];
	static char*    sNo_Pause_Time_Adjustment_Players[];
	static char*    sRTP_Start_Time_Players[];
	static char*    sDisable_Rate_Adapt_Players[];
	static char*    sFixed_Target_Time_Players[];
	static char*    sDisable_Thinning_Players[];

};
#endif //__QTSSPREFS_H__
