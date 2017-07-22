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

#include "QTSServerInterface.h"
#include "QTSSReflectorModule.h"
#include "QTSSModuleUtils.h"
#include "ReflectorSession.h"
#include "OSArrayObjectDeleter.h"
#include "QTSSMemoryDeleter.h"
#include "OSRef.h"
#include "OS.h"
#include "ResizeableStringFormatter.h"
#include "StringParser.h"
#include "QTAccessFile.h"

#include "RTPSessionOutput.h"
#include "SDPSourceInfo.h"

#include "SDPUtils.h"
#include "sdpCache.h"

#include "QueryParamList.h"
#include "EasyUtil.h"

using namespace std;

#ifndef __Win32__
#include <unistd.h>
#endif

#define REFLECTORSESSION_DEBUG 1

#if DEBUG
#define REFLECTOR_MODULE_DEBUGGING 0
#else
#define REFLECTOR_MODULE_DEBUGGING 0
#endif

// ATTRIBUTES
static QTSS_AttributeID         sOutputAttr = qtssIllegalAttrID;
static QTSS_AttributeID         sSessionAttr = qtssIllegalAttrID;
static QTSS_AttributeID         sStreamCookieAttr = qtssIllegalAttrID;
static QTSS_AttributeID         sRequestBodyAttr = qtssIllegalAttrID;
static QTSS_AttributeID         sBufferOffsetAttr = qtssIllegalAttrID;
static QTSS_AttributeID         sExpectedDigitFilenameErr = qtssIllegalAttrID;
static QTSS_AttributeID         sReflectorBadTrackIDErr = qtssIllegalAttrID;
static QTSS_AttributeID         sDuplicateBroadcastStreamErr = qtssIllegalAttrID;
static QTSS_AttributeID         sClientBroadcastSessionAttr = qtssIllegalAttrID;
static QTSS_AttributeID         sRTSPBroadcastSessionAttr = qtssIllegalAttrID;
static QTSS_AttributeID         sAnnounceRequiresSDPinNameErr = qtssIllegalAttrID;
static QTSS_AttributeID         sAnnounceDisabledNameErr = qtssIllegalAttrID;
static QTSS_AttributeID         sSDPcontainsInvalidMinimumPortErr = qtssIllegalAttrID;
static QTSS_AttributeID         sSDPcontainsInvalidMaximumPortErr = qtssIllegalAttrID;
static QTSS_AttributeID         sStaticPortsConflictErr = qtssIllegalAttrID;
static QTSS_AttributeID         sInvalidPortRangeErr = qtssIllegalAttrID;

static QTSS_AttributeID         sKillClientsEnabledAttr = qtssIllegalAttrID;
static QTSS_AttributeID         sRTPInfoWaitTimeAttr = qtssIllegalAttrID;

// STATIC DATA

// ref to the prefs dictionary object
static OSRefTable*      sSessionMap = NULL;
static const StrPtrLen  kCacheControlHeader("no-cache");
static QTSS_PrefsObject sServerPrefs = NULL;
static QTSS_ServerObject sServer = NULL;
static QTSS_ModulePrefsObject sPrefs = NULL;

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
static char*            sIPAllowList = NULL;
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

static char*	sRedirectBroadcastsKeyword = NULL;
static char*    sDefaultRedirectBroadcastsKeyword = "";
static char*    sBroadcastsRedirectDir = NULL;
static char*    sDefaultBroadcastsRedirectDir = ""; // match none
static char*    sDefaultBroadcastsDir = ""; // match all
static char*	sDefaultsBroadcasterGroup = "broadcaster";
static StrPtrLen sBroadcasterGroup;

static QTSS_AttributeID sBroadcastDirListID = qtssIllegalAttrID;

static int32_t   sWaitTimeLoopCount = 10;

// Important strings
static StrPtrLen    sSDPKillSuffix(".kill");
static StrPtrLen    sSDPSuffix("");
static StrPtrLen    sMOVSuffix(".mov");
static StrPtrLen    sSDPTooLongMessage("Announced SDP is too long");
static StrPtrLen    sSDPNotValidMessage("Announced SDP is not a valid SDP");
static StrPtrLen    sKILLNotValidMessage("Announced .kill is not a valid SDP");
static StrPtrLen    sSDPTimeNotValidMessage("SDP time is not valid or movie not available at this time.");
static StrPtrLen    sBroadcastNotAllowed("Broadcast is not allowed.");
static StrPtrLen    sBroadcastNotActive("Broadcast is not active.");
static StrPtrLen    sTheNowRangeHeader("npt=now-");

// FUNCTION PROTOTYPES

static QTSS_Error QTSSReflectorModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParams);
static QTSS_Error Register(QTSS_Register_Params* inParams);
static QTSS_Error Initialize(QTSS_Initialize_Params* inParams);
static QTSS_Error Shutdown();
static QTSS_Error ProcessRTSPRequest(QTSS_StandardRTSP_Params* inParams);
static QTSS_Error DoAnnounce(QTSS_StandardRTSP_Params* inParams);
static QTSS_Error DoDescribe(QTSS_StandardRTSP_Params* inParams);
ReflectorSession* FindOrCreateSession(StrPtrLen* inName, QTSS_StandardRTSP_Params* inParams, uint32_t inChannel = 0, StrPtrLen* inData = NULL, bool isPush = false, bool *foundSessionPtr = NULL);
static QTSS_Error DoSetup(QTSS_StandardRTSP_Params* inParams);
static QTSS_Error DoPlay(QTSS_StandardRTSP_Params* inParams, ReflectorSession* inSession);
static QTSS_Error DestroySession(QTSS_ClientSessionClosing_Params* inParams);
static void RemoveOutput(ReflectorOutput* inOutput, ReflectorSession* inSession, bool killClients);
static ReflectorSession* DoSessionSetup(QTSS_StandardRTSP_Params* inParams, QTSS_AttributeID inPathType, bool isPush = false, bool *foundSessionPtr = NULL, char** resultFilePath = NULL);
static QTSS_Error RereadPrefs();
static QTSS_Error ProcessRTPData(QTSS_IncomingData_Params* inParams);
static QTSS_Error ReflectorAuthorizeRTSPRequest(QTSS_StandardRTSP_Params* inParams);
static bool InfoPortsOK(QTSS_StandardRTSP_Params* inParams, SDPSourceInfo* theInfo, StrPtrLen* inPath);
void KillCommandPathInList();
bool KillSession(StrPtrLen *sdpPath, bool killClients);
QTSS_Error IntervalRole();
static bool AcceptSession(QTSS_StandardRTSP_Params* inParams);
static QTSS_Error RedirectBroadcast(QTSS_StandardRTSP_Params* inParams);
static bool AllowBroadcast(QTSS_RTSPRequestObject inRTSPRequest);
static bool InBroadcastDirList(QTSS_RTSPRequestObject inRTSPRequest);
static bool IsAbsolutePath(StrPtrLen *inPathPtr);
static QTSS_Error GetDeviceStream(Easy_GetDeviceStream_Params* inParams);

inline void KeepSession(QTSS_RTSPRequestObject theRequest, bool keep)
{
	(void)QTSS_SetValue(theRequest, qtssRTSPReqRespKeepAlive, 0, &keep, sizeof(keep));
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

	(void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sDuplicateBroadcastStreamName, NULL, qtssAttrDataTypeCharArray);
	(void)QTSS_IDForAttr(qtssTextMessagesObjectType, sDuplicateBroadcastStreamName, &sDuplicateBroadcastStreamErr);

	(void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sAnnounceRequiresSDPinName, NULL, qtssAttrDataTypeCharArray);
	(void)QTSS_IDForAttr(qtssTextMessagesObjectType, sAnnounceRequiresSDPinName, &sAnnounceRequiresSDPinNameErr);

	(void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sAnnounceDisabledName, NULL, qtssAttrDataTypeCharArray);
	(void)QTSS_IDForAttr(qtssTextMessagesObjectType, sAnnounceDisabledName, &sAnnounceDisabledNameErr);


	(void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sExpectedDigitFilenameName, NULL, qtssAttrDataTypeCharArray);
	(void)QTSS_IDForAttr(qtssTextMessagesObjectType, sExpectedDigitFilenameName, &sExpectedDigitFilenameErr);

	(void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sReflectorBadTrackIDErrName, NULL, qtssAttrDataTypeCharArray);
	(void)QTSS_IDForAttr(qtssTextMessagesObjectType, sReflectorBadTrackIDErrName, &sReflectorBadTrackIDErr);

	static char* sSDPcontainsInvalidMinumumPortErrName = "QTSSReflectorModuleSDPPortMinimumPort";
	(void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sSDPcontainsInvalidMinumumPortErrName, NULL, qtssAttrDataTypeCharArray);
	(void)QTSS_IDForAttr(qtssTextMessagesObjectType, sSDPcontainsInvalidMinumumPortErrName, &sSDPcontainsInvalidMinimumPortErr);

	static char* sSDPcontainsInvalidMaximumPortErrName = "QTSSReflectorModuleSDPPortMaximumPort";
	(void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sSDPcontainsInvalidMaximumPortErrName, NULL, qtssAttrDataTypeCharArray);
	(void)QTSS_IDForAttr(qtssTextMessagesObjectType, sSDPcontainsInvalidMaximumPortErrName, &sSDPcontainsInvalidMaximumPortErr);

	static char* sStaticPortsConflictErrName = "QTSSReflectorModuleStaticPortsConflict";
	(void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sStaticPortsConflictErrName, NULL, qtssAttrDataTypeCharArray);
	(void)QTSS_IDForAttr(qtssTextMessagesObjectType, sStaticPortsConflictErrName, &sStaticPortsConflictErr);

	static char* sInvalidPortRangeErrName = "QTSSReflectorModuleStaticPortPrefsBadRange";
	(void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sInvalidPortRangeErrName, NULL, qtssAttrDataTypeCharArray);
	(void)QTSS_IDForAttr(qtssTextMessagesObjectType, sInvalidPortRangeErrName, &sInvalidPortRangeErr);

	// Add an RTP session attribute for tracking ReflectorSession objects
	static char*        sOutputName = "QTSSReflectorModuleOutput";
	static char*        sSessionName = "QTSSReflectorModuleSession";
	static char*        sStreamCookieName = "QTSSReflectorModuleStreamCookie";
	static char*        sRequestBufferName = "QTSSReflectorModuleRequestBuffer";
	static char*        sRequestBufferLenName = "QTSSReflectorModuleRequestBufferLen";
	static char*        sBroadcasterSessionName = "QTSSReflectorModuleBroadcasterSession";
	static char*        sKillClientsEnabledName = "QTSSReflectorModuleTearDownClients";

	static char*        sRTPInfoWaitTime = "QTSSReflectorModuleRTPInfoWaitTime";
	(void)QTSS_AddStaticAttribute(qtssClientSessionObjectType, sRTPInfoWaitTime, NULL, qtssAttrDataTypeint32_t);
	(void)QTSS_IDForAttr(qtssClientSessionObjectType, sRTPInfoWaitTime, &sRTPInfoWaitTimeAttr);

	(void)QTSS_AddStaticAttribute(qtssClientSessionObjectType, sOutputName, NULL, qtssAttrDataTypeVoidPointer);
	(void)QTSS_IDForAttr(qtssClientSessionObjectType, sOutputName, &sOutputAttr);

	(void)QTSS_AddStaticAttribute(qtssClientSessionObjectType, sSessionName, NULL, qtssAttrDataTypeVoidPointer);
	(void)QTSS_IDForAttr(qtssClientSessionObjectType, sSessionName, &sSessionAttr);

	(void)QTSS_AddStaticAttribute(qtssRTPStreamObjectType, sStreamCookieName, NULL, qtssAttrDataTypeVoidPointer);
	(void)QTSS_IDForAttr(qtssRTPStreamObjectType, sStreamCookieName, &sStreamCookieAttr);

	(void)QTSS_AddStaticAttribute(qtssRTSPRequestObjectType, sRequestBufferName, NULL, qtssAttrDataTypeVoidPointer);
	(void)QTSS_IDForAttr(qtssRTSPRequestObjectType, sRequestBufferName, &sRequestBodyAttr);

	(void)QTSS_AddStaticAttribute(qtssRTSPRequestObjectType, sRequestBufferLenName, NULL, qtssAttrDataTypeUInt32);
	(void)QTSS_IDForAttr(qtssRTSPRequestObjectType, sRequestBufferLenName, &sBufferOffsetAttr);

	(void)QTSS_AddStaticAttribute(qtssClientSessionObjectType, sBroadcasterSessionName, NULL, qtssAttrDataTypeVoidPointer);
	(void)QTSS_IDForAttr(qtssClientSessionObjectType, sBroadcasterSessionName, &sClientBroadcastSessionAttr);

	(void)QTSS_AddStaticAttribute(qtssClientSessionObjectType, sKillClientsEnabledName, NULL, qtssAttrDataTypeBool16);
	(void)QTSS_IDForAttr(qtssClientSessionObjectType, sKillClientsEnabledName, &sKillClientsEnabledAttr);

	// keep the same attribute name for the RTSPSessionObject as used int he ClientSessionObject
	(void)QTSS_AddStaticAttribute(qtssRTSPSessionObjectType, sBroadcasterSessionName, NULL, qtssAttrDataTypeVoidPointer);
	(void)QTSS_IDForAttr(qtssRTSPSessionObjectType, sBroadcasterSessionName, &sRTSPBroadcastSessionAttr);

	// Reflector session needs to setup some parameters too.
	ReflectorStream::Register();
	// RTPSessionOutput needs to do the same
	RTPSessionOutput::Register();

	// Tell the server our name!
	static char* sModuleName = "QTSSReflectorModule";
	::strcpy(inParams->outModuleName, sModuleName);

	return QTSS_NoErr;
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
	ReflectorSession::Initialize();

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

	char *keyword = new char[theKeyWordStr.Len + 1];
	::memcpy(keyword, theKeyWordStr.Ptr, theKeyWordStr.Len);
	keyword[theKeyWordStr.Len] = 0;

	return keyword;
}

void SetMoviesRelativeDir()
{
	char* movieFolderString = NULL;
	(void)QTSS_GetValueAsString(sServerPrefs, qtssPrefsMovieFolder, 0, &movieFolderString);
	OSCharArrayDeleter deleter(movieFolderString);

	ResizeableStringFormatter redirectPath(NULL, 0);
	redirectPath.Put(movieFolderString);
	if (redirectPath.GetBytesWritten() > 0 && kPathDelimiterChar != redirectPath.GetBufPtr()[redirectPath.GetBytesWritten() - 1])
		redirectPath.PutChar(kPathDelimiterChar);
	redirectPath.Put(sBroadcastsRedirectDir);

	char *newMovieRelativeDir = new char[redirectPath.GetBytesWritten() + 1];
	::memcpy(newMovieRelativeDir, redirectPath.GetBufPtr(), redirectPath.GetBytesWritten());
	newMovieRelativeDir[redirectPath.GetBytesWritten()] = 0;

	delete[] sBroadcastsRedirectDir;
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

	delete[] sRedirectBroadcastsKeyword;
	char* tempKeyWord = QTSSModuleUtils::GetStringAttribute(sPrefs, "redirect_broadcast_keyword", sDefaultRedirectBroadcastsKeyword);

	sRedirectBroadcastsKeyword = GetTrimmedKeyWord(tempKeyWord);
	delete[] tempKeyWord;

	delete[] sBroadcastsRedirectDir;
	sBroadcastsRedirectDir = QTSSModuleUtils::GetStringAttribute(sPrefs, "redirect_broadcasts_dir", sDefaultBroadcastsRedirectDir);
	if (sBroadcastsRedirectDir && sBroadcastsRedirectDir[0] != kPathDelimiterChar)
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
				qtss_sprintf(min, "%u", minServerPort);
				qtss_sprintf(max, "%u", maxServerPort);
				QTSSModuleUtils::LogError(qtssWarningVerbosity, sStaticPortsConflictErr, 0, min, max);
			}

			if (sMinimumStaticSDPPort > sMaximumStaticSDPPort)
			{
				qtss_sprintf(min, "%u", sMinimumStaticSDPPort);
				qtss_sprintf(max, "%u", sMaximumStaticSDPPort);
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
	ReflectorSession* theSession = NULL;
	uint32_t theLen = sizeof(theSession);
	QTSS_Error theErr = QTSS_GetValue(inParams->inRTSPSession, sRTSPBroadcastSessionAttr, 0, &theSession, &theLen);

	//printf("QTSSReflectorModule.cpp:ProcessRTPData    sClientBroadcastSessionAttr=%"   _U32BITARG_   " theSession=%"   _U32BITARG_   " err=%" _S32BITARG_ " \n",sClientBroadcastSessionAttr, theSession,theErr);
	if (theSession == NULL || theErr != QTSS_NoErr)
		return QTSS_NoErr;

	// it is a broadcaster session
	//printf("QTSSReflectorModule.cpp:is broadcaster session\n");

	SourceInfo* theSoureInfo = theSession->GetSourceInfo();
	Assert(theSoureInfo != NULL);
	if (theSoureInfo == NULL)
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
				if (theStream == NULL) return QTSS_Unimplemented;

				SourceInfo::StreamInfo* theStreamInfo = theStream->GetStreamInfo();
				uint16_t serverReceivePort = theStreamInfo->fPort;

				bool isRTCP = false;
				if (theStream != NULL)
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
	return theErr;
}


QTSS_Error ProcessRTSPRequest(QTSS_StandardRTSP_Params* inParams)
{
	OSMutexLocker locker(sSessionMap->GetMutex()); //operating on sOutputAttr

	QTSS_RTSPMethod* theMethod = NULL;
	//printf("QTSSReflectorModule:ProcessRTSPRequest inClientSession=%"   _U32BITARG_   "\n", (uint32_t) inParams->inClientSession);
	uint32_t theLen = 0;
	if ((QTSS_GetValuePtr(inParams->inRTSPRequest, qtssRTSPReqMethod, 0,
		(void**)&theMethod, &theLen) != QTSS_NoErr) || (theLen != sizeof(QTSS_RTSPMethod)))
	{
		Assert(0);
		return QTSS_RequestFailed;
	}

	if (*theMethod == qtssAnnounceMethod)
		return DoAnnounce(inParams);
	if (*theMethod == qtssDescribeMethod)
		return DoDescribe(inParams);
	if (*theMethod == qtssSetupMethod)
		return DoSetup(inParams);

	RTPSessionOutput** theOutput = NULL;
	QTSS_Error theErr = QTSS_GetValuePtr(inParams->inClientSession, sOutputAttr, 0, (void**)&theOutput, &theLen);
	if ((theErr != QTSS_NoErr) || (theLen != sizeof(RTPSessionOutput*))) // a broadcaster push session
	{
		if (*theMethod == qtssPlayMethod || *theMethod == qtssRecordMethod)
			return DoPlay(inParams, NULL);
		else
			return QTSS_RequestFailed;
	}

	switch (*theMethod)
	{
	case qtssPlayMethod:
		return DoPlay(inParams, (*theOutput)->GetReflectorSession());
	case qtssTeardownMethod:
		// Tell the server that this session should be killed, and send a TEARDOWN response
		(void)QTSS_Teardown(inParams->inClientSession);
		(void)QTSS_SendStandardRTSPResponse(inParams->inRTSPRequest, inParams->inClientSession, 0);
		break;
	case qtssPauseMethod:
		(void)QTSS_Pause(inParams->inClientSession);
		(void)QTSS_SendStandardRTSPResponse(inParams->inRTSPRequest, inParams->inClientSession, 0);
		break;
	default:
		break;
	}
	return QTSS_NoErr;
}

ReflectorSession* DoSessionSetup(QTSS_StandardRTSP_Params* inParams, QTSS_AttributeID inPathType, bool isPush, bool *foundSessionPtr, char** resultFilePath)
{
	char* theFileNameStr = NULL;
	QTSS_Error theErr = QTSS_GetValueAsString(inParams->inRTSPRequest, qtssRTSPReqFileName, 0, &theFileNameStr);
	Assert(theErr == QTSS_NoErr);
	QTSSCharArrayDeleter theFileNameStrDeleter(theFileNameStr);
	if (theErr != QTSS_NoErr)
		return NULL;

	char* theQueryString = NULL;
	theErr = QTSS_GetValueAsString(inParams->inRTSPRequest, qtssRTSPReqQueryString, 0, &theQueryString);
	QTSSCharArrayDeleter theQueryStringDeleter(theQueryString);

	std::string queryTemp;
	if (theQueryString)
	{
		queryTemp = EasyUtil::Urldecode(theQueryString);
	}
	QueryParamList parList(const_cast<char *>(queryTemp.c_str()));

	uint32_t theChannelNum = 1;
	const char* chnNum = parList.DoFindCGIValueForParam(EASY_TAG_CHANNEL);
	if (chnNum)
	{
		theChannelNum = stoi(chnNum);
	}

	StrPtrLen theFullPath(theFileNameStr);

	if (theFullPath.Len > sMOVSuffix.Len)
	{
		StrPtrLen endOfPath2(&theFullPath.Ptr[theFullPath.Len - sMOVSuffix.Len], sMOVSuffix.Len);
		if (endOfPath2.Equal(sMOVSuffix)) // it is a .mov so it is not meant for us
		{
			return NULL;
		}
	}

	if (sAllowNonSDPURLs && !isPush)
	{
		// Check and see if the full path to this file matches an existing ReflectorSession
		StrPtrLen thePathPtr;
		OSCharArrayDeleter sdpPath(QTSSModuleUtils::GetFullPath(inParams->inRTSPRequest,
			inPathType,
			&thePathPtr.Len, &sSDPSuffix));

		thePathPtr.Ptr = sdpPath.GetObject();

		// If the actual file path has a .sdp in it, first look for the URL without the extra .sdp
		if (thePathPtr.Len > (sSDPSuffix.Len * 2))
		{
			// Check and see if there is a .sdp in the file path.
			// If there is, truncate off our extra ".sdp", cuz it isn't needed
			StrPtrLen endOfPath(&sdpPath.GetObject()[thePathPtr.Len - (sSDPSuffix.Len * 2)], sSDPSuffix.Len);
			if (endOfPath.Equal(sSDPSuffix))
			{
				sdpPath.GetObject()[thePathPtr.Len - sSDPSuffix.Len] = '\0';
				thePathPtr.Len -= sSDPSuffix.Len;
			}
		}
		if (resultFilePath != NULL)
			*resultFilePath = thePathPtr.GetAsCString();
		return FindOrCreateSession(&thePathPtr, inParams, theChannelNum);
	}
	else
	{
		if (!sDefaultBroadcastPushEnabled)
			return NULL;
		//
		// We aren't supposed to auto-append a .sdp, so just get the URL path out of the server
		//StrPtrLen theFullPath;
		//QTSS_Error theErr = QTSS_GetValuePtr(inParams->inRTSPRequest, qtssRTSPReqLocalPath, 0, (void**)&theFullPath.Ptr, &theFullPath.Len);
		//Assert(theErr == QTSS_NoErr);

		if (theFullPath.Len > sSDPSuffix.Len)
		{
			//
			// Check to make sure this path has a .sdp at the end. If it does,
			// attempt to get a reflector session for this URL.
			StrPtrLen endOfPath2(&theFullPath.Ptr[theFullPath.Len - sSDPSuffix.Len], sSDPSuffix.Len);
			if (endOfPath2.Equal(sSDPSuffix))
			{
				if (resultFilePath != NULL)
					*resultFilePath = theFullPath.GetAsCString();
				return FindOrCreateSession(&theFullPath, inParams, theChannelNum, NULL, isPush, foundSessionPtr);
			}
		}
		return NULL;
	}
}

void DoAnnounceAddRequiredSDPLines(QTSS_StandardRTSP_Params* inParams, ResizeableStringFormatter *editedSDP, char* theSDPPtr)
{
	SDPContainer checkedSDPContainer;
	checkedSDPContainer.SetSDPBuffer(theSDPPtr);
	if (!checkedSDPContainer.HasReqLines())
	{
		if (!checkedSDPContainer.HasLineType('v'))
		{ // add v line
			editedSDP->Put("v=0\r\n");
		}

		if (!checkedSDPContainer.HasLineType('s'))
		{ // add s line
			char* theSDPName = NULL;

			(void)QTSS_GetValueAsString(inParams->inRTSPRequest, qtssRTSPReqFilePath, 0, &theSDPName);
			QTSSCharArrayDeleter thePathStrDeleter(theSDPName);
			if (theSDPName == NULL)
				editedSDP->Put("s=unknown\r\n");
			else
			{
				editedSDP->Put("s=");
				editedSDP->Put(theSDPName);
				editedSDP->PutEOL();
			}
		}

		if (!checkedSDPContainer.HasLineType('t'))
		{ // add t line
			editedSDP->Put("t=0 0\r\n");
		}

		if (!checkedSDPContainer.HasLineType('o'))
		{ // add o line
			editedSDP->Put("o=");
			char tempBuff[256] = ""; tempBuff[255] = 0;
			char *nameStr = tempBuff;
			uint32_t buffLen = sizeof(tempBuff) - 1;
			(void)QTSS_GetValue(inParams->inClientSession, qtssCliSesFirstUserAgent, 0, nameStr, &buffLen);
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
				editedSDP->Put("announced_broadcast");
			else
				editedSDP->Put(nameStr, buffLen);

			editedSDP->Put(" ");

			buffLen = sizeof(tempBuff) - 1;
			(void)QTSS_GetValue(inParams->inClientSession, qtssCliSesRTSPSessionID, 0, &tempBuff, &buffLen);
			editedSDP->Put(tempBuff, buffLen);

			editedSDP->Put(" ");
			qtss_snprintf(tempBuff, sizeof(tempBuff) - 1, "%" _64BITARG_ "d", (int64_t)OS::UnixTime_Secs() + 2208988800LU);
			editedSDP->Put(tempBuff);

			editedSDP->Put(" IN IP4 ");
			(void)QTSS_GetValue(inParams->inClientSession, qtssCliRTSPSessRemoteAddrStr, 0, tempBuff, &buffLen);
			editedSDP->Put(tempBuff, buffLen);

			editedSDP->PutEOL();
		}
	}

	editedSDP->Put(theSDPPtr);
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
	char* theFileNameStr = NULL;
	(void)QTSS_GetValueAsString(inParams->inRTSPRequest, qtssRTSPReqFileName, 0, &theFileNameStr);
	QTSSCharArrayDeleter theFileNameStrDeleter(theFileNameStr);
	StrPtrLen theFullPath(theFileNameStr);

	char* theQueryString = NULL;
	QTSS_GetValueAsString(inParams->inRTSPRequest, qtssRTSPReqQueryString, 0, &theQueryString);
	QTSSCharArrayDeleter theQueryStringDeleter(theQueryString);

	std::string queryTemp;
	if (theQueryString)
	{
		queryTemp = EasyUtil::Urldecode(theQueryString);
	}
	QueryParamList parList(const_cast<char *>(queryTemp.c_str()));

	uint32_t theChannelNum = 1;
	const char* chnNum = parList.DoFindCGIValueForParam(EASY_TAG_CHANNEL);
	if (chnNum)
	{
		theChannelNum = stoi(chnNum);
	}

	char theStreamName[QTSS_MAX_NAME_LENGTH] = { 0 };
	sprintf(theStreamName, "%s%s%d", theFileNameStr, EASY_KEY_SPLITER, theChannelNum);

	// Check for a .kill at the end
	bool pathOK = false;
	bool killBroadcast = false;
	if (sAnnouncedKill && theFullPath.Len > sSDPKillSuffix.Len)
	{
		StrPtrLen endOfPath(theFullPath.Ptr + (theFullPath.Len - sSDPKillSuffix.Len), sSDPKillSuffix.Len);
		if (endOfPath.Equal(sSDPKillSuffix))
		{
			pathOK = true;
			killBroadcast = true;
		}
	}

	// Check for a .sdp at the end
	if (!pathOK)
	{
		if (theFullPath.Len <= sSDPSuffix.Len)
		{
			StrPtrLen endOfPath(theFullPath.Ptr + (theFullPath.Len - sSDPSuffix.Len), sSDPSuffix.Len);
			if (!endOfPath.Equal(sSDPSuffix))
				return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssPreconditionFailed, sAnnounceRequiresSDPinNameErr);
		}
	}

	// Ok, this is an sdp file. Retreive the entire contents of the SDP.
	// This has to be done asynchronously (in case the SDP stuff is fragmented across
	// multiple packets. So, we have to have a simple state machine.

	//
	// We need to know the content length to manage memory
	uint32_t theLen = 0;
	uint32_t* theContentLenP = NULL;
	QTSS_Error theErr = QTSS_GetValuePtr(inParams->inRTSPRequest, qtssRTSPReqContentLen, 0, (void**)&theContentLenP, &theLen);
	if ((theErr != QTSS_NoErr) || (theLen != sizeof(uint32_t)))
	{
		//
		// RETURN ERROR RESPONSE: ANNOUNCE WITHOUT CONTENT LENGTH
		return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssClientBadRequest, 0);
	}

	// Check if the content-length is more than the imposed maximum
	// if it is then return error response
	if ((sMaxAnnouncedSDPLengthInKbytes != 0) && (*theContentLenP > (sMaxAnnouncedSDPLengthInKbytes * 1024)))
		return QTSSModuleUtils::SendErrorResponseWithMessage(inParams->inRTSPRequest, qtssPreconditionFailed, &sSDPTooLongMessage);

	//
	// Check for the existence of 2 attributes in the request: a pointer to our buffer for
	// the request body, and the current offset in that buffer. If these attributes exist,
	// then we've already been here for this request. If they don't exist, add them.
	uint32_t theBufferOffset = 0;
	char* theRequestBody = NULL;

	theLen = sizeof(theRequestBody);
	theErr = QTSS_GetValue(inParams->inRTSPRequest, sRequestBodyAttr, 0, &theRequestBody, &theLen);

	//printf("QTSSReflectorModule:DoAnnounce theRequestBody =%s\n",theRequestBody);
	if (theErr != QTSS_NoErr)
	{
		//
		// First time we've been here for this request. Create a buffer for the content body and
		// shove it in the request.
		theRequestBody = new char[*theContentLenP + 1];
		memset(theRequestBody, 0, *theContentLenP + 1);
		theLen = sizeof(theRequestBody);
		theErr = QTSS_SetValue(inParams->inRTSPRequest, sRequestBodyAttr, 0, &theRequestBody, theLen);// SetValue creates an internal copy.
		Assert(theErr == QTSS_NoErr);

		//
		// Also store the offset in the buffer
		theLen = sizeof(theBufferOffset);
		theErr = QTSS_SetValue(inParams->inRTSPRequest, sBufferOffsetAttr, 0, &theBufferOffset, theLen);
		Assert(theErr == QTSS_NoErr);
	}

	theLen = sizeof(theBufferOffset);
	theErr = QTSS_GetValue(inParams->inRTSPRequest, sBufferOffsetAttr, 0, &theBufferOffset, &theLen);

	//
	// We have our buffer and offset. Read the data.
	theErr = QTSS_Read(inParams->inRTSPRequest, theRequestBody + theBufferOffset, *theContentLenP - theBufferOffset, &theLen);
	Assert(theErr != QTSS_BadArgument);

	if (theErr == QTSS_RequestFailed)
	{
		OSCharArrayDeleter charArrayPathDeleter(theRequestBody);
		//
		// NEED TO RETURN RTSP ERROR RESPONSE
		return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssClientBadRequest, 0);
	}

	if ((theErr == QTSS_WouldBlock) || (theLen < (*theContentLenP - theBufferOffset)))
	{
		//
		// Update our offset in the buffer
		theBufferOffset += theLen;
		(void)QTSS_SetValue(inParams->inRTSPRequest, sBufferOffsetAttr, 0, &theBufferOffset, sizeof(theBufferOffset));
		//printf("QTSSReflectorModule:DoAnnounce Request some more data \n");
		//
		// The entire content body hasn't arrived yet. Request a read event and wait for it.
		// Our DoAnnounce function will get called again when there is more data.
		theErr = QTSS_RequestEvent(inParams->inRTSPRequest, QTSS_ReadableEvent);
		Assert(theErr == QTSS_NoErr);
		return QTSS_NoErr;
	}

	Assert(theErr == QTSS_NoErr);


	//
	// If we've gotten here, we have the entire content body in our buffer.
	//

	if (killBroadcast)
	{
		theFullPath.Len -= sSDPKillSuffix.Len;
		if (KillSession(&theFullPath, killBroadcast))
			return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssServerInternal, 0);
		else
			return QTSSModuleUtils::SendErrorResponseWithMessage(inParams->inRTSPRequest, qtssClientNotFound, &sKILLNotValidMessage);
	}

	// ------------  Clean up missing required SDP lines

	ResizeableStringFormatter editedSDP(NULL, 0);
	DoAnnounceAddRequiredSDPLines(inParams, &editedSDP, theRequestBody);
	StrPtrLen editedSDPSPL(editedSDP.GetBufPtr(), editedSDP.GetBytesWritten());

	// ------------ Check the headers

	SDPContainer checkedSDPContainer;
	checkedSDPContainer.SetSDPBuffer(&editedSDPSPL);
	if (!checkedSDPContainer.IsSDPBufferValid())
	{
		return QTSSModuleUtils::SendErrorResponseWithMessage(inParams->inRTSPRequest, qtssUnsupportedMediaType, &sSDPNotValidMessage);
	}

	SDPSourceInfo theSDPSourceInfo(editedSDPSPL.Ptr, editedSDPSPL.Len);
	OSCharArrayDeleter charArrayPathDeleter(theRequestBody);

	if (!InfoPortsOK(inParams, &theSDPSourceInfo, &theFullPath)) // All validity checks like this check should be done before touching the file.
	{
		return QTSS_NoErr; // InfoPortsOK is sending back the error.
	}

	// ------------ reorder the sdp headers to make them proper.

	SDPLineSorter sortedSDP(&checkedSDPContainer);

	// ------------ Write the SDP 

	char* sessionHeaders = sortedSDP.GetSessionHeaders()->GetAsCString();
	OSCharArrayDeleter sessionHeadersDeleter(sessionHeaders);

	char* mediaHeaders = sortedSDP.GetMediaHeaders()->GetAsCString();
	OSCharArrayDeleter mediaHeadersDeleter(mediaHeaders);

	// sortedSDP.GetSessionHeaders()->PrintStrEOL();
	// sortedSDP.GetMediaHeaders()->PrintStrEOL();

#if 0
	   // write the file !! need error reporting
	FILE* theSDPFile = ::fopen(theFullPath.Ptr, "wb");//open 
	if (theSDPFile != NULL)
	{
		qtss_fprintf(theSDPFile, "%s", sessionHeaders);
		qtss_fprintf(theSDPFile, "%s", mediaHeaders);
		::fflush(theSDPFile);
		::fclose(theSDPFile);
	}
	else
	{
		return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssClientForbidden, 0);
	}
#endif 
	char sdpContext[1024] = { 0 };
	sprintf(sdpContext, "%s%s", sessionHeaders, mediaHeaders);
	CSdpCache::GetInstance()->setSdpMap(theStreamName, sdpContext);


	//printf("QTSSReflectorModule:DoAnnounce SendResponse OK=200\n");

	return QTSS_SendStandardRTSPResponse(inParams->inRTSPRequest, inParams->inClientSession, 0);
}

void DoDescribeAddRequiredSDPLines(QTSS_StandardRTSP_Params* inParams, ReflectorSession* theSession, QTSS_TimeVal modDate, ResizeableStringFormatter *editedSDP, StrPtrLen* theSDPPtr)
{
	SDPContainer checkedSDPContainer;
	checkedSDPContainer.SetSDPBuffer(theSDPPtr);
	if (!checkedSDPContainer.HasReqLines())
	{
		if (!checkedSDPContainer.HasLineType('v'))
		{ // add v line
			editedSDP->Put("v=0\r\n");
		}

		if (!checkedSDPContainer.HasLineType('s'))
		{ // add s line
			char* theSDPName = NULL;
			(void)QTSS_GetValueAsString(inParams->inRTSPRequest, qtssRTSPReqFilePath, 0, &theSDPName);
			QTSSCharArrayDeleter thePathStrDeleter(theSDPName);
			editedSDP->Put("s=");
			editedSDP->Put(theSDPName);
			editedSDP->PutEOL();
		}

		if (!checkedSDPContainer.HasLineType('t'))
		{ // add t line
			editedSDP->Put("t=0 0\r\n");
		}

		if (!checkedSDPContainer.HasLineType('o'))
		{ // add o line
			editedSDP->Put("o=broadcast_sdp ");
			char tempBuff[256] = "";
			tempBuff[255] = 0;
			qtss_snprintf(tempBuff, sizeof(tempBuff) - 1, "%"   _U32BITARG_   "", *(uint32_t *)&theSession);
			editedSDP->Put(tempBuff);

			editedSDP->Put(" ");
			// modified date is in milliseconds.  Convert to NTP seconds as recommended by rfc 2327
			qtss_snprintf(tempBuff, sizeof(tempBuff) - 1, "%" _64BITARG_ "d", (int64_t)(modDate / 1000) + 2208988800LU);
			editedSDP->Put(tempBuff);

			editedSDP->Put(" IN IP4 ");
			uint32_t buffLen = sizeof(tempBuff) - 1;
			(void)QTSS_GetValue(inParams->inClientSession, qtssCliSesHostName, 0, &tempBuff, &buffLen);
			editedSDP->Put(tempBuff, buffLen);

			editedSDP->PutEOL();
		}
	}

	editedSDP->Put(*theSDPPtr);

}

QTSS_Error DoDescribe(QTSS_StandardRTSP_Params* inParams)
{
	uint32_t theRefCount = 0;
	char *theFileName = NULL;
	ReflectorSession* theSession = DoSessionSetup(inParams, qtssRTSPReqFileName, false, NULL, &theFileName);
	OSCharArrayDeleter tempFilePath(theFileName);

	if (theSession == NULL)
		return QTSS_RequestFailed;

	theRefCount++;

	//	//redis,streamid/serial/channel.sdp,for example "./Movies/\streamid\serial\channel0.sdp"
	//	if(true)
	//	{
	//		//1.get the path
	//		char* theFileNameStr = NULL;
	//		QTSS_Error theErrEx = QTSS_GetValueAsString(inParams->inRTSPRequest, qtssRTSPReqLocalPath, 0, &theFileNameStr);
	//		Assert(theErrEx == QTSS_NoErr);
	//		QTSSCharArrayDeleter theFileNameStrDeleter(theFileNameStr);
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

	RTPSessionOutput** theOutput = NULL;
	uint32_t theLen = 0;
	QTSS_Error theErr = QTSS_GetValuePtr(inParams->inClientSession, sOutputAttr, 0, (void**)&theOutput, &theLen);

	// If there already  was an RTPSessionOutput attached to this Client Session,
	// destroy it. 
	if (theErr == QTSS_NoErr && theOutput != NULL)
	{
		RemoveOutput(*theOutput, (*theOutput)->GetReflectorSession(), false);
		RTPSessionOutput* theOutput = NULL;
		(void)QTSS_SetValue(inParams->inClientSession, sOutputAttr, 0, &theOutput, sizeof(theOutput));

	}
	// send the DESCRIBE response

	//above function has signalled that this request belongs to us, so let's respond
	iovec theDescribeVec[3] = { {0 } };

	Assert(theSession->GetLocalSDP()->Ptr != NULL);


	StrPtrLen theFileData;
	QTSS_TimeVal outModDate = 0;
	QTSS_TimeVal inModDate = -1;
	(void)QTSSModuleUtils::ReadEntireFile(theFileName, &theFileData, inModDate, &outModDate);
	OSCharArrayDeleter fileDataDeleter(theFileData.Ptr);

	// -------------- process SDP to remove connection info and add track IDs, port info, and default c= line

	StrPtrLen theSDPData;
	SDPSourceInfo tempSDPSourceInfo(theFileData.Ptr, theFileData.Len); // will make a copy and delete in destructor
	theSDPData.Ptr = tempSDPSourceInfo.GetLocalSDP(&theSDPData.Len); // returns a new buffer with processed sdp
	OSCharArrayDeleter sdpDeleter(theSDPData.Ptr); // delete the temp sdp source info buffer returned by GetLocalSDP

	if (theSDPData.Len <= 0) // can't find it on disk or it failed to parse just use the one in the session.
	{
		theSDPData.Ptr = theSession->GetLocalSDP()->Ptr; // this sdp isn't ours it must not be deleted
		theSDPData.Len = theSession->GetLocalSDP()->Len;
	}


	// ------------  Clean up missing required SDP lines

	ResizeableStringFormatter editedSDP(NULL, 0);
	DoDescribeAddRequiredSDPLines(inParams, theSession, outModDate, &editedSDP, &theSDPData);
	StrPtrLen editedSDPSPL(editedSDP.GetBufPtr(), editedSDP.GetBytesWritten());

	// ------------ Check the headers

	SDPContainer checkedSDPContainer;
	checkedSDPContainer.SetSDPBuffer(&editedSDPSPL);
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

	ResizeableStringFormatter buffer;
	SDPContainer* insertMediaLines = NULL;
	SDPLineSorter sortedSDP(&checkedSDPContainer, adjustMediaBandwidthPercent, insertMediaLines);
	delete insertMediaLines;

	// ------------ Write the SDP 

	uint32_t sessLen = sortedSDP.GetSessionHeaders()->Len;
	uint32_t mediaLen = sortedSDP.GetMediaHeaders()->Len;
	theDescribeVec[1].iov_base = sortedSDP.GetSessionHeaders()->Ptr;
	theDescribeVec[1].iov_len = sortedSDP.GetSessionHeaders()->Len;

	theDescribeVec[2].iov_base = sortedSDP.GetMediaHeaders()->Ptr;
	theDescribeVec[2].iov_len = sortedSDP.GetMediaHeaders()->Len;

	(void)QTSS_AppendRTSPHeader(inParams->inRTSPRequest, qtssCacheControlHeader,
		kCacheControlHeader.Ptr, kCacheControlHeader.Len);
	QTSSModuleUtils::SendDescribeResponse(inParams->inRTSPRequest, inParams->inClientSession,
		&theDescribeVec[0], 3, sessLen + mediaLen);

	if (theRefCount)
		sSessionMap->Release(theSession->GetRef());

#ifdef REFLECTORSESSION_DEBUG
	printf("QTSSReflectorModule.cpp:DoDescribe Session =%p refcount=%"   _U32BITARG_   "\n", theSession->GetRef(), theSession->GetRef()->GetRefCount());
#endif

	return QTSS_NoErr;
}

bool InfoPortsOK(QTSS_StandardRTSP_Params* inParams, SDPSourceInfo* theInfo, StrPtrLen* inPath)
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
				char thePort[32];
				qtss_sprintf(thePort, "%u", theInfoPort);

				char *thePath = inPath->GetAsCString();
				OSCharArrayDeleter charArrayPathDeleter(thePath);

				char *thePathPort = new char[inPath->Len + 32];
				OSCharArrayDeleter charArrayPathPortDeleter(thePathPort);

				qtss_sprintf(thePathPort, "%s:%s", thePath, thePort);
				(void)QTSSModuleUtils::LogError(qtssWarningVerbosity, theErrorMessageID, 0, thePathPort);

				StrPtrLen thePortStr(thePort);
				(void)QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssUnsupportedMediaType, theErrorMessageID, &thePortStr);

				return false;
			}
		}
	}

	return isOK;
}

ReflectorSession* FindOrCreateSession(StrPtrLen* inName, QTSS_StandardRTSP_Params* inParams, uint32_t inChannel, StrPtrLen* inData, bool isPush, bool *foundSessionPtr)
{
	OSMutexLocker locker(sSessionMap->GetMutex());

	char theStreamName[QTSS_MAX_NAME_LENGTH] = { 0 };
	sprintf(theStreamName, "%s%s%d", inName->Ptr, EASY_KEY_SPLITER, inChannel);

	StrPtrLen inPath(theStreamName);

	OSRef* theSessionRef = sSessionMap->Resolve(&inPath);
	ReflectorSession* theSession = NULL;

	if (theSessionRef == NULL)
	{
		if (!isPush)
		{
			return NULL;
		}

		StrPtrLen theFileData;
		StrPtrLen theFileDeleteData;

		if (inData == NULL)
		{
			(void)QTSSModuleUtils::ReadEntireFile(inPath.Ptr, &theFileDeleteData);
			theFileData = theFileDeleteData;
		}
		else
		{
			theFileData = *inData;
		}
		OSCharArrayDeleter fileDataDeleter(theFileDeleteData.Ptr);

		if (theFileData.Len <= 0)
			return NULL;

		SDPSourceInfo* theInfo = new SDPSourceInfo(theFileData.Ptr, theFileData.Len); // will make a copy

		if (!theInfo->IsReflectable())
		{
			delete theInfo;
			return NULL;
		}

		if (!InfoPortsOK(inParams, theInfo, &inPath))
		{
			delete theInfo;
			return NULL;
		}

		// Check if broadcast is allowed before doing anything else
		// At this point we know it is a definitely a reflector session
		// It is either incoming automatic broadcast setup or a client setup to view broadcast
		// In either case, verify whether the broadcast is allowed, and send forbidden response back
		if (!AllowBroadcast(inParams->inRTSPRequest))
		{
			(void)QTSSModuleUtils::SendErrorResponseWithMessage(inParams->inRTSPRequest, qtssClientForbidden, &sBroadcastNotAllowed);
			return NULL;
		}

		//
		// Setup a ReflectorSession and bind the sockets. If we are negotiating,
		// make sure to let the session know that this is a Push Session so
		// ports may be modified.
		uint32_t theSetupFlag = ReflectorSession::kMarkSetup;
		if (isPush)
			theSetupFlag |= ReflectorSession::kIsPushSession;

		theSession = new ReflectorSession(inName, inChannel);
		if (theSession == NULL)
		{
			return NULL;
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
			theSession->Signal(Task::kKillEvent);
			return NULL;
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
			if (!AllowBroadcast(inParams->inRTSPRequest))
			{
				(void)QTSSModuleUtils::SendErrorResponseWithMessage(inParams->inRTSPRequest, qtssClientForbidden, &sBroadcastNotAllowed);
				break;
			}

			if (foundSessionPtr)
				*foundSessionPtr = true;

			StrPtrLen theFileData;

			if (inData == NULL)
				(void)QTSSModuleUtils::ReadEntireFile(inPath.Ptr, &theFileData);
			OSCharArrayDeleter charArrayDeleter(theFileData.Ptr);

			if (theFileData.Len <= 0)
				break;

			SDPSourceInfo* theInfo = new SDPSourceInfo(theFileData.Ptr, theFileData.Len);
			if (theInfo == NULL)
				break;

			if (!InfoPortsOK(inParams, theInfo, &inPath))
			{
				delete theInfo;
				break;
			}

			delete theInfo;

			theSession = (ReflectorSession*)theSessionRef->GetObject();
			if (isPush && theSession && !(theSession->IsSetup()))
			{
				uint32_t theSetupFlag = ReflectorSession::kMarkSetup | ReflectorSession::kIsPushSession;
				QTSS_Error theErr = theSession->SetupReflectorSession(NULL, inParams, theSetupFlag);
				if (theErr != QTSS_NoErr)
				{
					theSession =  NULL;
					break;
				}
			}			;
		} while (0);

		if (theSession == NULL)
			sSessionMap->Release(theSessionRef);
	}

	Assert(theSession != NULL);

	// Turn off overbuffering if the "disable_overbuffering" pref says so
	if (sDisableOverbuffering)
		(void)QTSS_SetValue(inParams->inClientSession, qtssCliSesOverBufferEnabled, 0, &sFalse, sizeof(sFalse));

	return theSession;
}

// ONLY call when performing a setup.
void DeleteReflectorPushSession(QTSS_StandardRTSP_Params* inParams, ReflectorSession* theSession, bool foundSession)
{
	if(theSession)
		sSessionMap->Release(theSession->GetRef());

	ReflectorSession* stopSessionProcessing = NULL;
	QTSS_Error theErr = QTSS_SetValue(inParams->inClientSession, sClientBroadcastSessionAttr, 0, &stopSessionProcessing, sizeof(stopSessionProcessing));
	Assert(theErr == QTSS_NoErr);

	if (foundSession)
		return; // we didn't allocate the session so don't delete

	OSRef* theSessionRef = theSession->GetRef();
	if (theSessionRef != NULL)
	{
		theSession->TearDownAllOutputs(); // just to be sure because we are about to delete the session.
		sSessionMap->UnRegister(theSessionRef);// we had an error while setting up-- don't let anyone get the session
		//delete theSession;
		CSdpCache::GetInstance()->eraseSdpMap(theSession->GetSourceID()->Ptr);
		theSession->DelRedisLive();
		theSession->Signal(Task::kKillEvent);
	}
}

QTSS_Error AddRTPStream(ReflectorSession* theSession, QTSS_StandardRTSP_Params* inParams, QTSS_RTPStreamObject *newStreamPtr)
{
	// Ok, this is completely crazy but I can't think of a better way to do this that's
	// safe so we'll do it this way for now. Because the ReflectorStreams use this session's
	// stream queue, we need to make sure that each ReflectorStream is not reflecting to this
	// session while we call QTSS_AddRTPStream. One brutal way to do this is to grab each
	// ReflectorStream's mutex, which will stop every reflector stream from running.
	Assert(newStreamPtr != NULL);

	if (theSession != NULL)
		for (uint32_t x = 0; x < theSession->GetNumStreams(); x++)
			theSession->GetStreamByIndex(x)->GetMutex()->Lock();

	//
	// Turn off reliable UDP transport, because we are not yet equipped to
	// do overbuffering.
	QTSS_Error theErr = QTSS_AddRTPStream(inParams->inClientSession, inParams->inRTSPRequest, newStreamPtr, qtssASFlagsForceUDPTransport);

	if (theSession != NULL)
		for (uint32_t y = 0; y < theSession->GetNumStreams(); y++)
			theSession->GetStreamByIndex(y)->GetMutex()->Unlock();

	return theErr;
}

QTSS_Error DoSetup(QTSS_StandardRTSP_Params* inParams)
{
	ReflectorSession* theSession = NULL;

	uint32_t theLen = 0;
	uint32_t *transportModePtr = NULL;
	QTSS_Error theErr = QTSS_GetValuePtr(inParams->inRTSPRequest, qtssRTSPReqTransportMode, 0, (void**)&transportModePtr, &theLen);
	bool isPush = (transportModePtr != NULL && *transportModePtr == qtssRTPTransportModeRecord) ? true : false;
	bool foundSession = false;

	RTPSessionOutput** theOutput = NULL;
	theErr = QTSS_GetValuePtr(inParams->inClientSession, sOutputAttr, 0, (void**)&theOutput, &theLen);
	if (theLen != sizeof(RTPSessionOutput*))
	{
		//theLen = sizeof(theSession);
		//theErr = QTSS_GetValue(inParams->inClientSession, sSessionAttr, 0, &theSession, &theLen);

		if (theErr != QTSS_NoErr && !isPush)
		{
			theSession = DoSessionSetup(inParams, qtssRTSPReqFileName);
			if (theSession == NULL)
				return QTSS_RequestFailed;

			RTPSessionOutput* theNewOutput = new RTPSessionOutput(inParams->inClientSession, theSession, sServerPrefs, sStreamCookieAttr);
			theSession->AddOutput(theNewOutput, true);
			(void)QTSS_SetValue(inParams->inClientSession, sOutputAttr, 0, &theNewOutput, sizeof(theNewOutput));
		}
		else
		{
			uint32_t theLenTemp = sizeof(theSession);
			QTSS_Error theErr = QTSS_GetValue(inParams->inClientSession, sClientBroadcastSessionAttr, 0, &theSession, &theLenTemp);
			if (theSession == NULL)
			{
				theSession = DoSessionSetup(inParams, qtssRTSPReqFileName, isPush, &foundSession);
				if (theSession == NULL)
					return QTSS_RequestFailed;

			}
			else
			{
				;
			}

			theErr = QTSS_SetValue(inParams->inClientSession, sClientBroadcastSessionAttr, 0, &theSession, sizeof(theSession));
			Assert(theErr == QTSS_NoErr);

			//printf("QTSSReflectorModule.cpp:SET Session sClientBroadcastSessionAttr=%"   _U32BITARG_   " theSession=%"   _U32BITARG_   " err=%" _S32BITARG_ " \n",(uint32_t)sClientBroadcastSessionAttr, (uint32_t) theSession,theErr);
			(void)QTSS_SetValue(inParams->inClientSession, qtssCliSesTimeoutMsec, 0, &sBroadcasterSessionTimeoutMilliSecs, sizeof(sBroadcasterSessionTimeoutMilliSecs));
		}
	}
	else
	{
		theSession = (*theOutput)->GetReflectorSession();
		if (theSession == NULL)
			return QTSS_RequestFailed;
	}

	//unless there is a digit at the end of this path (representing trackID), don't
	//even bother with the request
	char* theDigitStr = NULL;
	(void)QTSS_GetValueAsString(inParams->inRTSPRequest, qtssRTSPReqFileDigit, 0, &theDigitStr);
	QTSSCharArrayDeleter theDigitStrDeleter(theDigitStr);
	if (theDigitStr == NULL)
	{
		if (isPush)
			DeleteReflectorPushSession(inParams, theSession, foundSession);
		return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssClientBadRequest, sExpectedDigitFilenameErr);
	}

	uint32_t theTrackID = ::strtol(theDigitStr, NULL, 10);

	if (isPush)
	{
		//printf("QTSSReflectorModule.cpp:DoSetup is push setup\n");

		// Get info about this trackID
		SourceInfo::StreamInfo* theStreamInfo = theSession->GetSourceInfo()->GetStreamInfoByTrackID(theTrackID);
		// If theStreamInfo is NULL, we don't have a legit track, so return an error
		if (theStreamInfo == NULL)
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

		uint16_t theReceiveBroadcastStreamPort = theStreamInfo->fPort;
		theErr = QTSS_SetValue(inParams->inRTSPRequest, qtssRTSPReqSetUpServerPort, 0, &theReceiveBroadcastStreamPort, sizeof(theReceiveBroadcastStreamPort));
		Assert(theErr == QTSS_NoErr);


		QTSS_RTPStreamObject newStream = NULL;
		theErr = AddRTPStream(theSession, inParams, &newStream);
		Assert(theErr == QTSS_NoErr);
		if (theErr != QTSS_NoErr)
		{
			DeleteReflectorPushSession(inParams, theSession, foundSession);
			return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssClientBadRequest, 0);
		}

		//send the setup response

		(void)QTSS_AppendRTSPHeader(inParams->inRTSPRequest, qtssCacheControlHeader,
			kCacheControlHeader.Ptr, kCacheControlHeader.Len);

		(void)QTSS_SendStandardRTSPResponse(inParams->inRTSPRequest, newStream, 0);

		theStreamInfo->fSetupToReceive = true;
		// This is an incoming data session. Set the Reflector Session in the ClientSession
		theErr = QTSS_SetValue(inParams->inClientSession, sClientBroadcastSessionAttr, 0, &theSession, sizeof(theSession));
		Assert(theErr == QTSS_NoErr);

		if (theSession != NULL)
			theSession->AddBroadcasterClientSession(inParams);

#ifdef REFLECTORSESSION_DEBUG
		printf("QTSSReflectorModule.cpp:DoSetup Session =%p refcount=%"   _U32BITARG_   "\n", theSession->GetRef(), theSession->GetRef()->GetRefCount());
#endif

		return QTSS_NoErr;
	}


	// Get info about this trackID
	SourceInfo::StreamInfo* theStreamInfo = theSession->GetSourceInfo()->GetStreamInfoByTrackID(theTrackID);
	// If theStreamInfo is NULL, we don't have a legit track, so return an error
	if (theStreamInfo == NULL)
		return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssClientBadRequest,
			sReflectorBadTrackIDErr);

	StrPtrLen* thePayloadName = &theStreamInfo->fPayloadName;
	QTSS_RTPPayloadType thePayloadType = theStreamInfo->fPayloadType;

	StringParser parser(thePayloadName);

	parser.GetThru(NULL, '/');
	theStreamInfo->fTimeScale = parser.ConsumeInteger(NULL);
	if (theStreamInfo->fTimeScale == 0)
		theStreamInfo->fTimeScale = 90000;

	QTSS_RTPStreamObject newStream = NULL;
	{
		// Ok, this is completely crazy but I can't think of a better way to do this that's
		// safe so we'll do it this way for now. Because the ReflectorStreams use this session's
		// stream queue, we need to make sure that each ReflectorStream is not reflecting to this
		// session while we call QTSS_AddRTPStream. One brutal way to do this is to grab each
		// ReflectorStream's mutex, which will stop every reflector stream from running.

		for (uint32_t x = 0; x < theSession->GetNumStreams(); x++)
			theSession->GetStreamByIndex(x)->GetMutex()->Lock();

		theErr = QTSS_AddRTPStream(inParams->inClientSession, inParams->inRTSPRequest, &newStream, 0);

		for (uint32_t y = 0; y < theSession->GetNumStreams(); y++)
			theSession->GetStreamByIndex(y)->GetMutex()->Unlock();

		if (theErr != QTSS_NoErr)
			return theErr;
	}

	// Set up dictionary items for this stream
	theErr = QTSS_SetValue(newStream, qtssRTPStrPayloadName, 0, thePayloadName->Ptr, thePayloadName->Len);
	Assert(theErr == QTSS_NoErr);
	theErr = QTSS_SetValue(newStream, qtssRTPStrPayloadType, 0, &thePayloadType, sizeof(thePayloadType));
	Assert(theErr == QTSS_NoErr);
	theErr = QTSS_SetValue(newStream, qtssRTPStrTrackID, 0, &theTrackID, sizeof(theTrackID));
	Assert(theErr == QTSS_NoErr);
	theErr = QTSS_SetValue(newStream, qtssRTPStrTimescale, 0, &theStreamInfo->fTimeScale, sizeof(theStreamInfo->fTimeScale));
	Assert(theErr == QTSS_NoErr);

	// We only want to allow over buffering to dynamic rate clients   
	int32_t  canDynamicRate = -1;
	theLen = sizeof(canDynamicRate);
	(void)QTSS_GetValue(inParams->inRTSPRequest, qtssRTSPReqDynamicRateState, 0, (void*)&canDynamicRate, &theLen);
	if (canDynamicRate < 1) // -1 no rate field, 0 off
		(void)QTSS_SetValue(inParams->inClientSession, qtssCliSesOverBufferEnabled, 0, &sFalse, sizeof(sFalse));

	// Place the stream cookie in this stream for future reference
	void* theStreamCookie = theSession->GetStreamCookie(theTrackID);
	Assert(theStreamCookie != NULL);
	theErr = QTSS_SetValue(newStream, sStreamCookieAttr, 0, &theStreamCookie, sizeof(theStreamCookie));
	Assert(theErr == QTSS_NoErr);

	// Set the number of quality levels.
	static uint32_t sNumQualityLevels = ReflectorSession::kNumQualityLevels;
	theErr = QTSS_SetValue(newStream, qtssRTPStrNumQualityLevels, 0, &sNumQualityLevels, sizeof(sNumQualityLevels));
	Assert(theErr == QTSS_NoErr);

	//send the setup response
	(void)QTSS_AppendRTSPHeader(inParams->inRTSPRequest, qtssCacheControlHeader,
		kCacheControlHeader.Ptr, kCacheControlHeader.Len);
	(void)QTSS_SendStandardRTSPResponse(inParams->inRTSPRequest, newStream, qtssSetupRespDontWriteSSRC);

#ifdef REFLECTORSESSION_DEBUG
	printf("QTSSReflectorModule.cpp:DoSetup Session =%p refcount=%"   _U32BITARG_   "\n", theSession->GetRef(), theSession->GetRef()->GetRefCount());
#endif

	return QTSS_NoErr;
}



bool HaveStreamBuffers(QTSS_StandardRTSP_Params* inParams, ReflectorSession* inSession)
{
	if (inSession == NULL || inParams == NULL)
		return false;

	bool haveBufferedStreams = true; // set to false and return if we can't set the packets
	uint32_t y = 0;

	int64_t packetArrivalTime = 0;

	//lock all streams
	for (y = 0; y < inSession->GetNumStreams(); y++)
		inSession->GetStreamByIndex(y)->GetMutex()->Lock();

	QTSS_RTPStreamObject* theRef = NULL;
	uint32_t theLen = 0;
	uint32_t theStreamIndex = 0;
	for (theStreamIndex = 0;
		QTSS_GetValuePtr(inParams->inClientSession, qtssCliSesStreamObjects, theStreamIndex, (void**)&theRef, &theLen) == QTSS_NoErr;
		theStreamIndex++)
	{
		ReflectorStream* theReflectorStream = inSession->GetStreamByIndex(theStreamIndex);

		//  if (!theReflectorStream->HasFirstRTCP())
		//      printf("theStreamIndex =%"   _U32BITARG_   " no rtcp\n", theStreamIndex);

		//  if (!theReflectorStream->HasFirstRTP())
		//      printf("theStreamIndex = %"   _U32BITARG_   " no rtp\n", theStreamIndex);

		if ((theReflectorStream == NULL) || (false == theReflectorStream->HasFirstRTP()))
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

		QTSS_Error theErr = QTSS_SetValue(*theRef, qtssRTPStrFirstSeqNumber, 0, &firstSeqNum, sizeof(firstSeqNum));
		Assert(theErr == QTSS_NoErr);

		theErr = QTSS_SetValue(*theRef, qtssRTPStrFirstTimestamp, 0, &firstTimeStamp, sizeof(firstTimeStamp));
		Assert(theErr == QTSS_NoErr);


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

	if (inSession == NULL)	// 推送端
	{
		if (!sDefaultBroadcastPushEnabled)
			return QTSS_RequestFailed;

		theLen = sizeof(inSession);
		theErr = QTSS_GetValue(inParams->inClientSession, sClientBroadcastSessionAttr, 0, &inSession, &theLen);
		if (theErr != QTSS_NoErr)
			return QTSS_RequestFailed;

		theErr = QTSS_SetValue(inParams->inClientSession, sKillClientsEnabledAttr, 0, &sTearDownClientsOnDisconnect, sizeof(sTearDownClientsOnDisconnect));
		if (theErr != QTSS_NoErr)
			return QTSS_RequestFailed;

		Assert(inSession != NULL);

		theErr = QTSS_SetValue(inParams->inRTSPSession, sRTSPBroadcastSessionAttr, 0, &inSession, sizeof(inSession));
		if (theErr != QTSS_NoErr)
			return QTSS_RequestFailed;

		//printf("QTSSReflectorModule:SET for att err=%" _S32BITARG_ " id=%" _S32BITARG_ "\n",theErr,inParams->inRTSPSession);

		// this code needs to be cleaned up
		// Check and see if the full path to this file matches an existing ReflectorSession
		StrPtrLen thePathPtr;
		OSCharArrayDeleter sdpPath(QTSSModuleUtils::GetFullPath(inParams->inRTSPRequest,
			qtssRTSPReqFilePath,
			&thePathPtr.Len, &sSDPSuffix));

		thePathPtr.Ptr = sdpPath.GetObject();


		// remove trackID designation from the path if it is there
		char *trackStr = thePathPtr.FindString("/trackID=");
		if (trackStr != NULL && *trackStr != 0)
		{
			*trackStr = 0; // terminate the string.
			thePathPtr.Len = ::strlen(thePathPtr.Ptr);
		}

		// If the actual file path has a .sdp in it, first look for the URL without the extra .sdp
		if (thePathPtr.Len > (sSDPSuffix.Len * 2))
		{
			// Check and see if there is a .sdp in the file path.
			// If there is, truncate off our extra ".sdp", cuz it isn't needed
			StrPtrLen endOfPath(&sdpPath.GetObject()[thePathPtr.Len - (sSDPSuffix.Len * 2)], sSDPSuffix.Len);
			if (endOfPath.Equal(sSDPSuffix))
			{
				sdpPath.GetObject()[thePathPtr.Len - sSDPSuffix.Len] = '\0';
				thePathPtr.Len -= sSDPSuffix.Len;
			}
		}

		//// do all above so we can add the session to the map with Resolve here.
		//// we must only do this once.
		//OSRef* debug = sSessionMap->Resolve(&thePathPtr);
		//if (debug != inSession->GetRef())
		//{   
		//	 return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssClientBadRequest, 0);
		//}

		KeepSession(inParams->inRTSPRequest, true);
		//printf("QTSSReflectorModule.cpp:DoPlay (PUSH) inRTSPSession=%"   _U32BITARG_   " inClientSession=%"   _U32BITARG_   "\n",(uint32_t)inParams->inRTSPSession,(uint32_t)inParams->inClientSession);
	}
	else	// 客户端
	{
		RTPSessionOutput**  theOutput = NULL;
		theLen = 0;
		theErr = QTSS_GetValuePtr(inParams->inClientSession, sOutputAttr, 0, (void**)&theOutput, &theLen);
		if ((theErr != QTSS_NoErr) || (theLen != sizeof(RTPSessionOutput*)) || (theOutput == NULL))
			return QTSS_RequestFailed;
		(*theOutput)->InitializeStreams();

		// Tell the session what the bitrate of this reflection is. This is nice for logging,
		// it also allows the server to scale the TCP buffer size appropriately if we are
		// interleaving the data over TCP. This must be set before calling QTSS_Play so the
		// server can use it from within QTSS_Play
		uint32_t bitsPerSecond = inSession->GetBitRate();
		(void)QTSS_SetValue(inParams->inClientSession, qtssCliSesMovieAverageBitRate, 0, &bitsPerSecond, sizeof(bitsPerSecond));

		if (sPlayResponseRangeHeader)
		{
			StrPtrLen temp;
			theErr = QTSS_GetValuePtr(inParams->inClientSession, sRTPInfoWaitTimeAttr, 0, (void**)&temp.Ptr, &temp.Len);
			if (theErr != QTSS_NoErr)
				QTSS_AppendRTSPHeader(inParams->inRTSPRequest, qtssRangeHeader, sTheNowRangeHeader.Ptr, sTheNowRangeHeader.Len);
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

				theErr = QTSS_Play(inParams->inClientSession, inParams->inRTSPRequest, qtssPlayRespWriteTrackInfo);
				if (theErr != QTSS_NoErr)
					return theErr;
			}
			else
			{
				int32_t waitTimeLoopCount = 0;
				theLen = sizeof(waitTimeLoopCount);
				theErr = QTSS_GetValue(inParams->inClientSession, sRTPInfoWaitTimeAttr, 0, &waitTimeLoopCount, &theLen);
				if (theErr != QTSS_NoErr)
					(void)QTSS_SetValue(inParams->inClientSession, sRTPInfoWaitTimeAttr, 0, &sWaitTimeLoopCount, sizeof(sWaitTimeLoopCount));
				else
				{
					if (waitTimeLoopCount < 1)
						return QTSSModuleUtils::SendErrorResponseWithMessage(inParams->inRTSPRequest, qtssClientNotFound, &sBroadcastNotActive);

					waitTimeLoopCount--;
					(void)QTSS_SetValue(inParams->inClientSession, sRTPInfoWaitTimeAttr, 0, &waitTimeLoopCount, sizeof(waitTimeLoopCount));

				}

				//printf("QTSSReflectorModule:DoPlay  wait 100ms waitTimeLoopCount=%ld\n", waitTimeLoopCount);
				int64_t interval = 1 * 100; // 100 millisecond
				QTSS_SetIdleTimer(interval);
				return QTSS_NoErr;
			}
		}
		else
		{
			theErr = QTSS_Play(inParams->inClientSession, inParams->inRTSPRequest, qtssPlayFlagsAppendServerInfo);
			if (theErr != QTSS_NoErr)
				return theErr;

		}

	}

	(void)QTSS_SendStandardRTSPResponse(inParams->inRTSPRequest, inParams->inClientSession, flags);

#ifdef REFLECTORSESSION_DEBUG
	printf("QTSSReflectorModule.cpp:DoPlay Session =%p refcount=%"   _U32BITARG_   "\n", inSession->GetRef(), inSession->GetRef()->GetRefCount());
#endif

	return QTSS_NoErr;
}


bool KillSession(StrPtrLen *sdpPathStr, bool killClients)
{
	OSRef* theSessionRef = sSessionMap->Resolve(sdpPathStr);
	if (theSessionRef != NULL)
	{
		ReflectorSession*   theSession = (ReflectorSession*)theSessionRef->GetObject();
		RemoveOutput(NULL, theSession, killClients);
		(void)QTSS_Teardown(theSession->GetBroadcasterSession());
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
		if (theRef == NULL)
			continue;

		commandPath.Reset();
		commandPath.Put(*(theRef->GetString()));
		commandPath.Put(sSDPKillSuffix);
		commandPath.PutTerminator();

		char *theCommandPath = commandPath.GetBufPtr();
		QTSS_Object outFileObject;
		QTSS_Error  err = QTSS_OpenFileObject(theCommandPath, qtssOpenFileNoFlags, &outFileObject);
		if (err == QTSS_NoErr)
		{
			(void)QTSS_CloseFileObject(outFileObject);
			::unlink(theCommandPath);
			KillSession(theRef->GetString(), true);
		}
	}

}

QTSS_Error DestroySession(QTSS_ClientSessionClosing_Params* inParams)
{
	RTPSessionOutput**  theOutput = NULL;
	ReflectorOutput*    outputPtr = NULL;
	ReflectorSession*   theSession = NULL;

	OSMutexLocker locker(sSessionMap->GetMutex());

	uint32_t theLen = sizeof(theSession);
	QTSS_Error theErr = QTSS_GetValue(inParams->inClientSession, sClientBroadcastSessionAttr, 0, &theSession, &theLen);
	//printf("QTSSReflectorModule.cpp:DestroySession    sClientBroadcastSessionAttr=%"   _U32BITARG_   " theSession=%"   _U32BITARG_   " err=%" _S32BITARG_ " \n",(uint32_t)sClientBroadcastSessionAttr, (uint32_t)theSession,theErr);

	if (theSession != NULL)	// 推送端
	{
		ReflectorSession*   deletedSession = NULL;
		theErr = QTSS_SetValue(inParams->inClientSession, sClientBroadcastSessionAttr, 0, &deletedSession, sizeof(deletedSession));

		SourceInfo* theSoureInfo = theSession->GetSourceInfo();
		if (theSoureInfo == NULL)
			return QTSS_NoErr;

		uint32_t  numStreams = theSession->GetNumStreams();
		SourceInfo::StreamInfo* theStreamInfo = NULL;

		for (uint32_t index = 0; index < numStreams; index++)
		{
			theStreamInfo = theSoureInfo->GetStreamInfo(index);
			if (theStreamInfo != NULL)
				theStreamInfo->fSetupToReceive = false;
		}

		bool killClients = false; // the pref as the default
		uint32_t theLenTemp = sizeof(killClients);
		(void)QTSS_GetValue(inParams->inClientSession, sKillClientsEnabledAttr, 0, &killClients, &theLenTemp);

		//printf("QTSSReflectorModule.cpp:DestroySession broadcaster theSession=%"   _U32BITARG_   "\n", (uint32_t) theSession);
		theSession->RemoveSessionFromOutput(inParams->inClientSession);

		RemoveOutput(NULL, theSession, killClients);
	}
	else // 客户端
	{
		theLen = 0;
		theErr = QTSS_GetValuePtr(inParams->inClientSession, sOutputAttr, 0, (void**)&theOutput, &theLen);
		if ((theErr != QTSS_NoErr) || (theLen != sizeof(RTPSessionOutput*)) || (theOutput == NULL) || (*theOutput == NULL))
			return QTSS_RequestFailed;
		theSession = (*theOutput)->GetReflectorSession();

		if (theOutput != NULL)
			outputPtr = (ReflectorOutput*)*theOutput;

		if (outputPtr != NULL)
		{
			RemoveOutput(outputPtr, theSession, false);
			RTPSessionOutput* theOutput = NULL;
			(void)QTSS_SetValue(inParams->inClientSession, sOutputAttr, 0, &theOutput, sizeof(theOutput));
		}

	}

	return QTSS_NoErr;
}

void RemoveOutput(ReflectorOutput* inOutput, ReflectorSession* theSession, bool killClients)
{
	// 对ReflectorSession的引用继续处理,包括推送端和客户端
	Assert(theSession);
	if (theSession != NULL)
	{
		if (inOutput != NULL)
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
		if (theSessionRef != NULL)
		{
			//printf("QTSSReflectorModule.cpp:RemoveOutput UnRegister session =%p refcount=%"   _U32BITARG_   "\n", theSessionRef, theSessionRef->GetRefCount() ) ;       
			if (inOutput != NULL)
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

				theSession->Signal(Task::kKillEvent);
			}
		}
	}
	delete inOutput;
}


bool AcceptSession(QTSS_StandardRTSP_Params* inParams)
{
	QTSS_RTSPSessionObject inRTSPSession = inParams->inRTSPSession;
	QTSS_RTSPRequestObject theRTSPRequest = inParams->inRTSPRequest;

	QTSS_ActionFlags action = QTSSModuleUtils::GetRequestActions(theRTSPRequest);
	if (action != qtssActionFlagsWrite)
		return false;

	if (QTSSModuleUtils::UserInGroup(QTSSModuleUtils::GetUserProfileObject(theRTSPRequest), sBroadcasterGroup.Ptr, sBroadcasterGroup.Len))
		return true; // ok we are allowing this broadcaster user

	char remoteAddress[20] = { 0 };
	StrPtrLen theClientIPAddressStr(remoteAddress, sizeof(remoteAddress));
	QTSS_Error err = QTSS_GetValue(inRTSPSession, qtssRTSPSesRemoteAddrStr, 0, (void*)theClientIPAddressStr.Ptr, &theClientIPAddressStr.Len);
	if (err != QTSS_NoErr)
		return false;

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
		QTSS_RTSPRequestObject request = inParams->inRTSPRequest;
		(void)QTSSModuleUtils::AuthorizeRequest(request, &allowed, &allowed, &allowed);
		return QTSS_NoErr;
	}

	bool allowNoAccessFiles = false;
	QTSS_ActionFlags noAction = ~qtssActionFlagsWrite; //no action anything but a write
	QTSS_ActionFlags authorizeAction = QTSSModuleUtils::GetRequestActions(inParams->inRTSPRequest);
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
	QTSS_RTSPRequestObject theRequest = inParams->inRTSPRequest;

	char* requestPathStr;
	(void)QTSS_GetValueAsString(theRequest, qtssRTSPReqFilePath, 0, &requestPathStr);
	QTSSCharArrayDeleter requestPathStrDeleter(requestPathStr);
	StrPtrLen theRequestPath(requestPathStr);
	StringParser theRequestPathParser(&theRequestPath);

	// request path begins with a '/' for ex. /mysample.mov or /redirect_broadcast_keyword/mysample.mov
	theRequestPathParser.Expect(kPathDelimiterChar);

	StrPtrLen theFirstPath;
	theRequestPathParser.ConsumeUntil(&theFirstPath, kPathDelimiterChar);
	Assert(theFirstPath.Len != 0);

	// If the redirect_broadcast_keyword and redirect_broadcast_dir prefs are set & the first part of the path matches the keyword
	if ((sRedirectBroadcastsKeyword && sRedirectBroadcastsKeyword[0] != 0)
		&& (sBroadcastsRedirectDir && sBroadcastsRedirectDir[0] != 0)
		&& theFirstPath.EqualIgnoreCase(sRedirectBroadcastsKeyword, ::strlen(sRedirectBroadcastsKeyword)))
	{
		// set qtssRTSPReqRootDir
		(void)QTSS_SetValue(theRequest, qtssRTSPReqRootDir, 0, sBroadcastsRedirectDir, ::strlen(sBroadcastsRedirectDir));

		// set the request file path to the new path with the keyword stripped
		StrPtrLen theStrippedRequestPath;
		theRequestPathParser.ConsumeLength(&theStrippedRequestPath, theRequestPathParser.GetDataRemaining());
		(void)QTSS_SetValue(theRequest, qtssRTSPReqFilePath, 0, theStrippedRequestPath.Ptr, theStrippedRequestPath.Len);
	}

	return QTSS_NoErr;
}

bool AllowBroadcast(QTSS_RTSPRequestObject inRTSPRequest)
{
	// If reflection of broadcasts is disabled, return false
	if (!sReflectBroadcasts)
		return false;

	// If request path is not in any of the broadcast_dir paths, return false
	if (!InBroadcastDirList(inRTSPRequest))
		return false;

	return true;
}

bool InBroadcastDirList(QTSS_RTSPRequestObject inRTSPRequest)
{
	//Babosa 2016.12.2
	return true;

	bool allowed = false;

	char* theURIPathStr;
	(void)QTSS_GetValueAsString(inRTSPRequest, qtssRTSPReqFilePath, 0, &theURIPathStr);
	QTSSCharArrayDeleter requestPathStrDeleter(theURIPathStr);

	char* theLocalPathStr;
	(void)QTSS_GetValueAsString(inRTSPRequest, qtssRTSPReqLocalPath, 0, &theLocalPathStr);
	StrPtrLenDel requestPath(theLocalPathStr);

	char* theRequestPathStr = NULL;
	char* theBroadcastDirStr = NULL;
	bool isURI = true;

	uint32_t index = 0;
	uint32_t numValues = 0;

	(void)QTSS_GetNumValues(sPrefs, sBroadcastDirListID, &numValues);

	if (numValues == 0)
		return true;

	while (!allowed && (index < numValues))
	{
		(void)QTSS_GetValueAsString(sPrefs, sBroadcastDirListID, index, &theBroadcastDirStr);
		StrPtrLen theBroadcastDir(theBroadcastDirStr);

		if (theBroadcastDir.Len == 0) // an empty dir matches all
			return true;

		if (IsAbsolutePath(&theBroadcastDir))
		{
			theRequestPathStr = theLocalPathStr;
			isURI = false;
		}
		else
			theRequestPathStr = theURIPathStr;

		StrPtrLen requestPath(theRequestPathStr);
		StringParser requestPathParser(&requestPath);
		StrPtrLen pathPrefix;
		if (isURI)
			requestPathParser.Expect(kPathDelimiterChar);
		requestPathParser.ConsumeLength(&pathPrefix, theBroadcastDir.Len);

		// if the first part of the request path matches the broadcast_dir path, return true
		if (pathPrefix.Equal(theBroadcastDir))
			allowed = true;

		(void)QTSS_Delete(theBroadcastDirStr);

		index++;
	}

	return allowed;
}

bool IsAbsolutePath(StrPtrLen *inPathPtr)
{
	StringParser thePathParser(inPathPtr);

#ifdef __Win32__
	if ((thePathParser[1] == ':') && (thePathParser[2] == kPathDelimiterChar))
#else
	if (thePathParser.PeekFast() == kPathDelimiterChar)
#endif
		return true;

	return false;
}

QTSS_Error GetDeviceStream(Easy_GetDeviceStream_Params* inParams)
{
	QTSS_Error theErr = QTSS_Unimplemented;

	if (inParams->inDevice && inParams->inStreamType == easyRTSPType)
	{

		OSMutexLocker locker(sSessionMap->GetMutex());

		char theStreamName[QTSS_MAX_NAME_LENGTH] = { 0 };
		sprintf(theStreamName, "%s%s%d", inParams->inDevice, EASY_KEY_SPLITER, inParams->inChannel);

		StrPtrLen inPath(theStreamName);

		OSRef* theSessionRef = sSessionMap->Resolve(&inPath);
		ReflectorSession* theSession = NULL;

		if (theSessionRef)
		{
			theSession = (ReflectorSession*)theSessionRef->GetObject();
			QTSS_ClientSessionObject clientSession = theSession->GetBroadcasterSession();
			Assert(theSession != NULL);

			if (clientSession)
			{
				char* theFullRequestURL = NULL;
				(void)QTSS_GetValueAsString(clientSession, qtssCliSesFullURL, 0, &theFullRequestURL);
				QTSSCharArrayDeleter theFileNameStrDeleter(theFullRequestURL);

				if (theFullRequestURL && inParams->outUrl)
				{
					strcpy(inParams->outUrl, theFullRequestURL);
					theErr = QTSS_NoErr;
				}
			}
			sSessionMap->Release(theSessionRef);
		}
	}

	return theErr;
}