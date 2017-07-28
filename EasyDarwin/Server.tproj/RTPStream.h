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

#include "QTSS.h"
#include "QTSSDictionary.h"
#include "QTSS_Private.h"

#include "UDPDemuxer.h"
#include "UDPSocketPool.h"

#include "RTSPRequestInterface.h"
#include "RTPSessionInterface.h"
#include "RTPPacketResender.h"
#include "QTSServerInterface.h"
#include "RTCPPacket.h"

#ifndef MIN
#define	MIN(a,b) (((a)<(b))?(a):(b))
#endif /* MIN */
#ifndef MAX
#define	MAX(a,b) (((a)>(b))?(a):(b))
#endif	/* MAX */

class RTPStream : public QTSSDictionary, public UDPDemuxerTask
{
    public:
        
        // Initializes dictionary resources
        static void Initialize();

        //
        // CONSTRUCTOR / DESTRUCTOR
        
        RTPStream(uint32_t inSSRC, RTPSessionInterface* inSession);
        ~RTPStream() override;
        
        //
        //ACCESS uint8_t
        
        uint32_t      GetSSRC()                   { return fSsrc; }
        uint8_t       GetRTPChannelNum()          { return fRTPChannel; }
        uint8_t       GetRTCPChannelNum()         { return fRTCPChannel; }
        RTPPacketResender* GetResender()        { return &fResender; }
        QTSS_RTPTransportType GetTransportType() { return fTransportType; }
        uint32_t      GetStalePacketsDropped()    { return fStalePacketsDropped; }
        uint32_t      GetTotalPacketsRecv()       { return fTotalPacketsRecv; }
        uint32_t      GetSDPStreamID()            { return fTrackID; } //streamID is trackID
		RTPSessionInterface &GetSession()		{ return *fSession; }
        
        // Setup uses the info in the RTSPRequestInterface to associate
        // all the necessary resources, ports, sockets, etc, etc, with this
        // stream.
        QTSS_Error Setup(RTSPRequestInterface* request, QTSS_AddStreamFlags inFlags);
        
        // Write sends RTP data to the client. Caller must specify
        // either qtssWriteFlagsIsRTP or qtssWriteFlagsIsRTCP
        QTSS_Error  Write(void* inBuffer, uint32_t inLen,
                                        uint32_t* outLenWritten, QTSS_WriteFlags inFlags) override;
        
        
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
        void SendRTCPSR(const int64_t& inTime, bool inAppendBye = false);
        
        //
        // Retransmits get sent when there is new data to be sent, but this function
        // should be called periodically even if there is no new packet data, as
        // the pipe should have a steady stream of data in it. 
        void SendRetransmits();

        //
        // Update the thinning parameters for this stream to match current prefs
        void SetThinningParams();
        
		//
		// Reset the delay parameters that are stored for the thinning calculations
		void ResetThinningDelayParams() { fLastCurrentPacketDelay = 0; }
		
		void SetLateTolerance(float inLateToleranceInSec) { fLateToleranceInSec = inLateToleranceInSec; }
		
		void EnableSSRC() { fEnableSSRC = true; }
		void DisableSSRC() { fEnableSSRC = false; }
				
        void            SetMinQuality() { SetQualityLevel(fNumQualityLevels); }
        void            SetMaxQuality() { SetQualityLevel(kMaxQualityLevel); }
        int32_t          GetQualityLevel();
        void            SetQualityLevel(int32_t level);
		void			HalveQualityLevel()
		{
			uint32_t minLevel = fNumQualityLevels - 1;
			SetQualityLevel(minLevel - (minLevel - GetQualityLevel()) / 2);
		}
		void			SetMaxQualityLevelLimit(int32_t newMaxLimit) //Changes what is the best quality level possible
		{
			int32_t minLevel = MAX(0, (int32_t) fNumQualityLevels - 2); //do not drop down  to key frames
			fMaxQualityLevel = MAX(MIN(minLevel, newMaxLimit), 0);
			SetQualityLevel(GetQualityLevel());
		}

		int32_t			GetMaxQualityLevelLimit() { return fMaxQualityLevel; }
		
		uint32_t          GetNumQualityLevels() { return fNumQualityLevels; } 
		QTSS_RTPPayloadType GetPayLoadType() { return fPayloadType; }
		
    private:
        
        enum
        {
            kMaxSsrcSizeInBytes         = 12,
            kDefaultPayloadBufSize      = 32,
            kSenderReportIntervalInSecs = 7,
            kNumPrebuiltChNums          = 10,
            kMaxQualityLevel            = 0,
            kIsRTCPPacket                 = true,
            kIsRTPPacket                  = false
        };
    
        int64_t fLastQualityChange;
        int32_t fQualityInterval;

        //either pointers to the statically allocated sockets (maintained by the server)
        //or fresh ones (only fresh in extreme special cases)
        UDPSocketPair*          fSockets;
        RTPSessionInterface*    fSession;

        // info for kinda reliable UDP
        //DssDurationTimer      fInfoDisplayTimer;
        int32_t                  fBytesSentThisInterval;
        int32_t                  fDisplayCount;
        bool                  fSawFirstPacket;
        int64_t                  fStreamCumDuration;
        // manages UDP retransmits
        RTPPacketResender       fResender;
        RTPBandwidthTracker*    fTracker;

        
        //who am i sending to?
        uint32_t      fRemoteAddr;
        uint16_t      fRemoteRTPPort;
        uint16_t      fRemoteRTCPPort;
        uint16_t      fLocalRTPPort;
		uint32_t	    fMonitorAddr;
		int         fMonitorSocket;
		uint32_t      fPlayerToMonitorAddr;

        //RTCP stuff 
        int64_t      fLastSenderReportTime;
        uint32_t      fPacketCount;
        uint32_t      fLastPacketCount;
        uint32_t      fPacketCountInRTCPInterval;
        uint32_t      fByteCount;
        
        // DICTIONARY ATTRIBUTES
        
        //Module assigns a streamID to this object
        uint32_t      fTrackID;
        
        //low-level RTP stuff 
        uint32_t      fSsrc;
        char        fSsrcString[kMaxSsrcSizeInBytes];
        StrPtrLen   fSsrcStringPtr;
        bool      fEnableSSRC;
        
        //Payload name and codec type.
        char                fPayloadNameBuf[kDefaultPayloadBufSize];
        QTSS_RTPPayloadType fPayloadType;

        //Media information.
        uint16_t      fFirstSeqNumber;//used in sending the play response
        uint32_t      fFirstTimeStamp;//RTP time
        uint32_t      fTimescale;
        
        //what is the URL for this stream?
        std::string   fStreamURL;
        
        int32_t      fQualityLevel;
        uint32_t      fNumQualityLevels;
        
        uint32_t      fLastRTPTimestamp;
		int64_t		fLastNTPTimeStamp;
		uint32_t		fEstRTT;				//The estimated RTT calculated from RTCP's DLSR and LSR fields
        
        // RTCP data
        uint32_t      fFractionLostPackets;
        uint32_t      fTotalLostPackets;
        uint32_t      fJitter;
        uint32_t      fReceiverBitRate;
        uint16_t      fAvgLateMsec;
        uint16_t      fPercentPacketsLost;
        uint16_t      fAvgBufDelayMsec;
        uint16_t      fIsGettingBetter;
        uint16_t      fIsGettingWorse;
        uint32_t      fNumEyes;
        uint32_t      fNumEyesActive;
        uint32_t      fNumEyesPaused;
        uint32_t      fTotalPacketsRecv;
        uint32_t      fPriorTotalPacketsRecv;
        uint16_t      fTotalPacketsDropped;
        uint16_t      fTotalPacketsLost;
        uint32_t      fCurPacketsLostInRTCPInterval;
        uint16_t      fClientBufferFill;
        uint16_t      fFrameRate;
        uint16_t      fExpectedFrameRate;
        uint16_t      fAudioDryCount;
        uint32_t      fClientSSRC;
        
        bool      fIsTCP;
        QTSS_RTPTransportType   fTransportType;
        
        // HTTP params
        // Each stream has a set of thinning related tolerances,
        // that are dependent on prefs and parameters in the SETUP.
        // These params, as well as the current packet delay determine
        // whether a packet gets dropped.
        int32_t      fTurnThinningOffDelay_TCP;
        int32_t      fIncreaseThinningDelay_TCP;
        int32_t      fDropAllPacketsForThisStreamDelay_TCP;
        uint32_t      fStalePacketsDropped_TCP;
        int64_t      fTimeStreamCaughtUp_TCP;
        int64_t      fLastQualityLevelIncreaseTime_TCP;
        //
        // Each stream has a set of thinning related tolerances,
        // that are dependent on prefs and parameters in the SETUP.
        // These params, as well as the current packet delay determine
        // whether a packet gets dropped.
        int32_t      fThinAllTheWayDelay;
        int32_t      fAlwaysThinDelay;
        int32_t      fStartThinningDelay;
        int32_t      fStartThickingDelay;
        int32_t      fThickAllTheWayDelay;
        int32_t      fQualityCheckInterval;
        int32_t      fDropAllPacketsForThisStreamDelay;
        uint32_t      fStalePacketsDropped;
        int64_t      fLastCurrentPacketDelay;
        bool      fWaitOnLevelAdjustment;
        
        float     fBufferDelay; // from the sdp
        float     fLateToleranceInSec;
                
        // Pointer to the stream ref (this is just a this pointer)
        QTSS_StreamRef  fStreamRef;
        
        uint32_t      fCurrentAckTimeout;
        int32_t      fMaxSendAheadTimeMSec;
        
#if DEBUG
        uint32_t      fNumPacketsDroppedOnTCPFlowControl;
        int64_t      fFlowControlStartedMsec;
        int64_t      fFlowControlDurationMsec;
#endif
        
        // If we are interleaving RTP data over the TCP connection,
        // these are channel numbers to use for RTP & RTCP
        uint8_t   fRTPChannel;
        uint8_t   fRTCPChannel;
        
        QTSS_RTPNetworkMode     fNetworkMode;
        
        int64_t  fStreamStartTimeOSms;
                
        int32_t fLastQualityLevel;
        int32_t fLastRateLevel;
       
        bool fDisableThinning;
        int64_t fLastQualityUpdate;
        uint32_t fDefaultQualityLevel;
        int32_t fMaxQualityLevel;
		bool fInitialMaxQualityLevelIsSet;
		bool fUDPMonitorEnabled;
		uint16_t fMonitorVideoDestPort;
		uint16_t fMonitorAudioDestPort;
        
        //-----------------------------------------------------------
        // acutally write the data out that way
        QTSS_Error  InterleavedWrite(void* inBuffer, uint32_t inLen, uint32_t* outLenWritten, unsigned char channel );

        // implements the ReliableRTP protocol
        QTSS_Error  ReliableRTPWrite(void* inBuffer, uint32_t inLen, const int64_t& curPacketDelay);

         
        void        SetTCPThinningParams();
        QTSS_Error  TCPWrite(void* inBuffer, uint32_t inLen, uint32_t* outLenWritten, uint32_t inFlags);

        static QTSSAttrInfoDict::AttrInfo   sAttributes[];
        static StrPtrLen                    sChannelNums[];
        static QTSS_ModuleState             sRTCPProcessModuleState;

        static char *noType;
        static char *UDP;
        static char *RUDP;
        static char *TCP;
        
        bool UpdateQualityLevel(const int64_t& inTransmitTime, const int64_t& inCurrentPacketDelay,
                                        const int64_t& inCurrentTime, uint32_t inPacketSize);
        
        void            DisableThinning() { fDisableThinning = true; }
		void			SetInitialMaxQualityLevel();
        
        char *GetStreamTypeStr();
        enum { rtp = 0, rtcpSR = 1, rtcpRR = 2, rtcpACK = 3, rtcpAPP = 4 };
        float GetStreamStartTimeSecs() { return (float) ((OS::Milliseconds() - this->fSession->GetSessionCreateTime())/1000.0); }
        void PrintPacket(char *inBuffer, uint32_t inLen, int32_t inType); 
        void PrintRTP(char* packetBuff, uint32_t inLen);
        void PrintRTCPSenderReport(char* packetBuff, uint32_t inLen);
inline  void PrintPacketPrefEnabled(char *inBuffer,uint32_t inLen, int32_t inType) { if (QTSServerInterface::GetServer()->GetPrefs()->PacketHeaderPrintfsEnabled() ) this->PrintPacket(inBuffer,inLen, inType); }

        void SetOverBufferState(RTSPRequestInterface* request);
        
        bool TestRTCPPackets(StrPtrLen* inPacketPtr, uint32_t itemName);
        
        void UDPMonitorWrite(void* thePacketData, uint32_t inLen, bool isRTCP);


};

#endif // __RTPSTREAM_H__
