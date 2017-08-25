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

	// ADD ATTRIBUTE

	static QTSS_Error   QTSS_AddStaticAttribute(QTSS_ObjectType inObjectType, const char* inAttrName, void* inUnused, QTSS_AttrDataType inAttrDataType);
	static QTSS_Error   QTSS_AddInstanceAttribute(QTSS_Object inObject, const char* inAttrName, void* inUnused, QTSS_AttrDataType inAttrDataType);

	// REMOVE ATTRIBUTE

	static QTSS_Error   QTSS_RemoveInstanceAttribute(QTSS_Object inObject, QTSS_AttributeID inID);

	// ATTRIBUTE INFO
	static QTSS_Error   QTSS_IDForAttr(QTSS_ObjectType inType, const char* inTag, QTSS_AttributeID* outID);

	static QTSS_Error   QTSS_GetAttrInfoByName(QTSS_Object inObject, const char* inAttrName, QTSS_Object* outAttrInfoObject);

	// ATTRIBUTE VALUES

	static QTSS_Error   QTSS_SetValue(QTSS_Object inDictionary, QTSS_AttributeID inID, uint32_t inIndex, const void* inBuffer, uint32_t inLen);
	static QTSS_Error   QTSS_SetValuePtr(QTSS_Object inDictionary, QTSS_AttributeID inID, const void* inBuffer, uint32_t inLen);

	// SERVICE ROUTINES

	static QTSS_Error   QTSS_AddService(const char* inServiceName, QTSS_ServiceFunctionPtr inFunctionPtr);
	static QTSS_Error   QTSS_IDForService(const char* inTag, QTSS_ServiceID* outID);
	static QTSS_Error   QTSS_DoService(QTSS_ServiceID inID, QTSS_ServiceFunctionArgsPtr inArgs);

	// ASYNC I/O ROUTINES
	static QTSS_Error   QTSS_SetIdleTimer(int64_t inMsecToWait);
};

#endif //__QTSSCALLBACKS_H__
