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
	 File:       RTPStream.cpp

	 Contains:   Implementation of RTPStream class.
 */



#include <stdlib.h>
#include <ctime>
#include <memory>

#ifndef __Win32__
#include <arpa/inet.h>
#include <fcntl.h>
#endif
#include "RTPStream.h"
#include "RTPSessionInterface.h"

#include "QTSServerInterface.h"
#include "OS.h"

#include "RTCPPacket.h"
#include "RTCPAPPPacket.h"
#include "RTCPAPPQTSSPacket.h"
#include "RTCPAckPacket.h"
#include "RTCPSRPacket.h"
#include "RTCPAPPNADUPacket.h"

#include "RTSPRequest.h"
#include "ServerPrefs.h"

#define RTCP_TESTING 0

RTPStream::RTPStream(uint32_t inSSRC, RTPSessionInterface* inSession)
	: fSession(inSession),
	fSsrc(inSSRC)
{
}

RTPStream::~RTPStream()
{
	if (fSockets != nullptr)
	{
		getSingleton()->GetSocketPool()->ReleaseUDPSocketPair(fSockets);
	}
}

QTSS_Error RTPStream::Setup(RTSPRequest* request, QTSS_AddStreamFlags inFlags)
{
	//Get the URL for this track
	fStreamURL = request->GetFileName();//just in case someone wants to use string routines

	//
	// Setup the transport type
	fTransportType = request->GetTransportType();
	fNetworkMode = request->GetNetworkMode();

	// Check to see if this RTP stream should be sent over TCP.
	if (fTransportType == qtssRTPTransportTypeTCP)
	{
		fIsTCP = true;

		// If it is, get 2 channel numbers from the RTSP session.
		fRTPChannel = request->GetSession()->GetTwoChannelNumbers(fSession->GetSessionID());
		fRTCPChannel = fRTPChannel + 1;

		// If we are interleaving, this is all we need to do to setup.
		return QTSS_NoErr;
	}

	return QTSS_NoErr;
}

void RTPStream::SendSetupResponse(RTSPRequestInterface* inRequest)
{
	// This function appends a session header to the SETUP response, and
	// checks to see if it is a 304 Not Modified. If it is, it sends the entire
	// response and returns an error
	inRequest->AppendSessionHeader(fSession->GetSessionID());

	this->AppendTransport(inRequest);

	//else the client didn't send a header so do nothing 

	inRequest->SendHeader();
}

void RTPStream::AppendTransport(RTSPRequestInterface* request)
{

	std::string ssrcStr = (fEnableSSRC) ? std::to_string(fSsrc) : std::string();

	// We are either going to append the RTP / RTCP port numbers (UDP),
	// or the channel numbers (TCP, interleaved)
	if (!fIsTCP)
	{
		//
		// With UDP retransmits its important the client starts sending RTCPs
		// to the right address right away. The sure-firest way to get the client
		// to do this is to put the src address in the transport. So now we do that always.
		//
		boost::string_view theSrcIPAddress = ServerPrefs::GetTransportSrcAddr();
		if (theSrcIPAddress.empty()) {
			//StrPtrLen *p = fSockets->GetSocketA()->GetLocalAddrStr();
			theSrcIPAddress = std::string("127.0.0.1");
		}


		if (request->IsPushRequest())
		{
			std::string rtpPort = std::to_string(request->GetSetUpServerPort());
			std::string rtcpPort = std::to_string(request->GetSetUpServerPort() + 1);
			request->AppendTransportHeader(rtpPort, rtcpPort, {}, {}, theSrcIPAddress, ssrcStr);
		}
		else
		{
			// Append UDP socket port numbers.
			UDPSocket* theRTPSocket = fSockets->GetSocketA();
			UDPSocket* theRTCPSocket = fSockets->GetSocketB();
			StrPtrLen *p = theRTPSocket->GetLocalPortStr();
			StrPtrLen *p1 = theRTCPSocket->GetLocalPortStr();
			request->AppendTransportHeader(std::string(p->Ptr, p->Len),
				std::string(p1->Ptr, p1->Len), {}, {}, theSrcIPAddress, ssrcStr);
		}
	}
	else
	{
		// If these channel numbers fall outside prebuilt range, we will have to call sprintf.
		std::string rtpChannel = std::to_string(fRTPChannel);
		std::string rtcpChannel = std::to_string(fRTCPChannel);
		request->AppendTransportHeader({}, {}, rtpChannel, rtcpChannel, {}, ssrcStr);
	}
}

void    RTPStream::AppendRTPInfo(QTSS_RTSPHeader inHeader, RTSPRequestInterface* request, uint32_t inFlags, bool lastInfo)
{
	//format strings for the various numbers we need to send back to the client
	std::string rtpTimeBuf = {}; // (inFlags & qtssPlayRespWriteTrackInfo) ? std::to_string(fFirstTimeStamp) : std::string{};
	std::string seqNumberBuf = {}; // (inFlags & qtssPlayRespWriteTrackInfo) ? std::to_string(fFirstSeqNumber) : std::string{};

	// There is no SSRC in RTP-Info header, it goes in the transport header.
	request->AppendRTPInfoHeader(inHeader, fStreamURL, seqNumberBuf, rtpTimeBuf, lastInfo);
}

/*********************************
/
/   InterleavedWrite
/
/   Write the given RTP packet out on the RTSP channel in interleaved format.
/   update quality levels and statistics
/   on success refresh the RTP session timeout to keep it alive
/
*/

//ReliableRTPWrite must be called from a fSession mutex protected caller
QTSS_Error  RTPStream::InterleavedWrite(const std::vector<char> &inBuffer, uint32_t* outLenWritten, unsigned char channel)
{

	if (fSession->GetRTSPSession() == nullptr) // RTSPSession required for interleaved write
	{
		return EAGAIN;
	}

	OSMutexLocker   locker(fSession->GetRTSPSessionMutex());

	//char blahblah[2048];

	QTSS_Error err = fSession->GetRTSPSession()->InterleavedWrite(inBuffer, outLenWritten, channel);
	//QTSS_Error err = fSession->GetRTSPSession()->InterleavedWrite( blahblah, 2044, outLenWritten, channel);

	// reset the timeouts when the connection is still alive
	// wehn transmitting over HTTP, we're not going to get
	// RTCPs that would normally Refresh the session time.
	if (err == QTSS_NoErr)
		fSession->RefreshTimeout(); // RTSP session gets refreshed internally in WriteV

	return err;
}

QTSS_Error  RTPStream::Write(const std::vector<char> &thePacket, uint32_t* outLenWritten, uint32_t inFlags)
{
	Assert(fSession != nullptr);
	if (!fSession->GetSessionMutex()->TryLock())
		return EAGAIN;


	QTSS_Error err = QTSS_NoErr;
	int64_t theTime = OS::Milliseconds();

	if (inFlags & qtssWriteFlagsIsRTCP)
	{
		if (fTransportType == qtssRTPTransportTypeTCP)// write out in interleave format on the RTSP TCP channel
		{
			err = this->InterleavedWrite(thePacket, outLenWritten, fRTCPChannel);
		}
	}
	else if (inFlags & qtssWriteFlagsIsRTP)
	{
		//
		// Check to make sure our quality level is correct. This function
		// also tells us whether this packet is just too old to send
		if (fTransportType == qtssRTPTransportTypeTCP)    // write out in interleave format on the RTSP TCP channel.
			err = this->InterleavedWrite(thePacket, outLenWritten, fRTPChannel);

		//if (err != QTSS_NoErr)
		//  printf("flow controlled\n");
		if (err == QTSS_NoErr && !thePacket.empty())
		{
		}
	}
	else
	{
		fSession->GetSessionMutex()->Unlock();// Make sure to unlock the mutex
		return QTSS_BadArgument;//qtssWriteFlagsIsRTCP or qtssWriteFlagsIsRTP wasn't specified
	}

	if (outLenWritten != nullptr)
		*outLenWritten = thePacket.size();

	fSession->GetSessionMutex()->Unlock();// Make sure to unlock the mutex
	return err;
}

void RTPStream::ProcessIncomingInterleavedData(uint8_t inChannelNum, RTSPSessionInterface* inRTSPSession, StrPtrLen* inPacket)
{
	if (inChannelNum == fRTPChannel)
	{
		//
		// Currently we don't do anything with incoming RTP packets. Eventually,
		// we might need to make a role to deal with these
	}
	else if (inChannelNum == fRTCPChannel)
		this->ProcessIncomingRTCPPacket(inPacket);
}

void RTPStream::ProcessIncomingRTCPPacket(StrPtrLen* inPacket)
{
	StrPtrLen currentPtr(*inPacket);
	bool hasPacketLoss = false;
	uint32_t highestSeqNum = 0;
	bool hasNADU = false;

	// Modules are guarenteed atomic access to the session. Also, the RTSP Session accessed
	// below could go away at any time. So we need to lock the RTP session mutex.
	// *BUT*, when this function is called the caller already has the UDP socket pool &
	// UDP Demuxer mutexes. Blocking on grabbing this mutex could cause a deadlock.
	// So, dump this RTCP packet if we can't get the mutex.
	if (!fSession->GetSessionMutex()->TryLock())
		return;

	//no matter what happens (whether or not this is a valid packet) reset the timeouts
	fSession->RefreshTimeout();
	if (fSession->GetRTSPSession() != nullptr)
		fSession->GetRTSPSession()->RefreshTimeout();

	while (currentPtr.Len > 0)
	{
		/*
			Due to the variable-type nature of RTCP packets, this is a bit unusual...
			We initially treat the packet as a generic RTCPPacket in order to determine its'
			actual packet type.  Once that is figgered out, we treat it as its' actual packet type
		*/
		RTCPPacket rtcpPacket;
		if (!rtcpPacket.ParsePacket((uint8_t*)currentPtr.Ptr, currentPtr.Len))
		{
			fSession->GetSessionMutex()->Unlock();
			return;//abort if we discover a malformed RTCP packet
		}

		switch (rtcpPacket.GetPacketType())
		{
		case RTCPPacket::kReceiverPacketType:
			break;

		case RTCPPacket::kAPPPacketType:
			break;
		default:
			break;

		}


		currentPtr.Ptr += (rtcpPacket.GetPacketLength() * 4) + 4;
		currentPtr.Len -= (rtcpPacket.GetPacketLength() * 4) + 4;
	}

	fSession->GetSessionMutex()->Unlock();
}