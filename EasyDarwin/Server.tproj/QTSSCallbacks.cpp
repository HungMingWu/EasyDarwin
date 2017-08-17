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
#include "QTSSSocket.h"
#include "QTSServerInterface.h"
#include "QTSSDataConverter.h"
#include "QTSSModule.h"
#include "RTSPRequest.h"

#include <errno.h>

#include "EasyProtocolDef.h"
#include "EasyProtocol.h"

#include "ReflectorSession.h"

using namespace EasyDarwin::Protocol;
using namespace std;

#define __QTSSCALLBACKS_DEBUG__ 0
#define debug_printf if (__QTSSCALLBACKS_DEBUG__) printf

void    QTSSCallbacks::QTSS_ConvertToUnixTime(int64_t *inQTSS_MilliSecondsPtr, time_t* outSecondsPtr)
{
	if ((nullptr != outSecondsPtr) && (nullptr != inQTSS_MilliSecondsPtr))
		*outSecondsPtr = OS::TimeMilli_To_UnixTimeSecs(*inQTSS_MilliSecondsPtr);
}



QTSS_Error  QTSSCallbacks::QTSS_AddRole(QTSS_Role inRole)
{
	auto* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
	if (OSThread::GetCurrent() != nullptr)
		theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();

	// Roles can only be added before modules have had their Initialize role invoked.
	if ((theState == nullptr) || (theState->curRole != QTSS_Register_Role))
		return QTSS_OutOfState;

	return theState->curModule->AddRole(inRole);
}



QTSS_Error QTSSCallbacks::QTSS_LockObject(QTSS_Object inDictionary)
{
	if (inDictionary == nullptr)
		return QTSS_BadArgument;

	((QTSSDictionary*)inDictionary)->GetMutex()->Lock();
	((QTSSDictionary*)inDictionary)->SetLocked(true);
	return QTSS_NoErr;
}

QTSS_Error QTSSCallbacks::QTSS_UnlockObject(QTSS_Object inDictionary)
{
	if (inDictionary == nullptr)
		return QTSS_BadArgument;

	((QTSSDictionary*)inDictionary)->SetLocked(false);
	((QTSSDictionary*)inDictionary)->GetMutex()->Unlock();

	return QTSS_NoErr;
}

QTSS_Error  QTSSCallbacks::QTSS_AddStaticAttribute(QTSS_ObjectType inObjectType, const char* inAttrName, void* inUnused, QTSS_AttrDataType inAttrDataType)
{
	Assert(inUnused == nullptr);
	auto* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
	if (OSThread::GetCurrent() != nullptr)
		theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();

	// Static attributes can only be added before modules have had their Initialize role invoked.
	if ((theState == nullptr) || (theState->curRole != QTSS_Register_Role))
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

QTSS_Error QTSSCallbacks::QTSS_GetAttrInfoByIndex(QTSS_Object inObject, uint32_t inIndex, QTSS_Object* outAttrInfoObject)
{
	if (inObject == nullptr)
		return QTSS_BadArgument;

	return ((QTSSDictionary*)inObject)->GetAttrInfoByIndex(inIndex, (QTSSAttrInfoDict**)outAttrInfoObject);
}

QTSS_Error QTSSCallbacks::QTSS_GetAttrInfoByID(QTSS_Object inObject, QTSS_AttributeID inAttrID, QTSS_Object* outAttrInfoObject)
{
	if (inObject == nullptr || (inAttrID == qtssIllegalAttrID))
		return QTSS_BadArgument;

	return ((QTSSDictionary*)inObject)->GetAttrInfoByID(inAttrID, (QTSSAttrInfoDict**)outAttrInfoObject);
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

QTSS_Error  QTSSCallbacks::QTSS_SetValuePtr(QTSS_Object inDictionary, QTSS_AttributeID inID, const void* inBuffer, uint32_t inLen)
{
	if ((inDictionary == nullptr) || ((inBuffer == nullptr) && (inLen > 0)))
		return QTSS_BadArgument;
	return ((QTSSDictionary*)inDictionary)->SetValuePtr(inID, inBuffer, inLen);
}

QTSS_Error  QTSSCallbacks::QTSS_GetNumValues(QTSS_Object inObject, QTSS_AttributeID inID, uint32_t* outNumValues)
{
	if ((inObject == nullptr) || (outNumValues == nullptr) || (inID == qtssIllegalAttrID))
		return QTSS_BadArgument;

	*outNumValues = ((QTSSDictionary*)inObject)->GetNumValues(inID);
	return QTSS_NoErr;
}

QTSS_Error QTSSCallbacks::QTSS_GetNumAttributes(QTSS_Object inObject, uint32_t* outNumValues)
{

	if (outNumValues == nullptr)
		return QTSS_BadArgument;

	if (inObject == nullptr)
		return QTSS_BadArgument;

	OSMutexLocker locker(((QTSSDictionary*)inObject)->GetMutex());

	*outNumValues = 0;

	// Get the Static Attribute count
	QTSSDictionaryMap* theMap = ((QTSSDictionary*)inObject)->GetDictionaryMap();
	if (theMap != nullptr)
		*outNumValues += theMap->GetNumNonRemovedAttrs();
	// Get the Instance Attribute count
	theMap = ((QTSSDictionary*)inObject)->GetInstanceDictMap();
	if (theMap != nullptr)
		*outNumValues += theMap->GetNumNonRemovedAttrs();

	return QTSS_NoErr;
}

QTSS_Error  QTSSCallbacks::QTSS_RemoveValue(QTSS_Object inObject, QTSS_AttributeID inID, uint32_t inIndex)
{
	if (inObject == nullptr)
		return QTSS_BadArgument;

	return ((QTSSDictionary*)inObject)->RemoveValue(inID, inIndex);
}



QTSS_Error  QTSSCallbacks::QTSS_Write(QTSS_StreamRef inStream, void* inBuffer, uint32_t inLen, uint32_t* outLenWritten, uint32_t inFlags)
{
	if (inStream == nullptr)
		return QTSS_BadArgument;
	QTSS_Error theErr = ((QTSSStream*)inStream)->Write(inBuffer, inLen, outLenWritten, inFlags);

	// Server internally propogates POSIX errorcodes such as EAGAIN and ENOTCONN up to this
	// level. The API guarentees that no POSIX errors get returned, so we have QTSS_Errors
	// to replace them. So we have to replace them here.
	if (theErr == EAGAIN)
		return QTSS_WouldBlock;
	else if (theErr > 0)
		return QTSS_NotConnected;
	else
		return theErr;
}

QTSS_Error  QTSSCallbacks::QTSS_WriteV(QTSS_StreamRef inStream, iovec* inVec, uint32_t inNumVectors, uint32_t inTotalLength, uint32_t* outLenWritten)
{
	if (inStream == nullptr)
		return QTSS_BadArgument;
	QTSS_Error theErr = ((QTSSStream*)inStream)->WriteV(inVec, inNumVectors, inTotalLength, outLenWritten);

	// Server internally propogates POSIX errorcodes such as EAGAIN and ENOTCONN up to this
	// level. The API guarentees that no POSIX errors get returned, so we have QTSS_Errors
	// to replace them. So we have to replace them here.
	if (theErr == EAGAIN)
		return QTSS_WouldBlock;
	else if (theErr > 0)
		return QTSS_NotConnected;
	else
		return theErr;
}

QTSS_Error  QTSSCallbacks::QTSS_Flush(QTSS_StreamRef inStream)
{
	if (inStream == nullptr)
		return QTSS_BadArgument;
	QTSS_Error theErr = ((QTSSStream*)inStream)->Flush();

	// Server internally propogates POSIX errorcodes such as EAGAIN and ENOTCONN up to this
	// level. The API guarentees that no POSIX errors get returned, so we have QTSS_Errors
	// to replace them. So we have to replace them here.
	if (theErr == EAGAIN)
		return QTSS_WouldBlock;
	else if (theErr > 0)
		return QTSS_NotConnected;
	else
		return theErr;
}

QTSS_Error  QTSSCallbacks::QTSS_Read(QTSS_StreamRef inStream, void* ioBuffer, uint32_t inBufLen, uint32_t* outLengthRead)
{
	if ((inStream == nullptr) || (ioBuffer == nullptr))
		return QTSS_BadArgument;
	QTSS_Error theErr = ((QTSSStream*)inStream)->Read(ioBuffer, inBufLen, outLengthRead);

	// Server internally propogates POSIX errorcodes such as EAGAIN and ENOTCONN up to this
	// level. The API guarentees that no POSIX errors get returned, so we have QTSS_Errors
	// to replace them. So we have to replace them here.
	if (theErr == EAGAIN)
		return QTSS_WouldBlock;
	else if (theErr > 0)
		return QTSS_NotConnected;
	else
		return theErr;
}

QTSS_Error  QTSSCallbacks::QTSS_OpenFileObject(char* inPath, QTSS_OpenFileFlags inFlags, QTSS_Object* outFileObject)
{
	if ((inPath == nullptr) || (outFileObject == nullptr))
		return QTSS_BadArgument;

	//
	// Create a new file object
	auto* theNewFile = new QTSSFile();
	QTSS_Error theErr = theNewFile->Open(inPath, inFlags);

	if (theErr != QTSS_NoErr)
		delete theNewFile; // No module wanted to open the file.
	else
		*outFileObject = theNewFile;

	return theErr;
}

QTSS_Error  QTSSCallbacks::QTSS_CloseFileObject(QTSS_Object inFileObject)
{
	if (inFileObject == nullptr)
		return QTSS_BadArgument;

	auto* theFile = (QTSSFile*)inFileObject;

	theFile->Close();
	delete theFile;
	return QTSS_NoErr;
}

QTSS_Error  QTSSCallbacks::QTSS_AddService(const char* inServiceName, QTSS_ServiceFunctionPtr inFunctionPtr)
{
	auto* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
	if (OSThread::GetCurrent() != nullptr)
		theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();

	// This may happen if this callback is occurring on module-created thread
	if (theState == nullptr)
		return QTSS_OutOfState;

	// Roles can only be added before modules have had their Initialize role invoked.
	if (theState->curRole != QTSS_Register_Role)
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

QTSS_Error  QTSSCallbacks::QTSS_RequestEvent(QTSS_StreamRef inStream, QTSS_EventType inEventMask)
{
	// First thing to do is to alter the thread's module state to reflect the fact
	// that an event is outstanding.
	auto* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
	if (OSThread::GetCurrent() != nullptr)
		theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();

	if (theState == nullptr)
		return QTSS_RequestFailed;

	if (theState->curTask == nullptr)
		return QTSS_OutOfState;

	theState->eventRequested = true;

	// Now, tell this stream to be ready for the requested event
	auto* theStream = (QTSSStream*)inStream;
	theStream->SetTask(theState->curTask);
	theStream->RequestEvent(inEventMask);
	return QTSS_NoErr;
}

QTSS_Error  QTSSCallbacks::QTSS_SignalStream(QTSS_StreamRef inStream)
{
	if (inStream == nullptr)
		return QTSS_BadArgument;

	auto* theStream = (QTSSStream*)inStream;
	if (theStream->GetTask() != nullptr)
		theStream->GetTask()->Signal(Task::kReadEvent);
	return QTSS_NoErr;
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

	theState->eventRequested = true;
	theState->idleTime = inMsecToWait;
	return QTSS_NoErr;
}

QTSS_Error  QTSSCallbacks::QTSS_SetIdleRoleTimer(int64_t inMsecToWait)
{

	auto* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
	if (OSThread::GetCurrent() != nullptr)
		theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();

	// This may happen if this callback is occurring on module-created thread
	if (theState == nullptr)
		return QTSS_RequestFailed;

	if (theState->curModule == nullptr)
		return QTSS_RequestFailed;


	QTSSModule* theModule = theState->curModule;
	QTSS_ModuleState* thePrivateModuleState = theModule->GetModuleState();
	thePrivateModuleState->idleTime = inMsecToWait;
	theModule->Signal(Task::kUpdateEvent);


	return QTSS_NoErr;
}

QTSS_Error  QTSSCallbacks::QTSS_RequestLockedCallback()
{
	auto* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
	if (OSThread::GetCurrent() != nullptr)
		theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();

	// This may happen if this callback is occurring on module-created thread
	if (theState == nullptr)
		return QTSS_RequestFailed;

	if (theState->curTask == nullptr)
		return QTSS_OutOfState;

	theState->globalLockRequested = true; //x

	return QTSS_NoErr;
}

bool      QTSSCallbacks::QTSS_IsGlobalLocked()
{
	auto* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
	if (OSThread::GetCurrent() != nullptr)
		theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();

	// This may happen if this callback is occurring on module-created thread
	if (theState == nullptr)
		return false;

	if (theState->curTask == nullptr)
		return false;

	return theState->isGlobalLocked;
}

QTSS_Error  QTSSCallbacks::QTSS_Authenticate(const char* inAuthUserName, const char* inAuthResourceLocalPath, const char* inAuthMoviesDir, QTSS_ActionFlags inAuthRequestAction, QTSS_AuthScheme inAuthScheme, RTSPRequest* ioAuthRequestObject)
{
	if ((inAuthUserName == nullptr) || (inAuthResourceLocalPath == nullptr) || (inAuthMoviesDir == nullptr) || (ioAuthRequestObject == nullptr))
		return QTSS_BadArgument;
	if (inAuthRequestAction == qtssActionFlagsNoFlags)
		return QTSS_BadArgument;
	if (inAuthScheme == qtssAuthNone)
		return QTSS_BadArgument;

	// First create a RTSPRequestInterface object 
	// There is no session attached to it, so just pass in NULL for the RTSPSession
	// Set all the attributes required by the authentication module, using the input values
	ioAuthRequestObject->SetAuthUserName({ inAuthUserName, ::strlen(inAuthUserName) });
	ioAuthRequestObject->SetLocalPath({ inAuthResourceLocalPath, ::strlen(inAuthResourceLocalPath) });
	ioAuthRequestObject->SetRootDir({ inAuthMoviesDir, ::strlen(inAuthMoviesDir) });
	ioAuthRequestObject->SetAction(inAuthRequestAction);
	ioAuthRequestObject->SetAuthScheme(inAuthScheme);
	QTSSUserProfile *profile = ioAuthRequestObject->GetUserProfile();
	(void)profile->SetValue(qtssUserName, 0, inAuthUserName, ::strlen(inAuthUserName), QTSSDictionary::kDontObeyReadOnly);


	// Because this is a role being executed from inside a callback, we need to
	// make sure that QTSS_RequestEvent will not work.
	Task* curTask = nullptr;
	auto* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
	if (OSThread::GetCurrent() != nullptr)
		theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();

	if (theState != nullptr)
		curTask = theState->curTask;

	// Setup the authentication param block
	QTSS_RoleParams theAuthenticationParams;
	//theAuthenticationParams.rtspAthnParams.inRTSPRequest = request;

	QTSS_Error theErr = QTSS_RequestFailed;

	bool allowedDefault = QTSServerInterface::GetServer()->GetPrefs()->GetAllowGuestDefault();
	bool allowed = allowedDefault; //server pref?
	bool hasUser = false;
	bool handled = false;


	// Call all the modules that are registered for the RTSP Authorize Role 
	for (const auto &theModulePtr : QTSServerInterface::GetModule(QTSSModule::kRTSPAthnRole))
	{
		ioAuthRequestObject->SetAllowed(allowedDefault);
		ioAuthRequestObject->SetHasUser(false);
		ioAuthRequestObject->SetAuthHandled(false);

		theErr = QTSS_NoErr;
		if (theModulePtr)
		{
			theErr = theModulePtr->CallDispatch(QTSS_RTSPAuthenticate_Role, &theAuthenticationParams);
		}
		else
		{
			continue;
		}
		allowed = ioAuthRequestObject->GetAllowed();
		hasUser = ioAuthRequestObject->GetHasUser();
		handled = ioAuthRequestObject->GetAuthHandled();
		debug_printf("QTSSCallbacks::QTSS_Authenticate allowedDefault =%d allowed= %d hasUser = %d handled=%d \n", allowedDefault, allowed, hasUser, handled);


		if (hasUser || handled) //See RTSPSession.cpp::Run state=kAuthenticatingRequest
		{
			break;
		}
	}


	// Reset the curTask to what it was before this role started
	if (theState != nullptr)
		theState->curTask = curTask;

	return theErr;
}

QTSS_Error	QTSSCallbacks::QTSS_Authorize(RTSPRequest* inAuthRequestObject, std::string* outAuthRealm, bool* outAuthUserAllowed)
{
	if (inAuthRequestObject == nullptr)
		return QTSS_BadArgument;

	// Because this is a role being executed from inside a callback, we need to
	// make sure that QTSS_RequestEvent will not work.
	Task* curTask = nullptr;
	auto* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
	if (OSThread::GetCurrent() != nullptr)
		theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();

	if (theState != nullptr)
		curTask = theState->curTask;

	QTSS_RoleParams theParams;
	theParams.rtspRequestParams.inRTSPSession = nullptr;
	theParams.rtspRequestParams.inRTSPRequest = inAuthRequestObject;
	theParams.rtspRequestParams.inClientSession = nullptr;

	QTSS_Error theErr = QTSS_RequestFailed;
	bool 		allowedDefault = QTSServerInterface::GetServer()->GetPrefs()->GetAllowGuestDefault();
	*outAuthUserAllowed = allowedDefault;
	bool      allowed = allowedDefault; //server pref?
	bool      hasUser = false;
	bool      handled = false;


	// Call all the modules that are registered for the RTSP Authorize Role 

	for (const auto &theModulePtr : QTSServerInterface::GetModule(QTSSModule::kRTSPAuthRole))
	{
		inAuthRequestObject->SetAllowed(true);
		inAuthRequestObject->SetHasUser(false);
		inAuthRequestObject->SetAuthHandled(false);

		theErr = QTSS_NoErr;
		if (theModulePtr)
		{
			if (__QTSSCALLBACKS_DEBUG__)
				theModulePtr->GetValue(qtssModName)->PrintStr("QTSSModule::CallDispatch ENTER module=", "\n");

			theErr = theModulePtr->CallDispatch(QTSS_RTSPAuthorize_Role, &theParams);
		}
		else
		{
			continue;
		}

		allowed = inAuthRequestObject->GetAllowed();
		hasUser = inAuthRequestObject->GetHasUser();
		handled = inAuthRequestObject->GetAuthHandled();
		debug_printf("QTSSCallbacks::QTSS_Authorize allowedDefault =%d allowed= %d hasUser = %d handled=%d \n", allowedDefault, allowed, hasUser, handled);

		*outAuthUserAllowed = allowed;
		//notes:
		//if (allowed && !handled)  break; //old module               
		//if (!allowed && handled) /new module handled the request but not authorized keep trying
		//if (allowed && handled) //new module allowed but keep trying in case someone denies.

		if (!allowed && !handled)  //old module break on !allowed
		{
			break;
		}
	}

	// outAuthRealm is set to the realm that is given by the module that has denied authentication
	*outAuthRealm = std::string(inAuthRequestObject->GetURLRealm());

	return theErr;
}

void* QTSSCallbacks::Easy_GetRTSPPushSessions()
{
	OSRefTable* reflectorSessionMap = QTSServerInterface::GetServer()->GetReflectorSessionMap();

	EasyMsgSCRTSPLiveSessionsACK ack;
	ack.SetHeaderValue(EASY_TAG_VERSION, "1.0");
	ack.SetHeaderValue(EASY_TAG_CSEQ, "1");

	uint32_t uIndex = 0;
	OSMutexLocker locker(reflectorSessionMap->GetMutex());

	for (OSRefHashTableIter theIter(reflectorSessionMap->GetHashTable()); !theIter.IsDone(); theIter.Next())
	{
		OSRef* theRef = theIter.GetCurrent();
		auto* theSession = (ReflectorSession*)theRef->GetObject();

		EasyDarwinRTSPSession session;
		session.index = uIndex;

		auto* clientSession = (RTPSession*)theSession->GetBroadcasterSession();

		if (clientSession == nullptr) continue;

		session.Url = std::string(clientSession->GetAbsoluteURL());
		session.Name = theSession->GetStreamName().data();
		session.numOutputs = theSession->GetNumOutputs();
		session.channel = theSession->GetChannelNum();
		ack.AddSession(session);
		uIndex++;
	}

	char count[16] = { 0 };
	sprintf(count, "%d", uIndex);
	ack.SetBodyValue(EASY_TAG_SESSION_COUNT, count);

	string msg = ack.GetMsg();

	uint32_t theMsgLen = strlen(msg.c_str());
	auto* retMsg = new char[theMsgLen + 1];
	retMsg[theMsgLen] = '\0';
	strncpy(retMsg, msg.c_str(), strlen(msg.c_str()));
	return (void*)retMsg;
}


//void *QTSSCallbacks::Easy_GetRTSPRecordSessions(char* inSessionName, uint64_t startTime, uint64_t endTime) 
//{
//	return nullptr;
	//char * rootdir = RTSPRecordSession::getNetRecordRootPath();

	//EasyMsgSCRecordList ack;
	//ack.SetHeaderValue(EASY_TAG_VERSION, "1.0");
	//ack.SetHeaderValue(EASY_TAG_CSEQ, "1");

	//char folder[QTSS_MAX_NAME_LENGTH] = { 0 };
	////char movieFolder[QTSS_MAX_NAME_LENGTH] = { 0 };
	////uint32_t pathLen = QTSS_MAX_NAME_LENGTH;
	////QTSServerInterface::GetServer()->GetPrefs()->GetMovieFolder(&movieFolder[0], &pathLen);

	//char *movieFolder = RTSPRecordSession::getRecordRootPath();

	//char httpRoot[QTSS_MAX_NAME_LENGTH] = { 0 };

	//sprintf(httpRoot, "%sMP4/%s/", rootdir, inSessionName);

	//char subDir[QTSS_MAX_NAME_LENGTH] = { 0 };
	//sprintf(subDir, "%s/", inSessionName);

	////char rootDir[QTSS_MAX_NAME_LENGTH] = { 0 };
	////sprintf(rootDir,"%s/", movieFolder);
	//sprintf(folder, "%sMP4/%s/", movieFolder, subDir);
	//vector<FileAttributeInfo> list;

	//vector<string> machList;
	//machList.push_back(".mp4");
	//SearchFileDir *dir = SearchFileDir::getInstance();
	//uint32_t uIndex = 0;
	//if (dir->searchFileList(folder, list, machList, false)) {

	//	for (auto iter : list) {
	//		if (iter.file_info.st_mtime >= startTime && iter.file_info.st_ctime <= endTime) {
	//			EasyDarwinRecordSession session;
	//			session.index = uIndex;
	//			session.Name = httpRoot + iter.fileName;
	//			session.startTime = iter.file_info.st_ctime;
	//			session.endTime = iter.file_info.st_mtime;
	//			ack.AddRecord(session);
	//			uIndex++;
	//		}
	//	}
	//}

	//char count[16] = { 0 };
	//sprintf(count, "%d", uIndex);
	//ack.SetBodyValue(EASY_TAG_SESSION_COUNT, count);

	//string msg = ack.GetMsg();

	//uint32_t theMsgLen = strlen(msg.c_str());
	//char* retMsg = new char[theMsgLen + 1];
	//retMsg[theMsgLen] = '\0';
	//strncpy(retMsg, msg.c_str(), strlen(msg.c_str()));
	//return (void*)retMsg;
//}