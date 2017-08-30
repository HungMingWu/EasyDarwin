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
    File:       SourceInfo.cpp

    Contains:   Implements object defined in .h file.
                    

*/

#include "SourceInfo.h"
#include "SocketUtils.h"

SourceInfo::SourceInfo(const SourceInfo& copy)
:   fStreamArray(nullptr), fNumStreams(copy.fNumStreams), 
    fOutputArray(nullptr), fNumOutputs(copy.fNumOutputs),
    fTimeSet(copy.fTimeSet),fStartTimeUnixSecs(copy.fStartTimeUnixSecs),
    fEndTimeUnixSecs(copy.fEndTimeUnixSecs), fSessionControlType(copy.fSessionControlType),
    fHasValidTime(false)
{   
    
    if(copy.fStreamArray != nullptr && fNumStreams != 0)
    {
        fStreamArray = new StreamInfo[fNumStreams];
        for (uint32_t index=0; index < fNumStreams; index++)
            fStreamArray[index].Copy(copy.fStreamArray[index]);
    }
    
    if(copy.fOutputArray != nullptr && fNumOutputs != 0)
    {
        fOutputArray = new OutputInfo[fNumOutputs];
        for (uint32_t index2=0; index2 < fNumOutputs; index2++)
            fOutputArray[index2].Copy(copy.fOutputArray[index2]);
    }
    
}

SourceInfo::~SourceInfo()
{
    if(fStreamArray != nullptr)
        delete [] fStreamArray;

    if(fOutputArray != nullptr)
        delete [] fOutputArray;
        
}

bool  SourceInfo::IsReflectable()
{
    if (fStreamArray == nullptr)
        return false;
    if (fNumStreams == 0)
        return false;
        
    //each stream's info must meet certain criteria
    for (uint32_t x = 0; x < fNumStreams; x++)
    {
        if (fStreamArray[x].fIsTCP)
            continue;
            
        if ((!this->IsReflectableIPAddr(fStreamArray[x].fDestIPAddr)) ||
            (fStreamArray[x].fTimeToLive == 0))
            return false;
    }
    return true;
}

bool  SourceInfo::IsReflectableIPAddr(uint32_t inIPAddr)
{
	//fix ffmpeg push rtsp stream setup error
	return true;
    if (SocketUtils::IsMulticastIPAddr(inIPAddr) || SocketUtils::IsLocalIPAddr(inIPAddr))
        return true;
    return false;
}

bool  SourceInfo::HasTCPStreams()
{   
    //each stream's info must meet certain criteria
    for (uint32_t x = 0; x < fNumStreams; x++)
    {
        if (fStreamArray[x].fIsTCP)
            return true;
    }
    return false;
}

bool  SourceInfo::HasIncomingBroacast()
{   
    //each stream's info must meet certain criteria
    for (uint32_t x = 0; x < fNumStreams; x++)
    {
        if (fStreamArray[x].fSetupToReceive)
            return true;
    }
    return false;
}
SourceInfo::StreamInfo* SourceInfo::GetStreamInfo(uint32_t inIndex)
{
    Assert(inIndex < fNumStreams);
    if (fStreamArray == nullptr)
        return nullptr;
    if (inIndex < fNumStreams)
        return &fStreamArray[inIndex];
    else
        return nullptr;
}

SourceInfo::StreamInfo* SourceInfo::GetStreamInfoByTrackID(uint32_t inTrackID)
{
    if (fStreamArray == nullptr)
        return nullptr;
    for (uint32_t x = 0; x < fNumStreams; x++)
    {
        if (fStreamArray[x].fTrackID == inTrackID)
            return &fStreamArray[x];
    }
    return nullptr;
}

SourceInfo::OutputInfo* SourceInfo::GetOutputInfo(uint32_t inIndex)
{
    Assert(inIndex < fNumOutputs);
    if (fOutputArray == nullptr)
        return nullptr;
    if (inIndex < fNumOutputs)
        return &fOutputArray[inIndex];
    else
        return nullptr;
}

uint32_t SourceInfo::GetNumNewOutputs()
{
    uint32_t theNumNewOutputs = 0;
    for (uint32_t x = 0; x < fNumOutputs; x++)
    {
        if (!fOutputArray[x].fAlreadySetup)
            theNumNewOutputs++;
    }
    return theNumNewOutputs;
}

bool  SourceInfo::SetActiveNTPTimes(uint32_t startTimeNTP,uint32_t endTimeNTP)
{   // right now only handles earliest start and latest end time.

    //printf("SourceInfo::SetActiveNTPTimes start=%"   _U32BITARG_   " end=%"   _U32BITARG_   "\n",startTimeNTP,endTimeNTP);
    bool accepted = false;
    do 
    {
        if ((startTimeNTP > 0) && (endTimeNTP > 0) && (endTimeNTP < startTimeNTP)) break; // not valid NTP time
        
        uint32_t startTimeUnixSecs = 0; 
        uint32_t endTimeUnixSecs  = 0; 
        
        if (startTimeNTP != 0 && IsValidNTPSecs(startTimeNTP)) // allow anything less than 1970 
            startTimeUnixSecs = NTPSecs_to_UnixSecs(startTimeNTP);// convert to 1970 time
        
        if (endTimeNTP != 0 && !IsValidNTPSecs(endTimeNTP)) // don't allow anything less than 1970
            break;
            
        if (endTimeNTP != 0) // convert to 1970 time
            endTimeUnixSecs = NTPSecs_to_UnixSecs(endTimeNTP);

        fStartTimeUnixSecs = startTimeUnixSecs;
        fEndTimeUnixSecs = endTimeUnixSecs; 
        accepted = true;
        
    }  while(0);
    
    //char buffer[kTimeStrSize];
    //printf("SourceInfo::SetActiveNTPTimes fStartTimeUnixSecs=%"   _U32BITARG_   " fEndTimeUnixSecs=%"   _U32BITARG_   "\n",fStartTimeUnixSecs,fEndTimeUnixSecs);
    //printf("SourceInfo::SetActiveNTPTimes start time = %s",std::ctime(&fStartTimeUnixSecs) );
    //printf("SourceInfo::SetActiveNTPTimes end time = %s",std::ctime(&fEndTimeUnixSecs) );
    fHasValidTime = accepted;
    return accepted;
}

bool SourceInfo::Equal(SourceInfo* inInfo)
{
    // Check to make sure the # of streams matches up
    if (this->GetNumStreams() != inInfo->GetNumStreams())
        return false;
    
    // Check the src & dest addr, and port of each stream. 
    for (uint32_t x = 0; x < this->GetNumStreams(); x++)
    {
        if (GetStreamInfo(x)->fDestIPAddr != inInfo->GetStreamInfo(x)->fDestIPAddr)
            return false;
        if (GetStreamInfo(x)->fSrcIPAddr != inInfo->GetStreamInfo(x)->fSrcIPAddr)
            return false;
        
        // If either one of the comparators is 0 (the "wildcard" port), then we know at this point
        // they are equivalent
        if ((GetStreamInfo(x)->fPort == 0) || (inInfo->GetStreamInfo(x)->fPort == 0))
            return true;
            
        // Neither one is the wildcard port, so they must be the same
        if (GetStreamInfo(x)->fPort != inInfo->GetStreamInfo(x)->fPort)
            return false;
    }
    return true;
}

void SourceInfo::StreamInfo::Copy(const StreamInfo& copy)
{
    fSrcIPAddr = copy.fSrcIPAddr;
    fDestIPAddr = copy.fDestIPAddr;
    fPort = copy.fPort;
    fTimeToLive = copy.fTimeToLive;
    fPayloadType = copy.fPayloadType;
    fPayloadName = copy.fPayloadName;
    fTrackID = copy.fTrackID;
	fTrackName = copy.fTrackName;
    fBufferDelay = copy.fBufferDelay;
    fIsTCP = copy.fIsTCP;
    fSetupToReceive = copy.fSetupToReceive;
    fTimeScale = copy.fTimeScale;    
}

SourceInfo::StreamInfo::~StreamInfo()
{
}

void SourceInfo::OutputInfo::Copy(const OutputInfo& copy)
{
    fDestAddr = copy.fDestAddr;
    fLocalAddr = copy.fLocalAddr;
    fTimeToLive = copy.fTimeToLive;
    fNumPorts = copy.fNumPorts;
    if(fNumPorts != 0)
    {
        fPortArray = new uint16_t[fNumPorts];
        ::memcpy(fPortArray, copy.fPortArray, fNumPorts * sizeof(uint16_t));
    }
    fBasePort = copy.fBasePort;
    fAlreadySetup = copy.fAlreadySetup;
}

SourceInfo::OutputInfo::~OutputInfo()
{
    if (fPortArray != nullptr)
        delete [] fPortArray;
}

bool SourceInfo::OutputInfo::Equal(const OutputInfo& info)
{
    if ((fDestAddr == info.fDestAddr) && (fLocalAddr == info.fLocalAddr) && (fTimeToLive == info.fTimeToLive))
    {
        if ((fBasePort != 0) && (fBasePort == info.fBasePort))
            return true;
        else if ((fNumPorts == 0) || ((fNumPorts == info.fNumPorts) && (fPortArray[0] == info.fPortArray[0])))
            return true;
    }
    return false;
}


