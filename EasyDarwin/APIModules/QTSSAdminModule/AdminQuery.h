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
#ifndef _ADMINQUERY_H_
#define _ADMINQUERY_H_

#ifndef __Win32__
#include <unistd.h>     /* for getopt() et al */
#endif

#include "QTSSAdminModule.h"
#include "StringParser.h"
#include "StrPtrLen.h"

 /*
 r = recurse -> walk downward in hierarchy
 v = verbose -> return full path in name
 a = access -> return read/write access
 t = type -> return type of value ** perhaps better not to support
 f = filter -> return filter
 p = path -> return path
 */


class QueryURI
{
public:

	enum { eMaxAttributeSize = 60, eMaxBufferSize = 2048 };
	struct URIField
	{
		char                    fFieldName[eMaxAttributeSize + 1];
		UInt32                  fFieldLen;
		SInt32                  fID;
		StrPtrLen*              fData;
	};

	enum
	{
		eModuleID = 0,
		eRootID = 1,
		eURL = 2,
		eQuery = 3,
		eParameters = 4,
		eSnapshot = 5,
		eCommand = 6,
		eValue = 7,
		eType = 8,
		eAccess = 9,
		eName = 10,

		eFilter1,
		eFilter2,
		eFilter3,
		eFilter4,
		eFilter5,
		eFilter6,
		eFilter7,
		eFilter8,
		eFilter9,
		eFilter10,

		eNumAttributes
	};

	enum //commands
	{
		kGETCommand = 0,
		kSETCommand = 1,
		kADDCommand = 2,
		kDELCommand = 3,
		kLastCommand = 4
	};

	enum
	{
		kRecurseParam = 1 << 0,
		kVerboseParam = 1 << 1,
		kAccessParam = 1 << 2,
		kTypeParam = 1 << 3,
		kFilterParam = 1 << 4,
		kPathParam = 1 << 5,
		kDebugParam = 1 << 6,
		kIndexParam = 1 << 7
	};

	static uint8_t sNotQueryData[];
	static uint8_t sWhiteQuoteOrEOL[];
	static uint8_t sWhitespaceOrQuoteMask[];

	static URIField sURIFields[];
	static char *sCommandDefs[];

	URIField *fURIFieldsPtr;

	void URLParse(StrPtrLen *inStream);

	void SetQueryData() { if (fIsAdminQuery) { SetSnapShot(); SetParamBits(0); SetCommand(); SetAccessFlags(); } }

	StrPtrLen*  GetModuleID() { return    fURIFieldsPtr[eModuleID].fData; };
	StrPtrLen*  GetRootID() { return    fURIFieldsPtr[eRootID].fData; };
	StrPtrLen*  GetURL() { return    fURIFieldsPtr[eURL].fData; };
	StrPtrLen*  GetQuery() { return    fURIFieldsPtr[eQuery].fData; };
	StrPtrLen*  GetParameters() { return    fURIFieldsPtr[eParameters].fData; };
	StrPtrLen*  GetSnapshot() { return    fURIFieldsPtr[eSnapshot].fData; };

	StrPtrLen*  GetCommand() { return    fURIFieldsPtr[eCommand].fData; };
	StrPtrLen*  GetValue() { return    fURIFieldsPtr[eValue].fData; };
	StrPtrLen*  GetType() { return    fURIFieldsPtr[eType].fData; };
	StrPtrLen*  GetAccess() { return    fURIFieldsPtr[eAccess].fData; };
	StrPtrLen*  GetName() { return    fURIFieldsPtr[eName].fData; };
	StrPtrLen*  GetFilter(UInt32 index) { return fURIFieldsPtr[eFilter1 + index].fData; };

	StrPtrLen*  GetEvalMsg() { return    &fQueryEvalMessage; };

	UInt32      GetAccessFlags() { return    fAccessFlags; };
	UInt32      GetSnapshotID() { return    fSnapshotID; };
	UInt32      GetParamBits() { return    fParamBits; };
	bool      IsAdminQuery() { return    fIsAdminQuery; };
	bool      UseSnapShot() { return    fUseSnapShot; };

	bool      RecurseParam() { return    (bool)((fParamBits & kRecurseParam) != 0); };
	bool      VerboseParam() { return    (bool)((fParamBits & kVerboseParam) != 0); };
	bool      AccessParam() { return    (bool)((fParamBits & kAccessParam) != 0); };
	bool      TypeParam() { return    (bool)((fParamBits & kTypeParam) != 0); };
	bool      FilterParam() { return    (bool)((fParamBits & kFilterParam) != 0); };
	bool      PathParam() { return    (bool)((fParamBits & kPathParam) != 0); };
	bool      DebugParam() { return    (bool)((fParamBits & kDebugParam) != 0); };
	bool      IndexParam() { return    (bool)((fParamBits & kIndexParam) != 0); };
	void        SetQueryHasResponse() { fQueryHasResponse = true; };
	bool      QueryHasReponse() { if (fQueryEvalResult > 0) return true; else return fQueryHasResponse; };
	UInt32      GetEvaluResult() { return fQueryEvalResult; };
	StrPtrLen*  NextSegment(StrPtrLen *currentPathPtr, StrPtrLen *outNextPtr);
	void        SetAccessFlags();
	void        SetParamBits(UInt32 forcebits);
	void        SetSnapShot();
	SInt32      GetCommandID() { return    fTheCommand; };

	char        fLastPath[1024];
	QueryURI(StrPtrLen *inStream);
	~QueryURI();
	UInt32      EvalQuery(UInt32 *forceResultPtr, char *forceMessagePtr);

	char        fQueryMessageBuff[1024];
	bool      fIsPref;
	SInt16      fNumFilters;
	bool      fHasQuery;
private:
	char        fURIBuffer[QueryURI::eMaxBufferSize];
	char        fQueryBuffer[QueryURI::eMaxBufferSize];
	StrPtrLen   fURIFieldSPL[QueryURI::eNumAttributes];
	StrPtrLen   fAdminFullURI;
	UInt32      fParamBits;
	UInt32      fSnapshotID;
	UInt32      fAccessFlags;
	bool      fIsAdminQuery;
	bool      fUseSnapShot;
	StrPtrLen   fCurrentPath;
	StrPtrLen   fNext;
	bool      fQueryHasResponse;
	UInt32      fQueryEvalResult;
	StrPtrLen   fQueryEvalMessage;
	SInt32      fTheCommand;
	void        SetCommand();
	void        ParseURLString(StringParser *parserPtr, StrPtrLen *urlPtr);
	void        ParseQueryString(StringParser *parserPtr, StrPtrLen *urlPtr);

	UInt32      CheckInvalidIterator(char* evalMessageBuff);
	UInt32      CheckInvalidArrayIterator(char* evalMessageBuff);
	UInt32      CheckInvalidRecurseParam(char* evalMessageBuff);
};


#endif // _ADMINQUERY_H_ 
