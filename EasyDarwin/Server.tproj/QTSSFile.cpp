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
	 File:       QTSSFile.h

	 Contains:
 */

#include "QTSSFile.h"
#include "QTSServerInterface.h"

QTSSAttrInfoDict::AttrInfo  QTSSFile::sAttributes[] =
{   /*fields:   fAttrName, fFuncPtr, fAttrDataType, fAttrPermission */
	/* 0 */ { "qtssFlObjStream",                nullptr,   qtssAttrDataTypeQTSS_StreamRef, qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 1 */ { "qtssFlObjFileSysModuleName",     nullptr,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 2 */ { "qtssFlObjLength",                nullptr,   qtssAttrDataTypeuint64_t,         qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
	/* 3 */ { "qtssFlObjPosition",              nullptr,   qtssAttrDataTypeuint64_t,         qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 4 */ { "qtssFlObjModDate",               nullptr,   qtssAttrDataTypeuint64_t,         qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite }
};

void    QTSSFile::Initialize()
{
	for (uint32_t x = 0; x < qtssFlObjNumParams; x++)
		QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kFileDictIndex)->
		SetAttribute(x, sAttributes[x].fAttrName, sAttributes[x].fFuncPtr, sAttributes[x].fAttrDataType, sAttributes[x].fAttrPermission);
}

QTSSFile::QTSSFile()
	: QTSSDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kFileDictIndex)),
	fModule(nullptr),
	fPosition(0),
	fLength(0),
	fModDate(0)
{
	fThisPtr = this;
	//
	// The stream is just a pointer to this thing
	this->SetVal(qtssFlObjStream, &fThisPtr, sizeof(fThisPtr));
	this->SetVal(qtssFlObjLength, &fLength, sizeof(fLength));
	this->SetVal(qtssFlObjPosition, &fPosition, sizeof(fPosition));
	this->SetVal(qtssFlObjModDate, &fModDate, sizeof(fModDate));
}

QTSS_Error  QTSSFile::Open(char* inPath, QTSS_OpenFileFlags inFlags)
{
	//
	// Because this is a role being executed from inside a callback, we need to
	// make sure that QTSS_RequestEvent will not work.
	Task* curTask = nullptr;
	auto* theState = (QTSS_ModuleState*)OSThread::GetMainThreadData();
	if (OSThread::GetCurrent() != nullptr)
		theState = (QTSS_ModuleState*)OSThread::GetCurrent()->GetThreadData();

	if (theState != nullptr)
		curTask = theState->curTask;

	QTSS_RoleParams theParams;
	theParams.openFileParams.inPath = inPath;
	theParams.openFileParams.inFlags = inFlags;
	theParams.openFileParams.inFileObject = this;

	QTSS_Error theErr = QTSS_FileNotFound;
	uint32_t x = 0;

	for (; x < QTSServerInterface::GetNumModulesInRole(QTSSModule::kOpenFilePreProcessRole); x++)
	{
		theErr = QTSServerInterface::GetModule(QTSSModule::kOpenFilePreProcessRole, x)->CallDispatch(QTSS_OpenFilePreProcess_Role, &theParams);
		if (theErr != QTSS_FileNotFound)
		{
			fModule = QTSServerInterface::GetModule(QTSSModule::kOpenFilePreProcessRole, x);
			break;
		}
	}

	if (theErr == QTSS_FileNotFound)
	{
		// None of the prepreprocessors claimed this file. Invoke the default file handler
		if (QTSServerInterface::GetNumModulesInRole(QTSSModule::kOpenFileRole) > 0)
		{
			fModule = QTSServerInterface::GetModule(QTSSModule::kOpenFileRole, 0);
			theErr = QTSServerInterface::GetModule(QTSSModule::kOpenFileRole, 0)->CallDispatch(QTSS_OpenFile_Role, &theParams);

		}
	}

	//
	// Reset the curTask to what it was before this role started
	if (theState != nullptr)
		theState->curTask = curTask;

	return theErr;
}

void    QTSSFile::Close()
{
	Assert(fModule != nullptr);

	QTSS_RoleParams theParams;
	theParams.closeFileParams.inFileObject = this;
	(void)fModule->CallDispatch(QTSS_CloseFile_Role, &theParams);
}


QTSS_Error  QTSSFile::Read(void* ioBuffer, uint32_t inBufLen, uint32_t* outLengthRead)
{
	Assert(fModule != nullptr);
	uint32_t theLenRead = 0;

	//
	// Invoke the owning QTSS API module. Setup a param block to do so.
	QTSS_RoleParams theParams;
	theParams.readFileParams.inFileObject = this;
	theParams.readFileParams.inFilePosition = fPosition;
	theParams.readFileParams.ioBuffer = ioBuffer;
	theParams.readFileParams.inBufLen = inBufLen;
	theParams.readFileParams.outLenRead = &theLenRead;

	QTSS_Error theErr = fModule->CallDispatch(QTSS_ReadFile_Role, &theParams);

	fPosition += theLenRead;
	if (outLengthRead != nullptr)
		*outLengthRead = theLenRead;

	return theErr;
}

QTSS_Error  QTSSFile::Seek(uint64_t inNewPosition)
{
	uint64_t* theFileLength = nullptr;
	uint32_t theParamLength = 0;

	(void)this->GetValuePtr(qtssFlObjLength, 0, (void**)&theFileLength, &theParamLength);

	if (theParamLength != sizeof(uint64_t))
		return QTSS_RequestFailed;

	if (inNewPosition > *theFileLength)
		return QTSS_RequestFailed;

	fPosition = inNewPosition;
	return QTSS_NoErr;
}

QTSS_Error  QTSSFile::Advise(uint64_t inPosition, uint32_t inAdviseSize)
{
	Assert(fModule != nullptr);

	//
	// Invoke the owning QTSS API module. Setup a param block to do so.
	QTSS_RoleParams theParams;
	theParams.adviseFileParams.inFileObject = this;
	theParams.adviseFileParams.inPosition = inPosition;
	theParams.adviseFileParams.inSize = inAdviseSize;

	return fModule->CallDispatch(QTSS_AdviseFile_Role, &theParams);
}

QTSS_Error  QTSSFile::RequestEvent(QTSS_EventType inEventMask)
{
	Assert(fModule != nullptr);

	//
	// Invoke the owning QTSS API module. Setup a param block to do so.
	QTSS_RoleParams theParams;
	theParams.reqEventFileParams.inFileObject = this;
	theParams.reqEventFileParams.inEventMask = inEventMask;

	return fModule->CallDispatch(QTSS_RequestEventFile_Role, &theParams);
}
