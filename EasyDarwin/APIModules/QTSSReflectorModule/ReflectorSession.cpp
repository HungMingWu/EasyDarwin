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
	File:       ReflectorSession.cpp
	Contains:   Implementation of object defined in ReflectorSession.h.
*/

#include <chrono>
#include "ReflectorSession.h"
#include "SocketUtils.h"
#include "QTSServerInterface.h"
#include <boost/asio/io_service.hpp>

extern boost::asio::io_service io_service;
ReflectorSession::ReflectorSession(boost::string_view inSourceID, const SDPSourceInfo& inInfo) :
	fSessionName(inSourceID),
	fSourceInfo(inInfo),
	timer(io_service)
{
	fRef.Set(&fSessionName[0], this);
	timer.async_wait(std::bind(&ReflectorSession::Run, this, std::placeholders::_1));
}


ReflectorSession::~ReflectorSession()
{
	for (auto &stream : fStreamArray)
		stream->SetMyReflectorSession(nullptr);
}

QTSS_Error ReflectorSession::SetupReflectorSession(QTSS_StandardRTSP_Params& inParams, uint32_t inFlags, bool filterState, uint32_t filterTimeout)
{
	// this must be set to the new SDP.
	fLocalSDP = fSourceInfo.GetLocalSDP();

	fStreamArray.resize(fSourceInfo.GetNumStreams());

	for (uint32_t x = 0; x < fSourceInfo.GetNumStreams(); x++)
	{
		fStreamArray[x] = std::make_unique<ReflectorStream>(fSourceInfo.GetStreamInfo(x));
		// Obviously, we may encounter an error binding the reflector sockets.
		// If that happens, we'll just abort here, which will leave the ReflectorStream
		// array in an inconsistent state, so we need to make sure in our cleanup
		// code to check for NULL.
		QTSS_Error theError = fStreamArray[x]->BindSockets(inParams.inRTSPRequest, inParams.inClientSession, inFlags, filterState, filterTimeout);
		if (theError != QTSS_NoErr)
		{
			fStreamArray[x] = nullptr;
			return theError;
		}
		fStreamArray[x]->SetMyReflectorSession(this);
		// If the port was 0, update it to reflect what the actual RTP port is.
		fSourceInfo.GetStreamInfo(x)->fPort = fStreamArray[x]->GetStreamInfo()->fPort;
		//printf("ReflectorSession::SetupReflectorSession fSourceInfo->GetStreamInfo(x)->fPort= %u\n",fSourceInfo->GetStreamInfo(x)->fPort);   
	}

	if (inFlags & kMarkSetup)
		fIsSetup = true;

	return QTSS_NoErr;
}

void ReflectorSession::AddBroadcasterClientSession(RTPSession* inClientSession)
{
	for (auto &stream : fStreamArray)
	{
		stream->GetSocketPair()->GetSocketA()->AddBroadcasterSession(inClientSession);
		stream->GetSocketPair()->GetSocketB()->AddBroadcasterSession(inClientSession);
	}
}

void    ReflectorSession::AddOutput(ReflectorOutput* inOutput, bool isClient)
{
	Assert(fSourceInfo.GetNumStreams() > 0);

	// We need to make sure that this output goes into the same bucket for each ReflectorStream.
	while (true)
	{
		uint32_t x = 0;
		for (; x < fSourceInfo.GetNumStreams(); x++)
		{
			fStreamArray[x]->AddOutput(inOutput);
			if (isClient)
				fStreamArray[x]->IncEyeCount();
		}

		break;
	}
}

void    ReflectorSession::RemoveOutput(ReflectorOutput* inOutput, bool isClient)
{
	for (uint32_t y = 0; y < fSourceInfo.GetNumStreams(); y++)
	{
		fStreamArray[y]->RemoveOutput(inOutput);
		if (isClient)
			fStreamArray[y]->DecEyeCount();
	}
}

void ReflectorSession::TearDownAllOutputs()
{
	for (auto &stream : fStreamArray)
		stream->TearDownAllOutputs();
}

void    ReflectorSession::RemoveSessionFromOutput()
{
	for (auto &stream : fStreamArray)
	{
		stream->GetSocketPair()->GetSocketA()->RemoveBroadcasterSession();
		stream->GetSocketPair()->GetSocketB()->RemoveBroadcasterSession();
	}
}

void*   ReflectorSession::GetStreamCookie(uint32_t inStreamID)
{
	for (uint32_t x = 0; x < fSourceInfo.GetNumStreams(); x++)
	{
		if (fSourceInfo.GetStreamInfo(x)->fTrackID == inStreamID)
			return fStreamArray[x]->GetStreamCookie();
	}
	return nullptr;
}

void ReflectorSession::Run(const boost::system::error_code &ec)
{
	if (ec == boost::asio::error::operation_aborted) {
		printf("ReflectorSession timer canceled\n");
		return;
	}
	timer.expires_from_now(std::chrono::seconds(20));
	timer.async_wait(std::bind(&ReflectorSession::Run, this, std::placeholders::_1));
}