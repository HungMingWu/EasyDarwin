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
	 File:       QTSSAccessModule.cpp

	 Contains:   Implementation of QTSSAccessModule.



 */

#include <memory>

#include "QTSSAccessModule.h"

#include "defaultPaths.h"

#include "QTSSDictionary.h"
#include "StrPtrLen.h"
#include "MyAssert.h"
#include "AccessChecker.h"
#include "QTAccessFile.h"
#include "QTSSModuleUtils.h"
#include "RTSPRequest.h"

#ifndef __Win32__
#include <unistd.h>
#endif

#include <errno.h>



 // ATTRIBUTES

 // STATIC DATA


#define MODPREFIX_ "modAccess_"

static StrPtrLen    sSDPSuffix(".sdp");
static OSMutex*     sUserMutex = nullptr;

static bool         sDefaultAuthenticationEnabled = true;
static bool         sAuthenticationEnabled = true;

static char* sDefaultUsersFilePath = DEFAULTPATHS_ETC_DIR "qtusers";
static char* sUsersFilePath = nullptr;

static char* sDefaultGroupsFilePath = DEFAULTPATHS_ETC_DIR "qtgroups";
static char* sGroupsFilePath = nullptr;

static char* sDefaultAccessFileName = "qtaccess";

static QTSS_AttributeID sBadNameMessageAttrID = qtssIllegalAttrID;
static QTSS_AttributeID sUsersFileNotFoundMessageAttrID = qtssIllegalAttrID;
static QTSS_AttributeID sGroupsFileNotFoundMessageAttrID = qtssIllegalAttrID;
static QTSS_AttributeID sBadUsersFileMessageAttrID = qtssIllegalAttrID;
static QTSS_AttributeID sBadGroupsFileMessageAttrID = qtssIllegalAttrID;

static QTSS_StreamRef           sErrorLogStream = nullptr;
static QTSS_TextMessagesObject  sMessages = nullptr;
static QTSS_ModulePrefsObject   sPrefs = nullptr;
static QTSS_PrefsObject         sServerPrefs = nullptr;

static AccessChecker**          sAccessCheckers;
static uint32_t                   sNumAccessCheckers = 0;
static uint32_t                   sAccessCheckerArraySize = 0;

static bool                   sAllowGuestDefaultEnabled = true;
static bool                   sDefaultGuestEnabled = true;

// FUNCTION PROTOTYPES

static QTSS_Error QTSSAccessModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParams);
static QTSS_Error Register();
static QTSS_Error Initialize(QTSS_Initialize_Params* inParams);
static QTSS_Error Shutdown();
static QTSS_Error RereadPrefs();
static QTSS_Error AuthenticateRTSPRequest(QTSS_RTSPAuth_Params* inParams);
static QTSS_Error AccessAuthorizeRTSPRequest(QTSS_StandardRTSP_Params* inParams);
static char*      GetCheckedFileName();

// FUNCTION IMPLEMENTATIONS


QTSS_Error QTSSAccessModule_Main(void* inPrivateArgs)
{
	return _stublibrary_main(inPrivateArgs, QTSSAccessModuleDispatch);
}


QTSS_Error  QTSSAccessModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParams)
{
	switch (inRole)
	{
	case QTSS_Register_Role:
		return Register();

	case QTSS_Initialize_Role:
		return Initialize(&inParams->initParams);

	case QTSS_RereadPrefs_Role:
		return RereadPrefs();

	case QTSS_RTSPAuthenticate_Role:
		if (sAuthenticationEnabled)
			return AuthenticateRTSPRequest(&inParams->rtspAthnParams);
		break;

	case QTSS_RTSPAuthorize_Role:
		if (sAuthenticationEnabled)
			return AccessAuthorizeRTSPRequest(&inParams->rtspRequestParams);
		break;

	case QTSS_Shutdown_Role:
		return Shutdown();
	default: break;
	}

	return QTSS_NoErr;
}

QTSS_Error Register()
{
	// Do role & attribute setup
	(void)QTSS_AddRole(QTSS_Initialize_Role);
	(void)QTSS_AddRole(QTSS_RereadPrefs_Role);
	(void)QTSS_AddRole(QTSS_RTSPAuthenticate_Role);
	(void)QTSS_AddRole(QTSS_RTSPAuthorize_Role);

	// Add AuthenticateName and Password attributes
	static char*        sBadAccessFileName = "QTSSAccessModuleBadAccessFileName";
	static char*        sUsersFileNotFound = "QTSSAccessModuleUsersFileNotFound";
	static char*        sGroupsFileNotFound = "QTSSAccessModuleGroupsFileNotFound";
	static char*        sBadUsersFile = "QTSSAccessModuleBadUsersFile";
	static char*        sBadGroupsFile = "QTSSAccessModuleBadGroupsFile";

	(void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sBadAccessFileName, nullptr, qtssAttrDataTypeCharArray);
	(void)QTSS_IDForAttr(qtssTextMessagesObjectType, sBadAccessFileName, &sBadNameMessageAttrID);

	(void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sUsersFileNotFound, nullptr, qtssAttrDataTypeCharArray);
	(void)QTSS_IDForAttr(qtssTextMessagesObjectType, sUsersFileNotFound, &sUsersFileNotFoundMessageAttrID);

	(void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sGroupsFileNotFound, nullptr, qtssAttrDataTypeCharArray);
	(void)QTSS_IDForAttr(qtssTextMessagesObjectType, sGroupsFileNotFound, &sGroupsFileNotFoundMessageAttrID);

	(void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sBadUsersFile, nullptr, qtssAttrDataTypeCharArray);
	(void)QTSS_IDForAttr(qtssTextMessagesObjectType, sBadUsersFile, &sBadUsersFileMessageAttrID);

	(void)QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sBadGroupsFile, nullptr, qtssAttrDataTypeCharArray);
	(void)QTSS_IDForAttr(qtssTextMessagesObjectType, sBadGroupsFile, &sBadGroupsFileMessageAttrID);

	return QTSS_NoErr;
}


QTSS_Error Initialize(QTSS_Initialize_Params* inParams)
{
	// Create an array of AccessCheckers
	sAccessCheckers = new AccessChecker*[2];
	sAccessCheckers[0] = new AccessChecker();
	sNumAccessCheckers = 1;
	sAccessCheckerArraySize = 2;

	// Setup module utils
	QTSSModuleUtils::Initialize(inParams->inMessages, inParams->inServer, inParams->inErrorLogStream);
	sErrorLogStream = inParams->inErrorLogStream;
	sMessages = inParams->inMessages;
	sPrefs = QTSSModuleUtils::GetModulePrefsObject(inParams->inModule);
	sServerPrefs = inParams->inPrefs;
	sUserMutex = new OSMutex();
	RereadPrefs();
	QTAccessFile::Initialize();

	return QTSS_NoErr;
}

QTSS_Error Shutdown()
{
	//cleanup

	// delete all the AccessCheckers
	uint32_t index;
	for (index = 0; index < sNumAccessCheckers; index++)
		delete sAccessCheckers[index];
	delete[] sAccessCheckers;
	sNumAccessCheckers = 0;

	// delete the main users and groups path

	//if(sUsersFilePath != sDefaultUsersFilePath) 
	// sUsersFilePath is assigned by a call to QTSSModuleUtils::GetStringAttribute which always
	// allocates memory even if it just returns the default value
	delete[] sUsersFilePath;
	sUsersFilePath = nullptr;

	//if(sGroupsFilePath != sDefaultGroupsFilePath)
	// sGroupsFilePath is assigned by a call to QTSSModuleUtils::GetStringAttribute which always
	// allocates memory even if it just returns the default value
	delete[] sGroupsFilePath;
	sGroupsFilePath = nullptr;

	return QTSS_NoErr;
}

char* GetCheckedFileName()
{
	static char *badChars = "/'\"";
	char        theBadCharMessage[] = "' '";
	char* result = QTSSModuleUtils::GetStringAttribute(sPrefs, MODPREFIX_"qtaccessfilename", sDefaultAccessFileName);
	StrPtrLen searchStr(result);

	char* theBadChar = strpbrk(searchStr.Ptr, badChars);
	if (theBadChar != nullptr)
	{
		theBadCharMessage[1] = theBadChar[0];
		QTSSModuleUtils::LogError(qtssWarningVerbosity, sBadNameMessageAttrID, 0, theBadCharMessage, result);

		delete[] result;
		result = new char[::strlen(sDefaultAccessFileName) + 2];
		::strcpy(result, sDefaultAccessFileName);
	}
	return result;
}

QTSS_Error RereadPrefs()
{
	OSMutexLocker locker(sUserMutex);

	//
	// Use the standard GetAttribute routine to retrieve the correct values for our preferences
	QTSSModuleUtils::GetAttribute(sPrefs, MODPREFIX_"enabled", qtssAttrDataTypeBool16,
		&sAuthenticationEnabled, &sDefaultAuthenticationEnabled, sizeof(sAuthenticationEnabled));

	//if(sUsersFilePath != sDefaultUsersFilePath)
	// sUsersFilePath is assigned by a call to QTSSModuleUtils::GetStringAttribute which always
	// allocates memory even if it just returns the default value
	// delete this old memory before reassigning it to new memory
	delete[] sUsersFilePath;
	sUsersFilePath = nullptr;

	//if(sGroupsFilePath != sDefaultGroupsFilePath)
	// sGroupsFilePath is assigned by a call to QTSSModuleUtils::GetStringAttribute which always
	// allocates memory even if it just returns the default value
	// delete this old memory before reassigning it to new memory
	delete[] sGroupsFilePath;
	sGroupsFilePath = nullptr;

	sUsersFilePath = QTSSModuleUtils::GetStringAttribute(sPrefs, MODPREFIX_"usersfilepath", sDefaultUsersFilePath);
	sGroupsFilePath = QTSSModuleUtils::GetStringAttribute(sPrefs, MODPREFIX_"groupsfilepath", sDefaultGroupsFilePath);
	// GetCheckedFileName always allocates memory
	char* accessFile = GetCheckedFileName();
	// QTAccessFile::SetAccessFileName makes its own copy, 
	// so delete the previous allocated memory after this call
	QTAccessFile::SetAccessFileName(accessFile);
	delete[] accessFile;

	if (sAccessCheckers[0]->HaveFilePathsChanged(sUsersFilePath, sGroupsFilePath))
	{
		sAccessCheckers[0]->UpdateFilePaths(sUsersFilePath, sGroupsFilePath);
		uint32_t err;
		err = sAccessCheckers[0]->UpdateUserProfiles();
		if (err & AccessChecker::kUsersFileNotFoundErr)
			QTSSModuleUtils::LogError(qtssWarningVerbosity, sUsersFileNotFoundMessageAttrID, 0, sUsersFilePath, nullptr);
		else if (err & AccessChecker::kBadUsersFileErr)
			QTSSModuleUtils::LogError(qtssWarningVerbosity, sBadUsersFileMessageAttrID, 0, sUsersFilePath, nullptr);
		if (err & AccessChecker::kGroupsFileNotFoundErr)
			QTSSModuleUtils::LogError(qtssWarningVerbosity, sGroupsFileNotFoundMessageAttrID, 0, sGroupsFilePath, nullptr);
		else if (err & AccessChecker::kBadGroupsFileErr)
			QTSSModuleUtils::LogError(qtssWarningVerbosity, sBadGroupsFileMessageAttrID, 0, sGroupsFilePath, nullptr);
	}

	QTSSModuleUtils::GetAttribute(sServerPrefs, "enable_allow_guest_default", qtssAttrDataTypeBool16,
		&sAllowGuestDefaultEnabled, (void *)&sDefaultGuestEnabled, sizeof(sAllowGuestDefaultEnabled));


	return QTSS_NoErr;
}

QTSS_Error AuthenticateRTSPRequest(QTSS_RTSPAuth_Params* inParams)
{
	QTSS_RTSPRequestObject  theRTSPRequest = inParams->inRTSPRequest;
	RTSPRequest *pReq = (RTSPRequest *)inParams->inRTSPRequest;
	uint32_t fileErr;

	OSMutexLocker locker(sUserMutex);

	if ((nullptr == inParams) || (nullptr == inParams->inRTSPRequest))
		return QTSS_RequestFailed;

	// Get the user profile object from the request object
	QTSS_UserProfileObject theUserProfile = nullptr;
	uint32_t len = sizeof(QTSS_UserProfileObject);
	QTSS_Error theErr = ((QTSSDictionary*)theRTSPRequest)->GetValue(qtssRTSPReqUserProfile, 0, (void*)&theUserProfile, &len);
	Assert(len == sizeof(QTSS_UserProfileObject));
	if (theErr != QTSS_NoErr)
		return theErr;

	bool defaultPaths = true;
	// Check for a users and groups file in the access file
	// For this, first get local file path and root movie directory
	//get the local file path
	std::string  pathBuffStr(pReq->GetLocalPath());

	if (pathBuffStr.empty())
		return QTSS_RequestFailed;
	//get the root movie directory
	char*   movieRootDirStr = QTSSModuleUtils::GetMoviesRootDir_Copy(theRTSPRequest);
	std::unique_ptr<char[]> movieRootDeleter(movieRootDirStr);
	if (nullptr == movieRootDirStr)
		return QTSS_RequestFailed;
	// Now get the access file path
	char* accessFilePath = QTAccessFile::GetAccessFile_Copy(movieRootDirStr, pathBuffStr.c_str());
	std::unique_ptr<char[]> accessFilePathDeleter(accessFilePath);
	// Parse the access file for the AuthUserFile and AuthGroupFile keywords
	char* usersFilePath = nullptr;
	char* groupsFilePath = nullptr;

	// Get the request action from the request object
	QTSS_ActionFlags action = qtssActionFlagsNoFlags;
	len = sizeof(action);
	theErr = ((QTSSDictionary*)theRTSPRequest)->GetValue(qtssRTSPReqAction, 0, (void*)&action, &len);
	Assert(len == sizeof(action));
	if (theErr != QTSS_NoErr)
		return theErr;

	// Allocates memory for usersFilePath and groupsFilePath
	QTSS_AuthScheme authScheme = QTAccessFile::FindUsersAndGroupsFilesAndAuthScheme(accessFilePath, action, &usersFilePath, &groupsFilePath);

	if ((usersFilePath != nullptr) || (groupsFilePath != nullptr))
		defaultPaths = false;

	if (usersFilePath == nullptr)
		usersFilePath = strdup(sUsersFilePath);

	if (groupsFilePath == nullptr)
		groupsFilePath = strdup(sGroupsFilePath);

	std::unique_ptr<char[]> userPathDeleter(usersFilePath);
	std::unique_ptr<char[]> groupPathDeleter(groupsFilePath);

	AccessChecker* currentChecker = nullptr;
	uint32_t index;

	// If the default users and groups file are not the ones we need
	if (!defaultPaths)
	{
		// check if there is one AccessChecker that matches the needed paths
		// Don't have to check for the first one (or element zero) because it has the default paths
		for (index = 1; index < sNumAccessCheckers; index++)
		{
			// If an access checker that matches the users and groups file paths is found
			if (!sAccessCheckers[index]->HaveFilePathsChanged(usersFilePath, groupsFilePath))
			{
				currentChecker = sAccessCheckers[index];
				break;
			}
		}
		// If an existing AccessChecker for the needed paths isn't found
		if (currentChecker == nullptr)
		{
			// Grow the AccessChecker array if needed
			if (sNumAccessCheckers == sAccessCheckerArraySize)
			{
				AccessChecker** oldAccessCheckers = sAccessCheckers;
				sAccessCheckers = new AccessChecker*[sAccessCheckerArraySize * 2];
				for (index = 0; index < sNumAccessCheckers; index++)
				{
					sAccessCheckers[index] = oldAccessCheckers[index];
				}
				sAccessCheckerArraySize *= 2;
				delete[] oldAccessCheckers;
			}

			// And create a new AccessChecker for the paths
			sAccessCheckers[sNumAccessCheckers] = new AccessChecker();
			sAccessCheckers[sNumAccessCheckers]->UpdateFilePaths(usersFilePath, groupsFilePath);
			fileErr = sAccessCheckers[sNumAccessCheckers]->UpdateUserProfiles();

			if (fileErr & AccessChecker::kUsersFileNotFoundErr)
				QTSSModuleUtils::LogError(qtssWarningVerbosity, sUsersFileNotFoundMessageAttrID, 0, usersFilePath, nullptr);
			else if (fileErr & AccessChecker::kBadUsersFileErr)
				QTSSModuleUtils::LogError(qtssWarningVerbosity, sBadUsersFileMessageAttrID, 0, usersFilePath, nullptr);
			if (fileErr & AccessChecker::kGroupsFileNotFoundErr)
				QTSSModuleUtils::LogError(qtssWarningVerbosity, sGroupsFileNotFoundMessageAttrID, 0, groupsFilePath, nullptr);
			else if (fileErr & AccessChecker::kBadGroupsFileErr)
				QTSSModuleUtils::LogError(qtssWarningVerbosity, sBadGroupsFileMessageAttrID, 0, groupsFilePath, nullptr);

			currentChecker = sAccessCheckers[sNumAccessCheckers];
			sNumAccessCheckers++;
		}

	}
	else
	{
		currentChecker = sAccessCheckers[0];
	}

	// Before retrieving the user profile information
	// check if the groups/users files have been modified and update them otherwise
	fileErr = currentChecker->UpdateUserProfiles();

	/*
	// This is for logging the errors if users file and/or the groups file is not found or corrupted
	char* usersFile = currentChecker->GetUsersFilePathPtr();
	char* groupsFile = currentChecker->GetGroupsFilePathPtr();

	if(fileErr & AccessChecker::kUsersFileNotFoundErr)
		QTSSModuleUtils::LogError(qtssWarningVerbosity,sUsersFileNotFoundMessageAttrID, 0, usersFile, NULL);
	else if(fileErr & AccessChecker::kBadUsersFileErr)
		QTSSModuleUtils::LogError(qtssWarningVerbosity,sBadUsersFileMessageAttrID, 0, usersFile, NULL);
	if(fileErr & AccessChecker::kGroupsFileNotFoundErr)
		QTSSModuleUtils::LogError(qtssWarningVerbosity,sGroupsFileNotFoundMessageAttrID, 0, groupsFile, NULL);
	else if(fileErr & AccessChecker::kBadGroupsFileErr)
		QTSSModuleUtils::LogError(qtssWarningVerbosity,sBadGroupsFileMessageAttrID, 0, groupsFile, NULL);
	  */

	  // Retrieve the password data and group information for the user and set them
	  // in the qtssRTSPReqUserProfile attr
	  // The password data is crypt of the real password for Basic authentication
	  // and it is MD5(username:realm:password) for Digest authentication

		  // It the access file didn't contain an auth scheme, then get the auth scheme out of the request object
		  // else, set the qtssRTSPReqAuthScheme to that found in the access file

	if (authScheme == qtssAuthNone)
	{
		// Get the authentication scheme from the request object
		len = sizeof(authScheme);
		theErr = ((QTSSDictionary*)theRTSPRequest)->GetValue(qtssRTSPReqAuthScheme, 0, (void*)&authScheme, &len);
		Assert(len == sizeof(authScheme));
		if (theErr != QTSS_NoErr)
			return theErr;
	}
	else
	{
		theErr = QTSS_SetValue(theRTSPRequest, qtssRTSPReqAuthScheme, 0, (void*)&authScheme, sizeof(authScheme));
		if (theErr != QTSS_NoErr)
			return theErr;
	}

	// Set the qtssUserRealm to the realm value retrieved from the users file
	// This should be used for digest auth scheme, and if no realm is found in the qtaccess file, then
	// it should be used for basic auth scheme.
	// No memory is allocated; just a pointer is returned
	StrPtrLen* authRealm = currentChecker->GetAuthRealm();
	(void)QTSS_SetValue(theUserProfile, qtssUserRealm, 0, (void*)(authRealm->Ptr), (authRealm->Len));


	// Get the username from the user profile object
	char*   usernameBuf = nullptr;
	theErr = ((QTSSDictionary*)theUserProfile)->GetValueAsString(qtssUserName, 0, &usernameBuf);
	std::unique_ptr<char[]> usernameBufDeleter(usernameBuf);
	StrPtrLen username(usernameBuf);
	if (theErr != QTSS_NoErr)
		return theErr;

	// No memory is allocated; just a pointer to the profile is returned
	AccessChecker::UserProfile* profile = currentChecker->RetrieveUserProfile(&username);

	if (profile == nullptr)
		return QTSS_NoErr;

	// Set the qtssUserPassword attribute to either the crypted password or the digest password
	// based on the authentication scheme
	if (authScheme == qtssAuthBasic)
		(void)QTSS_SetValue(theUserProfile, qtssUserPassword, 0, (void*)((profile->cryptPassword).Ptr), (profile->cryptPassword).Len);
	else if (authScheme == qtssAuthDigest)
		(void)QTSS_SetValue(theUserProfile, qtssUserPassword, 0, (void*)((profile->digestPassword).Ptr), (profile->digestPassword).Len);


	// Set the multivalued qtssUserGroups attr to the groups the user belongs to, if any
	uint32_t maxLen = profile->maxGroupNameLen;
	for (index = 0; index < profile->numGroups; index++)
	{
		uint32_t curLen = ::strlen(profile->groups[index]);
		if (curLen < maxLen)
		{
			auto* groupWithPaddedZeros = new char[maxLen];  // memory allocated
			::memcpy(groupWithPaddedZeros, profile->groups[index], curLen);
			::memset(groupWithPaddedZeros + curLen, '\0', maxLen - curLen);
			(void)QTSS_SetValue(theUserProfile, qtssUserGroups, index, (void*)groupWithPaddedZeros, maxLen);
			delete[] groupWithPaddedZeros;                 // memory deleted
		}
		else
		{
			(void)QTSS_SetValue(theUserProfile, qtssUserGroups, index, (void*)(profile->groups[index]), maxLen);
		}
	}

	return QTSS_NoErr;
}

QTSS_Error AccessAuthorizeRTSPRequest(QTSS_StandardRTSP_Params* inParams)
{
	bool allowNoAccessFiles = sAllowGuestDefaultEnabled; //no access files allowed means allowing guest access (unknown users)
	QTSS_ActionFlags noAction = ~qtssActionFlagsRead; // allow any action
	QTSS_ActionFlags authorizeAction = QTSSModuleUtils::GetRequestActions(inParams->inRTSPRequest);
	bool authorized = false;
	bool allowAnyUser = false;
	QTAccessFile accessFile;
	return  accessFile.AuthorizeRequest(inParams, allowNoAccessFiles, noAction, authorizeAction, &authorized, &allowAnyUser);
}
