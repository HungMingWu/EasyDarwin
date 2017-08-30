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
    File:       QTSSModuleUtils.cpp

    Contains:   Implements utility routines defined in QTSSModuleUtils.h.
                    
*/

#include <cstdint>
#include <memory>
#include <boost/algorithm/string/predicate.hpp>
#include "QTSSModuleUtils.h"
#include "QTSSDictionary.h"
#include "StrPtrLen.h"
#include "MyAssert.h"
#include "StringFormatter.h"
#include "ResizeableStringFormatter.h"
#include "StringParser.h"
#include "RTPSession.h"
#include "QTSSDataConverter.h"
#include "RTSPRequest.h"
#include "QTSSUserProfile.h"
#ifndef __Win32__
#include <netinet/in.h>
#endif

#ifdef __solaris__
#include <limits.h>
#endif
#include "sdpCache.h"


QTSServerInterface*         QTSSModuleUtils::sServer = nullptr;

void    QTSSModuleUtils::Initialize(QTSServerInterface* inServer)
{
    sServer = inServer;
}

QTSS_Error QTSSModuleUtils::ReadEntireFile(char* inPath, StrPtrLen* outData, QTSS_TimeVal inModDate, QTSS_TimeVal* outModDate)
	{	
#if 0    
		QTSS_Object theFileObject = NULL;
#endif
		QTSS_Error theErr = QTSS_NoErr;
		
		outData->Ptr = nullptr;
		outData->Len = 0;
		
		do { 
   #if 0     
			// Use the QTSS file system API to read the file
			theErr = QTSS_OpenFileObject(inPath, 0, &theFileObject);
			if (theErr != QTSS_NoErr)
				break;
   #endif
			uint32_t theParamLen = 8;
			QTSS_TimeVal* theModDate = nullptr;
			unsigned long long date = 0;
			//theErr = QTSS_GetValuePtr(theFileObject, qtssFlObjModDate, 0, (void**)&theModDate, &theParamLen);
			date = CSdpCache::GetInstance()->getSdpCacheDate(inPath);
			theModDate = (QTSS_TimeVal*)&date;
			Assert(theParamLen == sizeof(QTSS_TimeVal));
			if(theParamLen != sizeof(QTSS_TimeVal))
				break;
			if(outModDate != nullptr)
				*outModDate = (QTSS_TimeVal)*theModDate;
	
			if(inModDate != -1) {	
				// If file hasn't been modified since inModDate, don't have to read the file
				if(*theModDate <= inModDate)
					break;
			}
			
			theParamLen = 8;
			uint64_t* theLength = nullptr;
			uint64_t sdpLen = 0;
			//theErr = QTSS_GetValuePtr(theFileObject, qtssFlObjLength, 0, (void**)&theLength, &theParamLen);
			char *sdpContext = CSdpCache::GetInstance()->getSdpMap(inPath);
			if(sdpContext == nullptr)
			{
				theErr = QTSS_RequestFailed;
			}
			else
			{
				sdpLen = strlen(sdpContext);
			}
			
			theLength = &sdpLen;
			
			if (theParamLen != sizeof(uint64_t))
				break;
			
			if (*theLength > INT32_MAX)
				break;
	
			// Allocate memory for the file data
			outData->Ptr = new char[ (int32_t) (*theLength + 1) ];
			outData->Len = (int32_t) *theLength;
			outData->Ptr[outData->Len] = 0;
		
			// Read the data
			uint32_t recvLen = 0;
			if(sdpContext)
			{
				recvLen = *theLength;
				memcpy(outData->Ptr,sdpContext,*theLength);
			}
			
			if (theErr != QTSS_NoErr)
			{
				outData->Delete();
				break;
			}	
			Assert(outData->Len == recvLen);
		
		}while(false);
	
#if 0    
		// Close the file
		if(theFileObject != NULL) {
			theErr = QTSS_CloseFileObject(theFileObject);
		}
#endif    
		return theErr;
	}
          
QTSS_Error  QTSSModuleUtils::AppendRTPMetaInfoHeader(   RTSPRequest* inRequest,
                                                        StrPtrLen* inRTPMetaInfoHeader,
                                                        RTPMetaInfoPacket::FieldID* inFieldIDArray)
{
    //
    // For formatting the response header
    char tempBuffer[128];
    ResizeableStringFormatter theFormatter(tempBuffer, 128);
    
    StrPtrLen theHeader(*inRTPMetaInfoHeader);
    
    //
    // For marking which fields were requested by the client
    bool foundFieldArray[RTPMetaInfoPacket::kNumFields];
    ::memset(foundFieldArray, 0, sizeof(bool) * RTPMetaInfoPacket::kNumFields);
    
    char* theEndP = theHeader.Ptr + theHeader.Len;
    uint16_t fieldNameValue = 0;
    
    while (theHeader.Ptr <= (theEndP - sizeof(RTPMetaInfoPacket::FieldName)))
    {
        auto* theFieldName = (RTPMetaInfoPacket::FieldName*)theHeader.Ptr;
        ::memcpy (&fieldNameValue, theFieldName, sizeof(uint16_t));

        RTPMetaInfoPacket::FieldIndex theFieldIndex = RTPMetaInfoPacket::GetFieldIndexForName(ntohs(fieldNameValue));
        
        //
        // This field is not supported (not in the field ID array), so
        // don't put it in the response
        if ((theFieldIndex == RTPMetaInfoPacket::kIllegalField) ||
            (inFieldIDArray[theFieldIndex] == RTPMetaInfoPacket::kFieldNotUsed))
        {
            theHeader.Ptr += 3;
            continue;
        }
        
        //
        // Mark that this field has been requested by the client
        foundFieldArray[theFieldIndex] = true;
        
        //
        // This field is good to go... put it in the response   
        theFormatter.Put(theHeader.Ptr, sizeof(RTPMetaInfoPacket::FieldName));
        
        if (inFieldIDArray[theFieldIndex] != RTPMetaInfoPacket::kUncompressed)
        {
            //
            // If the caller wants this field to be compressed (there
            // is an ID associated with the field), put the ID in the response
            theFormatter.PutChar('=');
            theFormatter.Put(inFieldIDArray[theFieldIndex]);
        }
        
        //
        // Field separator
        theFormatter.PutChar(';');
            
        //
        // Skip onto the next field name in the header
        theHeader.Ptr += 3;
    }

    //
    // Go through the caller's FieldID array, and turn off the fields
    // that were not requested by the client.
    for (uint32_t x = 0; x < RTPMetaInfoPacket::kNumFields; x++)
    {
        if (!foundFieldArray[x])
            inFieldIDArray[x] = RTPMetaInfoPacket::kFieldNotUsed;
    }
    
    //
    // No intersection between requested headers and supported headers!
    if (theFormatter.GetCurrentOffset() == 0)
        return QTSS_ValueNotFound; // Not really the greatest error!
        
    //
    // When appending the header to the response, strip off the last ';'.
    // It's not needed.
    inRequest->AppendHeader(qtssXRTPMetaInfoHeader, 
		boost::string_view(theFormatter.GetBufPtr(), theFormatter.GetCurrentOffset() - 1));
	return QTSS_NoErr;
}

QTSS_Error  QTSSModuleUtils::SendErrorResponse( RTSPRequest* inRequest,
                                                QTSS_RTSPStatusCode inStatusCode)
{
    static bool sFalse = false;
    
    //set RTSP headers necessary for this error response message
	inRequest->SetStatus(inStatusCode);
	inRequest->SetResponseKeepAlive(sFalse);
    StringFormatter theErrorMsgFormatter(nullptr, 0);
       
    //send the response header. In all situations where errors could happen, we
    //don't really care, cause there's nothing we can do anyway!
	inRequest->SendHeader();

    //
    // Now that we've formatted the message into the temporary buffer,
    // write it out to the request stream and the Client Session object
	inRequest->Write(theErrorMsgFormatter.GetBufPtr(), theErrorMsgFormatter.GetBytesWritten(), nullptr, 0);
	inRequest->SetRespMsg({ theErrorMsgFormatter.GetBufPtr(), theErrorMsgFormatter.GetBytesWritten() });
    
    return QTSS_RequestFailed;
}

QTSS_Error	QTSSModuleUtils::SendErrorResponseWithMessage( RTSPRequest* inRequest,
														QTSS_RTSPStatusCode inStatusCode)
{
    static bool sFalse = false;
    
	//set RTSP headers necessary for this error response message
	inRequest->SetStatus(inStatusCode);
	inRequest->SetResponseKeepAlive(sFalse);  
    //send the response header. In all situations where errors could happen, we
    //don't really care, cause there's nothing we can do anyway!
	inRequest->SendHeader();
    
    return QTSS_RequestFailed;
}

static void ReqSendDescribeResponse(RTSPRequest *inRequest)
{
	if (inRequest->GetStatus() == qtssRedirectNotModified)
	{
		(void)inRequest->SendHeader();
		return;
	}

	// write date and expires
	inRequest->AppendDateAndExpires();

	//write content type header
	static boost::string_view sContentType("application/sdp");
	inRequest->AppendHeader(qtssContentTypeHeader, sContentType);

	// write x-Accept-Retransmit header
	static boost::string_view sRetransmitProtocolName("our-retransmit");
	inRequest->AppendHeader(qtssXAcceptRetransmitHeader, sRetransmitProtocolName);

	// write x-Accept-Dynamic-Rate header
	static boost::string_view dynamicRateEnabledStr("1");
	inRequest->AppendHeader(qtssXAcceptDynamicRateHeader, dynamicRateEnabledStr);

	//write content base header

	inRequest->AppendContentBaseHeader(inRequest->GetAbsoluteURL());

	//I believe the only error that can happen is if the client has disconnected.
	//if that's the case, just ignore it, hopefully the calling module will detect
	//this and return control back to the server ASAP 
	inRequest->SendHeader();
}

void    QTSSModuleUtils::SendDescribeResponse(RTSPRequest* inRequest,
                                                    iovec* describeData,
                                                    uint32_t inNumVectors,
                                                    uint32_t inTotalLength)
{
    //write content size header
	inRequest->AppendHeader(qtssContentLengthHeader, std::to_string(inTotalLength));

	ReqSendDescribeResponse(inRequest);

        // On solaris, the maximum # of vectors is very low (= 16) so to ensure that we are still able to
        // send the SDP if we have a number greater than the maximum allowed, we coalesce the vectors into
        // a single big buffer
	inRequest->WriteV(describeData, inNumVectors, inTotalLength, nullptr);

}

boost::string_view QTSSModuleUtils::GetUserName(QTSSUserProfile* inUserProfile)
{
	return inUserProfile->GetUserName();
}

bool QTSSModuleUtils::UserInGroup(QTSSUserProfile* inUserProfile, boost::string_view inGroup)
{
	if (nullptr == inUserProfile || inGroup.empty()) 
		return false;
		
	boost::string_view userName = inUserProfile->GetUserName();
	if (userName.empty()) // no user to check
		return false;

	std::vector<std::string> userGroups = inUserProfile->GetUserGroups();

	if (userGroups.empty()) // no groups to check
		return false;
	
	for (const auto &item : userGroups)
		if (boost::equals(item, inGroup))
			return true;

	return false;
}


bool QTSSModuleUtils::AddressInList(QTSS_Object inObject, QTSS_AttributeID listID, StrPtrLen *inAddressPtr)
{
    StrPtrLenDel strDeleter;
    char*   theAttributeString = nullptr;    
    IPComponentStr inAddress(inAddressPtr);
    IPComponentStr addressFromList;
    
    if (!inAddress.Valid())
        return false;

    uint32_t numValues = ((QTSSDictionary*)inObject)->GetNumValues(listID);
    
    for (uint32_t index = 0; index < numValues; index ++)
    { 
        strDeleter.Delete();
        (void)((QTSSDictionary*)inObject)->GetValueAsString(listID, index, &theAttributeString);
        strDeleter.Set(theAttributeString);
 
        addressFromList.Set(&strDeleter);
        if (addressFromList.Equal(&inAddress))
            return true;
    }

    return false;
}

bool QTSSModuleUtils::FindStringInAttributeList(QTSS_Object inObject, QTSS_AttributeID listID, StrPtrLen *inStrPtr)
{
    StrPtrLenDel tempString;
     
    if (nullptr == inStrPtr || nullptr == inStrPtr->Ptr || 0 == inStrPtr->Len)
        return false;

    uint32_t numValues = ((QTSSDictionary*)inObject)->GetNumValues(listID);
    
    for (uint32_t index = 0; index < numValues; index ++)
    { 
        tempString.Delete();
        (void)((QTSSDictionary*)inObject)->GetValueAsString(listID, index, &tempString.Ptr);
        tempString.Set(tempString.Ptr);
		if (tempString.Ptr == nullptr)
			return false;
			
        if (tempString.Equal(StrPtrLen("*",1)))
            return true;
        
        if (inStrPtr->FindString(tempString.Ptr))
            return true;
            
   }

    return false;
}

bool QTSSModuleUtils::HavePlayerProfile(QTSS_PrefsObject inPrefObjectToCheck, QTSS_StandardRTSP_Params* inParams, uint32_t feature)
{
    std::string userAgentStrV(inParams->inClientSession->GetUserAgent());
    StrPtrLen userAgentStr((char *)userAgentStrV.c_str());
    
    switch (feature)
    {
        case QTSSModuleUtils::kRequiresRTPInfoSeqAndTime:
        {        
            return QTSSModuleUtils::FindStringInAttributeList(inPrefObjectToCheck,  qtssPrefsPlayersReqRTPHeader, &userAgentStr);
        }
        break;
        
        case QTSSModuleUtils::kAdjustBandwidth:
        {
            return QTSSModuleUtils::FindStringInAttributeList(inPrefObjectToCheck,  qtssPrefsPlayersReqBandAdjust, &userAgentStr);
        }
        break;
		
        case QTSSModuleUtils::kDisablePauseAdjustedRTPTime:
        {
            return QTSSModuleUtils::FindStringInAttributeList(inPrefObjectToCheck,  qtssPrefsPlayersReqNoPauseTimeAdjust, &userAgentStr);
        }
        break;
        
        case QTSSModuleUtils::kDelayRTPStreamsUntilAfterRTSPResponse:
        {
            return QTSSModuleUtils::FindStringInAttributeList(inPrefObjectToCheck,  qtssPrefsPlayersReqRTPStartTimeAdjust, &userAgentStr);
        }
        break;        
    }
    
    return false;
}


QTSS_Error QTSSModuleUtils::AuthorizeRequest(RTSPRequest* theRTSPRequest, bool allowed, bool foundUser, bool authContinue)
{
	theRTSPRequest->SetUserAllow(allowed);
	theRTSPRequest->SetUserFound(foundUser);
	theRTSPRequest->SetAuthHandle(authContinue);
        
    return QTSS_NoErr;
}





IPComponentStr IPComponentStr::sLocalIPCompStr("127.0.0.*");

IPComponentStr::IPComponentStr(char *theAddressPtr)
{
    StrPtrLen sourceStr(theAddressPtr);
     (void) this->Set(&sourceStr);    
}

IPComponentStr::IPComponentStr(StrPtrLen *sourceStrPtr)
{
    (void) this->Set(sourceStrPtr);    
}

bool IPComponentStr::Set(StrPtrLen *theAddressStrPtr)
{
    fIsValid = false;
   
    StringParser IP_Paser(theAddressStrPtr);
    StrPtrLen *piecePtr = &fAddressComponent[0];

    while (IP_Paser.GetDataRemaining() > 0) 
    {
        IP_Paser.ConsumeUntil(piecePtr,'.');    
        if (piecePtr->Len == 0) 
            break;
        
        IP_Paser.ConsumeLength(nullptr, 1);
        if (piecePtr == &fAddressComponent[IPComponentStr::kNumComponents -1])
        {
           fIsValid = true;
           break;
        }
        
        piecePtr++;
    };
     
    return fIsValid;
}


bool IPComponentStr::Equal(IPComponentStr *testAddressPtr)
{
    if (testAddressPtr == nullptr) 
        return false;
    
    if ( !this->Valid() || !testAddressPtr->Valid() )
        return false;

    for (uint16_t component= 0 ; component < IPComponentStr::kNumComponents ; component ++)
    {
        StrPtrLen *allowedPtr = this->GetComponent(component);
        StrPtrLen *testPtr = testAddressPtr->GetComponent(component);
                       
        if ( testPtr->Equal("*") || allowedPtr->Equal("*") )
            continue;
            
        if (!testPtr->Equal(*allowedPtr) ) 
            return false; 
    };  
    
    return true;
}


