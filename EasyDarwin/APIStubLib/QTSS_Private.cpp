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
    
    theArgs->outStubLibraryVersion = QTSS_API_VERSION;
    theArgs->outDispatchFunction = inDispatchFunc;
    
    return QTSS_NoErr;
}

// STUB FUNCTION DEFINITIONS

time_t          QTSS_MilliSecsTo1970Secs(int64_t inQTSS_MilliSeconds)
{
    time_t outSeconds = 0;
    (sCallbacks->addr [kConvertToUnixTimeCallback]) (&inQTSS_MilliSeconds, &outSeconds);
    return outSeconds;
}

// STARTUP ROUTINES
    
QTSS_Error  QTSS_AddRole(QTSS_Role inRole)
{
    return (sCallbacks->addr [kAddRoleCallback]) (inRole);  
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

QTSS_Error QTSS_GetAttrInfoByIndex(QTSS_Object inObject, uint32_t inIndex, QTSS_Object* outAttrInfoObject)
{
    return (sCallbacks->addr [kGetAttrInfoByIndexCallback]) (inObject, inIndex, outAttrInfoObject); 
}

QTSS_Error QTSS_GetAttrInfoByID(QTSS_Object inObject, QTSS_AttributeID inAttrID, QTSS_Object* outAttrInfoObject)
{
    return (sCallbacks->addr [kGetAttrInfoByIDCallback]) (inObject, inAttrID, outAttrInfoObject);   
}

QTSS_Error QTSS_GetAttrInfoByName(QTSS_Object inObject, char* inAttrName, QTSS_Object* outAttrInfoObject)
{
    return (sCallbacks->addr [kGetAttrInfoByNameCallback]) (inObject, inAttrName, outAttrInfoObject);   
}

QTSS_Error  QTSS_SetValue (QTSS_Object inDictionary, QTSS_AttributeID inID,uint32_t inIndex,  const void* inBuffer,  uint32_t inLen)
{
    return (sCallbacks->addr [kSetAttributeByIDCallback]) (inDictionary, inID, inIndex, inBuffer, inLen);   
}

QTSS_Error  QTSS_SetValuePtr (QTSS_Object inDictionary, QTSS_AttributeID inID, const void* inBuffer,  uint32_t inLen)
{
    return (sCallbacks->addr [kSetAttributePtrCallback]) (inDictionary, inID, inBuffer, inLen); 
}

QTSS_Error  QTSS_GetNumValues (QTSS_Object inObject, QTSS_AttributeID inID, uint32_t* outNumValues)
{
    return (sCallbacks->addr [kGetNumValuesCallback]) (inObject, inID, outNumValues);   
}

QTSS_Error  QTSS_GetNumAttributes (QTSS_Object inObject, uint32_t* outNumValues)
{
    return (sCallbacks->addr [kGetNumAttributesCallback]) (inObject, outNumValues); 
}

QTSS_Error  QTSS_RemoveValue (QTSS_Object inObject, QTSS_AttributeID inID, uint32_t inIndex)
{
    return (sCallbacks->addr [kRemoveValueCallback]) (inObject, inID, inIndex); 
}

// STREAM ROUTINES

QTSS_Error  QTSS_Write(QTSS_StreamRef inStream, const void* inBuffer, uint32_t inLen, uint32_t* outLenWritten, uint32_t inFlags)
{
    return (sCallbacks->addr [kWriteCallback]) (inStream, inBuffer, inLen, outLenWritten, inFlags); 
}

QTSS_Error  QTSS_WriteV(QTSS_StreamRef inStream, iovec* inVec, uint32_t inNumVectors, uint32_t inTotalLength, uint32_t* outLenWritten)
{
    return (sCallbacks->addr [kWriteVCallback]) (inStream, inVec, inNumVectors, inTotalLength, outLenWritten);  
}

QTSS_Error  QTSS_Flush(QTSS_StreamRef inStream)
{
    return (sCallbacks->addr [kFlushCallback]) (inStream);  
}

QTSS_Error  QTSS_Read(QTSS_StreamRef inRef, void* ioBuffer, uint32_t inBufLen, uint32_t* outLengthRead)
{
    return (sCallbacks->addr [kReadCallback]) (inRef, ioBuffer, inBufLen, outLengthRead);       
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

// FILE SYSTEM ROUTINES

QTSS_Error  QTSS_OpenFileObject(char* inPath, QTSS_OpenFileFlags inFlags, QTSS_Object* outFileObject)
{
    return (sCallbacks->addr [kOpenFileObjectCallback]) (inPath, inFlags, outFileObject);       
}

QTSS_Error  QTSS_CloseFileObject(QTSS_Object inFileObject)
{
    return (sCallbacks->addr [kCloseFileObjectCallback]) (inFileObject);        
}

// ASYNC I/O STREAM ROUTINES

QTSS_Error  QTSS_RequestEvent(QTSS_StreamRef inStream, QTSS_EventType inEventMask)
{
    return (sCallbacks->addr [kRequestEventCallback]) (inStream, inEventMask);      
}

QTSS_Error  QTSS_SignalStream(QTSS_StreamRef inStream)
{
    return (sCallbacks->addr [kSignalStreamCallback]) (inStream);       
}

QTSS_Error  QTSS_SetIdleTimer(int64_t inIdleMsec)
{
    return (sCallbacks->addr [kSetIdleTimerCallback]) (inIdleMsec);     
}

QTSS_Error  QTSS_SetIntervalRoleTimer(int64_t inIdleMsec)
{
    return (sCallbacks->addr [kSetIntervalRoleTimerCallback]) (inIdleMsec);     
}

QTSS_Error  QTSS_RequestGlobalLock()
{
    return (sCallbacks->addr [kRequestGlobalLockCallback])  ();
}

// SYNCH GLOBAL MULTIPLE READERS/SINGLE WRITER ROUTINES

bool  QTSS_IsGlobalLocked()
{
    return (bool) (sCallbacks->addr [kIsGlobalLockedCallback])  ();
}

QTSS_Error  QTSS_LockObject(QTSS_Object inObject)
{
    return (sCallbacks->addr [kLockObjectCallback])  (inObject);
}

QTSS_Error  QTSS_UnlockObject(QTSS_Object inObject)
{
    return (sCallbacks->addr [kUnlockObjectCallback])  (inObject);
}

// AUTHENTICATION AND AUTHORIZATION ROUTINE
QTSS_Error  QTSS_Authenticate(  const char* inAuthUserName, 
                                const char* inAuthResourceLocalPath, 
                                const char* inAuthMoviesDir, 
                                QTSS_ActionFlags inAuthRequestAction, 
                                QTSS_AuthScheme inAuthScheme, 
                                QTSS_RTSPRequestObject ioAuthRequestObject)
{
    return (sCallbacks->addr [kAuthenticateCallback]) (inAuthUserName, inAuthResourceLocalPath, inAuthMoviesDir, inAuthRequestAction, inAuthScheme, ioAuthRequestObject);
}

QTSS_Error	QTSS_Authorize(QTSS_RTSPRequestObject inAuthRequestObject, char** outAuthRealm, bool* outAuthUserAllowed)
{
    return (sCallbacks->addr [kAuthorizeCallback]) (inAuthRequestObject, outAuthRealm, outAuthUserAllowed);
}

void* Easy_GetRTSPPushSessions()
{
	return (void *) ((QTSS_CallbackPtrProcPtr) sCallbacks->addr [kGetRTSPPushSessionsCallback]) ();
}
