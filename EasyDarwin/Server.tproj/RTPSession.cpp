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
	 File:       RTPSession.cpp

	 Contains:   Implementation of RTPSession class.

	 Change History (most recent first):
 */

#include "RTPSession.h"

#include "QTSServerInterface.h"
#include "QTSS.h"
#include "OS.h"
#include "RTSPRequest.h"
#include "QTSSReflectorModule.h"
#include "ServerPrefs.h"

RTPSession::RTPSession() :
	RTPSessionInterface()
{
#if DEBUG
	fActivateCalled = false;
#endif

	this->SetTaskName("RTPSession");
}

RTPSession::~RTPSession()
{
	// Delete all the streams
	for (auto theStream : fStreamBuffer)
		delete theStream;
}

QTSS_Error  RTPSession::Activate(boost::string_view inSessionID)
{
	//Set the session ID for this session
	fRTSPSessionID = std::string(inSessionID);
	fRTSPSessionIDV.Set((char *)fRTSPSessionID.c_str(), fRTSPSessionID.length());
	fRTPMapElem.Set(fRTSPSessionIDV, this);

	QTSServerInterface* theServer = getSingleton();

	//Activate puts the session into the RTPSession Map
	QTSS_Error err = theServer->GetRTPSessionMap()->Register(&fRTPMapElem);
	if (err == EPERM)
		return err;
	Assert(err == QTSS_NoErr);

#if DEBUG
	fActivateCalled = true;
#endif
	return QTSS_NoErr;
}

RTPStream*  RTPSession::FindRTPStreamForChannelNum(uint8_t inChannelNum)
{
	for (auto theStream : fStreamBuffer)
		if ((theStream->GetRTPChannelNum() == inChannelNum) || (theStream->GetRTCPChannelNum() == inChannelNum))
				return theStream;
	return nullptr; // Couldn't find a matching stream
}

QTSS_Error RTPSession::AddStream(RTSPRequest* request, RTPStream** outStream,
	QTSS_AddStreamFlags inFlags)
{
	Assert(outStream != nullptr);

	// Create a new SSRC for this stream. This should just be a random number unique
	// to all the streams in the session
	uint32_t theSSRC = 0;
	while (theSSRC == 0)
	{
		theSSRC = (int32_t)::rand();

		for (auto theStream : fStreamBuffer)
		{
			if (theStream->GetSSRC() == theSSRC)
				theSSRC = 0;
		}
	}

	*outStream = new RTPStream(theSSRC, this);

	QTSS_Error theErr = (*outStream)->Setup(request, inFlags);
	if (theErr != QTSS_NoErr)
		// If we couldn't setup the stream, make sure not to leak memory!
		delete *outStream;
	else
	{
		// If the stream init succeeded, then put it into the array of setup streams
		fStreamBuffer.push_back(*outStream);
	}
	return theErr;
}

QTSS_Error  RTPSession::Play(RTSPRequestInterface* request, QTSS_PlayFlags inFlags)
{
	//first setup the play associated session interface variables
	Assert(request != nullptr);

	//we are definitely playing now, so schedule the object!
	fState = qtssPlayingState;
	fPlayFlags = inFlags;

	this->Signal(Task::kStartEvent);

	return QTSS_NoErr;
}

void RTPSession::Pause()
{
	fState = qtssPausedState;

	for (auto theStream : fStreamBuffer)
	{
		//(*theStream)->Pause();
	}
}

void RTPSession::Teardown()
{
	OSMutexLocker locker(this->GetRTSPSessionMutex());
	// ourselves with it right now.

	// Note that this function relies on the session mutex being grabbed, because
	// this fRTSPSession pointer could otherwise be being used simultaneously by
	// an RTP stream.
	if (fRTSPSession != nullptr)
		fRTSPSession->DecrementObjectHolderCount();
	fRTSPSession = nullptr;
	fState = qtssPausedState;
	this->Signal(Task::kKillEvent);
}

void RTPSession::SendPlayResponse(RTSPRequestInterface* request, uint32_t inFlags)
{
	QTSS_RTSPHeader theHeader = qtssRTPInfoHeader;

	bool lastValue = false;
	for (size_t x = 0; x < fStreamBuffer.size(); x++)
	{
		RTPStream* theStream = fStreamBuffer[x];
		if (x == (fStreamBuffer.size() - 1))
			lastValue = true;
		theStream->AppendRTPInfo(theHeader, request, inFlags, lastValue);
		theHeader = qtssSameAsLastHeader;

	}
	request->SendHeader();
}

int64_t RTPSession::Run()
{
#if DEBUG
	Assert(fActivateCalled);
#endif
	EventFlags events = this->GetEvents();
	QTSS_RTPSendPackets_Params rtpSendPacketsParams;
	QTSS_ClientSessionClosing_Params clientSessionClosingParams;
	clientSessionClosingParams.inClientSession = this;    //every single role being invoked now has this
													//as the first parameter
	//if we have been instructed to go away, then let's delete ourselves
	if ((events & Task::kKillEvent) || (events & Task::kTimeoutEvent) || (fModuleDoingAsyncStuff))
	{
		if (!fModuleDoingAsyncStuff)
		{
			if (events & Task::kTimeoutEvent)
				fClosingReason = qtssCliSesCloseTimeout;

			//deletion is a bit complicated. For one thing, it must happen from within
			//the Run function to ensure that we aren't getting events when we are deleting
			//ourselves. We also need to make sure that we aren't getting RTSP requests
			//(or, more accurately, that the stream object isn't being used by any other
			//threads). We do this by first removing the session from the session map.

			// We cannot block waiting to UnRegister, because we have to
			// give the RTSPSessionTask a chance to release the RTPSession.
			OSRefTable* sessionTable = getSingleton()->GetRTPSessionMap();
			Assert(sessionTable != nullptr);
			if (!sessionTable->TryUnRegister(&fRTPMapElem))
			{
				this->Signal(Task::kKillEvent);// So that we get back to this place in the code
				return kCantGetMutexIdleTime;
			}

			// The ClientSessionClosing role is allowed to do async stuff
			fModuleDoingAsyncStuff = true;  // So that we know to jump back to the
			fCurrentModule = 0;             // right place in the code

			// Set the reason parameter 
			clientSessionClosingParams.inReason = fClosingReason;
		}

		//at this point, we know no one is using this session, so invoke the
		//session cleanup role. We don't need to grab the session mutex before
		//invoking modules here, because the session is unregistered and
		//therefore there's no way another thread could get involved anyway

		ReflectionModule::DestroySession(&clientSessionClosingParams);


		return -1;//doing this will cause the destructor to get called.
	}

	//if the stream is currently paused, just return without doing anything.
	//We'll get woken up again when a play is issued
	if (fState == qtssPausedState)
		return 0;

	//Make sure to grab the session mutex here, to protect the module against
	//RTSP requests coming in while it's sending packets
	{
		OSMutexLocker locker(&fSessionMutex);

		//just make sure we haven't been scheduled before our scheduled play
		//time. If so, reschedule ourselves for the proper time. (if client
		//sends a play while we are already playing, this may occur)
		rtpSendPacketsParams.inCurrentTime = OS::Milliseconds();
		if (fNextSendPacketsTime > rtpSendPacketsParams.inCurrentTime)
		{
			RTPStream** retransStream = nullptr;
			uint32_t retransStreamLen = 0;

			rtpSendPacketsParams.outNextPacketTime = fNextSendPacketsTime - rtpSendPacketsParams.inCurrentTime;
		}
		else
		{
			rtpSendPacketsParams.outNextPacketTime = 0;
			// Async event registration is definitely allowed from this role.
			//make sure not to get deleted accidently!
			if (rtpSendPacketsParams.outNextPacketTime < 0)
				rtpSendPacketsParams.outNextPacketTime = 0;
			fNextSendPacketsTime = rtpSendPacketsParams.inCurrentTime + rtpSendPacketsParams.outNextPacketTime;
		}

	}

	Assert(rtpSendPacketsParams.outNextPacketTime >= 0);//we'd better not get deleted accidently!
	return rtpSendPacketsParams.outNextPacketTime;
}

