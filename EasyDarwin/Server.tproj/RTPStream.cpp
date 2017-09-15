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

#if DEBUG
#define RTP_TCP_STREAM_DEBUG 1
#define RTP_RTCP_DEBUG 1
#else
#define RTP_TCP_STREAM_DEBUG 0
#define RTP_RTCP_DEBUG 0
#endif


#if RTP_RTCP_DEBUG
#define DEBUG_RTCP_PRINTF(s) printf s
#else
#define DEBUG_RTCP_PRINTF(s) {}
#endif

#define RTCP_TESTING 0

char *RTPStream::noType = "no-type";
char *RTPStream::UDP = "UDP";
char *RTPStream::RUDP = "RUDP";
char *RTPStream::TCP = "TCP";

RTPStream::RTPStream(uint32_t inSSRC, RTPSessionInterface* inSession)
	: fSession(inSession),
	fSsrc(inSSRC),
	fQualityLevel(ServerPrefs::GetDefaultStreamQuality()),

	fStreamStartTimeOSms(OS::Milliseconds()),
	fDisableThinning(ServerPrefs::GetDisableThinning()),
	fDefaultQualityLevel(ServerPrefs::GetDefaultStreamQuality()),
	fMaxQualityLevel(fDefaultQualityLevel),
	fUDPMonitorEnabled(ServerPrefs::GetUDPMonitorEnabled()),
	fMonitorVideoDestPort(ServerPrefs::GetUDPMonitorVideoPort()),
	fMonitorAudioDestPort(ServerPrefs::GetUDPMonitorAudioPort())
{
	if (fUDPMonitorEnabled)
	{
		std::string destIP(ServerPrefs::GetMonitorDestIP());
		std::string srcIP(ServerPrefs::GetMonitorSrcIP());

		fMonitorAddr = SocketUtils::ConvertStringToAddr(destIP.c_str());
		fPlayerToMonitorAddr = SocketUtils::ConvertStringToAddr(srcIP.c_str());

		fMonitorSocket = ::socket(AF_INET, SOCK_DGRAM, 0);
#ifdef __Win32__
		u_long one = 1;
		(void) ::ioctlsocket(fMonitorSocket, FIONBIO, &one);
#else
		int flag = ::fcntl(fMonitorSocket, F_GETFL, 0);
		(void) ::fcntl(fMonitorSocket, F_SETFL, flag | O_NONBLOCK);
#endif
	}
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

#if RTP_PACKET_RESENDER_DEBUGGING
	//fResender.LogClose(fFlowControlDurationMsec);
	//printf("Flow control duration msec: %" _64BITARG_ "d. Max outstanding packets: %d\n", fFlowControlDurationMsec, fResender.GetMaxPacketsInList());
#endif

#if RTP_TCP_STREAM_DEBUG
	if (fIsTCP)
		printf("DEBUG: ~RTPStream %li sends got EAGAIN'd.\n", (int32_t)fNumPacketsDroppedOnTCPFlowControl);
#endif

	if (fMonitorSocket != 0)
	{
#ifdef __Win32__
		::closesocket(fMonitorSocket);
#else   
		close(fMonitorSocket);
#endif
	}

}

int32_t RTPStream::GetQualityLevel()
{
	if (fTransportType == qtssRTPTransportTypeUDP)
		return std::min<int32_t>(fQualityLevel, fNumQualityLevels - 1);
	else
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

	if (fTransportType == qtssRTPTransportTypeUDP)
		fQualityLevel = level;
	else
		fSession->SetQualityLevel(level);

	fLastQualityLevel = level;
}

void  RTPStream::SetOverBufferState(RTSPRequestInterface* request)
{
	int32_t requestedOverBufferState = request->GetDynamicRateState();
	bool enableOverBuffer = false;

	switch (fTransportType)
	{
	case qtssRTPTransportTypeReliableUDP:
		{
			enableOverBuffer = true; // default is on
			if (requestedOverBufferState == 0) // client specifically set to false
				enableOverBuffer = false;
		}
		break;

	case qtssRTPTransportTypeUDP:
		{
			enableOverBuffer = false; // always off
		}
		break;


	case qtssRTPTransportTypeTCP:
		{

			enableOverBuffer = true; // default is on same as 4.0 and earlier. Allows tcp to compensate for falling behind from congestion or slow-start. 
			if (requestedOverBufferState == 0) // client specifically set to false
				enableOverBuffer = false;
		}
		break;

	}

	//over buffering is enabled for the session by default
	//if any stream turns it off then it is off for all streams
	//a disable is from either the stream type default or a specific rtsp command to disable
	if (!enableOverBuffer)
		fSession->GetOverbufferWindow()->TurnOverbuffering(false);
}

QTSS_Error RTPStream::Setup(RTSPRequest* request, QTSS_AddStreamFlags inFlags)
{
	//Get the URL for this track
	fStreamURL = request->GetFileName();//just in case someone wants to use string routines

	//
	// Store the late-tolerance value that came out of hte x-RTP-Options header,
	// so that when it comes time to determine our thinning params (when we PLAY),
	// we will know this
	fLateToleranceInSec = request->GetLateToleranceInSec();
	if (fLateToleranceInSec == -1.0)
		fLateToleranceInSec = 1.5;

	//
	// Setup the transport type
	fTransportType = request->GetTransportType();
	fNetworkMode = request->GetNetworkMode();

	//
	// Check to see if caller is forcing raw UDP transport
	if ((fTransportType == qtssRTPTransportTypeReliableUDP) && (inFlags & qtssASFlagsForceUDPTransport))
		fTransportType = qtssRTPTransportTypeUDP;

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
	if (fTransportType != qtssRTPTransportTypeUDP)
	{
		this->SetQualityLevel(*(fSession->GetQualityLevelPtr()));
	}


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

	else if (fTransportType == qtssRTPTransportTypeReliableUDP)
	{
		//
		// FIXME - we probably want to get rid of this slow start flag in the API
		bool useSlowStart = !(inFlags & qtssASFlagsDontUseSlowStart);
		if (!ServerPrefs::IsSlowStartEnabled())
			useSlowStart = false;

		fTracker = fSession->GetBandwidthTracker();

		fResender.SetBandwidthTracker(fTracker);
		fResender.SetDestination(fSockets->GetSocketA(), fRemoteAddr, fRemoteRTPPort);

#if RTP_PACKET_RESENDER_DEBUGGING
		if (QTSServerInterface::GetServer()->GetPrefs()->IsAckLoggingEnabled())
		{
			char        url[256];
			char        logfile[256];
			sprintf(logfile, "resend_log_%"   _U32BITARG_   "", fSession->GetRTSPSession()->GetSessionID());
			StrPtrLen   logName(logfile);
			fResender.SetLog(&logName);

			StrPtrLen   *presoURL = fSession->GetValue(qtssCliSesPresentationURL);
			uint32_t      clientAddr = request->GetSession()->GetSocket()->GetRemoteAddr();
			memcpy(url, presoURL->Ptr, presoURL->Len);
			url[presoURL->Len] = 0;
			printf("RTPStream::Setup for %s will use ACKS, ip addr: %li.%li.%li.%li\n", url, (clientAddr & 0xff000000) >> 24
				, (clientAddr & 0x00ff0000) >> 16
				, (clientAddr & 0x0000ff00) >> 8
				, (clientAddr & 0x000000ff)
			);
		}
#endif
	}

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
	if (ServerPrefs::GetRTSPTimeoutInSecs() > 0)  // adv the timeout
		inRequest->AppendSessionHeaderWithTimeout(fSession->GetSessionID(), std::to_string(ServerPrefs::GetRTSPTimeoutInSecs()));
	else
		inRequest->AppendSessionHeaderWithTimeout(fSession->GetSessionID(), {}); // no timeout in resp.

	this->AppendTransport(inRequest);

	//
	// Append the x-RTP-Options header if there was a late-tolerance field
	if (!inRequest->GetLateToleranceStr().empty())
		inRequest->AppendHeader(qtssXTransportOptionsHeader, inRequest->GetLateToleranceStr());

	//
	// Append the retransmit header if the client sent it
	boost::string_view theRetrHdr = inRequest->GetHeaderDict().Get(qtssXRetransmitHeader);
	if (!theRetrHdr.empty() && (fTransportType == qtssRTPTransportTypeReliableUDP))
		inRequest->AppendHeader(qtssXRetransmitHeader, theRetrHdr);

	// Append the dynamic rate header if the client sent it
	int32_t theRequestedRate = inRequest->GetDynamicRateState();
	static boost::string_view sHeaderOn("1");
	static boost::string_view sHeaderOff("0");
	if (theRequestedRate > 0)	// the client sent the header and wants a dynamic rate
	{
		if (fSession->GetOverbufferWindow()->GetOverbufferEnabled())
			inRequest->AppendHeader(qtssXDynamicRateHeader, sHeaderOn); // send 1 if overbuffering is turned on
		else
			inRequest->AppendHeader(qtssXDynamicRateHeader, sHeaderOff); // send 0 if overbuffering is turned off
	}
	else if (theRequestedRate == 0) // the client sent the header but doesn't want a dynamic rate
		inRequest->AppendHeader(qtssXDynamicRateHeader, sHeaderOff);
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
	request->AppendRTPInfoHeader(inHeader, fStreamURL, seqNumberBuf, {}, rtpTimeBuf, lastInfo);
}


//UDP Monitor reflected  write
void RTPStream::UDPMonitorWrite(void* thePacketData, uint32_t inLen, bool isRTCP)
{
	if (false == fUDPMonitorEnabled || 0 == fMonitorSocket || nullptr == thePacketData)
		return;

	if ((0 != fPlayerToMonitorAddr) && (this->fRemoteAddr != fPlayerToMonitorAddr))
		return;

	uint16_t RTCPportOffset = (true == isRTCP) ? 1 : 0;


	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(fMonitorAddr);

	if (fPayloadType == qtssVideoPayloadType)
		sin.sin_port = /*(in_port_t) */htons(fMonitorVideoDestPort + RTCPportOffset);
	else if (fPayloadType == qtssAudioPayloadType)
		sin.sin_port = /*(in_port_t)*/ htons(fMonitorAudioDestPort + RTCPportOffset);

	if (sin.sin_port != 0)
	{
		int result = ::sendto(fMonitorSocket, (const char*)thePacketData, inLen, 0, (struct sockaddr *)&sin, sizeof(struct sockaddr));
		//if (DEBUG)
		// {   if (result < 0)
		//         printf("RTCP Monitor Socket sendto failed\n");
		//     else if (0)
		//         printf("RTCP Monitor Socket sendto port=%hu, packetLen=%"   _U32BITARG_   "\n", ntohs(sin.sin_port), inLen);
		// }
	}

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
QTSS_Error  RTPStream::InterleavedWrite(void* inBuffer, uint32_t inLen, uint32_t* outLenWritten, unsigned char channel)
{

	if (fSession->GetRTSPSession() == nullptr) // RTSPSession required for interleaved write
	{
		return EAGAIN;
	}

	OSMutexLocker   locker(fSession->GetRTSPSessionMutex());

	//char blahblah[2048];

	QTSS_Error err = fSession->GetRTSPSession()->InterleavedWrite(inBuffer, inLen, outLenWritten, channel);
	//QTSS_Error err = fSession->GetRTSPSession()->InterleavedWrite( blahblah, 2044, outLenWritten, channel);
#if DEBUG
	//if (outLenWritten != NULL)
	//{
	//  Assert((*outLenWritten == 0) || (*outLenWritten == 2044));
	//}
#endif


#if DEBUG
	if (err == EAGAIN)
	{
		fNumPacketsDroppedOnTCPFlowControl++;
	}
#endif      

	// reset the timeouts when the connection is still alive
	// wehn transmitting over HTTP, we're not going to get
	// RTCPs that would normally Refresh the session time.
	if (err == QTSS_NoErr)
		fSession->RefreshTimeout(); // RTSP session gets refreshed internally in WriteV

#if RTP_TCP_STREAM_DEBUG
//printf( "DEBUG: RTPStream fCurrentPacketDelay %li, fQualityLevel %i\n", (int32_t)fCurrentPacketDelay, (int)fQualityLevel );
#endif

	return err;
}

//SendRetransmits must be called from a fSession mutex protected caller
void    RTPStream::SendRetransmits()
{

	if (fTransportType == qtssRTPTransportTypeReliableUDP)
		fResender.ResendDueEntries();


}

//ReliableRTPWrite must be called from a fSession mutex protected caller
QTSS_Error RTPStream::ReliableRTPWrite(void* inBuffer, uint32_t inLen, const int64_t& curPacketDelay)
{
	QTSS_Error err = QTSS_NoErr;

	// this must ALSO be called in response to a packet timeout
	// event that can be resecheduled as necessary by the fResender
	// for -hacking- purposes we'l do it just as we write packets,
	// but we won't be able to play low bit-rate movies ( like MIDI )
	// until this is a schedulable task

	// Send retransmits for all streams on this session
	//
	// Send retransmits if we need to
	for (auto retransStream : fSession->GetStreams())
	{
		//printf("Resending packets for stream: %d\n",(*retransStream)->fTrackID);
		//printf("RTPStream::ReliableRTPWrite. Calling ResendDueEntries\n");
		retransStream->fResender.ResendDueEntries();
	}

	if (!fSawFirstPacket)
	{
		fSawFirstPacket = true;
		fStreamCumDuration = 0;
		fStreamCumDuration = OS::Milliseconds() - fSession->GetPlayTime();
		//fInfoDisplayTimer.ResetToDuration( 1000 - fStreamCumDuration % 1000 );
	}

#if RTP_PACKET_RESENDER_DEBUGGING
	fResender.SetDebugInfo(fTrackID, fRemoteRTCPPort, curPacketDelay);
	fBytesSentThisInterval = fResender.SpillGuts(fBytesSentThisInterval);
#endif

	if (fResender.IsFlowControlled())
	{
		//      printf("Flow controlled\n");
#if DEBUG
		if (fFlowControlStartedMsec == 0)
		{
			//printf("Flow control start\n");
			fFlowControlStartedMsec = OS::Milliseconds();
		}
#endif
		err = QTSS_WouldBlock;
	}
	else
	{
#if DEBUG   
		if (fFlowControlStartedMsec != 0)
		{
			fFlowControlDurationMsec += OS::Milliseconds() - fFlowControlStartedMsec;
			fFlowControlStartedMsec = 0;
		}
#endif

		//
		// Assign a lifetime to the packet using the current delay of the packet and
		// the time until this packet becomes stale.
		fBytesSentThisInterval += inLen;
		fResender.AddPacket(inBuffer, inLen, (int32_t)(fDropAllPacketsForThisStreamDelay - curPacketDelay));

		(void)fSockets->GetSocketA()->SendTo(fRemoteAddr, fRemoteRTPPort, inBuffer, inLen);
	}


	return err;
}

void RTPStream::SetThinningParams()
{
	int32_t toleranceAdjust = 1500 - (int32_t(fLateToleranceInSec * 1000));

	if (fPayloadType == qtssVideoPayloadType)
		fDropAllPacketsForThisStreamDelay = ServerPrefs::GetDropAllVideoPacketsTimeInMsec() - toleranceAdjust;
	else
		fDropAllPacketsForThisStreamDelay = ServerPrefs::GetDropAllPacketsTimeInMsec() - toleranceAdjust;

	fThinAllTheWayDelay = ServerPrefs::GetThinAllTheWayTimeInMsec() - toleranceAdjust;
	fAlwaysThinDelay = ServerPrefs::GetAlwaysThinTimeInMsec() - toleranceAdjust;
	fStartThinningDelay = ServerPrefs::GetStartThinningTimeInMsec() - toleranceAdjust;
	fStartThickingDelay = ServerPrefs::GetStartThickingTimeInMsec() - toleranceAdjust;
	fThickAllTheWayDelay = ServerPrefs::GetThickAllTheWayTimeInMsec();
	fQualityCheckInterval = ServerPrefs::GetQualityCheckIntervalInMsec();
	fSession->fLastQualityCheckTime = 0;
	fSession->fLastQualityCheckMediaTime = 0;
	fSession->fStartedThinning = false;


}

void RTPStream::SetInitialMaxQualityLevel()
{
	uint32_t movieBitRate = GetSession().GetMovieAvgBitrate();
	uint32_t bandwidth = GetSession().GetMaxBandwidthBits();
	if (bandwidth != 0 && movieBitRate != 0)
	{
		double ratio = movieBitRate / static_cast<double>(bandwidth);

		//interpolate between ratio and fNumQualityLevels such that 0.90 maps to 0 and 3.0 maps to fNumQualityLevels
		SetMaxQualityLevelLimit(static_cast<int32_t>(fNumQualityLevels * (ratio / 2.1 - 0.43)));
		SetQualityLevel(GetQualityLevel());
	}
}


bool RTPStream::UpdateQualityLevel(const int64_t& inTransmitTime, const int64_t& inCurrentPacketDelay,
	const int64_t& inCurrentTime, uint32_t inPacketSize)
{
	Assert(fNumQualityLevels > 0);

	if (inTransmitTime <= fSession->GetPlayTime())
		return true;

	//->geyijyn@20150427
	//---视频流不进行thinning 算法丢帧
	//<- 
	if (fPayloadType == qtssVideoPayloadType)
		return true;

	if (fTransportType == qtssRTPTransportTypeUDP)
		return true;

	if (fSession->fLastQualityCheckTime == 0)
	{
		// Reset the interval for checking quality levels
		fSession->fLastQualityCheckTime = inCurrentTime;
		fSession->fLastQualityCheckMediaTime = inTransmitTime;
		fLastCurrentPacketDelay = inCurrentPacketDelay;
		return true;
	}

	if (!fSession->fStartedThinning)
	{
		// if we're still behind but not falling further behind, then don't thin
		if ((inCurrentPacketDelay > fStartThinningDelay) && (inCurrentPacketDelay - fLastCurrentPacketDelay < 250))
		{
			if (inCurrentPacketDelay < fLastCurrentPacketDelay)
				fLastCurrentPacketDelay = inCurrentPacketDelay;
			return true;
		}
		else
		{
			fSession->fStartedThinning = true;
		}
	}

	if ((fSession->fLastQualityCheckTime == 0) || (inCurrentPacketDelay > fThinAllTheWayDelay))
	{
		//
		// Reset the interval for checking quality levels
		fSession->fLastQualityCheckTime = inCurrentTime;
		fSession->fLastQualityCheckMediaTime = inTransmitTime;
		fLastCurrentPacketDelay = inCurrentPacketDelay;

		if (inCurrentPacketDelay > fThinAllTheWayDelay)
		{
			//
			// If we have fallen behind enough such that we risk trasmitting
			// stale packets to the client, AGGRESSIVELY thin the stream
			this->SetMinQuality();
			//          if (fPayloadType == qtssVideoPayloadType)
			//              printf("Q=%d, delay = %qd\n", GetQualityLevel(), inCurrentPacketDelay);
			if (inCurrentPacketDelay > fDropAllPacketsForThisStreamDelay)
			{
				fStalePacketsDropped++;
				return false; // We should not send this packet
			}
		}
	}

	if (fNumQualityLevels <= 2)
	{
		if ((inCurrentPacketDelay < fStartThickingDelay) && (GetQualityLevel() > 0))
			this->SetMaxQuality();

		return true;        // not enough quality levels to do fine tuning
	}

	if (((inCurrentTime - fSession->fLastQualityCheckTime) > fQualityCheckInterval) ||
		((inTransmitTime - fSession->fLastQualityCheckMediaTime) > fQualityCheckInterval))
	{
		if ((inCurrentPacketDelay > fAlwaysThinDelay) && (GetQualityLevel() < (int32_t)fNumQualityLevels))
			SetQualityLevel(GetQualityLevel() + 1);
		else if ((inCurrentPacketDelay > fStartThinningDelay) && (inCurrentPacketDelay > fLastCurrentPacketDelay))
		{
			if (!fWaitOnLevelAdjustment && (GetQualityLevel() < (int32_t)fNumQualityLevels))
			{
				SetQualityLevel(GetQualityLevel() + 1);
				fWaitOnLevelAdjustment = true;
			}
			else
				fWaitOnLevelAdjustment = false;
		}

		if ((inCurrentPacketDelay < fStartThickingDelay) && (GetQualityLevel() > 0) && (inCurrentPacketDelay < fLastCurrentPacketDelay))
		{
			SetQualityLevel(GetQualityLevel() - 1);
			fWaitOnLevelAdjustment = true;
		}

		if (inCurrentPacketDelay < fThickAllTheWayDelay)
		{
			this->SetMaxQuality();
			fWaitOnLevelAdjustment = false;
		}

		//		if (fPayloadType == qtssVideoPayloadType)
		//			printf("Q=%d, delay = %qd\n", GetQualityLevel(), inCurrentPacketDelay);
		fLastCurrentPacketDelay = inCurrentPacketDelay;
		fSession->fLastQualityCheckTime = inCurrentTime;
		fSession->fLastQualityCheckMediaTime = inTransmitTime;
	}

	return true; // We should send this packet
}


QTSS_Error  RTPStream::Write(void* inBuffer, uint32_t inLen, uint32_t* outLenWritten, uint32_t inFlags)
{
	Assert(fSession != nullptr);
	if (!fSession->GetSessionMutex()->TryLock())
		return EAGAIN;


	QTSS_Error err = QTSS_NoErr;
	int64_t theTime = OS::Milliseconds();

	//
	// Data passed into this version of write must be a QTSS_PacketStruct
	auto* thePacket = (QTSS_PacketStruct*)inBuffer;
	thePacket->suggestedWakeupTime = -1;
	int64_t theCurrentPacketDelay = theTime - thePacket->packetTransmitTime;

	//If we are doing rate-adaptation, set the maximum quality level if the bandwidth header is received
	if (!fInitialMaxQualityLevelIsSet && !fDisableThinning)
	{
		fInitialMaxQualityLevelIsSet = true;
		SetInitialMaxQualityLevel();
	}

	//
	// Empty the overbuffer window
	fSession->GetOverbufferWindow()->EmptyOutWindow(theTime);

	//
	// Update the bit rate value
	fSession->UpdateCurrentBitRate(theTime);

	//
	// Is this the first write in a write burst?
	if (inFlags & qtssWriteFlagsWriteBurstBegin)
		fSession->GetOverbufferWindow()->MarkBeginningOfWriteBurst();

	if (inFlags & qtssWriteFlagsIsRTCP)
	{
		//
		// Check to see if this packet is ready to send
		if (false == fSession->GetOverbufferWindow()->GetOverbufferEnabled()) // only force rtcps on time if overbuffering is off
		{
			thePacket->suggestedWakeupTime = fSession->GetOverbufferWindow()->CheckTransmitTime(thePacket->packetTransmitTime, theTime, inLen);
			if (thePacket->suggestedWakeupTime > theTime)
			{
				Assert(thePacket->suggestedWakeupTime >= fSession->GetOverbufferWindow()->GetSendInterval());
				fSession->GetSessionMutex()->Unlock();// Make sure to unlock the mutex
				return QTSS_WouldBlock;
			}
		}

		if (fTransportType == qtssRTPTransportTypeTCP)// write out in interleave format on the RTSP TCP channel
		{
			err = this->InterleavedWrite(thePacket->packetData, inLen, outLenWritten, fRTCPChannel);
		}
		else if (inLen > 0)
		{
			(void)this->fSockets->GetSocketB()->SendTo(fRemoteAddr, fRemoteRTCPPort, thePacket->packetData, inLen);

			this->UDPMonitorWrite(thePacket->packetData, inLen, kIsRTCPPacket);

		}
;
	}
	else if (inFlags & qtssWriteFlagsIsRTP)
	{
		{   //
			// Check to see if this packet fits in the overbuffer window
			thePacket->suggestedWakeupTime = fSession->GetOverbufferWindow()->CheckTransmitTime(thePacket->packetTransmitTime, theTime, inLen);
		}

		if (thePacket->suggestedWakeupTime > theTime)
		{
			// Assert(thePacket->suggestedWakeupTime >= fSession->GetOverbufferWindow()->GetSendInterval());
#if RTP_PACKET_RESENDER_DEBUGGING
			fResender.logprintf("Overbuffer window full. Num bytes in overbuffer: %d. Wakeup time: %qd\n", fSession->GetOverbufferWindow()->AvailableSpaceInWindow(), thePacket->packetTransmitTime);
#endif
			//printf("Overbuffer window full. Returning: %qd\n", thePacket->suggestedWakeupTime - theTime);

			fSession->GetSessionMutex()->Unlock();// Make sure to unlock the mutex
			return QTSS_WouldBlock;
		}

		//
		// Check to make sure our quality level is correct. This function
		// also tells us whether this packet is just too old to send
		if (this->UpdateQualityLevel(thePacket->packetTransmitTime, theCurrentPacketDelay, theTime, inLen))
		{
			if (fTransportType == qtssRTPTransportTypeTCP)    // write out in interleave format on the RTSP TCP channel.
				err = this->InterleavedWrite(thePacket->packetData, inLen, outLenWritten, fRTPChannel);
			else if (fTransportType == qtssRTPTransportTypeReliableUDP)
				err = this->ReliableRTPWrite(thePacket->packetData, inLen, theCurrentPacketDelay);
			else if (inLen > 0)
			{
				(void)fSockets->GetSocketA()->SendTo(fRemoteAddr, fRemoteRTPPort, thePacket->packetData, inLen);

				this->UDPMonitorWrite(thePacket->packetData, inLen, kIsRTPPacket);
			}

			auto* theSeqNumP = (uint16_t*)thePacket->packetData;
			uint16_t theSeqNum = ntohs(theSeqNumP[1]);

#if 0 // testing
			{
				if (err == 0)
				{
					static int64_t time = -1;
					static int byteCount = 0;
					static int64_t startTime = -1;
					static int totalBytes = 0;
					static int numPackets = 0;
					static int64_t firstTime;

					if (theTime - time > 1000)
					{
						if (time != -1)
						{
							printf("   %qd KBit (%d in %qd secs)", byteCount * 8 * 1000 / (theTime - time) / 1024, totalBytes, (theTime - startTime) / 1000);
							if (fTracker)
								printf(" Window = %d\n", fTracker->CongestionWindow());
							else
								printf("\n");
							printf("Packet #%d xmit time = %qd\n", numPackets, (thePacket->packetTransmitTime - firstTime) / 1000);
						}
						else
						{
							startTime = theTime;
							firstTime = thePacket->packetTransmitTime;
						}

						byteCount = 0;
						time = theTime;
					}

					byteCount += inLen;
					totalBytes += inLen;
					numPackets++;

					printf("Packet %d for time %qd sent at %qd (%d bytes)\n", theSeqNum, thePacket->packetTransmitTime - fSession->GetPlayTime(), theTime - fSession->GetPlayTime(), inLen);
				}
			}
#endif

		}

#if RTP_PACKET_RESENDER_DEBUGGING
		if (err != QTSS_NoErr)
			fResender.logprintf("Flow controlled: %qd Overbuffer window: %d. Cur time %qd\n", theCurrentPacketDelay, fSession->GetOverbufferWindow()->AvailableSpaceInWindow(), theTime);
		else
			fResender.logprintf("Sent packet: %d. Overbuffer window: %d Transmit time %qd. Cur time %qd\n", ntohs(theSeqNum[1]), fSession->GetOverbufferWindow()->AvailableSpaceInWindow(), thePacket->packetTransmitTime, theTime);
#endif
		//if (err != QTSS_NoErr)
		//  printf("flow controlled\n");
		if (err == QTSS_NoErr && inLen > 0)
		{
			// Update statistics if we were actually able to send the data (don't
			// update if the socket is flow controlled or some such thing)

			fSession->GetOverbufferWindow()->AddPacketToWindow(inLen);
			fSession->UpdatePacketsSent(1);
			fSession->UpdateBytesSent(inLen);
			getSingleton()->IncrementTotalRTPBytes(inLen);
			getSingleton()->IncrementTotalPackets();

			getSingleton()->IncrementTotalLate(theCurrentPacketDelay);
			getSingleton()->IncrementTotalQuality(this->GetQualityLevel());

			// Record the RTP timestamp for RTCPs
			auto* timeStampP = (uint32_t*)(thePacket->packetData);
			fLastRTPTimestamp = ntohl(timeStampP[1]);

			//stream statistics
			fPacketCount++;
			fByteCount += inLen;

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
				this->SendRTCPSR(thePacket->packetTransmitTime);
			}

		}
	}
	else
	{
		fSession->GetSessionMutex()->Unlock();// Make sure to unlock the mutex
		return QTSS_BadArgument;//qtssWriteFlagsIsRTCP or qtssWriteFlagsIsRTP wasn't specified
	}

	if (outLenWritten != nullptr)
		*outLenWritten = inLen;

	fSession->GetSessionMutex()->Unlock();// Make sure to unlock the mutex
	return err;
}



// SendRTCPSR is called by the session as well as the strem
// SendRTCPSR must be called from a fSession mutex protected caller
void RTPStream::SendRTCPSR(const int64_t& inTime, bool inAppendBye)
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
#if RTP_PACKET_RESENDER_DEBUGGING
	fResender.logprintf("Recommending ack timeout of: %d\n", fSession->GetBandwidthTracker()->RecommendedClientAckTimeout());
#endif
	theSR->SetAckTimeout(fSession->GetBandwidthTracker()->RecommendedClientAckTimeout());

	uint32_t thePacketLen = theSR->GetSRPacketLen();
	if (inAppendBye)
		thePacketLen = theSR->GetSRWithByePacketLen();

	QTSS_Error err = QTSS_NoErr;
	if (fTransportType == qtssRTPTransportTypeTCP)    // write out in interleave format on the RTSP TCP channel
	{
		uint32_t  wasWritten;
		err = this->InterleavedWrite(theSR->GetSRPacket(), thePacketLen, &wasWritten, fRTCPChannel);
	}
	else
	{
		void *ptr = theSR->GetSRPacket();
		err = fSockets->GetSocketB()->SendTo(fRemoteAddr, fRemoteRTCPPort, ptr, thePacketLen);
		this->UDPMonitorWrite(ptr, thePacketLen, kIsRTCPPacket);
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


	fReceiverBitRate = compressedQTSSPacket.GetReceiverBitRate();
	fAvgLateMsec = compressedQTSSPacket.GetAverageLateMilliseconds();

	fPercentPacketsLost = compressedQTSSPacket.GetPercentPacketsLost();
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
	if (fTransportType != qtssRTPTransportTypeUDP)
	{
		//  printf("Setting over buffer to %d\n", compressedQTSSPacket.GetOverbufferWindowSize());
		fSession->GetOverbufferWindow()->SetWindowSize(compressedQTSSPacket.GetOverbufferWindowSize());
	}

#ifdef DEBUG_RTCP_PACKETS
	compressedQTSSPacket.Dump();
#endif

	return true;

}


bool RTPStream::ProcessAckPacket(RTCPPacket &rtcpPacket, int64_t &curTime)
{
	RTCPAckPacket theAckPacket;
	uint8_t* packetBuffer = rtcpPacket.GetPacketBuffer();
	uint32_t packetLen = (rtcpPacket.GetPacketLength() * 4) + RTCPPacket::kRTCPHeaderSizeInBytes;

	if (!theAckPacket.ParseAPPData(packetBuffer, packetLen))
		return false;

	if (nullptr != fTracker && false == fTracker->ReadyForAckProcessing()) // this stream must be ready to receive acks.  Between RTSP setup and sending of first packet on stream we must protect against a bad ack.
		return false;//abort if we receive an ack when we haven't sent anything.

	// Only check for ack packets if we are using Reliable UDP
	if (fTransportType == qtssRTPTransportTypeReliableUDP)
	{
		uint16_t theSeqNum = theAckPacket.GetAckSeqNum();
		fResender.AckPacket(theSeqNum, curTime);
		//printf("Got ack: %d\n",theSeqNum);

		for (uint16_t maskCount = 0; maskCount < theAckPacket.GetAckMaskSizeInBits(); maskCount++)
		{
			if (theAckPacket.IsNthBitEnabled(maskCount))
			{
				fResender.AckPacket(theSeqNum + maskCount + 1, curTime);
				//printf("Got ack in mask: %d\n",theSeqNum + maskCount + 1);
			}
		}

	}

	return true;



}

bool RTPStream::TestRTCPPackets(StrPtrLen* inPacketPtr, uint32_t itemName)
{
	// Testing?
	if (!RTCP_TESTING)
		return false;


	itemName = RTCPNaduPacket::kNaduPacketName;


	printf("RTPStream::TestRTCPPackets received packet inPacketPtr.Ptr=%p inPacketPtr.len =%lu\n", inPacketPtr->Ptr, inPacketPtr->Len);

	switch (itemName)
	{

	case RTCPAckPacket::kAckPacketName:
	case RTCPAckPacket::kAckPacketAlternateName:
		{
			printf("testing RTCPAckPacket");
			RTCPAckPacket::GetTestPacket(inPacketPtr);
		}
		break;

	case RTCPCompressedQTSSPacket::kCompressedQTSSPacketName:
		{
			printf("testing RTCPCompressedQTSSPacket");
			RTCPCompressedQTSSPacket::GetTestPacket(inPacketPtr);
		}
		break;

	case RTCPNaduPacket::kNaduPacketName:
		{
			printf("testing RTCPNaduPacket");
			RTCPNaduPacket::GetTestPacket(inPacketPtr);
		}
		break;

	};

	printf(" using packet inPacketPtr.Ptr=%p inPacketPtr.len =%lu\n", inPacketPtr->Ptr, inPacketPtr->Len);

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

	this->TestRTCPPackets(&currentPtr, 0);

	while (currentPtr.Len > 0)
	{
		DEBUG_RTCP_PRINTF(("RTPStream::ProcessIncomingRTCPPacket start parse rtcp currentPtr.Len = %"   _U32BITARG_   "\n", currentPtr.Len));

		/*
			Due to the variable-type nature of RTCP packets, this is a bit unusual...
			We initially treat the packet as a generic RTCPPacket in order to determine its'
			actual packet type.  Once that is figgered out, we treat it as its' actual packet type
		*/
		RTCPPacket rtcpPacket;
		if (!rtcpPacket.ParsePacket((uint8_t*)currentPtr.Ptr, currentPtr.Len))
		{
			fSession->GetSessionMutex()->Unlock();
			DEBUG_RTCP_PRINTF(("malformed rtcp packet\n"));
			return;//abort if we discover a malformed RTCP packet
		}
		// Increment our RTCP Packet and byte counters for the session.

		fSession->IncrTotalRTCPPacketsRecv();
		fSession->IncrTotalRTCPBytesRecv((int16_t)currentPtr.Len);

		switch (rtcpPacket.GetPacketType())
		{
		case RTCPPacket::kReceiverPacketType:
			{   DEBUG_RTCP_PRINTF(("RTPStream::ProcessIncomingRTCPPacket kReceiverPacketType\n"));
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

#ifdef DEBUG_RTCP_PACKETS
			receiverPacket.Dump();
#endif
			}
			break;

		case RTCPPacket::kAPPPacketType:
			{
				DEBUG_RTCP_PRINTF(("RTPStream::ProcessIncomingRTCPPacket kAPPPacketType\n"));
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

		case RTCPPacket::kSDESPacketType:
			{
				DEBUG_RTCP_PRINTF(("RTPStream::ProcessIncomingRTCPPacket kSDESPacketType\n"));
#ifdef DEBUG_RTCP_PACKETS
				SourceDescriptionPacket sdesPacket;
				if (!sdesPacket.ParsePacket((uint8_t*)currentPtr.Ptr, currentPtr.Len))
				{
					fSession->GetSessionMutex()->Unlock();
					return;//abort if we discover a malformed app packet
				}

				sedsPacket.Dump();
#endif
			}
			break;

		default:
			DEBUG_RTCP_PRINTF(("RTPStream::ProcessIncomingRTCPPacket Unknown Packet Type\n"));
			//  WarnV(false, "Unknown RTCP Packet Type");
			break;

		}


		currentPtr.Ptr += (rtcpPacket.GetPacketLength() * 4) + 4;
		currentPtr.Len -= (rtcpPacket.GetPacketLength() * 4) + 4;

		DEBUG_RTCP_PRINTF(("RTPStream::ProcessIncomingRTCPPacket end parse rtcp currentPtr.Len = %"   _U32BITARG_   "\n", currentPtr.Len));
	}

	// Invoke the RTCP modules, allowing them to process this packet
	QTSS_RoleParams theParams;
	theParams.rtcpProcessParams.inRTPStream = this;
	theParams.rtcpProcessParams.inClientSession = (RTPSession *)fSession;
	theParams.rtcpProcessParams.inRTCPPacketData = inPacket->Ptr;
	theParams.rtcpProcessParams.inRTCPPacketDataLen = inPacket->Len;

	fSession->GetSessionMutex()->Unlock();
}

float RTPStream::GetStreamStartTimeSecs()
{
	return (float)((OS::Milliseconds() - this->fSession->GetSessionCreateTime()) / 1000.0);
}

char* RTPStream::GetStreamTypeStr()
{
	char *streamType = nullptr;

	switch (fTransportType)
	{
	case qtssRTPTransportTypeUDP:   streamType = RTPStream::UDP;
		break;

	case qtssRTPTransportTypeReliableUDP: streamType = RTPStream::RUDP;
		break;

	case qtssRTPTransportTypeTCP: streamType = RTPStream::TCP;
		break;

	default:
		streamType = RTPStream::noType;
	};

	return streamType;
}

void RTPStream::PrintRTP(char* packetBuff, uint32_t inLen)
{

	uint16_t sequence = ntohs(((uint16_t*)packetBuff)[1]);
	uint32_t timestamp = ntohl(((uint32_t*)packetBuff)[1]);
	uint32_t ssrc = ntohl(((uint32_t*)packetBuff)[2]);



	if (fFirstTimeStamp == 0)
		fFirstTimeStamp = timestamp;

	float rtpTimeInSecs = 0.0;
	if (fTimescale > 0 && fFirstTimeStamp < timestamp)
		rtpTimeInSecs = (float)(timestamp - fFirstTimeStamp) / (float)fTimescale;

	if (!fPayloadName.empty())
		printf("%s\n", fPayloadName.c_str());
	else
		printf("?");


	printf(" H_ssrc=%" _S32BITARG_ " H_seq=%u H_ts=%"   _U32BITARG_   " seq_count=%"   _U32BITARG_   " ts_secs=%.3f \n", ssrc, sequence, timestamp, fPacketCount + 1, rtpTimeInSecs);

}


void RTPStream::PrintRTCPSenderReport(char* packetBuff, uint32_t inLen)
{

	auto* theReport = (uint32_t*)packetBuff;

	theReport++;
	uint32_t ssrc = htonl(*theReport);

	theReport++;
	int64_t ntp = 0;
	::memcpy(&ntp, theReport, sizeof(int64_t));
	ntp = OS::NetworkToHostSInt64(ntp);
	time_t theTime = OS::Time1900Fixed64Secs_To_UnixTimeSecs(ntp);

	theReport += 2;
	uint32_t timestamp = ntohl(*theReport);
	float theTimeInSecs = 0.0;

	if (fFirstTimeStamp == 0)
		fFirstTimeStamp = timestamp;

	if (fTimescale > 0 && fFirstTimeStamp < timestamp)
		theTimeInSecs = (float)(timestamp - fFirstTimeStamp) / (float)fTimescale;

	theReport++;
	uint32_t packetcount = ntohl(*theReport);

	theReport++;
	uint32_t bytecount = ntohl(*theReport);

	if (!fPayloadName.empty())
		printf("%s\n", fPayloadName.c_str());
	else
		printf("?");

	printf(" H_ssrc=%"   _U32BITARG_   " H_bytes=%"   _U32BITARG_   " H_ts=%"   _U32BITARG_   " H_pckts=%"   _U32BITARG_   " ts_secs=%.3f H_ntp=%s\n",
		ssrc, bytecount, timestamp, packetcount, theTimeInSecs, std::ctime(&theTime));
}