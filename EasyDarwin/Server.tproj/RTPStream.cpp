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

#include "SocketUtils.h"
#include "RTSPRequest.h"
#include "ServerPrefs.h"

#define RTCP_TESTING 0

RTPStream::RTPStream(uint32_t inSSRC, RTPSessionInterface* inSession)
	: fSession(inSession),
	fSsrc(inSSRC),
	fQualityLevel(ServerPrefs::GetDefaultStreamQuality()),

	fDisableThinning(true), //ServerPrefs::GetDisableThinning()),
	fDefaultQualityLevel(ServerPrefs::GetDefaultStreamQuality()),
	fMaxQualityLevel(fDefaultQualityLevel)
{
}

RTPStream::~RTPStream()
{
	QTSS_Error err = QTSS_NoErr;
	if (fSockets != nullptr)
	{
		// If there is an UDP socket pair associated with this stream, make sure to free it up
		fSockets->GetSocketBDemux().UnregisterTask({ fRemoteAddr, fRemoteRTCPPort });
		Assert(err == QTSS_NoErr);

		getSingleton()->GetSocketPool()->ReleaseUDPSocketPair(fSockets);
	}
}

int32_t RTPStream::GetQualityLevel()
{
	return fSession->GetQualityLevel();
}

void RTPStream::SetQualityLevel(int32_t level)
{
	if (fDisableThinning)
		return;

	int32_t minLevel = std::max<int32_t>(0, fNumQualityLevels - 1);
	level = std::min<int32_t>(std::max<int32_t>(level, fMaxQualityLevel), minLevel);

	if (level == minLevel) //Instead of going down to key-frames only, go down to key-frames plus 1 P frame instead.
		level++;

	if (level == fQualityLevel)
		return;

	fSession->SetQualityLevel(level);

	fLastQualityLevel = level;
}

void  RTPStream::SetOverBufferState(RTSPRequestInterface* request)
{
	bool enableOverBuffer = false;

	switch (fTransportType)
	{
	case qtssRTPTransportTypeTCP:
		{
			enableOverBuffer = true; // default is on same as 4.0 and earlier. Allows tcp to compensate for falling behind from congestion or slow-start. 
		}
		break;

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

	//
	// decide whether to overbuffer
	this->SetOverBufferState(request);

	// Check to see if this RTP stream should be sent over TCP.
	if (fTransportType == qtssRTPTransportTypeTCP)
	{
		fIsTCP = true;
		fSession->GetOverbufferWindow()->SetWindowSize(UINT32_MAX);

		// If it is, get 2 channel numbers from the RTSP session.
		fRTPChannel = request->GetSession()->GetTwoChannelNumbers(fSession->GetSessionID());
		fRTCPChannel = fRTPChannel + 1;

		// If we are interleaving, this is all we need to do to setup.
		return QTSS_NoErr;
	}

	//
	// This track is not interleaved, so let the session know that all
	// tracks are not interleaved. This affects our scheduling of packets
	fSession->SetAllTracksInterleaved(false);

	//Get and store the remote addresses provided by the client. The remote addr is the
	//same as the RTSP client's IP address, unless an alternate was specified in the
	//transport header.
	fRemoteAddr = request->GetSession()->GetSocket()->GetRemoteAddr();
	if (request->GetDestAddr() != INADDR_ANY)
	{
		// Sending data to other addresses could be used in malicious ways, therefore
		// it is up to the module as to whether this sort of request might be allowed
		if (!(inFlags & qtssASFlagsAllowDestination))
			return request->SendErrorResponse(qtssClientBadRequest);
		fRemoteAddr = request->GetDestAddr();
	}
	fRemoteRTPPort = request->GetClientPortA();
	fRemoteRTCPPort = request->GetClientPortB();

	if ((fRemoteRTPPort == 0) || (fRemoteRTCPPort == 0))
		return request->SendErrorResponse(qtssClientBadRequest);

	//make sure that the client is advertising an even-numbered RTP port,
	//and that the RTCP port is actually one greater than the RTP port
	if ((fRemoteRTPPort & 1) != 0)
		return request->SendErrorResponse(qtssClientBadRequest);

	// comment out check below. This allows the rtcp port to be non-contiguous with the rtp port.
	//   if (fRemoteRTCPPort != (fRemoteRTPPort + 1))
	//       return QTSSModuleUtils::SendErrorResponse(request, qtssClientBadRequest, qtssMsgRTCPPortMustBeOneBigger);       

	   // Find the right source address for this stream. If it isn't specified in the
	   // RTSP request, assume it is the same interface as for the RTSP request.
	uint32_t sourceAddr = request->GetSession()->GetSocket()->GetLocalAddr();
	if ((request->GetSourceAddr() != INADDR_ANY) && (SocketUtils::IsLocalIPAddr(request->GetSourceAddr())))
		sourceAddr = request->GetSourceAddr();

	// if the transport is TCP or RUDP, then we only want one session quality level instead of a per stream one
	this->SetQualityLevel(*(fSession->GetQualityLevelPtr()));


	// If the destination address is multicast, we need to setup multicast socket options
	// on the sockets. Because these options may be different for each stream, we need
	// a dedicated set of sockets
	if (SocketUtils::IsMulticastIPAddr(fRemoteAddr))
	{
		fSockets = getSingleton()->GetSocketPool()->CreateUDPSocketPair(sourceAddr, 0);

		if (fSockets != nullptr)
		{
			//Set options on both sockets. Not really sure why we need to specify an
			//outgoing interface, because these sockets are already bound to an interface!
			QTSS_Error err = fSockets->GetSocketA()->SetTtl(request->GetTtl());
			if (err == QTSS_NoErr)
				err = fSockets->GetSocketB()->SetTtl(request->GetTtl());
			if (err == QTSS_NoErr)
				err = fSockets->GetSocketA()->SetMulticastInterface(fSockets->GetSocketA()->GetLocalAddr());
			if (err == QTSS_NoErr)
				err = fSockets->GetSocketB()->SetMulticastInterface(fSockets->GetSocketB()->GetLocalAddr());
			if (err != QTSS_NoErr)
				return request->SendErrorResponse(qtssServerInternal);
		}
	}
	else
		fSockets = getSingleton()->GetSocketPool()->GetUDPSocketPair(sourceAddr, 0, fRemoteAddr,
			fRemoteRTCPPort);

	if (fSockets == nullptr)
		return request->SendErrorResponse(qtssServerInternal);

	//
	// Record the Server RTP port
	fLocalRTPPort = fSockets->GetSocketA()->GetLocalPort();

	//finally, register with the demuxer to get RTCP packets from the proper address
	Assert(true == fSockets->GetSocketBDemux().RegisterTask({ fRemoteAddr, fRemoteRTCPPort }, this));
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
	std::string rtpTimeBuf = (inFlags & qtssPlayRespWriteTrackInfo) ? std::to_string(fFirstTimeStamp) : std::string{};
	std::string seqNumberBuf = (inFlags & qtssPlayRespWriteTrackInfo) ? std::to_string(fFirstSeqNumber) : std::string{};

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

QTSS_Error  RTPStream::Write(QTSS_PacketStruct* thePacket, uint32_t* outLenWritten, uint32_t inFlags)
{
	Assert(fSession != nullptr);
	if (!fSession->GetSessionMutex()->TryLock())
		return EAGAIN;


	QTSS_Error err = QTSS_NoErr;
	int64_t theTime = OS::Milliseconds();

	//
	// Data passed into this version of write must be a QTSS_PacketStruct
	int64_t theCurrentPacketDelay = theTime - thePacket->packetTransmitTime;

	//
	// Is this the first write in a write burst?
	if (inFlags & qtssWriteFlagsWriteBurstBegin)
		fSession->GetOverbufferWindow()->MarkBeginningOfWriteBurst();

	if (inFlags & qtssWriteFlagsIsRTCP)
	{
		if (fTransportType == qtssRTPTransportTypeTCP)// write out in interleave format on the RTSP TCP channel
		{
			err = this->InterleavedWrite(thePacket->packetData, outLenWritten, fRTCPChannel);
		}
	}
	else if (inFlags & qtssWriteFlagsIsRTP)
	{
		//
		// Check to make sure our quality level is correct. This function
		// also tells us whether this packet is just too old to send
		if (fTransportType == qtssRTPTransportTypeTCP)    // write out in interleave format on the RTSP TCP channel.
			err = this->InterleavedWrite(thePacket->packetData, outLenWritten, fRTPChannel);

		//if (err != QTSS_NoErr)
		//  printf("flow controlled\n");
		if (err == QTSS_NoErr && !thePacket->packetData.empty())
		{
			// Update statistics if we were actually able to send the data (don't
			// update if the socket is flow controlled or some such thing)

			fSession->GetOverbufferWindow()->AddPacketToWindow(thePacket->packetData.size());

			// Record the RTP timestamp for RTCPs
			auto* timeStampP = (uint32_t*)(&thePacket->packetData[0]);
			fLastRTPTimestamp = ntohl(timeStampP[1]);

			//stream statistics
			fPacketCount++;
			fByteCount += thePacket->packetData.size();

			// Send an RTCP sender report if it's time. Again, we only want to send an
			// RTCP if the RTP packet was sent sucessfully
			// If doing rate-adaptation, then send an RTCP SR every seconds so that we get faster RTT feedback.
			uint32_t senderReportInterval = kSenderReportIntervalInSecs;
			if ((fSession->GetPlayFlags() & qtssPlayFlagsSendRTCP) &&
				(theTime > (fLastSenderReportTime + (senderReportInterval * 1000))))
			{
				fLastSenderReportTime = theTime;
				// CISCO comments
				// thePacket->packetTransmissionTime is
				// the expected transmission time, which
				// is what we should report in RTCP for
				// synchronization purposes, not theTime,
				// which is the actual transmission time.
				this->SendRTCPSR();
			}

		}
	}
	else
	{
		fSession->GetSessionMutex()->Unlock();// Make sure to unlock the mutex
		return QTSS_BadArgument;//qtssWriteFlagsIsRTCP or qtssWriteFlagsIsRTP wasn't specified
	}

	if (outLenWritten != nullptr)
		*outLenWritten = thePacket->packetData.size();

	fSession->GetSessionMutex()->Unlock();// Make sure to unlock the mutex
	return err;
}



// SendRTCPSR is called by the session as well as the strem
// SendRTCPSR must be called from a fSession mutex protected caller
void RTPStream::SendRTCPSR(bool inAppendBye)
{
	// This will roll over, after which payloadByteCount will be all messed up.
	// But because it is a 32 bit number, that is bound to happen eventually,
	// and we are limited by the RTCP packet format in that respect, so this is
	// pretty much ok.
	uint32_t payloadByteCount = fByteCount - (12 * fPacketCount);

	RTCPSRPacket* theSR = fSession->GetSRPacket();
	theSR->SetSSRC(fSsrc);
	theSR->SetClientSSRC(fClientSSRC);
	//fLastNTPTimeStamp = fSession->GetNTPPlayTime() + OS::TimeMilli_To_Fixed64Secs(inTime - fSession->GetPlayTime());
	fLastNTPTimeStamp = OS::TimeMilli_To_1900Fixed64Secs(OS::Milliseconds()); //The time value should be filled in as late as possible.
	theSR->SetNTPTimestamp(fLastNTPTimeStamp);
	theSR->SetRTPTimestamp(fLastRTPTimestamp);
	theSR->SetPacketCount(fPacketCount);
	theSR->SetByteCount(payloadByteCount);
	theSR->SetAckTimeout(fSession->GetBandwidthTracker()->RecommendedClientAckTimeout());

	uint32_t thePacketLen = theSR->GetSRPacketLen();
	if (inAppendBye)
		thePacketLen = theSR->GetSRWithByePacketLen();
	std::vector<char> temp(theSR->GetSRPacket(), theSR->GetSRPacket() + thePacketLen);
	QTSS_Error err = QTSS_NoErr;
	if (fTransportType == qtssRTPTransportTypeTCP)    // write out in interleave format on the RTSP TCP channel
	{
		uint32_t  wasWritten;

		err = this->InterleavedWrite(temp, &wasWritten, fRTCPChannel);
	}
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


bool RTPStream::ProcessNADUPacket(RTCPPacket &rtcpPacket, int64_t &curTime, StrPtrLen &currentPtr, uint32_t highestSeqNum)
{
	RTCPNaduPacket naduPacket;
	uint8_t* packetBuffer = rtcpPacket.GetPacketBuffer();
	uint32_t packetLen = (rtcpPacket.GetPacketLength() * 4) + RTCPPacket::kRTCPHeaderSizeInBytes;

	if (!naduPacket.ParseAPPData((uint8_t*)currentPtr.Ptr, currentPtr.Len))
		return false;//abort if we discover a malformed app packet

	return true;
}

bool RTPStream::ProcessCompressedQTSSPacket(RTCPPacket &rtcpPacket, int64_t &curTime, StrPtrLen &currentPtr)
{
	RTCPCompressedQTSSPacket compressedQTSSPacket;
	uint8_t* packetBuffer = rtcpPacket.GetPacketBuffer();
	uint32_t packetLen = (rtcpPacket.GetPacketLength() * 4) + RTCPPacket::kRTCPHeaderSizeInBytes;

	if (!compressedQTSSPacket.ParseAPPData((uint8_t*)currentPtr.Ptr, currentPtr.Len))
		return false;//abort if we discover a malformed app packet

	fAvgBufDelayMsec = compressedQTSSPacket.GetAverageBufferDelayMilliseconds();
	fIsGettingBetter = (uint16_t)compressedQTSSPacket.GetIsGettingBetter();
	fIsGettingWorse = (uint16_t)compressedQTSSPacket.GetIsGettingWorse();
	fNumEyes = compressedQTSSPacket.GetNumEyes();
	fNumEyesActive = compressedQTSSPacket.GetNumEyesActive();
	fNumEyesPaused = compressedQTSSPacket.GetNumEyesPaused();
	fTotalPacketsRecv = compressedQTSSPacket.GetTotalPacketReceived();
	fTotalPacketsDropped = compressedQTSSPacket.GetTotalPacketsDropped();
	fTotalPacketsLost = compressedQTSSPacket.GetTotalPacketsLost();
	fClientBufferFill = compressedQTSSPacket.GetClientBufferFill();
	fFrameRate = compressedQTSSPacket.GetFrameRate();
	fExpectedFrameRate = compressedQTSSPacket.GetExpectedFrameRate();
	fAudioDryCount = compressedQTSSPacket.GetAudioDryCount();


	// Update our overbuffer window size to match what the client is telling us
	fSession->GetOverbufferWindow()->SetWindowSize(compressedQTSSPacket.GetOverbufferWindowSize());
	return true;
}


bool RTPStream::ProcessAckPacket(RTCPPacket &rtcpPacket, int64_t &curTime)
{
	RTCPAckPacket theAckPacket;
	uint8_t* packetBuffer = rtcpPacket.GetPacketBuffer();
	uint32_t packetLen = (rtcpPacket.GetPacketLength() * 4) + RTCPPacket::kRTCPHeaderSizeInBytes;

	if (!theAckPacket.ParseAPPData(packetBuffer, packetLen))
		return false;

	return true;
}

void RTPStream::ProcessIncomingRTCPPacket(StrPtrLen* inPacket)
{
	StrPtrLen currentPtr(*inPacket);
	int64_t curTime = OS::Milliseconds();
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
		// Increment our RTCP Packet and byte counters for the session.

		fSession->IncrTotalRTCPPacketsRecv();
		fSession->IncrTotalRTCPBytesRecv((int16_t)currentPtr.Len);

		switch (rtcpPacket.GetPacketType())
		{
		case RTCPPacket::kReceiverPacketType:
			{
			RTCPReceiverPacket receiverPacket;
			if (!receiverPacket.ParseReport((uint8_t*)currentPtr.Ptr, currentPtr.Len))
			{
				fSession->GetSessionMutex()->Unlock();
				return;//abort if we discover a malformed receiver report
			}

			//
			// Set the Client SSRC based on latest RTCP
			fClientSSRC = rtcpPacket.GetPacketSSRC();

			fFractionLostPackets = receiverPacket.GetCumulativeFractionLostPackets();
			fJitter = receiverPacket.GetCumulativeJitter();

			uint32_t curTotalLostPackets = receiverPacket.GetCumulativeTotalLostPackets();

			// Workaround for client problem.  Sometimes it appears to report a bogus lost packet count.
			// Since we can't have lost more packets than we sent, ignore the packet if that seems to be the case.
			if (curTotalLostPackets - fTotalLostPackets <= fPacketCount - fLastPacketCount)
			{
				// if current value is less than the old value, that means that the packets are out of order
				//  just wait for another packet that arrives in the right order later and for now, do nothing
				if (curTotalLostPackets > fTotalLostPackets)
				{
					//increment the server total by the new delta
					getSingleton()->IncrementTotalRTPPacketsLost(curTotalLostPackets - fTotalLostPackets);
					fCurPacketsLostInRTCPInterval = curTotalLostPackets - fTotalLostPackets;
					//                  printf("fCurPacketsLostInRTCPInterval = %d\n", fCurPacketsLostInRTCPInterval);
					fTotalLostPackets = curTotalLostPackets;
					hasPacketLoss = true;
				}
				else if (curTotalLostPackets == fTotalLostPackets)
				{
					fCurPacketsLostInRTCPInterval = 0;
					//                  printf("fCurPacketsLostInRTCPInterval set to 0\n");
				}


				fPacketCountInRTCPInterval = fPacketCount - fLastPacketCount;
				fLastPacketCount = fPacketCount;
			}

			//Marks down the highest sequence number received and calculates the RTT from the DLSR and the LSR
			if (receiverPacket.GetReportCount() > 0)
			{
				highestSeqNum = receiverPacket.GetHighestSeqNumReceived(0);

				uint32_t lsr = receiverPacket.GetLastSenderReportTime(0);
				uint32_t dlsr = receiverPacket.GetLastSenderReportDelay(0);

				if (lsr != 0)
				{
					uint32_t diff = static_cast<uint32_t>(OS::TimeMilli_To_1900Fixed64Secs(curTime) >> 16) - lsr - dlsr;
					auto measuredRTT = static_cast<uint32_t>(OS::Fixed64Secs_To_TimeMilli(static_cast<int64_t>(diff) << 16));

					if (measuredRTT < 60000) //make sure that the RTT is not some ridiculously large value
					{
						fEstRTT = fEstRTT == 0 ? measuredRTT : std::min<int32_t>(measuredRTT, fEstRTT);
					}
				}
			}

			}
			break;

		case RTCPPacket::kAPPPacketType:
			{
				bool packetOK = false;
				RTCPAPPPacket theAPPPacket;
				if (!theAPPPacket.ParseAPPPacket((uint8_t*)currentPtr.Ptr, currentPtr.Len))
				{
					fSession->GetSessionMutex()->Unlock();
					return;//abort if we discover a malformed receiver report
				}
				uint32_t itemName = theAPPPacket.GetAppPacketName();
				itemName = theAPPPacket.GetAppPacketName();
				switch (itemName)
				{

				case RTCPAckPacket::kAckPacketName:
				case RTCPAckPacket::kAckPacketAlternateName:
					{
						packetOK = this->ProcessAckPacket(rtcpPacket, curTime);
					}
					break;

				case RTCPCompressedQTSSPacket::kCompressedQTSSPacketName:
					{
						packetOK = this->ProcessCompressedQTSSPacket(rtcpPacket, curTime, currentPtr);
					}
					break;

				case RTCPNaduPacket::kNaduPacketName:
					{
						packetOK = this->ProcessNADUPacket(rtcpPacket, curTime, currentPtr, highestSeqNum);
						hasNADU = true;
					}
					break;

				default:
					{

					}
					break;
				}

				if (!packetOK)
				{
					fSession->GetSessionMutex()->Unlock();
					return;//abort if we discover a malformed receiver report
				}

			}
			break;
		default:
			break;

		}


		currentPtr.Ptr += (rtcpPacket.GetPacketLength() * 4) + 4;
		currentPtr.Len -= (rtcpPacket.GetPacketLength() * 4) + 4;
	}

	fSession->GetSessionMutex()->Unlock();
}