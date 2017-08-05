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
    File:       SourceInfo.h

    Contains:   This object contains an interface for getting at any bit
                of "interesting" information regarding a content source in a
                format - independent manner.
                
                For instance, the derived object SDPSourceInfo parses an
                SDP file and retrieves all the SourceInfo information from that file.
                    
    

*/

#ifndef __SOURCE_INFO_H__
#define __SOURCE_INFO_H__

#include "QTSS.h"
#include "StrPtrLen.h"
#include "OSQueue.h"
#include "OS.h"

class SourceInfo
{
    public:
    
        SourceInfo() {}
        SourceInfo(const SourceInfo& copy);// Does copy dynamically allocated data
        virtual ~SourceInfo(); // Deletes the dynamically allocated data
        
        enum
        {
            eDefaultBufferDelay = 3
        };
        
        // Returns whether this source is reflectable.
        bool  IsReflectable();

        // Each source is comprised of a set of streams. Those streams have
        // the following metadata.
        struct StreamInfo
        {
            StreamInfo() : fBufferDelay((float) eDefaultBufferDelay) {}
            ~StreamInfo(); // Deletes the memory allocated for the fPayloadName string 
            
            void Copy(const StreamInfo& copy);// Does copy dynamically allocated data
            
            uint32_t fSrcIPAddr{0};  // Src IP address of content (this may be 0 if not known for sure)
            uint32_t fDestIPAddr{0}; // Dest IP address of content (destination IP addr for source broadcast!)
            uint16_t fPort{0};       // Dest (RTP) port of source content
            uint16_t fTimeToLive{0}; // Ttl for this stream
            QTSS_RTPPayloadType fPayloadType{0};   // Payload type of this stream
            std::string fPayloadName; // Payload name of this stream
            uint32_t fTrackID{0};    // ID of this stream
			std::string fTrackName;//Track Name of this stream
            float fBufferDelay; // buffer delay (default is 3 seconds)
            bool  fIsTCP{false};     // Is this a TCP broadcast? If this is the case, the port and ttl are not valid
            bool  fSetupToReceive{false};    // If true then a push to the server is setup on this stream.
            uint32_t  fTimeScale{0};
        };
        
        // Returns the number of StreamInfo objects (number of Streams in this source)
        uint32_t      GetNumStreams() { return fNumStreams; }
        StreamInfo* GetStreamInfo(uint32_t inStreamIndex);
        StreamInfo* GetStreamInfoByTrackID(uint32_t inTrackID);
         
        // If this source is to be Relayed, it may have "Output" information. This
        // tells the reader where to forward the incoming streams onto. There may be
        // 0 -> N OutputInfo objects in this SourceInfo. Each OutputInfo refers to a
        // single, complete copy of ALL the input streams. The fPortArray field
        // contains one RTP port for each incoming stream.
        struct OutputInfo
        {
            OutputInfo() {}
            ~OutputInfo(); // Deletes the memory allocated for fPortArray
            
            // Returns true if the two are equal
            bool Equal(const OutputInfo& info);
            
            void Copy(const OutputInfo& copy);// Does copy dynamically allocated data

            uint32_t fDestAddr{0};       // Destination address to forward the input onto
            uint32_t fLocalAddr{0};      // Address of local interface to send out on (may be 0)
            uint16_t fTimeToLive{0};     // Time to live for resulting output (if multicast)
            uint16_t* fPortArray{nullptr};     // 1 destination RTP port for each Stream.
            uint32_t fNumPorts{0};       // Size of the fPortArray (usually equal to fNumStreams)
            uint16_t fBasePort{0};       // The base destination RTP port - for i=1 to fNumStreams fPortArray[i] = fPortArray[i-1] + 2
            bool  fAlreadySetup{false};  // A flag used in QTSSReflectorModule.cpp
        };

        // Returns the number of OutputInfo objects.
        uint32_t      GetNumOutputs() { return fNumOutputs; }
        uint32_t      GetNumNewOutputs(); // Returns # of outputs not already setup

        OutputInfo* GetOutputInfo(uint32_t inOutputIndex);
        
        // GetLocalSDP. This may or may not be supported by sources. Typically, if
        // the source is reflectable, this must be supported. It returns a newly
        // allocated buffer (that the caller is responsible for) containing an SDP
        // description of the source, stripped of all network info.
		virtual std::string GetLocalSDP() { return {}; }
        
        // This is only supported by the RTSPSourceInfo sub class
        virtual bool IsRTSPSourceInfo() { return false; }
        
                // This is only supported by the RCFSourceInfo sub class and its derived classes
                virtual char*   Name()  { return nullptr; }
                
        virtual bool Equal(SourceInfo* inInfo);
        
        // SDP scheduled times supports earliest start and latest end -- doesn't handle repeat times or multiple active times.
        #define kNTP_Offset_From_1970 2208988800LU
        time_t  NTPSecs_to_UnixSecs(time_t time) {return (time_t) (time - (uint32_t)kNTP_Offset_From_1970);}
        uint32_t  UnixSecs_to_NTPSecs(time_t time) {return (uint32_t) (time + (uint32_t)kNTP_Offset_From_1970);}
        bool  SetActiveNTPTimes(uint32_t startNTPTime,uint32_t endNTPTime);
        bool  IsValidNTPSecs(uint32_t time) {return time >= (uint32_t) kNTP_Offset_From_1970 ? true : false;}
        bool  IsPermanentSource() { return ((fStartTimeUnixSecs == 0) && (fEndTimeUnixSecs == 0)) ? true : false; }
        bool  IsActiveTime(time_t unixTimeSecs);
        bool  IsActiveNow() { return IsActiveTime(OS::UnixTime_Secs()); }
        bool  IsRTSPControlled() {return (fSessionControlType == kRTSPSessionControl) ? true : false; }
        bool  HasTCPStreams();
        bool  HasIncomingBroacast();
        time_t  GetStartTimeUnixSecs() {return fStartTimeUnixSecs; }
        time_t  GetEndTimeUnixSecs() {return fEndTimeUnixSecs; }
        uint32_t  GetDurationSecs();
        enum {kSDPTimeControl, kRTSPSessionControl};
    protected:
        
        //utility function used by IsReflectable
        bool IsReflectableIPAddr(uint32_t inIPAddr);

        StreamInfo* fStreamArray{nullptr};
        uint32_t      fNumStreams{0};
        
        OutputInfo* fOutputArray{nullptr};
        uint32_t      fNumOutputs{0};
        
        bool      fTimeSet{false};
        time_t      fStartTimeUnixSecs{0};
        time_t      fEndTimeUnixSecs{0};
            
        uint32_t      fSessionControlType{kRTSPSessionControl};
        bool      fHasValidTime;
};



#endif //__SOURCE_INFO_H__
