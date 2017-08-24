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
    File:       QTSS_Private.h

    Contains:   Implementation-specific structures and typedefs used by the
                implementation of QTSS API in the Darwin Streaming Server
                    
    
    
*/


#ifndef QTSS_PRIVATE_H
#define QTSS_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "OSHeaders.h"
#include "QTSS.h"

class QTSSModule;
class Task;

typedef QTSS_Error  (*QTSS_CallbackProcPtr)(...);
typedef void*       (*QTSS_CallbackPtrProcPtr)(...);

enum
{
    // Indexes for each callback routine. Addresses of the callback routines get
    // placed in an array. 
    // IMPORTANT: When adding new callbacks, add only to the end of the list and increment the 
    //            kLastCallback value. Inserting or changing the index order will break dynamic modules
    //            built with another release.
    
    kAddRoleCallback                = 4,

    kIDForTagCallback               = 6,

	kSetAttributeByIDCallback       = 9,
    kWriteCallback                  = 10,

    kAddServiceCallback             = 13,
    kIDForServiceCallback           = 14,
    kDoServiceCallback              = 15,

    kRequestEventCallback           = 23,
    kSetIdleTimerCallback           = 24,

    kReadCallback                   = 27,
    kGetNumValuesCallback           = 30,
    kGetNumAttributesCallback       = 31,

    kAddStaticAttributeCallback     = 35,
    kAddInstanceAttributeCallback   = 36,
    kRemoveInstanceAttributeCallback= 37,
    kGetAttrInfoByIndexCallback     = 38,
    kGetAttrInfoByNameCallback      = 39,
    kGetAttrInfoByIDCallback        = 40,

    kRequestGlobalLockCallback      = 47, 
    kIsGlobalLockedCallback         = 48, 

    kAuthenticateCallback           = 50,
    kAuthorizeCallback              = 51,   

    kSetAttributePtrCallback        = 57,
    kSetIntervalRoleTimerCallback   = 58,

	kLastCallback                   = 62
};

typedef struct {
    // Callback function pointer array
    QTSS_CallbackProcPtr addr [kLastCallback];
} QTSS_Callbacks, *QTSS_CallbacksPtr;

typedef struct
{
    QTSS_CallbacksPtr       inCallbacks;
    QTSS_StreamRef          inErrorLogStream;
    QTSS_DispatchFuncPtr    outDispatchFunction;
    
} QTSS_PrivateArgs, *QTSS_PrivateArgsPtr;

typedef struct
{
    QTSSModule* curModule;  // this structure is setup in each thread
    QTSS_Role   curRole;    // before invoking a module in a role. Sometimes
    Task*       curTask;    // this info. helps callback implementation
    bool      eventRequested;
    bool      globalLockRequested;    // request event with global lock.
    bool      isGlobalLocked;
    int64_t      idleTime;   // If a module has requested idle time.
    
} QTSS_ModuleState, *QTSS_ModuleStatePtr;

QTSS_StreamRef  GetErrorLogStream();


#ifdef __cplusplus
}
#endif

#endif
