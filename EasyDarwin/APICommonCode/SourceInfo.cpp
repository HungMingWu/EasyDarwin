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
:   fStreamArray(NULL), fNumStreams(copy.fNumStreams), 
    fOutputArray(NULL), fNumOutputs(copy.fNumOutputs),
    fTimeSet(copy.fTimeSet),fStartTimeUnixSecs(copy.fStartTimeUnixSecs),
    fEndTimeUnixSecs(copy.fEndTimeUnixSecs), fSessionControlType(copy.fSessionControlType),
    fHasValidTime(false)
{   
    
    if(copy.fStreamArray != NULL && fNumStreams != 0)
    {
        fStreamArray = new StreamInfo[fNumStreams];
        for (uint32_t index=0; index < fNumStreams; index++)
            fStreamArray[index].Copy(copy.fStreamArray[index]);
    }
    
    if(copy.fOutputArray != NULL && fNumOutputs != 0)
    {
        fOutputArray = new OutputInfo[fNumOutputs];
        for (uint32_t index2=0; index2 < fNumOutputs; index2++)
            fOutputArray[index2].Copy(copy.fOutputArray[index2]);
    }
    
}

SourceInfo::~SourceInfo()
{
    if(fStreamArray != NULL)
        delete [] fStreamArray;

    if(fOutputArray != NULL)
        delete [] fOutputArray;
        
}

bool  SourceInfo::IsReflectable()
{
    if (fStreamArray == NULL)
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
    if (fStreamArray == NULL)
        return NULL;
    if (inIndex < fNumStreams)
        return &fStreamArray[inIndex];
    else
        return NULL;
}

SourceInfo::StreamInfo* SourceInfo::GetStreamInfoByTrackID(uint32_t inTrackID)
{
    if (fStreamArray == NULL)
        return NULL;
    for (uint32_t x = 0; x < fNumStreams; x++)
    {
        if (fStreamArray[x].fTrackID == inTrackID)
            return &fStreamArray[x];
    }
    return NULL;
}

SourceInfo::OutputInfo* SourceInfo::GetOutputInfo(uint32_t inIndex)
{
    Assert(inIndex < fNumOutputs);
    if (fOutputArray == NULL)
        return NULL;
    if (inIndex < fNumOutputs)
        return &fOutputArray[inIndex];
    else
        return NULL;
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
    //printf("SourceInfo::SetActiveNTPTimes start time = %s",qtss_ctime(&fStartTimeUnixSecs, buffer, sizeof(buffer)) );
    //printf("SourceInfo::SetActiveNTPTimes end time = %s",qtss_ctime(&fEndTimeUnixSecs, buffer, sizeof(buffer)) );
    fHasValidTime = accepted;
    return accepted;
}

bool  SourceInfo::IsActiveTime(time_t unixTimeSecs)
{ 
    // order of tests are important here
    // we do it this way because of the special case time value of 0 for end time
    // start - 0 = unbounded 
    // 0 - 0 = permanent
    if (false == fHasValidTime)
        return false;
        
    if (unixTimeSecs < 0) //check valid value
        return false;
        
    if (IsPermanentSource()) //check for 0 0
        return true;
    
    if (unixTimeSecs < fStartTimeUnixSecs)
        return false; //too early

    if (fEndTimeUnixSecs == 0)  
        return true;// accept any time after start

    if (unixTimeSecs > fEndTimeUnixSecs)
        return false; // too late

    return true; // ok start <= time <= end

}


uint32_t SourceInfo::GetDurationSecs() 
{    
    
    if (fEndTimeUnixSecs == 0) // unbounded time
        return (uint32_t) ~0; // max time
    
    time_t timeNow = OS::UnixTime_Secs();
    if (fEndTimeUnixSecs <= timeNow) // the active time has past or duration is 0 so return the minimum duration
        return (uint32_t) 0; 
            
    if (fStartTimeUnixSecs == 0) // relative duration = from "now" to end time
        return fEndTimeUnixSecs - timeNow;
    
    return fEndTimeUnixSecs - fStartTimeUnixSecs; // this must be a duration because of test for endtime above

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
    if ((copy.fPayloadName).Ptr != NULL)
        fPayloadName.Set((copy.fPayloadName).GetAsCString(), (copy.fPayloadName).Len);
    fTrackID = copy.fTrackID;
	if ((copy.fTrackName).Ptr != NULL)
		fTrackName.Set((copy.fTrackName).GetAsCString(), (copy.fTrackName).Len);
    fBufferDelay = copy.fBufferDelay;
    fIsTCP = copy.fIsTCP;
    fSetupToReceive = copy.fSetupToReceive;
    fTimeScale = copy.fTimeScale;    
}

SourceInfo::StreamInfo::~StreamInfo()
{
    if (fPayloadName.Ptr != NULL)
        delete fPayloadName.Ptr;
    fPayloadName.Len = 0;

	if (fTrackName.Ptr != NULL)
        delete fTrackName.Ptr;
	fTrackName.Len = 0;
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
    if (fPortArray != NULL)
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


