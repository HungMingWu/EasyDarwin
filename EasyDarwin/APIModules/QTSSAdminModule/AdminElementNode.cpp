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
	 File:       AdminElementNode.cpp

	 Contains:   Implements Admin Elements



 */


#ifndef __Win32__
#include <unistd.h>     /* for getopt() et al */
#endif

#include <stdio.h>      /* for //printf */
#include <stdlib.h>     /* for getloadavg & other useful stuff */
#include <memory>
#include "QTSS.h"
#include "StrPtrLen.h"
#include "OSHashTable.h"
#include "OSMutex.h"
#include "OSRef.h"
#include "AdminElementNode.h"
 //#include "OSHeaders.h"

static char* sParameterDelimeter = ";";
static char* sListDelimeter = ",";
static char* sAccess = "a=";
static char* sType = "t=";
static StrPtrLen sDoAllSPL("*");
static StrPtrLen sDoAllIndexIteratorSPL(":");

#define MEMORYDEBUGGING 0
#if MEMORYDEBUGGING
static int32_t sMaxPtrs = 10000;
static void * sPtrArray[10000];
static char * sSourceArray[10000];
#endif

bool  ElementNode_DoAll(StrPtrLen* str)
{
	Assert(str);
	bool isIterator = false;

	if (str->Equal(sDoAllSPL) || str->Equal(sDoAllIndexIteratorSPL))
		isIterator = true;

	return isIterator;
}

void ElementNode_InitPtrArray()
{
#if MEMORYDEBUGGING
	memset(sPtrArray, 0, sizeof(sPtrArray));
	memset(sSourceArray, 0, sizeof(sSourceArray));
#endif
}

void ElementNode_InsertPtr(void *ptr, char * src)
{
#if MEMORYDEBUGGING
	if (ptr == NULL)
		return;

	for (int32_t index = 0; index < sMaxPtrs; index++)
	{
		if (sPtrArray[index] == NULL)
		{
			sPtrArray[index] = ptr;
			sSourceArray[index] = src;
			//printf("%s INSERTED ptr=%p countPtrs=%" _S32BITARG_ "\n",src, ptr, ElementNode_CountPtrs());  
			return;
		}
	}

	printf("ElementNode_InsertPtr no space in ptr array\n");
	Assert(0);
#endif
}

bool ElementNode_FindPtr(void *ptr, char * src)
{   // use for validating duplicates at some point
#if MEMORYDEBUGGING
	if (ptr == NULL)
		return false;

	for (int32_t index = 0; index < sMaxPtrs; index++)
	{
		if (sPtrArray[index] == ptr)
			return true;
	}

#endif
	return false;
}

void ElementNode_RemovePtr(void *ptr, char * src)
{
#if MEMORYDEBUGGING
	if (ptr == NULL)
		return;

	int16_t foundCount = 0;
	for (int32_t index = 0; index < sMaxPtrs; index++)
	{
		if (sPtrArray[index] == ptr)
		{
			sPtrArray[index] = NULL;
			sSourceArray[index] = NULL;
			//printf("%s REMOVED ptr countPtrs=%" _S32BITARG_ "\n",src,ElementNode_CountPtrs());
			foundCount++; // use for validating duplicates at some point
			return;
		}
	}

	if (foundCount == 0)
	{
		printf("PTR NOT FOUND ElementNode_RemovePtr %s ptr=%p countPtrs=%" _S32BITARG_ "\n", src, ptr, ElementNode_CountPtrs());
		Assert(0);
	}
#endif
}

int32_t ElementNode_CountPtrs()
{
#if MEMORYDEBUGGING
	int32_t count = 0;
	for (int32_t index = 0; index < sMaxPtrs; index++)
	{
		if (sPtrArray[index] != NULL)
			count++;
	}

	return count;
#else
	return 0;
#endif
}

void ElementNode_ShowPtrs()
{
#if MEMORYDEBUGGING
	for (int32_t index = 0; index < sMaxPtrs; index++)
	{
		if (sPtrArray[index] != NULL)
			printf("ShowPtrs ptr=%p source=%s\n", sPtrArray[index], sSourceArray[index]);
	}
#endif
}

void PRINT_STR(StrPtrLen *spl)
{

	if (spl && spl->Ptr && spl->Ptr[0] != 0)
	{
		char buff[1024] = { 0 };
		memcpy(buff, spl->Ptr, spl->Len);
		printf("%s len=%"   _U32BITARG_   "\n", buff, spl->Len);
	}
	else
	{
		printf("(null)\n");
	}
}

void COPYBUFFER(char *dest, char *src, int8_t size)
{
	if ((dest != nullptr) && (src != nullptr) && (size > 0))
		memcpy(dest, src, size);
	else
		Assert(0);
};

char* NewCharArrayCopy(StrPtrLen *theStringPtr)
{
	char* newArray = nullptr;
	if (theStringPtr != nullptr)
	{
		newArray = new char[theStringPtr->Len + 1];
		if (newArray != nullptr)
		{
			memcpy(newArray, theStringPtr->Ptr, theStringPtr->Len);
			newArray[theStringPtr->Len] = 0;
		}
	}
	return newArray;
}



ElementNode::ElementNode()
{
	fDataSource = nullptr;
	fNumFields = 0;
	fPathLen = 0;
	fInitialized = false;

	fFieldIDs = nullptr;
	fFieldDataPtrs = nullptr;
	fFieldOSRefPtrs = nullptr;
	fParentNodePtr = nullptr;
	fElementMap = nullptr;
	fSelfPtr = nullptr;
	fPathBuffer[0] = 0;
	fPathSPL.Set(fPathBuffer, 0);
	fIsTop = false;
	fDataFieldsType = eDynamic;

};

void ElementNode::Initialize(int32_t index, ElementNode *parentPtr, QueryURI *queryPtr, StrPtrLen *currentSegmentPtr, QTSS_Initialize_Params *initParams, QTSS_Object nodeSource, DataFieldsType dataFieldsType)
{
	//printf("------ ElementNode::Initialize ---------\n");

	if (!fInitialized)
	{
		SetParentNode(parentPtr);
		SetSource(nodeSource);

		SetNodeInfo(parentPtr->GetNodeInfoPtr(index));
		SetNodeName(parentPtr->GetName(index));
		SetMyElementDataPtr(parentPtr->GetElementDataPtr(index));

		fDataFieldsType = dataFieldsType;

		StrPtrLen nextSegment;
		StrPtrLen nextnextSegment;
		(void)queryPtr->NextSegment(currentSegmentPtr, &nextSegment);
		(void)queryPtr->NextSegment(&nextSegment, &nextnextSegment);
		bool forceAll = nextSegment.Equal(sDoAllIndexIteratorSPL) | nextnextSegment.Equal(sDoAllIndexIteratorSPL);

		if (GetFields() == nullptr)
			InitializeAllFields(true, nullptr, nodeSource, queryPtr, currentSegmentPtr, forceAll);

		fInitialized = true;
	}
	SetupNodes(queryPtr, currentSegmentPtr, initParams);

};


ElementNode::~ElementNode()
{

	//printf("ElementNode::~ElementNode delete %s Element Node # fields = %"   _U32BITARG_   "\n",GetNodeName(), fNumFields);

	for (int32_t index = 0; !IsStopItem(index); index++)
	{
		OSRef *theRefPtr = GetOSRef(index);
		if (theRefPtr != nullptr)
		{
			//printf("deleting hash entry of %s \n", GetName(index));
			SetOSRef(index, nullptr);
			(fElementMap->GetHashTable())->Remove(theRefPtr);
			delete (OSRef*)theRefPtr;  ElementNode_RemovePtr(theRefPtr, "ElementNode::~ElementNode OSRef *");
		}

		char *dataPtr = GetElementDataPtr(index);
		if (dataPtr != nullptr)
		{
			SetElementDataPtr(index, nullptr, IsNodeElement(index));
		}
	}

	delete fElementMap;  ElementNode_RemovePtr(fElementMap, "ElementNode::~ElementNode fElementMap");

	fElementMap = nullptr;

	uint32_t i = 0;
	for (i = 0; i < GetNumFields(); i++)
	{
		SetElementDataPtr(i, nullptr, IsNodeElement(i));
		fFieldDataPtrs[i] = nullptr;
	}
	delete fFieldDataPtrs; ElementNode_RemovePtr(fFieldDataPtrs, "ElementNode::~ElementNode fFieldDataPtrs");
	fFieldDataPtrs = nullptr;

	for (i = 0; i < GetNumFields(); i++)
	{
		delete fFieldOSRefPtrs[i]; ElementNode_RemovePtr(fFieldOSRefPtrs[i], "ElementNode::~ElementNode fFieldOSRefPtrs");
		fFieldOSRefPtrs[i] = nullptr;
	}
	delete fFieldOSRefPtrs;  ElementNode_RemovePtr(fFieldOSRefPtrs, "ElementNode::~ElementNode fFieldOSRefPtrs");
	fFieldOSRefPtrs = nullptr;

	if (fDataFieldsType == eDynamic)
	{
		delete fFieldIDs;  ElementNode_RemovePtr(fFieldIDs, "ElementNode::~ElementNode fFieldIDs");
		fFieldIDs = nullptr;
	}

	SetNodeName(nullptr);

};

QTSS_Error ElementNode::AllocateFields(uint32_t numFields)
{
	//printf("-------- ElementNode::AllocateFields ----------\n");
	//printf("ElementNode::AllocateFields numFields=%"   _U32BITARG_   "\n",numFields);

	QTSS_Error err = QTSS_NotEnoughSpace;

	Assert(GetNumFields() == 0);
	SetNumFields(numFields);

	if (numFields > 0) do
	{
		Assert(fFieldIDs == nullptr);
		fFieldIDs = new ElementNode::ElementDataFields[numFields]; ElementNode_InsertPtr(fFieldIDs, "ElementNode::AllocateFields fFieldIDs array");
		Assert(fFieldIDs != nullptr);
		if (fFieldIDs == nullptr) break;
		memset(fFieldIDs, 0, numFields * sizeof(ElementNode::ElementDataFields));

		Assert(fElementMap == nullptr);
		fElementMap = new OSRefTable();  ElementNode_InsertPtr(fElementMap, "ElementNode::AllocateFields fElementMap OSRefTable");
		Assert(fElementMap != nullptr);
		if (fElementMap == nullptr) break;

		Assert(fFieldDataPtrs == nullptr);
		fFieldDataPtrs = new char*[numFields]; ElementNode_InsertPtr(fFieldDataPtrs, "ElementNode::AllocateFields fFieldDataPtrs array");
		Assert(fFieldDataPtrs != nullptr);
		if (fFieldDataPtrs == nullptr) break;
		memset(fFieldDataPtrs, 0, numFields * sizeof(char*));

		Assert(fFieldOSRefPtrs == nullptr);
		fFieldOSRefPtrs = new OSRef*[numFields];  ElementNode_InsertPtr(fFieldOSRefPtrs, "ElementNode::AllocateFields fFieldDataPtrs array");
		Assert(fFieldOSRefPtrs != nullptr);
		if (fFieldOSRefPtrs == nullptr) break;
		memset(fFieldOSRefPtrs, 0, numFields * sizeof(OSRef*));

		err = QTSS_NoErr;
	} while (false);

	return err;
};




void ElementNode::SetFields(uint32_t i, QTSS_Object attrInfoObject)
{
	//printf("------- ElementNode::SetFields -------- \n");

	uint32_t ioLen = 0;
	QTSS_Error err = QTSS_NoErr;
	if (fFieldIDs[i].fFieldName[0] != 0)
		return;

	if (fFieldIDs[i].fFieldName[0] == 0)
	{
		fFieldIDs[i].fFieldLen = eMaxAttributeNameSize;
		err = QTSS_GetValue(attrInfoObject, qtssAttrName, 0, &fFieldIDs[i].fFieldName, (uint32_t *)&fFieldIDs[i].fFieldLen);
		Assert(err == QTSS_NoErr);
		if (fFieldIDs[i].fFieldName != nullptr)
			fFieldIDs[i].fFieldName[fFieldIDs[i].fFieldLen] = 0;
	}

	ioLen = sizeof(fFieldIDs[i].fAPI_ID);
	err = QTSS_GetValue(attrInfoObject, qtssAttrID, 0, &fFieldIDs[i].fAPI_ID, &ioLen);
	Assert(err == QTSS_NoErr);

	ioLen = sizeof(fFieldIDs[i].fAPI_Type);
	err = QTSS_GetValue(attrInfoObject, qtssAttrDataType, 0, &fFieldIDs[i].fAPI_Type, &ioLen);
	Assert(err == QTSS_NoErr);
	if (fFieldIDs[i].fAPI_Type == 0 || err != QTSS_NoErr)
	{
		//printf("QTSS_GetValue err = %" _S32BITARG_ " attrInfoObject=%"   _U32BITARG_   " qtssAttrDataType = %"   _U32BITARG_   " \n",err, (uint32_t)  attrInfoObject, (uint32_t) fFieldIDs[i].fAPI_Type);
	}

	if (fFieldIDs[i].fAPI_Type == qtssAttrDataTypeQTSS_Object)
		fFieldIDs[i].fFieldType = eNode;

	ioLen = sizeof(fFieldIDs[i].fAccessPermissions);
	err = QTSS_GetValue(attrInfoObject, qtssAttrPermissions, 0, &fFieldIDs[i].fAccessPermissions, &ioLen);
	Assert(err == QTSS_NoErr);

	fFieldIDs[i].fAccessData[0] = 0;
	if (fFieldIDs[i].fAccessPermissions & qtssAttrModeRead)
	{
		strcat(fFieldIDs[i].fAccessData, "r");
	}

	if (fFieldIDs[i].fAccessPermissions & qtssAttrModeWrite && fFieldIDs[i].fAPI_Type != qtssAttrDataTypeQTSS_Object)
	{
		strcat(fFieldIDs[i].fAccessData, "w");
	}

	if (fFieldIDs[i].fAccessPermissions & qtssAttrModeInstanceAttrAllowed && fFieldIDs[i].fAPI_Type == qtssAttrDataTypeQTSS_Object)
	{
		strcat(fFieldIDs[i].fAccessData, "w");
	}

	if (GetMyFieldType() != eNode && GetNumFields() > 1)
	{
		strcat(fFieldIDs[i].fAccessData, "d");
	}
	if (fFieldIDs[i].fAccessPermissions & qtssAttrModeDelete)
	{
		strcat(fFieldIDs[i].fAccessData, "d");
	}


	if (fFieldIDs[i].fAccessPermissions & qtssAttrModePreempSafe)
	{
		strcat(fFieldIDs[i].fAccessData, "p");
	}

	fFieldIDs[i].fAccessLen = strlen(fFieldIDs[i].fAccessData);

	//printf("ElementNode::SetFields name=%s api_id=%" _S32BITARG_ " \n",fFieldIDs[i].fFieldName, fFieldIDs[i].fAPI_ID);
	//DebugShowFieldDataType(i);    
};


ElementNode* ElementNode::CreateArrayAttributeNode(uint32_t index, QTSS_Object source, QTSS_Object attributeInfo, uint32_t arraySize)
{
	//printf("------- ElementNode::CreateArrayAttributeNode --------\n");
	//printf("ElementNode::CreateArrayAttributeNode name = %s index = %"   _U32BITARG_   " arraySize =%"   _U32BITARG_   " \n",fFieldIDs[index].fFieldName, index,arraySize);

	ElementDataFields* fieldPtr = nullptr;
	SetFields(index, attributeInfo);
	fFieldIDs[index].fFieldType = eArrayNode;

	auto* nodePtr = new ElementNode(); ElementNode_InsertPtr(nodePtr, "ElementNode::CreateArrayAttributeNode ElementNode*");
	this->SetElementDataPtr(index, (char *)nodePtr, true);
	Assert(nodePtr != nullptr);
	if (nullptr == nodePtr) return nullptr;

	nodePtr->SetSource(source); // the node's API source
	nodePtr->AllocateFields(arraySize);

	if (this->GetNodeInfoPtr(index) == nullptr)
	{   //printf("ElementNode::CreateArrayAttributeNode index = %"   _U32BITARG_   " this->GetNodeInfoPtr(index) == NULL \n",index);
	}
	nodePtr->SetNodeInfo(this->GetNodeInfoPtr(index));

	for (uint32_t i = 0; !nodePtr->IsStopItem(i); i++)
	{
		fieldPtr = nodePtr->GetElementFieldPtr(i);
		Assert(fieldPtr != nullptr);

		nodePtr->SetFields(i, attributeInfo);

		fieldPtr->fIndex = i; // set the API attribute index

		// set the name of the field to the array index 
		fieldPtr->fFieldName[0] = 0;
		sprintf(fieldPtr->fFieldName, "%"   _U32BITARG_, i);
		fieldPtr->fFieldLen = ::strlen(fieldPtr->fFieldName);

		if (fieldPtr->fAPI_Type != qtssAttrDataTypeQTSS_Object)
		{
			//printf("ElementNode::CreateArrayAttributeNode array field index = %"   _U32BITARG_   " name = %s api Source = %"   _U32BITARG_   " \n", (uint32_t)  i,fieldPtr->fFieldName, (uint32_t) source);
			fieldPtr->fAPISource = source; // the attribute's source is the same as node source
		}
		else
		{
			fieldPtr->fFieldType = eNode;
			// this is an array of objects so record each object as the source for a new node
			uint32_t sourceLen = sizeof(fieldPtr->fAPISource);
			QTSS_Error err = QTSS_GetValue(source, fieldPtr->fAPI_ID, fieldPtr->fIndex, &fieldPtr->fAPISource, &sourceLen);
			Warn(err == QTSS_NoErr);
			if (err != QTSS_NoErr)
			{   //printf("Error Getting Value for %s type = qtssAttrDataTypeQTSS_Object err = %"   _U32BITARG_   "\n", fieldPtr->fFieldName,err);
				fieldPtr->fAPISource = nullptr;
			}

			QTSS_AttributeID id;
			bool foundFilteredAttribute = GetFilteredAttributeID(GetMyName(), nodePtr->GetMyName(), &id);
			if (foundFilteredAttribute)
			{
				GetFilteredAttributeName(fieldPtr, id);
			}

			//printf("ElementNode::CreateArrayAttributeNode array field index = %"   _U32BITARG_   " name = %s api Source = %"   _U32BITARG_   " \n", i,fieldPtr->fFieldName, (uint32_t) fieldPtr->fAPISource);
		}

		nodePtr->fElementMap->Register(nodePtr->GetOSRef(i));
	}
	nodePtr->SetNodeName(GetName(index));
	nodePtr->SetParentNode(this);
	nodePtr->SetSource(source);
	nodePtr->fInitialized = true;

	return nodePtr;

}

void ElementNode::InitializeAllFields(bool allocateFields, QTSS_Object defaultAttributeInfo, QTSS_Object source, QueryURI *queryPtr, StrPtrLen *currentSegmentPtr, bool forceAll = false)
{
	//printf("------- ElementNode::InitializeAllFields -------- \n");

	QTSS_Error err = QTSS_NoErr;
	QTSS_Object theAttributeInfo;

	if (allocateFields)
	{
		uint32_t numFields = this->CountAttributes(source);
		err = AllocateFields(numFields);
		//printf("ElementNode::InitializeAllFields AllocateFields numFields =  %"   _U32BITARG_   " error = %" _S32BITARG_ " \n",numFields, err);
	}

	if (err == QTSS_NoErr)
	{
		uint32_t numValues = 0;

		for (uint32_t i = 0; !IsStopItem(i); i++)
		{
			if (defaultAttributeInfo == nullptr)
			{
				err = QTSS_GetAttrInfoByIndex(source, i, &theAttributeInfo);
				Assert(err == QTSS_NoErr);
				if (err != QTSS_NoErr)
				{   //printf("QTSS_GetAttrInfoByIndex returned err = %"   _U32BITARG_   " \n",err);
				}
			}
			else
			{
				theAttributeInfo = defaultAttributeInfo;
			}

			SetFields(i, theAttributeInfo);

			if ((int32_t)fFieldIDs[i].fAPI_ID < 0)
			{   //printf("ElementNode::InitializeAllFields name = %s index = %" _S32BITARG_ " numValues =%"   _U32BITARG_   " \n",fFieldIDs[i].fFieldName, (int32_t) fFieldIDs[i].fAPI_ID,numValues);
			}
			numValues = this->CountValues(source, fFieldIDs[i].fAPI_ID);
			//printf("ElementNode::InitializeAllFields name = %s index = %"   _U32BITARG_   " numValues =%"   _U32BITARG_   " \n",fFieldIDs[i].fFieldName, fFieldIDs[i].fAPI_ID,numValues);

			QTSS_AttributeID id;
			bool foundFilteredAttribute = GetFilteredAttributeID(GetMyName(), GetName(i), &id);

			StrPtrLen nextSegment;
			(void)queryPtr->NextSegment(currentSegmentPtr, &nextSegment);

			if (forceAll || nextSegment.Equal(sDoAllIndexIteratorSPL) || queryPtr->IndexParam() || numValues > 1 || foundFilteredAttribute)
			{
				ElementNode *nodePtr = CreateArrayAttributeNode(i, source, theAttributeInfo, numValues);
				Assert(nodePtr != nullptr);
				/*
				if (NULL == nodePtr)
				{   //printf("ElementNode::InitializeAllFields(NULL == CreateArrayAttributeNode  nodePtr\n");
				}
				if (NULL == GetElementDataPtr(i))
				{   //printf("ElementNode::InitializeAllFields(NULL == GetElementDataPtr (i=%"   _U32BITARG_   ") nodePtr=%"   _U32BITARG_   " \n",i, (uint32_t) nodePtr);
				}
				*/

			}
			else
			{
				//printf("ElementNode::InitializeAllFields field index = %"   _U32BITARG_   " name = %s api Source = %"   _U32BITARG_   " \n", i,fFieldIDs[i].fFieldName, (uint32_t) source);
			}

			err = fElementMap->Register(GetOSRef(i));
			if (err != QTSS_NoErr)
			{   //printf("ElementNode::InitializeAllFields  Register returned err = %"   _U32BITARG_   " field = %s node=%s \n",err,GetName(i),GetMyName());
			}
			Assert(err == QTSS_NoErr);
		}
	}
};


void ElementNode::SetNodeInfo(ElementDataFields *nodeInfoPtr)
{
	if (nodeInfoPtr == nullptr)
	{
		//printf("---- SetNodeInfo nodeInfoPtr = NULL \n");
	}
	else
	{
		//printf("---- SetNodeInfo nodeInfoPtr name = %s \n",nodeInfoPtr->fFieldName);
		fSelfPtr = nodeInfoPtr;
	}
};


void ElementNode::SetNodeName(char *namePtr)
{
	if (namePtr == nullptr)
	{
		delete fNodeNameSPL.Ptr; ElementNode_RemovePtr(fNodeNameSPL.Ptr, "ElementNode::SetNodeName char* ");
		fNodeNameSPL.Set(nullptr, 0);
		return;
	}

	if (fNodeNameSPL.Ptr != nullptr)
	{
		delete fNodeNameSPL.Ptr; ElementNode_RemovePtr(fNodeNameSPL.Ptr, "ElementNode::SetNodeName char* ");
		fNodeNameSPL.Set(nullptr, 0);
	}
	//printf(" ElementNode::SetNodeName new NodeName = %s \n",namePtr);
	int len = ::strlen(namePtr);
	fNodeNameSPL.Ptr = new char[len + 1]; ElementNode_InsertPtr(fNodeNameSPL.Ptr, "ElementNode::SetNodeName ElementNode* chars");
	fNodeNameSPL.Len = len;
	memcpy(fNodeNameSPL.Ptr, namePtr, len);
	fNodeNameSPL.Ptr[len] = 0;
};

ElementNode::ElementDataFields *ElementNode::GetElementFieldPtr(int32_t index)
{
	ElementNode::ElementDataFields *resultPtr = nullptr;
	Assert(fFieldIDs != nullptr);
	Assert((index >= 0) && (index < (int32_t)fNumFields));
	if ((index >= 0) && (index < (int32_t)fNumFields))
		resultPtr = &fFieldIDs[index];
	return resultPtr;
}

char *ElementNode::GetElementDataPtr(int32_t index)
{
	char *resultPtr = nullptr;
	Assert((index >= 0) && (index < (int32_t)fNumFields));
	if (fInitialized && (fFieldDataPtrs != nullptr) && (index >= 0) && (index < (int32_t)fNumFields))
	{
		resultPtr = fFieldDataPtrs[index];
	}
	return resultPtr;
}

void ElementNode::SetElementDataPtr(int32_t index, char *data, bool isNode)
{
	//printf("------ElementNode::SetElementDataPtr----- \n");
	//printf("ElementNode::SetElementDataPtr index = %" _S32BITARG_ " fNumFields = %" _S32BITARG_ " \n", index,fNumFields);
	Assert((index >= 0) && (index < (int32_t)fNumFields));
	if ((index >= 0) && (index < (int32_t)fNumFields))
	{   //Assert(fFieldDataPtrs[index] == NULL);
		if (fDataFieldsType != eStatic)
		{
			if (isNode)
			{
				delete (ElementNode*)fFieldDataPtrs[index]; ElementNode_RemovePtr(fFieldDataPtrs[index], "ElementNode::SetElementDataPtr ElementNode* fFieldDataPtrs");
			}
			else
			{
				delete fFieldDataPtrs[index]; ElementNode_RemovePtr(fFieldDataPtrs[index], "ElementNode::SetElementDataPtr char* fFieldDataPtrs");
			}
		}
		fFieldDataPtrs[index] = data;
		//printf("ElementNode::SetElementDataPtr index = %" _S32BITARG_ " \n", index);
	}
}



inline void ElementNode::DebugShowFieldDataType(int32_t /*index*/)
{
	//char field[100];
	//field[0] = ' ';
	//char* typeStringPtr = GetAPI_TypeStr(index);
	//printf("debug: %s=%s\n",GetName(index),typeStringPtr);

}

inline void ElementNode::DebugShowFieldValue(int32_t /*index*/)
{
	//printf("debug: %s=%s\n",GetName(index),GetElementDataPtr(index));
}

ElementNode::ElementDataFields *ElementNode::GetNodeInfoPtr(int32_t index)
{
	ElementNode::ElementDataFields *resultPtr = GetElementFieldPtr(index);
	Assert(resultPtr != nullptr);

	if ((resultPtr != nullptr) && ((eNode != resultPtr->fFieldType) && (eArrayNode != resultPtr->fFieldType)))
		resultPtr = nullptr;
	return resultPtr;
}


void ElementNode::SetUpSingleNode(QueryURI *queryPtr, StrPtrLen *currentSegmentPtr, StrPtrLen *nextSegmentPtr, int32_t index, QTSS_Initialize_Params *initParams)
{
	//printf("--------ElementNode::SetUpSingleNode ------------\n");
	if (queryPtr && currentSegmentPtr && nextSegmentPtr&& initParams) do
	{
		if (!queryPtr->RecurseParam() && (nextSegmentPtr->Len == 0)) break;

		ElementNode::ElementDataFields *theNodePtr = GetNodeInfoPtr(index);
		if (nullptr == theNodePtr)
		{
			//printf(" ElementNode::SetUpSingleNode (NULL == theNodePtr(%" _S32BITARG_ ")) name=%s \n",index,GetName(index));
			break;
		}

		if (!IsNodeElement(index))
		{
			//printf(" ElementNode::SetUpSingleNode (apiType != qtssAttrDataTypeQTSS_Object) \n");
			break;
		}

		// filter unnecessary nodes     
		char *nodeName = GetName(index);
		if (nodeName != nullptr)
		{
			StrPtrLen nodeNameSPL(nodeName);
			if ((!nodeNameSPL.Equal(*nextSegmentPtr) && !ElementNode_DoAll(nextSegmentPtr))
				&&
				!(queryPtr->RecurseParam() && (nextSegmentPtr->Len == 0))
				)
			{
				//printf(" ElementNode::SetUpSingleNode SPL TEST SKIP NodeElement= %s\n",GetName(index));
				//printf("ElementNode::SetUpAllNodes skip nextSegmentPtr=");PRINT_STR(nextSegmentPtr);
				break;
			}

		}

		auto* nodePtr = (ElementNode *)GetElementDataPtr(index);
		if (nodePtr == nullptr)
		{
			//printf("ElementNode::SetUpSingleNode %s nodePtr == NULL make new nodePtr index = %" _S32BITARG_ "\n", GetMyName(),index);
			nodePtr = new ElementNode(); ElementNode_InsertPtr(nodePtr, "ElementNode::SetUpSingleNode ElementNode* new ElementNode() ");
			SetElementDataPtr(index, (char *)nodePtr, true);
		}

		if (nodePtr != nullptr)
		{
			StrPtrLen tempSegment;
			(void)queryPtr->NextSegment(nextSegmentPtr, &tempSegment);
			currentSegmentPtr = nextSegmentPtr;
			nextSegmentPtr = &tempSegment;


			if (!nodePtr->fInitialized)
			{
				//printf("ElementNode::SetUpSingleNode Node !fInitialized -- Initialize %s\n",GetName(index));
				//printf("ElementNode::SetUpSingleNode GetValue source = %"   _U32BITARG_   " name = %s id = %"   _U32BITARG_   " \n",(uint32_t)  GetSource(),(uint32_t)  GetName(index),(uint32_t) GetAPI_ID(index));

				ElementDataFields* fieldPtr = GetElementFieldPtr(index);
				if (fieldPtr != nullptr && fieldPtr->fAPI_Type == qtssAttrDataTypeQTSS_Object)
				{
					uint32_t sourceLen = sizeof(fieldPtr->fAPISource);
					(void)QTSS_GetValue(GetSource(), fieldPtr->fAPI_ID, fieldPtr->fIndex, &fieldPtr->fAPISource, &sourceLen);
				}

				QTSS_Object theSourceObject = GetAPISource(index);
				//Warn(theSourceObject != NULL);

				nodePtr->Initialize(index, this, queryPtr, nextSegmentPtr, initParams, theSourceObject, eDynamic);
				nodePtr->SetUpAllElements(queryPtr, currentSegmentPtr, nextSegmentPtr, initParams);
				fInitialized = true;

				break;

			}
			else
			{
				nodePtr->SetUpAllElements(queryPtr, currentSegmentPtr, nextSegmentPtr, initParams);
			}
		}

	} while (false);

	return;
}

bool ElementNode::SetUpOneDataField(uint32_t index)
{
	//printf("----ElementNode::SetUpOneDataField----\n");       
	//printf(" ElementNode::SetUpOneDataField parent = %s field name=%s\n",GetNodeName(), GetName(index));  

	QTSS_AttributeID inID = GetAPI_ID(index);
	bool isNodeResult = IsNodeElement(index);
	char *testPtr = GetElementDataPtr(index);
	//Warn(NULL == testPtr);
	if (nullptr != testPtr)
	{   //printf(" ElementNode::SetUpOneDataField skip field already setup parent = %s field name=%s\n",GetNodeName(), GetName(index)); 
		return isNodeResult;
	}

	if (!isNodeResult)
	{
		//printf("ElementNode::SetUpOneDataField %s Source=%"   _U32BITARG_   " Field index=%"   _U32BITARG_   " API_ID=%"   _U32BITARG_   " value index=%"   _U32BITARG_   "\n",GetName(index),GetSource(), index,inID,GetAttributeIndex(index));  
		SetElementDataPtr(index, NewIndexElement(GetSource(), inID, GetAttributeIndex(index)), false);
	}
	else
	{
		//printf("ElementNode::SetUpOneDataField %s Source=%"   _U32BITARG_   " Field index=%"   _U32BITARG_   " API_ID=%"   _U32BITARG_   " value index=%"   _U32BITARG_   "\n",GetName(index),(uint32_t) GetSource(),(uint32_t)  index,(uint32_t) inID,(uint32_t) GetAttributeIndex(index));  
		//printf("ElementNode::SetUpOneDataField %s IsNodeElement index = %"   _U32BITARG_   "\n",GetName(index),(uint32_t)  index);   
		//DebugShowFieldDataType(index);
	}

	DebugShowFieldValue(index);

	return isNodeResult;
}

void ElementNode::SetUpAllElements(QueryURI *queryPtr, StrPtrLen *currentSegmentPtr, StrPtrLen *nextSegmentPtr, QTSS_Initialize_Params *initParams)
{
	//printf("---------ElementNode::SetUpAllElements------- \n");

	for (int32_t index = 0; !IsStopItem(index); index++)
	{
		SetUpSingleElement(queryPtr, currentSegmentPtr, nextSegmentPtr, index, initParams);
	}
}




void ElementNode::SetUpSingleElement(QueryURI *queryPtr, StrPtrLen *currentSegmentPtr, StrPtrLen *nextSegmentPtr, int32_t index, QTSS_Initialize_Params *initParams)
{
	//printf("---------ElementNode::SetUpSingleElement------- \n");
	StrPtrLen indexNodeNameSPL;
	GetNameSPL(index, &indexNodeNameSPL);
	if ((queryPtr->RecurseParam() && (nextSegmentPtr->Len == 0))
		||
		(indexNodeNameSPL.Equal(*nextSegmentPtr) || ElementNode_DoAll(nextSegmentPtr))
		) // filter unnecessary elements        
	{

		bool isNode = SetUpOneDataField((uint32_t)index);
		if (isNode)
		{
			//printf("ElementNode::SetUpSingleElement isNode=true calling SetUpSingleNode \n");
			SetUpSingleNode(queryPtr, currentSegmentPtr, nextSegmentPtr, index, initParams);
		}
	}
	else
	{   //printf("ElementNode::SetUpSingleElement filter element=%s\n",GetName(index));
	}
}


void ElementNode::SetUpAllNodes(QueryURI *queryPtr, StrPtrLen *currentSegmentPtr, StrPtrLen *nextSegmentPtr, QTSS_Initialize_Params *initParams)
{
	//printf("--------ElementNode::SetUpAllNodes------- \n");
	for (int32_t index = 0; !IsStopItem(index); index++)
	{
		if (!queryPtr->RecurseParam() && (nextSegmentPtr->Len == 0)) break;

		//printf("ElementNode::SetUpAllNodes index = %" _S32BITARG_ " nextSegmentPtr=", index);PRINT_STR(nextSegmentPtr);
		StrPtrLen indexNodeNameSPL;
		GetNameSPL(index, &indexNodeNameSPL);
		if (IsNodeElement(index)
			&&
			((queryPtr->RecurseParam() && (nextSegmentPtr->Len == 0))
				||
				(indexNodeNameSPL.Equal(*nextSegmentPtr) || ElementNode_DoAll(nextSegmentPtr))
				)
			) // filter unnecessary nodes       
			SetUpSingleNode(queryPtr, currentSegmentPtr, nextSegmentPtr, index, initParams);
		else
		{
			//printf("ElementNode::SetUpAllNodes skip index = %" _S32BITARG_ " indexNodeName=", index);PRINT_STR(&indexNodeNameSPL);
			//printf("ElementNode::SetUpAllNodes skip nextSegmentPtr=");PRINT_STR(nextSegmentPtr);
		}
	}
}

QTSS_Error ElementNode::GetAttributeSize(QTSS_Object inObject, QTSS_AttributeID inID, uint32_t inIndex, uint32_t* outLenPtr)
{
	return QTSS_GetValue(inObject, inID, inIndex, nullptr, outLenPtr);
}

char *ElementNode::NewIndexElement(QTSS_Object inObject, QTSS_AttributeID inID, uint32_t inIndex)
{
	char *resultPtr = nullptr;

	Assert(inObject != nullptr);

	if (inObject != nullptr)
	{
		QTSS_Error err = QTSS_GetValueAsString(inObject, inID, inIndex, &resultPtr); ElementNode_InsertPtr(resultPtr, "ElementNode::NewIndexElement QTSS_GetValueAsString ");
		if (err != QTSS_NoErr)
		{   //printf("ElementNode::NewIndexElement QTSS_GetValueAsString object= %p id=%"   _U32BITARG_   " index=%"   _U32BITARG_   " err= %" _S32BITARG_ " \n",inObject,inID, inIndex, err);
		}
	}
	return resultPtr;
}


inline  int32_t ElementNode::ResolveSPLKeyToIndex(StrPtrLen *keyPtr)
{
	int32_t index = -1;
	PointerSizedInt object = 0;

	if (fElementMap != nullptr && keyPtr != nullptr && keyPtr->Len > 0)
	{
		OSRef* osrefptr = fElementMap->Resolve(keyPtr);
		if (osrefptr != nullptr)
		{
			object = (PointerSizedInt)osrefptr->GetObject();
			index = (int32_t)object;
		}
	}

	return index;
}


uint32_t ElementNode::CountAttributes(QTSS_Object source)
{
	//printf("------ElementNode::CountAttributes-------\n");
	//printf("ElementNode::CountAttributes SOURCE = %"   _U32BITARG_   " \n", (uint32_t) source);

	uint32_t numFields = 0;

	(void)QTSS_GetNumAttributes(source, &numFields);

	//printf("ElementNode::CountAttributes %s = %"   _U32BITARG_   " \n",GetNodeName() ,numFields);

	return numFields;
}

uint32_t ElementNode::CountValues(QTSS_Object source, uint32_t apiID)
{
	//printf("------ElementNode::CountValues-------\n");
	uint32_t numFields = 0;

	(void)QTSS_GetNumValues(source, apiID, &numFields);

	//printf("ElementNode::CountValues %s = %"   _U32BITARG_   " \n",GetNodeName() ,numFields);

	return numFields;
}



OSRef* ElementNode::GetOSRef(int32_t index)
{

	OSRef* resultPtr = fFieldOSRefPtrs[index];
	//      Assert(resultPtr != NULL);
	if (resultPtr == nullptr)
	{
		StrPtrLen theName;
		fFieldOSRefPtrs[index] = new OSRef(); Assert(fFieldOSRefPtrs[index] != nullptr); ElementNode_InsertPtr(fFieldOSRefPtrs[index], "ElementNode::GetOSRef new OSRef() fFieldOSRefPtrs ");
		GetNameSPL(index, &theName); Assert(theName.Len != 0);
		//printf("ElementNode::GetOSRef index = %" _S32BITARG_ " name = %s \n", index, theName.Ptr);
		fFieldOSRefPtrs[index]->Set(theName, (void *)index);
		if (0 != theName.Len && nullptr != theName.Ptr) //return the ptr else NULL
			resultPtr = fFieldOSRefPtrs[index];
	}


	return resultPtr;
}

void ElementNode::SetOSRef(int32_t index, OSRef* refPtr)
{
	Assert((index >= 0) && (index < (int32_t)fNumFields));
	if (fInitialized && (index >= 0) && (index < (int32_t)fNumFields))
		fFieldOSRefPtrs[index] = refPtr;
}

void ElementNode::GetFullPath(StrPtrLen *resultPtr)
{
	//printf("ElementNode::GetFullPath this node name %s \n",GetNodeName());

	Assert(fPathSPL.Ptr != nullptr);

	if (fPathSPL.Len != 0)
	{
		resultPtr->Set(fPathSPL.Ptr, fPathSPL.Len);
		//printf("ElementNode::GetFullPath has path=%s\n",resultPtr->Ptr);
		return;
	}

	ElementNode *parentPtr = GetParentNode();
	if (parentPtr != nullptr)
	{
		StrPtrLen parentPath;
		parentPtr->GetFullPath(&parentPath);
		memcpy(fPathSPL.Ptr, parentPath.Ptr, parentPath.Len);
		fPathSPL.Ptr[parentPath.Len] = 0;
		fPathSPL.Len = parentPath.Len;
	}

	uint32_t nodeNameLen = GetNodeNameLen();
	if (nodeNameLen > 0)
	{
		fPathSPL.Len += nodeNameLen + 1;
		Assert(fPathSPL.Len < kmaxPathlen);
		if (fPathSPL.Len < kmaxPathlen)
		{
			strcat(fPathSPL.Ptr, GetNodeName());
			strcat(fPathSPL.Ptr, "/");
			fPathSPL.Len = strlen(fPathSPL.Ptr);
		}
	}

	resultPtr->Set(fPathSPL.Ptr, fPathSPL.Len);
	//printf("ElementNode::GetFullPath element=%s received full path=%s \n",GetMyName(),resultPtr->Ptr);
}

void ElementNode::RespondWithSelfAdd(QTSS_StreamRef inStream, QueryURI *queryPtr)
{
	char messageBuffer[1024] = "";
	StrPtrLen bufferSPL(messageBuffer);
	QTSS_Error err = QTSS_NoErr;

	//printf("ElementNode::RespondWithSelfAdd NODE = %s index = %" _S32BITARG_ " \n",GetNodeName(), (int32_t) index);

	if (!fInitialized)
	{   //printf("ElementNode::RespondWithSelfAdd not Initialized EXIT\n");
		return;
	}
	if (nullptr == queryPtr)
	{   //printf("ElementNode::RespondWithSelfAdd NULL == queryPtr EXIT\n");
		return;
	}

	if (nullptr == inStream)
	{   //printf("ElementNode::RespondWithSelfAdd NULL == inStream EXIT\n");
		return;
	}

	char *dataPtr = GetMyElementDataPtr();
	bool nullData = false;
	static char *nullErr = "(null)";
	if (nullptr == dataPtr)
	{   //printf("ElementNode::RespondWithSelfAdd NULL == dataPtr EXIT\n");
		dataPtr = nullErr;
		nullData = true;
	}

	queryPtr->SetQueryHasResponse();



#if CHECKACCESS
	/*
		StrPtrLen *accessParamsPtr=queryPtr->GetAccess();
		if (accessParamsPtr == NULL)
		{
				uint32_t result = 400;
				sprintf(messageBuffer,  "Attribute Access is required");
				(void) queryPtr->EvalQuery(&result, messageBuffer);
				return;
		}

		accessFlags = queryPtr->GetAccessFlags();
		if (0 == (accessFlags & qtssAttrModeWrite))
		{
				uint32_t result = 400;
				sprintf(messageBuffer,  "Attribute must have write access");
				(void) queryPtr->EvalQuery(&result, messageBuffer);
				return;
		}
	*/
#endif


	StrPtrLen* valuePtr = queryPtr->GetValue();
	std::unique_ptr<char[]> value(NewCharArrayCopy(valuePtr));
	if (!valuePtr || !valuePtr->Ptr)
	{
		uint32_t result = 400;
		sprintf(messageBuffer, "Attribute value is required");
		(void)queryPtr->EvalQuery(&result, messageBuffer);
		return;
	}


	StrPtrLen *typePtr = queryPtr->GetType();
	std::unique_ptr<char[]> dataType(NewCharArrayCopy(typePtr));
	if (!typePtr || !typePtr->Ptr)
	{
		uint32_t result = 400;
		sprintf(messageBuffer, "Attribute type is required");
		(void)queryPtr->EvalQuery(&result, messageBuffer);
		return;
	}

	QTSS_AttrDataType attrDataType = qtssAttrDataTypeUnknown;
	if (typePtr && typePtr->Len > 0)
	{
		err = QTSS_TypeStringToType(dataType.get(), &attrDataType);
		Assert(err == QTSS_NoErr);
		//printf("ElementNode::RespondWithSelfAdd theType=%s typeID=%"   _U32BITARG_   " \n",dataType.GetObject(), attrDataType);
	}

	//printf("ElementNode::RespondWithSelfAdd theValue= %s theType=%s typeID=%"   _U32BITARG_   " \n",value.GetObject(), typePtr->Ptr, attrDataType);
	char valueBuff[2048] = "";
	uint32_t len = 2048;
	err = QTSS_StringToValue(value.get(), attrDataType, valueBuff, &len);
	if (err)
	{
		uint32_t result = 400;
		sprintf(messageBuffer, "QTSS_Error=%" _S32BITARG_ " from ElementNode::RespondWithSelfAdd QTSS_ConvertStringToType", err);
		(void)queryPtr->EvalQuery(&result, messageBuffer);
		return;
	}

	if (GetMyFieldType() != eNode)
	{
		uint32_t result = 500;
		sprintf(messageBuffer, "Internal error");
		(void)queryPtr->EvalQuery(&result, messageBuffer);
		return;
	}

	StrPtrLen *namePtr = queryPtr->GetName();
	std::unique_ptr<char[]> nameDeleter(NewCharArrayCopy(namePtr));
	if (!namePtr || !namePtr->Ptr || namePtr->Len == 0)
	{
		uint32_t result = 400;
		sprintf(messageBuffer, "Missing name for attribute");
		(void)queryPtr->EvalQuery(&result, messageBuffer);
		return;
	}

	err = QTSS_AddInstanceAttribute(GetSource(), nameDeleter.get(), nullptr, attrDataType);
	//printf("QTSS_AddInstanceAttribute(source=%"   _U32BITARG_   ", name=%s, NULL, %d, %"   _U32BITARG_   ")\n",GetSource(),nameDeleter.GetObject(),attrDataType,accessFlags);
	if (err)
	{
		uint32_t result = 400;
		if (err == QTSS_AttrNameExists)
		{
			sprintf(messageBuffer, "The name %s already exists QTSS_Error=%" _S32BITARG_ " from QTSS_AddInstanceAttribute", nameDeleter.get(), err);
		}
		else
		{
			sprintf(messageBuffer, "QTSS_Error=%" _S32BITARG_ " from QTSS_AddInstanceAttribute", err);
		}
		(void)queryPtr->EvalQuery(&result, messageBuffer);
		return;
	}
	QTSS_Object attrInfoObject;
	err = QTSS_GetAttrInfoByName(GetSource(), nameDeleter.get(), &attrInfoObject);
	if (err)
	{
		uint32_t result = 400;
		sprintf(messageBuffer, "QTSS_Error=%" _S32BITARG_ " from QTSS_GetAttrInfoByName", err);
		(void)queryPtr->EvalQuery(&result, messageBuffer);
		return;
	}
	QTSS_AttributeID attributeID = 0;
	uint32_t attributeLen = sizeof(attributeID);
	err = QTSS_GetValue(attrInfoObject, qtssAttrID, 0, &attributeID, &attributeLen);
	if (err)
	{
		uint32_t result = 400;
		sprintf(messageBuffer, "QTSS_Error=%" _S32BITARG_ " from QTSS_GetValue", err);
		(void)queryPtr->EvalQuery(&result, messageBuffer);
		return;
	}

	err = QTSS_SetValue(GetSource(), attributeID, 0, valueBuff, len);
	if (err)
	{
		uint32_t result = 400;
		sprintf(messageBuffer, "QTSS_Error=%" _S32BITARG_ " from QTSS_SetValue", err);
		(void)queryPtr->EvalQuery(&result, messageBuffer);
		return;
	}

}

void ElementNode::RespondWithSelf(QTSS_StreamRef inStream, QueryURI *queryPtr)
{
	//printf("ElementNode::RespondWithSelf = %s \n",GetNodeName());

	static char *nullErr = "(null)";
	if (QueryURI::kADDCommand == queryPtr->GetCommandID())
	{
		if (GetMyFieldType() == eArrayNode)
		{
			RespondToAdd(inStream, 0, queryPtr);
		}
		else
		{
			RespondWithSelfAdd(inStream, queryPtr);
		}

		return;

	}

	if (QueryURI::kDELCommand == queryPtr->GetCommandID())
	{
		GetParentNode()->RespondToDel(inStream, GetMyKey(), queryPtr, true);
		return;
	}


	if (GetNodeName() == nullptr)
	{   //printf("ElementNode::RespondWithSelf Node = %s is Uninitialized no name so LEAVE\n",GetNodeName() );
		return;
	}

	if (!fInitialized)
	{   //printf("ElementNode::RespondWithSelf not Initialized EXIT\n");
		return;
	}

	if (nullptr == queryPtr)
	{   //printf("ElementNode::RespondWithSelf NULL == queryPtr EXIT\n");
		return;
	}

	if (queryPtr->fNumFilters > 0)
	{
		bool foundFilter = false;
		StrPtrLen*  theFilterPtr;
		for (int32_t count = 0; count < queryPtr->fNumFilters; count++)
		{
			theFilterPtr = queryPtr->GetFilter(count);
			if (theFilterPtr && theFilterPtr->Equal(StrPtrLen(GetMyName())))
			{
				foundFilter = true;
				//printf("ElementNode::RespondWithSelf found filter = ");PRINT_STR(theFilterPtr);
				break;
			}
		}
		if (!foundFilter) return;
	}

	StrPtrLen bufferSPL;

	uint32_t parameters = queryPtr->GetParamBits();
	parameters &= ~QueryURI::kRecurseParam; // clear recurse flag
	parameters &= ~QueryURI::kDebugParam; // clear verbose flag
	parameters &= ~QueryURI::kIndexParam; // clear index flag


	bool isVerbosePath = 0 != (parameters & QueryURI::kVerboseParam);
	if (isVerbosePath)
	{
		parameters &= ~QueryURI::kVerboseParam; // clear verbose flag
		GetFullPath(&bufferSPL);
		(void)QTSS_Write(inStream, bufferSPL.Ptr, ::strlen(bufferSPL.Ptr), nullptr, 0);
		//printf("ElementNode::RespondWithSelf Path=%s \n",bufferSPL.Ptr);
	}

	if (IsNodeElement())
	{
		if (!isVerbosePath) // this node name already in path
		{
			(void)QTSS_Write(inStream, GetNodeName(), GetNodeNameLen(), nullptr, 0);
			(void)QTSS_Write(inStream, "/", 1, nullptr, 0);
			//printf("ElementNode::RespondWithSelf %s/ \n",GetNodeName());
		}
	}
	else
	{   //printf(" ****** ElementNode::RespondWithSelf NOT a node **** \n");
		(void)QTSS_Write(inStream, GetNodeName(), GetNodeNameLen(), nullptr, 0);
		(void)QTSS_Write(inStream, "=", 1, nullptr, 0);
		//printf(" %s=",GetNodeName());

		char *dataPtr = GetMyElementDataPtr();
		if (dataPtr == nullptr)
		{
			(void)QTSS_Write(inStream, nullErr, ::strlen(nullErr), nullptr, 0);
		}
		else
		{
			(void)QTSS_Write(inStream, dataPtr, ::strlen(dataPtr), nullptr, 0);
		}
		//printf(" %s",buffer);

	}

	if (parameters)
	{
		(void)QTSS_Write(inStream, sParameterDelimeter, 1, nullptr, 0);
		//printf(" %s",sParameterDelimeter);
	}

	if (parameters & QueryURI::kAccessParam)
	{
		(void)QTSS_Write(inStream, sAccess, 2, nullptr, 0);
		//printf(" %s",sAccess);
		(void)QTSS_Write(inStream, GetMyAccessData(), GetMyAccessLen(), nullptr, 0);
		//printf("%s",GetMyAccessData());
		parameters &= ~QueryURI::kAccessParam; // clear access flag

		if (parameters)
		{
			(void)QTSS_Write(inStream, sListDelimeter, 1, nullptr, 0);
			//printf("%s",sListDelimeter);
		}
	}

	if (parameters & QueryURI::kTypeParam)
	{
		(void)QTSS_Write(inStream, sType, 2, nullptr, 0);
		//printf(" %s",sType);
		char *theTypeString = GetMyAPI_TypeStr();
		if (theTypeString == nullptr)
		{
			(void)QTSS_Write(inStream, nullErr, ::strlen(nullErr), nullptr, 0);
		}
		else
		{
			(void)QTSS_Write(inStream, theTypeString, strlen(theTypeString), nullptr, 0);
			//printf("%s",theTypeString);
		}

		parameters &= ~QueryURI::kTypeParam; // clear type flag

		if (parameters)
		{
			(void)QTSS_Write(inStream, sListDelimeter, 1, nullptr, 0);
			//printf("%s",sListDelimeter);
		}
	}
	queryPtr->SetQueryHasResponse();
	(void)QTSS_Write(inStream, "\n", 1, nullptr, 0);
	//printf("\n");

}

void    ElementNode::RespondToAdd(QTSS_StreamRef inStream, int32_t index, QueryURI *queryPtr)
{
	char messageBuffer[1024] = "";

	//printf("ElementNode::RespondToAdd NODE = %s index = %" _S32BITARG_ " \n",GetNodeName(), (int32_t) index);
	if (GetNumFields() == 0)
	{
		uint32_t result = 405;
		sprintf(messageBuffer, "Attribute does not allow adding. Action not allowed");
		(void)queryPtr->EvalQuery(&result, messageBuffer);
		//printf("ElementNode::RespondToAdd error = %s \n",messageBuffer);
		return;
	}

	if (GetFieldType(index) == eNode)
	{
		RespondWithSelfAdd(inStream, queryPtr);
		return;
	}

	static char *nullErr = "(null)";
	bool nullData = false;
	QTSS_Error err = QTSS_NoErr;
	StrPtrLen bufferSPL(messageBuffer);


	if (!fInitialized)
	{   //printf("ElementNode::RespondToAdd not Initialized EXIT\n");
		return;
	}
	if (nullptr == queryPtr)
	{   //printf("ElementNode::RespondToAdd NULL == queryPtr EXIT\n");
		return;
	}

	if (nullptr == inStream)
	{   //printf("ElementNode::RespondToAdd NULL == inStream EXIT\n");
		return;
	}

	char *dataPtr = GetElementDataPtr(index);
	if (nullptr == dataPtr)
	{   //printf("ElementNode::RespondToAdd NULL == dataPtr EXIT\n");
		//  return;
		dataPtr = nullErr;
		nullData = true;
	}

	queryPtr->SetQueryHasResponse();


	uint32_t accessFlags = 0;
	StrPtrLen *accessParamsPtr = queryPtr->GetAccess();
	if (accessParamsPtr != nullptr)
		accessFlags = queryPtr->GetAccessFlags();
	else
		accessFlags = GetAccessPermissions(index);



	StrPtrLen* valuePtr = queryPtr->GetValue();
	std::unique_ptr<char[]> value(NewCharArrayCopy(valuePtr));
	if (!valuePtr || !valuePtr->Ptr)
	{
		uint32_t result = 400;
		sprintf(messageBuffer, "No value found");
		(void)queryPtr->EvalQuery(&result, messageBuffer);
		return;
	}

	//printf("ElementNode::RespondToAdd theValue= %s theType=%s typeID=%"   _U32BITARG_   " \n",value.GetObject(), GetAPI_TypeStr(index), GetAPI_Type(index));
	char valueBuff[2048] = "";
	uint32_t len = 2048;
	err = QTSS_StringToValue(value.get(), GetAPI_Type(index), valueBuff, &len);
	if (err)
	{
		uint32_t result = 400;
		sprintf(messageBuffer, "QTSS_Error=%" _S32BITARG_ " from QTSS_ConvertStringToType", err);
		(void)queryPtr->EvalQuery(&result, messageBuffer);
		return;
	}

	if (GetFieldType(index) != eNode)
	{
		std::unique_ptr<char[]> typeDeleter(NewCharArrayCopy(queryPtr->GetType()));
		StrPtrLen theQueryType(typeDeleter.get());

		if (typeDeleter.get())
		{
			StrPtrLen attributeString(GetAPI_TypeStr(index));
			if (!attributeString.Equal(theQueryType))
			{
				uint32_t result = 400;
				sprintf(messageBuffer, "Type %s does not match attribute type %s", typeDeleter.get(), attributeString.Ptr);
				(void)queryPtr->EvalQuery(&result, messageBuffer);
				return;
			}
		}

		QTSS_Object source = GetSource();

		uint32_t tempBuff;
		uint32_t attributeLen = sizeof(tempBuff);
		(void)QTSS_GetValue(source, GetAPI_ID(index), 0, &tempBuff, &attributeLen);
		if (attributeLen != len)
		{
			uint32_t result = 400;
			sprintf(messageBuffer, "Data length %"   _U32BITARG_   " does not match attribute len %"   _U32BITARG_   "", len, attributeLen);
			(void)queryPtr->EvalQuery(&result, messageBuffer);
			return;
		}


		uint32_t numValues = 0;
		err = QTSS_GetNumValues(source, GetAPI_ID(index), &numValues);
		if (err)
		{
			uint32_t result = 400;
			sprintf(messageBuffer, "QTSS_Error=%" _S32BITARG_ " from QTSS_GetNumValues", err);
			(void)queryPtr->EvalQuery(&result, messageBuffer);
			return;
		}

		//printf("ElementNode::RespondToAdd QTSS_SetValue object=%"   _U32BITARG_   " attrID=%"   _U32BITARG_   ", index = %"   _U32BITARG_   " valuePtr=%"   _U32BITARG_   " valuelen =%"   _U32BITARG_   " \n",GetSource(),GetAPI_ID(index), GetAttributeIndex(index), valueBuff,len);
		err = QTSS_SetValue(source, GetAPI_ID(index), numValues, valueBuff, len);
		if (err)
		{
			uint32_t result = 400;
			sprintf(messageBuffer, "QTSS_Error=%" _S32BITARG_ " from QTSS_SetValue", err);
			(void)queryPtr->EvalQuery(&result, messageBuffer);
			return;
		}

	}

}

void    ElementNode::RespondToSet(QTSS_StreamRef inStream, int32_t index, QueryURI *queryPtr)
{
	static char *nullErr = "(null)";
	bool nullData = false;
	QTSS_Error err = QTSS_NoErr;
	char messageBuffer[1024] = "";
	StrPtrLen bufferSPL(messageBuffer);

	//printf("ElementNode::RespondToSet NODE = %s index = %" _S32BITARG_ " \n",GetNodeName(), (int32_t) index);

	if (!fInitialized)
	{   //printf("ElementNode::RespondToSet not Initialized EXIT\n");
		return;
	}
	if (nullptr == queryPtr)
	{   //printf("ElementNode::RespondToSet NULL == queryPtr EXIT\n");
		return;
	}

	if (nullptr == inStream)
	{   //printf("ElementNode::RespondToSet NULL == inStream EXIT\n");
		return;
	}

	char *dataPtr = GetElementDataPtr(index);
	if (nullptr == dataPtr)
	{   //printf("ElementNode::RespondToSet NULL == dataPtr EXIT\n");
		//  return;
		dataPtr = nullErr;
		nullData = true;
	}

	queryPtr->SetQueryHasResponse();

	std::unique_ptr<char[]> typeDeleter(NewCharArrayCopy(queryPtr->GetType()));
	StrPtrLen theQueryType(typeDeleter.get());

	if (theQueryType.Len > 0)
	{
		StrPtrLen attributeString(GetAPI_TypeStr(index));
		if (!attributeString.Equal(theQueryType))
		{
			uint32_t result = 400;
			sprintf(messageBuffer, "Type %s does not match attribute type %s", typeDeleter.get(), attributeString.Ptr);
			(void)queryPtr->EvalQuery(&result, messageBuffer);
			return;
		}
	}

	if (0 == (GetAccessPermissions(index) & qtssAttrModeWrite))
	{
		uint32_t result = 400;
		sprintf(messageBuffer, "Attribute is read only. Action not allowed");
		(void)queryPtr->EvalQuery(&result, messageBuffer);
		return;
	}

	if (GetFieldType(index) == eNode)
	{
		uint32_t result = 400;
		sprintf(messageBuffer, "Set of type %s not allowed", typeDeleter.get());
		//printf("ElementNode::RespondToSet (GetFieldType(index) == eNode) %s\n",messageBuffer);
		(void)queryPtr->EvalQuery(&result, messageBuffer);
		return;
	}
	else do
	{

		StrPtrLen* valuePtr = queryPtr->GetValue();
		if (!valuePtr || !valuePtr->Ptr) break;

		char valueBuff[2048] = "";
		uint32_t len = 2048;
		std::unique_ptr<char[]> value(NewCharArrayCopy(valuePtr));

		//printf("ElementNode::RespondToSet valuePtr->Ptr= %s\n",value.GetObject());

		err = QTSS_StringToValue(value.get(), GetAPI_Type(index), valueBuff, &len);
		if (err)
		{   //sprintf(messageBuffer,  "QTSS_Error=%" _S32BITARG_ " from QTSS_ConvertStringToType",err);
			break;
		}

		//printf("ElementNode::RespondToSet QTSS_SetValue object=%"   _U32BITARG_   " attrID=%"   _U32BITARG_   ", index = %"   _U32BITARG_   " valuePtr=%"   _U32BITARG_   " valuelen =%"   _U32BITARG_   " \n",GetSource(),GetAPI_ID(index), GetAttributeIndex(index), valueBuff,len);
		err = QTSS_SetValue(GetSource(), GetAPI_ID(index), GetAttributeIndex(index), valueBuff, len);
		if (err)
		{   //sprintf(messageBuffer,  "QTSS_Error=%" _S32BITARG_ " from QTSS_SetValue",err);
			break;
		}

	} while (false);

	if (err != QTSS_NoErr)
	{
		uint32_t result = 400;
		(void)queryPtr->EvalQuery(&result, messageBuffer);
		//printf("ElementNode::RespondToSet %s len = %"   _U32BITARG_   " ",messageBuffer, result);
		return;
	}

}

void    ElementNode::RespondToDel(QTSS_StreamRef inStream, int32_t index, QueryURI *queryPtr, bool delAttribute)
{
	static char *nullErr = "(null)";
	bool nullData = false;
	QTSS_Error err = QTSS_NoErr;
	char messageBuffer[1024] = "";
	StrPtrLen bufferSPL(messageBuffer);

	//printf("ElementNode::RespondToDel NODE = %s index = %" _S32BITARG_ " \n",GetNodeName(), (int32_t) index);

	if (!fInitialized)
	{   //printf("ElementNode::RespondToDel not Initialized EXIT\n");
		return;
	}
	if (nullptr == queryPtr)
	{   //printf("ElementNode::RespondToDel NULL == queryPtr EXIT\n");
		return;
	}

	if (nullptr == inStream)
	{   //printf("ElementNode::RespondToDel NULL == inStream EXIT\n");
		return;
	}

	//printf("ElementNode::RespondToDel NODE = %s index = %" _S32BITARG_ " \n",GetNodeName(), (int32_t) index);
	if (GetNumFields() == 0
		|| (0 == (GetAccessPermissions(index) & qtssAttrModeDelete) && GetMyFieldType() == eArrayNode && GetNumFields() == 1)
		|| (0 == (GetAccessPermissions(index) & qtssAttrModeDelete) && GetMyFieldType() != eArrayNode)
		)
	{
		uint32_t result = 405;
		sprintf(messageBuffer, "Attribute does not allow deleting. Action not allowed");
		(void)queryPtr->EvalQuery(&result, messageBuffer);
		//printf("ElementNode::RespondToDel error = %s \n",messageBuffer);
		return;
	}

	char *dataPtr = GetElementDataPtr(index);
	if (nullptr == dataPtr)
	{   //printf("ElementNode::RespondToDel NULL == dataPtr EXIT\n");
		//  return;
		dataPtr = nullErr;
		nullData = true;
	}

	queryPtr->SetQueryHasResponse();

	// DMS - Removeable is no longer a permission bit
	//
	//if (GetMyFieldType() != eArrayNode && 0 == (GetAccessPermissions(index) & qtssAttrModeRemoveable)) 
	//{
	//  uint32_t result = 405;
	//  sprintf(messageBuffer,  "Attribute is not removable. Action not allowed");
	//  (void) queryPtr->EvalQuery(&result, messageBuffer);
	//  return;
	//} 

	if (GetMyFieldType() == eArrayNode && !delAttribute)
	{
		uint32_t result = 500;
		err = QTSS_RemoveValue(GetSource(), GetAPI_ID(index), GetAttributeIndex(index));
		sprintf(messageBuffer, "QTSS_Error=%" _S32BITARG_ " from QTSS_RemoveValue", err);
		//printf("ElementNode::RespondToDel QTSS_RemoveValue object=%"   _U32BITARG_   " attrID=%"   _U32BITARG_   " index=%"   _U32BITARG_   " %s\n",GetSource(),GetAPI_ID(index),GetAttributeIndex(index),messageBuffer);
		if (err)
		{
			(void)queryPtr->EvalQuery(&result, messageBuffer);
		}
	}
	else
	{
		//printf("ElementNode::RespondToDel QTSS_RemoveInstanceAttribute object=%"   _U32BITARG_   " attrID=%"   _U32BITARG_   " \n",GetSource(),GetAPI_ID(index));
		err = QTSS_RemoveInstanceAttribute(GetSource(), GetAPI_ID(index));
		if (err)
		{
			sprintf(messageBuffer, "QTSS_Error=%" _S32BITARG_ " from QTSS_RemoveInstanceAttribute", err);
		}

	}

	if (err != QTSS_NoErr)
	{
		uint32_t result = 400;
		(void)queryPtr->EvalQuery(&result, messageBuffer);
		//printf("ElementNode::RespondToDel %s len = %"   _U32BITARG_   " ",messageBuffer, result);
		return;
	}

}

bool ElementNode::IsFiltered(int32_t index, QueryURI *queryPtr)
{
	bool foundFilter = false;
	StrPtrLen*  theFilterPtr;
	for (int32_t count = 0; count < queryPtr->fNumFilters; count++)
	{
		theFilterPtr = queryPtr->GetFilter(count);
		if (theFilterPtr && theFilterPtr->Equal(StrPtrLen(GetName(index))))
		{
			foundFilter = true;
			break;
		}
	}
	return foundFilter;
}

void ElementNode::RespondToGet(QTSS_StreamRef inStream, int32_t index, QueryURI *queryPtr)
{
	static char *nullErr = "(null)";
	bool nullData = false;

	//printf("ElementNode::RespondToGet NODE = %s index = %" _S32BITARG_ " \n",GetNodeName(), (int32_t) index);

	if (!fInitialized)
	{   //printf("ElementNode::RespondToGet not Initialized EXIT\n");
		return;
	}
	if (nullptr == queryPtr)
	{   //printf("ElementNode::RespondToGet NULL == queryPtr EXIT\n");
		return;
	}

	if (nullptr == inStream)
	{   //printf("ElementNode::RespondToGet NULL == inStream EXIT\n");
		return;
	}

	char *dataPtr = GetElementDataPtr(index);
	if (nullptr == dataPtr)
	{   //printf("ElementNode::RespondToGet NULL == dataPtr EXIT\n");
		//  return;
		dataPtr = nullErr;
		nullData = true;
	}

	StrPtrLen bufferSPL;

	uint32_t parameters = queryPtr->GetParamBits();
	parameters &= ~QueryURI::kRecurseParam; // clear verbose flag
	parameters &= ~QueryURI::kDebugParam; // clear debug flag
	parameters &= ~QueryURI::kIndexParam; // clear index flag

	//printf("ElementNode::RespondToGet QTSS_SetValue object=%"   _U32BITARG_   " attrID=%"   _U32BITARG_   ", index = %"   _U32BITARG_   " \n",GetSource(),GetAPI_ID(index), GetAttributeIndex(index));


	if ((parameters & QueryURI::kVerboseParam))
	{
		parameters &= ~QueryURI::kVerboseParam; // clear verbose flag
		GetFullPath(&bufferSPL);
		(void)QTSS_Write(inStream, bufferSPL.Ptr, ::strlen(bufferSPL.Ptr), nullptr, 0);
		//printf("ElementNode::RespondToGet Path=%s \n",bufferSPL.Ptr);
	}


	(void)QTSS_Write(inStream, GetName(index), GetNameLen(index), nullptr, 0);
	//printf("ElementNode::RespondToGet %s:len = %"   _U32BITARG_   "",GetName(index),(uint32_t) GetNameLen(index));

	if (IsNodeElement(index))
	{
		(void)QTSS_Write(inStream, "/\"", 1, nullptr, 0);
		//printf(" %s/\"",GetNodeName());
	}
	else
	{
		if (nullData)
		{
			(void)QTSS_Write(inStream, "=", 1, nullptr, 0);
			(void)QTSS_Write(inStream, dataPtr, ::strlen(dataPtr), nullptr, 0);
		}
		else
		{
			(void)QTSS_Write(inStream, "=\"", 2, nullptr, 0);
			(void)QTSS_Write(inStream, dataPtr, ::strlen(dataPtr), nullptr, 0);
			(void)QTSS_Write(inStream, "\"", 1, nullptr, 0);
		}
	}

	//printf(" %s len = %d ",buffer, ::strlen(buffer));
	//DebugShowFieldDataType(index);

	if (parameters)
	{
		(void)QTSS_Write(inStream, sParameterDelimeter, 1, nullptr, 0);
		//printf(" %s",sParameterDelimeter);
	}

	if (parameters & QueryURI::kAccessParam)
	{
		(void)QTSS_Write(inStream, sAccess, 2, nullptr, 0);
		//printf(" %s",sAccess);
		(void)QTSS_Write(inStream, GetAccessData(index), GetAccessLen(index), nullptr, 0);
		//printf("%s",GetAccessData(index));
		parameters &= ~QueryURI::kAccessParam; // clear access flag

		if (parameters)
		{
			(void)QTSS_Write(inStream, sListDelimeter, 1, nullptr, 0);
			//printf("%s",sListDelimeter);
		}
	}

	if (parameters & QueryURI::kTypeParam)
	{
		(void)QTSS_Write(inStream, sType, 2, nullptr, 0);
		//printf(" %s",sType);
		char* typeStringPtr = GetAPI_TypeStr(index);
		if (typeStringPtr == nullptr)
		{
			//printf("ElementNode::RespondToGet typeStringPtr is NULL for type = %s \n", typeStringPtr);
			(void)QTSS_Write(inStream, nullErr, ::strlen(nullErr), nullptr, 0);
		}
		else
		{
			//printf("ElementNode::RespondToGet type = %s \n", typeStringPtr);
			(void)QTSS_Write(inStream, typeStringPtr, strlen(typeStringPtr), nullptr, 0);
		}

		parameters &= ~QueryURI::kTypeParam; // clear type flag

		if (parameters)
		{
			(void)QTSS_Write(inStream, sListDelimeter, 1, nullptr, 0);
			//printf("%s",sListDelimeter);
		}
	}


	(void)QTSS_Write(inStream, "\n", 1, nullptr, 0);
	//printf(" %s","\n");

	queryPtr->SetQueryHasResponse();

}

void    ElementNode::RespondToKey(QTSS_StreamRef inStream, int32_t index, QueryURI *queryPtr)
{
	int32_t command = queryPtr->GetCommandID();
	//printf("ElementNode::RespondToKey command = %" _S32BITARG_ " node =%s index=%" _S32BITARG_ "\n",command, GetNodeName(),index);

	switch (command)
	{
	case QueryURI::kGETCommand: RespondToGet(inStream, index, queryPtr);
		break;

	case QueryURI::kSETCommand: RespondToSet(inStream, index, queryPtr);
		break;

	case QueryURI::kADDCommand: RespondToAdd(inStream, index, queryPtr);
		break;

	case QueryURI::kDELCommand: RespondToDel(inStream, index, queryPtr, false);
		break;
	}

}

void ElementNode::RespondWithNodeName(QTSS_StreamRef inStream, QueryURI * /*unused queryPtr*/)
{

	//printf("ElementNode::RespondWithNodeName NODE = %s \n",GetNodeName());

	if (!fInitialized)
	{    //printf("ElementNode::RespondWithNodeName not Initialized EXIT\n");
		return;
	}

	StrPtrLen fullPathSPL;
	GetFullPath(&fullPathSPL);

	(void)QTSS_Write(inStream, "Container=\"/", ::strlen("Container=\"/"), nullptr, 0);

	(void)QTSS_Write(inStream, fPathSPL.Ptr, ::strlen(fPathSPL.Ptr), nullptr, 0);
	//printf("ElementNode::RespondWithNodeName Path=%s \n",fPathSPL.Ptr);

	(void)QTSS_Write(inStream, "\"", 1, nullptr, 0);
	//printf("\"");     

	(void)QTSS_Write(inStream, "\n", 1, nullptr, 0);
	//printf("\n");

}

void ElementNode::RespondWithSingleElement(QTSS_StreamRef inStream, QueryURI *queryPtr, StrPtrLen *currentSegmentPtr)
{

	//printf("ElementNode::RespondWithSingleElement Current Node = %s\n",GetNodeName() );

	if (!fInitialized)
	{
		//printf("ElementNode::RespondWithSingleElement failed Not Initialized %s\n",GetNodeName() );
		return;
	}

	if (GetNodeName() == nullptr)
	{
		//printf("ElementNode::RespondWithSingleElement Node = %s is Uninitialized LEAVE\n",GetNodeName() );
		return;
	}

	Assert(queryPtr != nullptr);
	Assert(currentSegmentPtr != nullptr);
	Assert(currentSegmentPtr->Ptr != nullptr);
	Assert(currentSegmentPtr->Len != 0);

	int32_t key = ResolveSPLKeyToIndex(currentSegmentPtr);
	//printf("ElementNode::RespondWithSingleElement key = %" _S32BITARG_ "\n",key);
	//printf("currentSegmentPtr="); PRINT_STR(currentSegmentPtr);

	if (key < 0)
	{
		//printf("ElementNode::RespondWithSingleElement key = %" _S32BITARG_ " NOT FOUND no ELEMENT\n",key);
		return;
	}

	if ((queryPtr == nullptr) || (currentSegmentPtr == nullptr) || (currentSegmentPtr->Ptr == nullptr) || (currentSegmentPtr->Len == 0))
	{
		//printf("ElementNode::RespondWithSingleElement currentSegmentPtr || queryPtr = NULL\n");
		return;
	}

	//add here

	StrPtrLen nextSegment;
	(void)queryPtr->NextSegment(currentSegmentPtr, &nextSegment);

	if ((nextSegment.Len == 0) && !queryPtr->RecurseParam()) // only respond if we are at the end of the path
	{
		//printf("currentSegmentPtr="); PRINT_STR(currentSegmentPtr);
		//printf("nextSegment="); PRINT_STR(&nextSegment);
		//printf("ElementNode::RespondWithSingleElement Current Node = %s Call RespondWithNodeName\n",GetNodeName() );
		if (QueryURI::kGETCommand == queryPtr->GetCommandID())
			RespondWithNodeName(inStream, queryPtr);
	}

	if (IsNodeElement(key))
	{
		auto *theNodePtr = (ElementNode *)GetElementDataPtr(key);
		if (theNodePtr)
		{
			//printf("ElementNode::RespondWithSingleElement Current Node = %s Call RespondToQuery\n",GetNodeName() );
			theNodePtr->RespondToQuery(inStream, queryPtr, currentSegmentPtr);
		}
	}
	else
	{
		//printf("ElementNode::RespondWithSingleElement call RespondToKey\n");
		if ((queryPtr->fNumFilters > 0) && (QueryURI::kGETCommand == queryPtr->GetCommandID()))
		{
			StrPtrLen*  theFilterPtr;
			int32_t index;
			for (int32_t count = 0; count < queryPtr->fNumFilters; count++)
			{
				theFilterPtr = queryPtr->GetFilter(count);
				index = ResolveSPLKeyToIndex(theFilterPtr);
				if (index < 0) continue;
				RespondToKey(inStream, index, queryPtr);
				//printf("ElementNode::RespondWithSingleElement found filter = ");PRINT_STR(theFilterPtr);
				break;
			}
			//printf("ElementNode::RespondWithSingleElement found filter = ?");PRINT_STR(theFilterPtr);
		}
		else
		{
			RespondToKey(inStream, key, queryPtr);
		}
	}

}


void ElementNode::RespondWithAllElements(QTSS_StreamRef inStream, QueryURI *queryPtr, StrPtrLen *currentSegmentPtr)
{
	//printf("ElementNode::RespondWithAllElements %s\n",GetNodeName());
	//printf("ElementNode::RespondWithAllElements fDataFieldsStop = %d \n",fDataFieldsStop);

	if (GetNodeName() == nullptr)
	{   //printf("ElementNode::RespondWithAllElements %s is Uninitialized LEAVE\n",GetNodeName());
		return;
	}

	if (!fInitialized)
	{   //printf("ElementNode::RespondWithAllElements %s is Uninitialized LEAVE\n",GetNodeName());
		return;
	}

	StrPtrLen nextSegment;
	(void)queryPtr->NextSegment(currentSegmentPtr, &nextSegment);

	if ((nextSegment.Len == 0 || nextSegment.Ptr == nullptr))  // only respond if we are at the end of the path
		if (QueryURI::kGETCommand == queryPtr->GetCommandID())
			RespondWithNodeName(inStream, queryPtr);

	if ((queryPtr->fNumFilters > 0) && (QueryURI::kGETCommand == queryPtr->GetCommandID()))
	{
		StrPtrLen*  theFilterPtr;
		int32_t index;
		for (int32_t count = 0; count < queryPtr->fNumFilters; count++)
		{
			theFilterPtr = queryPtr->GetFilter(count);
			index = ResolveSPLKeyToIndex(theFilterPtr);
			if (index < 0) continue;

			if ((nextSegment.Len == 0 || nextSegment.Ptr == nullptr))
			{
				if ((!IsNodeElement(index)) || (IsNodeElement(index) && queryPtr->RecurseParam()))    // only respond if we are at the end of the path 
					RespondToKey(inStream, index, queryPtr);
			}

			//printf("ElementNode::RespondWithAllElements found filter = ");PRINT_STR(theFilterPtr);
		}
	}
	else
	{
		uint32_t index = 0;
		for (index = 0; !IsStopItem(index); index++)
		{
			//printf("RespondWithAllElements = %d \n",index);
			//printf("ElementNode::RespondWithAllElements nextSegment="); PRINT_STR(&nextSegment);

			if ((nextSegment.Len == 0 || nextSegment.Ptr == nullptr))
			{
				if ((!IsNodeElement(index)) || (IsNodeElement(index) && queryPtr->RecurseParam()))   // only respond if we are at the end of the path
					RespondToKey(inStream, index, queryPtr);
			}

		}
	}

	uint32_t index = 0;
	for (index = 0; !IsStopItem(index); index++)
	{

		if (IsNodeElement(index))
		{
			//printf("ElementNode::RespondWithAllElements FoundNode\n");
			//printf("ElementNode::RespondWithAllElements currentSegmentPtr="); PRINT_STR(currentSegmentPtr);
			//printf("ElementNode::RespondWithAllElements nextSegment="); PRINT_STR(&nextSegment);
			auto *theNodePtr = (ElementNode *)GetElementDataPtr(index);

			if (theNodePtr)
			{
				//printf("ElementNode::RespondWithAllElements Current Node = %s Call RespondToQuery\n",GetNodeName() );
				theNodePtr->RespondToQuery(inStream, queryPtr, &nextSegment);
			}
			else
			{
				//printf("ElementNode::RespondWithAllElements Current Node index= %"   _U32BITARG_   " NULL = %s\n",index,GetName(index));

			}
		}
	}
}



void ElementNode::RespondWithAllNodes(QTSS_StreamRef inStream, QueryURI *queryPtr, StrPtrLen *currentSegmentPtr)
{

	//printf("ElementNode::RespondWithAllNodes %s\n",GetNodeName());

	if (!fInitialized)
	{   //printf("ElementNode::RespondWithAllNodes %s is Uninitialized LEAVE\n",GetNodeName());
		return;
	}

	if (GetNodeName() == nullptr)
	{   //printf("ElementNode::RespondWithAllNodes %s is Uninitialized LEAVE\n",GetNodeName());
		return;
	}

	StrPtrLen nextSegment;
	(void)queryPtr->NextSegment(currentSegmentPtr, &nextSegment);

	for (int32_t index = 0; !IsStopItem(index); index++)
	{
		if (!queryPtr->RecurseParam() && (currentSegmentPtr->Len == 0))
		{
			Assert(0);
			break;
		}


		if (IsNodeElement(index))
		{
			//printf("ElementNode::RespondWithAllNodes FoundNode\n");
			//printf("ElementNode::RespondWithAllNodes currentSegmentPtr="); PRINT_STR(currentSegmentPtr);
			auto *theNodePtr = (ElementNode *)GetElementDataPtr(index);
			//printf("ElementNode::RespondWithAllNodes nextSegment="); PRINT_STR(&nextSegment);
			if (theNodePtr)
			{
				//printf("ElementNode::RespondWithAllNodes Current Node = %s Call RespondToQuery\n",GetNodeName() );
				theNodePtr->RespondToQuery(inStream, queryPtr, currentSegmentPtr);
			}
		}
	}
}

void ElementNode::RespondToQuery(QTSS_StreamRef inStream, QueryURI *queryPtr, StrPtrLen *currentPathPtr)
{
	//printf("----- ElementNode::RespondToQuery ------\n");
	//printf("ElementNode::RespondToQuery NODE = %s\n",GetNodeName());

	Assert(nullptr != queryPtr);
	Assert(nullptr != currentPathPtr);

	if (!fInitialized)
	{   //printf("ElementNode::RespondToQuery %s is Uninitialized LEAVE\n",GetNodeName());
		return;

	}

	if (GetNodeName() == nullptr)
	{   //printf("ElementNode::RespondToQuery %s is Uninitialized LEAVE\n",GetNodeName());
		return;
	}


	bool recurse = queryPtr->RecurseParam();
	bool doAllNext = false;
	bool doAllNextNext = false;
	StrPtrLen nextSegment;
	StrPtrLen nextnextSegment;
	StrPtrLen nextnextnextSegment;


	if (queryPtr && currentPathPtr) do
	{
		(void)queryPtr->NextSegment(currentPathPtr, &nextSegment);
		(void)queryPtr->NextSegment(&nextSegment, &nextnextSegment);
		(void)queryPtr->NextSegment(&nextnextSegment, &nextnextnextSegment);

		//printf("ElementNode::RespondToQuery currentPathPtr="); PRINT_STR( currentPathPtr);
		//printf("ElementNode::RespondToQuery nextSegment="); PRINT_STR( &nextSegment);
		//printf("ElementNode::RespondToQuery nextnextSegment="); PRINT_STR( &nextnextSegment);

		 // recurse param is set and this is the end of the path
		if (recurse && ((0 == currentPathPtr->Len) || (0 == nextSegment.Len)))
		{   // admin 
			//printf("ElementNode::RespondToQuery 1)RespondToQuery -> RespondWithAllElements ") ;PRINT_STR( GetNodeNameSPL());
			RespondWithAllElements(inStream, queryPtr, &nextSegment);
			break;
		}

		// recurse param is not set and this is the end of the path
		if ((!recurse && ((0 == currentPathPtr->Len) || (0 == nextSegment.Len)))
			)
		{   // admin 
			//printf("ElementNode::RespondToQuery 2)RespondToQuery -> RespondWithSelf ") ;PRINT_STR( GetNodeNameSPL());
			if (fIsTop)
				(void)QTSS_Write(inStream, "Container=\"/\"\n", ::strlen("Container=\"/\"\n"), nullptr, 0);

			RespondWithSelf(inStream, queryPtr);
			break;
		}


		doAllNext = ElementNode_DoAll(&nextSegment);
		doAllNextNext = ElementNode_DoAll(&nextnextSegment);

		if (doAllNext && (0 == nextnextSegment.Len))
		{   // admin/*
			//printf("ElementNode::RespondToQuery 3)RespondToQuery -> RespondWithAllElements ");PRINT_STR( &nextSegment);
			RespondWithAllElements(inStream, queryPtr, &nextSegment);
			break;
		}

		if (doAllNext && doAllNextNext)
		{   // admin/*/*
			//printf("ElementNode::RespondToQuery 4)RespondToQuery -> RespondWithAllNodes ");PRINT_STR( currentPathPtr);
			RespondWithAllNodes(inStream, queryPtr, &nextSegment);
			break;
		}


		if (doAllNext && (nextnextSegment.Len > 0))
		{   // admin/*/attribute
			//printf("ElementNode::RespondToQuery 5)RespondToQuery -> RespondWithAllNodes  ");PRINT_STR( &nextSegment);
			RespondWithAllNodes(inStream, queryPtr, &nextSegment);
			break;
		}

		// admin/attribute
		//printf("ElementNode::RespondToQuery 6)RespondToQuery -> RespondWithSingleElement ");PRINT_STR( &nextSegment);
		RespondWithSingleElement(inStream, queryPtr, &nextSegment);

	} while (false);

	if (QueryURI::kGETCommand != queryPtr->GetCommandID() && (!queryPtr->fIsPref))
	{
		queryPtr->fIsPref = IsPreferenceContainer(GetMyName(), nullptr);
	}
	//printf("ElementNode::RespondToQuery LEAVE\n");
}


void ElementNode::SetupNodes(QueryURI *queryPtr, StrPtrLen *currentPathPtr, QTSS_Initialize_Params *initParams)
{
	//printf("----- ElementNode::SetupNodes ------ NODE = %s\n", GetNodeName()); 
	//printf("ElementNode::SetupNodes currentPathPtr ="); PRINT_STR(currentPathPtr);
	if (fSelfPtr == nullptr)
	{   //printf("******* ElementNode::SetupNodes (fSelfPtr == NULLL) \n");
	}
	Assert(nullptr != queryPtr);
	Assert(nullptr != currentPathPtr);

	if (queryPtr && currentPathPtr) do
	{
		bool doAll = false;
		StrPtrLen nextSegment;

		(void)queryPtr->NextSegment(currentPathPtr, &nextSegment);
		doAll = ElementNode_DoAll(&nextSegment);

		StrPtrLen *thisNamePtr = GetNodeNameSPL();
		//printf("ElementNode::SetupNodes thisNamePtr="); PRINT_STR(thisNamePtr);

		if (((doAll) && (currentPathPtr->Equal(*thisNamePtr) || ElementNode_DoAll(currentPathPtr)))
			|| (queryPtr->RecurseParam())
			)
		{
			SetUpAllElements(queryPtr, currentPathPtr, &nextSegment, initParams);
			break;
		}

		int32_t index = ResolveSPLKeyToIndex(&nextSegment);
		if (index < 0)
		{
			//printf("ElementNode::SetupNodes FAILURE ResolveSPLKeyToIndex = %d NODE = %s\n", index, GetNodeName());
			break;
		}

		SetUpAllNodes(queryPtr, currentPathPtr, &nextSegment, initParams);

		if (nullptr == GetElementDataPtr(index))
		{   //printf("ElementNode::SetupNodes call SetUpSingleElement index=%"   _U32BITARG_   " nextSegment=");PRINT_STR( &nextSegment);
			SetUpSingleElement(queryPtr, currentPathPtr, &nextSegment, index, initParams);
		}


	} while (false);

}

void ElementNode::GetFilteredAttributeName(ElementDataFields* fieldPtr, QTSS_AttributeID theID)
{
	fieldPtr->fFieldLen = 0;
	char *theName = nullptr;
	(void)QTSS_GetValueAsString(fieldPtr->fAPISource, theID, 0, &theName);
	std::unique_ptr<char[]> nameDeleter(theName);
	if (theName != nullptr)
	{
		uint32_t len = strlen(theName);
		if (len < eMaxAttributeNameSize)
		{
			memcpy(fieldPtr->fFieldName, theName, len);
			fieldPtr->fFieldName[len] = 0;
			fieldPtr->fFieldLen = len;
		}
	}
}

bool ElementNode::GetFilteredAttributeID(char *parentName, char *nodeName, QTSS_AttributeID* foundID)
{
	bool found = false;

	if (0 == strcmp("server", parentName))
	{
		if (0 == strcmp("qtssSvrClientSessions", nodeName))
		{
			if (foundID)
				*foundID = qtssCliSesCounterID;
			found = true;
		}

		if (0 == strcmp("qtssSvrModuleObjects", nodeName))
		{
			if (foundID)
				*foundID = qtssModName;
			found = true;
		}
	}
	return found;
};

bool ElementNode::IsPreferenceContainer(char *nodeName, QTSS_AttributeID* foundID)
{
	bool found = false;
	if (foundID) *foundID = 0;
	//printf(" ElementNode::IsPreferenceContainer name = %s \n",nodeName);
	if (0 == strcmp("qtssSvrPreferences", nodeName))
	{
		if (foundID) *foundID = qtssCliSesCounterID;
		found = true;
	}

	if (0 == strcmp("qtssModPrefs", nodeName))
	{
		if (foundID) *foundID = qtssModName;
		found = true;
	}

	return found;
};

ElementNode::ElementDataFields AdminClass::sAdminSelf[] = // special case of top of tree
{   // key, API_ID,     fIndex,     Name,       Name_Len,       fAccessPermissions, Access, access_Len, fAPI_Type, fFieldType ,fAPISource
	{0,     0,          0,          "admin",    strlen("admin"),    qtssAttrModeRead, "r", strlen("r"),0,   ElementNode::eNode, nullptr    }
};

ElementNode::ElementDataFields AdminClass::sAdminFieldIDs[] =
{   // key, API_ID,     fIndex,     Name,       Name_Len,       fAccessPermissions, Access, access_Len, fAPI_Type, fFieldType ,fAPISource
	{0,     0,          0,          "server",   strlen("server"),qtssAttrModeRead,  "r", strlen("r"),qtssAttrDataTypeQTSS_Object,   ElementNode::eNode, nullptr    }
};



void AdminClass::Initialize(QTSS_Initialize_Params *initParams, QueryURI *queryPtr)
{

	//printf("----- Initialize AdminClass ------\n");

	SetParentNode(nullptr);
	SetNodeInfo(&sAdminSelf[0]);// special case of this node as top of tree so it sets self
	Assert(nullptr != GetMyName());
	SetNodeName(GetMyName());
	SetSource(nullptr);
	StrPtrLen *currentPathPtr = queryPtr->GetRootID();
	uint32_t numFields = 1;
	SetNumFields(numFields);
	fFieldIDs = sAdminFieldIDs;
	fDataFieldsType = eStatic;
	fPathBuffer[0] = 0;
	fPathSPL.Set(fPathBuffer, 0);
	fIsTop = true;
	fInitialized = true;
	do
	{
		Assert(fElementMap == nullptr);
		fElementMap = new OSRefTable();  ElementNode_InsertPtr(fElementMap, "AdminClass::Initialize ElementNode* fElementMap ");
		Assert(fElementMap != nullptr);
		if (fElementMap == nullptr) break;

		Assert(fFieldDataPtrs == nullptr);
		fFieldDataPtrs = new char*[numFields];  ElementNode_InsertPtr(fFieldDataPtrs, "AdminClass::Initialize ElementNode* fFieldDataPtrs array ");
		Assert(fFieldDataPtrs != nullptr);
		if (fFieldDataPtrs == nullptr) break;
		memset(fFieldDataPtrs, 0, numFields * sizeof(char*));

		Assert(fFieldOSRefPtrs == nullptr);
		fFieldOSRefPtrs = new OSRef *[numFields]; ElementNode_InsertPtr(fFieldOSRefPtrs, "AdminClass::Initialize ElementNode* fFieldOSRefPtrs array ");
		Assert(fFieldOSRefPtrs != nullptr);
		if (fFieldOSRefPtrs == nullptr) break;
		memset(fFieldOSRefPtrs, 0, numFields * sizeof(OSRef*));

		QTSS_Error err = fElementMap->Register(GetOSRef(0));
		Assert(err == QTSS_NoErr);
	} while (false);

	if (queryPtr && currentPathPtr) do
	{
		StrPtrLen nextSegment;
		if (!queryPtr->NextSegment(currentPathPtr, &nextSegment)) break;

		SetupNodes(queryPtr, currentPathPtr, initParams);
	} while (false);


};

void AdminClass::SetUpSingleNode(QueryURI *queryPtr, StrPtrLen *currentSegmentPtr, StrPtrLen *nextSegmentPtr, int32_t index, QTSS_Initialize_Params *initParams)
{
	//printf("-------- AdminClass::SetUpSingleNode ---------- \n");
	switch (index)
	{
	case eServer:
		//printf("AdminClass::SetUpSingleNode case eServer\n");
		fNodePtr = new ElementNode();  ElementNode_InsertPtr(fNodePtr, "AdminClass::SetUpSingleNode ElementNode * new ElementNode()");
		SetElementDataPtr(index, (char *)fNodePtr, true);
		if (fNodePtr)
		{
			fNodePtr->Initialize(index, this, queryPtr, nextSegmentPtr, initParams, initParams->inServer, eDynamic);
		}
		break;
	};

}
void AdminClass::SetUpSingleElement(QueryURI *queryPtr, StrPtrLen *currentSegmentPtr, StrPtrLen *nextSegmentPtr, int32_t index, QTSS_Initialize_Params *initParams)
{
	//printf("---------AdminClass::SetUpSingleElement------- \n");
	SetUpSingleNode(queryPtr, currentSegmentPtr, nextSegmentPtr, index, initParams);
}

AdminClass::~AdminClass()
{   //printf("AdminClass::~AdminClass() \n");
	delete (ElementNode*)fNodePtr; ElementNode_RemovePtr(fNodePtr, "AdminClass::~AdminClass ElementNode* fNodePtr");
	fNodePtr = nullptr;
}

