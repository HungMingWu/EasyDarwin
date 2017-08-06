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
	 File:       QTSSWebDebugModule.cpp

	 Contains:   Implements web debug module


 */

#include "QTSSWebDebugModule.h"
#include "StrPtrLen.h"
#include "QTSSDictionary.h"

 // STATIC DATA

static QTSS_AttributeID sStateAttr = qtssIllegalAttrID;

static StrPtrLen    sRequestHeader("GET /debug HTTP");

#if MEMORY_DEBUGGING
static char*        sResponseHeader = "HTTP/1.0 200 OK\r\nServer: TimeShare/1.0\r\nConnection: Close\r\nContent-Type: text/html\r\n\r\n";
static char*        sResponseEnd = "</BODY></HTML>";
#endif

// FUNCTION PROTOTYPES

QTSS_Error QTSSWebDebugModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParams);
static QTSS_Error   Register(QTSS_Register_Params* inParams);
static QTSS_Error   Filter(QTSS_Filter_Params* inParams);

QTSS_Error QTSSWebDebugModule_Main(void* inPrivateArgs)
{
	return _stublibrary_main(inPrivateArgs, QTSSWebDebugModuleDispatch);
}

QTSS_Error QTSSWebDebugModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParams)
{
	switch (inRole)
	{
	case QTSS_Register_Role:
		return Register(&inParams->regParams);
	case QTSS_RTSPFilter_Role:
		return Filter(&inParams->rtspFilterParams);
	}
	return QTSS_NoErr;
}

QTSS_Error Register(QTSS_Register_Params* inParams)
{
	// Do role & attribute setup
	(void)QTSS_AddRole(QTSS_RTSPFilter_Role);

	// Register an attribute
	static char*        sStateName = "QTSSWebDebugModuleState";
	(void)QTSS_AddStaticAttribute(qtssRTSPRequestObjectType, sStateName, nullptr, qtssAttrDataTypeUInt32);
	(void)QTSS_IDForAttr(qtssRTSPRequestObjectType, sStateName, &sStateAttr);

	// Tell the server our name!
	static char* sModuleName = "QTSSWebDebugModule";
	::strcpy(inParams->outModuleName, sModuleName);

	return QTSS_NoErr;
}

QTSS_Error Filter(QTSS_Filter_Params* inParams)
{
	uint32_t theLen = 0;
	char* theFullRequest = nullptr;
	((QTSSDictionary*)inParams->inRTSPRequest)->GetValuePtr(qtssRTSPReqFullRequest, 0, (void**)&theFullRequest, &theLen);

	if ((theFullRequest == nullptr) || (theLen < sRequestHeader.Len))
		return QTSS_NoErr;
	if (::memcmp(theFullRequest, sRequestHeader.Ptr, sRequestHeader.Len) != 0)
		return QTSS_NoErr;

	return QTSS_NoErr;
}
