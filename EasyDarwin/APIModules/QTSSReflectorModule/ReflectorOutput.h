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
    File:       ReflectorOutput.h

    Contains:   VERY simple abstract base class that defines one virtual method, WritePacket.
                This is extremely useful to the reflector, which, using one of these objects,
                can transparently reflect a packet, not being aware of how it will actually be
                written to the network
                    


*/

#ifndef __REFLECTOR_OUTPUT_H__
#define __REFLECTOR_OUTPUT_H__

#include "QTSS.h"
#include "OSHeaders.h"
#include "MyAssert.h"
#include "OS.h"

class ReflectorPacket;

class ReflectorOutput
{
	public:
    
		ReflectorOutput(size_t numStreams) : fBookmarkedPacketsElemsArray(numStreams * 2) {};
		virtual ~ReflectorOutput() = default;
        
        // an array of packet elements ( from fPacketQueue in ReflectorSender )
        // possibly one for each ReflectorSender that sends data to this ReflectorOutput        
        std::vector<ReflectorPacket*> fBookmarkedPacketsElemsArray;
		OSMutex             fMutex;
	public:
		inline  ReflectorPacket*    GetBookMarkedPacket(const std::list<std::unique_ptr<ReflectorPacket>> &thePacketQueue);
		inline  void SetBookMarkPacket(ReflectorPacket* thePacketElemPtr);
        
        // WritePacket
        //
        // Pass in the packet contents, the cookie of the stream to which it will be written,
        // and the QTSS API write flags (this should either be qtssWriteFlagsIsRTP or IsRTCP
        // packetLateness is how many MSec's late this packet is in being delivered ( will be < 0 if its early )
        // If this function returns QTSS_WouldBlock, timeToSendThisPacketAgain will
        // be set to # of msec in which the packet can be sent, or -1 if unknown
        virtual QTSS_Error  WritePacket(const std::vector<char> &inPacket, void* inStreamCookie,
			uint32_t inFlags,
			uint64_t packetID) = 0;
    
        virtual void      TearDown() = 0;
        virtual bool      IsUDP() = 0;
        virtual bool      IsPlaying() = 0;
        
        enum { kWaitMilliSec = 5, kMaxWaitMilliSec = 1000 };
};

void  ReflectorOutput::SetBookMarkPacket(ReflectorPacket* thePacketElemPtr)
{
	for (auto &elem : fBookmarkedPacketsElemsArray)
		if (elem == nullptr) {
			elem = thePacketElemPtr;
			return;
		}
}

ReflectorPacket*    ReflectorOutput::GetBookMarkedPacket(const std::list<std::unique_ptr<ReflectorPacket>> &thePacketQueue)
{
    ReflectorPacket*        packetElem = nullptr;              

    // see if we've bookmarked a held packet for this Sender in this Output
    for (auto &bookmarkedElem : fBookmarkedPacketsElemsArray)
    {         
		if (!bookmarkedElem) continue;

		auto it = std::find_if(begin(thePacketQueue), end(thePacketQueue),
			[bookmarkedElem](const std::unique_ptr<ReflectorPacket> &pkt) {
			return pkt.get() == bookmarkedElem;
		});
		if (it != end(thePacketQueue))
		{
			// this packet was previously bookmarked for this specific queue
			// remove if from the bookmark list and use it
			// to jump ahead into the Sender's over all packet queue                        
			bookmarkedElem = nullptr;
			packetElem = bookmarkedElem;
			break;
		}
    }

    return packetElem;
}
#endif //__REFLECTOR_OUTPUT_H__
