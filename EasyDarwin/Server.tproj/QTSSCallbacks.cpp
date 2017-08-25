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
	 File:       QTSSCallbacks.cpp

	 Contains:   Implements QTSS Callback functions.
 */

#include "QTSSCallbacks.h"
#include "QTSSDictionary.h"
#include "QTSSStream.h"
#include "RTSPRequestInterface.h"
#include "RTPSession.h"
#include "OS.h"
#include "QTSSFile.h"
#include "QTSServerInterface.h"
#include "QTSSDataConverter.h"
#include "QTSSModule.h"
#include "RTSPRequest.h"

#include <errno.h>

#include "ReflectorSession.h"

using namespace std;

#define __QTSSCALLBACKS_DEBUG__ 0
#define debug_printf if (__QTSSCALLBACKS_DEBUG__) printf

QTSS_Error  QTSSCallbacks::QTSS_AddStaticAttribute(QTSS_ObjectType inObjectType, const char* inAttrName, void* inUnused, QTSS_AttrDataType inAttrDataType)
{
	Assert(inUnused == nullptr);
	auto* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
	if (OSThread::GetCurrent() != nullptr)
		theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();

	// Static attributes can only be added before modules have had their Initialize role invoked.
	if (theState == nullptr)
		return QTSS_OutOfState;

	uint32_t theDictionaryIndex = QTSSDictionaryMap::GetMapIndex(inObjectType);
	if (theDictionaryIndex == QTSSDictionaryMap::kIllegalDictionary)
		return QTSS_BadArgument;

	QTSSDictionaryMap* theMap = QTSSDictionaryMap::GetMap(theDictionaryIndex);
	return theMap->AddAttribute(inAttrName, nullptr, inAttrDataType, qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe);
}

QTSS_Error  QTSSCallbacks::QTSS_AddInstanceAttribute(QTSS_Object inObject, const char* inAttrName, void* inUnused, QTSS_AttrDataType inAttrDataType)
{
	Assert(inUnused == nullptr);
	if ((inObject == nullptr) || (inAttrName == nullptr))
		return QTSS_BadArgument;

	return ((QTSSDictionary*)inObject)->AddInstanceAttribute(inAttrName, nullptr, inAttrDataType, qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModeDelete | qtssAttrModePreempSafe);
}

QTSS_Error QTSSCallbacks::QTSS_RemoveInstanceAttribute(QTSS_Object inObject, QTSS_AttributeID inID)
{
	if (inObject == nullptr || (inID == qtssIllegalAttrID))
		return QTSS_BadArgument;

	return ((QTSSDictionary*)inObject)->RemoveInstanceAttribute(inID);
}


QTSS_Error  QTSSCallbacks::QTSS_IDForAttr(QTSS_ObjectType inType, const char* inName, QTSS_AttributeID* outID)
{
	if (outID == nullptr)
		return QTSS_BadArgument;

	uint32_t theDictionaryIndex = QTSSDictionaryMap::GetMapIndex(inType);
	if (theDictionaryIndex == QTSSDictionaryMap::kIllegalDictionary)
		return QTSS_BadArgument;

	return QTSSDictionaryMap::GetMap(theDictionaryIndex)->GetAttrID(inName, outID);
}

QTSS_Error QTSSCallbacks::QTSS_GetAttrInfoByName(QTSS_Object inObject, const char* inAttrName, QTSS_Object* outAttrInfoObject)
{
	if (inObject == nullptr)
		return QTSS_BadArgument;

	return ((QTSSDictionary*)inObject)->GetAttrInfoByName(inAttrName, (QTSSAttrInfoDict**)outAttrInfoObject);
}

QTSS_Error  QTSSCallbacks::QTSS_SetValue(QTSS_Object inDictionary, QTSS_AttributeID inID, uint32_t inIndex, const void* inBuffer, uint32_t inLen)
{
	if ((inDictionary == nullptr) || ((inBuffer == nullptr) && (inLen > 0)) || (inID == qtssIllegalAttrID))
		return QTSS_BadArgument;
	return ((QTSSDictionary*)inDictionary)->SetValue(inID, inIndex, inBuffer, inLen);
}

QTSS_Error  QTSSCallbacks::QTSS_AddService(const char* inServiceName, QTSS_ServiceFunctionPtr inFunctionPtr)
{
	auto* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
	if (OSThread::GetCurrent() != nullptr)
		theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();

	// This may happen if this callback is occurring on module-created thread
	if (theState == nullptr)
		return QTSS_OutOfState;

	return QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kServiceDictIndex)->
		AddAttribute(inServiceName, (QTSS_AttrFunctionPtr)inFunctionPtr, qtssAttrDataTypeUnknown, qtssAttrModeRead);
}

QTSS_Error  QTSSCallbacks::QTSS_IDForService(const char* inTag, QTSS_ServiceID* outID)
{
	return QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kServiceDictIndex)->
		GetAttrID(inTag, outID);
}

QTSS_Error  QTSSCallbacks::QTSS_DoService(QTSS_ServiceID inID, QTSS_ServiceFunctionArgsPtr inArgs)
{
	// Make sure that the service ID is in fact valid

	QTSSDictionaryMap* theMap = QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kServiceDictIndex);
	int32_t theIndex = theMap->ConvertAttrIDToArrayIndex(inID);
	if (theIndex < 0)
		return QTSS_IllegalService;

	// Get the service function 
	auto theFunction = (QTSS_ServiceFunctionPtr)theMap->GetAttrFunction(theIndex);

	// Invoke it, return the result.    
	return (theFunction)(inArgs);
}

QTSS_Error  QTSSCallbacks::QTSS_SetIdleTimer(int64_t inMsecToWait)
{
	auto* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
	if (OSThread::GetCurrent() != nullptr)
		theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();

	// This may happen if this callback is occurring on module-created thread
	if (theState == nullptr)
		return QTSS_RequestFailed;

	if (theState->curTask == nullptr)
		return QTSS_OutOfState;

	theState->idleTime = inMsecToWait;
	return QTSS_NoErr;
}