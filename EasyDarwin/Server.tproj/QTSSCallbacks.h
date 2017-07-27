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
	 File:       QTSSCallbacks.h

	 Contains:   All the QTSS callback functions


 */


#ifndef __QTSSCALLBACKS_H__
#define __QTSSCALLBACKS_H__

#include "QTSS.h"

class QTSSCallbacks
{
public:

	// TIME ROUTINES
	static void QTSS_ConvertToUnixTime(int64_t* inQTSS_MilliSecondsPtr, time_t* outSecondsPtr);

	// STARTUP ROUTINES

	static QTSS_Error   QTSS_AddRole(QTSS_Role inRole);

	// DICTIONARY ROUTINES

	// DICTIONARY LOCKING
	static QTSS_Error   QTSS_LockObject(QTSS_Object inDictionary);
	static QTSS_Error   QTSS_UnlockObject(QTSS_Object inDictionary);

	// CREATE NEW OBJECT TYPE
	static QTSS_Error   QTSS_CreateObjectType(QTSS_ObjectType* outType);

	// ADD ATTRIBUTE

	static QTSS_Error   QTSS_AddAttribute(QTSS_ObjectType inType, const char* inTag, void* inUnused);
	static QTSS_Error   QTSS_AddStaticAttribute(QTSS_ObjectType inObjectType, const char* inAttrName, void* inUnused, QTSS_AttrDataType inAttrDataType);
	static QTSS_Error   QTSS_AddInstanceAttribute(QTSS_Object inObject, const char* inAttrName, void* inUnused, QTSS_AttrDataType inAttrDataType);

	// REMOVE ATTRIBUTE

	static QTSS_Error   QTSS_RemoveInstanceAttribute(QTSS_Object inObject, QTSS_AttributeID inID);

	// ATTRIBUTE INFO
	static QTSS_Error   QTSS_IDForAttr(QTSS_ObjectType inType, const char* inTag, QTSS_AttributeID* outID);

	static QTSS_Error   QTSS_GetAttrInfoByName(QTSS_Object inObject, const char* inAttrName, QTSS_Object* outAttrInfoObject);
	static QTSS_Error   QTSS_GetAttrInfoByID(QTSS_Object inObject, QTSS_AttributeID inAttrID, QTSS_Object* outAttrInfoObject);
	static QTSS_Error   QTSS_GetAttrInfoByIndex(QTSS_Object inObject, uint32_t inIndex, QTSS_Object* outAttrInfoObject);

	static QTSS_Error   QTSS_GetNumAttributes(QTSS_Object inObject, uint32_t* outNumValues);

	// ATTRIBUTE VALUES

	static QTSS_Error   QTSS_SetValue(QTSS_Object inDictionary, QTSS_AttributeID inID, uint32_t inIndex, const void* inBuffer, uint32_t inLen);
	static QTSS_Error   QTSS_SetValuePtr(QTSS_Object inDictionary, QTSS_AttributeID inID, const void* inBuffer, uint32_t inLen);
	static QTSS_Error   QTSS_CreateObject(QTSS_Object inDictionary, QTSS_AttributeID inID, QTSS_ObjectType inType, uint32_t* outIndex, QTSS_Object* outCreatedObject);
	static QTSS_Error   QTSS_GetNumValues(QTSS_Object inObject, QTSS_AttributeID inID, uint32_t* outNumValues);
	static QTSS_Error   QTSS_RemoveValue(QTSS_Object inObject, QTSS_AttributeID inID, uint32_t inIndex);

	// STREAM ROUTINES

	static QTSS_Error   QTSS_Write(QTSS_StreamRef inStream, void* inBuffer, uint32_t inLen, uint32_t* outLenWritten, QTSS_WriteFlags inFlags);
	static QTSS_Error   QTSS_WriteV(QTSS_StreamRef inStream, iovec* inVec, uint32_t inNumVectors, uint32_t inTotalLength, uint32_t* outLenWritten);
	static QTSS_Error   QTSS_Flush(QTSS_StreamRef inStream);
	static QTSS_Error   QTSS_Read(QTSS_StreamRef inRef, void* ioBuffer, uint32_t inBufLen, uint32_t* outLengthRead);

	// FILE SYSTEM ROUTINES

	static QTSS_Error   QTSS_OpenFileObject(char* inPath, QTSS_OpenFileFlags inFlags, QTSS_Object* outFileObject);
	static QTSS_Error   QTSS_CloseFileObject(QTSS_Object inFileObject);

	// SOCKET ROUTINES

	static QTSS_Error   QTSS_CreateStreamFromSocket(int inFileDesc, QTSS_StreamRef* outStream);
	static QTSS_Error   QTSS_DestroySocketStream(QTSS_StreamRef inStream);

	// SERVICE ROUTINES

	static QTSS_Error   QTSS_AddService(const char* inServiceName, QTSS_ServiceFunctionPtr inFunctionPtr);
	static QTSS_Error   QTSS_IDForService(const char* inTag, QTSS_ServiceID* outID);
	static QTSS_Error   QTSS_DoService(QTSS_ServiceID inID, QTSS_ServiceFunctionArgsPtr inArgs);

	// RTSP ROUTINES

	static QTSS_Error   QTSS_SendRTSPHeaders(QTSS_RTSPRequestObject inRef);
	static QTSS_Error   QTSS_AppendRTSPHeader(QTSS_RTSPRequestObject inRef, QTSS_RTSPHeader inHeader, char* inValue, uint32_t inValueLen);

	// ASYNC I/O ROUTINES
	static QTSS_Error   QTSS_RequestEvent(QTSS_StreamRef inStream, QTSS_EventType inEventMask);
	static QTSS_Error   QTSS_SignalStream(QTSS_StreamRef inStream);
	static QTSS_Error   QTSS_SetIdleTimer(int64_t inMsecToWait);
	static QTSS_Error   QTSS_SetIdleRoleTimer(int64_t inMsecToWait);


	static QTSS_Error   QTSS_RequestLockedCallback();
	static bool			QTSS_IsGlobalLocked();
	static QTSS_Error   QTSS_UnlockGlobalLock();

	// AUTHENTICATION AND AUTHORIZATION ROUTINE
	static QTSS_Error   QTSS_Authenticate(const char* inAuthUserName, const char* inAuthResourceLocalPath, const char* inAuthMoviesDir, QTSS_ActionFlags inAuthRequestAction, QTSS_AuthScheme inAuthScheme, QTSS_RTSPRequestObject ioAuthRequestObject);
	static QTSS_Error	QTSS_Authorize(QTSS_RTSPRequestObject inAuthRequestObject, char** outAuthRealm, bool* outAuthUserAllowed);

	static void* Easy_GetRTSPPushSessions();
};

#endif //__QTSSCALLBACKS_H__
