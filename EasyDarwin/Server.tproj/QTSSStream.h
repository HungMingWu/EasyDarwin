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
    File:       QTSSStream.h

    Contains:   Abstract base class containing the prototypes for generalized
                stream functions.
                
                Any server object that wants to act as a QTSS_StreamRef should
                derive off of this and implement one or more of the stream APIs.

*/


#ifndef __QTSS_STREAM_H__
#define __QTSS_STREAM_H__

#include "QTSS.h"
#include "Task.h"

class QTSSStream
{
    public:

        QTSSStream() : fTask(nullptr) {}
        virtual ~QTSSStream() {}
        
        //
        // A stream can have a task associated with it. If this stream supports
        // async I/O, the task is needed to know what to wakeup when there is an event
        void    SetTask(Task* inTask)   { fTask = inTask; }
        Task*   GetTask()               { return fTask; }

        virtual QTSS_Error  Read(void* /*ioBuffer*/, uint32_t /*inLen*/, uint32_t* /*outLen*/)
                                                            { return QTSS_Unimplemented; }
                                                            
        virtual QTSS_Error  Write(void* /*inBuffer*/, uint32_t /*inLen*/, uint32_t* /*outLenWritten*/, uint32_t /*inFlags*/)
                                                            { return QTSS_Unimplemented; }
        
        virtual QTSS_Error  WriteV(iovec* /*inVec*/, uint32_t /*inNumVectors*/, uint32_t /*inTotalLength*/, uint32_t* /*outLenWritten*/)
                                                            { return QTSS_Unimplemented; }
                                                            
        virtual QTSS_Error  Flush()                         { return QTSS_Unimplemented; }
        
        virtual QTSS_Error  Seek(uint64_t /*inNewPosition*/)  { return QTSS_Unimplemented; }
        
        virtual QTSS_Error  Advise(uint64_t /*inPosition*/, uint32_t /*inAdviseSize*/)
                                                            { return QTSS_Unimplemented; }
                                                            
        virtual QTSS_Error  RequestEvent(QTSS_EventType /*inEventMask*/)
                                                            { return QTSS_Unimplemented; }
    
    private:
    
        Task* fTask;
};

#endif //__QTSS_STREAM_H__
