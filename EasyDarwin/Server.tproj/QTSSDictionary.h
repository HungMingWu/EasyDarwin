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
	 File:       QTSSDictionary.h

	 Contains:   Definitions of two classes: QTSSDictionary and QTSSDictionaryMap.
				 Collectively, these classes implement the "dictionary" APIs in QTSS
				 API. A QTSSDictionary corresponds to a QTSS_Object,
				 a QTSSDictionaryMap corresponds to a QTSS_ObjectType.

	 Created: Tue, Mar 2, 1999 @ 4:23 PM
 */



#ifndef _QTSSDICTIONARY_H_
#define _QTSSDICTIONARY_H_

#include <stdlib.h>
#include "QTSS.h"
#include "OSHeaders.h"
#include "OSMutex.h"
#include "StrPtrLen.h"
#include "MyAssert.h"
#include "QTSSStream.h"

class QTSSDictionary;
class QTSSDictionaryMap;
class QTSSAttrInfoDict;

#define __DICTIONARY_TESTING__ 0

//
// Function prototype for attr functions
typedef void* (*QTSS_AttrFunctionPtr)(QTSSDictionary*, uint32_t*);

class QTSSDictionary : public QTSSStream
{
public:

	//
	// CONSTRUCTOR / DESTRUCTOR

	QTSSDictionary(QTSSDictionaryMap* inMap, OSMutex* inMutex = nullptr);
	~QTSSDictionary() override;

	//
	// QTSS API CALLS

	// Flags used by internal callers of these routines
	enum
	{
		kNoFlags = 0,
		kDontObeyReadOnly = 1,
		kDontCallCompletionRoutine = 2
	};

	// This version of GetValue copies the element into a buffer provided by the caller
	// Returns:     QTSS_BadArgument, QTSS_NotPreemptiveSafe (if attribute is not preemptive safe),
	//              QTSS_BadIndex (if inIndex is bad)
	QTSS_Error GetValue(QTSS_AttributeID inAttrID, uint32_t inIndex, void* ioValueBuffer, uint32_t* ioValueLen);


	//This version of GetValue returns a pointer to the internal buffer for the attribute.
	//Only usable if the attribute is preemptive safe.
	//
	// Returns:     Same as above, but also QTSS_NotEnoughSpace, if value is too big for buffer.
	QTSS_Error GetValuePtr(QTSS_AttributeID inAttrID, uint32_t inIndex, void** outValueBuffer, uint32_t* outValueLen)
	{
		return GetValuePtr(inAttrID, inIndex, outValueBuffer, outValueLen, false);
	}

	// This version of GetValue converts the value to a string before returning it. Memory for
	// the string is allocated internally.
	//
	// Returns: QTSS_BadArgument, QTSS_BadIndex, QTSS_ValueNotFound
	QTSS_Error GetValueAsString(QTSS_AttributeID inAttrID, uint32_t inIndex, char** outString);

	// Returns:     QTSS_BadArgument, QTSS_ReadOnly (if attribute is read only),
	//              QTSS_BadIndex (attempt to set indexed parameter with param retrieval)
	QTSS_Error SetValue(QTSS_AttributeID inAttrID, uint32_t inIndex,
		const void* inBuffer, uint32_t inLen, uint32_t inFlags = kNoFlags);

	// Returns:     QTSS_BadArgument, QTSS_ReadOnly (if attribute is read only),
	QTSS_Error SetValuePtr(QTSS_AttributeID inAttrID,
		const void* inBuffer, uint32_t inLen, uint32_t inFlags = kNoFlags);

	// Returns:     QTSS_BadArgument, QTSS_ReadOnly (if attribute is read only),
	QTSS_Error CreateObjectValue(QTSS_AttributeID inAttrID, uint32_t* outIndex,
		QTSSDictionary** newObject, QTSSDictionaryMap* inMap = nullptr,
		uint32_t inFlags = kNoFlags);

	// Returns:     QTSS_BadArgument, QTSS_ReadOnly, QTSS_BadIndex
	QTSS_Error RemoveValue(QTSS_AttributeID inAttrID, uint32_t inIndex, uint32_t inFlags = kNoFlags);

	// Utility routine used by the two external flavors of GetValue
	QTSS_Error GetValuePtr(QTSS_AttributeID inAttrID, uint32_t inIndex,
		void** outValueBuffer, uint32_t* outValueLen,
		bool isInternal);

	//
	// ACCESSORS

	QTSSDictionaryMap*  GetDictionaryMap() { return fMap; }

	// Returns the Instance dictionary map for this dictionary. This may return NULL
	// if there are no instance attributes in this dictionary
	QTSSDictionaryMap*  GetInstanceDictMap() { return fInstanceMap; }

	// Returns the number of values associated with a given attribute
	uint32_t              GetNumValues(QTSS_AttributeID inAttrID);
	void                SetNumValues(QTSS_AttributeID inAttrID, uint32_t inNumValues);

	// Meant only for internal server use. Does no error checking,
	// doesn't invoke the param retrieval function.
	StrPtrLen*  GetValue(QTSS_AttributeID inAttrID)
	{
		return &fAttributes[inAttrID].fAttributeData;
	}

	OSMutex*    GetMutex() { return fMutexP; }

	void		SetLocked(bool inLocked) { fLocked = inLocked; }
	bool		IsLocked() { return fLocked; }

	//
	// GETTING ATTRIBUTE INFO
	QTSS_Error GetAttrInfoByIndex(uint32_t inIndex, QTSSAttrInfoDict** outAttrInfoDict);
	QTSS_Error GetAttrInfoByName(const char* inAttrName, QTSSAttrInfoDict** outAttrInfoDict);
	QTSS_Error GetAttrInfoByID(QTSS_AttributeID inAttrID, QTSSAttrInfoDict** outAttrInfoDict);


	//
	// INSTANCE ATTRIBUTES

	QTSS_Error  AddInstanceAttribute(const char* inAttrName,
		QTSS_AttrFunctionPtr inFuncPtr,
		QTSS_AttrDataType inDataType,
		QTSS_AttrPermission inPermission);

	QTSS_Error  RemoveInstanceAttribute(QTSS_AttributeID inAttr);
	//
	// MODIFIERS

	// These functions are meant to be used by the server when it is setting up the
	// dictionary attributes. They do no error checking.

	// They don't set fNumAttributes & fAllocatedInternally.
	void    SetVal(QTSS_AttributeID inAttrID, void* inValueBuffer, uint32_t inBufferLen);
	void    SetVal(QTSS_AttributeID inAttrID, StrPtrLen* inNewValue)
	{
		this->SetVal(inAttrID, inNewValue->Ptr, inNewValue->Len);
	}

	// Call this if you want to assign empty storage to an attribute
	void    SetEmptyVal(QTSS_AttributeID inAttrID, void* inBuf, uint32_t inBufLen);

#if __DICTIONARY_TESTING__
	static void Test(); // API test for these objects
#endif

protected:

	// Derived classes can provide a completion routine for some dictionary functions
	virtual void    RemoveValueComplete(uint32_t /*inAttrIndex*/, QTSSDictionaryMap* /*inMap*/, uint32_t /*inValueIndex*/) {}

	virtual void    SetValueComplete(uint32_t /*inAttrIndex*/, QTSSDictionaryMap* /*inMap*/,
		uint32_t /*inValueIndex*/, void* /*inNewValue*/, uint32_t /*inNewValueLen*/) {}
	virtual void    RemoveInstanceAttrComplete(uint32_t /*inAttrindex*/, QTSSDictionaryMap* /*inMap*/) {}

	virtual QTSSDictionary* CreateNewDictionary(QTSSDictionaryMap* inMap, OSMutex* inMutex);

private:

	struct DictValueElement
	{
		// This stores all necessary information for each attribute value.

		DictValueElement() {}

		// Does not delete! You Must call DeleteAttributeData for that
		~DictValueElement() = default;

		StrPtrLen   fAttributeData; // The data
		uint32_t      fAllocatedLen{0};  // How much space do we have allocated?
		uint32_t      fNumAttributes{0}; // If this is an iterated attribute, how many?
		bool      fAllocatedInternally{false}; //Should we delete this memory?
		bool      fIsDynamicDictionary{false}; //is this a dictionary object?
	};

	DictValueElement    fAttributes[QTSS_MAX_ATTRIBUTE_NUMS];
	DictValueElement*   fInstanceAttrs;
	uint32_t              fInstanceArraySize;
	QTSSDictionaryMap*  fMap;
	QTSSDictionaryMap*  fInstanceMap;
	OSMutex*            fMutexP;
	bool				fMyMutex;
	bool				fLocked;

	void DeleteAttributeData(DictValueElement* inDictValues,
		uint32_t inNumValues, QTSSDictionaryMap* theMap);
};


class QTSSAttrInfoDict : public QTSSDictionary
{
public:

	struct AttrInfo
	{
		// This is all the relevent information for each dictionary
		// attribute.
		char                    fAttrName[QTSS_MAX_ATTRIBUTE_NAME_SIZE + 1];
		QTSS_AttrFunctionPtr    fFuncPtr;
		QTSS_AttrDataType       fAttrDataType;
		QTSS_AttrPermission     fAttrPermission;
	};

	QTSSAttrInfoDict();
	~QTSSAttrInfoDict() override;

private:

	AttrInfo fAttrInfo;
	QTSS_AttributeID fID{qtssIllegalAttrID};

	static AttrInfo sAttributes[];

	friend class QTSSDictionaryMap;

};

class QTSSDictionaryMap
{
public:

	//
	// This must be called before using any QTSSDictionary or QTSSDictionaryMap functionality
	static void Initialize();

	// Stores all meta-information for attributes

	// CONSTRUCTOR FLAGS
	enum
	{
		kNoFlags = 0,
		kAllowRemoval = 1,
		kIsInstanceMap = 2,
		kInstanceAttrsAllowed = 4,
		kCompleteFunctionsAllowed = 8
	};

	//
	// CONSTRUCTOR / DESTRUCTOR

	QTSSDictionaryMap(uint32_t inNumReservedAttrs, uint32_t inFlags = kNoFlags);
	~QTSSDictionaryMap() {
		for (uint32_t i = 0; i < fAttrArraySize; i++)
			delete fAttrArray[i];
		delete[] fAttrArray;
	}

	//
	// QTSS API CALLS

	// All functions either return QTSS_BadArgument or QTSS_NoErr
	QTSS_Error      AddAttribute(const char* inAttrName,
		QTSS_AttrFunctionPtr inFuncPtr,
		QTSS_AttrDataType inDataType,
		QTSS_AttrPermission inPermission);

	//
	// Marks this attribute as removed
	QTSS_Error  RemoveAttribute(QTSS_AttributeID inAttrID);
	QTSS_Error  UnRemoveAttribute(QTSS_AttributeID inAttrID);
	QTSS_Error  CheckRemovePermission(QTSS_AttributeID inAttrID);

	//
	// Searching / Iteration. These never return removed attributes
	QTSS_Error  GetAttrInfoByName(const char* inAttrName, QTSSAttrInfoDict** outAttrInfoDict, bool returnRemovedAttr = false);
	QTSS_Error  GetAttrInfoByID(QTSS_AttributeID inID, QTSSAttrInfoDict** outAttrInfoDict);
	QTSS_Error  GetAttrInfoByIndex(uint32_t inIndex, QTSSAttrInfoDict** outAttrInfoDict);
	QTSS_Error  GetAttrID(const char* inAttrName, QTSS_AttributeID* outID);

	//
	// PRIVATE ATTR PERMISSIONS
	enum
	{
		qtssPrivateAttrModeRemoved = 0x80000000
	};

	//
	// CONVERTING attribute IDs to array indexes. Returns -1 if inAttrID doesn't exist
	inline int32_t                   ConvertAttrIDToArrayIndex(QTSS_AttributeID inAttrID);

	static bool           IsInstanceAttrID(QTSS_AttributeID inAttrID)
	{
		return (inAttrID & 0x80000000) != 0;
	}

	// ACCESSORS

	// These functions do no error checking. Be careful.

	// Includes removed attributes
	uint32_t          GetNumAttrs() { return fNextAvailableID; }
	uint32_t          GetNumNonRemovedAttrs() { return fNumValidAttrs; }

	bool                  IsPreemptiveSafe(uint32_t inIndex)
	{
		Assert(inIndex < fNextAvailableID); return (bool)(fAttrArray[inIndex]->fAttrInfo.fAttrPermission & qtssAttrModePreempSafe);
	}

	bool                  IsWriteable(uint32_t inIndex)
	{
		Assert(inIndex < fNextAvailableID); return (bool)(fAttrArray[inIndex]->fAttrInfo.fAttrPermission & qtssAttrModeWrite);
	}

	bool                  IsCacheable(uint32_t inIndex)
	{
		Assert(inIndex < fNextAvailableID); return (bool)(fAttrArray[inIndex]->fAttrInfo.fAttrPermission & qtssAttrModeCacheable);
	}

	bool                  IsRemoved(uint32_t inIndex)
	{
		Assert(inIndex < fNextAvailableID); return (bool)(fAttrArray[inIndex]->fAttrInfo.fAttrPermission & qtssPrivateAttrModeRemoved);
	}

	QTSS_AttrFunctionPtr    GetAttrFunction(uint32_t inIndex)
	{
		Assert(inIndex < fNextAvailableID); return fAttrArray[inIndex]->fAttrInfo.fFuncPtr;
	}

	char*                   GetAttrName(uint32_t inIndex)
	{
		Assert(inIndex < fNextAvailableID); return fAttrArray[inIndex]->fAttrInfo.fAttrName;
	}

	QTSS_AttributeID        GetAttrID(uint32_t inIndex)
	{
		Assert(inIndex < fNextAvailableID); return fAttrArray[inIndex]->fID;
	}

	QTSS_AttrDataType       GetAttrType(uint32_t inIndex)
	{
		Assert(inIndex < fNextAvailableID); return fAttrArray[inIndex]->fAttrInfo.fAttrDataType;
	}

	bool                  InstanceAttrsAllowed() { return (bool)(fFlags & kInstanceAttrsAllowed); }
	bool                  CompleteFunctionsAllowed() { return (bool)(fFlags & kCompleteFunctionsAllowed); }

	// MODIFIERS

	// Sets this attribute ID to have this information

	void        SetAttribute(QTSS_AttributeID inID,
		const char* inAttrName,
		QTSS_AttrFunctionPtr inFuncPtr,
		QTSS_AttrDataType inDataType,
		QTSS_AttrPermission inPermission);


	//
	// DICTIONARY MAPS

	// All dictionary maps are stored here, and are accessable
	// through these routines

	// This enum allows all QTSSDictionaryMaps to be stored in an array 
	enum
	{
		kServerDictIndex = 0,
		kPrefsDictIndex = 1,
		kTextMessagesDictIndex = 2,
		kServiceDictIndex = 3,

		kRTPStreamDictIndex = 4,
		kClientSessionDictIndex = 5,
		kRTSPSessionDictIndex = 6,
		kRTSPRequestDictIndex = 7,

		kFileDictIndex = 9,
		kModuleDictIndex = 10,
		kModulePrefsDictIndex = 11,
		kAttrInfoDictIndex = 12,
		kQTSSUserProfileDictIndex = 13,
		kQTSSConnectedUserDictIndex = 14,

		kHTTPSessionDictIndex = 15,

		kNumDictionaries = 16,

		kNumDynamicDictionaryTypes = 500,
		kIllegalDictionary = kNumDynamicDictionaryTypes + kNumDictionaries
	};

	// This function converts a QTSS_ObjectType to an index
	static uint32_t                   GetMapIndex(QTSS_ObjectType inType);

	// Using one of the above predefined indexes, this returns the corresponding map
	static QTSSDictionaryMap*       GetMap(uint32_t inIndex)
	{
		Assert(inIndex < kNumDynamicDictionaryTypes + kNumDictionaries); return sDictionaryMaps[inIndex];
	}

	static QTSS_ObjectType          CreateNewMap();

private:

	//
	// Repository for dictionary maps

	static QTSSDictionaryMap*       sDictionaryMaps[kNumDictionaries + kNumDynamicDictionaryTypes];
	static uint32_t                   sNextDynamicMap;

	enum
	{
		kMinArraySize = 20
	};

	uint32_t                          fNextAvailableID;
	uint32_t                          fNumValidAttrs;
	uint32_t                          fAttrArraySize;
	QTSSAttrInfoDict**              fAttrArray;
	uint32_t                          fFlags;

	friend class QTSSDictionary;
};

inline int32_t   QTSSDictionaryMap::ConvertAttrIDToArrayIndex(QTSS_AttributeID inAttrID)
{
	int32_t theIndex = inAttrID & 0x7FFFFFFF;
	if ((theIndex < 0) || (theIndex >= (int32_t)fNextAvailableID))
		return -1;
	else
		return theIndex;
}


#endif
