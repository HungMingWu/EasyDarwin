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
		RTPSessionInterface &GetSession()		{ return *fSession; }
        
        // Setup uses the info in the RTSPRequestInterface to associate
        // all the necessary resources, ports, sockets, etc, etc, with this
        // stream.
        QTSS_Error Setup(RTSPRequest* request, QTSS_AddStreamFlags inFlags);
        
        // Write sends RTP data to the client. Caller must specify
        // either qtssWriteFlagsIsRTP or qtssWriteFlagsIsRTCP
        QTSS_Error  Write(const std::vector<char> &thePacket,
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

		void EnableSSRC() { fEnableSSRC = true; }
		void DisableSSRC() { fEnableSSRC = false; }

		bool isTCP() const { return fIsTCP; }
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

        //either pointers to the statically allocated sockets (maintained by the server)
        //or fresh ones (only fresh in extreme special cases)
		UDPSocketPair*          fSockets{ nullptr };
        RTPSessionInterface*    fSession;

        //low-level RTP stuff 
        uint32_t      fSsrc;
		bool      fEnableSSRC{ false };

		Attributes attr;
        
        //what is the URL for this stream?
        std::string   fStreamURL;

		bool      fIsTCP{ false };
		QTSS_RTPTransportType   fTransportType{ qtssRTPTransportTypeTCP };

        // If we are interleaving RTP data over the TCP connection,
        // these are channel numbers to use for RTP & RTCP
		uint8_t   fRTPChannel{ 0 };
		uint8_t   fRTCPChannel{ 0 };
        
		QTSS_RTPNetworkMode     fNetworkMode{ qtssRTPNetworkModeDefault };

        //-----------------------------------------------------------
        // acutally write the data out that way
        QTSS_Error  InterleavedWrite(const std::vector<char> &inBuffer, uint32_t* outLenWritten, unsigned char channel );

        enum { rtp = 0, rtcpSR = 1, rtcpRR = 2, rtcpACK = 3, rtcpAPP = 4 };
};

#endif // __RTPSTREAM_H__
