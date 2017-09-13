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
	 File:       RTSPSession.cpp

	 Contains:   Implemenation of RTSPSession objects
 */
#define __RTSP_HTTP_DEBUG__ 0
#define __RTSP_HTTP_VERBOSE__ 0
#define __RTSP_AUTH_DEBUG__ 0
#define debug_printf if (__RTSP_AUTH_DEBUG__) printf

#include <memory>
#include <random>
#include <fmt/format.h>
#include <boost/algorithm/string/predicate.hpp>

#include "RTSPSession.h"
#include "RTSPRequest.h"
#include "QTSServerInterface.h"

#include "MyAssert.h"

#include "QTSS.h"
#include "QTSSDataConverter.h"
#include "QTSSReflectorModule.h"
#include "ServerPrefs.h"
#include "RTSPServer.h"
#include "sdpCache.h"

#include <errno.h>

static RTPSession* GetRTPSession(StrPtrLen *str)
{
	OSRefTable* theMap = getSingleton()->GetRTPSessionMap();
	OSRef* theRef = theMap->Resolve(str);
	if (theRef != nullptr)
		return (RTPSession*)theRef->GetObject();
	return nullptr;
}

// hack stuff
static boost::string_view                    sBroadcasterSessionName = "QTSSReflectorModuleBroadcasterSession";

static StrPtrLen    sVideoStr("video");
static StrPtrLen    sAudioStr("audio");
static StrPtrLen    sRtpMapStr("rtpmap");
static StrPtrLen    sControlStr("control");
static StrPtrLen    sBufferDelayStr("x-bufferdelay");
static boost::string_view    sContentType("application/x-random-data");

static boost::string_view    sEmptyStr("");

RTSPSession::RTSPSession()
	: RTSPSessionInterface(),
	fReadMutex()
{
	this->SetTaskName("RTSPSession");

	// Setup the QTSS param block, as none of these fields will change through the course of this session.
	rtspParams.inRTSPSession = this;
	rtspParams.inRTSPRequest = nullptr;
	rtspParams.inClientSession = nullptr;

	fLastRTPSessionID[0] = 0;
	fLastRTPSessionIDPtr.Set(fLastRTPSessionID, 0);
	Assert(fLastRTPSessionIDPtr.Ptr == &fLastRTPSessionID[0]);

}

RTSPSession::~RTSPSession()
{
	fLiveSession = false; //used in Clean up request to remove the RTP session.
	this->CleanupRequest();// Make sure that all our objects are deleted

	if (fRequest)
	{
		delete fRequest;
		fRequest = nullptr;
	}
}

int64_t RTSPSession::Run()
{
	EventFlags events = this->GetEvents();
	QTSS_Error err = QTSS_NoErr;
	Assert(fLastRTPSessionIDPtr.Ptr == &fLastRTPSessionID[0]);

	//check for a timeout or a kill. If so, just consider the session dead
	if ((events & Task::kTimeoutEvent) || (events & Task::kKillEvent))
		fLiveSession = false;

	while (this->IsLiveSession())
	{
		// RTSP Session state machine. There are several well defined points in an RTSP request
		// where this session may have to return from its run function and wait for a new event.
		// Because of this, we need to track our current state and return to it.

		switch (fState)
		{
		case kReadingFirstRequest:
			{
				if ((err = fInputStream.ReadRequest()) == QTSS_NoErr)
				{
					// If the RequestStream returns QTSS_NoErr, it means
					// that we've read all outstanding data off the socket,
					// and still don't have a full request. Wait for more data.

					//+rt use the socket that reads the data, may be different now.
					fSocket.RequestEvent(EV_RE);
					return 0;
				}

				if ((err != QTSS_RequestArrived) && (err != E2BIG))
				{
					// Any other error implies that the client has gone away. At this point,
					// we can't have 2 sockets, so we don't need to do the "half closed" check
					// we do below
					Assert(err > 0);
					Assert(!this->IsLiveSession());
					break;
				}

				if (err == QTSS_RequestArrived)
					fState = kHaveNonTunnelMessage;
				// If we get an E2BIG, it means our buffer was overfilled.
				// In that case, we can just jump into the following state, and
				// the code their does a check for this error and returns an error.
				if (err == E2BIG)
					fState = kHaveNonTunnelMessage;
			}
			continue;

		case kReadingRequest:
			{
				// We should lock down the session while reading in data,
				// because we can't snarf up a POST while reading.
				OSMutexLocker readMutexLocker(&fReadMutex);

				// we should be only reading an RTSP request here, no HTTP tunnel messages

				if ((err = fInputStream.ReadRequest()) == QTSS_NoErr)
				{
					// If the RequestStream returns QTSS_NoErr, it means
					// that we've read all outstanding data off the socket,
					// and still don't have a full request. Wait for more data.

					//+rt use the socket that reads the data, may be different now.
					fSocket.RequestEvent(EV_RE);
					return 0;
				}

				if ((err != QTSS_RequestArrived) && (err != E2BIG) && (err != QTSS_BadArgument))
				{
					//Any other error implies that the input connection has gone away.
					// We should only kill the whole session if we aren't doing HTTP.
					// (If we are doing HTTP, the POST connection can go away)
					Assert(err > 0);
					if (fSocket.IsConnected())
					{
						fSocket.Cleanup();
						return 0;
					}
					else
					{
						Assert(!this->IsLiveSession());
						break;
					}
				}
				fState = kHaveNonTunnelMessage;
				// fall thru to kHaveNonTunnelMessage
			}

		case kHaveNonTunnelMessage:// 说明请求报文格式是正确的，请求已进入受理状态
			{
				// should only get here when fInputStream has a full message built.

				Assert(fInputStream.GetRequestBuffer());

				if (fRequest == nullptr)
				{
					//                    printf("RTSPRequest size########## %d\n",sizeof(RTSPRequest));
					fRequest = new RTSPRequest(this);
				}
				fRequest->ReInit(this);

				rtspParams.inRTSPRequest = fRequest;

				// We have an RTSP request and are about to begin processing. We need to
				// make sure that anyone sending interleaved data on this session won't
				// be allowed to do so until we are done sending our response
				// We also make sure that a POST session can't snarf in while we're
				// processing the request.
				fReadMutex.Lock();
				fSessionMutex.Lock();

				// The fOutputStream's fBytesWritten counter is used to
				// count the # of bytes for this RTSP response. So, at
				// this point, reset it to 0 (we can then just let it increment
				// until the next request comes in)
				fOutputStream.ResetBytesWritten();

				// Check for an overfilled buffer, and return an error.
				if (err == E2BIG)
				{
					(void)fRequest->SendErrorResponse(qtssClientBadRequest);
					fState = kPostProcessingRequest;
					break;
				}
				// Check for a corrupt base64 error, return an error
				if (err == QTSS_BadArgument)
				{
					(void)fRequest->SendErrorResponse(qtssClientBadRequest);
					fState = kPostProcessingRequest;
					break;
				}

				Assert(err == QTSS_RequestArrived);
				fState = kFilteringRequest;

				// Note that there is no break here. We'd like to continue onto the next
				// state at this point. This goes for every case in this case statement
			}

		case kFilteringRequest:
			{
				// We received something so auto refresh
				// The need to auto refresh is because the api doesn't allow a module to refresh at this point
				fTimeoutTask.RefreshTimeout();

				//
				// Before we even do this, check to see if this is a *data* packet,
				// in which case this isn't an RTSP request, so we don't need to go
				// through any of the remaining steps

				if (fInputStream.IsDataPacket())
				{
					this->HandleIncomingDataPacket();

					fState = kCleaningUp;
					break;
				}

				if (fRequest->HasResponseBeenSent())
				{
					fState = kPostProcessingRequest;
					break;
				}

				this->SetupRequest();

				// This might happen if there is some syntax or other error,
				// or if it is an OPTIONS request
				if (fRequest->HasResponseBeenSent())
				{
					fState = kPostProcessingRequest;
					break;
				}
				fState = kPreprocessingRequest;
			}

		case kPreprocessingRequest:
			{
				// Invoke preprocessor modules
				{
					// Manipulation of the RTPSession from the point of view of
					// a module is guarenteed to be atomic by the API.
					Assert(fRTPSession != nullptr);
					OSMutexLocker   locker(fRTPSession->GetSessionMutex());
					ReflectionModule::ProcessRTSPRequest(rtspParams);
				}
				if (fRequest->HasResponseBeenSent())
				{
					fState = kPostProcessingRequest;
					break;
				}
				fState = kProcessingRequest;
			}

		case kProcessingRequest:
			{
				// If no preprocessor sends a response, move onto the request processing module. It
				// is ALWAYS supposed to send a response, but if it doesn't, we have a canned error
				// to send back.
				if (!fRequest->HasResponseBeenSent())
				{
					// no modules took this one so send back a parameter error
					if (fRequest->GetMethod() == qtssSetParameterMethod) // keep session
					{
						fRequest->SetStatus(qtssSuccessOK);
						fRequest->SendHeader();
					}
					else
					{
						fRequest->SendErrorResponse(qtssServerInternal);
					}
				}

				fState = kPostProcessingRequest;
			}

		case kPostProcessingRequest:
			{
				// Post process the request *before* sending the response. Therefore, we
				// will post process regardless of whether the client actually gets our response
				// or not.

				//if this is not a keepalive request, we should kill the session NOW
				fLiveSession = fRequest->GetResponseKeepAlive();

				if (fRTPSession != nullptr)
				{
					// Invoke postprocessor modules only if there is an RTP session. We do NOT want
					// postprocessors running when filters or syntax errors have occurred in the request!
					{
						// Manipulation of the RTPSession from the point of view of
						// a module is guarenteed to be atomic by the API.
						OSMutexLocker   locker(fRTPSession->GetSessionMutex());

						// Set the current RTSP session for this RTP session.
						// We do this here because we need to make sure the SessionMutex
						// is grabbed while we do this. Only do this if the RTSP session
						// is still alive, of course.
						if (this->IsLiveSession())
							fRTPSession->UpdateRTSPSession(this);

					}
				}
				fState = kSendingResponse;
			}

		case kSendingResponse:
			{
				// Sending the RTSP response consists of making sure the
				// RTSP request output buffer is completely flushed to the socket.
				Assert(fRequest != nullptr);

				err = fOutputStream.Flush();

				if (err == EAGAIN)
				{
					// If we get this error, we are currently flow-controlled and should
					// wait for the socket to become writeable again
					fSocket.RequestEvent(EV_WR);
					this->ForceSameThread();    // We are holding mutexes, so we need to force
												// the same thread to be used for next Run()
					return 0;
				}
				else if (err != QTSS_NoErr)
				{
					// Any other error means that the client has disconnected, right?
					Assert(!this->IsLiveSession());
					break;
				}

				fState = kCleaningUp;
			}

		case kCleaningUp:
			{
				// Cleaning up consists of making sure we've read all the incoming Request Body
				// data off of the socket
				if (this->GetRemainingReqBodyLen() > 0)
				{
					err = this->DumpRequestData();

					if (err == EAGAIN)
					{
						fSocket.RequestEvent(EV_RE);
						this->ForceSameThread();    // We are holding mutexes, so we need to force
													// the same thread to be used for next Run()
						return 0;
					}
				}

				// If we've gotten here, we've flushed all the data. Cleanup,
				// and wait for our next request!
				this->CleanupRequest();
				fState = kReadingRequest;
			}
		}
	}

    //printf("RTSPSession fObjectHolders:%d !\n", fObjectHolders.load());

	//fObjectHolders--  
	if (!IsLiveSession() && fObjectHolders > 0) {
		OSRefTable* theMap = getSingleton()->GetRTPSessionMap();
		OSRef* theRef = theMap->Resolve(&fLastRTPSessionIDPtr);
		if (theRef != nullptr) {
			fRTPSession = (RTPSession*)theRef->GetObject();
			if (fRTPSession) fRTPSession->Teardown();
			while (theRef->GetRefCount() > 0)
				theMap->Release(fRTPSession->GetRef());
			fRTPSession = nullptr;
		}
	}

	// Make absolutely sure there are no resources being occupied by the session
	// at this point.
	this->CleanupRequest();

	// Only delete if it is ok to delete!
	if (fObjectHolders == 0)
    {
        //printf("RTSPSesion Run Return -1\n");
		return -1;
    }

	// If we are here because of a timeout, but we can't delete because someone
	// is holding onto a reference to this session, just reschedule the timeout.
	//
	// At this point, however, the session is DEAD.
	return 0;
}

void RTSPSession::SetupRequest()
{
	// First parse the request
	QTSS_Error theErr = fRequest->Parse();
	if (theErr != QTSS_NoErr)
		return;

	// let's also refresh RTP session timeout so that it's kept alive in sync with the RTSP session.
	 //
	 // Attempt to find the RTP session for this request.
	fRTPSession = FindRTPSession();

	if (fRTPSession != nullptr)
	{
		fRTPSession->RefreshTimeout();
		uint32_t headerBits = fRequest->GetBandwidthHeaderBits();
		if (headerBits != 0)
			fRTPSession->SetLastRTSPBandwithBits(headerBits);
	}
	QTSS_RTSPStatusCode statusCode = qtssSuccessOK;
	char *body = nullptr;
	uint32_t bodySizeBytes = 0;

	// If this is an OPTIONS request, don't even bother letting modules see it. Just
	// send a standard OPTIONS response, and be done.
	if (fRequest->GetMethod() == qtssOptionsMethod)// OPTIONS请求
	{
		boost::string_view cSeq = fRequest->GetHeaderDict().Get(qtssCSeqHeader);
		if (cSeq.empty())
		{
			fRequest->SetStatus(qtssClientBadRequest);
			fRequest->SendHeader();
			return;
		}

		fRequest->AppendHeader(qtssPublicHeader, QTSServerInterface::GetPublicHeader());

		// DJM PROTOTYPE
		boost::string_view require = fRequest->GetHeaderDict().Get(qtssRequireHeader);

		fRequest->SendHeader();

		// now write the body if there is one
		if (bodySizeBytes > 0 && body != nullptr)
			fRequest->Write(body, bodySizeBytes, nullptr, 0);

		return;
	}

	// If this is a SET_PARAMETER request, don't let modules see it.
	if (fRequest->GetMethod() == qtssSetParameterMethod)
	{


		// Check that it has the CSeq header
		boost::string_view cSeq = fRequest->GetHeaderDict().Get(qtssCSeqHeader);
		if (cSeq.empty()) // keep session
		{
			fRequest->SetStatus(qtssClientBadRequest);
			fRequest->SendHeader();
			return;
		}


		// If the RTPSession doesn't exist, return error
		if (fRTPSession == nullptr) // keep session
		{
			fRequest->SetStatus(qtssClientSessionNotFound);
			fRequest->SendHeader();
			return;
		}

		// refresh RTP session timeout so that it's kept alive in sync with the RTSP session.
		if (fRequest->GetLateToleranceInSec() != -1)
		{
			fRTPSession->SetStreamThinningParams(fRequest->GetLateToleranceInSec());
			fRequest->SendHeader();
			return;
		}
		// let modules handle it if they want it.

	}

	// If this is a DESCRIBE request, make sure there is no SessionID. This is not allowed,
	// and may screw up modules if we let them see this request.
	if (fRequest->GetMethod() == qtssDescribeMethod)
	{
		if (!fRequest->GetHeaderDict().Get(qtssSessionHeader).empty())
		{
			(void)fRequest->SendErrorResponse(qtssClientHeaderFieldNotValid);
			return;
		}
	}

	// If we don't have an RTP session yet, create one...
	if (fRTPSession == nullptr)
	{
		theErr = CreateNewRTPSession();
		if (theErr != QTSS_NoErr)
			return;
	}

	uint32_t headerBits = fRequest->GetBandwidthHeaderBits();
	if (headerBits != 0)
		fRTPSession->SetLastRTSPBandwithBits(headerBits);

	// If it's a play request and the late tolerance is sent in the request use this value
	if ((fRequest->GetMethod() == qtssPlayMethod) && (fRequest->GetLateToleranceInSec() != -1))
		fRTPSession->SetStreamThinningParams(fRequest->GetLateToleranceInSec());

	// Check to see if this is a "ping" PLAY request (a PLAY request while already
	// playing with no Range header). If so, just send back a 200 OK response and do nothing.
	// No need to go to modules to do this, because this is an RFC documented behavior  
	if ((fRequest->GetMethod() == qtssPlayMethod) && (fRTPSession->GetSessionState() == qtssPlayingState)
		&& (fRequest->GetStartTime() == -1) && (fRequest->GetStopTime() == -1))
	{
		fRequest->SendHeader();
		fRTPSession->RefreshTimeout();
		return;
	}

	Assert(fRTPSession != nullptr); // At this point, we must have one!
	rtspParams.inClientSession = fRTPSession;

}

void RTSPSession::CleanupRequest()
{
	if (fRTPSession != nullptr)
	{
		// Release the ref.
		OSRefTable* theMap = getSingleton()->GetRTPSessionMap();
		theMap->Release(fRTPSession->GetRef());

		// nullptr out any references to this RTP session
		fRTPSession = nullptr;
		rtspParams.inClientSession = nullptr;
	}

	if (this->IsLiveSession() == false) //clear out the ID so it can't be re-used.
	{
		fLastRTPSessionID[0] = 0;
		fLastRTPSessionIDPtr.Set(fLastRTPSessionID, 0);
	}

	if (fRequest != nullptr)
	{
		// nullptr out any references to the current request
		//delete fRequest;
		//fRequest = nullptr;
		rtspParams.inRTSPRequest = nullptr;
	}

	fSessionMutex.Unlock();
	fReadMutex.Unlock();

	// Clear out our last value for request body length before moving onto the next request
	this->SetRequestBodyLength(-1);
}

RTPSession*  RTSPSession::FindRTPSession()
{
	// This function attempts to locate the appropriate RTP session for this RTSP
	// Request. It uses an RTSP session ID as a key to finding the correct RTP session,
	// and it looks for this session ID in two places. First, the RTSP session ID header
	// in the RTSP request, and if there isn't one there, in the RTSP session object itself.

	boost::string_view theSessionID = fRequest->GetHeaderDict().Get(qtssSessionHeader);
	if (!theSessionID.empty())
	{
		StrPtrLen theSessionIDV((char *)theSessionID.data(), theSessionID.length());
		return GetRTPSession(&theSessionIDV);
	}

	// If there wasn't a session ID in the headers, look for one in the RTSP session itself
	if (fLastRTPSessionIDPtr.Len > 0)
		return GetRTPSession(&fLastRTPSessionIDPtr);

	return nullptr;
}

QTSS_Error  RTSPSession::CreateNewRTPSession()
{
	Assert(fLastRTPSessionIDPtr.Ptr == &fLastRTPSessionID[0]);

	// This is a brand spanking new session. At this point, we need to create
	// a new RTPSession object that will represent this session until it completes.
	// Then, we need to pass the session onto one of the modules

	// First of all, ask the server if it's ok to add a new session
	QTSS_Error theErr = this->IsOkToAddNewRTPSession();
	if (theErr != QTSS_NoErr)
		return theErr;

	// Create the RTPSession object
	Assert(fRTPSession == nullptr);
	fRTPSession = new RTPSession();

	{
		//
		// Lock the RTP session down so that it won't delete itself in the
		// unusual event there is a timeout while we are doing this.
		OSMutexLocker locker(fRTPSession->GetSessionMutex());

		// So, generate a session ID for this session
		QTSS_Error activationError = EPERM;
		while (activationError == EPERM)
		{
			fLastRTPSessionIDPtr.Len = this->GenerateNewSessionID(fLastRTPSessionID);

			//ok, some module has bound this session, we can activate it.
			//At this point, we may find out that this new session ID is a duplicate.
			//If that's the case, we'll simply retry until we get a unique ID
			activationError = fRTPSession->Activate(fLastRTPSessionID);
		}
		Assert(activationError == QTSS_NoErr);
	}
	Assert(fLastRTPSessionIDPtr.Ptr == &fLastRTPSessionID[0]);

	// Activate adds this session into the RTP session map. We need to therefore
	// make sure to resolve the RTPSession object out of the map, even though
	// we don't actually need to pointer.
	OSRef* theRef = getSingleton()->GetRTPSessionMap()->Resolve(&fLastRTPSessionIDPtr);
	Assert(theRef != nullptr);

	return QTSS_NoErr;
}

uint32_t RTSPSession::GenerateNewSessionID(char* ioBuffer)
{
	std::random_device rd;
	std::mt19937 mt(rd());
	std::uniform_int_distribution<int64_t> dist;

	int64_t theSessionID = dist(mt);
	sprintf(ioBuffer, "%" _64BITARG_ "d", theSessionID);
	Assert(::strlen(ioBuffer) < QTSS_MAX_SESSION_ID_LENGTH);
	return ::strlen(ioBuffer);
}

bool RTSPSession::OverMaxConnections(uint32_t buffer)
{
	return false;
}

QTSS_Error RTSPSession::IsOkToAddNewRTPSession()
{
	QTSServerInterface* theServer = getSingleton();
	QTSS_ServerState theServerState = theServer->GetServerState();

	//we may want to deny this connection for a couple of different reasons
	//if the server is refusing new connections
	if ((theServerState == qtssRefusingConnectionsState) ||
		(theServerState == qtssIdleState) ||
		(theServerState == qtssFatalErrorState) ||
		(theServerState == qtssShuttingDownState))
		return fRequest->SendErrorResponse(qtssServerUnavailable);

	//if the max connection limit has been hit 
	if (this->OverMaxConnections(0))
		return fRequest->SendErrorResponse(qtssClientNotEnoughBandwidth);

	//if the max bandwidth limit has been hit
	int32_t maxKBits = ServerPrefs::GetMaxKBitsBandwidth();
	if ((maxKBits > -1) && (theServer->GetCurBandwidthInBits() >= ((uint32_t)maxKBits * 1024)))
		return fRequest->SendErrorResponse(qtssClientNotEnoughBandwidth);

	//if the server is too loaded down (CPU too high, whatever)
	// --INSERT WORKING CODE HERE--

	return QTSS_NoErr;
}

QTSS_Error RTSPSession::DumpRequestData()
{
	char theDumpBuffer[2048];

	QTSS_Error theErr = QTSS_NoErr;
	while (theErr == QTSS_NoErr)
		theErr = this->Read(theDumpBuffer, 2048, nullptr);

	return theErr;
}

void RTSPSession::HandleIncomingDataPacket()
{

	// Attempt to find the RTP session for this request.
	auto   packetChannel = (uint8_t)fInputStream.GetRequestBuffer()->Ptr[1];
	boost::string_view theSessionID = GetSessionIDForChannelNum(packetChannel);

	if (theSessionID.empty())
		return;

	StrPtrLen theSessionIDV((char *)theSessionID.data(), theSessionID.length());
	fRTPSession = GetRTPSession(&theSessionIDV);

	if (fRTPSession == nullptr)
		return;

	StrPtrLen packetWithoutHeaders(fInputStream.GetRequestBuffer()->Ptr + 4, fInputStream.GetRequestBuffer()->Len - 4);

	fRTPSession->RefreshTimeout();
	RTPStream* theStream = fRTPSession->FindRTPStreamForChannelNum(packetChannel);
	theStream->ProcessIncomingInterleavedData(packetChannel, this, &packetWithoutHeaders);

	//
	// We currently don't support async notifications from within this role
	QTSS_IncomingData_Params rtspIncomingDataParams;
	rtspIncomingDataParams.inRTSPSession = this;

	rtspIncomingDataParams.inClientSession = fRTPSession;
	rtspIncomingDataParams.inPacketData = fInputStream.GetRequestBuffer()->Ptr;
	rtspIncomingDataParams.inPacketLen = fInputStream.GetRequestBuffer()->Len;

	ReflectionModule::ProcessRTPData(&rtspIncomingDataParams);
}

RTSPSession1::RTSPSession1(RTSPServer &server, std::shared_ptr<Connection> connection) noexcept 
: mServer(server), connection(std::move(connection)) 
{
	try {
		//auto remote_endpoint = connection->socket.lowest_layer().remote_endpoint();
		//request = std::make_shared<RTSPRequest1>(remote_endpoint.address().to_string(), remote_endpoint.port());
		request = std::make_shared<RTSPRequest1>();
	}
	catch (...) {
		request = std::make_shared<RTSPRequest1>();
	}
}

std::unique_ptr<ReflectorSession> RTSPSession1::CreateSession(boost::string_view sessionName)
{
	std::lock_guard<std::mutex> lock(mServer.session_mutex);
	auto it = mServer.sessionMap.find(std::string(sessionName));
	if (it == mServer.sessionMap.end()) {
		boost::string_view theFileData = CSdpCache::GetInstance()->getSdpMap(sessionName);

		if (theFileData.empty())
			return nullptr;

		auto* theInfo = new SDPSourceInfo(theFileData); // will make a copy

		if (!theInfo->IsReflectable())
		{
			delete theInfo;
			return nullptr;
		}

		//
		// Setup a ReflectorSession and bind the sockets. If we are negotiating,
		// make sure to let the session know that this is a Push Session so
		// ports may be modified.
		uint32_t theSetupFlag = ReflectorSession::kMarkSetup | ReflectorSession::kIsPushSession;

		ReflectorSession1 *theSession = new ReflectorSession1(sessionName);

		QTSS_Error theErr = QTSS_NoErr;// theSession->SetupReflectorSession(theInfo, inParams, theSetupFlag, sOneSSRCPerStream, sTimeoutSSRCSecs);
		if (theErr != QTSS_NoErr)
		{
			//delete theSession;
			//CSdpCache::GetInstance()->eraseSdpMap(theSession->GetStreamName());
			//theSession->StopTimer();
			return nullptr;
		}
		mServer.sessionMap.insert(std::make_pair(sessionName, theSession));
	}
	return {};
}

void RTSPSession1::do_setup()
{
	bool isPush = request->IsPushRequest();
	if (!rtp_OutputSession)
	{
		if (!isPush)
		{
#if 0
			theSession = DoSessionSetup(inParams, isPush);
			if (theSession == nullptr)
				return QTSS_RequestFailed;
			auto* theNewOutput = new RTPSessionOutput(inParams->inClientSession, theSession, sStreamCookieName);
			theSession->AddOutput(theNewOutput, true);
			inParams->inClientSession->addAttribute(sOutputName, theNewOutput);
#endif
		}
		else
		{
			if (!broadcastSession)
			{
				broadcastSession = CreateSession("live.sdp");
				//if (theSession == nullptr)
//					return QTSS_RequestFailed;
			}
		}
	}
	else
	{
		//auto theSession = outputSession.value().GetReflectorSession();
	}
}