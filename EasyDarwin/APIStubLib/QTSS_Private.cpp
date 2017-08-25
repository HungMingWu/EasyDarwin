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
    File:       QTSS_Private.c

    Contains:   Code for stub library and stub callback functions.  
*/

#include <stdlib.h>
#ifndef __Win32__
#include <sys/types.h>
#include <sys/uio.h>
#endif

#include "QTSS.h"
#include "QTSS_Private.h"

static QTSS_CallbacksPtr    sCallbacks = nullptr;
static QTSS_StreamRef       sErrorLogStream = nullptr;

QTSS_Error _stublibrary_main(void* inPrivateArgs, QTSS_DispatchFuncPtr inDispatchFunc)
{
    auto theArgs = (QTSS_PrivateArgsPtr)inPrivateArgs;
    
    // Setup

    sCallbacks = theArgs->inCallbacks;
    sErrorLogStream = theArgs->inErrorLogStream;
    
    // Send requested information back to the server
    
    theArgs->outDispatchFunction = inDispatchFunc;
    
    return QTSS_NoErr;
}

// DICTIONARY ROUTINES

QTSS_Error  QTSS_AddStaticAttribute(QTSS_ObjectType inObjectType, char* inAttrName, void* inUnused, QTSS_AttrDataType inAttrDataType)
{
    return (sCallbacks->addr [kAddStaticAttributeCallback]) (inObjectType, inAttrName, inUnused, inAttrDataType);   
}

QTSS_Error  QTSS_AddInstanceAttribute(QTSS_Object inObject, char* inAttrName, void* inUnused, QTSS_AttrDataType inAttrDataType)
{
    return (sCallbacks->addr [kAddInstanceAttributeCallback]) (inObject, inAttrName, inUnused, inAttrDataType); 
}

QTSS_Error QTSS_RemoveInstanceAttribute(QTSS_Object inObject, QTSS_AttributeID inID)
{
    return (sCallbacks->addr [kRemoveInstanceAttributeCallback]) (inObject, inID);  
}

QTSS_Error  QTSS_IDForAttr(QTSS_ObjectType inType, const char* inTag, QTSS_AttributeID* outID)
{
    return (sCallbacks->addr [kIDForTagCallback]) (inType, inTag, outID);   
}

QTSS_Error QTSS_GetAttrInfoByName(QTSS_Object inObject, char* inAttrName, QTSS_Object* outAttrInfoObject)
{
    return (sCallbacks->addr [kGetAttrInfoByNameCallback]) (inObject, inAttrName, outAttrInfoObject);   
}

QTSS_Error  QTSS_SetValue (QTSS_Object inDictionary, QTSS_AttributeID inID,uint32_t inIndex,  const void* inBuffer,  uint32_t inLen)
{
    return (sCallbacks->addr [kSetAttributeByIDCallback]) (inDictionary, inID, inIndex, inBuffer, inLen);   
}

// SERVICE ROUTINES

QTSS_Error QTSS_AddService(const char* inServiceName, QTSS_ServiceFunctionPtr inFunctionPtr)
{
    return (sCallbacks->addr [kAddServiceCallback]) (inServiceName, inFunctionPtr);     
}

QTSS_Error QTSS_IDForService(const char* inTag, QTSS_ServiceID* outID)
{
    return (sCallbacks->addr [kIDForServiceCallback]) (inTag, outID);   
}

QTSS_Error QTSS_DoService(QTSS_ServiceID inID, QTSS_ServiceFunctionArgsPtr inArgs)
{
    return (sCallbacks->addr [kDoServiceCallback]) (inID, inArgs);  
}

// ASYNC I/O STREAM ROUTINES

QTSS_Error  QTSS_SetIdleTimer(int64_t inIdleMsec)
{
    return (sCallbacks->addr [kSetIdleTimerCallback]) (inIdleMsec);     
}