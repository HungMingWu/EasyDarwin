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
	 File:       QTSSPrefs.cpp

	 Contains:   Implements class defined in QTSSPrefs.h.

	 Change History (most recent first):

 */

#include "QTSSPrefs.h"
#include "MyAssert.h"
#include "QTSSDataConverter.h"
#include "OSArrayObjectDeleter.h"
#include <APICommonCode/QTSSModuleUtils.h>


QTSSPrefs::QTSSPrefs(XMLPrefsParser* inPrefsSource, StrPtrLen* inModuleName, QTSSDictionaryMap* inMap,
	bool areInstanceAttrsAllowed, QTSSPrefs* parentDictionary)
	: QTSSDictionary(inMap, &fPrefsMutex),
	fPrefsSource(inPrefsSource),
	fPrefName(NULL),
	fParentDictionary(parentDictionary)
{
	if (inModuleName != NULL)
		fPrefName = inModuleName->GetAsCString();
}

QTSSDictionary* QTSSPrefs::CreateNewDictionary(QTSSDictionaryMap* inMap, OSMutex* /* inMutex */)
{
	return new QTSSPrefs(fPrefsSource, NULL, inMap, true, this);
}

void QTSSPrefs::RereadPreferences()
{
	RereadObjectPreferences(GetContainerRef());
}

void QTSSPrefs::RereadObjectPreferences(ContainerRef container)
{
	QTSS_Error theErr = QTSS_NoErr;

	//
	// Keep track of which pref attributes should remain. All others
	// will be removed.
	// This routine uses names because it adds and deletes attributes. This means attribute indexes,positions and counts are constantly changing.
	uint32_t initialNumAttrs = 0;
	if (this->GetInstanceDictMap() != NULL)
	{
		initialNumAttrs = this->GetInstanceDictMap()->GetNumAttrs();
	};

	char** modulePrefInServer;
	if (initialNumAttrs > 0)
	{
		modulePrefInServer = new char*[initialNumAttrs];
		::memset(modulePrefInServer, 0, sizeof(char*) * initialNumAttrs);
	}
	else
	{
		modulePrefInServer = NULL;
	}

	OSMutexLocker locker(&fPrefsMutex);
	uint32_t theNumPrefs = fPrefsSource->GetNumPrefsByContainer(container);

	for (uint32_t i = 0; i < initialNumAttrs; i++) // pull out all the names in the server 
	{
		QTSSAttrInfoDict* theAttrInfoPtr = NULL;
		theErr = this->GetInstanceDictMap()->GetAttrInfoByIndex(i, &theAttrInfoPtr);
		if (theErr != QTSS_NoErr)
			continue;

		uint32_t nameLen = 0;
		theErr = theAttrInfoPtr->GetValuePtr(qtssAttrName, 0, (void **)&modulePrefInServer[i], &nameLen);
		Assert(theErr == QTSS_NoErr);
		//printf("QTSSPrefs::RereadPreferences modulePrefInServer in server=%s\n",modulePrefInServer[i]);
	}

	// Use the names of the attributes in the attribute map as the key values for
	// finding preferences in the config file.

	for (uint32_t x = 0; x < theNumPrefs; x++)
	{
		char* thePrefTypeStr = NULL;
		char* thePrefName = NULL;
		(void)fPrefsSource->GetPrefValueByIndex(container, x, 0, &thePrefName, &thePrefTypeStr);

		// What type is this data type?
		QTSS_AttrDataType thePrefType = QTSSDataConverter::TypeStringToType(thePrefTypeStr);

		//
		// Check to see if there is an attribute with this name already in the
		// instance map. If one matches, then we don't need to add this attribute.
		QTSSAttrInfoDict* theAttrInfo = NULL;
		if (this->GetInstanceDictMap() != NULL)
			(void)this->GetInstanceDictMap()->GetAttrInfoByName(thePrefName,
				&theAttrInfo,
				false); // false=don't return info on deleted attributes
		uint32_t theLen = sizeof(QTSS_AttrDataType);
		QTSS_AttributeID theAttrID = qtssIllegalAttrID;

		for (uint32_t i = 0; i < initialNumAttrs; i++) // see if this name is in the server
		{
			if (modulePrefInServer[i] != NULL && thePrefName != NULL && 0 == ::strcmp(modulePrefInServer[i], thePrefName))
			{
				modulePrefInServer[i] = NULL; // in the server so don't delete later
			 //printf("QTSSPrefs::RereadPreferences modulePrefInServer in file and in server=%s\n",thePrefName);
			}
		}

		if (theAttrInfo == NULL)
		{
			theAttrID = this->addPrefAttribute(thePrefName, thePrefType); // not present or deleted
			this->SetPrefValuesFromFile(container, x, theAttrID, 0); // will add another or replace a deleted attribute
		}
		else
		{
			QTSS_AttrDataType theAttrType = qtssAttrDataTypeUnknown;
			theErr = theAttrInfo->GetValue(qtssAttrDataType, 0, &theAttrType, &theLen);
			Assert(theErr == QTSS_NoErr);

			theLen = sizeof(theAttrID);
			theErr = theAttrInfo->GetValue(qtssAttrID, 0, &theAttrID, &theLen);
			Assert(theErr == QTSS_NoErr);

			if (theAttrType != thePrefType)
			{
				//
				// This is not the same pref as before, because the data types
				// are different. Remove the old one from the map, add the new one.
				(void)this->RemoveInstanceAttribute(theAttrID);
				theAttrID = this->addPrefAttribute(thePrefName, thePrefType);
			}
			else
			{
				//
				// This pref already exists
			}

			//
			// Set the values
			this->SetPrefValuesFromFile(container, x, theAttrID, 0);

			// Mark this pref as found.
			int32_t theIndex = this->GetInstanceDictMap()->ConvertAttrIDToArrayIndex(theAttrID);
			Assert(theIndex >= 0);
		}
	}

	// Remove all attributes that no longer apply
	if (this->GetInstanceDictMap() != NULL && initialNumAttrs > 0)
	{
		for (uint32_t a = 0; a < initialNumAttrs; a++)
		{
			if (NULL != modulePrefInServer[a]) // found a pref in the server that wasn't in the file
			{
				QTSSAttrInfoDict* theAttrInfoPtr = NULL;
				theErr = this->GetInstanceDictMap()->GetAttrInfoByName(modulePrefInServer[a], &theAttrInfoPtr);
				Assert(theErr == QTSS_NoErr);
				if (theErr != QTSS_NoErr) continue;

				QTSS_AttributeID theAttrID = qtssIllegalAttrID;
				uint32_t theLen = sizeof(theAttrID);
				theErr = theAttrInfoPtr->GetValue(qtssAttrID, 0, &theAttrID, &theLen);
				Assert(theErr == QTSS_NoErr);
				if (theErr != QTSS_NoErr) continue;

				if (0)
				{
					char* theName = NULL;
					uint32_t nameLen = 0;
					theAttrInfoPtr->GetValuePtr(qtssAttrName, 0, (void **)&theName, &nameLen);
					printf("QTSSPrefs::RereadPreferences about to delete modulePrefInServer=%s attr=%s id=%"   _U32BITARG_   "\n", modulePrefInServer[a], theName, theAttrID);
				}


				this->GetInstanceDictMap()->RemoveAttribute(theAttrID);
				modulePrefInServer[a] = NULL;
			}
		}
	}

	delete modulePrefInServer;
}

void QTSSPrefs::SetPrefValuesFromFile(ContainerRef container, uint32_t inPrefIndex, QTSS_AttributeID inAttrID, uint32_t inNumValues)
{
	ContainerRef pref = fPrefsSource->GetPrefRefByIndex(container, inPrefIndex);
	SetPrefValuesFromFileWithRef(pref, inAttrID, inNumValues);
}

void QTSSPrefs::SetPrefValuesFromFileWithRef(ContainerRef pref, QTSS_AttributeID inAttrID, uint32_t inNumValues)
{
	//
	// We have an attribute ID for this pref, it is in the map and everything.
	// Now, let's add all the values that are in the pref file.
	if (pref == 0)
		return;

	uint32_t numPrefValues = inNumValues;
	if (inNumValues == 0)
		numPrefValues = fPrefsSource->GetNumPrefValues(pref);

	char* thePrefName = NULL;
	char* thePrefTypeStr = NULL;
	QTSS_AttrDataType thePrefType = qtssAttrDataTypeUnknown;

	// find the type.  If this is a QTSSObject, then we need to call a different routine
	char* thePrefValue = fPrefsSource->GetPrefValueByRef(pref, 0, &thePrefName, &thePrefTypeStr);
	thePrefType = QTSSDataConverter::TypeStringToType(thePrefTypeStr);
	if (thePrefType == qtssAttrDataTypeQTSS_Object)
	{
		SetObjectValuesFromFile(pref, inAttrID, numPrefValues, thePrefName);
		return;
	}

	uint32_t maxPrefValueSize = 0;
	QTSS_Error theErr = QTSS_NoErr;

	//
	// We have to loop through all the values associated with this pref twice:
	// first, to figure out the length (in bytes) of the longest value, secondly
	// to actually copy these values into the dictionary.
	for (uint32_t y = 0; y < numPrefValues; y++)
	{
		uint32_t tempMaxPrefValueSize = 0;

		thePrefValue = fPrefsSource->GetPrefValueByRef(pref, y, &thePrefName, &thePrefTypeStr);

		theErr = QTSSDataConverter::StringToValue(thePrefValue, thePrefType,
			NULL, &tempMaxPrefValueSize);
		Assert(theErr == QTSS_NotEnoughSpace);

		if (tempMaxPrefValueSize > maxPrefValueSize)
			maxPrefValueSize = tempMaxPrefValueSize;
	}


	for (uint32_t z = 0; z < numPrefValues; z++)
	{
		thePrefValue = fPrefsSource->GetPrefValueByRef(pref, z, &thePrefName, &thePrefTypeStr);
		this->SetPrefValue(inAttrID, z, thePrefValue, thePrefType, maxPrefValueSize);
	}

	//
	// Make sure the dictionary knows exactly how many values are associated with
	// this pref
	this->SetNumValues(inAttrID, numPrefValues);
}

void QTSSPrefs::SetObjectValuesFromFile(ContainerRef pref, QTSS_AttributeID inAttrID, uint32_t inNumValues, char* prefName)
{
	for (uint32_t z = 0; z < inNumValues; z++)
	{
		ContainerRef object = fPrefsSource->GetObjectValue(pref, z);
		QTSSPrefs* prefObject;
		uint32_t len = sizeof(QTSSPrefs*);
		QTSS_Error err = this->GetValue(inAttrID, z, &prefObject, &len);
		if (err != QTSS_NoErr)
		{
			uint32_t tempIndex;
			err = CreateObjectValue(inAttrID, &tempIndex, (QTSSDictionary**)&prefObject, NULL, QTSSDictionary::kDontObeyReadOnly | QTSSDictionary::kDontCallCompletionRoutine);
			Assert(err == QTSS_NoErr);
			Assert(tempIndex == z);
			if (err != QTSS_NoErr)  // this shouldn't happen
				return;
			StrPtrLen temp(prefName);
			prefObject->fPrefName = temp.GetAsCString();
		}
		prefObject->RereadObjectPreferences(object);
	}

	//
	// Make sure the dictionary knows exactly how many values are associated with
	// this pref
	this->SetNumValues(inAttrID, inNumValues);
}

void    QTSSPrefs::SetPrefValue(QTSS_AttributeID inAttrID, uint32_t inAttrIndex,
	char* inPrefValue, QTSS_AttrDataType inPrefType, uint32_t inValueSize)

{
	static const uint32_t kMaxPrefValueSize = 1024;
	char convertedPrefValue[kMaxPrefValueSize];
	::memset(convertedPrefValue, 0, kMaxPrefValueSize);
	Assert(inValueSize < kMaxPrefValueSize);

	uint32_t convertedBufSize = kMaxPrefValueSize;
	QTSS_Error theErr = QTSSDataConverter::StringToValue
	(inPrefValue, inPrefType, convertedPrefValue, &convertedBufSize);
	Assert(theErr == QTSS_NoErr);

	if (inValueSize == 0)
		inValueSize = convertedBufSize;

	this->SetValue(inAttrID, inAttrIndex, convertedPrefValue, inValueSize, QTSSDictionary::kDontObeyReadOnly | QTSSDictionary::kDontCallCompletionRoutine);

}

QTSS_AttributeID QTSSPrefs::addPrefAttribute(const char* inAttrName, QTSS_AttrDataType inDataType)
{
	QTSS_Error theErr = this->AddInstanceAttribute(inAttrName, NULL, inDataType, qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModeDelete);
	Assert(theErr == QTSS_NoErr);

	QTSS_AttributeID theID = qtssIllegalAttrID;
	theErr = this->GetInstanceDictMap()->GetAttrID(inAttrName, &theID);
	Assert(theErr == QTSS_NoErr);

	return theID;
}

void    QTSSPrefs::RemoveValueComplete(uint32_t inAttrIndex, QTSSDictionaryMap* inMap,
	uint32_t inValueIndex)
{
	ContainerRef objectRef = GetContainerRef();
	ContainerRef pref = fPrefsSource->GetPrefRefByName(objectRef, inMap->GetAttrName(inAttrIndex));
	Assert(pref != NULL);
	if (pref != NULL)
		fPrefsSource->RemovePrefValue(pref, inValueIndex);

	if (fPrefsSource->WritePrefsFile())
		QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgCantWriteFile, 0);
}

void    QTSSPrefs::RemoveInstanceAttrComplete(uint32_t inAttrIndex, QTSSDictionaryMap* inMap)
{
	ContainerRef objectRef = GetContainerRef();
	ContainerRef pref = fPrefsSource->GetPrefRefByName(objectRef, inMap->GetAttrName(inAttrIndex));
	Assert(pref != NULL);
	if (pref != NULL)
	{
		fPrefsSource->RemovePref(pref);
	}

	if (fPrefsSource->WritePrefsFile())
		QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgCantWriteFile, 0);
}

void QTSSPrefs::SetValueComplete(uint32_t inAttrIndex, QTSSDictionaryMap* inMap,
	uint32_t inValueIndex, void* inNewValue, uint32_t inNewValueLen)
{
	ContainerRef objectRef = GetContainerRef();
	ContainerRef pref = fPrefsSource->AddPref(objectRef, inMap->GetAttrName(inAttrIndex), QTSSDataConverter::TypeToTypeString(inMap->GetAttrType(inAttrIndex)));
	if (inMap->GetAttrType(inAttrIndex) == qtssAttrDataTypeQTSS_Object)
	{
		QTSSPrefs* object = *(QTSSPrefs**)inNewValue;   // value is a pointer to a QTSSPrefs object
		StrPtrLen temp(inMap->GetAttrName(inAttrIndex));
		object->fPrefName = temp.GetAsCString();
		if (inValueIndex == fPrefsSource->GetNumPrefValues(pref))
			fPrefsSource->AddNewObject(pref);
	}
	else
	{
		OSCharArrayDeleter theValueAsString(QTSSDataConverter::ValueToString(inNewValue, inNewValueLen, inMap->GetAttrType(inAttrIndex)));
		fPrefsSource->SetPrefValue(pref, inValueIndex, theValueAsString.GetObject());
	}

	if (fPrefsSource->WritePrefsFile())
		QTSSModuleUtils::LogError(qtssWarningVerbosity, qtssMsgCantWriteFile, 0);
}

ContainerRef QTSSPrefs::GetContainerRefForObject(QTSSPrefs* object)
{
	ContainerRef thisContainer = GetContainerRef();
	ContainerRef pref = fPrefsSource->GetPrefRefByName(thisContainer, object->fPrefName);
	if (pref == NULL)
		return NULL;

	if (fPrefsSource->GetNumPrefValues(pref) <= 1)
		return fPrefsSource->GetObjectValue(pref, 0);

	QTSSAttrInfoDict* theAttrInfoPtr = NULL;
	QTSS_Error theErr = this->GetInstanceDictMap()->GetAttrInfoByName(object->fPrefName, &theAttrInfoPtr);
	Assert(theErr == QTSS_NoErr);
	if (theErr != QTSS_NoErr) return NULL;
	QTSS_AttributeID theAttrID = qtssIllegalAttrID;
	uint32_t len = sizeof(theAttrID);
	theErr = theAttrInfoPtr->GetValue(qtssAttrID, 0, &theAttrID, &len);
	Assert(theErr == QTSS_NoErr);
	if (theErr != QTSS_NoErr) return NULL;

	uint32_t index = 0;
	QTSSPrefs* prefObject;
	len = sizeof(prefObject);
	while (this->GetValue(theAttrID, index, &prefObject, &len) == QTSS_NoErr)
	{
		if (prefObject == object)
		{
			return fPrefsSource->GetObjectValue(pref, index);
		}
	}

	return NULL;
}

ContainerRef QTSSPrefs::GetContainerRef()
{
	if (fParentDictionary == NULL)  // this is a top level Pref, so it must be a module
		return fPrefsSource->GetRefForModule(fPrefName);
	else
		return fParentDictionary->GetContainerRefForObject(this);
}
