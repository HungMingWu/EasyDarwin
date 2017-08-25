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
#include <fmt/format.h>
#include <boost/algorithm/string/predicate.hpp>

#include "RTSPSession.h"
#include "RTSPRequest.h"
#include "QTSServerInterface.h"

#include "MyAssert.h"

#include "QTSS.h"
#include "QTSSModuleUtils.h"
#include "md5digest.h"
#include "QTSSDataConverter.h"
#include "QTSSReflectorModule.h"

#if __FreeBSD__ || __hpux__	
#include <unistd.h>
#endif

#include <errno.h>

#if __solaris__ || __linux__ || __sgi__	|| __hpux__
#include <crypt.h>
#endif

static RTPSession* GetRTPSession(StrPtrLen *str)
{
	OSRefTable* theMap = QTSServerInterface::GetServer()->GetRTPSessionMap();
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

static StrPtrLen    sAuthAlgorithm("md5");
static boost::string_view    sAuthQop("auth");
static StrPtrLen    sEmptyStr("");

RTSPSession::RTSPSession()
	: RTSPSessionInterface(),
	fReadMutex()
{
	this->SetTaskName("RTSPSession");

	QTSServerInterface::GetServer()->AlterCurrentRTSPSessionCount(1);

	// Setup the QTSS param block, as none of these fields will change through the course of this session.
	rtspParams.inRTSPSession = this;
	rtspParams.inRTSPRequest = nullptr;
	rtspParams.inClientSession = nullptr;

	fModuleState.curModule = nullptr;
	fModuleState.curTask = this;
	fModuleState.curRole = 0;

	fLastRTPSessionID[0] = 0;
	fLastRTPSessionIDPtr.Set(fLastRTPSessionID, 0);
	Assert(fLastRTPSessionIDPtr.Ptr == &fLastRTPSessionID[0]);

}

RTSPSession::~RTSPSession()
{
	// Invoke the session closing modules
	QTSS_RoleParams theParams;
	theParams.rtspSessionClosingParams.inRTSPSession = this;

	// Invoke modules
	for (const auto &theModule : QTSServerInterface::GetModule(QTSSModule::kRTSPSessionClosingRole))
		theModule->CallDispatch(QTSS_RTSPSessionClosing_Role, &theParams);

	fLiveSession = false; //used in Clean up request to remove the RTP session.
	this->CleanupRequest();// Make sure that all our objects are deleted
	if (fSessionType == qtssRTSPSession)
		QTSServerInterface::GetServer()->AlterCurrentRTSPSessionCount(-1);
	else
		QTSServerInterface::GetServer()->AlterCurrentRTSPHTTPSessionCount(-1);

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

	// Some callbacks look for this struct in the thread object
	OSThreadDataSetter theSetter(&fModuleState, nullptr);

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
					fInputSocketP->RequestEvent(EV_RE);
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
					fInputSocketP->RequestEvent(EV_RE);
					return 0;
				}

				if ((err != QTSS_RequestArrived) && (err != E2BIG) && (err != QTSS_BadArgument))
				{
					//Any other error implies that the input connection has gone away.
					// We should only kill the whole session if we aren't doing HTTP.
					// (If we are doing HTTP, the POST connection can go away)
					Assert(err > 0);
					if (fOutputSocketP->IsConnected())
					{
						// If we've gotten here, this must be an HTTP session with
						// a dead input connection. If that's the case, we should
						// clean up immediately so as to not have an open socket
						// needlessly lingering around, taking up space.
						Assert(fOutputSocketP != fInputSocketP);
						Assert(!fInputSocketP->IsConnected());
						fInputSocketP->Cleanup();
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
					(void)QTSSModuleUtils::SendErrorResponse(fRequest, qtssClientBadRequest,
						qtssMsgRequestTooLong);
					fState = kPostProcessingRequest;
					break;
				}
				// Check for a corrupt base64 error, return an error
				if (err == QTSS_BadArgument)
				{
					(void)QTSSModuleUtils::SendErrorResponse(fRequest, qtssClientBadRequest,
						qtssMsgBadBase64);
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

				//
				// In case a module wants to replace the request
				char* theReplacedRequest = nullptr;
				char* oldReplacedRequest = nullptr;

				// Setup the filter param block
				QTSS_RoleParams theFilterParams;
				theFilterParams.rtspFilterParams.inRTSPSession = this;
				theFilterParams.rtspFilterParams.inRTSPRequest = fRequest;
				theFilterParams.rtspFilterParams.outNewRequest = &theReplacedRequest;

				// Invoke filter modules
				for (const auto &theModule : QTSServerInterface::GetModule(QTSSModule::kRTSPFilterRole))
				{
					fModuleState.idleTime = 0;

					theModule->CallDispatch(QTSS_RTSPFilter_Role, &theFilterParams);
	


					//
					// Check to see if this module has replaced the request. If so, check
					// to see if there is an old replacement that we should delete
					if (theReplacedRequest != nullptr)
					{
						if (oldReplacedRequest != nullptr)
							delete[] oldReplacedRequest;

						fRequest->SetFullRequest({ theReplacedRequest, ::strlen(theReplacedRequest) });
						oldReplacedRequest = theReplacedRequest;
						theReplacedRequest = nullptr;
					}
				}

				fCurrentModule = 0;
				if (fRequest->HasResponseBeenSent())
				{
					fState = kPostProcessingRequest;
					break;
				}

				if (fSentOptionsRequest && this->ParseOptionsResponse())
				{
					fRoundTripTime = (int32_t)(OS::Milliseconds() - fOptionsRequestSendTime);
					//printf("RTSPSession::Run RTT time = %" _S32BITARG_ " msec\n", fRoundTripTime);
					fState = kSendingResponse;
					break;
				}
				else
					// Otherwise, this is a normal request, so parse it and get the RTPSession.
					this->SetupRequest();

				// This might happen if there is some syntax or other error,
				// or if it is an OPTIONS request
				if (fRequest->HasResponseBeenSent())
				{
					fState = kPostProcessingRequest;
					break;
				}
				fState = kRoutingRequest;
			}
		case kRoutingRequest:
			{
				// Invoke router modules
				{
					// Manipulation of the RTPSession from the point of view of
					// a module is guaranteed to be atomic by the API.
					Assert(fRTPSession != nullptr);
					OSMutexLocker   locker(fRTPSession->GetSessionMutex());

					ReflectionModule::RedirectBroadcast(&rtspParams);
				}
				fCurrentModule = 0;

				// SetupAuthLocalPath must happen after kRoutingRequest and before kAuthenticatingRequest
				// placed here so that if the state is shifted to kPostProcessingRequest from a response being sent
				// then the AuthLocalPath will still be set.
				fRequest->SetupAuthLocalPath();

				if (fRequest->HasResponseBeenSent())
				{
					fState = kPostProcessingRequest;
					break;
				}

				if (fRequest->SkipAuthorization())
				{
					// Skip the authentication and authorization states

					// The foll. normally gets executed at the end of the authorization state 
					// Prepare for kPreprocessingRequest state.
					fState = kPreprocessingRequest;

					if (fRequest->GetMethod() == qtssSetupMethod)
						// Make sure to erase the session ID stored in the request at this point.
						// If we fail to do so, this same session would be used if another
						// SETUP was issued on this same TCP connection.
						fLastRTPSessionIDPtr.Len = 0;
					else if (fLastRTPSessionIDPtr.Len == 0)
						fLastRTPSessionIDPtr.Len = ::strlen(fLastRTPSessionIDPtr.Ptr);

					break;
				}
				else
					fState = kAuthenticatingRequest;
			}

		case kAuthenticatingRequest:
			{
				bool      allowedDefault = QTSServerInterface::GetServer()->GetPrefs()->GetAllowGuestDefault();
				bool      allowed = allowedDefault; //server pref?
				bool      hasUser = false;
				bool      handled = false;
				bool      wasHandled = false;

				StrPtrLenDel prefRealm(QTSServerInterface::GetServer()->GetPrefs()->GetAuthorizationRealm());
				if (prefRealm.Ptr != nullptr)
				{
					fRequest->SetURLRealm({ prefRealm.Ptr, prefRealm.Len });
				}


				QTSS_RTSPMethod method = fRequest->GetMethod();
				if (method != qtssIllegalMethod) do
				{   //Set the request action before calling the authentication module

					if ((method == qtssAnnounceMethod) || ((method == qtssSetupMethod) && fRequest->IsPushRequest()))
					{
						fRequest->SetAction(qtssActionFlagsWrite);
						break;
					}

					if (fRTPSession->getAttribute(sBroadcasterSessionName))
					{
						fRequest->SetAction(qtssActionFlagsWrite); // an incoming broadcast session
						break;
					}

					fRequest->SetAction(qtssActionFlagsRead);
				} while (false);
				else
				{
					Assert(0);
				}

				if (fRequest->GetAuthScheme() == qtssAuthNone)
				{
					QTSS_AuthScheme scheme = QTSServerInterface::GetServer()->GetPrefs()->GetAuthScheme();
					if (scheme == qtssAuthBasic)
						fRequest->SetAuthScheme(qtssAuthBasic);
					else if (scheme == qtssAuthDigest)
						fRequest->SetAuthScheme(qtssAuthDigest);

					if (scheme == qtssAuthDigest)
						debug_printf("RTSPSession.cpp:kAuthenticatingRequest  scheme == qtssAuthDigest\n");
				}

				// Setup the authentication param block
				QTSS_RoleParams theAuthenticationParams;
				theAuthenticationParams.rtspAthnParams.inRTSPRequest = fRequest;

				fModuleState.idleTime = 0;

				fRequest->SetAllowed(allowed);
				fRequest->SetHasUser(hasUser);
				fRequest->SetAuthHandled(handled);
				fRequest->SetDigestChallenge(lastDigestChallenge);


				for (const auto &theModule : QTSServerInterface::GetModule(QTSSModule::kRTSPAthnRole))
				{
					fRequest->SetAllowed(allowedDefault);
					fRequest->SetHasUser(false);
					fRequest->SetAuthHandled(false);

					if (nullptr == theModule)
						continue;

					(void)theModule->CallDispatch(QTSS_RTSPAuthenticate_Role, &theAuthenticationParams);

					allowed = fRequest->GetAllowed();
					hasUser = fRequest->GetHasUser();
					handled = fRequest->GetAuthHandled();
					debug_printf("RTSPSession::Run Role(kAuthenticatingRequest) allowedDefault =%d allowed= %d hasUser = %d handled=%d \n", allowedDefault, allowed, hasUser, handled);
					if (handled)
						wasHandled = handled;

					if (hasUser || handled)
					{
						break;
					}

				}

				if (!wasHandled) //don't check and possibly fail the user if it the user has already been checked.
					this->CheckAuthentication();

				fCurrentModule = 0;
				if (fRequest->HasResponseBeenSent())
				{
					fState = kPostProcessingRequest;
					break;
				}
				fState = kAuthorizingRequest;
			}
		case kAuthorizingRequest:
			{
				// Invoke authorization modules
				bool      allowedDefault = QTSServerInterface::GetServer()->GetPrefs()->GetAllowGuestDefault();
				bool      allowed = true;
				bool      hasUser = false;
				bool      handled = false;
				QTSS_Error  theErr = QTSS_NoErr;

				// Invoke authorization modules

				// Manipulation of the RTPSession from the point of view of
				// a module is guaranteed to be atomic by the API.
				Assert(fRTPSession != nullptr);
				OSMutexLocker   locker(fRTPSession->GetSessionMutex());

				fRequest->SetAllowed(allowed);
				fRequest->SetHasUser(hasUser);
				fRequest->SetAuthHandled(handled);


				fRequest->SetHasUser(false);
				fRequest->SetAuthHandled(false);

				fModuleState.idleTime = 0;

				(void)ReflectionModule::ReflectorAuthorizeRTSPRequest(&rtspParams);


				// allowed != default means a module has set the result
				// handled means a module wants to be the primary for this request
				// -- example qtaccess says only allow valid user and allowed default is false.  So module says handled, hasUser is false, allowed is false
				// 
				allowed = fRequest->GetAllowed();
				hasUser = fRequest->GetHasUser();
				handled = fRequest->GetAuthHandled();
				debug_printf("RTSPSession::Run Role(kAuthorizingRequest) allowedDefault =%d allowed= %d hasUser = %d handled=%d \n", allowedDefault, allowed, hasUser, handled);

				this->SaveRequestAuthorizationParams(fRequest);

				if (!allowed)
				{
					if (false == fRequest->HasResponseBeenSent())
					{
						QTSS_AuthScheme challengeScheme = fRequest->GetAuthScheme();

						if (challengeScheme == qtssAuthDigest)
						{
							debug_printf("RTSPSession.cpp:kAuthorizingRequest  scheme == qtssAuthDigest)\n");
						}
						else if (challengeScheme == qtssAuthBasic)
						{
							debug_printf("RTSPSession.cpp:kAuthorizingRequest  scheme == qtssAuthBasic)\n");
						}

						if (challengeScheme == qtssAuthBasic) {
							fRTPSession->SetAuthScheme(qtssAuthBasic);
							theErr = fRequest->SendBasicChallenge();
						}
						else if (challengeScheme == qtssAuthDigest) {
							fRTPSession->UpdateDigestAuthChallengeParams(false, false, RTSPSessionInterface::kNoQop);
							theErr = fRequest->SendDigestChallenge(fRTPSession->GetAuthQop(), fRTPSession->GetAuthNonce(), fRTPSession->GetAuthOpaque());
						}
						else {
							// No authentication scheme is given and the request was not allowed,
							// so send a 403: Forbidden message
							theErr = fRequest->SendForbiddenResponse();
						}
						if (QTSS_NoErr != theErr) // We had an error so bail on the request quit the session and post process the request.
						{
							fRequest->SetResponseKeepAlive(false);
							fCurrentModule = 0;
							fState = kPostProcessingRequest;
							break;

						}
					}
				}

				fCurrentModule = 0;
				if (fRequest->HasResponseBeenSent())
				{
					fState = kPostProcessingRequest;
					break;
				}

				// Prepare for kPreprocessingRequest state.
				fState = kPreprocessingRequest;

				if (fRequest->GetMethod() == qtssSetupMethod)
					// Make sure to erase the session ID stored in the request at this point.
					// If we fail to do so, this same session would be used if another
					// SETUP was issued on this same TCP connection.
					fLastRTPSessionIDPtr.Len = 0;
				else if (fLastRTPSessionIDPtr.Len == 0)
					fLastRTPSessionIDPtr.Len = ::strlen(fLastRTPSessionIDPtr.Ptr);
			}

		case kPreprocessingRequest:
			{
				// Invoke preprocessor modules
				{
					// Manipulation of the RTPSession from the point of view of
					// a module is guarenteed to be atomic by the API.
					Assert(fRTPSession != nullptr);
					OSMutexLocker   locker(fRTPSession->GetSessionMutex());
					ReflectionModule::ProcessRTSPRequest(&rtspParams);
				}
				fCurrentModule = 0;
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
				fModuleState.idleTime = 0;
				auto modules = QTSServerInterface::GetModule(QTSSModule::kRTSPRequestRole);
				if (!modules.empty())
				{
					// Manipulation of the RTPSession from the point of view of
					// a module is guarenteed to be atomic by the API.
					Assert(fRTPSession != nullptr);
					OSMutexLocker   locker(fRTPSession->GetSessionMutex());
				}

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
						QTSSModuleUtils::SendErrorResponse(fRequest, qtssServerInternal, qtssMsgNoModuleForRequest);
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

						// Make sure the RTPSession contains a copy of the realStatusCode in this request
						uint32_t realStatusCode = RTSPProtocol::GetStatusCode(fRequest->GetStatus());
						fRTPSession->SetStatusCode(realStatusCode);

						fRTPSession->SetRespMsg(fRequest->GetRespMsg());

						// Set the current RTSP session for this RTP session.
						// We do this here because we need to make sure the SessionMutex
						// is grabbed while we do this. Only do this if the RTSP session
						// is still alive, of course.
						if (this->IsLiveSession())
							fRTPSession->UpdateRTSPSession(this);

					}
				}
				fCurrentModule = 0;
				fState = kSendingResponse;
			}

		case kSendingResponse:
			{
				// Sending the RTSP response consists of making sure the
				// RTSP request output buffer is completely flushed to the socket.
				Assert(fRequest != nullptr);

				// If x-dynamic-rate header is sent with a value of 1, send OPTIONS request
				if ((fRequest->GetMethod() == qtssSetupMethod) && (fRequest->GetStatus() == qtssSuccessOK)
					&& (fRequest->GetDynamicRateState() == 1) && fRoundTripTimeCalculation)
				{
					this->SaveOutputStream();
					this->ResetOutputStream();
					this->SendOptionsRequest();
				}

				if (fSentOptionsRequest && (fRequest->GetMethod() == qtssIllegalMethod))
				{
					this->ResetOutputStream();
					this->RevertOutputStream();
					fSentOptionsRequest = false;
				}

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
						fInputSocketP->RequestEvent(EV_RE);
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
		OSRefTable* theMap = QTSServerInterface::GetServer()->GetRTPSessionMap();
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

void RTSPSession::CheckAuthentication() {

	QTSSUserProfile* profile = fRequest->GetUserProfile();
	StrPtrLen* userPassword = profile->GetValue(qtssUserPassword);
	boost::string_view userPaddwordV(userPassword->Ptr, userPassword->Len);
	QTSS_AuthScheme scheme = fRequest->GetAuthScheme();
	bool authenticated = true;

	// Check if authorization information returned by the client is for the scheme that the server sent the challenge
	if (scheme != (fRTPSession->GetAuthScheme())) {
		authenticated = false;
	}
	else if (scheme == qtssAuthBasic) {
		// For basic authentication, the authentication module returns the crypt of the password, 
		std::string reqPasswdStr(fRequest->GetPassWord());
		char* userPasswdStr = userPassword->GetAsCString(); // memory allocated

		if (userPassword->Len == 0)
		{
			authenticated = false;
		}
		else
		{
#ifdef __Win32__
			// The password is md5 encoded for win32
			char md5EncodeResult[120];
			// no memory is allocated in this function call
			MD5Encode((char *)reqPasswdStr.c_str(), userPasswdStr, md5EncodeResult, sizeof(md5EncodeResult));
			if (::strcmp(userPasswdStr, md5EncodeResult) != 0)
				authenticated = false;
#else
			if (::strcmp(userPasswdStr, (char*)crypt(reqPasswdStr, userPasswdStr)) != 0)
				authenticated = false;
#endif
		}

		delete[] userPasswdStr;    // deleting allocated memory
		userPasswdStr = nullptr;
	}
	else if (scheme == qtssAuthDigest) {
		// For digest authentication, md5 digest comparison
		// The text returned by the authentication module in qtssUserPassword is MD5 hash of (username:realm:password)

		uint32_t qop = fRequest->GetAuthQop();
		boost::string_view opaque = fRequest->GetAuthOpaque();
		boost::string_view sessionOpaque = fRTPSession->GetAuthOpaque();
		uint32_t sessionQop = fRTPSession->GetAuthQop();

		do {
			// The Opaque string should be the same as that sent by the server
			// The QoP should be the same as that sent by the server
			if (!boost::iequals(sessionOpaque, opaque)) {
				authenticated = false;
				break;
			}

			if (sessionQop != qop) {
				authenticated = false;
				break;
			}

			// All these are just pointers to existing memory... no new memory is allocated
			//StrPtrLen* userName = profile->GetValue(qtssUserName);
			//StrPtrLen* realm = fRequest->GetAuthRealm();
			boost::string_view nonce = fRequest->GetAuthNonce();
			boost::string_view method = RTSPProtocol::GetMethodString(fRequest->GetMethod());
			boost::string_view digestUri = fRequest->GetAuthUri();
			boost::string_view responseDigest = fRequest->GetAuthResponse();
			//StrPtrLen hA1;
			std::string requestDigest;
			boost::string_view emptyStr;

			boost::string_view cNonce = fRequest->GetAuthCNonce();
			// Since qtssUserPassword = md5(username:realm:password)
			// Just convert the 16 bit hash to a 32 bit char array to get HA1
			//HashToString((unsigned char *)userPassword->Ptr, &hA1);
			//CalcHA1(&sAuthAlgorithm, userName, realm, userPassword, nonce, cNonce, &hA1);


			// For qop="auth"
			if (qop == RTSPSessionInterface::kAuthQop) {
				boost::string_view nonceCount = fRequest->GetAuthNonceCount();
				uint32_t ncValue = 0;

				// Convert nounce count (which is a string of 8 hex digits) into a uint32_t
				if (!nonceCount.empty())
				{
					// Convert nounce count (which is a string of 8 hex digits) into a uint32_t                 
					uint32_t bufSize = sizeof(ncValue);
					std::string tempString(nonceCount);
					//tempString.ToUpper();
					QTSSDataConverter::ConvertCHexStringToBytes((char *)tempString.c_str(),
						&ncValue,
						&bufSize);
					ncValue = ntohl(ncValue);

				}
				// nonce count must not be repeated by the client
				if (ncValue < (fRTPSession->GetAuthNonceCount())) {
					authenticated = false;
					break;
				}

				// allocates memory for requestDigest.Ptr
				requestDigest = CalcRequestDigest(userPaddwordV, nonce, nonceCount, cNonce, sAuthQop, method, digestUri, emptyStr);
				// If they are equal, check if nonce used by client is same as the one sent by the server

			}   // For No qop
			else if (qop == RTSPSessionInterface::kNoQop)
			{
				// allocates memory for requestDigest->Ptr
				requestDigest = CalcRequestDigest(userPaddwordV, nonce, emptyStr, emptyStr, emptyStr, method, digestUri, emptyStr);
			}

			// hA1 is allocated memory 
			//delete [] hA1.Ptr;

			if (boost::equals(responseDigest, requestDigest)) {
				if (!boost::equals(nonce, fRTPSession->GetAuthNonce()))
					fRequest->SetStale(true);
				authenticated = true;
			}
			else {
				authenticated = false;
			}

		} while (false);
	}

	if (!fRequest->GetAuthHandled())
	{
		if ((!authenticated) || (authenticated && (fRequest->GetStale()))) {
			debug_printf("erasing username from profile\n");
			(void)profile->SetValue(qtssUserName, 0, sEmptyStr.Ptr, sEmptyStr.Len, QTSSDictionary::kDontObeyReadOnly);
			(void)profile->SetValue(qtssUserPassword, 0, sEmptyStr.Ptr, sEmptyStr.Len, QTSSDictionary::kDontObeyReadOnly);
			(void)profile->clearUserGroups();
		}
	}
}

bool RTSPSession::ParseOptionsResponse()
{
	boost::string_view t1(fRequest->GetFullRequest());
	StrPtrLen t1V((char *)t1.data(), t1.length());
	StringParser parser(&t1V);
	static StrPtrLen sRTSPStr("RTSP", 4);
	StrPtrLen theProtocol;
	parser.ConsumeLength(&theProtocol, 4);

	return (theProtocol.Equal(sRTSPStr));
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
		if (boost::iequals(require, RTSPProtocol::GetHeaderString(qtssXRandomDataSizeHeader)))
		{
			body = (char*)RTSPSessionInterface::sOptionsRequestBody;
			bodySizeBytes = fRequest->GetRandomDataSize();
			Assert(bodySizeBytes <= sizeof(RTSPSessionInterface::sOptionsRequestBody));
			fRequest->AppendHeader(qtssContentTypeHeader, sContentType);
			fRequest->AppendContentLength(bodySizeBytes);
		}

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
			(void)QTSSModuleUtils::SendErrorResponse(fRequest, qtssClientHeaderFieldNotValid, qtssMsgNoSesIDOnDescribe);
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

	// Setup Authorization params;
	fRequest->ParseAuthHeader();
}

void RTSPSession::CleanupRequest()
{
	if (fRTPSession != nullptr)
	{
		// Release the ref.
		OSRefTable* theMap = QTSServerInterface::GetServer()->GetRTPSessionMap();
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

		// Because this is a new RTP session, setup some dictionary attributes
		// pertaining to RTSP that only need to be set once
		this->SetupClientSessionAttrs();

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
	OSRef* theRef = QTSServerInterface::GetServer()->GetRTPSessionMap()->Resolve(&fLastRTPSessionIDPtr);
	Assert(theRef != nullptr);

	return QTSS_NoErr;
}

void RTSPSession::SetupClientSessionAttrs()
{
	// get and pass presentation url
	fRTPSession->SetPresentationURL(fRequest->GetURI());

	// get and pass full request url
	fRTPSession->SetAbsoluteURL(fRequest->GetAbsoluteURL());

	// get and pass request host name
	fRTPSession->SetHost(fRequest->GetHeaderDict().Get(qtssHostHeader));

	// get and pass user agent header
	fRTPSession->SetUserAgent(fRequest->GetHeaderDict().Get(qtssUserAgentHeader));

	// get and pass CGI params
	if (fRequest->GetMethod() == qtssDescribeMethod)
		fRTPSession->SetQueryString(fRequest->GetQueryString());

	// store RTSP session info in the RTPSession.   
	fRTPSession->SetRemoteAddr(GetRemoteAddr());
	fRTPSession->SetLocalDNS(GetLocalDNS());
	fRTPSession->SetLocalAddr(GetLocalAddr());
}

uint32_t RTSPSession::GenerateNewSessionID(char* ioBuffer)
{
	//RANDOM NUMBER GENERATOR

	//We want to make our session IDs as random as possible, so use a bunch of
	//current server statistics to generate a random int64_t.

	//Generate the random number in two uint32_t parts. The first uint32_t uses
	//statistics out of a random RTP session.
	int64_t theMicroseconds = OS::Microseconds();
	::srand((unsigned int)theMicroseconds);
	uint32_t theFirstRandom = ::rand();

	QTSServerInterface* theServer = QTSServerInterface::GetServer();

	{
		OSMutexLocker locker(theServer->GetRTPSessionMap()->GetMutex());
		OSRefHashTable* theHashTable = theServer->GetRTPSessionMap()->GetHashTable();
		if (theHashTable->GetNumEntries() > 0)
		{
			theFirstRandom %= theHashTable->GetNumEntries();
			theFirstRandom >>= 2;

			OSRefHashTableIter theIter(theHashTable);
			//Iterate through the session map, finding a random session
			for (uint32_t theCount = 0; theCount < theFirstRandom; theIter.Next(), theCount++)
				Assert(!theIter.IsDone());

			auto* theSession = (RTPSession*)theIter.GetCurrent()->GetObject();
			theFirstRandom += theSession->GetPacketsSent();
			theFirstRandom += (uint32_t)theSession->GetSessionCreateTime();
			theFirstRandom += (uint32_t)theSession->GetPlayTime();
			theFirstRandom += (uint32_t)theSession->GetBytesSent();
		}
	}
	//Generate the first half of the random number
	::srand((unsigned int)theFirstRandom);
	theFirstRandom = ::rand();

	//Now generate the second half
	uint32_t theSecondRandom = ::rand();
	theSecondRandom += theServer->GetCurBandwidthInBits();
	theSecondRandom += theServer->GetAvgBandwidthInBits();
	theSecondRandom += theServer->GetRTPPacketsPerSec();
	theSecondRandom += (uint32_t)theServer->GetTotalRTPBytes();
	theSecondRandom += theServer->GetTotalRTPSessions();

	::srand((unsigned int)theSecondRandom);
	theSecondRandom = ::rand();

	auto theSessionID = (int64_t)theFirstRandom;
	theSessionID <<= 32;
	theSessionID += (int64_t)theSecondRandom;
	sprintf(ioBuffer, "%" _64BITARG_ "d", theSessionID);
	Assert(::strlen(ioBuffer) < QTSS_MAX_SESSION_ID_LENGTH);
	return ::strlen(ioBuffer);
}

bool RTSPSession::OverMaxConnections(uint32_t buffer)
{
	QTSServerInterface* theServer = QTSServerInterface::GetServer();
	int32_t maxConns = theServer->GetPrefs()->GetMaxConnections();
	bool overLimit = false;

	if (maxConns > -1) // limit connections
	{
		uint32_t maxConnections = (uint32_t)maxConns + buffer;
		if ((theServer->GetNumRTPSessions() > maxConnections)
			||
			(theServer->GetNumRTSPSessions() + theServer->GetNumRTSPHTTPSessions() > maxConnections)
			)
		{
			overLimit = true;
		}
	}

	return overLimit;

}

QTSS_Error RTSPSession::IsOkToAddNewRTPSession()
{
	QTSServerInterface* theServer = QTSServerInterface::GetServer();
	QTSS_ServerState theServerState = theServer->GetServerState();

	//we may want to deny this connection for a couple of different reasons
	//if the server is refusing new connections
	if ((theServerState == qtssRefusingConnectionsState) ||
		(theServerState == qtssIdleState) ||
		(theServerState == qtssFatalErrorState) ||
		(theServerState == qtssShuttingDownState))
		return QTSSModuleUtils::SendErrorResponse(fRequest, qtssServerUnavailable,
			qtssMsgRefusingConnections);

	//if the max connection limit has been hit 
	if (this->OverMaxConnections(0))
		return QTSSModuleUtils::SendErrorResponse(fRequest, qtssClientNotEnoughBandwidth,
			qtssMsgTooManyClients);

	//if the max bandwidth limit has been hit
	int32_t maxKBits = theServer->GetPrefs()->GetMaxKBitsBandwidth();
	if ((maxKBits > -1) && (theServer->GetCurBandwidthInBits() >= ((uint32_t)maxKBits * 1024)))
		return QTSSModuleUtils::SendErrorResponse(fRequest, qtssClientNotEnoughBandwidth,
			qtssMsgTooMuchThruput);

	//if the server is too loaded down (CPU too high, whatever)
	// --INSERT WORKING CODE HERE--

	return QTSS_NoErr;
}

void RTSPSession::SaveRequestAuthorizationParams(RTSPRequest *theRTSPRequest)
{
	// Set the RTSP session's copy of the user name
	boost::string_view userName = theRTSPRequest->GetAuthUserName();
	fUserName = std::string(userName);
	fRTPSession->SetUserName(userName);

	// Same thing... user password
	boost::string_view password = theRTSPRequest->GetPassWord();
	SetPassword(password);
	fRTPSession->SetPassword(password);

	boost::string_view tempPtr = theRTSPRequest->GetURLRealm();
	if (tempPtr.empty())
	{
		// If there is no realm explicitly specified in the request, then let's get the default out of the prefs
		std::unique_ptr<char[]> theDefaultRealm(QTSServerInterface::GetServer()->GetPrefs()->GetAuthorizationRealm());
		char *realm = theDefaultRealm.get();
		uint32_t len = ::strlen(theDefaultRealm.get());
		SetLastURLRealm({ realm, len });
		fRTPSession->SetRealm({ realm, len });
	}
	else
	{
		SetLastURLRealm(tempPtr);
		fRTPSession->SetRealm(tempPtr);
	}

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

	fCurrentModule = 0;
}