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
	 File:       RTPSessionInterface.h

	 Contains:   Implementation of object defined in .h
 */

#include "RTPSessionInterface.h"
#include "QTSServerInterface.h"
#include "RTSPRequestInterface.h"
#include "QTSS.h"
#include "OS.h"
#include "RTPStream.h"
#include "md5.h"
#include "md5digest.h"
#include "base64.h"

unsigned int            RTPSessionInterface::sRTPSessionIDCounter = 0;


QTSSAttrInfoDict::AttrInfo  RTPSessionInterface::sAttributes[] =
{   /*fields:   fAttrName, fFuncPtr, fAttrDataType, fAttrPermission */
	/* 0  */ {},
	/* 1  */ {},
	/* 2  */ {},
	/* 3  */ {},
	/* 4  */ {},
	/* 5  */ {},
	/* 6  */ {},
	/* 7  */ {},
	/* 8  */ {},
	/* 9  */ {},
	/* 10 */ {},
	/* 11 */ {},
	/* 12 */ {},
	/* 13 */ {} ,
	/* 14 */ {} ,
	/* 15 */ {},

	/* 16 */ {},
	/* 17 */ {},
	/* 18 */ {},
	/* 19 */ {},
	/* 20 */ {},
	/* 21 */ {},
	/* 22 */ {},
	/* 23 */ {},
	/* 24 */ {},
	/* 25 */ {},

	/* 26 */ {},
	/* 27 */ {},
	/* 28 */ {},
	/* 29 */ {},
	/* 30 */ {},
	/* 31 */ {},
	/* 32 */ {},
	/* 33 */ {},
	/* 34 */ {},
	/* 35 */ {},
	/* 36 */ {},
	/* 37 */ {}
};

void    RTPSessionInterface::Initialize()
{
	for (uint32_t x = 0; x < qtssCliSesNumParams; x++)
		QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kClientSessionDictIndex)->
		SetAttribute(x, sAttributes[x].fAttrName, sAttributes[x].fFuncPtr,
			sAttributes[x].fAttrDataType, sAttributes[x].fAttrPermission);
}

RTPSessionInterface::RTPSessionInterface()
	: QTSSDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kClientSessionDictIndex), nullptr),
	Task(),
	// assume true until proven false!
	fTimeoutTask(nullptr, QTSServerInterface::GetServer()->GetPrefs()->GetRTPSessionTimeoutInSecs() * 1000),
	fTracker(QTSServerInterface::GetServer()->GetPrefs()->IsSlowStartEnabled()),
	fOverbufferWindow(QTSServerInterface::GetServer()->GetPrefs()->GetSendIntervalInMsec(), UINT32_MAX, QTSServerInterface::GetServer()->GetPrefs()->GetMaxSendAheadTimeInSecs(),
		QTSServerInterface::GetServer()->GetPrefs()->GetOverbufferRate()),
	fAuthScheme(QTSServerInterface::GetServer()->GetPrefs()->GetAuthScheme())
{
	//don't actually setup the fTimeoutTask until the session has been bound!
	//(we don't want to get timeouts before the session gets bound)

	fTimeoutTask.SetTask(this);
	fTimeout = QTSServerInterface::GetServer()->GetPrefs()->GetRTPSessionTimeoutInSecs() * 1000;
	//fUniqueID = (uint32_t)atomic_add(&sRTPSessionIDCounter, 1);
	fUniqueID = ++sRTPSessionIDCounter;

	// fQualityUpdate is a counter the starting value is the unique ID so every session starts at a different position
	fQualityUpdate = fUniqueID;

	//mark the session create time
	fSessionCreateTime = OS::Milliseconds();
}

void RTPSessionInterface::UpdateRTSPSession(RTSPSessionInterface* inNewRTSPSession)
{
	if (inNewRTSPSession != fRTSPSession)
	{
		// If there was an old session, let it know that we are done
		if (fRTSPSession != nullptr)
			fRTSPSession->DecrementObjectHolderCount();

		// Increment this count to prevent the RTSP session from being deleted
		fRTSPSession = inNewRTSPSession;
		fRTSPSession->IncrementObjectHolderCount();
	}
}

char* RTPSessionInterface::GetSRBuffer(uint32_t inSRLen)
{
	if (fSRBuffer.Len < inSRLen)
	{
		delete[] fSRBuffer.Ptr;
		fSRBuffer.Set(new char[2 * inSRLen], 2 * inSRLen);
	}
	return fSRBuffer.Ptr;
}

QTSS_Error RTPSessionInterface::DoSessionSetupResponse(RTSPRequestInterface* inRequest)
{
	// This function appends a session header to the SETUP response, and
	// checks to see if it is a 304 Not Modified. If it is, it sends the entire
	// response and returns an error
	if (QTSServerInterface::GetServer()->GetPrefs()->GetRTSPTimeoutInSecs() > 0)  // adv the timeout
		inRequest->AppendSessionHeaderWithTimeout(GetSessionID(), QTSServerInterface::GetServer()->GetPrefs()->GetRTSPTimeoutAsString());
	else
		inRequest->AppendSessionHeaderWithTimeout(GetSessionID(), {}); // no timeout in resp.

	if (inRequest->GetStatus() == qtssRedirectNotModified)
	{
		(void)inRequest->SendHeader();
		return QTSS_RequestFailed;
	}
	return QTSS_NoErr;
}

void RTPSessionInterface::UpdateBitRateInternal(const int64_t& curTime)
{
	if (fState == qtssPausedState)
	{
		fMovieCurrentBitRate = 0;
		fLastBitRateUpdateTime = curTime;
		fLastBitRateBytes = fBytesSent;
	}
	else
	{
		uint32_t bitsInInterval = (fBytesSent - fLastBitRateBytes) * 8;
		int64_t updateTime = (curTime - fLastBitRateUpdateTime) / 1000;
		if (updateTime > 0) // leave Bit Rate the same if updateTime is 0 also don't divide by 0.
			fMovieCurrentBitRate = (uint32_t)(bitsInInterval / updateTime);
		fTracker.UpdateAckTimeout(bitsInInterval, curTime - fLastBitRateUpdateTime);
		fLastBitRateBytes = fBytesSent;
		fLastBitRateUpdateTime = curTime;
	}
	//printf("fMovieCurrentBitRate=%"   _U32BITARG_   "\n",fMovieCurrentBitRate);
	//printf("Cur bandwidth: %d. Cur ack timeout: %d.\n",fTracker.GetCurrentBandwidthInBps(), fTracker.RecommendedClientAckTimeout());
}

float RTPSessionInterface::GetPacketLossPercent()
{
	RTPStream* theStream = nullptr;
	uint32_t theLen = sizeof(theStream);

	int64_t packetsLost = 0;
	int64_t packetsSent = 0;

	for (auto theStream : fStreamBuffer)
	{
		uint32_t streamCurPacketsLost = theStream->GetPacketsLostInRTCPInterval();
		//printf("stream = %d streamCurPacketsLost = %"   _U32BITARG_   " \n",x, streamCurPacketsLost);

		uint32_t streamCurPackets = 0;
		theLen = sizeof(uint32_t);
		theStream->GetValue(qtssRTPStrPacketCountInRTCPInterval, 0, &streamCurPackets, &theLen);
		//printf("stream = %d streamCurPackets = %"   _U32BITARG_   " \n",x, streamCurPackets);

		packetsSent += (int64_t)streamCurPackets;
		packetsLost += (int64_t)streamCurPacketsLost;
		//printf("stream calculated loss = %f \n",x, (float) streamCurPacketsLost / (float) streamCurPackets);

	}

	//Assert(packetsLost <= packetsSent);
	if (packetsSent > 0)
	{
		if (packetsLost <= packetsSent)
			fPacketLossPercent = (float)((((float)packetsLost / (float)packetsSent) * 100.0));
		else
			fPacketLossPercent = 100.0;
	}
	else
		fPacketLossPercent = 0.0;

	return fPacketLossPercent;
}

void RTPSessionInterface::CreateDigestAuthenticationNonce() {

	// Calculate nonce: MD5 of sessionid:timestamp
	int64_t curTime = OS::Milliseconds();
	auto* curTimeStr = new char[128];
	sprintf(curTimeStr, "%" _64BITARG_ "d", curTime);

	// Delete old nonce before creating a new one
	if (fAuthNonce.Ptr != nullptr)
		delete[] fAuthNonce.Ptr;

	MD5_CTX ctxt;
	unsigned char nonceStr[16];
	unsigned char colon[] = ":";
	MD5_Init(&ctxt);
	boost::string_view sesID = this->GetSessionID();
	MD5_Update(&ctxt, (unsigned char *)sesID.data(), sesID.length());
	MD5_Update(&ctxt, (unsigned char *)colon, 1);
	MD5_Update(&ctxt, (unsigned char *)curTimeStr, ::strlen(curTimeStr));
	MD5_Final(nonceStr, &ctxt);
	HashToString(nonceStr, &fAuthNonce);

	delete[] curTimeStr; // No longer required once nonce is created

	// Set the nonce count value to zero 
	// as a new nonce has been created  
	fAuthNonceCount = 0;

}

void RTPSessionInterface::SetChallengeParams(QTSS_AuthScheme scheme, uint32_t qop, bool newNonce, bool createOpaque)
{
	// Set challenge params 
	// Set authentication scheme
	fAuthScheme = scheme;

	if (fAuthScheme == qtssAuthDigest) {
		// Set Quality of Protection 
		// auth-int (Authentication with integrity) not supported yet
		fAuthQop = qop;

		if (newNonce || (fAuthNonce.Ptr == nullptr))
			this->CreateDigestAuthenticationNonce();

		if (createOpaque) {
			// Generate a random uint32_t and convert it to a string 
			// The base64 encoded form of the string is made the opaque value
			int64_t theMicroseconds = OS::Microseconds();
			::srand((unsigned int)theMicroseconds);
			uint32_t randomNum = ::rand();
			auto* randomNumStr = new char[128];
			sprintf(randomNumStr, "%"   _U32BITARG_   "", randomNum);
			int len = ::strlen(randomNumStr);
			fAuthOpaque.Len = Base64encode_len(len);
			auto *opaqueStr = new char[fAuthOpaque.Len];
			(void)Base64encode(opaqueStr, randomNumStr, len);
			delete[] randomNumStr;                 // Don't need this anymore
			if (fAuthOpaque.Ptr != nullptr)             // Delete existing pointer before assigning new
				delete[] fAuthOpaque.Ptr;              // one
			fAuthOpaque.Ptr = opaqueStr;
		}
		else {
			if (fAuthOpaque.Ptr != nullptr)
				delete[] fAuthOpaque.Ptr;
			fAuthOpaque.Len = 0;
		}
		// Increase the Nonce Count by one
		// This number is a count of the next request the server
		// expects with this nonce. (Implies that the server
		// has already received nonce count - 1 requests that 
		// sent authorization with this nonce
		fAuthNonceCount++;
	}
}

void RTPSessionInterface::UpdateDigestAuthChallengeParams(bool newNonce, bool createOpaque, uint32_t qop) {
	if (newNonce || (fAuthNonce.Ptr == nullptr))
		this->CreateDigestAuthenticationNonce();


	if (createOpaque) {
		// Generate a random uint32_t and convert it to a string 
		// The base64 encoded form of the string is made the opaque value
		int64_t theMicroseconds = OS::Microseconds();
		::srand((unsigned int)theMicroseconds);
		uint32_t randomNum = ::rand();
		auto* randomNumStr = new char[128];
		sprintf(randomNumStr, "%"   _U32BITARG_   "", randomNum);
		int len = ::strlen(randomNumStr);
		fAuthOpaque.Len = Base64encode_len(len);
		auto *opaqueStr = new char[fAuthOpaque.Len];
		(void)Base64encode(opaqueStr, randomNumStr, len);
		delete[] randomNumStr;                 // Don't need this anymore
		if (fAuthOpaque.Ptr != nullptr)             // Delete existing pointer before assigning new
			delete[] fAuthOpaque.Ptr;              // one
		fAuthOpaque.Ptr = opaqueStr;
		fAuthOpaque.Len = ::strlen(opaqueStr);
	}
	else {
		if (fAuthOpaque.Ptr != nullptr)
			delete[] fAuthOpaque.Ptr;
		fAuthOpaque.Len = 0;
	}
	fAuthNonceCount++;

	fAuthQop = qop;
}
