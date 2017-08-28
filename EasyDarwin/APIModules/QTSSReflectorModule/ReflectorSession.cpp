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
#include "OS.h"
#include "QTSServerInterface.h"
#include <boost/asio/io_service.hpp>

#ifndef __Win32__
#include <unistd.h>
#endif

FileDeleter::FileDeleter(StrPtrLen* inSDPPath)
{
	Assert(inSDPPath);
	fFilePath.Len = inSDPPath->Len;
	fFilePath.Ptr = new char[inSDPPath->Len + 1];
	Assert(fFilePath.Ptr);
	memcpy(fFilePath.Ptr, inSDPPath->Ptr, inSDPPath->Len);
	fFilePath.Ptr[inSDPPath->Len] = 0;
}

FileDeleter::~FileDeleter()
{
	//printf("FileDeleter::~FileDeleter delete = %s \n",fFilePath.Ptr);
	::unlink(fFilePath.Ptr);
	delete fFilePath.Ptr;
	fFilePath.Ptr = nullptr;
	fFilePath.Len = 0;
}

extern boost::asio::io_service io_service;
ReflectorSession::ReflectorSession(boost::string_view inSourceID, uint32_t inChannelNum, SourceInfo* inInfo) :
	fSessionName(inSourceID),
	fChannelNum(inChannelNum),
	fSourceInfo(inInfo),
	fInitTimeMS(OS::Milliseconds()),
	fNoneOutputStartTimeMS(OS::Milliseconds()),
	timer(io_service)
{
	if (!fSessionName.empty())
	{
		char streamID[QTSS_MAX_NAME_LENGTH + 10] = { 0 };

		sprintf(streamID, "%s%s%d", fSessionName.c_str(), EASY_KEY_SPLITER, fChannelNum);
		fSourceID.Ptr = new char[::strlen(streamID) + 1];
		::strncpy(fSourceID.Ptr, streamID, strlen(streamID));
		fSourceID.Ptr[strlen(streamID)] = '\0';
		fSourceID.Len = strlen(streamID);
		fRef.Set(fSourceID, this);
	}
	timer.async_wait(std::bind(&ReflectorSession::Run, this, std::placeholders::_1));
}


ReflectorSession::~ReflectorSession()
{
	// For each stream, check to see if the ReflectorStream should be deleted
	for (uint32_t x = 0; x < fSourceInfo->GetNumStreams(); x++)
	{
		if (fStreamArray[x] == nullptr)
			continue;

		fStreamArray[x]->SetMyReflectorSession(nullptr);
	}

	// We own this object when it is given to us, so delete it now

	delete fSourceInfo;
	fSourceID.Delete();
}

QTSS_Error ReflectorSession::SetupReflectorSession(SourceInfo* inInfo, QTSS_StandardRTSP_Params* inParams, uint32_t inFlags, bool filterState, uint32_t filterTimeout)
{
	// use the current SourceInfo
	if (inInfo == nullptr)
		inInfo = fSourceInfo;

	// Store a reference to this sourceInfo permanently
	Assert((fSourceInfo == nullptr) || (inInfo == fSourceInfo));
	fSourceInfo = inInfo;

	// this must be set to the new SDP.
	fLocalSDP = inInfo->GetLocalSDP();

	fStreamArray.resize(fSourceInfo->GetNumStreams());

	for (uint32_t x = 0; x < fSourceInfo->GetNumStreams(); x++)
	{
		fStreamArray[x] = std::make_unique<ReflectorStream>(fSourceInfo->GetStreamInfo(x));
		// Obviously, we may encounter an error binding the reflector sockets.
		// If that happens, we'll just abort here, which will leave the ReflectorStream
		// array in an inconsistent state, so we need to make sure in our cleanup
		// code to check for NULL.
		QTSS_Error theError = fStreamArray[x]->BindSockets(inParams, inFlags, filterState, filterTimeout);
		if (theError != QTSS_NoErr)
		{
			fStreamArray[x] = nullptr;
			return theError;
		}
		fStreamArray[x]->SetMyReflectorSession(this);

		fStreamArray[x]->SetEnableBuffer(this->fHasBufferedStreams);// buffering is done by the stream's sender

		// If the port was 0, update it to reflect what the actual RTP port is.
		fSourceInfo->GetStreamInfo(x)->fPort = fStreamArray[x]->GetStreamInfo()->fPort;
		//printf("ReflectorSession::SetupReflectorSession fSourceInfo->GetStreamInfo(x)->fPort= %u\n",fSourceInfo->GetStreamInfo(x)->fPort);   
	}

	if (inFlags & kMarkSetup)
		fIsSetup = true;

	return QTSS_NoErr;
}

void ReflectorSession::AddBroadcasterClientSession(QTSS_StandardRTSP_Params* inParams)
{
	if (fStreamArray.empty() || nullptr == inParams)
		return;

	for (uint32_t x = 0; x < fSourceInfo->GetNumStreams(); x++)
	{
		if (fStreamArray[x] != nullptr)
		{   //printf("AddBroadcasterSession=%"   _U32BITARG_   "\n",inParams->inClientSession);
			((ReflectorSocket*)fStreamArray[x]->GetSocketPair()->GetSocketA())->AddBroadcasterSession(inParams->inClientSession);
			((ReflectorSocket*)fStreamArray[x]->GetSocketPair()->GetSocketB())->AddBroadcasterSession(inParams->inClientSession);
		}
	}
	fBroadcasterSession = inParams->inClientSession;
}

void    ReflectorSession::AddOutput(ReflectorOutput* inOutput, bool isClient)
{
	Assert(fSourceInfo->GetNumStreams() > 0);

	// We need to make sure that this output goes into the same bucket for each ReflectorStream.
	int32_t bucket = -1;
	int32_t lastBucket = -1;

	while (true)
	{
		uint32_t x = 0;
		for (; x < fSourceInfo->GetNumStreams(); x++)
		{
			bucket = fStreamArray[x]->AddOutput(inOutput, bucket);
			if (bucket == -1)   // If this output couldn't be added to this bucket,
				break;          // break and try again
			else
			{
				lastBucket = bucket; // Remember the last successful bucket placement.
				if (isClient)
					fStreamArray[x]->IncEyeCount();
			}
		}

		if (bucket == -1)
		{
			// If there was some kind of conflict adding this output to this bucket,
			// we need to remove it from the streams to which it was added.
			for (uint32_t y = 0; y < x; y++)
			{
				fStreamArray[y]->RemoveOutput(inOutput);
				if (isClient)
					fStreamArray[y]->DecEyeCount();
			}

			// Because there was an error, we need to start the whole process over again,
			// this time starting from a higher bucket
			lastBucket = bucket = lastBucket + 1;
		}
		else
			break;
	}
	//(void)atomic_add(&fNumOutputs, 1);
	++fNumOutputs;
}

void    ReflectorSession::RemoveOutput(ReflectorOutput* inOutput, bool isClient)
{
	//(void)atomic_sub(&fNumOutputs, 1);
	--fNumOutputs;
	for (uint32_t y = 0; y < fSourceInfo->GetNumStreams(); y++)
	{
		fStreamArray[y]->RemoveOutput(inOutput);
		if (isClient)
			fStreamArray[y]->DecEyeCount();
	}

	if (fNumOutputs == 0)
		SetNoneOutputStartTimeMS();
}

void ReflectorSession::TearDownAllOutputs()
{
	for (uint32_t y = 0; y < fSourceInfo->GetNumStreams(); y++)
		fStreamArray[y]->TearDownAllOutputs();
}

void    ReflectorSession::RemoveSessionFromOutput(RTPSession* inSession)
{
	for (uint32_t x = 0; x < fSourceInfo->GetNumStreams(); x++)
	{
		((ReflectorSocket*)fStreamArray[x]->GetSocketPair()->GetSocketA())->RemoveBroadcasterSession(inSession);
		((ReflectorSocket*)fStreamArray[x]->GetSocketPair()->GetSocketB())->RemoveBroadcasterSession(inSession);
	}
	fBroadcasterSession = nullptr;
}

uint32_t  ReflectorSession::GetBitRate()
{
	uint32_t retval = 0;
	for (const auto &streamArray : fStreamArray)
		if (streamArray != nullptr)
			retval += streamArray->GetBitRate();

	return retval;
}

bool ReflectorSession::Equal(SourceInfo* inInfo)
{
	return fSourceInfo->Equal(inInfo);
}

void*   ReflectorSession::GetStreamCookie(uint32_t inStreamID)
{
	for (uint32_t x = 0; x < fSourceInfo->GetNumStreams(); x++)
	{
		if (fSourceInfo->GetStreamInfo(x)->fTrackID == inStreamID)
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
	int64_t sNowTime = OS::Milliseconds();
	int64_t sNoneTime = GetNoneOutputStartTimeMS();
	if ((GetNumOutputs() == 0) && (sNowTime - sNoneTime >= /*QTSServerInterface::GetServer()->GetPrefs()->GetRTPSessionTimeoutInSecs()*/35 * 1000))
	{
	}
	else
	{
	}
	timer.expires_from_now(std::chrono::seconds(20));
	timer.async_wait(std::bind(&ReflectorSession::Run, this, std::placeholders::_1));
}
