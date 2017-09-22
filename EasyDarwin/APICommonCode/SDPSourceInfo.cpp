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
    File:       SDPSourceInfo.cpp

    Contains:   Implementation of object defined in .h file


*/

#include <boost/spirit/include/qi.hpp>
#include <fmt/format.h>

#include "SDPSourceInfo.h"

#include "SocketUtils.h"
#include "SDPUtils.h"

namespace qi = boost::spirit::qi;

static std::string    sCLine("c=IN IP4 0.0.0.0");
static std::string    sControlLine("a=control:*");
static std::string    sVideoStr("video");
static std::string    sAudioStr("audio");
static std::string    sRtpMapStr("rtpmap");
static std::string    sControlStr("control");
static std::string    sBufferDelayStr("x-bufferdelay");
static std::string    sBroadcastControlStr("x-broadcastcontrol");
static std::string    sAutoDisconnect("RTSP");
static std::string    sAutoDisconnectTime("TIME");

template <typename S>
static std::vector<std::string> spirit_direct(const S& input, char const* delimiter)
{
	std::vector<std::string> result;
	if (!qi::parse(input.begin(), input.end(),
		qi::raw[*(qi::char_ - qi::char_(delimiter))] % qi::char_(delimiter), result))
		result.push_back(std::string(input));
	return result;
}

SDPSourceInfo::~SDPSourceInfo()
{
    // Not reqd as the destructor of the 
    // base class will take care of delete the stream array
    // and output array if allocated
    /* 
    if (fStreamArray != NULL)
    {
        char* theCharArray = (char*)fStreamArray;
        delete [] theCharArray;
    }
    */
}

bool  SDPSourceInfo::IsReflectable()
{
	if (fStreamArray.empty())
		return false;

	//each stream's info must meet certain criteria
	for (const auto & stream : fStreamArray)
	{
		if (stream.fIsTCP)
			continue;

		if (stream.fTimeToLive == 0)
			return false;
	}
	return true;
}

StreamInfo* SDPSourceInfo::GetStreamInfo(uint32_t inIndex)
{
	if (inIndex < fStreamArray.size())
		return &fStreamArray[inIndex];
	else
		return nullptr;
}

const StreamInfo* SDPSourceInfo::GetStreamInfoByTrackID(uint32_t inTrackID) const
{
	if (fStreamArray.empty())
		return nullptr;
	for (const auto &stream : fStreamArray)
		if (stream.fTrackID == inTrackID)
			return &stream;
	return nullptr;
}

std::string SDPSourceInfo::GetLocalSDP()
{
    Assert(!fSDPData.empty());

    bool appendCLine = true;
    uint32_t trackIndex = 0;
    
    std::string localSDP;
    
	std::vector<std::string> sdpLines = spirit_direct(fSDPData, "\r\n");
    // Only generate our own trackIDs if this file doesn't have 'em.
    // Our assumption here is that either the file has them, or it doesn't.
    // A file with some trackIDs, and some not, won't work.
    bool hasControlLine = false;

    for (const auto sdpLine : sdpLines)
    {
        //stop when we reach an empty line.
		if (sdpLine.empty()) continue;
            
        switch (sdpLine[0])
        {
            case 'c':
                break;//ignore connection information
            case 'm':
            {
                //append new connection information right before the first 'm'
                if (appendCLine)
                {
					localSDP += sCLine + "\r\n";
                    if (!hasControlLine)
						localSDP += sControlLine + "\r\n";                    
                    appendCLine = false;
                }
                //the last "a=" for each m should be the control a=
                if ((trackIndex > 0) && (!hasControlLine))
					localSDP += fmt::format("a=control:trackID={}\r\n", trackIndex);

                //now write the 'm' line, but strip off the port information
				std::string mPrefix, mSuffix;
				bool r = qi::phrase_parse(sdpLine.cbegin(), sdpLine.cend(),
					+(qi::char_ - qi::digit) >> qi::omit[qi::ushort_] >> +(qi::char_),
					qi::eoi, mPrefix, mSuffix);
                localSDP += mPrefix + "0" + mSuffix + "\r\n";
                trackIndex++;
                break;
            }
            case 'a':
            {
				std::string aLineType, rest;
				bool r = qi::phrase_parse(sdpLine.cbegin(), sdpLine.cend(),
					qi::no_case["a="] >> +(qi::alpha) >> ":" >> +(qi::char_),
					qi::eoi, aLineType, rest);
                if (aLineType == sControlStr)
                {
					uint32_t trackID;
					r = qi::phrase_parse(rest.cbegin(), rest.cend(),
						qi::omit[+(qi::alpha)] >> "=" >> qi::uint_,
						qi::ascii::blank, trackID);

					if (r)
                    {
						localSDP += std::string("a=control:trackID=")
							      + std::to_string(trackID) + "\r\n";
						hasControlLine = true;
						break;
                    }
                }
               
				localSDP += sdpLine + "\r\n";
                break;
            }
            default:
            {
				localSDP += sdpLine + "\r\n";
            }
        }
    }
    
    if ((trackIndex > 0) && (!hasControlLine))
		localSDP += fmt::format("a=control:trackID={}\r\n",trackIndex);
    
    SDPContainer rawSDPContainer(localSDP); 

    return SortSDPLine(rawSDPContainer); // return a new copy of the sorted SDP
}

void SDPSourceInfo::Parse(boost::string_view sdpData)
{
    //
    // There are some situations in which Parse can be called twice.
    // If that happens, just return and don't do anything the second time.
    if (!fSDPData.empty())
        return;
        
    Assert(fStreamArray.empty());
    
    fSDPData = std::string(sdpData);

    // If there is no trackID information in this SDP, we make the track IDs start
    // at 1 -> N
    uint32_t currentTrack = 1;
    
    bool hasGlobalStreamInfo = false;
    StreamInfo theGlobalStreamInfo; //needed if there is one c= header independent of
                                    //individual streams

	std::vector<std::string> sdpLines = spirit_direct(fSDPData, "\r\n");

    // set the default destination as our default IP address and set the default ttl
    theGlobalStreamInfo.fDestIPAddr = INADDR_ANY;
    theGlobalStreamInfo.fTimeToLive = kDefaultTTL;
        
    //Set bufferdelay to default of 3
    theGlobalStreamInfo.fBufferDelay = (float) eDefaultBufferDelay;
    
    //Now actually get all the data on all the streams
	for (const auto &sdpLine : sdpLines)
    {
        if (sdpLine.empty())
            continue;//skip over any blank lines

        switch (sdpLine[0])
        {
            case 't':
            {
				uint32_t ntpStart, ntpEnd;
				bool r = qi::phrase_parse(sdpLine.cbegin(), sdpLine.cend(),
					qi::no_case["t="] >> qi::uint_ >> qi::uint_,
					qi::ascii::blank, ntpStart, ntpEnd);
                
                SetActiveNTPTimes(ntpStart,ntpEnd);
            }
            break;
            
            case 'm':
            {
				StreamInfo newInfo;
                if (hasGlobalStreamInfo)
                {
                    newInfo.fDestIPAddr = theGlobalStreamInfo.fDestIPAddr;
                    newInfo.fTimeToLive = theGlobalStreamInfo.fTimeToLive;
                }
                newInfo.fTrackID = currentTrack;
                currentTrack++;
                
				std::string theStreamType, transportID;
				uint16_t tempPort;
				bool r = qi::phrase_parse(sdpLine.cbegin(), sdpLine.cend(),
					qi::no_case["m="] >> +(qi::alpha) >> qi::ushort_ >> +(qi::char_),
					qi::ascii::blank, theStreamType, tempPort, transportID);
                
                if (theStreamType == sVideoStr)
                    newInfo.fPayloadType = qtssVideoPayloadType;
                else if (theStreamType == sAudioStr)
                    newInfo.fPayloadType = qtssAudioPayloadType;

                if ((tempPort > 0) && (tempPort < 65536))
                    newInfo.fPort = tempPort;

                static const std::string kTCPTransportStr("RTP/AVP/TCP");
                if (transportID == kTCPTransportStr)
                    newInfo.fIsTCP = true;
                    
				fStreamArray.push_back(std::move(newInfo));
            }
            break;
            case 'a':
            {
				std::string aLineType, rest;
				bool r = qi::phrase_parse(sdpLine.cbegin(), sdpLine.cend(),
					qi::no_case["a="] >> +(qi::alpha) >> ":" >> +(qi::char_),
					qi::eoi, aLineType, rest);

                if (aLineType == sBroadcastControlStr)
                {   
					// found a control line for the broadcast (delete at time or delete at end of broadcast/server startup) 
                    // printf("found =%s\n",sBroadcastControlStr);
                    if (rest == sAutoDisconnect)
                    {
                       fSessionControlType = kRTSPSessionControl; 
                    }  
                    else if (rest == sAutoDisconnectTime)
                    {
                       fSessionControlType = kSDPTimeControl; 
                    }
                }

                //if we haven't even hit an 'm' line yet, just ignore all 'a' lines
                if (fStreamArray.empty())
                    break;
                    
                if (aLineType == sRtpMapStr)
                {
					std::string codecName;
					r = qi::phrase_parse(rest.cbegin(), rest.cend(),
						qi::omit[qi::uint_] >> +(qi::char_),
						qi::ascii::blank, codecName);
                    //mark the codec type if this line has a codec name on it. If we already
                    //have a codec type for this track, just ignore this line
					if (fStreamArray.back().fPayloadName.empty())
						fStreamArray.back().fPayloadName = std::move(codecName);
                }
                else if (aLineType == sControlStr)
                {
					uint32_t trackID;
					r = qi::phrase_parse(rest.cbegin(), rest.cend(),
						qi::omit[+(qi::alpha)] >> "=" >> qi::uint_,
						qi::ascii::blank, trackID);

					fStreamArray.back().fTrackName = rest;
					fStreamArray.back().fTrackID = trackID;
                }
                else if (aLineType == sBufferDelayStr)
                {
					// if a BufferDelay is found then set all of the streams to the same buffer delay (it's global)
					float delay;
					r = qi::phrase_parse(rest.cbegin(), rest.cend(),
						qi::omit[+(qi::alpha)] >> "=" >> qi::float_,
						qi::ascii::blank, delay);
					theGlobalStreamInfo.fBufferDelay = delay;
                }
            }
            break;
            case 'c':
            {
				std::string IP;
				uint16_t tempTtl = kDefaultTTL;
				bool r = qi::phrase_parse(sdpLine.cbegin(), sdpLine.cend(),
					qi::no_case["c=in ip4"] >> *(qi::char_ - "/") >> -("/" >> qi::ushort_),
					qi::ascii::blank, IP, tempTtl);

                uint32_t tempIPAddr = SocketUtils::ConvertStringToAddr(IP.c_str());
 
                if (!fStreamArray.empty())
                {
                    //if this c= line is part of a stream, it overrides the
                    //global stream information
                    fStreamArray.back().fDestIPAddr = tempIPAddr;
                    fStreamArray.back().fTimeToLive = (uint16_t) tempTtl;
                } else {
                    theGlobalStreamInfo.fDestIPAddr = tempIPAddr;
                    theGlobalStreamInfo.fTimeToLive = (uint16_t) tempTtl;
                    hasGlobalStreamInfo = true;
                }
            }
        }
    }       
    
    // Add the default buffer delay
    auto bufferDelay = (float) eDefaultBufferDelay;
    if (theGlobalStreamInfo.fBufferDelay != (float) eDefaultBufferDelay)
        bufferDelay = theGlobalStreamInfo.fBufferDelay;
    
    for (auto &stream : fStreamArray)
		stream.fBufferDelay = bufferDelay;
}

bool  SDPSourceInfo::SetActiveNTPTimes(uint32_t startTimeNTP, uint32_t endTimeNTP)
{   // right now only handles earliest start and latest end time.

	//printf("SourceInfo::SetActiveNTPTimes start=%"   _U32BITARG_   " end=%"   _U32BITARG_   "\n",startTimeNTP,endTimeNTP);
	bool accepted = false;
	do
	{
		if ((startTimeNTP > 0) && (endTimeNTP > 0) && (endTimeNTP < startTimeNTP)) break; // not valid NTP time

		uint32_t startTimeUnixSecs = 0;
		uint32_t endTimeUnixSecs = 0;

		if (startTimeNTP != 0 && IsValidNTPSecs(startTimeNTP)) // allow anything less than 1970 
			startTimeUnixSecs = NTPSecs_to_UnixSecs(startTimeNTP);// convert to 1970 time

		if (endTimeNTP != 0 && !IsValidNTPSecs(endTimeNTP)) // don't allow anything less than 1970
			break;

		if (endTimeNTP != 0) // convert to 1970 time
			endTimeUnixSecs = NTPSecs_to_UnixSecs(endTimeNTP);

		fStartTimeUnixSecs = startTimeUnixSecs;
		fEndTimeUnixSecs = endTimeUnixSecs;
		accepted = true;

	} while (0);

	//char buffer[kTimeStrSize];
	//printf("SourceInfo::SetActiveNTPTimes fStartTimeUnixSecs=%"   _U32BITARG_   " fEndTimeUnixSecs=%"   _U32BITARG_   "\n",fStartTimeUnixSecs,fEndTimeUnixSecs);
	//printf("SourceInfo::SetActiveNTPTimes start time = %s",std::ctime(&fStartTimeUnixSecs) );
	//printf("SourceInfo::SetActiveNTPTimes end time = %s",std::ctime(&fEndTimeUnixSecs) );
	fHasValidTime = accepted;
	return accepted;
}
