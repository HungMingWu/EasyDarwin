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
	File:       QTSSReflectorModule.cpp
	Contains:   Implementation of QTSSReflectorModule class.
*/

#include <boost/spirit/include/qi.hpp>
#include "QTSServerInterface.h"
#include "QTSSReflectorModule.h"
#include "QTSSModuleUtils.h"
#include "ReflectorSession.h"
#include "OSRef.h"
#include "OS.h"
#include "ResizeableStringFormatter.h"
#include "QTAccessFile.h"
#include "RTPSession.h"
#include "RTPSessionOutput.h"
#include "SDPSourceInfo.h"

#include "SDPUtils.h"
#include "sdpCache.h"
#include "RTSPRequest.h"

#include "QueryParamList.h"
#include "EasyUtil.h"
#include "RTSPSession.h"
#include "QTSSFile.h"

using namespace std;
namespace qi = boost::spirit::qi;

#ifndef __Win32__
#include <unistd.h>
#endif

#define REFLECTORSESSION_DEBUG 1

#if DEBUG
#define REFLECTOR_MODULE_DEBUGGING 0
#else
#define REFLECTOR_MODULE_DEBUGGING 0
#endif

static boost::string_view       sBroadcasterSessionName = "QTSSReflectorModuleBroadcasterSession";
static boost::string_view       sStreamCookieName = "QTSSReflectorModuleStreamCookie";
static boost::string_view       sRTPInfoWaitTime = "QTSSReflectorModuleRTPInfoWaitTime";
static boost::string_view       sOutputName = "QTSSReflectorModuleOutput";
static boost::string_view       sKillClientsEnabledName = "QTSSReflectorModuleTearDownClients";

//static boost::string_view       sSessionName = "QTSSReflectorModuleSession";
// ATTRIBUTES
static QTSS_AttributeID         sBufferOffsetAttr = qtssIllegalAttrID;
static QTSS_AttributeID         sExpectedDigitFilenameErr = qtssIllegalAttrID;
static QTSS_AttributeID         sReflectorBadTrackIDErr = qtssIllegalAttrID;
static QTSS_AttributeID         sDuplicateBroadcastStreamErr = qtssIllegalAttrID;
static QTSS_AttributeID         sAnnounceRequiresSDPinNameErr = qtssIllegalAttrID;
static QTSS_AttributeID         sAnnounceDisabledNameErr = qtssIllegalAttrID;
static QTSS_AttributeID         sSDPcontainsInvalidMinimumPortErr = qtssIllegalAttrID;
static QTSS_AttributeID         sSDPcontainsInvalidMaximumPortErr = qtssIllegalAttrID;
static QTSS_AttributeID         sStaticPortsConflictErr = qtssIllegalAttrID;
static QTSS_AttributeID         sInvalidPortRangeErr = qtssIllegalAttrID;

// STATIC DATA

// ref to the prefs dictionary object
static OSRefTable*      sSessionMap = nullptr;
static const boost::string_view kCacheControlHeader("no-cache");
static QTSS_PrefsObject sServerPrefs = nullptr;
static QTSServerInterface* sServer = nullptr;
static QTSS_ModulePrefsObject sPrefs = nullptr;

//
// Prefs
static bool   sAllowNonSDPURLs = true;
static bool   sDefaultAllowNonSDPURLs = true;

static bool   sRTPInfoDisabled = false;
static bool   sDefaultRTPInfoDisabled = false;

static bool   sAnnounceEnabled = true;
static bool   sDefaultAnnounceEnabled = true;
static bool   sBroadcastPushEnabled = true;
static bool   sDefaultBroadcastPushEnabled = true;
static bool   sAllowDuplicateBroadcasts = false;
static bool   sDefaultAllowDuplicateBroadcasts = false;

static uint32_t   sMaxBroadcastAnnounceDuration = 0;
static uint32_t   sDefaultMaxBroadcastAnnounceDuration = 0;
static uint16_t   sMinimumStaticSDPPort = 0;
static uint16_t   sDefaultMinimumStaticSDPPort = 20000;
static uint16_t   sMaximumStaticSDPPort = 0;
static uint16_t   sDefaultMaximumStaticSDPPort = 65535;

static bool   sTearDownClientsOnDisconnect = false;
static bool   sDefaultTearDownClientsOnDisconnect = false;

static bool   sOneSSRCPerStream = true;
static bool   sDefaultOneSSRCPerStream = true;

static uint32_t   sTimeoutSSRCSecs = 30;
static uint32_t   sDefaultTimeoutSSRCSecs = 30;

static uint32_t   sBroadcasterSessionTimeoutSecs = 30;
static uint32_t   sDefaultBroadcasterSessionTimeoutSecs = 30;
static uint32_t   sBroadcasterSessionTimeoutMilliSecs = sBroadcasterSessionTimeoutSecs * 1000;

static uint16_t sLastMax = 0;
static uint16_t sLastMin = 0;

static bool   sEnforceStaticSDPPortRange = false;
static bool   sDefaultEnforceStaticSDPPortRange = false;

static uint32_t   sMaxAnnouncedSDPLengthInKbytes = 4;
//static uint32_t   sDefaultMaxAnnouncedSDPLengthInKbytes = 4;

static QTSS_AttributeID sIPAllowListID = qtssIllegalAttrID;
static char*            sIPAllowList = nullptr;
static char*            sLocalLoopBackAddress = "127.0.0.*";

static bool   sAuthenticateLocalBroadcast = false;
static bool   sDefaultAuthenticateLocalBroadcast = false;

static bool	sDisableOverbuffering = false;
static bool	sDefaultDisableOverbuffering = false;
static bool	sFalse = false;

static bool   sReflectBroadcasts = true;
static bool   sDefaultReflectBroadcasts = true;

static bool   sAnnouncedKill = true;
static bool   sDefaultAnnouncedKill = true;


static bool   sPlayResponseRangeHeader = true;
static bool   sDefaultPlayResponseRangeHeader = true;

static bool   sPlayerCompatibility = true;
static bool   sDefaultPlayerCompatibility = true;

static uint32_t   sAdjustMediaBandwidthPercent = 100;
static uint32_t   sAdjustMediaBandwidthPercentDefault = 100;

static bool   sForceRTPInfoSeqAndTime = false;
static bool   sDefaultForceRTPInfoSeqAndTime = false;

static std::string	sRedirectBroadcastsKeyword;
static char*    sDefaultRedirectBroadcastsKeyword = "";
static std::string  sBroadcastsRedirectDir;
static char*    sDefaultBroadcastsRedirectDir = ""; // match none
static char*    sDefaultBroadcastsDir = ""; // match all
static char*	sDefaultsBroadcasterGroup = "broadcaster";
static StrPtrLen sBroadcasterGroup;

static QTSS_AttributeID sBroadcastDirListID = qtssIllegalAttrID;

static int32_t   sWaitTimeLoopCount = 10;

// Important strings
static boost::string_view    sSDPKillSuffix(".kill");
static boost::string_view    sSDPSuffix("");
static boost::string_view    sMOVSuffix(".mov");
static StrPtrLen    sSDPTooLongMessage("Announced SDP is too long");
static StrPtrLen    sSDPNotValidMessage("Announced SDP is not a valid SDP");
static StrPtrLen    sKILLNotValidMessage("Announced .kill is not a valid SDP");
static StrPtrLen    sSDPTimeNotValidMessage("SDP time is not valid or movie not available at this time.");
static StrPtrLen    sBroadcastNotAllowed("Broadcast is not allowed.");
static StrPtrLen    sBroadcastNotActive("Broadcast is not active.");
static boost::string_view    sTheNowRangeHeader("npt=now-");

// FUNCTION PROTOTYPES

static QTSS_Error QTSSReflectorModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParams);
static QTSS_Error Register(QTSS_Register_Params* inParams);
static QTSS_Error Initialize(QTSS_Initialize_Params* inParams);
static QTSS_Error Shutdown();
static QTSS_Error ProcessRTSPRequest(QTSS_StandardRTSP_Params* inParams);
static QTSS_Error DoAnnounce(QTSS_StandardRTSP_Params* inParams);
static QTSS_Error DoDescribe(QTSS_StandardRTSP_Params* inParams);
ReflectorSession* FindOrCreateSession(boost::string_view inName, QTSS_StandardRTSP_Params* inParams, uint32_t inChannel = 0, StrPtrLen* inData = nullptr, bool isPush = false, bool *foundSessionPtr = nullptr);
static QTSS_Error DoSetup(QTSS_StandardRTSP_Params* inParams);
static QTSS_Error DoPlay(QTSS_StandardRTSP_Params* inParams, ReflectorSession* inSession);
static QTSS_Error DestroySession(QTSS_ClientSessionClosing_Params* inParams);
static void RemoveOutput(ReflectorOutput* inOutput, ReflectorSession* inSession, bool killClients);
static ReflectorSession* DoSessionSetup(QTSS_StandardRTSP_Params* inParams, bool isPush = false, bool *foundSessionPtr = nullptr, std::string* resultFilePath = nullptr);
static QTSS_Error RereadPrefs();
static QTSS_Error ProcessRTPData(QTSS_IncomingData_Params* inParams);
static QTSS_Error ReflectorAuthorizeRTSPRequest(QTSS_StandardRTSP_Params* inParams);
static bool InfoPortsOK(QTSS_StandardRTSP_Params* inParams, SDPSourceInfo* theInfo, boost::string_view inPath);
void KillCommandPathInList();
bool KillSession(boost::string_view sdpPath, bool killClients);
QTSS_Error IntervalRole();
static bool AcceptSession(QTSS_StandardRTSP_Params* inParams);
static QTSS_Error RedirectBroadcast(QTSS_StandardRTSP_Params* inParams);
static QTSS_Error GetDeviceStream(Easy_GetDeviceStream_Params* inParams);

inline void KeepSession(RTSPRequest* theRequest, bool keep)
{
	theRequest->SetResponseKeepAlive(keep);
}

// FUNCTION IMPLEMENTATIONS
QTSS_Error QTSSReflectorModule_Main(void* inPrivateArgs)
{
	return _stublibrary_main(inPrivateArgs, QTSSReflectorModuleDispatch);
}

QTSS_Error  QTSSReflectorModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParams)
{
	switch (inRole)
	{
	case QTSS_Register_Role:
		return Register(&inParams->regParams);
	case QTSS_Initialize_Role:
		return Initialize(&inParams->initParams);
	case QTSS_RereadPrefs_Role:
		return RereadPrefs();
	case QTSS_RTSPRoute_Role:
		return RedirectBroadcast(&inParams->rtspRouteParams);
	case QTSS_RTSPPreProcessor_Role:
		return ProcessRTSPRequest(&inParams->rtspRequestParams);
	case QTSS_RTSPIncomingData_Role:
		return ProcessRTPData(&inParams->rtspIncomingDataParams);
	case QTSS_ClientSessionClosing_Role:
		return DestroySession(&inParams->clientSessionClosingParams);
	case QTSS_Shutdown_Role:
		return Shutdown();
	case QTSS_RTSPAuthorize_Role:
		return ReflectorAuthorizeRTSPRequest(&inParams->rtspRequestParams);
	case QTSS_Interval_Role:
		return IntervalRole();
	case Easy_GetDeviceStream_Role:
		return GetDeviceStream(&inParams->easyGetDeviceStreamParams);
	default: break;
	}
	return QTSS_NoErr;
}


QTSS_Error Register(QTSS_Register_Params* inParams)
{
	// Do role & attribute setup
	(void)QTSS_AddRole(QTSS_Initialize_Role);
	(void)QTSS_AddRole(QTSS_Shutdown_Role);
	(void)QTSS_AddRole(QTSS_RTSPPreProcessor_Role);
	(void)QTSS_AddRole(QTSS_ClientSessionClosing_Role);
	(void)QTSS_AddRole(QTSS_RTSPIncomingData_Role);
	(void)QTSS_AddRole(QTSS_RTSPAuthorize_Role);
	(void)QTSS_AddRole(QTSS_RereadPrefs_Role);
	(void)QTSS_AddRole(QTSS_RTSPRoute_Role);
	(void)QTSS_AddRole(Easy_GetDeviceStream_Role);

	// Add text messages attributes
	static char*        sExpectedDigitFilenameName = "QTSSReflectorModuleExpectedDigitFilename";
	static char*        sReflectorBadTrackIDErrName = "QTSSReflectorModuleBadTrackID";
	static char*        sDuplicateBroadcastStreamName = "QTSSReflectorModuleDuplicateBroadcastStream";
	static char*        sAnnounceRequiresSDPinName = "QTSSReflectorModuleAnnounceRequiresSDPSuffix";
	static char*        sAnnounceDisabledName = "QTSSReflectorModuleAnnounceDisabled";

	(void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sDuplicateBroadcastStreamName, nullptr, qtssAttrDataTypeCharArray);
	(void)QTSS_IDForAttr(qtssTextMessagesObjectType, sDuplicateBroadcastStreamName, &sDuplicateBroadcastStreamErr);

	(void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sAnnounceRequiresSDPinName, nullptr, qtssAttrDataTypeCharArray);
	(void)QTSS_IDForAttr(qtssTextMessagesObjectType, sAnnounceRequiresSDPinName, &sAnnounceRequiresSDPinNameErr);

	(void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sAnnounceDisabledName, nullptr, qtssAttrDataTypeCharArray);
	(void)QTSS_IDForAttr(qtssTextMessagesObjectType, sAnnounceDisabledName, &sAnnounceDisabledNameErr);


	(void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sExpectedDigitFilenameName, nullptr, qtssAttrDataTypeCharArray);
	(void)QTSS_IDForAttr(qtssTextMessagesObjectType, sExpectedDigitFilenameName, &sExpectedDigitFilenameErr);

	(void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sReflectorBadTrackIDErrName, nullptr, qtssAttrDataTypeCharArray);
	(void)QTSS_IDForAttr(qtssTextMessagesObjectType, sReflectorBadTrackIDErrName, &sReflectorBadTrackIDErr);

	static char* sSDPcontainsInvalidMinumumPortErrName = "QTSSReflectorModuleSDPPortMinimumPort";
	(void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sSDPcontainsInvalidMinumumPortErrName, nullptr, qtssAttrDataTypeCharArray);
	(void)QTSS_IDForAttr(qtssTextMessagesObjectType, sSDPcontainsInvalidMinumumPortErrName, &sSDPcontainsInvalidMinimumPortErr);

	static char* sSDPcontainsInvalidMaximumPortErrName = "QTSSReflectorModuleSDPPortMaximumPort";
	(void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sSDPcontainsInvalidMaximumPortErrName, nullptr, qtssAttrDataTypeCharArray);
	(void)QTSS_IDForAttr(qtssTextMessagesObjectType, sSDPcontainsInvalidMaximumPortErrName, &sSDPcontainsInvalidMaximumPortErr);

	static char* sStaticPortsConflictErrName = "QTSSReflectorModuleStaticPortsConflict";
	(void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sStaticPortsConflictErrName, nullptr, qtssAttrDataTypeCharArray);
	(void)QTSS_IDForAttr(qtssTextMessagesObjectType, sStaticPortsConflictErrName, &sStaticPortsConflictErr);

	static char* sInvalidPortRangeErrName = "QTSSReflectorModuleStaticPortPrefsBadRange";
	(void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sInvalidPortRangeErrName, nullptr, qtssAttrDataTypeCharArray);
	(void)QTSS_IDForAttr(qtssTextMessagesObjectType, sInvalidPortRangeErrName, &sInvalidPortRangeErr);

	// Reflector session needs to setup some parameters too.
	ReflectorStream::Register();

	// Tell the server our name!
	static char* sModuleName = "QTSSReflectorModule";
	::strcpy(inParams->outModuleName, sModuleName);

	return QTSS_NoErr;
}

static std::string buildStreamName(boost::string_view theStreamName, uint32_t theChannelNum)
{
	return std::string(theStreamName) + EASY_KEY_SPLITER + std::to_string(theChannelNum);
}

QTSS_Error Initialize(QTSS_Initialize_Params* inParams)
{
	// Setup module utils
	QTSSModuleUtils::Initialize(inParams->inMessages, inParams->inServer, inParams->inErrorLogStream);
	QTAccessFile::Initialize();
	sSessionMap = QTSServerInterface::GetServer()->GetReflectorSessionMap();
	sServerPrefs = inParams->inPrefs;
	sServer = inParams->inServer;
#if QTSS_REFLECTOR_EXTERNAL_MODULE
	// The reflector is dependent on a number of objects in the Common Utilities
	// library that get setup by the server if the reflector is internal to the
	// server.
	//
	// So, if the reflector is being built as a code fragment, it must initialize
	// those pieces itself
#if !MACOSXEVENTQUEUE
	::select_startevents();//initialize the select() implementation of the event queue
#endif
	OS::Initialize();
	Socket::Initialize();
	SocketUtils::Initialize();

	const uint32_t kNumReflectorThreads = 8;
	TaskThreadPool::AddThreads(kNumReflectorThreads);
	IdleTask::Initialize();
	Socket::StartThread();
#endif

	sPrefs = QTSSModuleUtils::GetModulePrefsObject(inParams->inModule);

	// Call helper class initializers
	ReflectorStream::Initialize(sPrefs);

	// Report to the server that this module handles DESCRIBE, SETUP, PLAY, PAUSE, and TEARDOWN
	static QTSS_RTSPMethod sSupportedMethods[] = { qtssDescribeMethod, qtssSetupMethod, qtssTeardownMethod, qtssPlayMethod, qtssPauseMethod, qtssAnnounceMethod, qtssRecordMethod };
	QTSSModuleUtils::SetupSupportedMethods(inParams->inServer, sSupportedMethods, 7);

	RereadPrefs();

	return QTSS_NoErr;
}

char *GetTrimmedKeyWord(char *prefKeyWord)
{
	StrPtrLen redirKeyWordStr(prefKeyWord);
	StringParser theRequestPathParser(&redirKeyWordStr);

	// trim leading / from the keyword
	while (theRequestPathParser.Expect(kPathDelimiterChar)) {};

	StrPtrLen theKeyWordStr;
	theRequestPathParser.ConsumeUntil(&theKeyWordStr, kPathDelimiterChar); // stop when we see a / and don't include

	auto *keyword = new char[theKeyWordStr.Len + 1];
	::memcpy(keyword, theKeyWordStr.Ptr, theKeyWordStr.Len);
	keyword[theKeyWordStr.Len] = 0;

	return keyword;
}

void SetMoviesRelativeDir()
{
	char* movieFolderString = nullptr;
	(void)((QTSSDictionary*)sServerPrefs)->GetValueAsString(qtssPrefsMovieFolder, 0, &movieFolderString);
	std::unique_ptr<char[]> deleter(movieFolderString);

	ResizeableStringFormatter redirectPath(nullptr, 0);
	redirectPath.Put(movieFolderString);
	if (redirectPath.GetBytesWritten() > 0 && kPathDelimiterChar != redirectPath.GetBufPtr()[redirectPath.GetBytesWritten() - 1])
		redirectPath.PutChar(kPathDelimiterChar);
	redirectPath.Put(sBroadcastsRedirectDir);

	auto *newMovieRelativeDir = new char[redirectPath.GetBytesWritten() + 1];
	::memcpy(newMovieRelativeDir, redirectPath.GetBufPtr(), redirectPath.GetBytesWritten());
	newMovieRelativeDir[redirectPath.GetBytesWritten()] = 0;

	sBroadcastsRedirectDir = newMovieRelativeDir;

}

QTSS_Error RereadPrefs()
{
	//
	// Use the standard GetPref routine to retrieve the correct values for our preferences
	QTSSModuleUtils::GetAttribute(sPrefs, "disable_rtp_play_info", qtssAttrDataTypeBool16,
		&sRTPInfoDisabled, &sDefaultRTPInfoDisabled, sizeof(sDefaultRTPInfoDisabled));

	QTSSModuleUtils::GetAttribute(sPrefs, "allow_non_sdp_urls", qtssAttrDataTypeBool16,
		&sAllowNonSDPURLs, &sDefaultAllowNonSDPURLs, sizeof(sDefaultAllowNonSDPURLs));

	QTSSModuleUtils::GetAttribute(sPrefs, "enable_broadcast_announce", qtssAttrDataTypeBool16,
		&sAnnounceEnabled, &sDefaultAnnounceEnabled, sizeof(sDefaultAnnounceEnabled));
	QTSSModuleUtils::GetAttribute(sPrefs, "enable_broadcast_push", qtssAttrDataTypeBool16,
		&sBroadcastPushEnabled, &sDefaultBroadcastPushEnabled, sizeof(sDefaultBroadcastPushEnabled));
	QTSSModuleUtils::GetAttribute(sPrefs, "max_broadcast_announce_duration_secs", qtssAttrDataTypeUInt32,
		&sMaxBroadcastAnnounceDuration, &sDefaultMaxBroadcastAnnounceDuration, sizeof(sDefaultMaxBroadcastAnnounceDuration));
	QTSSModuleUtils::GetAttribute(sPrefs, "allow_duplicate_broadcasts", qtssAttrDataTypeBool16,
		&sAllowDuplicateBroadcasts, &sDefaultAllowDuplicateBroadcasts, sizeof(sDefaultAllowDuplicateBroadcasts));

	QTSSModuleUtils::GetAttribute(sPrefs, "enforce_static_sdp_port_range", qtssAttrDataTypeBool16,
		&sEnforceStaticSDPPortRange, &sDefaultEnforceStaticSDPPortRange, sizeof(sDefaultEnforceStaticSDPPortRange));
	QTSSModuleUtils::GetAttribute(sPrefs, "minimum_static_sdp_port", qtssAttrDataTypeUInt16,
		&sMinimumStaticSDPPort, &sDefaultMinimumStaticSDPPort, sizeof(sDefaultMinimumStaticSDPPort));
	QTSSModuleUtils::GetAttribute(sPrefs, "maximum_static_sdp_port", qtssAttrDataTypeUInt16,
		&sMaximumStaticSDPPort, &sDefaultMaximumStaticSDPPort, sizeof(sDefaultMaximumStaticSDPPort));

	QTSSModuleUtils::GetAttribute(sPrefs, "kill_clients_when_broadcast_stops", qtssAttrDataTypeBool16,
		&sTearDownClientsOnDisconnect, &sDefaultTearDownClientsOnDisconnect, sizeof(sDefaultTearDownClientsOnDisconnect));
	QTSSModuleUtils::GetAttribute(sPrefs, "use_one_SSRC_per_stream", qtssAttrDataTypeBool16,
		&sOneSSRCPerStream, &sDefaultOneSSRCPerStream, sizeof(sDefaultOneSSRCPerStream));
	QTSSModuleUtils::GetAttribute(sPrefs, "timeout_stream_SSRC_secs", qtssAttrDataTypeUInt32,
		&sTimeoutSSRCSecs, &sDefaultTimeoutSSRCSecs, sizeof(sDefaultTimeoutSSRCSecs));

	QTSSModuleUtils::GetAttribute(sPrefs, "timeout_broadcaster_session_secs", qtssAttrDataTypeUInt32,
		&sBroadcasterSessionTimeoutSecs, &sDefaultBroadcasterSessionTimeoutSecs, sizeof(sDefaultTimeoutSSRCSecs));

	if (sBroadcasterSessionTimeoutSecs < 30)
		sBroadcasterSessionTimeoutSecs = 30;

	QTSSModuleUtils::GetAttribute(sPrefs, "authenticate_local_broadcast", qtssAttrDataTypeBool16,
		&sAuthenticateLocalBroadcast, &sDefaultAuthenticateLocalBroadcast, sizeof(sDefaultAuthenticateLocalBroadcast));

	QTSSModuleUtils::GetAttribute(sPrefs, "disable_overbuffering", qtssAttrDataTypeBool16,
		&sDisableOverbuffering, &sDefaultDisableOverbuffering, sizeof(sDefaultDisableOverbuffering));

	QTSSModuleUtils::GetAttribute(sPrefs, "allow_broadcasts", qtssAttrDataTypeBool16,
		&sReflectBroadcasts, &sDefaultReflectBroadcasts, sizeof(sDefaultReflectBroadcasts));

	QTSSModuleUtils::GetAttribute(sPrefs, "allow_announced_kill", qtssAttrDataTypeBool16,
		&sAnnouncedKill, &sDefaultAnnouncedKill, sizeof(sDefaultAnnouncedKill));

	QTSSModuleUtils::GetAttribute(sPrefs, "enable_play_response_range_header", qtssAttrDataTypeBool16,
		&sPlayResponseRangeHeader, &sDefaultPlayResponseRangeHeader, sizeof(sDefaultPlayResponseRangeHeader));

	QTSSModuleUtils::GetAttribute(sPrefs, "enable_player_compatibility", qtssAttrDataTypeBool16,
		&sPlayerCompatibility, &sDefaultPlayerCompatibility, sizeof(sDefaultPlayerCompatibility));

	QTSSModuleUtils::GetAttribute(sPrefs, "compatibility_adjust_sdp_media_bandwidth_percent", qtssAttrDataTypeUInt32,
		&sAdjustMediaBandwidthPercent, &sAdjustMediaBandwidthPercentDefault, sizeof(sAdjustMediaBandwidthPercentDefault));

	if (sAdjustMediaBandwidthPercent > 100)
		sAdjustMediaBandwidthPercent = 100;

	if (sAdjustMediaBandwidthPercent < 1)
		sAdjustMediaBandwidthPercent = 1;

	QTSSModuleUtils::GetAttribute(sPrefs, "force_rtp_info_sequence_and_time", qtssAttrDataTypeBool16,
		&sForceRTPInfoSeqAndTime, &sDefaultForceRTPInfoSeqAndTime, sizeof(sDefaultForceRTPInfoSeqAndTime));

	sBroadcasterGroup.Delete();
	sBroadcasterGroup.Set(QTSSModuleUtils::GetStringAttribute(sPrefs, "BroadcasterGroup", sDefaultsBroadcasterGroup));

	char* tempKeyWord = QTSSModuleUtils::GetStringAttribute(sPrefs, "redirect_broadcast_keyword", sDefaultRedirectBroadcastsKeyword);

	sRedirectBroadcastsKeyword = GetTrimmedKeyWord(tempKeyWord);
	delete[] tempKeyWord;

	sBroadcastsRedirectDir = QTSSModuleUtils::GetStringAttribute(sPrefs, "redirect_broadcasts_dir", sDefaultBroadcastsRedirectDir);
	if (!sBroadcastsRedirectDir.empty() && sBroadcastsRedirectDir[0] != kPathDelimiterChar)
		SetMoviesRelativeDir();

	delete[] QTSSModuleUtils::GetStringAttribute(sPrefs, "broadcast_dir_list", sDefaultBroadcastsDir); // initialize if there isn't one
	sBroadcastDirListID = QTSSModuleUtils::GetAttrID(sPrefs, "broadcast_dir_list");

	delete[] sIPAllowList;
	sIPAllowList = QTSSModuleUtils::GetStringAttribute(sPrefs, "ip_allow_list", sLocalLoopBackAddress);
	sIPAllowListID = QTSSModuleUtils::GetAttrID(sPrefs, "ip_allow_list");


	sBroadcasterSessionTimeoutMilliSecs = sBroadcasterSessionTimeoutSecs * 1000;

	if (sEnforceStaticSDPPortRange)
	{
		bool reportErrors = false;
		if (sLastMax != sMaximumStaticSDPPort)
		{
			sLastMax = sMaximumStaticSDPPort;
			reportErrors = true;
		}

		if (sLastMin != sMinimumStaticSDPPort)
		{
			sLastMin = sMinimumStaticSDPPort;
			reportErrors = true;
		}

		if (reportErrors)
		{
			uint16_t minServerPort = 6970;
			uint16_t maxServerPort = 9999;
			char min[32];
			char max[32];

			if (((sMinimumStaticSDPPort <= minServerPort) && (sMaximumStaticSDPPort >= minServerPort))
				|| ((sMinimumStaticSDPPort >= minServerPort) && (sMinimumStaticSDPPort <= maxServerPort))
				)
			{
				sprintf(min, "%u", minServerPort);
				sprintf(max, "%u", maxServerPort);
				QTSSModuleUtils::LogError(qtssWarningVerbosity, sStaticPortsConflictErr, 0, min, max);
			}

			if (sMinimumStaticSDPPort > sMaximumStaticSDPPort)
			{
				sprintf(min, "%u", sMinimumStaticSDPPort);
				sprintf(max, "%u", sMaximumStaticSDPPort);
				QTSSModuleUtils::LogError(qtssWarningVerbosity, sInvalidPortRangeErr, 0, min, max);
			}
		}
	}

	KillCommandPathInList();

	return QTSS_NoErr;
}

QTSS_Error Shutdown()
{
#if QTSS_REFLECTOR_EXTERNAL_MODULE
	TaskThreadPool::RemoveThreads();
#endif
	return QTSS_NoErr;
}

QTSS_Error IntervalRole() // not used
{
	(void)QTSS_SetIntervalRoleTimer(0); // turn off

	return QTSS_NoErr;
}


QTSS_Error ProcessRTPData(QTSS_IncomingData_Params* inParams)
{
	if (!sBroadcastPushEnabled)
		return QTSS_NoErr;

	//printf("QTSSReflectorModule:ProcessRTPData inRTSPSession=%"   _U32BITARG_   " inClientSession=%"   _U32BITARG_   "\n",inParams->inRTSPSession, inParams->inClientSession);

	uint32_t theLen;
	boost::optional<boost::any> attr = inParams->inRTSPSession->getAttribute(sBroadcasterSessionName);
	if (!attr) return QTSS_NoErr;
	ReflectorSession* theSession = boost::any_cast<ReflectorSession *>(attr.value());
	//printf("QTSSReflectorModule.cpp:ProcessRTPData    sClientBroadcastSessionAttr=%"   _U32BITARG_   " theSession=%"   _U32BITARG_   " err=%" _S32BITARG_ " \n",sClientBroadcastSessionAttr, theSession,theErr);

	// it is a broadcaster session
	//printf("QTSSReflectorModule.cpp:is broadcaster session\n");

	SourceInfo* theSoureInfo = theSession->GetSourceInfo();
	Assert(theSoureInfo != nullptr);
	if (theSoureInfo == nullptr)
		return QTSS_NoErr;

	uint32_t  numStreams = theSession->GetNumStreams();
	//printf("QTSSReflectorModule.cpp:ProcessRTPData numStreams=%"   _U32BITARG_   "\n",numStreams);
	{
		/*
		   Stream data such as RTP packets is encapsulated by an ASCII dollar
		   sign (24 hexadecimal), followed by a one-byte channel identifier,
		   followed by the length of the encapsulated binary data as a binary,
		   two-byte integer in network byte order. The stream data follows
		   immediately afterwards, without a CRLF, but including the upper-layer
		   protocol headers. Each $ block contains exactly one upper-layer
		   protocol data unit, e.g., one RTP packet.
		*/
		char*   packetData = inParams->inPacketData;

		uint8_t   packetChannel;
		packetChannel = (uint8_t)packetData[1];

		uint16_t  packetDataLen;
		memcpy(&packetDataLen, &packetData[2], 2);
		packetDataLen = ntohs(packetDataLen);

		char*   rtpPacket = &packetData[4];

		//uint32_t    packetLen = inParams->inPacketLen;
		//printf("QTSSReflectorModule.cpp:ProcessRTPData channel=%u theSoureInfo=%"   _U32BITARG_   " packetLen=%"   _U32BITARG_   " packetDatalen=%u\n",(uint16_t) packetChannel,theSoureInfo,inParams->inPacketLen,packetDataLen);

		if (1)
		{
			uint32_t inIndex = packetChannel / 2; // one stream per every 2 channels rtcp channel handled below
			if (inIndex < numStreams)
			{
				ReflectorStream* theStream = theSession->GetStreamByIndex(inIndex);
				if (theStream == nullptr) return QTSS_Unimplemented;

				SourceInfo::StreamInfo* theStreamInfo = theStream->GetStreamInfo();
				uint16_t serverReceivePort = theStreamInfo->fPort;

				bool isRTCP = false;
				if (theStream != nullptr)
				{
					if (packetChannel & 1)
					{
						serverReceivePort++;
						isRTCP = true;
					}
					theStream->PushPacket(rtpPacket, packetDataLen, isRTCP);
					//printf("QTSSReflectorModule.cpp:ProcessRTPData Send RTSP packet channel=%u to UDP localServerAddr=%"   _U32BITARG_   " serverReceivePort=%"   _U32BITARG_   " packetDataLen=%u \n", (uint16_t) packetChannel, localServerAddr, serverReceivePort,packetDataLen);
				}
			}
		}
	}
	return QTSS_NoErr;
}


QTSS_Error ProcessRTSPRequest(QTSS_StandardRTSP_Params* inParams)
{
	OSMutexLocker locker(sSessionMap->GetMutex()); //operating on sOutputAttr

	//printf("QTSSReflectorModule:ProcessRTSPRequest inClientSession=%"   _U32BITARG_   "\n", (uint32_t) inParams->inClientSession);
	uint32_t theLen = 0;
	QTSS_RTSPMethod theMethod = ((RTSPRequest*)inParams->inRTSPRequest)->GetMethod();

	if (theMethod == qtssAnnounceMethod)
		return DoAnnounce(inParams);
	if (theMethod == qtssDescribeMethod)
		return DoDescribe(inParams);
	if (theMethod == qtssSetupMethod)
		return DoSetup(inParams);


	auto opt = inParams->inClientSession->getAttribute(sOutputName);
	if (!opt) // a broadcaster push session
	{
		if (theMethod == qtssPlayMethod || theMethod == qtssRecordMethod)
			return DoPlay(inParams, nullptr);
		else
			return QTSS_RequestFailed;
	}

	RTPSessionOutput* theOutput = boost::any_cast<RTPSessionOutput*>(opt.value());
	switch (theMethod)
	{
	case qtssPlayMethod:
		return DoPlay(inParams, theOutput->GetReflectorSession());
	case qtssTeardownMethod:
		// Tell the server that this session should be killed, and send a TEARDOWN response
		inParams->inClientSession->Teardown();
		inParams->inRTSPRequest->SetKeepAlive(false);
		inParams->inRTSPRequest->SendHeader();
		break;
	case qtssPauseMethod:
		inParams->inClientSession->Pause();
		inParams->inRTSPRequest->SendHeader();
		break;
	default:
		break;
	}
	return QTSS_NoErr;
}

static std::string getQueryString(RTSPRequest *pReq) {
	std::string theQueryString = std::string(pReq->GetQueryString());
	if (!theQueryString.empty())
		theQueryString = EasyUtil::Urldecode(theQueryString.data());
	return theQueryString;
};

static uint32_t GetChannel(RTSPRequest *pReq) {
	std::string queryTemp = getQueryString(pReq);
	QueryParamList parList(const_cast<char *>(queryTemp.c_str()));
	const char* chnNum = parList.DoFindCGIValueForParam(EASY_TAG_CHANNEL);
	if (chnNum)
		return std::stoi(chnNum);
	return 1;
}

static ReflectorSession* DoSessionSetup(QTSS_StandardRTSP_Params* inParams, bool isPush, bool *foundSessionPtr, std::string* resultFilePath)
{
	std::string theFullPath = inParams->inRTSPRequest->GetFileName();

	uint32_t theChannelNum = GetChannel(inParams->inRTSPRequest);

	if (boost::ends_with(theFullPath, sMOVSuffix))
		return nullptr;

	if (sAllowNonSDPURLs && !isPush)
	{
		// Check and see if the full path to this file matches an existing ReflectorSession
		std::string fileName = inParams->inRTSPRequest->GetFileName();
		boost::string_view theRootDir = inParams->inRTSPRequest->GetRootDir();
		std::string t1(theRootDir);
		std::string t2 = fileName;
		std::string t3(sSDPSuffix);
		std::string thePathPtr(t1 + t2 + t3);

		if (resultFilePath != nullptr)
			*resultFilePath = thePathPtr;
		return FindOrCreateSession(thePathPtr, inParams, theChannelNum);
	}
	else
	{
		if (!sDefaultBroadcastPushEnabled)
			return nullptr;
		//
		// We aren't supposed to auto-append a .sdp, so just get the URL path out of the server
		//StrPtrLen theFullPath;
		//QTSS_Error theErr = QTSS_GetValuePtr(inParams->inRTSPRequest, qtssRTSPReqLocalPath, 0, (void**)&theFullPath.Ptr, &theFullPath.Len);
		//Assert(theErr == QTSS_NoErr);

		if (boost::ends_with(theFullPath, sSDPSuffix))
		{
			if (resultFilePath != nullptr)
				*resultFilePath = theFullPath;
			return FindOrCreateSession(theFullPath, inParams, theChannelNum, nullptr, isPush, foundSessionPtr);
		}
		return nullptr;
	}
}

std::string DoAnnounceAddRequiredSDPLines(QTSS_StandardRTSP_Params* inParams, char* theSDPPtr)
{
	std::string editedSDP;
	SDPContainer checkedSDPContainer;
	boost::string_view SDPStr(theSDPPtr, strlen(theSDPPtr));
	checkedSDPContainer.SetSDPBuffer(SDPStr);
	if (!checkedSDPContainer.HasReqLines())
	{
		if (!checkedSDPContainer.HasLineType('v'))
		{ // add v line
			editedSDP += "v=0\r\n";
		}

		if (!checkedSDPContainer.HasLineType('s'))
		{
			// add s line
			boost::string_view theSDPName = ((RTSPRequest*)inParams->inRTSPRequest)->GetAbsolutePath();
			if (theSDPName.empty())
				editedSDP += "s=unknown\r\n";
			else
			{
				editedSDP += "s=" + std::string(theSDPName) + "\r\n";
			}
		}

		if (!checkedSDPContainer.HasLineType('t'))
		{ // add t line
			editedSDP += "t=0 0\r\n";
		}

		if (!checkedSDPContainer.HasLineType('o'))
		{ // add o line
			editedSDP += "o=";
			char tempBuff[256] = ""; tempBuff[255] = 0;
			char *nameStr = tempBuff;
			uint32_t buffLen = sizeof(tempBuff) - 1;
			std::string userAgent(inParams->inClientSession->GetUserAgent());
			nameStr = (char *)userAgent.c_str();
			buffLen = userAgent.length();
			for (uint32_t c = 0; c < buffLen; c++)
			{
				if (StringParser::sEOLWhitespaceMask[(uint8_t)nameStr[c]])
				{
					nameStr[c] = 0;
					break;
				}
			}

			buffLen = ::strlen(nameStr);
			if (buffLen == 0)
				editedSDP += "announced_broadcast";
			else
				editedSDP += std::string(nameStr, buffLen);

			editedSDP += " ";

			buffLen = sizeof(tempBuff) - 1;
			editedSDP += std::string(inParams->inClientSession->GetSessionID());

			editedSDP += " ";
			snprintf(tempBuff, sizeof(tempBuff) - 1, "%" _64BITARG_ "d", (int64_t)OS::UnixTime_Secs() + 2208988800LU);
			editedSDP += tempBuff;

			editedSDP += " IN IP4 ";
			editedSDP += std::string(inParams->inClientSession->GetRemoteAddr());
			editedSDP += "\r\n";
		}
	}

	editedSDP += theSDPPtr;
	return editedSDP;
}


QTSS_Error DoAnnounce(QTSS_StandardRTSP_Params* inParams)
{
	if (!sAnnounceEnabled)
		return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssPreconditionFailed, sAnnounceDisabledNameErr);

	// If this is SDP data, the reflector has the ability to write the data
	// to the file system location specified by the URL.

	//
	// This is a completely stateless action. No ReflectorSession gets created (obviously).

	//
	// Eventually, we should really require access control before we do this.
	//printf("QTSSReflectorModule:DoAnnounce\n");
	//
	// Get the full path to this file
	std::string theFullPath = inParams->inRTSPRequest->GetFileName();

	uint32_t theChannelNum = GetChannel(inParams->inRTSPRequest);


	std::string theStreamName = buildStreamName(theFullPath, theChannelNum);

	// Check for a .kill at the end
	bool pathOK = false;
	bool killBroadcast = false;
	if (sAnnouncedKill && boost::ends_with(theFullPath, sSDPKillSuffix))
	{
		pathOK = true;
		killBroadcast = true;
	}

	// Check for a .sdp at the end
	if (!pathOK && !boost::ends_with(theFullPath, sSDPSuffix))
		return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssPreconditionFailed, sAnnounceRequiresSDPinNameErr);


	// Ok, this is an sdp file. Retreive the entire contents of the SDP.
	// This has to be done asynchronously (in case the SDP stuff is fragmented across
	// multiple packets. So, we have to have a simple state machine.

	//
	// We need to know the content length to manage memory
	uint32_t theLen = 0;
	uint32_t theContentLen = inParams->inRTSPRequest->GetContentLength();

	// Check if the content-length is more than the imposed maximum
	// if it is then return error response
	if ((sMaxAnnouncedSDPLengthInKbytes != 0) && theContentLen > (sMaxAnnouncedSDPLengthInKbytes * 1024))
		return QTSSModuleUtils::SendErrorResponseWithMessage(inParams->inRTSPRequest, qtssPreconditionFailed, &sSDPTooLongMessage);

	//
	// Check for the existence of 2 attributes in the request: a pointer to our buffer for
	// the request body, and the current offset in that buffer. If these attributes exist,
	// then we've already been here for this request. If they don't exist, add them.
	char theRequestBody[65536];

	//
	// We have our buffer and offset. Read the data.
	QTSS_Error theErr = inParams->inRTSPRequest->Read(theRequestBody, theContentLen, &theLen);
	Assert(theErr != QTSS_BadArgument);

	if (theErr == QTSS_RequestFailed)
	{
		std::unique_ptr<char[]> charArrayPathDeleter(theRequestBody);
		//
		// NEED TO RETURN RTSP ERROR RESPONSE
		return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssClientBadRequest, 0);
	}

	if ((theErr == QTSS_WouldBlock) || (theLen < theContentLen))
	{
		theErr = inParams->inRTSPRequest->RequestEvent(QTSS_ReadableEvent);
		Assert(theErr == QTSS_NoErr);
		return QTSS_NoErr;
	}

	Assert(theErr == QTSS_NoErr);


	//
	// If we've gotten here, we have the entire content body in our buffer.
	//

	if (killBroadcast)
	{
		boost::string_view t1(theFullPath.data(), theFullPath.length() - sSDPKillSuffix.length());
		if (KillSession(t1, killBroadcast))
			return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssServerInternal, 0);
		else
			return QTSSModuleUtils::SendErrorResponseWithMessage(inParams->inRTSPRequest, qtssClientNotFound, &sKILLNotValidMessage);
	}

	// ------------  Clean up missing required SDP lines

	std::string editedSDP = DoAnnounceAddRequiredSDPLines(inParams, theRequestBody);

	// ------------ Check the headers

	SDPContainer checkedSDPContainer;
	checkedSDPContainer.SetSDPBuffer(editedSDP);
	if (!checkedSDPContainer.IsSDPBufferValid())
	{
		return QTSSModuleUtils::SendErrorResponseWithMessage(inParams->inRTSPRequest, qtssUnsupportedMediaType, &sSDPNotValidMessage);
	}

	SDPSourceInfo theSDPSourceInfo(editedSDP.c_str(), editedSDP.length());

	if (!InfoPortsOK(inParams, &theSDPSourceInfo, theFullPath)) // All validity checks like this check should be done before touching the file.
	{
		return QTSS_NoErr; // InfoPortsOK is sending back the error.
	}

	// ------------ reorder the sdp headers to make them proper.

	SDPLineSorter sortedSDP(checkedSDPContainer);

	// ------------ Write the SDP 

	// sortedSDP.GetSessionHeaders()->PrintStrEOL();
	// sortedSDP.GetMediaHeaders()->PrintStrEOL();

#if 0
	   // write the file !! need error reporting
	FILE* theSDPFile = ::fopen(theFullPath.Ptr, "wb");//open 
	if (theSDPFile != NULL)
	{
		fprintf(theSDPFile, "%s", sessionHeaders);
		fprintf(theSDPFile, "%s", mediaHeaders);
		::fflush(theSDPFile);
		::fclose(theSDPFile);
	}
	else
	{
		return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssClientForbidden, 0);
	}
#endif 
	std::string sdpContext = sortedSDP.GetSortedSDPStr();
	CSdpCache::GetInstance()->setSdpMap((char *)theStreamName.c_str(), const_cast<char *>(sdpContext.c_str()));


	//printf("QTSSReflectorModule:DoAnnounce SendResponse OK=200\n");

	inParams->inRTSPRequest->SendHeader();
	return QTSS_NoErr;
}

std::string DoDescribeAddRequiredSDPLines(QTSS_StandardRTSP_Params* inParams, ReflectorSession* theSession, 
	QTSS_TimeVal modDate, boost::string_view theSDP)
{
	std::string editedSDP;
	SDPContainer checkedSDPContainer;
	checkedSDPContainer.SetSDPBuffer(theSDP);
	if (!checkedSDPContainer.HasReqLines())
	{
		if (!checkedSDPContainer.HasLineType('v'))
		{ // add v line
			editedSDP += "v=0\r\n";
		}

		if (!checkedSDPContainer.HasLineType('s'))
		{
			// add s line
			boost::string_view theSDPName = ((RTSPRequest*)inParams->inRTSPRequest)->GetAbsolutePath();
			editedSDP += "s=";
			editedSDP += std::string(theSDPName);
			editedSDP += "\r\n";
		}

		if (!checkedSDPContainer.HasLineType('t'))
		{ // add t line
			editedSDP += "t=0 0\r\n";
		}

		if (!checkedSDPContainer.HasLineType('o'))
		{ // add o line
			editedSDP += "o=broadcast_sdp ";
			char tempBuff[256] = "";
			tempBuff[255] = 0;
			snprintf(tempBuff, sizeof(tempBuff) - 1, "%"   _U32BITARG_   "", *(uint32_t *)&theSession);
			editedSDP += tempBuff;

			editedSDP += " ";
			// modified date is in milliseconds.  Convert to NTP seconds as recommended by rfc 2327
			snprintf(tempBuff, sizeof(tempBuff) - 1, "%" _64BITARG_ "d", (int64_t)(modDate / 1000) + 2208988800LU);
			editedSDP += tempBuff;

			editedSDP += " IN IP4 ";
			editedSDP += std::string(inParams->inClientSession->GetHost());
			editedSDP += "\r\n";
		}
	}

	editedSDP += std::string(theSDP);
	return editedSDP;
}

QTSS_Error DoDescribe(QTSS_StandardRTSP_Params* inParams)
{
	uint32_t theRefCount = 0;
	std::string theFileName;
	ReflectorSession* theSession = DoSessionSetup(inParams, false, nullptr, &theFileName);

	if (theSession == nullptr)
		return QTSS_RequestFailed;

	theRefCount++;

	//	//redis,streamid/serial/channel.sdp,for example "./Movies/\streamid\serial\channel0.sdp"
	//	if(true)
	//	{
	//		//1.get the path
	//		char* theFileNameStr = NULL;
	//		QTSS_Error theErrEx = QTSS_GetValueAsString(inParams->inRTSPRequest, qtssRTSPReqLocalPath, 0, &theFileNameStr);
	//		Assert(theErrEx == QTSS_NoErr);
	//		std::unique_ptr<char[]> theFileNameStrDeleter(theFileNameStr);
	//
	//		//2.get SessionID
	//		char chStreamId[64]={0};
	//#ifdef __Win32__//it's different between linux and windows
	//
	//		char movieFolder[256] = { 0 };
	//		uint32_t thePathLen = 256;
	//		QTSServerInterface::GetServer()->GetPrefs()->GetMovieFolder(&movieFolder[0], &thePathLen);
	//		StringParser parser(&StrPtrLen(theFileNameStr));
	//		StrPtrLen strName;
	//		parser.ConsumeLength(NULL,thePathLen);
	//		parser.Expect('\\');
	//		parser.ConsumeUntil(&strName,'\\');
	//		memcpy(chStreamId,strName.Ptr,strName.Len);
	//#else 
	//
	//#endif
	//		//3.auth the streamid in redis
	//		char chResult = 0;
	//		QTSS_RoleParams theParams;
	//		theParams.JudgeStreamIDParams.inStreanID = chStreamId;
	//		theParams.JudgeStreamIDParams.outresult = &chResult;
	//
	//		uint32_t numModules = QTSServerInterface::GetNumModulesInRole(QTSSModule::kRedisJudgeStreamIDRole);
	//		for ( uint32_t currentModule=0;currentModule < numModules; currentModule++)
	//		{
	//			QTSSModule* theModule = QTSServerInterface::GetModule(QTSSModule::kRedisJudgeStreamIDRole, currentModule);
	//			(void)theModule->CallDispatch(Easy_RedisJudgeStreamID_Role, &theParams);
	//		}
	//		//if(chResult == 0)
	//		if(false)
	//		{
	//			sSessionMap->Release(theSession->GetRef());//don't forget
	//			return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssClientBadRequest,0);;
	//		}
	//		//auth sucessfully
	//	}
	//	//redis

	uint32_t theLen = 0;
	auto opt = inParams->inClientSession->getAttribute(sOutputName);

	// If there already  was an RTPSessionOutput attached to this Client Session,
	// destroy it. 
	if (opt)
	{
		RTPSessionOutput* theOutput = boost::any_cast<RTPSessionOutput*>(opt.value());
		RemoveOutput(theOutput, theOutput->GetReflectorSession(), false);
		inParams->inClientSession->removeAttribute(sOutputName);
	}
	// send the DESCRIBE response

	//above function has signalled that this request belongs to us, so let's respond
	iovec theDescribeVec[3] = { 0 };

	Assert(!theSession->GetLocalSDP().empty());


	StrPtrLen theFileData;
	QTSS_TimeVal outModDate = 0;
	QTSS_TimeVal inModDate = -1;
	(void)QTSSModuleUtils::ReadEntireFile((char *)theFileName.c_str(), &theFileData, inModDate, &outModDate);
	std::unique_ptr<char[]> fileDataDeleter(theFileData.Ptr);

	// -------------- process SDP to remove connection info and add track IDs, port info, and default c= line

	SDPSourceInfo tempSDPSourceInfo(theFileData.Ptr, theFileData.Len); // will make a copy and delete in destructor
	std::string theSDPData = tempSDPSourceInfo.GetLocalSDP(); // returns a new buffer with processed sdp

	if (theSDPData.empty()) // can't find it on disk or it failed to parse just use the one in the session.
		theSDPData = std::string(theSession->GetLocalSDP());


	// ------------  Clean up missing required SDP lines

	std::string editedSDP = DoDescribeAddRequiredSDPLines(inParams, theSession, outModDate, theSDPData);

	// ------------ Check the headers

	SDPContainer checkedSDPContainer;
	checkedSDPContainer.SetSDPBuffer(editedSDP);
	if (!checkedSDPContainer.IsSDPBufferValid())
	{
		if(theRefCount)
			sSessionMap->Release(theSession->GetRef());

		return QTSSModuleUtils::SendErrorResponseWithMessage(inParams->inRTSPRequest, qtssUnsupportedMediaType, &sSDPNotValidMessage);
	}


	// ------------ Put SDP header lines in correct order
	float adjustMediaBandwidthPercent = 1.0;
	bool adjustMediaBandwidth = false;

	if (sPlayerCompatibility)
		adjustMediaBandwidth = QTSSModuleUtils::HavePlayerProfile(sServerPrefs, inParams, QTSSModuleUtils::kAdjustBandwidth);

	if (adjustMediaBandwidth)
		adjustMediaBandwidthPercent = (float)sAdjustMediaBandwidthPercent / 100.0;

	SDPLineSorter sortedSDP(checkedSDPContainer, adjustMediaBandwidthPercent);

	// ------------ Write the SDP 

	uint32_t sessLen = sortedSDP.GetSessionHeaders().length();
	uint32_t mediaLen = sortedSDP.GetMediaHeaders().length();
	theDescribeVec[1].iov_base = const_cast<char *>(sortedSDP.GetSessionHeaders().data());
	theDescribeVec[1].iov_len = sortedSDP.GetSessionHeaders().length();

	theDescribeVec[2].iov_base = const_cast<char *>(sortedSDP.GetMediaHeaders().data());
	theDescribeVec[2].iov_len = sortedSDP.GetMediaHeaders().length();

	inParams->inRTSPRequest->AppendHeader(qtssCacheControlHeader,
		kCacheControlHeader);
	QTSSModuleUtils::SendDescribeResponse(inParams->inRTSPRequest, 
		&theDescribeVec[0], 3, sessLen + mediaLen);

	if (theRefCount)
		sSessionMap->Release(theSession->GetRef());

#ifdef REFLECTORSESSION_DEBUG
	printf("QTSSReflectorModule.cpp:DoDescribe Session =%p refcount=%"   _U32BITARG_   "\n", theSession->GetRef(), theSession->GetRef()->GetRefCount());
#endif

	return QTSS_NoErr;
}

bool InfoPortsOK(QTSS_StandardRTSP_Params* inParams, SDPSourceInfo* theInfo, boost::string_view inPath)
{
	// Check the ports based on the Pref whether to enforce a static SDP port range.
	bool isOK = true;

	if (sEnforceStaticSDPPortRange)
	{
		for (uint32_t x = 0; x < theInfo->GetNumStreams(); x++)
		{
			uint16_t theInfoPort = theInfo->GetStreamInfo(x)->fPort;
			QTSS_AttributeID theErrorMessageID = qtssIllegalAttrID;
			if (theInfoPort != 0)
			{
				if (theInfoPort < sMinimumStaticSDPPort)
					theErrorMessageID = sSDPcontainsInvalidMinimumPortErr;
				else if (theInfoPort > sMaximumStaticSDPPort)
					theErrorMessageID = sSDPcontainsInvalidMaximumPortErr;
			}

			if (theErrorMessageID != qtssIllegalAttrID)
			{
				std::string thePathPort = std::string(inPath) + ":" + std::to_string(theInfoPort);
				(void)QTSSModuleUtils::LogError(qtssWarningVerbosity, theErrorMessageID, 0, (char *)thePathPort.c_str());

				StrPtrLen thePortStr((char *)thePathPort.c_str());
				(void)QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssUnsupportedMediaType, theErrorMessageID, &thePortStr);

				return false;
			}
		}
	}

	return isOK;
}

ReflectorSession* FindOrCreateSession(boost::string_view inName, QTSS_StandardRTSP_Params* inParams, uint32_t inChannel, StrPtrLen* inData, bool isPush, bool *foundSessionPtr)
{
	OSMutexLocker locker(sSessionMap->GetMutex());

	std::string theStreamName = buildStreamName(inName, inChannel);

	StrPtrLen inPath((char *)theStreamName.c_str());

	OSRef* theSessionRef = sSessionMap->Resolve(&inPath);
	ReflectorSession* theSession = nullptr;

	if (theSessionRef == nullptr)
	{
		if (!isPush)
		{
			return nullptr;
		}

		StrPtrLen theFileData;
		StrPtrLen theFileDeleteData;

		if (inData == nullptr)
		{
			(void)QTSSModuleUtils::ReadEntireFile(inPath.Ptr, &theFileDeleteData);
			theFileData = theFileDeleteData;
		}
		else
		{
			theFileData = *inData;
		}
		std::unique_ptr<char[]> fileDataDeleter(theFileDeleteData.Ptr);

		if (theFileData.Len <= 0)
			return nullptr;

		auto* theInfo = new SDPSourceInfo(theFileData.Ptr, theFileData.Len); // will make a copy

		if (!theInfo->IsReflectable())
		{
			delete theInfo;
			return nullptr;
		}

		boost::string_view inPathV(inPath.Ptr, inPath.Len);
		if (!InfoPortsOK(inParams, theInfo, inPathV))
		{
			delete theInfo;
			return nullptr;
		}

		// Check if broadcast is allowed before doing anything else
		// At this point we know it is a definitely a reflector session
		// It is either incoming automatic broadcast setup or a client setup to view broadcast
		// In either case, verify whether the broadcast is allowed, and send forbidden response back
		if (!sReflectBroadcasts)
		{
			(void)QTSSModuleUtils::SendErrorResponseWithMessage(inParams->inRTSPRequest, qtssClientForbidden, &sBroadcastNotAllowed);
			return nullptr;
		}

		//
		// Setup a ReflectorSession and bind the sockets. If we are negotiating,
		// make sure to let the session know that this is a Push Session so
		// ports may be modified.
		uint32_t theSetupFlag = ReflectorSession::kMarkSetup;
		if (isPush)
			theSetupFlag |= ReflectorSession::kIsPushSession;

		theSession = new ReflectorSession(inName, inChannel);
		if (theSession == nullptr)
		{
			return nullptr;
		}

		theSession->SetHasBufferedStreams(true); // buffer the incoming streams for clients

		// SetupReflectorSession stores theInfo in theSession so DONT delete the Info if we fail here, leave it alone.
		// deleting the session will delete the info.
		QTSS_Error theErr = theSession->SetupReflectorSession(theInfo, inParams, theSetupFlag, sOneSSRCPerStream, sTimeoutSSRCSecs);
		if (theErr != QTSS_NoErr)
		{
			//delete theSession;
			CSdpCache::GetInstance()->eraseSdpMap(theSession->GetSourceID()->Ptr);
			theSession->DelRedisLive();
			theSession->StopTimer();
			return nullptr;
		}

		//printf("Created reflector session = %"   _U32BITARG_   " theInfo=%"   _U32BITARG_   " \n", (uint32_t) theSession,(uint32_t)theInfo);
		//put the session's ID into the session map.
		theErr = sSessionMap->Register(theSession->GetRef());
		Assert(theErr == QTSS_NoErr);

		// unless we do this, the refcount won't increment (and we'll delete the session prematurely
		//if (!isPush)
		{
			OSRef* debug = sSessionMap->Resolve(&inPath);
			Assert(debug == theSession->GetRef());
		}
	}
	else
	{
#ifdef REFLECTORSESSION_DEBUG
		printf("QTSSReflectorModule.cpp:FindOrCreateSession Session =%p refcount=%"   _U32BITARG_   "\n", theSessionRef, theSessionRef->GetRefCount());
#endif
		// Check if broadcast is allowed before doing anything else
		// At this point we know it is a definitely a reflector session
		// It is either incoming automatic broadcast setup or a client setup to view broadcast
		// In either case, verify whether the broadcast is allowed, and send forbidden response
		// back
		do
		{
			if (!sReflectBroadcasts)
			{
				(void)QTSSModuleUtils::SendErrorResponseWithMessage(inParams->inRTSPRequest, qtssClientForbidden, &sBroadcastNotAllowed);
				break;
			}

			if (foundSessionPtr)
				*foundSessionPtr = true;

			StrPtrLen theFileData;

			if (inData == nullptr)
				(void)QTSSModuleUtils::ReadEntireFile(inPath.Ptr, &theFileData);
			std::unique_ptr<char[]> charArrayDeleter(theFileData.Ptr);

			if (theFileData.Len <= 0)
				break;

			auto* theInfo = new SDPSourceInfo(theFileData.Ptr, theFileData.Len);
			if (theInfo == nullptr)
				break;

			boost::string_view inPathV(inPath.Ptr, inPath.Len);
			if (!InfoPortsOK(inParams, theInfo, inPathV))
			{
				delete theInfo;
				break;
			}

			delete theInfo;

			theSession = (ReflectorSession*)theSessionRef->GetObject();
			if (isPush && theSession && !(theSession->IsSetup()))
			{
				uint32_t theSetupFlag = ReflectorSession::kMarkSetup | ReflectorSession::kIsPushSession;
				QTSS_Error theErr = theSession->SetupReflectorSession(nullptr, inParams, theSetupFlag);
				if (theErr != QTSS_NoErr)
				{
					theSession =  nullptr;
					break;
				}
			}			;
		} while (0);

		if (theSession == nullptr)
			sSessionMap->Release(theSessionRef);
	}

	Assert(theSession != nullptr);

	// Turn off overbuffering if the "disable_overbuffering" pref says so
	if (sDisableOverbuffering)
		inParams->inClientSession->SetOverBufferEnable(sFalse);

	return theSession;
}

// ONLY call when performing a setup.
void DeleteReflectorPushSession(QTSS_StandardRTSP_Params* inParams, ReflectorSession* theSession, bool foundSession)
{
	if(theSession)
		sSessionMap->Release(theSession->GetRef());

	inParams->inClientSession->removeAttribute(sBroadcasterSessionName);

	if (foundSession)
		return; // we didn't allocate the session so don't delete

	OSRef* theSessionRef = theSession->GetRef();
	if (theSessionRef != nullptr)
	{
		theSession->TearDownAllOutputs(); // just to be sure because we are about to delete the session.
		sSessionMap->UnRegister(theSessionRef);// we had an error while setting up-- don't let anyone get the session
		//delete theSession;
		CSdpCache::GetInstance()->eraseSdpMap(theSession->GetSourceID()->Ptr);
		theSession->DelRedisLive();
		theSession->StopTimer();
	}
}

QTSS_Error AddRTPStream(ReflectorSession* theSession, QTSS_StandardRTSP_Params* inParams, RTPStream **newStreamPtr)
{
	// Ok, this is completely crazy but I can't think of a better way to do this that's
	// safe so we'll do it this way for now. Because the ReflectorStreams use this session's
	// stream queue, we need to make sure that each ReflectorStream is not reflecting to this
	// session while we call QTSS_AddRTPStream. One brutal way to do this is to grab each
	// ReflectorStream's mutex, which will stop every reflector stream from running.
	Assert(newStreamPtr != nullptr);

	if (theSession != nullptr)
		for (uint32_t x = 0; x < theSession->GetNumStreams(); x++)
			theSession->GetStreamByIndex(x)->GetMutex()->Lock();

	//
	// Turn off reliable UDP transport, because we are not yet equipped to
	// do overbuffering.
	QTSS_Error theErr = inParams->inClientSession->AddStream(
		inParams->inRTSPRequest, newStreamPtr, qtssASFlagsForceUDPTransport);

	if (theSession != nullptr)
		for (uint32_t y = 0; y < theSession->GetNumStreams(); y++)
			theSession->GetStreamByIndex(y)->GetMutex()->Unlock();

	return theErr;
}

static void SendSetupRTSPResponse(RTPStream *inRTPInfo, RTSPRequestInterface *inRTSPRequest, uint32_t inFlags)
{
	if (inFlags & qtssSetupRespDontWriteSSRC)
		inRTPInfo->DisableSSRC();
	else
		inRTPInfo->EnableSSRC();

	inRTPInfo->SendSetupResponse(inRTSPRequest);
}

QTSS_Error DoSetup(QTSS_StandardRTSP_Params* inParams)
{
	ReflectorSession* theSession = nullptr;

	uint32_t theLen = 0;
	bool isPush = inParams->inRTSPRequest->IsPushRequest();
	bool foundSession = false;

	auto opt = inParams->inClientSession->getAttribute(sOutputName);
	if (!opt)
	{
		//theLen = sizeof(theSession);
		//theErr = QTSS_GetValue(inParams->inClientSession, sSessionAttr, 0, &theSession, &theLen);

		if (!isPush)
		{
			theSession = DoSessionSetup(inParams);
			if (theSession == nullptr)
				return QTSS_RequestFailed;

			auto* theNewOutput = new RTPSessionOutput(inParams->inClientSession, theSession, sServerPrefs, sStreamCookieName);
			theSession->AddOutput(theNewOutput, true);
			inParams->inClientSession->addAttribute(sOutputName, theNewOutput);
		}
		else
		{
			auto opt = inParams->inClientSession->getAttribute(sBroadcasterSessionName);
			if (!opt)
			{
				theSession = DoSessionSetup(inParams, isPush, &foundSession);
				if (theSession == nullptr)
					return QTSS_RequestFailed;
			}
			else
			{
				theSession = boost::any_cast<ReflectorSession*>(opt.value());
			}

			inParams->inClientSession->addAttribute(sBroadcasterSessionName, theSession);

			//printf("QTSSReflectorModule.cpp:SET Session sClientBroadcastSessionAttr=%"   _U32BITARG_   " theSession=%"   _U32BITARG_   " err=%" _S32BITARG_ " \n",(uint32_t)sClientBroadcastSessionAttr, (uint32_t) theSession,theErr);
			inParams->inClientSession->ResetTimeout(sBroadcasterSessionTimeoutMilliSecs);
		}
	}
	else
	{
		RTPSessionOutput*  theOutput = boost::any_cast<RTPSessionOutput*>(opt.value());
		theSession = theOutput->GetReflectorSession();
		if (theSession == nullptr)
			return QTSS_RequestFailed;
	}

	//unless there is a digit at the end of this path (representing trackID), don't
	//even bother with the request
	std::string theDigitStr = inParams->inRTSPRequest->GetFileDigit();
	if (theDigitStr.empty())
	{
		if (isPush)
			DeleteReflectorPushSession(inParams, theSession, foundSession);
		return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssClientBadRequest, sExpectedDigitFilenameErr);
	}

	uint32_t theTrackID = std::stoi(theDigitStr);

	if (isPush)
	{
		//printf("QTSSReflectorModule.cpp:DoSetup is push setup\n");

		// Get info about this trackID
		SourceInfo::StreamInfo* theStreamInfo = theSession->GetSourceInfo()->GetStreamInfoByTrackID(theTrackID);
		// If theStreamInfo is NULL, we don't have a legit track, so return an error
		if (theStreamInfo == nullptr)
		{
			DeleteReflectorPushSession(inParams, theSession, foundSession);
			return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssClientBadRequest,
				sReflectorBadTrackIDErr);
		}

		if (!sAllowDuplicateBroadcasts && theStreamInfo->fSetupToReceive)
		{
			DeleteReflectorPushSession(inParams, theSession, foundSession);
			return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssPreconditionFailed, sDuplicateBroadcastStreamErr);
		}

		inParams->inRTSPRequest->SetUpServerPort(theStreamInfo->fPort);

		RTPStream *newStream = nullptr;
		QTSS_Error theErr = AddRTPStream(theSession, inParams, &newStream);
		Assert(theErr == QTSS_NoErr);
		if (theErr != QTSS_NoErr)
		{
			DeleteReflectorPushSession(inParams, theSession, foundSession);
			return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssClientBadRequest, 0);
		}

		//send the setup response

		inParams->inRTSPRequest->AppendHeader(qtssCacheControlHeader,
			kCacheControlHeader);

		SendSetupRTSPResponse(newStream, inParams->inRTSPRequest, 0);

		theStreamInfo->fSetupToReceive = true;
		// This is an incoming data session. Set the Reflector Session in the ClientSession
		inParams->inClientSession->addAttribute(sBroadcasterSessionName, theSession);

		if (theSession != nullptr)
			theSession->AddBroadcasterClientSession(inParams);

#ifdef REFLECTORSESSION_DEBUG
		printf("QTSSReflectorModule.cpp:DoSetup Session =%p refcount=%"   _U32BITARG_   "\n", theSession->GetRef(), theSession->GetRef()->GetRefCount());
#endif

		return QTSS_NoErr;
	}


	// Get info about this trackID
	SourceInfo::StreamInfo* theStreamInfo = theSession->GetSourceInfo()->GetStreamInfoByTrackID(theTrackID);
	// If theStreamInfo is NULL, we don't have a legit track, so return an error
	if (theStreamInfo == nullptr)
		return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssClientBadRequest,
			sReflectorBadTrackIDErr);

	boost::string_view thePayloadName = theStreamInfo->fPayloadName;
	bool r = qi::phrase_parse(thePayloadName.cbegin(), thePayloadName.cend(),
		qi::omit[+(qi::char_ - "/")] >> "/" >> qi::uint_, qi::ascii::blank, theStreamInfo->fTimeScale);

	if (theStreamInfo->fTimeScale == 0)
		theStreamInfo->fTimeScale = 90000;

	RTPStream *newStream = nullptr;
	{
		// Ok, this is completely crazy but I can't think of a better way to do this that's
		// safe so we'll do it this way for now. Because the ReflectorStreams use this session's
		// stream queue, we need to make sure that each ReflectorStream is not reflecting to this
		// session while we call QTSS_AddRTPStream. One brutal way to do this is to grab each
		// ReflectorStream's mutex, which will stop every reflector stream from running.

		for (uint32_t x = 0; x < theSession->GetNumStreams(); x++)
			theSession->GetStreamByIndex(x)->GetMutex()->Lock();

		QTSS_Error theErr = inParams->inClientSession->AddStream(
			inParams->inRTSPRequest, &newStream, 0);

		for (uint32_t y = 0; y < theSession->GetNumStreams(); y++)
			theSession->GetStreamByIndex(y)->GetMutex()->Unlock();

		if (theErr != QTSS_NoErr)
			return theErr;
	}

	// Set up items for this stream
	newStream->SetPayloadName(thePayloadName);
	newStream->SetPayLoadType(theStreamInfo->fPayloadType);
	newStream->SetSDPStreamID(theTrackID);
	newStream->SetTimeScale(theStreamInfo->fTimeScale);

	// We only want to allow over buffering to dynamic rate clients   
	int32_t canDynamicRate = inParams->inRTSPRequest->GetDynamicRateState();
	if (canDynamicRate < 1) // -1 no rate field, 0 off
		inParams->inClientSession->SetOverBufferEnable(sFalse);

	// Place the stream cookie in this stream for future reference
	void* theStreamCookie = theSession->GetStreamCookie(theTrackID);
	Assert(theStreamCookie != nullptr);
	newStream->addAttribute(sStreamCookieName, theStreamCookie);

	// Set the number of quality levels.
	newStream->SetNumQualityLevels(ReflectorSession::kNumQualityLevels);

	//send the setup response
	inParams->inRTSPRequest->AppendHeader(qtssCacheControlHeader,
		kCacheControlHeader);
	SendSetupRTSPResponse(newStream, inParams->inRTSPRequest, qtssSetupRespDontWriteSSRC);

#ifdef REFLECTORSESSION_DEBUG
	printf("QTSSReflectorModule.cpp:DoSetup Session =%p refcount=%"   _U32BITARG_   "\n", theSession->GetRef(), theSession->GetRef()->GetRefCount());
#endif

	return QTSS_NoErr;
}



bool HaveStreamBuffers(QTSS_StandardRTSP_Params* inParams, ReflectorSession* inSession)
{
	if (inSession == nullptr || inParams == nullptr)
		return false;

	bool haveBufferedStreams = true; // set to false and return if we can't set the packets
	uint32_t y = 0;

	int64_t packetArrivalTime = 0;

	//lock all streams
	for (y = 0; y < inSession->GetNumStreams(); y++)
		inSession->GetStreamByIndex(y)->GetMutex()->Lock();



	auto vecMap = inParams->inClientSession->GetStreams();
	for (int i = 0; i < vecMap.size(); i++)
	{
		RTPStream* theRef = vecMap[i];
		ReflectorStream* theReflectorStream = inSession->GetStreamByIndex(i);

		//  if (!theReflectorStream->HasFirstRTCP())
		//      printf("theStreamIndex =%"   _U32BITARG_   " no rtcp\n", theStreamIndex);

		//  if (!theReflectorStream->HasFirstRTP())
		//      printf("theStreamIndex = %"   _U32BITARG_   " no rtp\n", theStreamIndex);

		if ((theReflectorStream == nullptr) || (false == theReflectorStream->HasFirstRTP()))
		{
			haveBufferedStreams = false;
			//printf("1 breaking no buffered streams\n");
			break;
		}

		uint16_t firstSeqNum = 0;
		uint32_t firstTimeStamp = 0;
		ReflectorSender* theSender = theReflectorStream->GetRTPSender();
		haveBufferedStreams = theSender->GetFirstPacketInfo(&firstSeqNum, &firstTimeStamp, &packetArrivalTime);
		//printf("theStreamIndex= %"   _U32BITARG_   " haveBufferedStreams=%d, seqnum=%d, timestamp=%"   _U32BITARG_   "\n", theStreamIndex, haveBufferedStreams, firstSeqNum, firstTimeStamp);

		if (!haveBufferedStreams)
		{
			//printf("2 breaking no buffered streams\n");
			break;
		}

		theRef->SetSeqNumber(firstSeqNum);
		theRef->SetTimeStamp(firstTimeStamp);
	}
	//unlock all streams
	for (y = 0; y < inSession->GetNumStreams(); y++)
		inSession->GetStreamByIndex(y)->GetMutex()->Unlock();

	return haveBufferedStreams;
}

QTSS_Error DoPlay(QTSS_StandardRTSP_Params* inParams, ReflectorSession* inSession)
{
	QTSS_Error theErr = QTSS_NoErr;
	uint32_t flags = 0;
	uint32_t theLen = 0;
	bool rtpInfoEnabled = false;

	if (inSession == nullptr)
	{
		if (!sDefaultBroadcastPushEnabled)
			return QTSS_RequestFailed;

		theLen = sizeof(inSession);
		auto opt = inParams->inClientSession->getAttribute(sBroadcasterSessionName);
		if (!opt) return QTSS_RequestFailed;
		inSession = boost::any_cast<ReflectorSession*>(opt.value());

		inParams->inClientSession->addAttribute(sKillClientsEnabledName, sTearDownClientsOnDisconnect);


		Assert(inSession != nullptr);

		inParams->inRTSPSession->addAttribute(sBroadcasterSessionName, inSession);
		KeepSession(inParams->inRTSPRequest, true);
		//printf("QTSSReflectorModule.cpp:DoPlay (PUSH) inRTSPSession=%"   _U32BITARG_   " inClientSession=%"   _U32BITARG_   "\n",(uint32_t)inParams->inRTSPSession,(uint32_t)inParams->inClientSession);
	}
	else
	{

		theLen = 0;
		auto opt = inParams->inClientSession->getAttribute(sOutputName);
		if (!opt)
			return QTSS_RequestFailed;

		RTPSessionOutput*  theOutput = boost::any_cast<RTPSessionOutput*>(opt.value());
		theOutput->InitializeStreams();

		// Tell the session what the bitrate of this reflection is. This is nice for logging,
		// it also allows the server to scale the TCP buffer size appropriately if we are
		// interleaving the data over TCP. This must be set before calling QTSS_Play so the
		// server can use it from within QTSS_Play
		uint32_t bitsPerSecond = inSession->GetBitRate();
		inParams->inClientSession->SetMovieAvgBitrate(bitsPerSecond);

		if (sPlayResponseRangeHeader)
		{
			auto opt = inParams->inClientSession->getAttribute(sRTPInfoWaitTime);
			if (!opt)
				inParams->inRTSPRequest->AppendHeader(qtssRangeHeader, sTheNowRangeHeader);
		}

		if (sPlayerCompatibility)
			rtpInfoEnabled = QTSSModuleUtils::HavePlayerProfile(sServerPrefs, inParams, QTSSModuleUtils::kRequiresRTPInfoSeqAndTime);

		if (sForceRTPInfoSeqAndTime)
			rtpInfoEnabled = true;

		if (sRTPInfoDisabled)
			rtpInfoEnabled = false;

		if (rtpInfoEnabled)
		{
			flags = qtssPlayRespWriteTrackInfo; //write first timestampe and seq num to rtpinfo

			bool haveBufferedStreams = HaveStreamBuffers(inParams, inSession);
			if (haveBufferedStreams) // send the cached rtp time and seq number in the response.
			{

				theErr = inParams->inClientSession->Play(
					inParams->inRTSPRequest, qtssPlayRespWriteTrackInfo);
				if (theErr != QTSS_NoErr)
					return theErr;
			}
			else
			{
				auto opt = inParams->inClientSession->getAttribute(sRTPInfoWaitTime);
				if (!opt)
					inParams->inClientSession->addAttribute(sRTPInfoWaitTime, (int32_t)0);
				else
				{
					int32_t waitTimeLoopCount = boost::any_cast<int32_t>(opt.value());
					if (waitTimeLoopCount < 1)
						return QTSSModuleUtils::SendErrorResponseWithMessage(inParams->inRTSPRequest, qtssClientNotFound, &sBroadcastNotActive);

					inParams->inClientSession->addAttribute(sRTPInfoWaitTime, waitTimeLoopCount - 1);
				}

				//printf("QTSSReflectorModule:DoPlay  wait 100ms waitTimeLoopCount=%ld\n", waitTimeLoopCount);
				int64_t interval = 1 * 100; // 100 millisecond
				QTSS_SetIdleTimer(interval);
				return QTSS_NoErr;
			}
		}
		else
		{
			theErr = inParams->inClientSession->Play(
				(RTSPRequestInterface*)inParams->inRTSPRequest, qtssPlayFlagsAppendServerInfo);
			if (theErr != QTSS_NoErr)
				return theErr;

		}

	}

	inParams->inClientSession->SendPlayResponse((RTSPRequestInterface*)inParams->inRTSPRequest, flags);

#ifdef REFLECTORSESSION_DEBUG
	printf("QTSSReflectorModule.cpp:DoPlay Session =%p refcount=%"   _U32BITARG_   "\n", inSession->GetRef(), inSession->GetRef()->GetRefCount());
#endif

	return QTSS_NoErr;
}


bool KillSession(boost::string_view sdpPathStr, bool killClients)
{
	StrPtrLen v1((char *)sdpPathStr.data(), sdpPathStr.length());
	OSRef* theSessionRef = sSessionMap->Resolve(&v1);
	if (theSessionRef != nullptr)
	{
		auto*   theSession = (ReflectorSession*)theSessionRef->GetObject();
		RemoveOutput(nullptr, theSession, killClients);
		theSession->GetBroadcasterSession()->Teardown();
		return true;
	}
	return false;
}


void KillCommandPathInList()
{
	char filePath[128] = "";
	ResizeableStringFormatter commandPath((char*)filePath, sizeof(filePath)); // ResizeableStringFormatter is safer and more efficient than StringFormatter for most paths.
	OSMutexLocker locker(sSessionMap->GetMutex());

	for (OSRefHashTableIter theIter(sSessionMap->GetHashTable()); !theIter.IsDone(); theIter.Next())
	{
		OSRef* theRef = theIter.GetCurrent();
		if (theRef == nullptr)
			continue;

		commandPath.Reset();
		commandPath.Put(*(theRef->GetString()));
		commandPath.Put(sSDPKillSuffix);
		commandPath.PutTerminator();

		char *theCommandPath = commandPath.GetBufPtr();
		std::unique_ptr<QTSSFile> outFileObject(new QTSSFile);
		QTSS_Error  err = outFileObject->Open(theCommandPath, qtssOpenFileNoFlags);
		if (err == QTSS_NoErr)
		{
			outFileObject->Close();
			::unlink(theCommandPath);
			StrPtrLen *p1 = theRef->GetString();
			boost::string_view t1(p1->Ptr, p1->Len);
			KillSession(t1, true);
		}
	}

}

QTSS_Error DestroySession(QTSS_ClientSessionClosing_Params* inParams)
{
	RTPSessionOutput**  theOutput = nullptr;
	ReflectorOutput*    outputPtr = nullptr;
	ReflectorSession*   theSession = nullptr;

	OSMutexLocker locker(sSessionMap->GetMutex());

	auto opt = inParams->inClientSession->getAttribute(sBroadcasterSessionName);
	//printf("QTSSReflectorModule.cpp:DestroySession    sClientBroadcastSessionAttr=%"   _U32BITARG_   " theSession=%"   _U32BITARG_   " err=%" _S32BITARG_ " \n",(uint32_t)sClientBroadcastSessionAttr, (uint32_t)theSession,theErr);

	if (opt)
	{
		inParams->inClientSession->removeAttribute(sBroadcasterSessionName);

		SourceInfo* theSoureInfo = theSession->GetSourceInfo();
		if (theSoureInfo == nullptr)
			return QTSS_NoErr;

		uint32_t  numStreams = theSession->GetNumStreams();
		SourceInfo::StreamInfo* theStreamInfo = nullptr;

		for (uint32_t index = 0; index < numStreams; index++)
		{
			theStreamInfo = theSoureInfo->GetStreamInfo(index);
			if (theStreamInfo != nullptr)
				theStreamInfo->fSetupToReceive = false;
		}

		auto opt = inParams->inClientSession->getAttribute(sKillClientsEnabledName);
		bool killClients = boost::any_cast<bool>(opt.value()); // the pref as the default
		//printf("QTSSReflectorModule.cpp:DestroySession broadcaster theSession=%"   _U32BITARG_   "\n", (uint32_t) theSession);
		theSession->RemoveSessionFromOutput(inParams->inClientSession);

		RemoveOutput(nullptr, theSession, killClients);
	}
	else // 客户端
	{
		auto opt = inParams->inClientSession->getAttribute(sOutputName);
		if (!opt)
			return QTSS_RequestFailed;
		RTPSessionOutput* theOutput = boost::any_cast<RTPSessionOutput*>(opt.value());
		theSession = theOutput->GetReflectorSession();

		if (theOutput != nullptr)
			outputPtr = theOutput;

		if (outputPtr != nullptr)
		{
			RemoveOutput(outputPtr, theSession, false);
			inParams->inClientSession->removeAttribute(sOutputName);
		}

	}

	return QTSS_NoErr;
}

void RemoveOutput(ReflectorOutput* inOutput, ReflectorSession* theSession, bool killClients)
{
	// 对ReflectorSession的引用继续处理,包括推送端和客户端
	Assert(theSession);
	if (theSession != nullptr)
	{
		if (inOutput != nullptr)
		{
			// ReflectorSession移除客户端
			theSession->RemoveOutput(inOutput, true);
		}
		else
		{
			// 推送端
			SourceInfo* theInfo = theSession->GetSourceInfo();
			Assert(theInfo);

			//if (theInfo->IsRTSPControlled())
			//{   
			//    FileDeleter(theSession->GetSourceID());
			//}
			//    

			if (killClients || sTearDownClientsOnDisconnect)
			{
				theSession->TearDownAllOutputs();
			}
		}
		// 检测推送端或者客户端退出时,ReflectorSession是否需要退出
		OSRef* theSessionRef = theSession->GetRef();
		if (theSessionRef != nullptr)
		{
			//printf("QTSSReflectorModule.cpp:RemoveOutput UnRegister session =%p refcount=%"   _U32BITARG_   "\n", theSessionRef, theSessionRef->GetRefCount() ) ;       
			if (inOutput != nullptr)
			{
				if (theSessionRef->GetRefCount() > 0)
					sSessionMap->Release(theSessionRef);
			}
			else
			{
				if (theSessionRef->GetRefCount() > 0)
					sSessionMap->Release(theSessionRef);
			}

#ifdef REFLECTORSESSION_DEBUG
			printf("QTSSReflectorModule.cpp:RemoveOutput Session =%p refcount=%"   _U32BITARG_   "\n", theSession->GetRef(), theSession->GetRef()->GetRefCount());
#endif
			if (theSessionRef->GetRefCount() == 0)
			{

#ifdef REFLECTORSESSION_DEBUG
				printf("QTSSReflectorModule.cpp:RemoveOutput UnRegister and delete session =%p refcount=%"   _U32BITARG_   "\n", theSessionRef, theSessionRef->GetRefCount());
#endif
				sSessionMap->UnRegister(theSessionRef);
				//delete theSession;
				CSdpCache::GetInstance()->eraseSdpMap(theSession->GetSourceID()->Ptr);
				theSession->DelRedisLive();

				theSession->StopTimer();
			}
		}
	}
	delete inOutput;
}


bool AcceptSession(QTSS_StandardRTSP_Params* inParams)
{
	RTSPSession* inRTSPSession = inParams->inRTSPSession;
	RTSPRequest* theRTSPRequest = inParams->inRTSPRequest;

	QTSS_ActionFlags action = theRTSPRequest->GetAction();
	if (action != qtssActionFlagsWrite)
		return false;

	if (QTSSModuleUtils::UserInGroup(theRTSPRequest->GetUserProfile(), sBroadcasterGroup.Ptr, sBroadcasterGroup.Len))
		return true; // ok we are allowing this broadcaster user

	boost::string_view remoteAddress = inRTSPSession->GetRemoteAddr();
	StrPtrLen theClientIPAddressStr((char *)remoteAddress.data(), remoteAddress.length());

	if (IPComponentStr(&theClientIPAddressStr).IsLocal())
	{
		if (sAuthenticateLocalBroadcast)
			return false;
		else
			return true;
	}

	if (QTSSModuleUtils::AddressInList(sPrefs, sIPAllowListID, &theClientIPAddressStr))
		return true;

	return false;
}

QTSS_Error ReflectorAuthorizeRTSPRequest(QTSS_StandardRTSP_Params* inParams)
{
	if (AcceptSession(inParams))
	{
		bool allowed = true;
		RTSPRequest* request = inParams->inRTSPRequest;
		(void)QTSSModuleUtils::AuthorizeRequest(request, &allowed, &allowed, &allowed);
		return QTSS_NoErr;
	}

	bool allowNoAccessFiles = false;
	QTSS_ActionFlags noAction = ~qtssActionFlagsWrite; //no action anything but a write
	QTSS_ActionFlags authorizeAction = ((RTSPRequest*)inParams->inRTSPRequest)->GetAction();
	//printf("ReflectorAuthorizeRTSPRequest authorizeAction=%d qtssActionFlagsWrite=%d\n", authorizeAction, qtssActionFlagsWrite);
	bool outAllowAnyUser = false;
	bool outAuthorized = false;
	QTAccessFile accessFile;
	accessFile.AuthorizeRequest(inParams, allowNoAccessFiles, noAction, authorizeAction, &outAuthorized, &outAllowAnyUser);

	if ((outAuthorized == false) && (authorizeAction & qtssActionFlagsWrite)) //handle it
	{
		//printf("ReflectorAuthorizeRTSPRequest SET not allowed\n");
		bool allowed = false;
		(void)QTSSModuleUtils::AuthorizeRequest(inParams->inRTSPRequest, &allowed, &allowed, &allowed);
	}
	return QTSS_NoErr;
}

QTSS_Error RedirectBroadcast(QTSS_StandardRTSP_Params* inParams)
{
	RTSPRequest* theRequest = inParams->inRTSPRequest;

	boost::string_view requestPathStr = theRequest->GetAbsolutePath();
	std::string theFirstPath, theStrippedRequestPath;
	bool r = qi::phrase_parse(requestPathStr.cbegin(), requestPathStr.cend(),
		"/" >> +(qi::char_ - "/") >> +(qi::char_), qi::ascii::blank, theFirstPath, theStrippedRequestPath);

	Assert(r && !theFirstPath.empty());

	// If the redirect_broadcast_keyword and redirect_broadcast_dir prefs are set & the first part of the path matches the keyword
	if (!sBroadcastsRedirectDir.empty()	&& boost::iequals(theFirstPath, sRedirectBroadcastsKeyword))
	{
		// set qtssRTSPReqRootDir
		theRequest->SetRootDir(sBroadcastsRedirectDir);
		theRequest->SetAbsolutePath(theStrippedRequestPath);
	}

	return QTSS_NoErr;
}

QTSS_Error GetDeviceStream(Easy_GetDeviceStream_Params* inParams)
{
	QTSS_Error theErr = QTSS_Unimplemented;

	if (inParams->inDevice && inParams->inStreamType == easyRTSPType)
	{

		OSMutexLocker locker(sSessionMap->GetMutex());

		std::string inDeviceName(inParams->inDevice);
		std::string theStreamName = buildStreamName(inDeviceName, inParams->inChannel);

		StrPtrLen inPath((char *)theStreamName.c_str());

		OSRef* theSessionRef = sSessionMap->Resolve(&inPath);
		ReflectorSession* theSession = nullptr;

		if (theSessionRef)
		{
			theSession = (ReflectorSession*)theSessionRef->GetObject();
			RTPSession* clientSession = theSession->GetBroadcasterSession();
			Assert(theSession != nullptr);

			if (clientSession)
			{
				std::string theFullRequestURL(clientSession->GetAbsoluteURL());

				if (!theFullRequestURL.empty() && inParams->outUrl)
				{
					strcpy(inParams->outUrl, theFullRequestURL.c_str());
					theErr = QTSS_NoErr;
				}
			}
			sSessionMap->Release(theSessionRef);
		}
	}

	return theErr;
}