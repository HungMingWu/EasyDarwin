/*
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
    File:       RTPStream.h

    Contains:   Represents a single client stream (audio, video, etc).
                Control API is similar to overall session API.
                
                Contains all stream-specific data & resources, used by Session when it
                wants to send out or receive data for this stream
                
                This is also the class that implements the RTP stream dictionary
                for QTSS API.
                

*/

#ifndef __RTPSTREAM_H__
#define __RTPSTREAM_H__

#include <algorithm>
#include "Attributes.h"
#include "QTSS.h"

#include "UDPSocketPool.h"
#include "RTSPRequestInterface.h"
#include "QTSServerInterface.h"
#include "RTCPPacket.h"

class RTPSessionInterface;

class RTPStream
{
    public:
        
        //
        // CONSTRUCTOR / DESTRUCTOR
        
        RTPStream(uint32_t inSSRC, RTPSessionInterface* inSession);
        ~RTPStream();
        
        //
        //ACCESS uint8_t
        
        uint32_t      GetSSRC()                   { return fSsrc; }
        uint8_t       GetRTPChannelNum()          { return fRTPChannel; }
        uint8_t       GetRTCPChannelNum()         { return fRTCPChannel; }
        QTSS_RTPTransportType GetTransportType() { return fTransportType; }
        uint32_t      GetStalePacketsDropped()    { return fStalePacketsDropped; }
        uint32_t      GetTotalPacketsRecv()       { return fTotalPacketsRecv; }
		void          SetSDPStreamID(uint32_t id) { fTrackID = id; }
		RTPSessionInterface &GetSession()		{ return *fSession; }
        
        // Setup uses the info in the RTSPRequestInterface to associate
        // all the necessary resources, ports, sockets, etc, etc, with this
        // stream.
        QTSS_Error Setup(RTSPRequest* request, QTSS_AddStreamFlags inFlags);
        
        // Write sends RTP data to the client. Caller must specify
        // either qtssWriteFlagsIsRTP or qtssWriteFlagsIsRTCP
        QTSS_Error  Write(QTSS_PacketStruct* thePacket,
                                        uint32_t* outLenWritten, QTSS_WriteFlags inFlags);
        
        
        //UTILITY uint8_t_t:
        //These are not necessary to call and do not manipulate the state of the
        //stream. They may, however, be useful services exported by the server
        
        // Formats a standard setup response.
        void            SendSetupResponse(RTSPRequestInterface* request);

        //Formats a transport header for this stream. 
        void            AppendTransport(RTSPRequestInterface* request);
        
        //Formats a RTP-Info header for this stream.
        //Isn't useful unless you've already called Play()
        void            AppendRTPInfo(QTSS_RTSPHeader inHeader,
                                        RTSPRequestInterface* request, uint32_t inFlags, bool lastInfo);

        //
        // When we get an incoming Interleaved Packet for this stream, this
        // function should be called
        void ProcessIncomingInterleavedData(uint8_t inChannelNum, RTSPSessionInterface* inRTSPSession, StrPtrLen* inPacket);

        //When we get a new RTCP packet, we can directly invoke the RTP session and tell it
        //to process the packet right now!
        void ProcessIncomingRTCPPacket(StrPtrLen* inPacket);

        //Process the incoming ack RTCP packet
        bool ProcessAckPacket(RTCPPacket &rtcpPacket, int64_t &curTime);

        //Process the incoming qtss app RTCP packet
        bool ProcessCompressedQTSSPacket(RTCPPacket &rtcpPacket, int64_t &curTime, StrPtrLen &currentPtr);
        
        bool ProcessNADUPacket(RTCPPacket &rtcpPacket, int64_t &curTime, StrPtrLen &currentPtr, uint32_t highestSeqNum);


        // Send a RTCP SR on this stream. Pass in true if this SR should also have a BYE
        void SendRTCPSR(bool inAppendBye = false);
		
		void EnableSSRC() { fEnableSSRC = true; }
		void DisableSSRC() { fEnableSSRC = false; }
				
        void            SetMinQuality() { SetQualityLevel(fNumQualityLevels); }
        void            SetMaxQuality() { SetQualityLevel(kMaxQualityLevel); }
        int32_t          GetQualityLevel();
        void            SetQualityLevel(int32_t level);

		int32_t			GetMaxQualityLevelLimit() { return fMaxQualityLevel; }
		
		uint32_t          GetNumQualityLevels() { return fNumQualityLevels; } 
		void              SetNumQualityLevels(uint32_t level) { fNumQualityLevels = level; }
		QTSS_RTPPayloadType GetPayLoadType() { return fPayloadType; }
		void SetPayLoadType(QTSS_RTPPayloadType type) { fPayloadType = type; }
		uint32_t GetTimeScale() const {	return fTimescale; }
		void SetTimeScale(uint32_t scale) { fTimescale = scale; }
		float GetBufferDelay() const { return fBufferDelay; }
		bool isTCP() const { return fIsTCP; }
		uint32_t GetTotalLostPackets() const { return fTotalLostPackets; }
		uint16_t GetWorse() const { return fIsGettingWorse; }
		uint16_t GetBetter() const { return fIsGettingBetter; }
		void SetPayloadName(boost::string_view name) { fPayloadName = std::string(name); }
		boost::string_view GetPayloadName() const { return fPayloadName; }
		uint32_t GetPacketsLostInRTCPInterval() { return fCurPacketsLostInRTCPInterval; }
		uint32_t GetPacketCountInRTCPInterval() { return 0; }
		void SetTimeStamp(uint32_t timestamp) { fFirstTimeStamp = timestamp; }
		void SetSeqNumber(uint16_t number) { fFirstSeqNumber = number; }
		uint16_t GetSeqNumber() const { return fFirstSeqNumber; }
		inline void addAttribute(boost::string_view key, boost::any value) {
			attr.addAttribute(key, value);
		}
		inline boost::optional<boost::any> getAttribute(boost::string_view key) {
			return attr.getAttribute(key);
		}
		inline void removeAttribute(boost::string_view key) {
			attr.removeAttribute(key);
		}
    private:
        
        enum
        {
            kSenderReportIntervalInSecs = 7,
            kMaxQualityLevel            = 0,
            kIsRTCPPacket                 = true,
            kIsRTPPacket                  = false
        };
    
		int64_t fLastQualityChange{ 0 };
        int32_t fQualityInterval;

        //either pointers to the statically allocated sockets (maintained by the server)
        //or fresh ones (only fresh in extreme special cases)
		UDPSocketPair*          fSockets{ nullptr };
        RTPSessionInterface*    fSession;
        
        //who am i sending to?
		uint32_t      fRemoteAddr{ 0 };
		uint16_t      fRemoteRTPPort{ 0 };
		uint16_t      fRemoteRTCPPort{ 0 };
		uint16_t      fLocalRTPPort{ 0 };

        //RTCP stuff 
		int64_t      fLastSenderReportTime{ 0 };
		uint32_t      fPacketCount{ 0 };
		uint32_t      fLastPacketCount{ 0 };
		uint32_t      fPacketCountInRTCPInterval{ 0 };
		uint32_t      fByteCount{ 0 };
        
        // DICTIONARY ATTRIBUTES
        
        //Module assigns a streamID to this object
		uint32_t      fTrackID{ 0 };
        
        //low-level RTP stuff 
        uint32_t      fSsrc;
		bool      fEnableSSRC{ false };
        
        //Payload name and codec type.
        std::string         fPayloadName;
		QTSS_RTPPayloadType fPayloadType{ qtssUnknownPayloadType };

		Attributes attr;
        //Media information.
		uint16_t      fFirstSeqNumber{ 0 };//used in sending the play response
		uint32_t      fFirstTimeStamp{ 0 };//RTP time
		uint32_t      fTimescale{ 0 };
        
        //what is the URL for this stream?
        std::string   fStreamURL;
        
        int32_t      fQualityLevel;
		uint32_t      fNumQualityLevels{ 0 };
        
		uint32_t      fLastRTPTimestamp{ 0 };
		int64_t		fLastNTPTimeStamp{ 0 };
		uint32_t		fEstRTT{ 0 };				//The estimated RTT calculated from RTCP's DLSR and LSR fields
        
        // RTCP data
		uint32_t      fFractionLostPackets{ 0 };
		uint32_t      fTotalLostPackets{ 0 };
		uint32_t      fJitter{ 0 };
		uint16_t      fAvgBufDelayMsec{ 0 };
		uint16_t      fIsGettingBetter{ 0 };
		uint16_t      fIsGettingWorse{ 0 };
		uint32_t      fNumEyes{ 0 };
		uint32_t      fNumEyesActive{ 0 };
		uint32_t      fNumEyesPaused{ 0 };
		uint32_t      fTotalPacketsRecv{ 0 };
        uint32_t      fPriorTotalPacketsRecv;
		uint16_t      fTotalPacketsDropped{ 0 };
		uint16_t      fTotalPacketsLost{ 0 };
		uint32_t      fCurPacketsLostInRTCPInterval{ 0 };
		uint16_t      fClientBufferFill{ 0 };
		uint16_t      fFrameRate{ 0 };
		uint16_t      fExpectedFrameRate{ 0 };
		uint16_t      fAudioDryCount{ 0 };
		uint32_t      fClientSSRC{ 0 };
        
		bool      fIsTCP{ false };
		QTSS_RTPTransportType   fTransportType{ qtssRTPTransportTypeTCP };
        
        // HTTP params
        // Each stream has a set of thinning related tolerances,
        // that are dependent on prefs and parameters in the SETUP.
        // These params, as well as the current packet delay determine
        // whether a packet gets dropped.
		int32_t      fTurnThinningOffDelay_TCP{ 0 };
		int32_t      fIncreaseThinningDelay_TCP{ 0 };
		int32_t      fDropAllPacketsForThisStreamDelay_TCP{ 0 };
		uint32_t      fStalePacketsDropped_TCP{ 0 };
		int64_t      fTimeStreamCaughtUp_TCP{ 0 };
		int64_t      fLastQualityLevelIncreaseTime_TCP{ 0 };
        //
        // Each stream has a set of thinning related tolerances,
        // that are dependent on prefs and parameters in the SETUP.
        // These params, as well as the current packet delay determine
        // whether a packet gets dropped.
		uint32_t      fStalePacketsDropped{ 0 };
        
		float     fBufferDelay{ 3.0 }; // from the sdp
                       
		uint32_t      fCurrentAckTimeout{ 0 };
		int32_t      fMaxSendAheadTimeMSec{ 0 };

        // If we are interleaving RTP data over the TCP connection,
        // these are channel numbers to use for RTP & RTCP
		uint8_t   fRTPChannel{ 0 };
		uint8_t   fRTCPChannel{ 0 };
        
		QTSS_RTPNetworkMode     fNetworkMode{ qtssRTPNetworkModeDefault };
        
		int32_t fLastQualityLevel{ 0 };
		int32_t fLastRateLevel{ 0 };
       
        bool fDisableThinning;
		int64_t fLastQualityUpdate{ 0 };
        uint32_t fDefaultQualityLevel;
        int32_t fMaxQualityLevel;
        
        //-----------------------------------------------------------
        // acutally write the data out that way
        QTSS_Error  InterleavedWrite(const std::vector<char> &inBuffer, uint32_t* outLenWritten, unsigned char channel );
         
        void        SetTCPThinningParams();
                
        void            DisableThinning() { fDisableThinning = true; }
		void			SetInitialMaxQualityLevel();
        
        enum { rtp = 0, rtcpSR = 1, rtcpRR = 2, rtcpACK = 3, rtcpAPP = 4 };

        void SetOverBufferState(RTSPRequestInterface* request);
};

#endif // __RTPSTREAM_H__
