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
	 File:       QTSSFlowControlModule.cpp

	 Contains:   Implements object defined in .h file
 */

#include <stdio.h>

#include "QTSSFlowControlModule.h"
#include "OSHeaders.h"
#include "QTSSModuleUtils.h"
#include "MyAssert.h"

 //Turns on printfs that are useful for debugging
#define FLOW_CONTROL_DEBUGGING 0

// ATTRIBUTES IDs

static QTSS_AttributeID sNumLossesAboveTolAttr = qtssIllegalAttrID;
static QTSS_AttributeID sNumLossesBelowTolAttr = qtssIllegalAttrID;
static QTSS_AttributeID sNumWorsesAttr = qtssIllegalAttrID;

// STATIC VARIABLES

static QTSS_ModulePrefsObject sPrefs = NULL;
static QTSS_PrefsObject     sServerPrefs = NULL;
static QTSS_ServerObject    sServer = NULL;

// Default values for preferences
static uint32_t   sDefaultLossThinTolerance = 30;
static uint32_t   sDefaultNumLossesToThin = 3;
static uint32_t   sDefaultLossThickTolerance = 5;
static uint32_t   sDefaultLossesToThick = 6;
static uint32_t   sDefaultWorsesToThin = 2;
static bool   sDefaultModuleEnabled = true;

// Current values for preferences
static uint32_t   sLossThinTolerance = 30;
static uint32_t   sNumLossesToThin = 3;
static uint32_t   sLossThickTolerance = 5;
static uint32_t   sLossesToThick = 6;
static uint32_t   sWorsesToThin = 2;
static bool   sModuleEnabled = true;

// Server preference we respect
static bool   sDisableThinning = false;


// FUNCTION PROTOTYPES

static QTSS_Error   QTSSFlowControlModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParamBlock);
static QTSS_Error   Register(QTSS_Register_Params* inParams);
static QTSS_Error   Initialize(QTSS_Initialize_Params* inParams);
static QTSS_Error   RereadPrefs();
static QTSS_Error   ProcessRTCPPacket(QTSS_RTCPProcess_Params* inParams);
static void         InitializeDictionaryItems(QTSS_RTPStreamObject inStream);




QTSS_Error QTSSFlowControlModule_Main(void* inPrivateArgs)
{
	return _stublibrary_main(inPrivateArgs, QTSSFlowControlModuleDispatch);
}

QTSS_Error  QTSSFlowControlModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParamBlock)
{
	switch (inRole)
	{
	case QTSS_Register_Role:
		return Register(&inParamBlock->regParams);
	case QTSS_Initialize_Role:
		return Initialize(&inParamBlock->initParams);
	case QTSS_RereadPrefs_Role:
		return RereadPrefs();
	case QTSS_RTCPProcess_Role:
		return ProcessRTCPPacket(&inParamBlock->rtcpProcessParams);
	default: break;
	}
	return QTSS_NoErr;
}

QTSS_Error Register(QTSS_Register_Params* inParams)
{
	// Do role setup
	(void)QTSS_AddRole(QTSS_Initialize_Role);
	(void)QTSS_AddRole(QTSS_RTCPProcess_Role);
	(void)QTSS_AddRole(QTSS_RereadPrefs_Role);


	// Add other attributes
	static char*        sNumLossesAboveToleranceName = "QTSSFlowControlModuleLossAboveTol";
	static char*        sNumLossesBelowToleranceName = "QTSSFlowControlModuleLossBelowTol";
	static char*        sNumGettingWorsesName = "QTSSFlowControlModuleGettingWorses";

	(void)QTSS_AddStaticAttribute(qtssRTPStreamObjectType, sNumLossesAboveToleranceName, NULL, qtssAttrDataTypeUInt32);
	(void)QTSS_IDForAttr(qtssRTPStreamObjectType, sNumLossesAboveToleranceName, &sNumLossesAboveTolAttr);

	(void)QTSS_AddStaticAttribute(qtssRTPStreamObjectType, sNumLossesBelowToleranceName, NULL, qtssAttrDataTypeUInt32);
	(void)QTSS_IDForAttr(qtssRTPStreamObjectType, sNumLossesBelowToleranceName, &sNumLossesBelowTolAttr);

	(void)QTSS_AddStaticAttribute(qtssRTPStreamObjectType, sNumGettingWorsesName, NULL, qtssAttrDataTypeUInt32);
	(void)QTSS_IDForAttr(qtssRTPStreamObjectType, sNumGettingWorsesName, &sNumWorsesAttr);

	// Tell the server our name!
	static char* sModuleName = "QTSSFlowControlModule";
	::strcpy(inParams->outModuleName, sModuleName);

	return QTSS_NoErr;
}

QTSS_Error Initialize(QTSS_Initialize_Params* inParams)
{
	QTSSModuleUtils::Initialize(inParams->inMessages, inParams->inServer, inParams->inErrorLogStream);
	sServer = inParams->inServer;
	sServerPrefs = inParams->inPrefs;
	sPrefs = QTSSModuleUtils::GetModulePrefsObject(inParams->inModule);
	return RereadPrefs();
}


QTSS_Error RereadPrefs()
{
	//
	// Use the standard GetPref routine to retrieve the correct values for our preferences
	QTSSModuleUtils::GetAttribute(sPrefs, "loss_thin_tolerance", qtssAttrDataTypeUInt32,
		&sLossThinTolerance, &sDefaultLossThinTolerance, sizeof(sLossThinTolerance));
	QTSSModuleUtils::GetAttribute(sPrefs, "num_losses_to_thin", qtssAttrDataTypeUInt32,
		&sNumLossesToThin, &sDefaultNumLossesToThin, sizeof(sNumLossesToThin));
	QTSSModuleUtils::GetAttribute(sPrefs, "loss_thick_tolerance", qtssAttrDataTypeUInt32,
		&sLossThickTolerance, &sDefaultLossThickTolerance, sizeof(sLossThickTolerance));
	QTSSModuleUtils::GetAttribute(sPrefs, "num_losses_to_thick", qtssAttrDataTypeUInt32,
		&sLossesToThick, &sDefaultLossesToThick, sizeof(sLossesToThick));
	QTSSModuleUtils::GetAttribute(sPrefs, "num_worses_to_thin", qtssAttrDataTypeUInt32,
		&sWorsesToThin, &sDefaultWorsesToThin, sizeof(sWorsesToThin));

	QTSSModuleUtils::GetAttribute(sPrefs, "flow_control_udp_thinning_module_enabled", qtssAttrDataTypeBool16,
		&sModuleEnabled, &sDefaultModuleEnabled, sizeof(sDefaultModuleEnabled));

	uint32_t len = sizeof(sDisableThinning);
	(void)QTSS_GetValue(sServerPrefs, qtssPrefsDisableThinning, 0, (void*)&sDisableThinning, &len);

	return QTSS_NoErr;
}

QTSS_Error ProcessRTCPPacket(QTSS_RTCPProcess_Params* inParams)
{
	if (!sModuleEnabled || sDisableThinning)
	{
		return QTSS_NoErr;
	}

#if FLOW_CONTROL_DEBUGGING
	QTSS_RTPPayloadType* thePayloadType = 0;
	uint32_t thePayloadLen = 0;
	(void)QTSS_GetValuePtr(inParams->inRTPStream, qtssRTPStrPayloadType, 0, (void**)&thePayloadType, &thePayloadLen);

	if ((*thePayloadType != 0) && (*thePayloadType == qtssVideoPayloadType))
		printf("Video track reporting:\n");
	else if ((*thePayloadType != 0) && (*thePayloadType == qtssAudioPayloadType))
		printf("Audio track reporting:\n");
	else
		printf("Unknown track reporting\n");
#endif

	//
	// Find out if this is a qtssRTPTransportTypeUDP. This is the only type of
	// transport we should monitor
	QTSS_RTPTransportType theTransportType = qtssRTPTransportTypeUDP;
	uint32_t theLen = sizeof(theTransportType);
	QTSS_Error theErr = QTSS_GetValue(inParams->inRTPStream, qtssRTPStrTransportType, 0, (void*)&theTransportType, &theLen);
	Assert(theErr == QTSS_NoErr);

	if (theTransportType != qtssRTPTransportTypeUDP)
		return QTSS_NoErr;

	//ALGORITHM FOR DETERMINING WHEN TO MAKE QUALITY ADJUSTMENTS IN THE STREAM:

	//This routine makes quality adjustment determinations for the server. It is designed
	//to be flexible: you may swap this algorithm out for another implemented in another module,
	//and this algorithm uses settings that are adjustable at runtime.

	//It uses the % loss statistic in the RTCP packets, as well as the "getting better" &
	//"getting worse" fields.

	//Less bandwidth will be served if the loss % of N number of RTCP packets is above M, where
	//N and M are runtime settings.

	//Less bandwidth will be served if "getting worse" is reported N number of times.

	//More bandwidth will be served if the loss % of N number of RTCPs is below M.
	//N will scale up over time.

	//More bandwidth will be served if the client reports "getting better"

	//If the initial values of our dictionary items aren't yet in, put them in.
	InitializeDictionaryItems(inParams->inRTPStream);

	QTSS_RTPStreamObject theStream = inParams->inRTPStream;

	bool ratchetMore = false;
	bool ratchetLess = false;

	bool clearPercentLossThinCount = true;
	bool clearPercentLossThickCount = true;

	uint32_t* uint32_tPtr = NULL;
	uint16_t* uint16_tPtr = NULL;
	theLen = 0;

	uint32_t theNumLossesAboveTol = 0;
	uint32_t theNumLossesBelowTol = 0;
	uint32_t theNumWorses = 0;

	// Get our current counts for this stream. If any of these aren't present, something is seriously wrong
	// with this dictionary, so we should probably just abort
	(void)QTSS_GetValuePtr(theStream, sNumLossesAboveTolAttr, 0, (void**)&uint32_tPtr, &theLen);
	if ((uint32_tPtr != NULL) && (theLen == sizeof(uint32_t)))
		theNumLossesAboveTol = *uint32_tPtr;

	(void)QTSS_GetValuePtr(theStream, sNumLossesBelowTolAttr, 0, (void**)&uint32_tPtr, &theLen);
	if ((uint32_tPtr != NULL) && (theLen == sizeof(uint32_t)))
		theNumLossesBelowTol = *uint32_tPtr;

	(void)QTSS_GetValuePtr(theStream, sNumWorsesAttr, 0, (void**)&uint32_tPtr, &theLen);
	if ((uint32_tPtr != NULL) && (theLen == sizeof(uint32_t)))
		theNumWorses = *uint32_tPtr;


	//First take any action necessitated by the loss percent
	(void)QTSS_GetValuePtr(inParams->inRTPStream, qtssRTPStrPercentPacketsLost, 0, (void**)&uint16_tPtr, &theLen);
	if ((uint16_tPtr != NULL) && (theLen == sizeof(uint16_t)))
	{
		uint16_t thePercentLoss = *uint16_tPtr;
		thePercentLoss /= 256; //Hmmm... looks like the client reports loss percent in multiples of 256
#if FLOW_CONTROL_DEBUGGING
		printf("Percent loss: %d\n", thePercentLoss);
#endif

		//check for a thinning condition
		if (thePercentLoss > sLossThinTolerance)
		{
			theNumLossesAboveTol++;//we must count this loss

			//We only adjust after a certain number of these in a row. Check to see if we've
			//satisfied the thinning condition, and adjust the count
			if (theNumLossesAboveTol >= sNumLossesToThin)
			{
#if FLOW_CONTROL_DEBUGGING
				printf("Percent loss too high: ratcheting less\n");
#endif
				ratchetLess = true;
			}
			else
			{
#if FLOW_CONTROL_DEBUGGING
				printf("Percent loss too high: Incrementing percent loss count to %"   _U32BITARG_   "\n", theNumLossesAboveTol);
#endif
				(void)QTSS_SetValue(theStream, sNumLossesAboveTolAttr, 0, &theNumLossesAboveTol, sizeof(theNumLossesAboveTol));
				clearPercentLossThinCount = false;
			}
		}
		//check for a thickening condition
		else if (thePercentLoss < sLossThickTolerance)
		{
			theNumLossesBelowTol++;//we must count this loss
			if (theNumLossesBelowTol >= sLossesToThick)
			{
#if FLOW_CONTROL_DEBUGGING
				printf("Percent is low: ratcheting more\n");
#endif
				ratchetMore = true;
			}
			else
			{
#if FLOW_CONTROL_DEBUGGING
				printf("Percent is low: Incrementing percent loss count to %"   _U32BITARG_   "\n", theNumLossesBelowTol);
#endif
				(void)QTSS_SetValue(theStream, sNumLossesBelowTolAttr, 0, &theNumLossesBelowTol, sizeof(theNumLossesBelowTol));
				clearPercentLossThickCount = false;
			}
		}
	}

	//Now take a look at the getting worse heuristic
	(void)QTSS_GetValuePtr(inParams->inRTPStream, qtssRTPStrGettingWorse, 0, (void**)&uint16_tPtr, &theLen);
	if ((uint16_tPtr != NULL) && (theLen == sizeof(uint16_t)))
	{
		uint16_t isGettingWorse = *uint16_tPtr;
		if (isGettingWorse != 0)
		{
			theNumWorses++;//we must count this getting worse

			//If we've gotten N number of getting worses, then thin. Otherwise, just
			//increment our count of getting worses
			if (theNumWorses >= sWorsesToThin)
			{
#if FLOW_CONTROL_DEBUGGING
				printf("Client reporting getting worse. Ratcheting less\n");
#endif
				ratchetLess = true;
			}
			else
			{
#if FLOW_CONTROL_DEBUGGING
				printf("Client reporting getting worse. Incrementing num worses count to %"   _U32BITARG_   "\n", theNumWorses);
#endif
				(void)QTSS_SetValue(theStream, sNumWorsesAttr, 0, &theNumWorses, sizeof(theNumWorses));
			}
		}
	}

	//Finally, if we get a getting better, automatically ratchet up
	(void)QTSS_GetValuePtr(inParams->inRTPStream, qtssRTPStrGettingBetter, 0, (void**)&uint16_tPtr, &theLen);
	if ((uint16_tPtr != NULL) && (theLen == sizeof(uint16_t)) && (*uint16_tPtr > 0))
		ratchetMore = true;

	//For clearing out counts below
	uint32_t zero = 0;

	//Based on the ratchetMore / ratchetLess variables, adjust the stream
	if (ratchetMore || ratchetLess)
	{

		uint32_t curQuality = 0;
		(void)QTSS_GetValuePtr(theStream, qtssRTPStrQualityLevel, 0, (void**)&uint32_tPtr, &theLen);
		if ((uint32_tPtr != NULL) && (theLen == sizeof(uint32_t)))
			curQuality = *uint32_tPtr;

		uint32_t numQualityLevels = 0;
		(void)QTSS_GetValuePtr(theStream, qtssRTPStrNumQualityLevels, 0, (void**)&uint32_tPtr, &theLen);
		if ((uint32_tPtr != NULL) && (theLen == sizeof(uint32_t)))
			numQualityLevels = *uint32_tPtr;

		if ((ratchetLess) && (curQuality < numQualityLevels))
		{
			curQuality++;
			if (curQuality > 1) // v3.0.1=v2.0.1 make level 2 means key frames in the file or max if reflected.
				curQuality = numQualityLevels;
			(void)QTSS_SetValue(theStream, qtssRTPStrQualityLevel, 0, &curQuality, sizeof(curQuality));
		}
		else if ((ratchetMore) && (curQuality > 0))
		{
			curQuality--;
			if (curQuality > 1)  // v3.0.1=v2.0.1 make level 2 means key frames in the file or max if reflected.
				curQuality = 1;
			(void)QTSS_SetValue(theStream, qtssRTPStrQualityLevel, 0, &curQuality, sizeof(curQuality));
		}


		bool *startedThinningPtr = NULL;
		int32_t numThinned = 0;
		(void)QTSS_GetValuePtr(inParams->inClientSession, qtssCliSesStartedThinning, 0, (void**)&startedThinningPtr, &theLen);
		if (false == *startedThinningPtr)
		{
			(void)QTSS_LockObject(sServer);
			*startedThinningPtr = true;

			(void)QTSS_GetValue(sServer, qtssSvrNumThinned, 0, (void*)&numThinned, &theLen);
			numThinned++;
			(void)QTSS_SetValue(sServer, qtssSvrNumThinned, 0, &numThinned, sizeof(numThinned));
			(void)QTSS_UnlockObject(sServer);
		}
		else if (curQuality == 0)
		{
			(void)QTSS_LockObject(sServer);
			*startedThinningPtr = false;

			(void)QTSS_GetValue(theStream, qtssSvrNumThinned, 0, (void*)&numThinned, &theLen);
			numThinned--;
			(void)QTSS_SetValue(sServer, qtssSvrNumThinned, 0, &numThinned, sizeof(numThinned));
			(void)QTSS_UnlockObject(sServer);
		}
		//When adjusting the quality, ALWAYS clear out ALL our counts of EVERYTHING. Note
		//that this is the ONLY way that the fNumGettingWorses count gets cleared
		(void)QTSS_SetValue(theStream, sNumWorsesAttr, 0, &zero, sizeof(zero));
#if FLOW_CONTROL_DEBUGGING
		printf("Clearing num worses count\n");
#endif
		clearPercentLossThinCount = true;
		clearPercentLossThickCount = true;
	}

	//clear thick / thin counts if we are supposed to.
	if (clearPercentLossThinCount)
	{
#if FLOW_CONTROL_DEBUGGING
		printf("Clearing num losses above tolerance count\n");
#endif
		(void)QTSS_SetValue(theStream, sNumLossesAboveTolAttr, 0, &zero, sizeof(zero));
	}
	if (clearPercentLossThickCount)
	{
#if FLOW_CONTROL_DEBUGGING
		printf("Clearing num losses below tolerance count\n");
#endif

		(void)QTSS_SetValue(theStream, sNumLossesBelowTolAttr, 0, &zero, sizeof(zero));
	}
	return QTSS_NoErr;
}

void    InitializeDictionaryItems(QTSS_RTPStreamObject inStream)
{
	uint32_t* theValue = NULL;
	uint32_t theValueLen = 0;

	QTSS_Error theErr = QTSS_GetValuePtr(inStream, sNumLossesAboveTolAttr, 0, (void**)&theValue, &theValueLen);

	if (theErr != QTSS_NoErr)
	{
		// The dictionary parameters haven't been initialized yet. Just set them all to 0.
		(void)QTSS_SetValue(inStream, sNumLossesAboveTolAttr, 0, &theValueLen, sizeof(theValueLen));
		(void)QTSS_SetValue(inStream, sNumLossesBelowTolAttr, 0, &theValueLen, sizeof(theValueLen));
		(void)QTSS_SetValue(inStream, sNumWorsesAttr, 0, &theValueLen, sizeof(theValueLen));
	}
}
