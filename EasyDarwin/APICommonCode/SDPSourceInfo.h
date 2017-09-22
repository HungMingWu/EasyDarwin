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
	 File:       SDPSourceInfo.h

	 Contains:   This object takes input SDP data, and uses it to support the SourceInfo
				 API.

	 Works only for QTSS

 */

#ifndef __SDP_SOURCE_INFO_H__
#define __SDP_SOURCE_INFO_H__

#include <vector>
#include <boost/utility/string_view.hpp>
#include "QTSS.h"

constexpr static float eDefaultBufferDelay = 3.0;

 // Each source is comprised of a set of streams. Those streams have
 // the following metadata.
struct StreamInfo
{
	StreamInfo() = default;
	~StreamInfo() = default; // Deletes the memory allocated for the fPayloadName string             
	uint32_t fSrcIPAddr{ 0 };  // Src IP address of content (this may be 0 if not known for sure)
	uint32_t fDestIPAddr{ 0 }; // Dest IP address of content (destination IP addr for source broadcast!)
	uint16_t fPort{ 0 };       // Dest (RTP) port of source content
	uint16_t fTimeToLive{ 0 }; // Ttl for this stream
	QTSS_RTPPayloadType fPayloadType{ 0 };   // Payload type of this stream
	std::string fPayloadName; // Payload name of this stream
	uint32_t fTrackID{ 0 };    // ID of this stream
	std::string fTrackName;//Track Name of this stream
	float fBufferDelay = eDefaultBufferDelay; // buffer delay (default is 3 seconds)
	bool  fIsTCP{ false };     // Is this a TCP broadcast? If this is the case, the port and ttl are not valid
	bool  fSetupToReceive{ false };    // If true then a push to the server is setup on this stream.
	uint32_t  fTimeScale{ 0 };
};

class SDPSourceInfo
{
public:

	// Uses the SDP Data to build up the StreamInfo structures
	SDPSourceInfo(boost::string_view sdpData) { Parse(sdpData); }
	SDPSourceInfo() = default;
	~SDPSourceInfo();

	// Returns the number of StreamInfo objects (number of Streams in this source)
	size_t GetNumStreams() const { return fStreamArray.size(); }
	StreamInfo* GetStreamInfo(uint32_t inStreamIndex);
	const StreamInfo* GetStreamInfoByTrackID(uint32_t inTrackID) const;

	// Returns whether this source is reflectable.
	bool  IsReflectable();

	// Parses out the SDP file provided, sets up the StreamInfo structures
	void    Parse(boost::string_view sdpData);

	// This function uses the Parsed SDP file, and strips out all the network information,
	// producing an SDP file that appears to be local.
	std::string  GetLocalSDP();

	// Returns the SDP data
	boost::string_view  GetSDPData() { return fSDPData; }

private:
	
	std::vector<StreamInfo> fStreamArray;
	enum
	{
		kDefaultTTL = 15    //UInt16
	};
	std::string   fSDPData;

	bool      fTimeSet{ false };
	time_t      fStartTimeUnixSecs{ 0 };
	time_t      fEndTimeUnixSecs{ 0 };

	uint32_t      fSessionControlType{ kRTSPSessionControl };
	bool      fHasValidTime;

	// SDP scheduled times supports earliest start and latest end -- doesn't handle repeat times or multiple active times.
#define kNTP_Offset_From_1970 2208988800LU
	time_t  NTPSecs_to_UnixSecs(time_t time) { return (time_t)(time - (uint32_t)kNTP_Offset_From_1970); }
	bool  SetActiveNTPTimes(uint32_t startNTPTime, uint32_t endNTPTime);
	bool  IsValidNTPSecs(uint32_t time) { return time >= (uint32_t)kNTP_Offset_From_1970 ? true : false; }
	time_t  GetStartTimeUnixSecs() { return fStartTimeUnixSecs; }
	time_t  GetEndTimeUnixSecs() { return fEndTimeUnixSecs; }
	enum { kSDPTimeControl, kRTSPSessionControl };
};
#endif

