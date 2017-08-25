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
	 File:       QTSSModule.cpp
	 Contains:   Implements object defined in QTSSModule.h
 */

#include <memory>
#include "QTSSModule.h"
#include "StringParser.h"
#include "QTSServerInterface.h"

bool  QTSSModule::sHasRTSPRequestModule = false;
bool  QTSSModule::sHasOpenFileModule = false;
bool  QTSSModule::sHasRTSPAuthenticateModule = false;

QTSSAttrInfoDict::AttrInfo  QTSSModule::sAttributes[] =
{   /*fields:   fAttrName, fFuncPtr, fAttrDataType, fAttrPermission */
	/* 0 */ {},
	/* 1 */ { "qtssModDesc",            nullptr,                   qtssAttrDataTypeCharArray,  qtssAttrModeRead | qtssAttrModeWrite },
	/* 2 */ { "qtssModVersion",         nullptr,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModeWrite },
	/* 3 */ { "qtssModRoles",           nullptr,                   qtssAttrDataTypeUInt32,     qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 4 */ { "qtssModPrefs",           nullptr,                   qtssAttrDataTypeQTSS_Object,qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeInstanceAttrAllowed },
	/* 5 */ { "qtssModAttributes",      nullptr,                   qtssAttrDataTypeQTSS_Object, qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeInstanceAttrAllowed }
};

char*    QTSSModule::sRoleNames[] =
{
		   "InitializeRole"           ,
		   "ShutdownRole"             ,
		   "RTSPFilterRole"           ,
		   "RTSPRouteRole"            ,
		   "RTSPAthnRole"             ,
		   "RTSPAuthRole"             ,
		   "RTSPPreProcessorRole"     ,
		   "RTSPRequestRole"          ,
		   "RTSPPostProcessorRole"    ,
		   "RTSPSessionClosingRole"   ,
		   "RTPSendPacketsRole"       ,
		   "ClientSessionClosingRole" ,
		   "RTCPProcessRole"          ,
		   "ErrorLogRole"             ,
		   "RereadPrefsRole"          ,
		   "OpenFileRole"             ,
		   "OpenFilePreProcessRole"   ,
		   "AdviseFileRole"           ,
		   "ReadFileRole"             ,
		   "CloseFileRole"            ,
		   "RequestEventFileRole"     ,
		   "RTSPIncomingDataRole"     ,
		   "StateChangeRole"          ,
		   "TimedIntervalRole"        ,
		   ""
};


void QTSSModule::Initialize()
{
	//Setup all the dictionary stuff
	for (uint32_t x = 0; x < qtssModNumParams; x++)
		QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kModuleDictIndex)->
		SetAttribute(x, sAttributes[x].fAttrName, sAttributes[x].fFuncPtr,
			sAttributes[x].fAttrDataType, sAttributes[x].fAttrPermission);
}

QTSSModule::QTSSModule(char* inPath)
	: QTSSDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kModuleDictIndex)),
	fPath(nullptr),
	fDispatchFunc(nullptr),
	fPrefs(nullptr),
	fAttributes(nullptr)
{
	this->SetTaskName("QTSSModule");

	fAttributes = new QTSSDictionary(nullptr, &fAttributesMutex);

	this->SetVal(qtssModPrefs, &fPrefs, sizeof(fPrefs));
	this->SetVal(qtssModAttributes, &fAttributes, sizeof(fAttributes));

	::memset(fRoleArray, 0, sizeof(fRoleArray));
	::memset(&fModuleState, 0, sizeof(fModuleState));

}

QTSS_Error  QTSSModule::SetupModule(QTSS_CallbacksPtr inCallbacks, QTSS_MainEntryPointPtr inEntrypoint)
{
	QTSS_Error theErr = QTSS_NoErr;

	// Load fragment from disk if necessary

	if (theErr != QTSS_NoErr)
		return theErr;

	// At this point, we must have an entrypoint
	if (inEntrypoint == nullptr)
		return QTSS_NotAModule;

	// Invoke the private initialization routine
	QTSS_PrivateArgs thePrivateArgs;
	thePrivateArgs.inCallbacks = inCallbacks;
	thePrivateArgs.outDispatchFunction = nullptr;

	theErr = (inEntrypoint)(&thePrivateArgs);
	if (theErr != QTSS_NoErr)
		return theErr;

	// Set the dispatch function so we'll be able to invoke this module later on

	fDispatchFunc = thePrivateArgs.outDispatchFunction;

	return QTSS_NoErr;
}

int32_t QTSSModule::GetPrivateRoleIndex(QTSS_Role apiRole)
{

	switch (apiRole)
	{
		// Map actual QTSS Role names to our private enum values. Turn on the proper one
		// in the role array
	case QTSS_RTSPFilter_Role:          return kRTSPFilterRole;
	case QTSS_RTSPAuthenticate_Role:    return kRTSPAthnRole;
	case QTSS_RTSPRequest_Role:         return kRTSPRequestRole;
	case QTSS_RTSPSessionClosing_Role:  return kRTSPSessionClosingRole;
	case QTSS_RTPSendPackets_Role:      return kRTPSendPacketsRole;
	case QTSS_RTCPProcess_Role:         return kRTCPProcessRole;
	case QTSS_ErrorLog_Role:            return kErrorLogRole;
	case QTSS_OpenFile_Role:            return kOpenFileRole;
	case QTSS_OpenFilePreProcess_Role:  return kOpenFilePreProcessRole;
	case QTSS_AdviseFile_Role:          return kAdviseFileRole;
	case QTSS_ReadFile_Role:            return kReadFileRole;
	case QTSS_CloseFile_Role:           return kCloseFileRole;
	case QTSS_RequestEventFile_Role:    return kRequestEventFileRole;
	case QTSS_StateChange_Role:         return kStateChangeRole;
	case Easy_HLSOpen_Role:				return kEasyHLSOpenRole;
	case Easy_HLSClose_Role:			return kEasyHLSCloseRole;
	case Easy_CMSFreeStream_Role:		return kEasyCMSFreeStreamRole;
	case Easy_RedisTTL_Role:			return kRedisTTLRole;
	case Easy_RedisSetRTSPLoad_Role:	return kRedisSetRTSPLoadRole;
	case Easy_RedisUpdateStreamInfo_Role:	return kRedisUpdateStreamInfoRole;
	case Easy_RedisGetAssociatedCMS_Role:	return kRedisGetAssociatedCMSRole;
	case Easy_RedisJudgeStreamID_Role:	return kRedisJudgeStreamIDRole;

	case Easy_LiveDeviceStream_Role:	return kLiveDeviceStreamRole;
	default:
		return -1;
	}
}

QTSS_Error  QTSSModule::AddRole(QTSS_Role inRole)
{
	// There can only be one QTSS_RTSPRequest processing module
	if ((inRole == QTSS_RTSPRequest_Role) && (sHasRTSPRequestModule))
		return QTSS_RequestFailed;
	if ((inRole == QTSS_OpenFilePreProcess_Role) && (sHasOpenFileModule))
		return QTSS_RequestFailed;

#if 0// Allow multiple modules in QTSS v6.0. Enabling forces the first auth module There can be only one module registered for QTSS_RTSPAuthenticate_Role 
	if ((inRole == QTSS_RTSPAuthenticate_Role) && (sHasRTSPAuthenticateModule))
		return QTSS_RequestFailed;
#endif


	int32_t arrayID = GetPrivateRoleIndex(inRole);
	if (arrayID < 0)
		return QTSS_BadArgument;

	fRoleArray[arrayID] = true;

	if (inRole == QTSS_RTSPRequest_Role)
		sHasRTSPRequestModule = true;
	if (inRole == QTSS_OpenFile_Role)
		sHasOpenFileModule = true;
	if (inRole == QTSS_RTSPAuthenticate_Role)
		sHasRTSPAuthenticateModule = true;

	//
	// Add this role to the array of roles attribute
	QTSS_Error theErr = this->SetValue(qtssModRoles, this->GetNumValues(qtssModRoles), &inRole, sizeof(inRole), QTSSDictionary::kDontObeyReadOnly);
	Assert(theErr == QTSS_NoErr);
	return QTSS_NoErr;
}

int64_t QTSSModule::Run()
{
	EventFlags events = this->GetEvents();

	OSThreadDataSetter theSetter(&fModuleState, nullptr);
	if (events & Task::kUpdateEvent)
	{   // force us to update to a new idle time
		return fModuleState.idleTime;// If the module has requested idle time...
	}

	if (fRoleArray[kTimedIntervalRole])
	{
		if (events & Task::kIdleEvent)
		{
			fModuleState.curModule = this;  // this structure is setup in each thread
			fModuleState.curTask = this;
			return fModuleState.idleTime; // If the module has requested idle time...
		}
	}

	return 0;
}
