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

#include <chrono>
#include <boost/spirit/include/qi.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include "QTSServerInterface.h"
#include "QTSSReflectorModule.h"
#include "ReflectorSession.h"
#include "OSRef.h"
#include "OS.h"
#include "ResizeableStringFormatter.h"
#include "RTPSession.h"
#include "RTPSessionOutput.h"
#include "SDPSourceInfo.h"

#include "SDPUtils.h"
#include "sdpCache.h"
#include "RTSPRequest.h"

#include "QueryParamList.h"
#include "RTSPSession.h"
#include "uri/decode.h"
#include "ServerPrefs.h"

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

// STATIC DATA

// ref to the prefs dictionary object
static OSRefTable*      sSessionMap = nullptr;
static QTSServerInterface* sServer = nullptr;

//
// Prefs
static bool   sDefaultAllowNonSDPURLs = true;

static bool   sDefaultRTPInfoDisabled = false;

static bool   sDefaultAnnounceEnabled = true;
static bool   sDefaultAllowDuplicateBroadcasts = false;

static uint32_t   sMaxBroadcastAnnounceDuration = 0;
static uint32_t   sDefaultMaxBroadcastAnnounceDuration = 0;
static uint16_t   sMinimumStaticSDPPort = 0;
static uint16_t   sDefaultMinimumStaticSDPPort = 20000;
static uint16_t   sMaximumStaticSDPPort = 0;
static uint16_t   sDefaultMaximumStaticSDPPort = 65535;

static bool   sDefaultTearDownClientsOnDisconnect = false;

static uint32_t   sDefaultBroadcasterSessionTimeoutSecs = 30;

static uint16_t sLastMax = 0;
static uint16_t sLastMin = 0;

static bool   sDefaultEnforceStaticSDPPortRange = false;

//static uint32_t   sDefaultMaxAnnouncedSDPLengthInKbytes = 4;

static char*            sIPAllowList = nullptr;
static char*            sLocalLoopBackAddress = "127.0.0.*";

static bool   sAuthenticateLocalBroadcast = false;
static bool   sDefaultAuthenticateLocalBroadcast = false;

static bool	sDefaultDisableOverbuffering = false;
static bool	sFalse = false;

static bool   sDefaultReflectBroadcasts = true;

static bool   sAnnouncedKill = true;
static bool   sDefaultAnnouncedKill = true;


static bool   sPlayResponseRangeHeader = true;
static bool   sDefaultPlayResponseRangeHeader = true;

static bool   sPlayerCompatibility = true;
static bool   sDefaultPlayerCompatibility = true;

static uint32_t   sAdjustMediaBandwidthPercent = 100;
static uint32_t   sAdjustMediaBandwidthPercentDefault = 100;

static bool   sDefaultForceRTPInfoSeqAndTime = false;

static std::string	sRedirectBroadcastsKeyword;
static char*    sDefaultRedirectBroadcastsKeyword = "";
static char*    sDefaultBroadcastsDir = ""; // match all
static char*	sDefaultsBroadcasterGroup = "broadcaster";
static std::string sBroadcasterGroup;

static int32_t   sWaitTimeLoopCount = 10;

// Important strings
static boost::string_view    sTheNowRangeHeader("npt=now-");

// FUNCTION PROTOTYPES

static QTSS_Error Shutdown();
static QTSS_Error DoAnnounce(QTSS_StandardRTSP_Params& inParams);
static QTSS_Error DoDescribe(QTSS_StandardRTSP_Params& inParams);
ReflectorSession* FindOrCreateSession(boost::string_view inName, QTSS_StandardRTSP_Params &inParams, bool isPush = false, bool *foundSessionPtr = nullptr);
static QTSS_Error DoSetup(QTSS_StandardRTSP_Params &inParams);
static QTSS_Error DoPlay(QTSS_StandardRTSP_Params& inParams, ReflectorSession* inSession);
static QTSS_Error DestroySession(QTSS_ClientSessionClosing_Params* inParams);
static void RemoveOutput(ReflectorOutput* inOutput, ReflectorSession* inSession, bool killClients);
static ReflectorSession* DoSessionSetup(QTSS_StandardRTSP_Params &inParams, bool isPush, bool *foundSessionPtr = nullptr, std::string* resultFilePath = nullptr);

namespace ReflectionModule
{
	QTSS_Error Register()
	{
		return QTSS_NoErr;
	}

	QTSS_Error Initialize(QTSS_Initialize_Params* inParams)
	{
		// Setup module utils
		sSessionMap = getSingleton()->GetReflectorSessionMap();
		sServer = inParams->inServer;

		// Report to the server that this module handles DESCRIBE, SETUP, PLAY, PAUSE, and TEARDOWN
		static std::vector<QTSS_RTSPMethod> sSupportedMethods =
		{
			qtssDescribeMethod,
			qtssSetupMethod,
			qtssTeardownMethod,
			qtssPlayMethod,
			qtssPauseMethod,
			qtssAnnounceMethod,
			qtssRecordMethod
		};
		inParams->inServer->AppendSupportMehod(sSupportedMethods);

		return QTSS_NoErr;
	}

	QTSS_Error ProcessRTSPRequest(QTSS_StandardRTSP_Params& inParams)
	{
		OSMutexLocker locker(sSessionMap->GetMutex());
		QTSS_RTSPMethod theMethod = inParams.inRTSPRequest->GetMethod();

		if (theMethod == qtssAnnounceMethod)
			return DoAnnounce(inParams);
		if (theMethod == qtssDescribeMethod)
			return DoDescribe(inParams);
		if (theMethod == qtssSetupMethod)
			return DoSetup(inParams);

		auto opt = inParams.inClientSession->getAttribute(sOutputName);
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
			inParams.inClientSession->Teardown();
			inParams.inRTSPRequest->SendHeader();
			break;
		case qtssPauseMethod:
			inParams.inClientSession->Pause();
			inParams.inRTSPRequest->SendHeader();
			break;
		default:
			break;
		}
		return QTSS_NoErr;
	}

	QTSS_Error ProcessRTPData(QTSS_IncomingData_Params* inParams)
	{
		//printf("QTSSReflectorModule:ProcessRTPData inRTSPSession=%"   _U32BITARG_   " inClientSession=%"   _U32BITARG_   "\n",inParams->inRTSPSession, inParams->inClientSession);

		boost::optional<boost::any> attr = inParams->inRTSPSession->getAttribute(sBroadcasterSessionName);
		if (!attr) return QTSS_NoErr;
		ReflectorSession* theSession = boost::any_cast<ReflectorSession *>(attr.value());
		//printf("QTSSReflectorModule.cpp:ProcessRTPData    sClientBroadcastSessionAttr=%"   _U32BITARG_   " theSession=%"   _U32BITARG_   " err=%" _S32BITARG_ " \n",sClientBroadcastSessionAttr, theSession,theErr);

		// it is a broadcaster session
		//printf("QTSSReflectorModule.cpp:is broadcaster session\n");

		SDPSourceInfo& theSoureInfo = theSession->GetSourceInfo();

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

					StreamInfo* theStreamInfo = theStream->GetStreamInfo();
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

			SDPSourceInfo& theSoureInfo = theSession->GetSourceInfo();

			uint32_t  numStreams = theSession->GetNumStreams();
			StreamInfo* theStreamInfo = nullptr;

			for (uint32_t index = 0; index < numStreams; index++)
			{
				theStreamInfo = theSoureInfo.GetStreamInfo(index);
				if (theStreamInfo != nullptr)
					theStreamInfo->fSetupToReceive = false;
			}

			auto opt = inParams->inClientSession->getAttribute(sKillClientsEnabledName);
			bool killClients = boost::any_cast<bool>(opt.value()); // the pref as the default
																   //printf("QTSSReflectorModule.cpp:DestroySession broadcaster theSession=%"   _U32BITARG_   "\n", (uint32_t) theSession);
			theSession->RemoveSessionFromOutput();

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
}

static ReflectorSession* DoSessionSetup(QTSS_StandardRTSP_Params &inParams, bool isPush, bool *foundSessionPtr, std::string* resultFilePath)
{
	std::string theFullPath = inParams.inRTSPRequest->GetFileName();

	if (!isPush)
	{
		if (resultFilePath != nullptr)
			*resultFilePath = theFullPath;
		return FindOrCreateSession(theFullPath, inParams);
	}
	else
	{
		//
		// We aren't supposed to auto-append a .sdp, so just get the URL path out of the server
		//StrPtrLen theFullPath;
		//QTSS_Error theErr = QTSS_GetValuePtr(inParams->inRTSPRequest, qtssRTSPReqLocalPath, 0, (void**)&theFullPath.Ptr, &theFullPath.Len);
		//Assert(theErr == QTSS_NoErr);
		if (resultFilePath != nullptr)
			*resultFilePath = theFullPath;
		return FindOrCreateSession(theFullPath, inParams, isPush, foundSessionPtr);
	}
}

QTSS_Error DoAnnounce(QTSS_StandardRTSP_Params& inParams)
{
	std::string theFullPath = inParams.inRTSPRequest->GetFileName();

	std::string theStreamName = theFullPath;

	// Ok, this is an sdp file. Retreive the entire contents of the SDP.
	// This has to be done asynchronously (in case the SDP stuff is fragmented across
	// multiple packets. So, we have to have a simple state machine.

	//
	// We need to know the content length to manage memory
	uint32_t theLen = 0;
	uint32_t theContentLen = inParams.inRTSPRequest->GetContentLength();

	//
	// Check for the existence of 2 attributes in the request: a pointer to our buffer for
	// the request body, and the current offset in that buffer. If these attributes exist,
	// then we've already been here for this request. If they don't exist, add them.
	char theRequestBody[65536];

	//
	// We have our buffer and offset. Read the data.
	QTSS_Error theErr = inParams.inRTSPRequest->Read(theRequestBody, theContentLen, &theLen);
	Assert(theErr != QTSS_BadArgument);

	if (theErr == QTSS_RequestFailed)
	{
		//
		// NEED TO RETURN RTSP ERROR RESPONSE
		return inParams.inRTSPRequest->SendErrorResponse(qtssClientBadRequest);
	}

	if ((theErr == QTSS_WouldBlock) || (theLen < theContentLen))
	{
		theErr = inParams.inRTSPRequest->RequestEvent(QTSS_ReadableEvent);
		Assert(theErr == QTSS_NoErr);
		return QTSS_NoErr;
	}

	Assert(theErr == QTSS_NoErr);

	// ------------  Clean up missing required SDP lines

	std::string editedSDP = std::string(theRequestBody, theContentLen);

	// ------------ Check the headers

	SDPContainer checkedSDPContainer(editedSDP);
	if (!checkedSDPContainer.Parse())
	{
		return inParams.inRTSPRequest->SendErrorResponseWithMessage(qtssUnsupportedMediaType);
	}

	// ------------ reorder the sdp headers to make them proper.

	auto sortedSDP = SortSDPLine(checkedSDPContainer);

	// ------------ Write the SDP 

	// sortedSDP.GetSessionHeaders()->PrintStrEOL();
	// sortedSDP.GetMediaHeaders()->PrintStrEOL();

	CSdpCache::GetInstance()->setSdpMap(theStreamName, sortedSDP);


	//printf("QTSSReflectorModule:DoAnnounce SendResponse OK=200\n");

	inParams.inRTSPRequest->SendHeader();
	return QTSS_NoErr;
}

QTSS_Error DoDescribe(QTSS_StandardRTSP_Params& inParams)
{
	std::string theFileName;
	ReflectorSession* theSession = DoSessionSetup(inParams, false, nullptr, &theFileName);

	if (theSession == nullptr)
		return QTSS_RequestFailed;

	uint32_t theLen = 0;
	auto opt = inParams.inClientSession->getAttribute(sOutputName);

	// If there already  was an RTPSessionOutput attached to this Client Session,
	// destroy it. 
	if (opt)
	{
		RTPSessionOutput* theOutput = boost::any_cast<RTPSessionOutput*>(opt.value());
		RemoveOutput(theOutput, theOutput->GetReflectorSession(), false);
		inParams.inClientSession->removeAttribute(sOutputName);
	}
	// send the DESCRIBE response

	//above function has signalled that this request belongs to us, so let's respond
	iovec theDescribeVec[3] = { 0 };

	Assert(!theSession->GetLocalSDP().empty());

	boost::string_view theFileData = CSdpCache::GetInstance()->getSdpMap(theFileName);

	// -------------- process SDP to remove connection info and add track IDs, port info, and default c= line

	SDPSourceInfo tempSDPSourceInfo(theFileData); // will make a copy and delete in destructor
	std::string theSDPData = tempSDPSourceInfo.GetLocalSDP(); // returns a new buffer with processed sdp

	if (theSDPData.empty()) // can't find it on disk or it failed to parse just use the one in the session.
		theSDPData = std::string(theSession->GetLocalSDP());


	// ------------  Clean up missing required SDP lines

	std::string editedSDP(theSDPData);

	// ------------ Check the headers

	SDPContainer checkedSDPContainer(editedSDP);
	if (!checkedSDPContainer.Parse())
	{
		sSessionMap->Release(theSession->GetRef());

		return inParams.inRTSPRequest->SendErrorResponseWithMessage(qtssUnsupportedMediaType);
	}


	// ------------ Put SDP header lines in correct order
	auto sortedSDP = SortSDPLine(checkedSDPContainer);

	// ------------ Write the SDP 

	theDescribeVec[1].iov_base = const_cast<char *>(sortedSDP.data());
	theDescribeVec[1].iov_len = sortedSDP.length();

	inParams.inRTSPRequest->SendDescribeResponse(&theDescribeVec[0], 2, sortedSDP.length());

	sSessionMap->Release(theSession->GetRef());

#ifdef REFLECTORSESSION_DEBUG
	printf("QTSSReflectorModule.cpp:DoDescribe Session =%p refcount=%"   _U32BITARG_   "\n", theSession->GetRef(), theSession->GetRef()->GetRefCount());
#endif

	return QTSS_NoErr;
}

ReflectorSession* FindOrCreateSession(boost::string_view inName, QTSS_StandardRTSP_Params &inParams, bool isPush, bool *foundSessionPtr)
{
	OSMutexLocker locker(sSessionMap->GetMutex());

	std::string theStreamName(inName);

	StrPtrLen inPath((char *)theStreamName.c_str());

	OSRef* theSessionRef = sSessionMap->Resolve(&inPath);
	ReflectorSession* theSession = nullptr;

	if (theSessionRef == nullptr)
	{
		if (!isPush)
		{
			return nullptr;
		}

		boost::string_view theFileData = CSdpCache::GetInstance()->getSdpMap(theStreamName);

		if (theFileData.empty())
			return nullptr;

		SDPSourceInfo theInfo(theFileData); // will make a copy

		if (!theInfo.IsReflectable())
		{
			return nullptr;
		}

		//
		// Setup a ReflectorSession and bind the sockets. If we are negotiating,
		// make sure to let the session know that this is a Push Session so
		// ports may be modified.
		uint32_t theSetupFlag = ReflectorSession::kMarkSetup;
		if (isPush)
			theSetupFlag |= ReflectorSession::kIsPushSession;

		theSession = new ReflectorSession(inName, theInfo);

		// SetupReflectorSession stores theInfo in theSession so DONT delete the Info if we fail here, leave it alone.
		// deleting the session will delete the info.
		QTSS_Error theErr = theSession->SetupReflectorSession(inParams, theSetupFlag);
		if (theErr != QTSS_NoErr)
		{
			//delete theSession;
			CSdpCache::GetInstance()->eraseSdpMap(theSession->GetStreamName());
			theSession->StopTimer();
			return nullptr;
		}

		//printf("Created reflector session = %"   _U32BITARG_   " theInfo=%"   _U32BITARG_   " \n", (uint32_t) theSession,(uint32_t)theInfo);
		//put the session's ID into the session map.
		theErr = sSessionMap->Register(theSession->GetRef());
		Assert(theErr == QTSS_NoErr);
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
			if (foundSessionPtr)
				*foundSessionPtr = true;

			boost::string_view theFileData = CSdpCache::GetInstance()->getSdpMap(theStreamName);

			if (theFileData.empty())
				break;

			theSession = (ReflectorSession*)theSessionRef->GetObject();
			if (isPush && theSession && !(theSession->IsSetup()))
			{
				uint32_t theSetupFlag = ReflectorSession::kMarkSetup | ReflectorSession::kIsPushSession;
				QTSS_Error theErr = theSession->SetupReflectorSession(inParams, theSetupFlag);
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

	return theSession;
}

// ONLY call when performing a setup.
void DeleteReflectorPushSession(QTSS_StandardRTSP_Params& inParams, ReflectorSession* theSession, bool foundSession)
{
	if(theSession)
		sSessionMap->Release(theSession->GetRef());

	inParams.inClientSession->removeAttribute(sBroadcasterSessionName);

	if (foundSession)
		return; // we didn't allocate the session so don't delete

	OSRef* theSessionRef = theSession->GetRef();
	if (theSessionRef != nullptr)
	{
		theSession->TearDownAllOutputs(); // just to be sure because we are about to delete the session.
		sSessionMap->UnRegister(theSessionRef);// we had an error while setting up-- don't let anyone get the session
		//delete theSession;
		CSdpCache::GetInstance()->eraseSdpMap(theSession->GetStreamName());
		theSession->StopTimer();
	}
}

QTSS_Error AddRTPStream(QTSS_StandardRTSP_Params& inParams, RTPStream **newStreamPtr)
{
	// Ok, this is completely crazy but I can't think of a better way to do this that's
	// safe so we'll do it this way for now. Because the ReflectorStreams use this session's
	// stream queue, we need to make sure that each ReflectorStream is not reflecting to this
	// session while we call QTSS_AddRTPStream. One brutal way to do this is to grab each
	// ReflectorStream's mutex, which will stop every reflector stream from running.
	Assert(newStreamPtr != nullptr);

	//
	// Turn off reliable UDP transport, because we are not yet equipped to
	// do overbuffering.
	QTSS_Error theErr = inParams.inClientSession->AddStream(
		inParams.inRTSPRequest, newStreamPtr, qtssASFlagsForceUDPTransport);

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

QTSS_Error DoSetup(QTSS_StandardRTSP_Params &inParams)
{
	ReflectorSession* theSession = nullptr;

	bool isPush = inParams.inRTSPRequest->IsPushRequest();
	bool foundSession = false;

	auto opt = inParams.inClientSession->getAttribute(sOutputName);
	if (!opt)
	{
		if (!isPush)
		{
			theSession = DoSessionSetup(inParams, isPush);
			if (theSession == nullptr)
				return QTSS_RequestFailed;

			auto* theNewOutput = new RTPSessionOutput(inParams.inClientSession, theSession, sStreamCookieName);
			theSession->AddOutput(theNewOutput, true);
			inParams.inClientSession->addAttribute(sOutputName, theNewOutput);
		}
		else
		{
			auto opt = inParams.inClientSession->getAttribute(sBroadcasterSessionName);
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

			inParams.inClientSession->addAttribute(sBroadcasterSessionName, theSession);
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
	std::string theDigitStr = inParams.inRTSPRequest->GetFileDigit();
	if (theDigitStr.empty())
	{
		if (isPush)
			DeleteReflectorPushSession(inParams, theSession, foundSession);
		return inParams.inRTSPRequest->SendErrorResponse(qtssClientBadRequest);
	}

	uint32_t theTrackID = std::stoi(theDigitStr);

	if (isPush)
	{
		//printf("QTSSReflectorModule.cpp:DoSetup is push setup\n");

		// Get info about this trackID
		const StreamInfo* theStreamInfo = theSession->GetSourceInfo().GetStreamInfoByTrackID(theTrackID);
		// If theStreamInfo is NULL, we don't have a legit track, so return an error
		if (theStreamInfo == nullptr)
		{
			DeleteReflectorPushSession(inParams, theSession, foundSession);
			return inParams.inRTSPRequest->SendErrorResponse(qtssClientBadRequest);
		}

		if (theStreamInfo->fSetupToReceive)
		{
			DeleteReflectorPushSession(inParams, theSession, foundSession);
			return inParams.inRTSPRequest->SendErrorResponse(qtssPreconditionFailed);
		}

		inParams.inRTSPRequest->SetUpServerPort(theStreamInfo->fPort);

		RTPStream *newStream = nullptr;
		QTSS_Error theErr = AddRTPStream(inParams, &newStream);
		Assert(theErr == QTSS_NoErr);
		if (theErr != QTSS_NoErr)
		{
			DeleteReflectorPushSession(inParams, theSession, foundSession);
			return inParams.inRTSPRequest->SendErrorResponse(qtssClientBadRequest);
		}

		//send the setup response

		SendSetupRTSPResponse(newStream, inParams.inRTSPRequest, 0);

		// This is an incoming data session. Set the Reflector Session in the ClientSession
		inParams.inClientSession->addAttribute(sBroadcasterSessionName, theSession);

		if (theSession != nullptr)
			theSession->AddBroadcasterClientSession(inParams.inClientSession);

#ifdef REFLECTORSESSION_DEBUG
		printf("QTSSReflectorModule.cpp:DoSetup Session =%p refcount=%"   _U32BITARG_   "\n", theSession->GetRef(), theSession->GetRef()->GetRefCount());
#endif

		return QTSS_NoErr;
	}


	// Get info about this trackID
	const StreamInfo* theStreamInfo = theSession->GetSourceInfo().GetStreamInfoByTrackID(theTrackID);
	// If theStreamInfo is NULL, we don't have a legit track, so return an error
	if (theStreamInfo == nullptr)
		return inParams.inRTSPRequest->SendErrorResponse(qtssClientBadRequest);

	boost::string_view thePayloadName = theStreamInfo->fPayloadName;
#if 0
	bool r = qi::phrase_parse(thePayloadName.cbegin(), thePayloadName.cend(),
		qi::omit[+(qi::char_ - "/")] >> "/" >> qi::uint_, qi::ascii::blank, theStreamInfo->fTimeScale);
#endif

	RTPStream *newStream = nullptr;
	{
		// Ok, this is completely crazy but I can't think of a better way to do this that's
		// safe so we'll do it this way for now. Because the ReflectorStreams use this session's
		// stream queue, we need to make sure that each ReflectorStream is not reflecting to this
		// session while we call QTSS_AddRTPStream. One brutal way to do this is to grab each
		// ReflectorStream's mutex, which will stop every reflector stream from running.

		QTSS_Error theErr = inParams.inClientSession->AddStream(
			inParams.inRTSPRequest, &newStream, 0);

		if (theErr != QTSS_NoErr)
			return theErr;
	}

	// Set up items for this stream
	newStream->SetPayloadName(thePayloadName);
	newStream->SetPayLoadType(theStreamInfo->fPayloadType);
	newStream->SetSDPStreamID(theTrackID);
	newStream->SetTimeScale(theStreamInfo->fTimeScale);

	// We only want to allow over buffering to dynamic rate clients   
	inParams.inClientSession->SetOverBufferEnable(sFalse);

	// Place the stream cookie in this stream for future reference
	void* theStreamCookie = theSession->GetStreamCookie(theTrackID);
	Assert(theStreamCookie != nullptr);
	newStream->addAttribute(sStreamCookieName, theStreamCookie);

	// Set the number of quality levels.
	newStream->SetNumQualityLevels(ReflectorSession::kNumQualityLevels);

	//send the setup response
	SendSetupRTSPResponse(newStream, inParams.inRTSPRequest, qtssSetupRespDontWriteSSRC);

#ifdef REFLECTORSESSION_DEBUG
	printf("QTSSReflectorModule.cpp:DoSetup Session =%p refcount=%"   _U32BITARG_   "\n", theSession->GetRef(), theSession->GetRef()->GetRefCount());
#endif

	return QTSS_NoErr;
}

QTSS_Error DoPlay(QTSS_StandardRTSP_Params& inParams, ReflectorSession* inSession)
{
	QTSS_Error theErr = QTSS_NoErr;
	uint32_t flags = 0;

	if (inSession == nullptr)
	{
		auto opt = inParams.inClientSession->getAttribute(sBroadcasterSessionName);
		if (!opt) return QTSS_RequestFailed;
		inSession = boost::any_cast<ReflectorSession*>(opt.value());

		Assert(inSession != nullptr);

		inParams.inRTSPSession->addAttribute(sBroadcasterSessionName, inSession);
		//printf("QTSSReflectorModule.cpp:DoPlay (PUSH) inRTSPSession=%"   _U32BITARG_   " inClientSession=%"   _U32BITARG_   "\n",(uint32_t)inParams->inRTSPSession,(uint32_t)inParams->inClientSession);
	}
	else
	{
		auto opt = inParams.inClientSession->getAttribute(sOutputName);
		if (!opt)
			return QTSS_RequestFailed;

		RTPSessionOutput*  theOutput = boost::any_cast<RTPSessionOutput*>(opt.value());
		theOutput->InitializeStreams();

		// Tell the session what the bitrate of this reflection is. This is nice for logging,
		// it also allows the server to scale the TCP buffer size appropriately if we are
		// interleaving the data over TCP. This must be set before calling QTSS_Play so the
		// server can use it from within QTSS_Play
		uint32_t bitsPerSecond = inSession->GetBitRate();
		inParams.inClientSession->SetMovieAvgBitrate(bitsPerSecond);

		if (sPlayResponseRangeHeader)
		{
			auto opt = inParams.inClientSession->getAttribute(sRTPInfoWaitTime);
			if (!opt)
				inParams.inRTSPRequest->AppendHeader(qtssRangeHeader, sTheNowRangeHeader);
		}

		theErr = inParams.inClientSession->Play(
			inParams.inRTSPRequest, qtssPlayFlagsAppendServerInfo);
		if (theErr != QTSS_NoErr)
			return theErr;
	}

	inParams.inClientSession->SendPlayResponse(inParams.inRTSPRequest, flags);

#ifdef REFLECTORSESSION_DEBUG
	printf("QTSSReflectorModule.cpp:DoPlay Session =%p refcount=%"   _U32BITARG_   "\n", inSession->GetRef(), inSession->GetRef()->GetRefCount());
#endif

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
			SDPSourceInfo& theInfo = theSession->GetSourceInfo();

			//if (theInfo->IsRTSPControlled())
			//{   
			//    FileDeleter(theSession->GetSourceID());
			//}
			//    

			if (killClients)
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
				CSdpCache::GetInstance()->eraseSdpMap(theSession->GetStreamName());

				theSession->StopTimer();
			}
		}
	}
	delete inOutput;
}