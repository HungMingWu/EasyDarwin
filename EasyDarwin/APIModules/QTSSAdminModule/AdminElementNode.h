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
	 File:       AdminElements.h

	 Contains:   implements various Admin Elements class


 */
#ifndef _ADMINELEMENTNODE_H_
#define _ADMINELEMENTNODE_H_



#ifndef __Win32__
#include <unistd.h>     /* for getopt() et al */
#endif

#include <stdio.h>      /* for //qtss_printf */
#include "OSArrayObjectDeleter.h"
#include "StrPtrLen.h"
#include "OSRef.h"
#include "AdminQuery.h"

void PRINT_STR(StrPtrLen *spl);
void COPYBUFFER(char *dest, char *src, int8_t size);

void ElementNode_InitPtrArray();
void ElementNode_InsertPtr(void *ptr, char * src);
void ElementNode_RemovePtr(void *ptr, char * src);
int32_t ElementNode_CountPtrs();
void ElementNode_ShowPtrs();

class ClientSession {
public:
	ClientSession(void) : fRTSPSessionID(0), fBitrate(0), fPacketLossPercent(0), fBytesSent(0), fTimeConnected(0) {};
	~ClientSession() { };
	uint32_t fRTSPSessionID;
	char fIPAddressStr[32];
	char fURLBuffer[512];
	uint32_t fBitrate;
	float fPacketLossPercent;
	int64_t fBytesSent;
	int64_t fTimeConnected;

};


class ElementNode
{
public:
	enum { eMaxAccessSize = 32, eMaxAttributeNameSize = 63, eMaxAPITypeSize = 63, eMaxAttrIDSize = sizeof(uint32_t) };

	enum { eData = 0, eArrayNode, eNode };


#define kEmptyRef (OSRef *)NULL
#define kEmptyData (char *)NULL


	enum { kFirstIndexItem = 0 };


	typedef enum
	{
		eStatic = 0,
		eDynamic = 1,
	} DataFieldsType;

	struct ElementDataFields
	{
		uint32_t                  fKey;
		uint32_t                  fAPI_ID;
		uint32_t                  fIndex;

		char                    fFieldName[eMaxAttributeNameSize + 1];
		uint32_t                  fFieldLen;

		QTSS_AttrPermission     fAccessPermissions;
		char                    fAccessData[eMaxAccessSize + 1];
		uint32_t                  fAccessLen;

		uint32_t                  fAPI_Type;
		uint32_t                  fFieldType;

		QTSS_Object             fAPISource;

	};

	int32_t                  fDataFieldsStop;

	uint32_t  CountElements();

	int32_t  GetMyStopItem() { Assert(fSelfPtr); return fDataFieldsStop; };
	uint32_t  GetMyKey() { Assert(fSelfPtr); return fSelfPtr->fKey; };
	char*   GetMyName() { Assert(fSelfPtr); return fSelfPtr->fFieldName; };
	uint32_t  GetMyNameLen() { Assert(fSelfPtr); return fSelfPtr->fFieldLen; };
	uint32_t  GetMyAPI_ID() { Assert(fSelfPtr); return fSelfPtr->fAPI_ID; };
	uint32_t  GetMyIndex() { Assert(fSelfPtr); return fSelfPtr->fIndex; };

	uint32_t  GetMyAPI_Type() { Assert(fSelfPtr); return fSelfPtr->fAPI_Type; };
	char*   GetMyAPI_TypeStr() { Assert(fSelfPtr); char* theTypeString = NULL; (void)QTSS_TypeToTypeString(GetMyAPI_Type(), &theTypeString); return theTypeString; };
	uint32_t  GetMyFieldType() { Assert(fSelfPtr); return fSelfPtr->fFieldType; };

	char*   GetMyAccessData() { Assert(fSelfPtr); return fSelfPtr->fAccessData; };
	uint32_t  GetMyAccessLen() { Assert(fSelfPtr); return fSelfPtr->fAccessLen; };
	uint32_t  GetMyAccessPermissions() { Assert(fSelfPtr); return fSelfPtr->fAccessPermissions; };

	void    GetMyNameSPL(StrPtrLen* str) { Assert(str); if (str != NULL) str->Set(fSelfPtr->fFieldName, fSelfPtr->fFieldLen); };
	void    GetMyAccess(StrPtrLen* str) { Assert(str); if (str != NULL) str->Set(fSelfPtr->fAccessData, fSelfPtr->fAccessLen); };
	QTSS_Object GetMySource() {
		Assert(fSelfPtr != NULL);
		//qtss_printf("GetMySource fSelfPtr->fAPISource = %"_U32BITARG_" \n", fSelfPtr->fAPISource); 
		return fSelfPtr->fAPISource;
	};

	bool  IsNodeElement() { Assert(this); return (this->GetMyFieldType() == eNode || this->GetMyFieldType() == eArrayNode); }


	bool  IsStopItem(int32_t index) { return index == GetMyStopItem(); };
	uint32_t  GetKey(int32_t index) { return fFieldIDs[index].fKey; };
	char*   GetName(int32_t index) { return fFieldIDs[index].fFieldName; };
	uint32_t  GetNameLen(int32_t index) { return fFieldIDs[index].fFieldLen; };
	uint32_t  GetAPI_ID(int32_t index) { return fFieldIDs[index].fAPI_ID; };
	uint32_t  GetAttributeIndex(int32_t index) { return fFieldIDs[index].fIndex; };
	uint32_t  GetAPI_Type(int32_t index) { return fFieldIDs[index].fAPI_Type; };
	char*   GetAPI_TypeStr(int32_t index) { char* theTypeStr = NULL; (void)QTSS_TypeToTypeString(GetAPI_Type(index), &theTypeStr); return theTypeStr; };
	uint32_t  GetFieldType(int32_t index) { return fFieldIDs[index].fFieldType; };
	char*   GetAccessData(int32_t index) { return fFieldIDs[index].fAccessData; };
	uint32_t  GetAccessLen(int32_t index) { return fFieldIDs[index].fAccessLen; };
	uint32_t  GetAccessPermissions(int32_t index) { return fFieldIDs[index].fAccessPermissions; };
	void    GetNameSPL(int32_t index, StrPtrLen* str) { if (str != NULL) str->Set(fFieldIDs[index].fFieldName, fFieldIDs[index].fFieldLen); };
	void    GetAccess(int32_t index, StrPtrLen* str) { if (str != NULL) str->Set(fFieldIDs[index].fAccessData, fFieldIDs[index].fAccessLen); };
	QTSS_Object GetAPISource(int32_t index) { return fFieldIDs[index].fAPISource; };
	bool  IsNodeElement(int32_t index) { return (GetFieldType(index) == eNode || GetFieldType(index) == eArrayNode); }

	enum
	{
		eAPI_ID = 0,
		eAPI_Name = 1,
		eAccess = 2,
		ePath = 3,
		eType = 4,
		eNumAttributes = 5
	};

	ElementNode();
	void Initialize(int32_t index, ElementNode *parentPtr, QueryURI *queryPtr, StrPtrLen *currentSegmentPtr, QTSS_Initialize_Params *initParams, QTSS_Object nodeSource, DataFieldsType dataFieldsType);
	virtual ~ElementNode();

	void            SetNodeName(char *namePtr);
	char *          GetNodeName() { return fNodeNameSPL.Ptr; };
	uint32_t          GetNodeNameLen() { return fNodeNameSPL.Len; };
	StrPtrLen*      GetNodeNameSPL() { return &fNodeNameSPL; };

	void            SetParentNode(ElementNode *parentPtr) { fParentNodePtr = parentPtr; };
	ElementNode*    GetParentNode() { return fParentNodePtr; };
	void            GetFullPath(StrPtrLen *resultPtr);

	OSRef*  GetOSRef(int32_t index);
	void    SetOSRef(int32_t index, OSRef* refPtr);
	int32_t  ResolveSPLKeyToIndex(StrPtrLen *keyPtr);
	virtual bool  SetUpOneDataField(uint32_t index);

	ElementDataFields   *GetElementFieldPtr(int32_t index);
	char                *GetElementDataPtr(int32_t index);
	void                SetElementDataPtr(int32_t index, char * data, bool isNode);
	void                SetMyElementDataPtr(char * data) { fSelfDataPtr = data; }
	char*               GetMyElementDataPtr() { return fSelfDataPtr; }
	bool              IsFiltered(int32_t index, QueryURI *queryPtr);

	ElementDataFields   *GetNodeInfoPtr(int32_t index);

	void    SetNodeInfo(ElementDataFields *nodeInfo);
	void    SetSource(void * dataSource) { fDataSource = dataSource; };
	void *  GetSource() {
		QTSS_Object source = GetMySource();
		if (source != NULL)
			return source;
		else
		{   //qtss_printf("GetSource return fDataSource = %"_U32BITARG_" \n",fDataSource);
			return fDataSource;
		}
	};

	virtual void    SetUpSingleNode(QueryURI *queryPtr, StrPtrLen *currentSegmentPtr, StrPtrLen *nextSegmentPtr, int32_t index, QTSS_Initialize_Params *initParams);
	virtual void    SetUpAllNodes(QueryURI *queryPtr, StrPtrLen *currentSegmentPtr, StrPtrLen *nextSegmentPtr, QTSS_Initialize_Params *initParams);

	virtual void    SetUpSingleElement(QueryURI *queryPtr, StrPtrLen *currentSegmentPtr, StrPtrLen *nextSegmentPtr, int32_t index, QTSS_Initialize_Params *initParams);
	virtual void    SetUpAllElements(QueryURI *queryPtr, StrPtrLen *currentSegmentPtr, StrPtrLen *nextSegmentPtr, QTSS_Initialize_Params *initParams);
	virtual void    SetupNodes(QueryURI *queryPtr, StrPtrLen *currentPathPtr, QTSS_Initialize_Params *initParams);


	void    RespondWithSelfAdd(QTSS_StreamRef inStream, QueryURI *queryPtr);
	void    RespondToAdd(QTSS_StreamRef inStream, int32_t index, QueryURI *queryPtr);
	void    RespondToSet(QTSS_StreamRef inStream, int32_t index, QueryURI *queryPtr);
	void    RespondToGet(QTSS_StreamRef inStream, int32_t index, QueryURI *queryPtr);
	void    RespondToDel(QTSS_StreamRef inStream, int32_t index, QueryURI *queryPtr, bool delAttribute);
	void    RespondToKey(QTSS_StreamRef inStream, int32_t index, QueryURI *queryPtr);

	void    RespondWithNodeName(QTSS_StreamRef inStream, QueryURI *queryPtr);
	void    RespondWithSelf(QTSS_StreamRef inStream, QueryURI *queryPtr);
	void    RespondWithSingleElement(QTSS_StreamRef inStream, QueryURI *queryPtr, StrPtrLen *currentSegmentPtr);
	void    RespondWithAllElements(QTSS_StreamRef inStream, QueryURI *queryPtr, StrPtrLen *currentSegmentPtr);
	void    RespondWithAllNodes(QTSS_StreamRef inStream, QueryURI *queryPtr, StrPtrLen *currentSegmentPtr);
	void    RespondToQuery(QTSS_StreamRef inStream, QueryURI *queryPtr, StrPtrLen *currentPathPtr);

	uint32_t  CountAttributes(QTSS_Object source);
	uint32_t  CountValues(QTSS_Object source, uint32_t apiID);

	QTSS_Error      AllocateFields(uint32_t numFields);
	void            InitializeAllFields(bool allocateFields, QTSS_Object defaultAttributeInfo, QTSS_Object source, QueryURI *queryPtr, StrPtrLen *currentSegmentPtr, bool forceAll);
	void            InitializeSingleField(StrPtrLen *currentSegmentPtr);
	void            SetFields(uint32_t i, QTSS_Object attrInfoObject);
	ElementNode*    CreateArrayAttributeNode(uint32_t index, QTSS_Object source, QTSS_Object attributeInfo, uint32_t arraySize);

	QTSS_Error      GetAttributeSize(QTSS_Object inObject, QTSS_AttributeID inID, uint32_t inIndex, uint32_t* outLenPtr);
	char*           NewIndexElement(QTSS_Object inObject, QTSS_AttributeID inID, uint32_t inIndex);
	uint32_t          GetNumFields() { return fNumFields; };
	void            SetNumFields(uint32_t numFields) { fNumFields = numFields; fDataFieldsStop = numFields; };

	ElementDataFields*  GetFields() { return fFieldIDs; };
	void                SetFields(ElementDataFields *fieldsPtr) { fFieldIDs = fieldsPtr; };
	void                SetFieldsType(DataFieldsType fDataFieldsType) { this->fDataFieldsType = fDataFieldsType; };

	static void GetFilteredAttributeName(ElementDataFields* fieldPtr, QTSS_AttributeID theID);
	static bool GetFilteredAttributeID(char *parentName, char *nodeName, QTSS_AttributeID* foundID);
	static bool IsPreferenceContainer(char *nodeName, QTSS_AttributeID* foundID);

	enum { kmaxPathlen = 1048 };
	char                fPathBuffer[kmaxPathlen];
	StrPtrLen           fPathSPL;
	StrPtrLen           fNodeNameSPL;

	QTSS_Object         fDataSource;
	int32_t              fNumFields;
	int32_t              fPathLen;
	bool              fInitialized;

	ElementDataFields*  fFieldIDs;
	ElementDataFields*  fSelfPtr;
	DataFieldsType      fDataFieldsType;
	char*               fSelfDataPtr;


	char**              fFieldDataPtrs;
	OSRef**             fFieldOSRefPtrs;
	ElementNode*        fParentNodePtr;
	OSRefTable*         fElementMap;

	bool              fIsTop;

private:

	inline void DebugShowFieldDataType(int32_t index);
	inline void DebugShowFieldValue(int32_t index);


};

class AdminClass : public ElementNode
{
public:
	QueryURI *fQueryPtr;
	ElementNode *fNodePtr;

	void SetUpSingleElement(QueryURI *queryPtr, StrPtrLen *currentSegmentPtr, StrPtrLen *nextSegmentPtr, int32_t index, QTSS_Initialize_Params *initParams);
	void SetUpSingleNode(QueryURI *queryPtr, StrPtrLen *currentSegmentPtr, StrPtrLen *nextSegmentPtr, int32_t index, QTSS_Initialize_Params *initParams);
	void Initialize(QTSS_Initialize_Params *initParams, QueryURI *queryPtr);
	AdminClass() :fQueryPtr(NULL), fNodePtr(NULL) {};
	~AdminClass();
	static ElementNode::ElementDataFields sAdminSelf[];
	static ElementNode::ElementDataFields sAdminFieldIDs[];
	enum
	{
		eServer = 0,
		eNumAttributes
	};
};




#endif // _ADMINELEMENTNODE_H_
