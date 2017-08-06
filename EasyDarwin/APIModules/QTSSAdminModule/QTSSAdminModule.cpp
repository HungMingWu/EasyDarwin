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
	 Copyright (c) 2013-2016 EasyDarwin.ORG.  All rights reserved.
	 Github: https://github.com/EasyDarwin
	 WEChat: EasyDarwin
	 Website: http://www.easydarwin.org
 */
 /*
	 File:       QTSSAdminModule.cpp
	 Contains:   Implements Admin module
 */

#include <string.h>

#ifdef _WIN32
#include <direct.h>
#endif // _WIN32

#include <memory>

#ifndef __Win32__
#include <unistd.h>     /* for getopt() et al */
#endif

#include <time.h>
#include <stdio.h>      /* for printf */
#include <stdlib.h>     /* for getloadavg & other useful stuff */
#include "QTSSAdminModule.h"
#include "StringParser.h"
#include "StrPtrLen.h"
#include "QTSSModuleUtils.h"
#include "OSMutex.h"
#include "OSRef.h"
#include "AdminElementNode.h"
#include "base64.h"
#include "md5digest.h"
#include "OS.h"
#include "RTSPRequest.h"
#include "RTSPSession.h"
#include "QTSServer.h"
#if __MacOSX__
#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>
#endif

#if __solaris__ || __linux__ || __sgi__ || __hpux__
#include <crypt.h>
#endif

#define DEBUG_ADMIN_MODULE 0

//**************************************************
#define kAuthNameAndPasswordBuffSize 512
#define kPasswordBuffSize kAuthNameAndPasswordBuffSize/2

// STATIC DATA
//**************************************************
#if DEBUG_ADMIN_MODULE
static uint32_t	sRequestCount = 0;
#endif

static QTSS_Initialize_Params sQTSSparams;

//static char* sResponseHeader = "HTTP/1.0 200 OK\r\nServer: QTSS\r\nConnection: Close\r\nContent-Type: text/plain\r\n\r\n";
static char* sResponseHeader = "HTTP/1.0 200 OK";
static char* sUnauthorizedResponseHeader = "HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"QTSS/modules/admin\"\r\nServer: QTSS\r\nConnection: Close\r\nContent-Type: text/plain\r\n\r\n";
static char* sPermissionDeniedHeader = "HTTP/1.1 403 Forbidden\r\nConnection: Close\r\nContent-Type: text/html\r\n\r\n";
static char* sHTMLBody = "<HTML><BODY>\n<P><b>Your request was denied by the server.</b></P>\n</BODY></HTML>\r\n\r\n";

static char* sVersionHeader = nullptr;
static char* sConnectionHeader = "Connection: Close";
static char* kDefaultHeader = "Server: EasyDarwin";
static char* sContentType = "Content-Type: text/plain";
static char* sEOL = "\r\n";
static char* sEOM = "\r\n\r\n";
static char* sAuthRealm = "QTSS/modules/admin";
static std::string sAuthResourceLocalPath = "/modules/admin/";

static QTSS_ServerObject        sServer = nullptr;
static QTSS_ModuleObject        sModule = nullptr;
static QTSS_ModulePrefsObject   sAdminPrefs = nullptr;
static QTSS_ModulePrefsObject   sAccessLogPrefs = nullptr;
static QTSS_ModulePrefsObject   sReflectorPrefs = nullptr;
static QTSS_ModulePrefsObject	sHLSModulePrefs = nullptr;

static QTSS_PrefsObject         sServerPrefs = nullptr;
static AdminClass               *sAdminPtr = nullptr;
static QueryURI                 *sQueryPtr = nullptr;
static OSMutex*                 sAdminMutex = nullptr;//admin module isn't reentrant
static uint32_t                   sVersion = 20030306;
static char *sDesc = "Implements HTTP based Admin Protocol for accessing server attributes";
static char decodedLine[kAuthNameAndPasswordBuffSize] = { 0 };
static char codedLine[kAuthNameAndPasswordBuffSize] = { 0 };
static QTSS_TimeVal             sLastRequestTime = 0;
static uint32_t                   sSessID = 0;

static StrPtrLen            sAuthRef("AuthRef");
#if __MacOSX__

static char*                sSecurityServerAuthKey = "com.apple.server.admin.streaming";
static AuthorizationItem    sRight = { sSecurityServerAuthKey, 0, NULL, 0 };
static AuthorizationRights  sRightSet = { 1, &sRight };
#endif

//ATTRIBUTES
//**************************************************
enum
{
	kMaxRequestTimeIntervalMilli = 1000,
	kDefaultRequestTimeIntervalMilli = 50
};
static uint32_t sDefaultRequestTimeIntervalMilli = kDefaultRequestTimeIntervalMilli;
static uint32_t sRequestTimeIntervalMilli = kDefaultRequestTimeIntervalMilli;

static bool sAuthenticationEnabled = true;
static bool sDefaultAuthenticationEnabled = true;

static bool sLocalLoopBackOnlyEnabled = true;
static bool sDefaultLocalLoopBackOnlyEnabled = true;

static bool sEnableRemoteAdmin = true;
static bool sDefaultEnableRemoteAdmin = true;

static QTSS_AttributeID sIPAccessListID = qtssIllegalAttrID;
static char*            sIPAccessList = nullptr;
static char*            sLocalLoopBackAddress = "127.0.0.*";

static char*            sAdministratorGroup = nullptr;
static char*            sDefaultAdministratorGroup = "admin";

static bool           sFlushing = false;
static QTSS_AttributeID sFlushingID = qtssIllegalAttrID;
static char*            sFlushingName = "QTSSAdminModuleFlushingState";
static uint32_t           sFlushingLen = sizeof(sFlushing);

static QTSS_AttributeID sAuthenticatedID = qtssIllegalAttrID;
static char*            sAuthenticatedName = "QTSSAdminModuleAuthenticatedState";

static QTSS_Error QTSSAdminModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParams);
static QTSS_Error Register(QTSS_Register_Params* inParams);
static QTSS_Error Initialize(QTSS_Initialize_Params* inParams);
static QTSS_Error FilterRequest(QTSS_Filter_Params* inParams);
static QTSS_Error RereadPrefs();
static QTSS_Error AuthorizeAdminRequest(QTSS_RTSPRequestObject request);
static bool AcceptSession(QTSS_RTSPSessionObject inRTSPSession);

#if !DEBUG_ADMIN_MODULE
#define APITests_DEBUG() 
#define ShowQuery_DEBUG()
#else
void ShowQuery_DEBUG()
{
	printf("======REQUEST #%"   _U32BITARG_   "======\n", ++sRequestCount);
	StrPtrLen*  aStr;
	aStr = sQueryPtr->GetURL();
	printf("URL="); PRINT_STR(aStr);

	aStr = sQueryPtr->GetQuery();
	printf("Query="); PRINT_STR(aStr);

	aStr = sQueryPtr->GetParameters();
	printf("Parameters="); PRINT_STR(aStr);

	aStr = sQueryPtr->GetCommand();
	printf("Command="); PRINT_STR(aStr);
	printf("CommandID=%" _S32BITARG_ " \n", sQueryPtr->GetCommandID());
	aStr = sQueryPtr->GetValue();
	printf("Value="); PRINT_STR(aStr);
	aStr = sQueryPtr->GetType();
	printf("Type="); PRINT_STR(aStr);
	aStr = sQueryPtr->GetAccess();
	printf("Access="); PRINT_STR(aStr);
}

void APITests_DEBUG()
{
	if (0)
	{
		printf("QTSSAdminModule start tests \n");

		if (0)
		{
			printf("admin called locked \n");
			const int ksleeptime = 15;
			printf("sleeping for %d seconds \n", ksleeptime);
			sleep(ksleeptime);
			printf("done sleeping \n");
			printf("QTSS_GlobalUnLock \n");
			(void)QTSS_GlobalUnLock();
			printf("again sleeping for %d seconds \n", ksleeptime);
			sleep(ksleeptime);
		}

		if (0)
		{
			printf(" GET VALUE PTR TEST \n");

			QTSS_Object *sessionsPtr = NULL;
			uint32_t      paramLen = sizeof(sessionsPtr);
			uint32_t      numValues = 0;
			QTSS_Error  err = 0;

			err = QTSS_GetNumValues(sServer, qtssSvrClientSessions, &numValues);
			err = QTSS_GetValuePtr(sServer, qtssSvrClientSessions, 0, (void**)&sessionsPtr, &paramLen);
			printf("Admin Module Num Sessions = %"   _U32BITARG_   " sessions[0] = %" _S32BITARG_ " err = %" _S32BITARG_ " paramLen =%"   _U32BITARG_   "\n", numValues, (int32_t)*sessionsPtr, err, paramLen);

			uint32_t      numAttr = 0;
			if (sessionsPtr)
			{
				err = QTSS_GetNumAttributes(*sessionsPtr, &numAttr);
				printf("Admin Module Num attributes = %"   _U32BITARG_   " sessions[0] = %" _S32BITARG_ "  err = %" _S32BITARG_ "\n", numAttr, (int32_t)*sessionsPtr, err);

				QTSS_Object theAttributeInfo;
				char nameBuff[128];
				uint32_t len = 127;
				for (uint32_t i = 0; i < numAttr; i++)
				{
					err = QTSS_GetAttrInfoByIndex(*sessionsPtr, i, &theAttributeInfo);
					nameBuff[0] = 0; len = 127;
					err = QTSS_GetValue(theAttributeInfo, qtssAttrName, 0, nameBuff, &len);
					nameBuff[len] = 0;
					printf("found %s \n", nameBuff);
				}
			}
		}

		if (0)
		{
			printf(" GET VALUE TEST \n");

			QTSS_Object sessions = NULL;
			uint32_t      paramLen = sizeof(sessions);
			uint32_t      numValues = 0;
			QTSS_Error  err = 0;

			err = QTSS_GetNumValues(sServer, qtssSvrClientSessions, &numValues);
			err = QTSS_GetValue(sServer, qtssSvrClientSessions, 0, (void*)&sessions, &paramLen);
			printf("Admin Module Num Sessions = %"   _U32BITARG_   " sessions[0] = %" _S32BITARG_ " err = %" _S32BITARG_ " paramLen = %"   _U32BITARG_   "\n", numValues, (int32_t)sessions, err, paramLen);

			if (sessions)
			{
				uint32_t      numAttr = 0;
				err = QTSS_GetNumAttributes(sessions, &numAttr);
				printf("Admin Module Num attributes = %"   _U32BITARG_   " sessions[0] = %" _S32BITARG_ "  err = %" _S32BITARG_ "\n", numAttr, (int32_t)sessions, err);

				QTSS_Object theAttributeInfo;
				char nameBuff[128];
				uint32_t len = 127;
				for (uint32_t i = 0; i < numAttr; i++)
				{
					err = QTSS_GetAttrInfoByIndex(sessions, i, &theAttributeInfo);
					nameBuff[0] = 0; len = 127;
					err = QTSS_GetValue(theAttributeInfo, qtssAttrName, 0, nameBuff, &len);
					nameBuff[len] = 0;
					printf("found %s \n", nameBuff);
				}
			}
		}


		if (0)
		{
			printf("----------------- Start test ----------------- \n");
			printf(" GET indexed pref TEST \n");

			QTSS_Error  err = 0;

			uint32_t      numAttr = 1;
			err = QTSS_GetNumAttributes(sAdminPrefs, &numAttr);
			printf("Admin Module Num preference attributes = %"   _U32BITARG_   " err = %" _S32BITARG_ "\n", numAttr, err);

			QTSS_Object theAttributeInfo;
			char valueBuff[512];
			char nameBuff[128];
			QTSS_AttributeID theID;
			uint32_t len = 127;
			uint32_t i = 0;
			printf("first pass over preferences\n");
			for (i = 0; i < numAttr; i++)
			{
				err = QTSS_GetAttrInfoByIndex(sAdminPrefs, i, &theAttributeInfo);
				nameBuff[0] = 0; len = 127;
				err = QTSS_GetValue(theAttributeInfo, qtssAttrName, 0, nameBuff, &len);
				nameBuff[len] = 0;

				theID = qtssIllegalAttrID; len = sizeof(theID);
				err = QTSS_GetValue(theAttributeInfo, qtssAttrID, 0, &theID, &len);
				printf("found preference=%s \n", nameBuff);
			}
			valueBuff[0] = 0; len = 512;
			err = QTSS_GetValue(sAdminPrefs, theID, 0, valueBuff, &len); valueBuff[len] = 0;
			printf("Admin Module QTSS_GetValue name = %s id = %" _S32BITARG_ " value=%s err = %" _S32BITARG_ "\n", nameBuff, theID, valueBuff, err);
			err = QTSS_SetValue(sAdminPrefs, theID, 0, valueBuff, len);
			printf("Admin Module QTSS_SetValue name = %s id = %" _S32BITARG_ " value=%s err = %" _S32BITARG_ "\n", nameBuff, theID, valueBuff, err);

			{   QTSS_ServiceID id;
			(void)QTSS_IDForService(QTSS_REREAD_PREFS_SERVICE, &id);
			(void)QTSS_DoService(id, NULL);
			}

			valueBuff[0] = 0; len = 512;
			err = QTSS_GetValue(sAdminPrefs, theID, 0, valueBuff, &len); valueBuff[len] = 0;
			printf("Admin Module QTSS_GetValue name = %s id = %" _S32BITARG_ " value=%s err = %" _S32BITARG_ "\n", nameBuff, theID, valueBuff, err);
			err = QTSS_SetValue(sAdminPrefs, theID, 0, valueBuff, len);
			printf("Admin Module QTSS_SetValue name = %s id = %" _S32BITARG_ " value=%s err = %" _S32BITARG_ "\n", nameBuff, theID, valueBuff, err);

			printf("second pass over preferences\n");
			for (i = 0; i < numAttr; i++)
			{
				err = QTSS_GetAttrInfoByIndex(sAdminPrefs, i, &theAttributeInfo);
				nameBuff[0] = 0; len = 127;
				err = QTSS_GetValue(theAttributeInfo, qtssAttrName, 0, nameBuff, &len);
				nameBuff[len] = 0;

				theID = qtssIllegalAttrID; len = sizeof(theID);
				err = QTSS_GetValue(theAttributeInfo, qtssAttrID, 0, &theID, &len);
				printf("found preference=%s \n", nameBuff);
			}
			printf("----------------- Done test ----------------- \n");
		}

	}
}

#endif

inline void KeepSession(QTSS_RTSPRequestObject theRequest, bool keep)
{
	(void)QTSS_SetValue(theRequest, qtssRTSPReqRespKeepAlive, 0, &keep, sizeof(keep));
}

// FUNCTION IMPLEMENTATIONS

QTSS_Error QTSSAdminModule_Main(void* inPrivateArgs)
{
	return _stublibrary_main(inPrivateArgs, QTSSAdminModuleDispatch);
}


QTSS_Error  QTSSAdminModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParams)
{
	switch (inRole)
	{
	case QTSS_Register_Role:
		return Register(&inParams->regParams);
	case QTSS_Initialize_Role:
		return Initialize(&inParams->initParams);
	case QTSS_RTSPFilter_Role:
		{
			if (!sEnableRemoteAdmin)
				break;
			return FilterRequest(&inParams->rtspFilterParams);
		}
	case QTSS_RTSPAuthorize_Role:
		return AuthorizeAdminRequest(inParams->rtspRequestParams.inRTSPRequest);
	case QTSS_RereadPrefs_Role:
		return RereadPrefs();
	}
	return QTSS_NoErr;
}

QTSS_Error Register(QTSS_Register_Params* inParams)
{
	// Do role & attribute setup
	(void)QTSS_AddRole(QTSS_Initialize_Role);
	(void)QTSS_AddRole(QTSS_RTSPFilter_Role);
	(void)QTSS_AddRole(QTSS_RereadPrefs_Role);
	(void)QTSS_AddRole(QTSS_RTSPAuthorize_Role);

	(void)QTSS_AddStaticAttribute(qtssRTSPRequestObjectType, sFlushingName, nullptr, qtssAttrDataTypeBool16);
	(void)QTSS_IDForAttr(qtssRTSPRequestObjectType, sFlushingName, &sFlushingID);

	(void)QTSS_AddStaticAttribute(qtssRTSPRequestObjectType, sAuthenticatedName, nullptr, qtssAttrDataTypeBool16);
	(void)QTSS_IDForAttr(qtssRTSPRequestObjectType, sAuthenticatedName, &sAuthenticatedID);

	// Tell the server our name!
	static char* sModuleName = "QTSSAdminModule";
	::strcpy(inParams->outModuleName, sModuleName);

	return QTSS_NoErr;
}

QTSS_Error RereadPrefs()
{

	delete[] sVersionHeader;
	sVersionHeader = QTSSModuleUtils::GetStringAttribute(sServer, "qtssSvrRTSPServerHeader", kDefaultHeader);

	delete[] sIPAccessList;
	sIPAccessList = QTSSModuleUtils::GetStringAttribute(sAdminPrefs, "IPAccessList", sLocalLoopBackAddress);
	sIPAccessListID = QTSSModuleUtils::GetAttrID(sAdminPrefs, "IPAccessList");

	QTSSModuleUtils::GetAttribute(sAdminPrefs, "Authenticate", qtssAttrDataTypeBool16, &sAuthenticationEnabled, &sDefaultAuthenticationEnabled, sizeof(sAuthenticationEnabled));
	QTSSModuleUtils::GetAttribute(sAdminPrefs, "LocalAccessOnly", qtssAttrDataTypeBool16, &sLocalLoopBackOnlyEnabled, &sDefaultLocalLoopBackOnlyEnabled, sizeof(sLocalLoopBackOnlyEnabled));
	QTSSModuleUtils::GetAttribute(sAdminPrefs, "RequestTimeIntervalMilli", qtssAttrDataTypeUInt32, &sRequestTimeIntervalMilli, &sDefaultRequestTimeIntervalMilli, sizeof(sRequestTimeIntervalMilli));
	QTSSModuleUtils::GetAttribute(sAdminPrefs, "enable_remote_admin", qtssAttrDataTypeBool16, &sEnableRemoteAdmin, &sDefaultEnableRemoteAdmin, sizeof(sDefaultEnableRemoteAdmin));

	delete[] sAdministratorGroup;
	sAdministratorGroup = QTSSModuleUtils::GetStringAttribute(sAdminPrefs, "AdministratorGroup", sDefaultAdministratorGroup);

	if (sRequestTimeIntervalMilli > kMaxRequestTimeIntervalMilli)
	{
		sRequestTimeIntervalMilli = kMaxRequestTimeIntervalMilli;
	}

	(void)QTSS_SetValue(sModule, qtssModDesc, 0, sDesc, strlen(sDesc) + 1);
	(void)QTSS_SetValue(sModule, qtssModVersion, 0, &sVersion, sizeof(sVersion));

	return QTSS_NoErr;
}

QTSS_Error Initialize(QTSS_Initialize_Params* inParams)
{
	sAdminMutex = new OSMutex();
	ElementNode_InitPtrArray();
	// Setup module utils
	QTSSModuleUtils::Initialize(inParams->inMessages, inParams->inServer, inParams->inErrorLogStream);

	sQTSSparams = *inParams;
	sServer = inParams->inServer;
	sModule = inParams->inModule;

	sAccessLogPrefs = QTSSModuleUtils::GetModulePrefsObject(QTSSModuleUtils::GetModuleObjectByName("QTSSAccessLogModule"));
	sReflectorPrefs = QTSSModuleUtils::GetModulePrefsObject(QTSSModuleUtils::GetModuleObjectByName("QTSSReflectorModule"));
	sHLSModulePrefs = QTSSModuleUtils::GetModulePrefsObject(QTSSModuleUtils::GetModuleObjectByName("EasyHLSModule"));

	sAdminPrefs = QTSSModuleUtils::GetModulePrefsObject(sModule);
	sServerPrefs = inParams->inPrefs;

	RereadPrefs();

	return QTSS_NoErr;
}

void ReportErr(QTSS_Filter_Params* inParams, uint32_t err)
{
	StrPtrLen* urlPtr = sQueryPtr->GetURL();
	StrPtrLen* evalMessagePtr = sQueryPtr->GetEvalMsg();
	char temp[32];

	if (urlPtr && evalMessagePtr)
	{
		sprintf(temp, "(%"   _U32BITARG_   ")", err);
		(void)QTSS_Write(inParams->inRTSPRequest, "error:", strlen("error:"), nullptr, 0);
		(void)QTSS_Write(inParams->inRTSPRequest, temp, strlen(temp), nullptr, 0);
		if (sQueryPtr->VerboseParam())
		{
			(void)QTSS_Write(inParams->inRTSPRequest, ";URL=", strlen(";URL="), nullptr, 0);
			if (urlPtr) (void)QTSS_Write(inParams->inRTSPRequest, urlPtr->Ptr, urlPtr->Len, nullptr, 0);
		}
		if (sQueryPtr->DebugParam())
		{
			(void)QTSS_Write(inParams->inRTSPRequest, ";", strlen(";"), nullptr, 0);
			(void)QTSS_Write(inParams->inRTSPRequest, evalMessagePtr->Ptr, evalMessagePtr->Len, nullptr, 0);
		}
		(void)QTSS_Write(inParams->inRTSPRequest, "\r\n\r\n", 4, nullptr, 0);
	}
}


inline bool AcceptAddress(StrPtrLen *theAddressPtr)
{
	IPComponentStr ipComponentStr(theAddressPtr);

	bool isLocalRequest = ipComponentStr.IsLocal();
	if (sLocalLoopBackOnlyEnabled && isLocalRequest)
		return true;

	if (sLocalLoopBackOnlyEnabled && !isLocalRequest)
		return false;

	if (QTSSModuleUtils::AddressInList(sAdminPrefs, sIPAccessListID, theAddressPtr))
		return true;

	return false;
}

inline bool IsAdminRequest(StringParser *theFullRequestPtr)
{
	bool handleRequest = false;
	if (theFullRequestPtr != nullptr) do
	{
		StrPtrLen   strPtr;
		theFullRequestPtr->ConsumeWord(&strPtr);
		if (!strPtr.Equal(StrPtrLen("GET"))) break;   //it's a "Get" request

		theFullRequestPtr->ConsumeWhitespace();
		if (!theFullRequestPtr->Expect('/')) break;

		theFullRequestPtr->ConsumeWord(&strPtr);
		if (strPtr.Len == 0 || !strPtr.Equal(StrPtrLen("modules"))) break;
		if (!theFullRequestPtr->Expect('/')) break;

		theFullRequestPtr->ConsumeWord(&strPtr);
		if (strPtr.Len == 0 || !strPtr.Equal(StrPtrLen("admin"))) break;
		handleRequest = true;

	} while (false);

	return handleRequest;
}

inline void ParseAuthNameAndPassword(StrPtrLen *codedStrPtr, StrPtrLen* namePtr, StrPtrLen* passwordPtr)
{
	if (!codedStrPtr || (codedStrPtr->Len >= kAuthNameAndPasswordBuffSize))
	{
		return;
	}

	StrPtrLen   codedLineStr;
	StrPtrLen   nameAndPassword;
	memset(decodedLine, 0, kAuthNameAndPasswordBuffSize);
	memset(codedLine, 0, kAuthNameAndPasswordBuffSize);

	memcpy(codedLine, codedStrPtr->Ptr, codedStrPtr->Len);
	codedLineStr.Set((char*)codedLine, codedStrPtr->Len);
	(void)Base64decode(decodedLine, codedLineStr.Ptr);

	nameAndPassword.Set((char*)decodedLine, strlen(decodedLine));
	StringParser parsedNameAndPassword(&nameAndPassword);

	parsedNameAndPassword.ConsumeUntil(namePtr, ':');
	parsedNameAndPassword.ConsumeLength(nullptr, 1);

	// password can have whitespace, so read until the end of the line, not just until whitespace
	parsedNameAndPassword.ConsumeUntil(passwordPtr, StringParser::sEOLMask);

	namePtr->Ptr[namePtr->Len] = 0;
	passwordPtr->Ptr[passwordPtr->Len] = 0;

	//printf("decoded nameAndPassword="); PRINT_STR(&nameAndPassword); 
	//printf("decoded name="); PRINT_STR(namePtr); 
	//printf("decoded password="); PRINT_STR(passwordPtr); 

	return;
};


inline bool OSXAuthenticate(StrPtrLen *keyStrPtr)
{
#if __MacOSX__
	//  Authorization: AuthRef QWxhZGRpbjpvcGVuIHNlc2FtZQ==
	bool result = false;

	if (keyStrPtr == NULL || keyStrPtr->Len == 0)
		return result;

	char *encodedKey = keyStrPtr->GetAsCString();
	std::unique_ptr<char[]> encodedKeyDeleter(encodedKey);

	char *decodedKey = new char[Base64decode_len(encodedKey) + 1];
	std::unique_ptr<char[]> decodedKeyDeleter(decodedKey);

	(void)Base64decode(decodedKey, encodedKey);

	AuthorizationExternalForm  *receivedExtFormPtr = (AuthorizationExternalForm  *)decodedKey;
	AuthorizationRef  receivedAuthorization;
	OSStatus status = AuthorizationCreateFromExternalForm(receivedExtFormPtr, &receivedAuthorization);

	if (status != errAuthorizationSuccess)
		return result;

	status = AuthorizationCopyRights(receivedAuthorization, &sRightSet, kAuthorizationEmptyEnvironment, kAuthorizationFlagExtendRights, NULL);
	if (status == errAuthorizationSuccess)
	{
		result = true;
	}

	AuthorizationFree(receivedAuthorization, kAuthorizationFlagDestroyRights);

	return result;

#else

	return false;

#endif

}

inline bool HasAuthentication(StringParser *theFullRequestPtr, StrPtrLen* namePtr, StrPtrLen* passwordPtr, StrPtrLen* outAuthTypePtr)
{
	//  Authorization: Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==
	bool hasAuthentication = false;
	StrPtrLen   strPtr;
	StrPtrLen   authType;
	StrPtrLen   authString;
	while (theFullRequestPtr->GetDataRemaining() > 0)
	{
		theFullRequestPtr->ConsumeWhitespace();
		theFullRequestPtr->ConsumeUntilWhitespace(&strPtr);
		if (strPtr.Len == 0 || !strPtr.Equal(StrPtrLen("Authorization:")))
			continue;

		theFullRequestPtr->ConsumeWhitespace();
		theFullRequestPtr->ConsumeUntilWhitespace(&authType);
		if (authType.Len == 0)
			continue;

		theFullRequestPtr->ConsumeWhitespace();
		theFullRequestPtr->ConsumeUntil(&authString, StringParser::sEOLMask);
		if (authString.Len == 0)
			continue;

		if (outAuthTypePtr != nullptr)
			outAuthTypePtr->Set(authType.Ptr, authType.Len);

		if (authType.Equal(StrPtrLen("Basic")))
		{
			(void)ParseAuthNameAndPassword(&authString, namePtr, passwordPtr);
			if (namePtr->Len == 0)
				continue;

			hasAuthentication = true;
			break;
		}
		else if (authType.Equal(sAuthRef))
		{
			namePtr->Set(nullptr, 0);
			passwordPtr->Set(authString.Ptr, authString.Len);
			hasAuthentication = true;
			break;
		}
	};

	return hasAuthentication;
}

bool  Authenticate(QTSS_RTSPRequestObject request, StrPtrLen* namePtr, StrPtrLen* passwordPtr)
{
	bool authenticated = true;

	char* authName = namePtr->GetAsCString();
	std::unique_ptr<char[]> authNameDeleter(authName);

	QTSS_ActionFlags authAction = qtssActionFlagsAdmin;

	// authenticate callback to retrieve the password 
	QTSS_Error err = QTSS_Authenticate(authName, sAuthResourceLocalPath.c_str(), 
		sAuthResourceLocalPath.c_str(), authAction, qtssAuthBasic, request);
	if (err != QTSS_NoErr) {
		return false; // Couldn't even call QTSS_Authenticate...abandon!
	}

	// Get the user profile object from the request object that was created in the authenticate callback
	QTSS_UserProfileObject theUserProfile = nullptr;
	uint32_t len = sizeof(QTSS_UserProfileObject);
	err = ((QTSSDictionary*)request)->GetValue(qtssRTSPReqUserProfile, 0, (void*)&theUserProfile, &len);
	Assert(len == sizeof(QTSS_UserProfileObject));
	if (err != QTSS_NoErr)
		authenticated = false;

	if (err == QTSS_NoErr) {
		char* reqPassword = passwordPtr->GetAsCString();
		std::unique_ptr<char[]> reqPasswordDeleter(reqPassword);
		char* userPassword = nullptr;
		(void)((QTSSDictionary*)theUserProfile)->GetValueAsString(qtssUserPassword, 0, &userPassword);
		std::unique_ptr<char[]> userPasswordDeleter(userPassword);

		if (userPassword == nullptr) {
			authenticated = false;
		}
		else {
#ifdef __Win32__
			// The password is md5 encoded for win32
			char md5EncodeResult[120];
			MD5Encode(reqPassword, userPassword, md5EncodeResult, sizeof(md5EncodeResult));
			if (::strcmp(userPassword, md5EncodeResult) != 0)
				authenticated = false;
#else
			if (::strcmp(userPassword, (char*)crypt(reqPassword, userPassword)) != 0)
				authenticated = false;
#endif
		}
	}

	char* realm = nullptr;
	bool allowed = true;
	//authorize callback to check authorization
	// allocates memory for realm
	err = QTSS_Authorize(request, &realm, &allowed);
	// QTSS_Authorize allocates memory for the realm string
	// we don't use the realm returned by the callback, but instead 
	// use our own.
	// delete the memory allocated for realm because we don't need it!
	std::unique_ptr<char[]> realmDeleter(realm);

	if (err != QTSS_NoErr) {
		printf("QTSSAdminModule::Authenticate: QTSS_Authorize failed\n");
		return false; // Couldn't even call QTSS_Authorize...abandon!
	}

	if (authenticated && allowed)
		return true;

	return false;
}


QTSS_Error AuthorizeAdminRequest(QTSS_RTSPRequestObject request)
{
	bool allowed = false;

	// get the resource path
	// if the path does not match the admin path, don't handle the request
	auto* pReq = (RTSPRequest*)request;
	std::string resourcePath(pReq->GetLocalPath());

	if (sAuthResourceLocalPath != resourcePath)
		return QTSS_NoErr;

	// get the type of request
	QTSS_ActionFlags action = QTSSModuleUtils::GetRequestActions(request);
	if (!(action & qtssActionFlagsAdmin))
		return QTSS_RequestFailed;

	QTSS_UserProfileObject theUserProfile = QTSSModuleUtils::GetUserProfileObject(request);
	if (nullptr == theUserProfile)
		return QTSS_RequestFailed;

	(void)QTSS_SetValue(request, qtssRTSPReqURLRealm, 0, sAuthRealm, ::strlen(sAuthRealm));

	// Authorize the user if the user belongs to the AdministratorGroup (this is an admin module pref)
	std::vector<std::string> groupsArray = QTSSModuleUtils::GetGroupsArray_Copy(theUserProfile);

	if (!groupsArray.empty())
	{
		uint32_t index = 0;
		for (index = 0; index < groupsArray.size(); index++)
		{
			if (strcmp(sAdministratorGroup, groupsArray[index].c_str()) == 0)
			{
				allowed = true;
				break;
			}
		}
	}

	if (!allowed)
		pReq->SetUserAllow(allowed);

	return QTSS_NoErr;
}


bool AcceptSession(QTSS_RTSPSessionObject inRTSPSession)
{
	std::string remoteAddress = ((RTSPSession*)inRTSPSession)->GetRemoteAddr();
	StrPtrLen theClientIPAddressStr((char *)remoteAddress.data(), remoteAddress.length());

	return AcceptAddress(&theClientIPAddressStr);
}

bool StillFlushing(QTSS_Filter_Params* inParams, bool flushing)
{

	QTSS_Error err = QTSS_NoErr;
	if (flushing)
	{
		err = QTSS_Flush(inParams->inRTSPRequest);
		//printf("Flushing session=%"   _U32BITARG_   " QTSS_Flush err =%" _S32BITARG_ "\n",sSessID,err); 
	}
	if (err == QTSS_WouldBlock) // more to flush later
	{
		sFlushing = true;
		(void)QTSS_SetValue(inParams->inRTSPRequest, sFlushingID, 0, (void*)&sFlushing, sFlushingLen);
		err = QTSS_RequestEvent(inParams->inRTSPRequest, QTSS_WriteableEvent);
		KeepSession(inParams->inRTSPRequest, true);
		//printf("Flushing session=%"   _U32BITARG_   " QTSS_RequestEvent err =%" _S32BITARG_ "\n",sSessID,err);
	}
	else
	{
		sFlushing = false;
		(void)QTSS_SetValue(inParams->inRTSPRequest, sFlushingID, 0, (void*)&sFlushing, sFlushingLen);
		KeepSession(inParams->inRTSPRequest, false);

		if (flushing) // we were flushing so reset the LastRequestTime
		{
			sLastRequestTime = OS::Milliseconds();
			//printf("Done Flushing session=%"   _U32BITARG_   "\n",sSessID);
			return true;
		}
	}

	return sFlushing;
}

bool IsAuthentic(QTSS_Filter_Params* inParams, StringParser *fullRequestPtr)
{
	bool isAuthentic = false;

	if (!sAuthenticationEnabled) // no authentication
	{
		isAuthentic = true;
	}
	else // must authenticate
	{
		std::string t1 = ((RTSPSession *)(inParams->inRTSPSession))->GetRemoteAddr();
		StrPtrLen theClientIPAddressStr((char *)t1.data(), t1.length());
		bool isLocal = IPComponentStr(&theClientIPAddressStr).IsLocal();

		StrPtrLen authenticateName;
		StrPtrLen authenticatePassword;
		StrPtrLen authType;
		bool hasAuthentication = HasAuthentication(fullRequestPtr, &authenticateName, &authenticatePassword, &authType);
		if (hasAuthentication)
		{
			if (authType.Equal(sAuthRef))
			{
				if (isLocal)
					isAuthentic = OSXAuthenticate(&authenticatePassword);
			}
			else
				isAuthentic = Authenticate(inParams->inRTSPRequest, &authenticateName, &authenticatePassword);
		}
	}
	//    if (isAuthentic)
	//        isAuthentic = AuthorizeAdminRequest(inParams->inRTSPRequest);
	(void)QTSS_SetValue(inParams->inRTSPRequest, sAuthenticatedID, 0, (void*)&isAuthentic, sizeof(isAuthentic));

	return isAuthentic;
}

inline bool InWaitInterval(QTSS_Filter_Params* inParams)
{
	QTSS_TimeVal nextExecuteTime = sLastRequestTime + sRequestTimeIntervalMilli;
	QTSS_TimeVal currentTime = OS::Milliseconds();
	if (currentTime < nextExecuteTime)
	{
		int32_t waitTime = (int32_t)(nextExecuteTime - currentTime) + 1;
		//printf("(currentTime < nextExecuteTime) sSessID = %"   _U32BITARG_   " waitTime =%" _S32BITARG_ " currentTime = %qd nextExecute = %qd interval=%"   _U32BITARG_   "\n",sSessID, waitTime, currentTime, nextExecuteTime,sRequestTimeIntervalMilli);
		(void)QTSS_SetIdleTimer(waitTime);
		KeepSession(inParams->inRTSPRequest, true);

		//printf("-- call me again after %" _S32BITARG_ " millisecs session=%"   _U32BITARG_   " \n",waitTime,sSessID);
		return true;
	}
	sLastRequestTime = OS::Milliseconds();
	//printf("handle sessID=%"   _U32BITARG_   " time=%qd \n",sSessID,currentTime);
	return false;
}

inline void GetQueryData(QTSS_RTSPRequestObject theRequest)
{
	sAdminPtr = new AdminClass();
	Assert(sAdminPtr != nullptr);
	if (sAdminPtr == nullptr)
	{   //printf ("new AdminClass() failed!! \n");
		return;
	}
	if (sAdminPtr != nullptr)
	{
		sAdminPtr->Initialize(&sQTSSparams, sQueryPtr);  // Get theData
	}
}

inline void SendHeader(QTSS_StreamRef inStream)
{
	(void)QTSS_Write(inStream, sResponseHeader, ::strlen(sResponseHeader), nullptr, 0);
	(void)QTSS_Write(inStream, sEOL, ::strlen(sEOL), nullptr, 0);
	(void)QTSS_Write(inStream, sVersionHeader, ::strlen(sVersionHeader), nullptr, 0);
	(void)QTSS_Write(inStream, sEOL, ::strlen(sEOL), nullptr, 0);
	(void)QTSS_Write(inStream, sConnectionHeader, ::strlen(sConnectionHeader), nullptr, 0);
	(void)QTSS_Write(inStream, sEOL, ::strlen(sEOL), nullptr, 0);
	(void)QTSS_Write(inStream, sContentType, ::strlen(sContentType), nullptr, 0);
	(void)QTSS_Write(inStream, sEOM, ::strlen(sEOM), nullptr, 0);
}

inline void SendResult(QTSS_StreamRef inStream)
{
	SendHeader(inStream);
	if (sAdminPtr != nullptr)
		sAdminPtr->RespondToQuery(inStream, sQueryPtr, sQueryPtr->GetRootID());

}

inline bool GetRequestAuthenticatedState(QTSS_Filter_Params* inParams)
{
	bool result = false;
	uint32_t paramLen = sizeof(result);
	QTSSDictionary *dict = (QTSSDictionary*)inParams->inRTSPRequest;
	QTSS_Error err = dict->GetValue(sAuthenticatedID, 0, (void*)&result, &paramLen);
	if (err != QTSS_NoErr)
	{
		paramLen = sizeof(result);
		result = false;
		err = QTSS_SetValue(inParams->inRTSPRequest, sAuthenticatedID, 0, (void*)&result, paramLen);
	}
	return result;
}

inline bool GetRequestFlushState(QTSS_Filter_Params* inParams)
{
	bool result = false;
	uint32_t paramLen = sizeof(result);
	QTSSDictionary *dict = (QTSSDictionary*)inParams->inRTSPRequest;
	QTSS_Error err = dict->GetValue(sFlushingID, 0, (void*)&result, &paramLen);
	if (err != QTSS_NoErr)
	{
		paramLen = sizeof(result);
		result = false;
		//printf("no flush val so set to false session=%"   _U32BITARG_   " err =%" _S32BITARG_ "\n",sSessID, err);
		err = QTSS_SetValue(inParams->inRTSPRequest, sFlushingID, 0, (void*)&result, paramLen);
		//printf("QTSS_SetValue flush session=%"   _U32BITARG_   " err =%" _S32BITARG_ "\n",sSessID, err);
	}
	return result;
}

QTSS_Error FilterRequest(QTSS_Filter_Params* inParams)
{
	if (nullptr == inParams || nullptr == inParams->inRTSPSession || nullptr == inParams->inRTSPRequest)
	{
		Assert(0);
		return QTSS_NoErr;
	}

	OSMutexLocker locker(sAdminMutex);
	//check to see if we should handle this request. Invokation is triggered
	//by a "GET /" request

	QTSS_RTSPRequestObject theRequest = inParams->inRTSPRequest;

	uint32_t paramLen = sizeof(sSessID);
	QTSS_Error err = ((QTSSDictionary*)inParams->inRTSPSession)->GetValue(qtssRTSPSesID, 0, (void*)&sSessID, &paramLen);
	if (err != QTSS_NoErr)
		return QTSS_NoErr;

	StrPtrLen theFullRequest;
	err = ((QTSSDictionary*)theRequest)->GetValuePtr(qtssRTSPReqFullRequest, 0, (void**)&theFullRequest.Ptr, &theFullRequest.Len);
	if (err != QTSS_NoErr)
		return QTSS_NoErr;


	StringParser fullRequest(&theFullRequest);

	if (!IsAdminRequest(&fullRequest))
		return QTSS_NoErr;

	if (!AcceptSession(inParams->inRTSPSession))
	{
		(void)QTSS_Write(inParams->inRTSPRequest, sPermissionDeniedHeader, ::strlen(sPermissionDeniedHeader), nullptr, 0);
		(void)QTSS_Write(inParams->inRTSPRequest, sHTMLBody, ::strlen(sHTMLBody), nullptr, 0);
		KeepSession(theRequest, false);
		return QTSS_NoErr;
	}

	if (!GetRequestAuthenticatedState(inParams)) // must authenticate before handling
	{
		if (QTSS_IsGlobalLocked()) // must NOT be global locked
			return QTSS_RequestFailed;

		if (!IsAuthentic(inParams, &fullRequest))
		{
			(void)QTSS_Write(inParams->inRTSPRequest, sUnauthorizedResponseHeader, ::strlen(sUnauthorizedResponseHeader), nullptr, 0);
			(void)QTSS_Write(inParams->inRTSPRequest, sHTMLBody, ::strlen(sHTMLBody), nullptr, 0);
			KeepSession(theRequest, false);
			return QTSS_NoErr;
		}

	}

	if (GetRequestFlushState(inParams))
	{
		StillFlushing(inParams, true);
		return QTSS_NoErr;
	}

	if (!QTSS_IsGlobalLocked())
	{
		if (InWaitInterval(inParams))
			return QTSS_NoErr;

		//printf("New Request Wait for GlobalLock session=%"   _U32BITARG_   "\n",sSessID);
		(void)QTSS_RequestGlobalLock();
		KeepSession(theRequest, true);
		return QTSS_NoErr;
	}

	//printf("Handle request session=%"   _U32BITARG_   "\n",sSessID);
	APITests_DEBUG();

	if (sQueryPtr != nullptr)
	{
		delete sQueryPtr;
		sQueryPtr = nullptr;
	}
	sQueryPtr = new QueryURI(&theFullRequest);
	if (sQueryPtr == nullptr) return QTSS_NoErr;

	ShowQuery_DEBUG();

	if (sAdminPtr != nullptr)
	{
		delete sAdminPtr;
		sAdminPtr = nullptr;
	}
	uint32_t result = sQueryPtr->EvalQuery(nullptr, nullptr);
	if (result == 0) do
	{
		if (ElementNode_CountPtrs() > 0)
		{
			ElementNode_ShowPtrs();
			Assert(0);
		}

		GetQueryData(theRequest);

		SendResult(theRequest);
		delete sAdminPtr;
		sAdminPtr = nullptr;

		if (sQueryPtr && !sQueryPtr->QueryHasReponse())
		{
			uint32_t err = 404;
			(void)sQueryPtr->EvalQuery(&err, nullptr);
			ReportErr(inParams, err);
			break;
		}

		if (sQueryPtr && sQueryPtr->QueryHasReponse())
		{
			ReportErr(inParams, sQueryPtr->GetEvaluResult());
		}

		if (sQueryPtr->fIsPref && sQueryPtr->GetEvaluResult() == 0)
		{
			QTSS_ServiceID id;
			(void)QTSS_IDForService(QTSS_REREAD_PREFS_SERVICE, &id);
			(void)QTSS_DoService(id, nullptr);
		}
	} while (false);
	else
	{
		SendHeader(theRequest);
		ReportErr(inParams, sQueryPtr->GetEvaluResult());
	}

	if (sQueryPtr != nullptr)
	{
		delete sQueryPtr;
		sQueryPtr = nullptr;
	}

	(void)StillFlushing(inParams, true);
	return QTSS_NoErr;

}
