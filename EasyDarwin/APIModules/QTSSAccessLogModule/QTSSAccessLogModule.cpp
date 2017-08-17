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
	 File:       QTSSAccessLogModule.cpp

	 Contains:   Implementation of an RTP access log module.


 */

#include "QTSSDictionary.h"
#include "QTSSAccessLogModule.h"
#include "QTSSModuleUtils.h"
#include "QTSSRollingLog.h"
#include "OSMutex.h"
#include "MyAssert.h"
#include "StringParser.h"
#include "StringFormatter.h"
#include "StringTranslator.h"
#include "StrPtrLen.h"
#include "UserAgentParser.h"
#include "Task.h"
#include "OS.h"
#include "RTPSession.h"
#include "RTSPRequest.h"
#include <ctime>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/io_service.hpp>
#define TESTUNIXTIME 0

class QTSSAccessLog;
class LogCheckTask;

// STATIC DATA
static boost::string_view        sLoggedAuthorizationName = "QTSSAccessLogModuleLoggedAuthorization";

// Default values for preferences
static bool   sDefaultLogEnabled = true;
static char*    sDefaultLogName = "StreamingServer";
static char*    sDefaultLogDir = nullptr;
static QTSS_PrefsObject     sServerPrefs = nullptr;

static uint32_t   sDefaultMaxLogBytes = 10240000;
static uint32_t   sDefaultRollInterval = 7;
static char*    sVoidField = "-";
static bool   sStartedUp = false;
static bool   sDefaultLogTimeInGMT = true;

// Current values for preferences
static bool   sLogEnabled = true;
static uint32_t   sMaxLogBytes = 51200000;
static uint32_t   sRollInterval = 7;
static bool   sLogTimeInGMT = true;

static OSMutex*             sLogMutex = nullptr;//Log module isn't reentrant
static QTSSAccessLog*       sAccessLog = nullptr;
static QTSS_ServerObject    sServer = nullptr;
static QTSS_ModulePrefsObject sPrefs = nullptr;
static LogCheckTask* sLogCheckTask = nullptr;

// This header conforms to the W3C "Extended Log File Format". 
// (See "http://www.w3.org/TR/WD-logfile.html" for details.)
// The final remark filed of the log header tells us if the logged times are in GMT or in system local time.
static char* sLogHeader = "#Software: %s\n"
"#Version: %s\n"    //%s == version
"#Date: %s\n"   //%s == date/time
"#Remark: all time values are in %s.\n" //%s == std::localtime or GMT
"#Fields: c-ip date time c-dns cs-uri-stem c-starttime x-duration c-rate c-status c-playerid"
" c-playerversion c-playerlanguage cs(User-Agent) c-os"
" c-osversion c-cpu filelength filesize avgbandwidth protocol transport audiocodec videocodec"
" sc-bytes cs-bytes c-bytes s-pkts-sent c-pkts-received c-pkts-lost-client c-buffercount"
" c-totalbuffertime c-quality s-ip s-dns s-totalclients s-cpu-util cs-uri-query c-username sc(Realm) \n";



//**************************************************
// CLASS DECLARATIONS
//**************************************************
extern boost::asio::io_service io_service;
class LogCheckTask
{
public:
	LogCheckTask() : timer(io_service) {
		timer.async_wait(std::bind(&LogCheckTask::Run, this, std::placeholders::_1));
	}
	~LogCheckTask() = default;
	void StopTimer() { timer.cancel(); }
private:
	boost::asio::steady_timer timer;
	void Run(const boost::system::error_code &ec);
};

class QTSSAccessLog : public QTSSRollingLog
{
public:

	QTSSAccessLog() = default;
	~QTSSAccessLog() override = default;

	char* GetLogName() override { return QTSSModuleUtils::GetStringAttribute(sPrefs, "request_logfile_name", sDefaultLogName); }
	char* GetLogDir() override { return QTSSModuleUtils::GetStringAttribute(sPrefs, "request_logfile_dir", sDefaultLogDir); }
	uint32_t GetRollIntervalInDays() override { return sRollInterval; }
	uint32_t GetMaxLogBytes() override { return sMaxLogBytes; }
	time_t WriteLogHeader(FILE *inFile) override;

};

// FUNCTION PROTOTYPES

static QTSS_Error   QTSSAccessLogModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParamBlock);
static QTSS_Error   Register(QTSS_Register_Params* inParams);
static QTSS_Error   Initialize(QTSS_Initialize_Params* inParams);
static QTSS_Error   RereadPrefs();
static QTSS_Error   Shutdown();
static QTSS_Error   PostProcess(QTSS_StandardRTSP_Params* inParams);
static QTSS_Error   ClientSessionClosing(QTSS_ClientSessionClosing_Params* inParams);
static QTSS_Error   LogRequest(RTPSession* inClientSession,
	QTSS_RTSPSessionObject inRTSPSession, QTSS_CliSesClosingReason *inCloseReasonPtr);
static void             CheckAccessLogState(bool forceEnabled);
static QTSS_Error   RollAccessLog(QTSS_ServiceFunctionArgsPtr inArgs);
static void         ReplaceSpaces(StrPtrLen *sourcePtr, StrPtrLen *destPtr, char *replaceStr);

static QTSS_Error   StateChange(QTSS_StateChange_Params* stateChangeParams);
static void         WriteStartupMessage();
static void         WriteShutdownMessage();

// FUNCTION IMPLEMENTATIONS

QTSS_Error QTSSAccessLogModule_Main(void* inPrivateArgs)
{
	return _stublibrary_main(inPrivateArgs, QTSSAccessLogModuleDispatch);
}

QTSS_Error QTSSAccessLogModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParamBlock)
{
	switch (inRole)
	{
	case QTSS_Register_Role:
		return Register(&inParamBlock->regParams);
	case QTSS_StateChange_Role:
		return StateChange(&inParamBlock->stateChangeParams);
	case QTSS_Initialize_Role:
		return Initialize(&inParamBlock->initParams);
	case QTSS_RereadPrefs_Role:
		return RereadPrefs();
	case QTSS_RTSPPostProcessor_Role:
		return PostProcess(&inParamBlock->rtspPostProcessorParams);
	case QTSS_ClientSessionClosing_Role:
		return ClientSessionClosing(&inParamBlock->clientSessionClosingParams);
	case QTSS_Shutdown_Role:
		return Shutdown();
	}
	return QTSS_NoErr;
}


QTSS_Error Register(QTSS_Register_Params* inParams)
{
	sLogMutex = new OSMutex();

	// Do role & service setup

	(void)QTSS_AddRole(QTSS_Initialize_Role);
	(void)QTSS_AddRole(QTSS_RTSPPostProcessor_Role);
	(void)QTSS_AddRole(QTSS_ClientSessionClosing_Role);
	(void)QTSS_AddRole(QTSS_RereadPrefs_Role);
	(void)QTSS_AddRole(QTSS_Shutdown_Role);
	(void)QTSS_AddRole(QTSS_StateChange_Role);

	(void)QTSS_AddService("RollAccessLog", &RollAccessLog);

	// Tell the server our name!
	static char* sModuleName = "QTSSAccessLogModule";
	::strcpy(inParams->outModuleName, sModuleName);

	return QTSS_NoErr;
}


QTSS_Error Initialize(QTSS_Initialize_Params* inParams)
{
	QTSSModuleUtils::Initialize(inParams->inMessages, inParams->inServer, inParams->inErrorLogStream);
	sServer = inParams->inServer;
	sPrefs = QTSSModuleUtils::GetModulePrefsObject(inParams->inModule);
	sServerPrefs = inParams->inPrefs;

	RereadPrefs();
	WriteStartupMessage();
	sLogCheckTask = new LogCheckTask();
	return QTSS_NoErr;
}


QTSS_Error RereadPrefs()
{
	delete[] sDefaultLogDir;
	(void)((QTSSDictionary*)sServerPrefs)->GetValueAsString(qtssPrefsErrorLogDir, 0, &sDefaultLogDir);

	QTSSModuleUtils::GetAttribute(sPrefs, "request_logging", qtssAttrDataTypeBool16,
		&sLogEnabled, &sDefaultLogEnabled, sizeof(sLogEnabled));
	QTSSModuleUtils::GetAttribute(sPrefs, "request_logfile_size", qtssAttrDataTypeUInt32,
		&sMaxLogBytes, &sDefaultMaxLogBytes, sizeof(sMaxLogBytes));
	QTSSModuleUtils::GetAttribute(sPrefs, "request_logfile_interval", qtssAttrDataTypeUInt32,
		&sRollInterval, &sDefaultRollInterval, sizeof(sRollInterval));
	QTSSModuleUtils::GetAttribute(sPrefs, "request_logtime_in_gmt", qtssAttrDataTypeBool16,
		&sLogTimeInGMT, &sDefaultLogTimeInGMT, sizeof(sLogTimeInGMT));

	CheckAccessLogState(false);

	return QTSS_NoErr;
}


QTSS_Error Shutdown()
{
	WriteShutdownMessage();
	if (sLogCheckTask != nullptr)
	{
		//sLogCheckTask is a task object, so don't delete it directly
		// instead we signal it to kill itself.
		sLogCheckTask->StopTimer();
		sLogCheckTask = nullptr;
	}
	return QTSS_NoErr;
}

QTSS_Error StateChange(QTSS_StateChange_Params* stateChangeParams)
{
	if (stateChangeParams->inNewState == qtssIdleState)
		WriteShutdownMessage();
	else if (stateChangeParams->inNewState == qtssRunningState)
		WriteStartupMessage();

	return QTSS_NoErr;
}



QTSS_Error PostProcess(QTSS_StandardRTSP_Params* inParams)
{
	static uint32_t sZero = 0;

	RTSPRequest *dict = (RTSPRequest *)inParams->inRTSPRequest;
	uint32_t theStatus = dict->GetRealStatusCode();

	QTSS_CliSesClosingReason theReason = qtssCliSesCloseClientTeardown;

	if ((theStatus == 401) || (theStatus == 403))
	{
		LogRequest(inParams->inClientSession, nullptr, &theReason);
		inParams->inClientSession->addAttribute(sLoggedAuthorizationName, theStatus);
	}
	else
		inParams->inClientSession->addAttribute(sLoggedAuthorizationName, sZero);

	return QTSS_NoErr;
}

#if TESTUNIXTIME
void TestUnixTime(time_t theTime, char *ioDateBuffer);
void TestUnixTime(time_t theTime, char *ioDateBuffer)
{
	Assert(NULL != ioDateBuffer);

	//use ansi routines for getting the date.
	time_t calendarTime = theTime;
	Assert(-1 != calendarTime);
	if (-1 == calendarTime)
		return;

	struct tm* theLocalTime = std::localtime(&calendarTime);
	Assert(NULL != theLocalTime);
	if (NULL == theLocalTime)
		return;

	//date needs to look like this for common log format: 29/Sep/1998:11:34:54 -0700
	//this wonderful ANSI routine just does it for you.
	//strftime(ioDateBuffer, kMaxDateBufferSize, "%d/%b/%Y:%H:%M:%S", theLocalTime);
	strftime(ioDateBuffer, QTSSRollingLog::kMaxDateBufferSizeInBytes, "%Y-%m-%d %H:%M:%S", theLocalTime);
	return;
}
#endif


QTSS_Error ClientSessionClosing(QTSS_ClientSessionClosing_Params* inParams)
{
	return LogRequest(inParams->inClientSession, nullptr, &inParams->inReason);
}

void ReplaceSpaces(StrPtrLen *sourcePtr, StrPtrLen *destPtr, char *replaceStr)
{

	if ((nullptr != destPtr) && (nullptr != destPtr->Ptr) && (0 < destPtr->Len)) destPtr->Ptr[0] = 0;
	do
	{
		if ((nullptr == sourcePtr)
			|| (nullptr == destPtr)
			|| (nullptr == sourcePtr->Ptr)
			|| (nullptr == destPtr->Ptr)
			|| (0 == sourcePtr->Len)
			|| (0 == destPtr->Len)
			)    break;

		if (0 == sourcePtr->Ptr[0])
		{
			destPtr->Len = 0;
			break;
		}

		const StrPtrLen replaceValue(replaceStr);
		StringFormatter formattedString(destPtr->Ptr, destPtr->Len);
		StringParser sourceStringParser(sourcePtr);
		StrPtrLen preStopChars;

		do
		{
			sourceStringParser.ConsumeUntil(&preStopChars, StringParser::sEOLWhitespaceMask);
			if (preStopChars.Len > 0)
			{
				formattedString.Put(preStopChars);// copy the string up to the space or eol. it will be truncated if there's not enough room.
				if (sourceStringParser.Expect(' ') && (formattedString.GetSpaceLeft() > replaceValue.Len))
				{
					formattedString.Put(replaceValue.Ptr, replaceValue.Len);
				}
				else //no space character or no room for replacement
				{
					break;
				}
			}

		} while (preStopChars.Len != 0);

		destPtr->Set(formattedString.GetBufPtr(), formattedString.GetBytesWritten());

	} while (false);
}


QTSS_Error LogRequest(RTPSession* inClientSession,
	QTSS_RTSPSessionObject inRTSPSession, QTSS_CliSesClosingReason *inCloseReasonPtr)
{
	static StrPtrLen sUnknownStr(sVoidField);
	static StrPtrLen sTCPStr("TCP");
	static StrPtrLen sUDPStr("UDP");

	//Fetch the URL, user agent, movielength & movie bytes to log out of the RTP session
	enum {
		eTempLogItemSize = 256, // must be same or larger than others
		eURLSize = 256,
		eUserAgentSize = 256,
		ePlayerIDSize = 32,
		ePlayerVersionSize = 32,
		ePlayerLangSize = 32,
		ePlayerOSSize = 32,
		ePlayerOSVersSize = 32,
		ePlayerCPUSize = 32
	};

	char tempLogItemBuf[eTempLogItemSize] = { 0 };
	StrPtrLen tempLogStr(tempLogItemBuf, eTempLogItemSize - 1);

	//
	// Check to see if this session is closing because authorization failed. If that's
	// the case, we've logged that already, let's not log it twice

	uint32_t theLen = 0;
	auto opt = inClientSession->getAttribute(sLoggedAuthorizationName);
	if (opt && boost::any_cast<uint32_t>(opt.value()) > 0)
		return QTSS_NoErr;

	///inClientSession should never be NULL
	//inRTSPRequest may be NULL if this is a timeout

	OSMutexLocker locker(sLogMutex);
	CheckAccessLogState(false);
	if (sAccessLog == nullptr)
		return QTSS_NoErr;

	//if logging is on, then log the request... first construct a timestamp
	char theDateBuffer[QTSSRollingLog::kMaxDateBufferSizeInBytes];
	bool result = QTSSRollingLog::FormatDate(theDateBuffer, sLogTimeInGMT);


	//for now, just ignore the error.
	if (!result)
		theDateBuffer[0] = '\0';

	theLen = sizeof(QTSS_RTSPSessionObject);
	RTSPSessionInterface* theRTSPSession = (RTSPSessionInterface*)inRTSPSession;
	if (theRTSPSession == nullptr)
		theRTSPSession = inClientSession->GetRTSPSession();

	// Get lots of neat info to log from the various dictionaries

	// Each attribute must be copied out to ensure that it is NULL terminated.
	// To ensure NULL termination, just memset the buffers to 0, and make sure that
	// the last byte of each array is untouched.

	uint32_t clientPacketsReceived = 0;
	uint32_t clientPacketsLost = 0;

	// First, get networking info from the RTSP session
	std::string localIPAddr(inClientSession->GetLocalAddr());
	std::string localDNS(inClientSession->GetLocalDNS());
	std::string remoteDNS(inClientSession->GetHost());
	std::string remoteAddr(inClientSession->GetRemoteAddr());
	std::string playerID(inClientSession->GetRemoteAddr());

	// Second, get networking info from the Client's session.
	// (Including the stats for incoming RTCP packets.)
	char urlBuf[eURLSize] = { 0 };
	RTPSession *rtpSession = (RTPSession *)inClientSession;
	std::string url(rtpSession->GetPresentationURL());
	float packetLossPercent = inClientSession->GetPacketLossPercent();
	double movieDuration = inClientSession->GetMovieDuration();
	uint64_t movieSizeInBytes = inClientSession->GetMovieSizeInBytes();
	uint32_t movieAverageBitRatePtr = inClientSession->GetMovieAvgBitrate();
	int64_t theCreateTime = inClientSession->GetSessionCreateTime();
	int64_t thePlayTime = inClientSession->GetFirstPlayTime();
	uint32_t rtpBytesSent = inClientSession->GetBytesSent();
	uint32_t rtpPacketsSent = inClientSession->GetPacketsSent();
	uint32_t rtcpBytesRecv = inClientSession->GetTotalRTCPBytesRecv();

	uint32_t startPlayTimeInSecs = (uint32_t)(((theCreateTime - thePlayTime) / 1000) + 0.5);


	// We need a value of 'c-bytes' to report as a log entry. This is supposed to be the total number
	// of bytes the client has received during the session. Unfortunately, the QT client does not give
	// us this number. We will use the following heuristic formula to estimate the number of bytes the
	// client has received during the session:
	//
	//     client-bytes-received = bytes-sent * (100.0 - percent-packet-lost) / 100.0
	//
	// The 'percent-packet-lost' value has been calculated internally by QTSS based on the RTCP packets
	// sent to the server from the client. If those values are accurate then the above formula will not 
	// be exactly correct but it will be nearly correct.

	auto clientBytesRecv = (uint32_t)((rtpBytesSent * (100.0 - packetLossPercent)) / 100.0);

	tempLogStr.Ptr[0] = 0; tempLogStr.Len = eUserAgentSize;
	std::string userAgentV(inClientSession->GetUserAgent());
	tempLogStr.Set((char *)userAgentV.c_str());

	char userAgentBuf[eUserAgentSize] = { 0 };
	StrPtrLen userAgent(userAgentBuf, eUserAgentSize - 1);
	ReplaceSpaces(&tempLogStr, &userAgent, "%20");

	UserAgentParser userAgentParser(&userAgent);

	//  StrPtrLen* playerID = userAgentParser.GetUserID() ;
	StrPtrLen* playerVersion = userAgentParser.GetUserVersion();
	StrPtrLen* playerLang = userAgentParser.GetUserLanguage();
	StrPtrLen* playerOS = userAgentParser.GetrUserOS();
	StrPtrLen* playerOSVers = userAgentParser.GetUserOSVersion();
	StrPtrLen* playerCPU = userAgentParser.GetUserCPU();

	//  char playerIDBuf[ePlayerIDSize] = {};   
	char playerVersionBuf[ePlayerVersionSize] = { 0 };
	char playerLangBuf[ePlayerLangSize] = { 0 };
	char playerOSBuf[ePlayerOSSize] = { 0 };
	char playerOSVersBuf[ePlayerOSVersSize] = { 0 };
	char playerCPUBuf[ePlayerCPUSize] = { 0 };

	uint32_t size;
	//  (ePlayerIDSize < playerID->Len ) ? size = ePlayerIDSize -1 : size = playerID->Len;
	//  if (playerID->Ptr != NULL) memcpy (playerIDBuf, playerID->Ptr, size);

	(ePlayerVersionSize < playerVersion->Len) ? size = ePlayerVersionSize - 1 : size = playerVersion->Len;
	if (playerVersion->Ptr != nullptr) memcpy(playerVersionBuf, playerVersion->Ptr, size);

	(ePlayerLangSize < playerLang->Len) ? size = ePlayerLangSize - 1 : size = playerLang->Len;
	if (playerLang->Ptr != nullptr) memcpy(playerLangBuf, playerLang->Ptr, size);

	(ePlayerOSSize < playerOS->Len) ? size = ePlayerOSSize - 1 : size = playerOS->Len;
	if (playerOS->Ptr != nullptr)  memcpy(playerOSBuf, playerOS->Ptr, size);

	(ePlayerOSVersSize < playerOSVers->Len) ? size = ePlayerOSVersSize - 1 : size = playerOSVers->Len;
	if (playerOSVers->Ptr != nullptr) memcpy(playerOSVersBuf, playerOSVers->Ptr, size);

	(ePlayerCPUSize < playerCPU->Len) ? size = ePlayerCPUSize - 1 : size = playerCPU->Len;
	if (playerCPU->Ptr != nullptr) memcpy(playerCPUBuf, playerCPU->Ptr, size);


	// clientPacketsReceived, clientPacketsLost, videoPayloadName and audioPayloadName
	// are all stored on a per-stream basis, so let's iterate through all the streams,
	// finding this information

	boost::string_view videoPayloadName;
	boost::string_view audioPayloadName;

	uint32_t qualityLevel = 0;
	uint32_t clientBufferTime = 0;
	uint32_t theStreamIndex = 0;
	QTSS_RTPStreamObject theRTPStreamObject = nullptr;

	for (auto theRTPStreamObject : rtpSession->GetStreams())
	{	
		RTPStream *dict = (RTPStream *)theRTPStreamObject;
		uint32_t streamPacketsReceived = dict->GetTotalPacketsRecv();
		uint32_t streamPacketsLost = dict->GetTotalLostPackets();

		// Add up packets received and packets lost to come up with a session wide total
		clientPacketsReceived += streamPacketsReceived;
		clientPacketsLost += streamPacketsLost;

		// Identify the video and audio codec types
		QTSS_RTPPayloadType thePayloadType = dict->GetPayLoadType();
		if (thePayloadType == qtssVideoPayloadType)
			videoPayloadName = dict->GetPayloadName();
		else if (thePayloadType == qtssAudioPayloadType)
			audioPayloadName = dict->GetPayloadName();

		float clientBufferTimePtr = dict->GetBufferDelay();
		if (clientBufferTimePtr != 0)
		{
			if (clientBufferTimePtr > clientBufferTime)
				clientBufferTime = (uint32_t)(clientBufferTimePtr + .5); // round up to full seconds
		}

	}

	// Add the client buffer time to our client start latency (in whole seconds).
	startPlayTimeInSecs += clientBufferTime;

	if (rtpPacketsSent == 0) // no packets sent
		qualityLevel = 0; // no quality
	else
	{
		if ((clientPacketsReceived == 0) && (clientPacketsLost == 0)) // no info from client 
			qualityLevel = 100; //so assume 100
		else
		{
			float qualityPercent = (float)clientPacketsReceived / (float)(clientPacketsReceived + clientPacketsLost);
			qualityPercent += (float).005; // round up
			qualityLevel = (uint32_t)((float) 100.0 * qualityPercent); // average of sum of packet counts for all streams
		}
	}

	//we may not have an RTSP request. Assume that the status code is 504 timeout, if there is an RTSP
	//request, though, we can find out what the real status code of the response is
	static uint32_t sTimeoutCode = 504;
	uint32_t theStatusCode = inClientSession->GetStatusCode();
	//  printf("qtssCliRTSPReqRealStatusCode = %"   _U32BITARG_   " \n", *theStatusCode);


	if (inCloseReasonPtr) do
	{
		if (theStatusCode < 300) // it was a succesful RTSP request but...
		{
			if (*inCloseReasonPtr == qtssCliSesCloseTimeout) // there was a timeout
			{
				theStatusCode = sTimeoutCode;
				//                  printf(" log timeout ");
				break;
			}

			if (*inCloseReasonPtr == qtssCliSesCloseClientTeardown) // there was a teardown
			{

				static QTSS_CliSesClosingReason sReason = qtssCliSesCloseClientTeardown;
				QTSS_CliSesClosingReason theReasonPtr = inClientSession->GetTeardownReason();

				if (theReasonPtr == qtssCliSesTearDownClientRequest) //  the client asked for a tear down
				{
					//                  printf(" client requests teardown  ");
					break;
				}

				if (theReasonPtr == qtssCliSesTearDownUnsupportedMedia) //  An error occured while streaming the file.
				{
					theStatusCode = 415;
					//                      printf(" log UnsupportedMedia ");
					break;
				}
				if (theReasonPtr == qtssCliSesTearDownBroadcastEnded) //  a broadcaster stopped broadcasting
				{
					theStatusCode = 452;
					//                      printf(" log broadcast removed ");
					break;
				}

				theStatusCode = 500; // some unknown reason for cancelling the connection
			}
		}

	} while (false);

	//  printf(" = %"   _U32BITARG_   " \n", *theStatusCode);


		// Find out what time it is
	int64_t curTime = OS::Milliseconds();

	uint32_t numCurClients = 0;
	theLen = sizeof(numCurClients);
	(void)((QTSSDictionary*)sServer)->GetValue(qtssRTPSvrCurConn, 0, &numCurClients, &theLen);

	/*
		IMPORTANT!!!!

		Some values such as cpu, #conns, need to be grabbed as the session starts, not when the teardown happened (I think)

	*/

#if TESTUNIXTIME
	char thetestDateBuffer[QTSSRollingLog::kMaxDateBufferSizeInBytes];
	TestUnixTime(QTSS_MilliSecsTo1970Secs(*theCreateTime), thetestDateBuffer);
	printf("%s\n", thetestDateBuffer);
#endif

	float zeroFloat = 0;
	uint64_t zerouint64_t = 0;
	float fcpuUtilized = 0;

	theLen = sizeof(fcpuUtilized);
	(void)((QTSSDictionary*)sServer)->GetValue(qtssSvrCPULoadPercent, 0, &fcpuUtilized, &theLen);
	auto cpuUtilized = (uint32_t)fcpuUtilized;

	char lastUserName[eTempLogItemSize] = { 0 };
	StrPtrLen lastUserNameStr(lastUserName, eTempLogItemSize);

	char lastURLRealm[eTempLogItemSize] = { 0 };
	StrPtrLen lastURLRealmStr(lastURLRealm, eTempLogItemSize);

	//printf("logging of saved params are in dictionary \n");

	boost::string_view t = inClientSession->GetUserName();
	tempLogStr.Set((char *)t.data(), t.length());
	ReplaceSpaces(&tempLogStr, &lastUserNameStr, "%20");

	tempLogStr.Ptr[0] = 0; tempLogStr.Len = eTempLogItemSize;
	t = inClientSession->GetRealm();
	tempLogStr.Set((char *)t.data(), t.length());
	ReplaceSpaces(&tempLogStr, &lastURLRealmStr, "%20");

	char respMsgBuffer[1024] = { 0 };
	boost::string_view theRespMsg = inClientSession->GetRespMsg();
	StrPtrLen respMsgEncoded(respMsgBuffer, 1024 - 1);
	int32_t theErr = StringTranslator::EncodeURL(theRespMsg.data(), theRespMsg.length(), respMsgEncoded.Ptr, respMsgEncoded.Len);
	if (theErr <= 0)
		respMsgEncoded.Ptr[0] = '\0';
	else
	{
		respMsgEncoded.Len = theErr;
		respMsgEncoded.Ptr[respMsgEncoded.Len] = '\0';
	}

	//cs-uri-query
	std::string urlQry(rtpSession->GetQueryString());

	char tempLogBuffer[1024];
	char logBuffer[2048];
	// compatible fields (no respMsgEncoded field)
	::memset(logBuffer, 0, 2048);
	sprintf(tempLogBuffer, "%s ", (remoteAddr.empty() == '\0') ? sVoidField : remoteAddr.c_str()); //c-ip*
	::strcpy(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%s ", (theDateBuffer[0] == '\0') ? sVoidField : theDateBuffer);   //date* time*
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%s ", (remoteDNS.empty()) ? sVoidField : remoteDNS.c_str()); //c-dns
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%s ", (url.empty()) ? sVoidField : url.c_str());   //cs-uri-stem*
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%"   _U32BITARG_   " ", startPlayTimeInSecs);  //c-starttime 
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%"   _U32BITARG_   " ", (QTSS_MilliSecsTo1970Secs(curTime)
		- QTSS_MilliSecsTo1970Secs(theCreateTime)));   //x-duration* 
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%" _S32BITARG_ " ", (uint32_t)1);  //c-rate
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%" _S32BITARG_ " ", theStatusCode);   //c-status*
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%s ", (playerID.empty() == '\0') ? sVoidField : playerID.c_str());   //c-playerid*
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%s ", (playerVersionBuf[0] == '\0') ? sVoidField : playerVersionBuf); //c-playerversion
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%s ", (playerLangBuf[0] == '\0') ? sVoidField : playerLangBuf);   //c-playerlanguage*
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%s ", (userAgent.Ptr[0] == '\0') ? sVoidField : userAgent.Ptr);   //cs(User-Agent) 
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%s ", (playerOSBuf[0] == '\0') ? sVoidField : playerOSBuf);   //c-os*
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%s ", (playerOSVersBuf[0] == '\0') ? sVoidField : playerOSVersBuf);   //c-osversion
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%s ", (playerCPUBuf[0] == '\0') ? sVoidField : playerCPUBuf); //c-cpu*
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%0.0f ", movieDuration); //filelength in secs*
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%" _64BITARG_ "d ", movieSizeInBytes); //filesize in bytes*
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%"   _U32BITARG_   " ", movieAverageBitRatePtr);    //avgbandwidth in bits per second
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%s ", "RTP"); //protocol
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%s ", (audioPayloadName.empty()) ? sVoidField : audioPayloadName.data()); //audiocodec*
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%s ", (videoPayloadName.empty()) ? sVoidField : videoPayloadName.data()); //videocodec*
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%"   _U32BITARG_   " ", rtpBytesSent);    //sc-bytes*
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%"   _U32BITARG_   " ", rtcpBytesRecv);    //cs-bytes*
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%"   _U32BITARG_   " ", clientBytesRecv);  //c-bytes
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%"   _U32BITARG_   " ", rtpPacketsSent);   //s-pkts-sent*
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%"   _U32BITARG_   " ", clientPacketsReceived);    //c-pkts-recieved
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%"   _U32BITARG_   " ", clientPacketsLost);    //c-pkts-lost-client*
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%"   _U32BITARG_   " ", (uint32_t)1);  //c-buffercount 
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%"   _U32BITARG_   " ", clientBufferTime);     //c-totalbuffertime*
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%"   _U32BITARG_   " ", qualityLevel); //c-quality 
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%s ", (localIPAddr.empty()) ? sVoidField : localIPAddr.c_str());   //s-ip 
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%s ", (localDNS.empty()) ? sVoidField : localDNS.c_str()); //s-dns
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%"   _U32BITARG_   " ", numCurClients);    //s-totalclients
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%"   _U32BITARG_   " ", cpuUtilized);  //s-cpu-util
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%s ", (urlQry.empty()) ? sVoidField : urlQry.c_str()); //cs-uri-query
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%s ", (lastUserName[0] == '\0') ? sVoidField : lastUserName); //c-username
	::strcat(logBuffer, tempLogBuffer);
	sprintf(tempLogBuffer, "%s ", (lastURLRealm[0] == '\0') ? sVoidField : lastURLRealm); //sc(Realm)
	::strcat(logBuffer, tempLogBuffer);

	::strcat(logBuffer, "\n");

	Assert(::strlen(logBuffer) < 2048);

	//finally, write the log message
	sAccessLog->WriteToLog(logBuffer, kAllowLogToRoll);

	return QTSS_NoErr;
}


void CheckAccessLogState(bool forceEnabled)
{
	//this function makes sure the logging state is in synch with the preferences.
	//extern variable declared in QTSSPreferences.h
	//check error log.
	if ((nullptr == sAccessLog) && (forceEnabled || sLogEnabled))
	{
		sAccessLog = new QTSSAccessLog();
		sAccessLog->EnableLog();
	}

	if ((nullptr != sAccessLog) && ((!forceEnabled) && (!sLogEnabled)))
	{
		sAccessLog->Delete(); //sAccessLog is a task object, so don't delete it directly
		sAccessLog = nullptr;
	}
}

// SERVICE ROUTINES

QTSS_Error RollAccessLog(QTSS_ServiceFunctionArgsPtr /*inArgs*/)
{
	const bool kForceEnable = true;

	OSMutexLocker locker(sLogMutex);
	//calling CheckLogState is a kludge to allow logs
	//to be rolled while logging is disabled.

	CheckAccessLogState(kForceEnable);

	if (sAccessLog != nullptr)
		sAccessLog->RollLog();

	CheckAccessLogState(!kForceEnable);
	return QTSS_NoErr;
}

// This task runs once an hour to check and see if the log needs to roll.
void LogCheckTask::Run(const boost::system::error_code &ec)
{
	if (ec == boost::asio::error::operation_aborted) {
		printf("RTPStatsUpdaterTask timer canceled\n");
		return;
	}
	static bool firstTime = true;

	// don't check the log for rolling the first time we run.
	if (firstTime)
	{
		firstTime = false;
	}
	else
	{
		bool success = false;

		if (sAccessLog != nullptr && sAccessLog->IsLogEnabled())
			success = sAccessLog->CheckRollLog();
		Assert(success);
	}
	timer.expires_from_now(std::chrono::hours(1));
	timer.async_wait(std::bind(&LogCheckTask::Run, this, std::placeholders::_1));
}

time_t QTSSAccessLog::WriteLogHeader(FILE *inFile)
{
	time_t calendarTime = QTSSRollingLog::WriteLogHeader(inFile);

	//format a date for the startup time
	char theDateBuffer[QTSSRollingLog::kMaxDateBufferSizeInBytes] = { 0 };
	bool result = QTSSRollingLog::FormatDate(theDateBuffer, false);

	char tempBuffer[1024] = { 0 };
	if (result)
	{
		StrPtrLen serverName;
		QTSSDictionary *dict = (QTSSDictionary *)sServer;
		dict->GetValuePtr(qtssSvrServerName, 0, (void**)&serverName.Ptr, &serverName.Len);
		StrPtrLen serverVersion;
		dict->GetValuePtr(qtssSvrServerVersion, 0, (void**)&serverVersion.Ptr, &serverVersion.Len);
		sprintf(tempBuffer, sLogHeader, serverName.Ptr, serverVersion.Ptr, theDateBuffer, sLogTimeInGMT ? "GMT" : "local time");
		this->WriteToLog(tempBuffer, !kAllowLogToRoll);
	}

	return calendarTime;
}


void    WriteStartupMessage()
{
	if (sStartedUp)
		return;

	sStartedUp = true;

	//format a date for the startup time
	char theDateBuffer[QTSSRollingLog::kMaxDateBufferSizeInBytes];
	bool result = QTSSRollingLog::FormatDate(theDateBuffer, false);

	char tempBuffer[1024];
	if (result)
		sprintf(tempBuffer, "#Remark: Streaming beginning STARTUP %s\n", theDateBuffer);

	// log startup message to error log as well.
	if ((result) && (sAccessLog != nullptr))
		sAccessLog->WriteToLog(tempBuffer, kAllowLogToRoll);
}

void    WriteShutdownMessage()
{
	if (!sStartedUp)
		return;

	sStartedUp = false;

	//log shutdown message
	//format a date for the shutdown time
	char theDateBuffer[QTSSRollingLog::kMaxDateBufferSizeInBytes];
	bool result = QTSSRollingLog::FormatDate(theDateBuffer, false);

	char tempBuffer[1024];
	if (result)
		sprintf(tempBuffer, "#Remark: Streaming beginning SHUTDOWN %s\n", theDateBuffer);

	if (result && sAccessLog != nullptr)
		sAccessLog->WriteToLog(tempBuffer, kAllowLogToRoll);
}


/*

Log file format recognized by Lariat Stats

#Fields: c-ip date time c-dns cs-uri-stem c-starttime x-duration c-rate c-status c-playerid c-playerversion c-playerlanguage cs(User-Agent) cs(Referer) c-hostexe c-hostexever c-os c-osversion c-cpu filelength filesize avgbandwidth protocol transport audiocodec videocodec channelURL sc-bytes c-bytes s-pkts-sent c-pkts-received c-pkts-lost-client c-pkts-lost-net c-pkts-lost-cont-net c-resendreqs c-pkts-recovered-ECC c-pkts-recovered-resent c-buffercount c-totalbuffertime c-quality s-ip s-dns s-totalclients s-cpu-util
e.g. 157.56.87.123 1998-01-19 22:53:59 foo.bar.com rtsp://ddelval1/56k_5min_1.mov 1 16 1 200 - 5.1.51.119 0409 - - dshow.exe 5.1.51.119 Windows_NT 4.0.0.1381 Pentium 78 505349 11000 RTSP UDP MPEG_Layer-3 MPEG-4_Video_High_Speed_Compressor_(MT) - 188387 188387 281 281 0 0 0 0 0 0 1 5 100 157.56.87.123 foo.bar.com 1 0

Notes:
In the table below, W3C/Custom - refers to whether the fields are supported by the W3C file format or if it is a Custom  field
All fields are space-delimited
Fields not used by Lariat Stats should be present in the log file in some form, preferably a "-"
Log files should be named according to the format: Filename.YYMMDDNNN.log, where Filename is specific to the server type, YYMMDD is the date of creation, and NNN is a 3 digit number used when there are multiple logs from the same date (e.g. 000, 001, 002, etc.)

Field Name  Value   W3C/Custom  Example value   Used by Stats

c-ip    IP address of client    W3C 157.100.200.300 y
date    Date of the access  W3C 11-16-98    y
time    Time of the access (HH:MM:SS)   W3C 15:30:30    y
c-dns   Resolved dns of the client  W3C fredj.ford.com  n
cs-uri-stem     Requested file  W3C rtsp://server/sample.asf y
c-starttime     Start time  W3C 0   [in seconds, no fractions]  n
x-duration  Duration of the session (s) W3C 31   [in seconds, no fractions] y
c-rate  Rate file was played by client   Custom 1   [1= play, -5=rewind, +5=fforward]   n
c-status    http return code     Custom 200  [mapped to http/rtsp status codes; 200 is success, 404 file not found...]  y
c-playerid  unique player ID     Custom [a GUID value]  y
c-playerversion player version   Custom 3.0.0.1212
c-playerlanguage    player language  Custom EN   [two letter country code]  y
cs(User-Agent)  user agent  W3C Mozilla/2.0+(compatible;+MSIE+3.0;+Windows 95)  - this is a sample user-agent string    n
cs(Referer)     referring URL   W3C http://www.gte.com  n
c-hostexe   host program     Custom iexplore.exe   [iexplore.exe, netscape.exe, dshow.exe, nsplay.exe, vb.exe, etcä]    n
c-hostexever    version  Custom 4.70.1215   y
c-os    os   Custom Windows   [Windows, Windows NT, Unix-[flavor], Mac-[flavor]]    y
c-osversion os version   Custom 4.0.0.1212  n
c-cpu   cpu type     Custom Pentium   [486, Pentium, Alpha %d, Mac?, Unix?] y
filelength  file length (s)  Custom 60   [in seconds, no fractions] y
filesize    file size (bytes)    Custom 86000   [ie: 86kbytes]  y
avgbandwidth         Custom 24300   [ie: 24.3kbps]  n
protocol         Custom RTSP  [rtsp, http]   n
transport        Custom UDP   [udp, tcp, or mc] n
audiocodec       Custom MPEG-Layer-3    y
videocodec       Custom MPEG4   y
channelURL       Custom http://server/channel.nsc   n
sc-bytes    bytes sent by server    W3C 30000   [30k bytes sent from the server to the client]  y
cs-bytes    bytes received by client    W3C 28000   [bytes received]    n
s-pkts-sent packets sent     Custom 55  y
c-pkts-recieved     packets received     Custom 50  n
c-pkts-lost-client  packets lost     Custom 5   y
c-pkts-lost-net          Custom 2   [renamed from erasures; refers to packets lost at the network layer]  n
c-pkts-lost-cont-net         Custom 2   [continuous packets lost at the network layer]  n
c-resendreqs    packets resent   Custom 5   y
c-pkts-recovered-ECC    packets resent successfully  Custom 1   [this refers to packets recovered in the client layer]  y
c-pkts-recovered-resent      Custom 5   [this refers to packets recovered via udp resend]   n
c-buffercount        Custom 1   n
c-totalbuffertime   seconds buffered     Custom 20   [in seconds]   y
c-quality   quality measurement  Custom 89   [in percent]   n
s-ip    server ip   W3C 155.12.1.234   [entered by the unicast server]  n
s-dns   server dns  W3C foo.company.com n
s-totalclients  total connections at time of access  Custom 201   [total clients]   n
s-cpu-util  cpu utilization at time of access    Custom 40   [in percent]   n
cs-uri-query        W3C language=EN&rate=1&CPU=486&protocol=RTSP&transport=UDP&quality=89&avgbandwidth=24300 n

*/
