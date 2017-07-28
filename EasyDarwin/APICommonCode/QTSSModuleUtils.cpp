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
#include "QTSSModuleUtils.h"
#include "QTSS_Private.h"
#include "QTSSDictionary.h"
#include "StrPtrLen.h"
#include "MyAssert.h"
#include "StringFormatter.h"
#include "ResizeableStringFormatter.h"
#include "QTAccessFile.h"
#include "StringParser.h"
#include "RTPSession.h"
#include "QTSSDataConverter.h"
#ifndef __Win32__
#include <netinet/in.h>
#endif

#ifdef __solaris__
#include <limits.h>
#endif
#include "sdpCache.h"


QTSS_TextMessagesObject     QTSSModuleUtils::sMessages = nullptr;
QTSS_ServerObject           QTSSModuleUtils::sServer = nullptr;
QTSS_StreamRef              QTSSModuleUtils::sErrorLog = nullptr;
bool                      QTSSModuleUtils::sEnableRTSPErrorMsg = false;
QTSS_ErrorVerbosity         QTSSModuleUtils::sMissingPrefVerbosity = qtssMessageVerbosity;

void    QTSSModuleUtils::Initialize(QTSS_TextMessagesObject inMessages,
                                    QTSS_ServerObject inServer,
                                    QTSS_StreamRef inErrorLog)
{
    sMessages = inMessages;
    sServer = inServer;
    sErrorLog = inErrorLog;
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
				// theErr = QTSS_Read(theFileObject, outData->Ptr, outData->Len, &recvLen);
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


void    QTSSModuleUtils::SetupSupportedMethods(QTSS_Object inServer, QTSS_RTSPMethod* inMethodArray, uint32_t inNumMethods)
{
    // Report to the server that this module handles DESCRIBE, SETUP, PLAY, PAUSE, and TEARDOWN
    uint32_t theNumMethods = 0;
    (void)QTSS_GetNumValues(inServer, qtssSvrHandledMethods, &theNumMethods);
    
    for (uint32_t x = 0; x < inNumMethods; x++)
        (void)QTSS_SetValue(inServer, qtssSvrHandledMethods, theNumMethods++, (void*)&inMethodArray[x], sizeof(inMethodArray[x]));
}

void    QTSSModuleUtils::LogError(  QTSS_ErrorVerbosity inVerbosity,
                                    QTSS_AttributeID inTextMessage,
                                    uint32_t /*inErrNumber*/,
                                    char* inArgument,
                                    char* inArg2)
{
    static char* sEmptyArg = "";
    
    if (sMessages == nullptr)
        return;
        
    // Retrieve the specified text message from the text messages dictionary.
    
    StrPtrLen theMessage;
    ((QTSSDictionary*)sMessages)->GetValuePtr(inTextMessage, 0, (void**)&theMessage.Ptr, &theMessage.Len);
    if ((theMessage.Ptr == nullptr) || (theMessage.Len == 0))
		((QTSSDictionary*)sMessages)->GetValuePtr(qtssMsgNoMessage, 0, (void**)&theMessage.Ptr, &theMessage.Len);

    if ((theMessage.Ptr == nullptr) || (theMessage.Len == 0))
        return;
    
    // sprintf and ::strlen will crash if inArgument is NULL
    if (inArgument == nullptr)
        inArgument = sEmptyArg;
    if (inArg2 == nullptr)
        inArg2 = sEmptyArg;
    
    // Create a new string, and put the argument into the new string.
    
    uint32_t theMessageLen = theMessage.Len + ::strlen(inArgument) + ::strlen(inArg2);

    std::unique_ptr<char[]> theLogString(new char[theMessageLen + 1]);
    sprintf(theLogString.get(), theMessage.Ptr, inArgument, inArg2);
    
    (void)QTSS_Write(sErrorLog, theLogString.get(), ::strlen(theLogString.get()),
                        nullptr, inVerbosity);
}

void QTSSModuleUtils::LogErrorStr( QTSS_ErrorVerbosity inVerbosity, char* inMessage) 
{  	
	if (inMessage == nullptr) return;  
	(void)QTSS_Write(sErrorLog, inMessage, ::strlen(inMessage), nullptr, inVerbosity);
}


void QTSSModuleUtils::LogPrefErrorStr( QTSS_ErrorVerbosity inVerbosity, char*  preference, char* inMessage)
{  	
	if (inMessage == nullptr || preference == nullptr) 
	{  Assert(0);
	   return;  
	}
	char buffer[1024];
	
	snprintf(buffer,sizeof(buffer), "Server preference %s %s",  preference, inMessage);
   
	(void)QTSS_Write(sErrorLog, buffer, ::strlen(buffer), nullptr, inVerbosity);
}

                        
char* QTSSModuleUtils::GetFullPath( QTSS_RTSPRequestObject inRequest,
                                    QTSS_AttributeID whichFileType,
                                    uint32_t* outLen,
                                    StrPtrLen* suffix)
{
    Assert(outLen != nullptr);
    
	(void)QTSS_LockObject(inRequest);
    // Get the proper file path attribute. This may return an error if
    // the file type is qtssFilePathTrunc attr, because there may be no path
    // once its truncated. That's ok. In that case, we just won't append a path.
    StrPtrLen theFilePath;
	((QTSSDictionary*)inRequest)->GetValuePtr(whichFileType, 0, (void**)&theFilePath.Ptr, &theFilePath.Len);
		
    StrPtrLen theRootDir;
    QTSS_Error theErr = ((QTSSDictionary*)inRequest)->GetValuePtr(qtssRTSPReqRootDir, 0, (void**)&theRootDir.Ptr, &theRootDir.Len);
	Assert(theErr == QTSS_NoErr);


    //trim off extra / characters before concatenating
    // so root/ + /path instead of becoming root//path  is now root/path  as it should be.
    
	if (theRootDir.Len && theRootDir.Ptr[theRootDir.Len -1] == kPathDelimiterChar
	    && theFilePath.Len  && theFilePath.Ptr[0] == kPathDelimiterChar)
	{
	    char *thePathEnd = &(theFilePath.Ptr[theFilePath.Len]);
	    while (theFilePath.Ptr != thePathEnd)
	    {
	        if (*theFilePath.Ptr != kPathDelimiterChar)
	            break;
	            
	        theFilePath.Ptr ++;
	        theFilePath.Len --;
	    }
	}

    //construct a full path out of the root dir path for this request,
    //and the url path.
    *outLen = theFilePath.Len + theRootDir.Len + 2;
    if (suffix != nullptr)
        *outLen += suffix->Len;
    
    auto* theFullPath = new char[*outLen];
    
    //write all the pieces of the path into this new buffer.
    StringFormatter thePathFormatter(theFullPath, *outLen);
    thePathFormatter.Put(theRootDir);
    thePathFormatter.Put(theFilePath);
    if (suffix != nullptr)
        thePathFormatter.Put(*suffix);
    thePathFormatter.PutTerminator();

    *outLen = *outLen - 2;
	
	(void)QTSS_UnlockObject(inRequest);
	
    return theFullPath;
}

QTSS_Error  QTSSModuleUtils::AppendRTPMetaInfoHeader(   QTSS_RTSPRequestObject inRequest,
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
    return QTSS_AppendRTSPHeader(inRequest, qtssXRTPMetaInfoHeader, theFormatter.GetBufPtr(), theFormatter.GetCurrentOffset() - 1);
}

QTSS_Error  QTSSModuleUtils::SendErrorResponse( QTSS_RTSPRequestObject inRequest,
                                                        QTSS_RTSPStatusCode inStatusCode,
                                                        QTSS_AttributeID inTextMessage,
                                                        StrPtrLen* inStringArg)
{
    static bool sFalse = false;
    
    //set RTSP headers necessary for this error response message
    (void)QTSS_SetValue(inRequest, qtssRTSPReqStatusCode, 0, &inStatusCode, sizeof(inStatusCode));
    (void)QTSS_SetValue(inRequest, qtssRTSPReqRespKeepAlive, 0, &sFalse, sizeof(sFalse));
    StringFormatter theErrorMsgFormatter(nullptr, 0);
    char *messageBuffPtr = nullptr;
    
    if (sEnableRTSPErrorMsg)
    {
        // Retrieve the specified message out of the text messages dictionary.
        StrPtrLen theMessage;
		((QTSSDictionary*)sMessages)->GetValuePtr(inTextMessage, 0, (void**)&theMessage.Ptr, &theMessage.Len);

        if ((theMessage.Ptr == nullptr) || (theMessage.Len == 0))
        {
            // If we couldn't find the specified message, get the default
            // "No Message" message, and return that to the client instead.
            
			((QTSSDictionary*)sMessages)->GetValuePtr(qtssMsgNoMessage, 0, (void**)&theMessage.Ptr, &theMessage.Len);
        }
        Assert(theMessage.Ptr != nullptr);
        Assert(theMessage.Len > 0);
        
        // Allocate a temporary buffer for the error message, and format the error message
        // into that buffer
        uint32_t theMsgLen = 256;
        if (inStringArg != nullptr)
            theMsgLen += inStringArg->Len;
        
        messageBuffPtr = new char[theMsgLen];
        messageBuffPtr[0] = 0;
        theErrorMsgFormatter.Set(messageBuffPtr, theMsgLen);
        //
        // Look for a %s in the string, and if one exists, replace it with the
        // argument passed into this function.
        
        //we can safely assume that message is in fact NULL terminated
        char* stringLocation = ::strstr(theMessage.Ptr, "%s");
        if (stringLocation != nullptr)
        {
            //write first chunk
            theErrorMsgFormatter.Put(theMessage.Ptr, stringLocation - theMessage.Ptr);
            
            if (inStringArg != nullptr && inStringArg->Len > 0)
            {
                //write string arg if it exists
                theErrorMsgFormatter.Put(inStringArg->Ptr, inStringArg->Len);
                stringLocation += 2;
            }
            //write last chunk
            theErrorMsgFormatter.Put(stringLocation, (theMessage.Ptr + theMessage.Len) - stringLocation);
        }
        else
            theErrorMsgFormatter.Put(theMessage);
        
        
        char buff[32];
        sprintf(buff,"%"   _U32BITARG_   "",theErrorMsgFormatter.GetBytesWritten());
        (void)QTSS_AppendRTSPHeader(inRequest, qtssContentLengthHeader, buff, ::strlen(buff));
    }
    
    //send the response header. In all situations where errors could happen, we
    //don't really care, cause there's nothing we can do anyway!
    (void)QTSS_SendRTSPHeaders(inRequest);

    //
    // Now that we've formatted the message into the temporary buffer,
    // write it out to the request stream and the Client Session object
    (void)QTSS_Write(inRequest, theErrorMsgFormatter.GetBufPtr(), theErrorMsgFormatter.GetBytesWritten(), nullptr, 0);
    (void)QTSS_SetValue(inRequest, qtssRTSPReqRespMsg, 0, theErrorMsgFormatter.GetBufPtr(), theErrorMsgFormatter.GetBytesWritten());
    
    delete [] messageBuffPtr;
    return QTSS_RequestFailed;
}

QTSS_Error	QTSSModuleUtils::SendErrorResponseWithMessage( QTSS_RTSPRequestObject inRequest,
														QTSS_RTSPStatusCode inStatusCode,
														StrPtrLen* inErrorMessagePtr)
{
    static bool sFalse = false;
    
    //set RTSP headers necessary for this error response message
    (void)QTSS_SetValue(inRequest, qtssRTSPReqStatusCode, 0, &inStatusCode, sizeof(inStatusCode));
    (void)QTSS_SetValue(inRequest, qtssRTSPReqRespKeepAlive, 0, &sFalse, sizeof(sFalse));
    StrPtrLen theErrorMessage(nullptr, 0);
    
    if (sEnableRTSPErrorMsg)
    {
		Assert(inErrorMessagePtr != nullptr);
		//Assert(inErrorMessagePtr->Ptr != NULL);
		//Assert(inErrorMessagePtr->Len != 0);
		theErrorMessage.Set(inErrorMessagePtr->Ptr, inErrorMessagePtr->Len);
		
        char buff[32];
        sprintf(buff,"%"   _U32BITARG_   "",inErrorMessagePtr->Len);
        (void)QTSS_AppendRTSPHeader(inRequest, qtssContentLengthHeader, buff, ::strlen(buff));
    }
    
    //send the response header. In all situations where errors could happen, we
    //don't really care, cause there's nothing we can do anyway!
    (void)QTSS_SendRTSPHeaders(inRequest);

    //
    // Now that we've formatted the message into the temporary buffer,
    // write it out to the request stream and the Client Session object
    (void)QTSS_Write(inRequest, theErrorMessage.Ptr, theErrorMessage.Len, nullptr, 0);
    (void)QTSS_SetValue(inRequest, qtssRTSPReqRespMsg, 0, theErrorMessage.Ptr, theErrorMessage.Len);
    
    return QTSS_RequestFailed;
}


QTSS_Error	QTSSModuleUtils::SendHTTPErrorResponse( QTSS_RTSPRequestObject inRequest,
													QTSS_SessionStatusCode inStatusCode,
                                                    bool inKillSession,
                                                    char *errorMessage)
{
    static bool sFalse = false;
    
    //set status code for access log
    (void)QTSS_SetValue(inRequest, qtssRTSPReqStatusCode, 0, &inStatusCode, sizeof(inStatusCode));

    if (inKillSession) // tell the server to end the session
        (void)QTSS_SetValue(inRequest, qtssRTSPReqRespKeepAlive, 0, &sFalse, sizeof(sFalse));
    
    ResizeableStringFormatter theErrorMessage(nullptr, 0); //allocates and deletes memory
    ResizeableStringFormatter bodyMessage(nullptr,0); //allocates and deletes memory

    char messageLineBuffer[64]; // used for each line
    static const int maxMessageBufferChars = sizeof(messageLineBuffer) -1;
    messageLineBuffer[maxMessageBufferChars] = 0; // guarantee termination

    // ToDo: put in a more meaningful http error message for each error. Not required by spec.
    // ToDo: maybe use the HTTP protcol class static error strings.
    char* errorMsg = "error"; 

    DateBuffer theDate;
    DateTranslator::UpdateDateBuffer(&theDate, 0); // get the current GMT date and time

    uint32_t realCode = 0;
    uint32_t len = sizeof(realCode);
	QTSSDictionary *dict = (QTSSDictionary *)inRequest;
    dict->GetValue(qtssRTSPReqRealStatusCode, 0,  (void*)&realCode,&len);

    char serverHeaderBuffer[64]; // the qtss Server: header field
    len = sizeof(serverHeaderBuffer) -1; // leave room for terminator
    ((QTSSDictionary*)sServer)->GetValue(qtssSvrRTSPServerHeader, 0,  (void*)serverHeaderBuffer,&len);
    serverHeaderBuffer[len] = 0; // terminate.
 
    snprintf(messageLineBuffer,maxMessageBufferChars, "HTTP/1.1 %"   _U32BITARG_   " %s",realCode, errorMsg);
    theErrorMessage.Put(messageLineBuffer,::strlen(messageLineBuffer));
    theErrorMessage.PutEOL();

    theErrorMessage.Put(serverHeaderBuffer,::strlen(serverHeaderBuffer));
    theErrorMessage.PutEOL();
 
    snprintf(messageLineBuffer,maxMessageBufferChars, "Date: %s",theDate.GetDateBuffer());
    theErrorMessage.Put(messageLineBuffer,::strlen(messageLineBuffer));
    theErrorMessage.PutEOL();
 
    bool addBody =  (errorMessage != nullptr && ::strlen(errorMessage) != 0); // body error message so add body headers
    if (addBody) // body error message so add body headers
    {
        // first create the html body
        static const StrPtrLen htmlBodyStart("<html><body>\n");
        bodyMessage.Put(htmlBodyStart.Ptr,htmlBodyStart.Len);
 
        //<h1>errorMessage</h1>\n
        static const StrPtrLen hStart("<h1>");
        bodyMessage.Put(hStart.Ptr,hStart.Len);

        bodyMessage.Put(errorMessage,::strlen(errorMessage));

        static const StrPtrLen hTerm("</h1>\n");
        bodyMessage.Put(hTerm.Ptr,hTerm.Len);
 
        static const StrPtrLen htmlBodyTerm("</body></html>\n");
        bodyMessage.Put(htmlBodyTerm.Ptr,htmlBodyTerm.Len);

        // write body headers
        static const StrPtrLen bodyHeaderType("Content-Type: text/html");
        theErrorMessage.Put(bodyHeaderType.Ptr,bodyHeaderType.Len);
        theErrorMessage.PutEOL();

        snprintf(messageLineBuffer,maxMessageBufferChars, "Content-Length: %"   _U32BITARG_   "", bodyMessage.GetBytesWritten());
        theErrorMessage.Put(messageLineBuffer,::strlen(messageLineBuffer));        
        theErrorMessage.PutEOL();
    }

    static const StrPtrLen headerClose("Connection: close");
    theErrorMessage.Put(headerClose.Ptr,headerClose.Len);
    theErrorMessage.PutEOL();

    theErrorMessage.PutEOL();  // terminate headers with empty line

    if (addBody) // add html body
    {
        theErrorMessage.Put(bodyMessage.GetBufPtr(),bodyMessage.GetBytesWritten());
    }

    //
    // Now that we've formatted the message into the temporary buffer,
    // write it out to the request stream and the Client Session object
    (void)QTSS_Write(inRequest, theErrorMessage.GetBufPtr(), theErrorMessage.GetBytesWritten(), nullptr, 0);
    (void)QTSS_SetValue(inRequest, qtssRTSPReqRespMsg, 0, theErrorMessage.GetBufPtr(), theErrorMessage.GetBytesWritten());
    
    return QTSS_RequestFailed;
}
														
void    QTSSModuleUtils::SendDescribeResponse(QTSS_RTSPRequestObject inRequest,
                                                    QTSS_ClientSessionObject inSession,
                                                    iovec* describeData,
                                                    uint32_t inNumVectors,
                                                    uint32_t inTotalLength)
{
    //write content size header
    char buf[32];
    sprintf(buf, "%" _S32BITARG_ "", inTotalLength);
    (void)QTSS_AppendRTSPHeader(inRequest, qtssContentLengthHeader, &buf[0], ::strlen(&buf[0]));

	((RTPSession*)inSession)->SendDescribeResponse((RTSPRequestInterface*)inRequest);

        // On solaris, the maximum # of vectors is very low (= 16) so to ensure that we are still able to
        // send the SDP if we have a number greater than the maximum allowed, we coalesce the vectors into
        // a single big buffer
#ifdef __solaris__
    if (inNumVectors > IOV_MAX )
    {
            char* describeDataBuffer = QTSSModuleUtils::CoalesceVectors(describeData, inNumVectors, inTotalLength);
            (void)QTSS_Write(inRequest, (void *)describeDataBuffer, inTotalLength, NULL, qtssWriteFlagsNoFlags);
            // deleting memory allocated by the CoalesceVectors call
            delete [] describeDataBuffer;
    }
    else
        (void)QTSS_WriteV(inRequest, describeData, inNumVectors, inTotalLength, NULL);
#else
    (void)QTSS_WriteV(inRequest, describeData, inNumVectors, inTotalLength, nullptr);
#endif

}

char*   QTSSModuleUtils::CoalesceVectors(iovec* inVec, uint32_t inNumVectors, uint32_t inTotalLength)
{
    if (inTotalLength == 0)
        return nullptr;
    
    auto* buffer = new char[inTotalLength];
    uint32_t bufferOffset = 0;
    
    for (uint32_t index = 0; index < inNumVectors; index++)
    {
        ::memcpy (buffer + bufferOffset, inVec[index].iov_base, inVec[index].iov_len);
        bufferOffset += inVec[index].iov_len;
    }
    
    Assert (bufferOffset == inTotalLength);
    
    return buffer;
}

QTSS_ModulePrefsObject QTSSModuleUtils::GetModulePrefsObject(QTSS_ModuleObject inModObject)
{
    QTSS_ModulePrefsObject thePrefsObject = nullptr;
    uint32_t theLen = sizeof(thePrefsObject);
    QTSS_Error theErr = ((QTSSDictionary*)inModObject)->GetValue(qtssModPrefs, 0, &thePrefsObject, &theLen);
    Assert(theErr == QTSS_NoErr);
    
    return thePrefsObject;
}

QTSS_Object QTSSModuleUtils::GetModuleAttributesObject(QTSS_ModuleObject inModObject)
{
    QTSS_Object theAttributesObject = nullptr;
    uint32_t theLen = sizeof(theAttributesObject);
    QTSS_Error theErr = ((QTSSDictionary*)inModObject)->GetValue(qtssModAttributes, 0, &theAttributesObject, &theLen);
    Assert(theErr == QTSS_NoErr);
    
    return theAttributesObject;
}

QTSS_ModulePrefsObject QTSSModuleUtils::GetModuleObjectByName(const StrPtrLen& inModuleName)
{
    QTSS_ModuleObject theModule = nullptr;
    uint32_t theLen = sizeof(theModule);
    
	QTSSDictionary *dict = (QTSSDictionary *)sServer;
    for (int x = 0; dict->GetValue(qtssSvrModuleObjects, x, &theModule, &theLen) == QTSS_NoErr; x++)
    {
        Assert(theModule != nullptr);
        Assert(theLen == sizeof(theModule));
        
        StrPtrLen theName;
        QTSS_Error theErr = ((QTSSDictionary*)theModule)->GetValuePtr(qtssModName, 0, (void**)&theName.Ptr, &theName.Len);
        Assert(theErr == QTSS_NoErr);
        
        if (inModuleName.Equal(theName))
            return theModule;
            
#if DEBUG
        theModule = NULL;
        theLen = sizeof(theModule);
#endif
    }
    return nullptr;
}

void    QTSSModuleUtils::GetAttribute(QTSS_Object inObject, char* inAttributeName, QTSS_AttrDataType inType, 
                                                void* ioBuffer, void* inDefaultValue, uint32_t inBufferLen)
{
    //
    // Check to make sure this attribute is the right type. If it's not, this will coerce
    // it to be the right type. This also returns the id of the attribute
    QTSS_AttributeID theID = QTSSModuleUtils::CheckAttributeDataType(inObject, inAttributeName, inType, inDefaultValue, inBufferLen);

    //
    // Get the attribute value.
    QTSS_Error theErr = ((QTSSDictionary*)inObject)->GetValue(theID, 0, ioBuffer, &inBufferLen);
    
    //
    // Caller should KNOW how big this attribute is
    Assert(theErr != QTSS_NotEnoughSpace);
    
    if (theErr != QTSS_NoErr)
    {
        //
        // If we couldn't get the attribute value for whatever reason, just use the
        // default if it was provided.
        ::memcpy(ioBuffer, inDefaultValue, inBufferLen);

        if (inBufferLen > 0)
        {
            //
            // Log an error for this pref only if there was a default value provided.
            char* theValueAsString = QTSSDataConverter::ValueToString(inDefaultValue, inBufferLen, inType);

            std::unique_ptr<char[]> theValueStr(theValueAsString);
            QTSSModuleUtils::LogError(  sMissingPrefVerbosity, 
                                        qtssServerPrefMissing,
                                        0,
                                        inAttributeName,
                                        theValueStr.get());
        }
        
        //
        // Create an entry for this attribute                           
        QTSSModuleUtils::CreateAttribute(inObject, inAttributeName, inType, inDefaultValue, inBufferLen);
    }
}

char*   QTSSModuleUtils::GetStringAttribute(QTSS_Object inObject, char* inAttributeName, char* inDefaultValue)
{
    uint32_t theDefaultValLen = 0;
    if (inDefaultValue != nullptr)
        theDefaultValLen = ::strlen(inDefaultValue);
    
    //
    // Check to make sure this attribute is the right type. If it's not, this will coerce
    // it to be the right type
    QTSS_AttributeID theID = QTSSModuleUtils::CheckAttributeDataType(inObject, inAttributeName, qtssAttrDataTypeCharArray, inDefaultValue, theDefaultValLen);

    char* theString = nullptr;
    (void)((QTSSDictionary*)inObject)->GetValueAsString(theID, 0, &theString);
    if (theString != nullptr)
        return theString;
    
    //
    // If we get here the attribute must be missing, so create it and log
    // an error.
    
    QTSSModuleUtils::CreateAttribute(inObject, inAttributeName, qtssAttrDataTypeCharArray, inDefaultValue, theDefaultValLen);
    
    //
    // Return the default if it was provided. Only log an error if the default value was provided
    if (theDefaultValLen > 0)
    {
        QTSSModuleUtils::LogError(  sMissingPrefVerbosity,
                                    qtssServerPrefMissing,
                                    0,
                                    inAttributeName,
                                    inDefaultValue);
    }
    
    if (inDefaultValue != nullptr)
    {
        //
        // Whether to return the default value or not from this function is dependent
        // solely on whether the caller passed in a non-NULL pointer or not.
        // This ensures that if the caller wants an empty-string returned as a default
        // value, it can do that.
        theString = new char[theDefaultValLen + 1];
        ::strcpy(theString, inDefaultValue);
        return theString;
    }
    return nullptr;
}

void    QTSSModuleUtils::GetIOAttribute(QTSS_Object inObject, char* inAttributeName, QTSS_AttrDataType inType,
                            void* ioDefaultResultBuffer, uint32_t inBufferLen)
{
    auto *defaultBuffPtr = new char[inBufferLen];
    ::memcpy(defaultBuffPtr,ioDefaultResultBuffer,inBufferLen);
    QTSSModuleUtils::GetAttribute(inObject, inAttributeName, inType, ioDefaultResultBuffer, defaultBuffPtr, inBufferLen);
    delete [] defaultBuffPtr;

}
                            

QTSS_AttributeID QTSSModuleUtils::GetAttrID(QTSS_Object inObject, char* inAttributeName)
{
    //
    // Get the attribute ID of this attribute.
    QTSS_Object theAttrInfo = nullptr;
    QTSS_Error theErr = QTSS_GetAttrInfoByName(inObject, inAttributeName, &theAttrInfo);
    if (theErr != QTSS_NoErr)
        return qtssIllegalAttrID;

    QTSS_AttributeID theID = qtssIllegalAttrID; 
    uint32_t theLen = sizeof(theID);
    theErr = ((QTSSDictionary*)theAttrInfo)->GetValue(qtssAttrID, 0, &theID, &theLen);
    Assert(theErr == QTSS_NoErr);

    return theID;
}

QTSS_AttributeID QTSSModuleUtils::CheckAttributeDataType(QTSS_Object inObject, char* inAttributeName, QTSS_AttrDataType inType, void* inDefaultValue, uint32_t inBufferLen)
{
    //
    // Get the attribute type of this attribute.
    QTSS_Object theAttrInfo = nullptr;
    QTSS_Error theErr = QTSS_GetAttrInfoByName(inObject, inAttributeName, &theAttrInfo);
    if (theErr != QTSS_NoErr)
        return qtssIllegalAttrID;

    QTSS_AttrDataType theAttributeType = qtssAttrDataTypeUnknown;
    uint32_t theLen = sizeof(theAttributeType);
	QTSSDictionary *dict = (QTSSDictionary *)theAttrInfo;
    theErr = dict->GetValue(qtssAttrDataType, 0, &theAttributeType, &theLen);
    Assert(theErr == QTSS_NoErr);
    
    QTSS_AttributeID theID = qtssIllegalAttrID; 
    theLen = sizeof(theID);
    theErr = dict->GetValue(qtssAttrID, 0, &theID, &theLen);
    Assert(theErr == QTSS_NoErr);

    if (theAttributeType != inType)
    {
        char* theValueAsString = QTSSDataConverter::ValueToString(inDefaultValue, inBufferLen, inType);

        std::unique_ptr<char[]> theValueStr(theValueAsString);
        QTSSModuleUtils::LogError(  qtssWarningVerbosity,
                                    qtssServerPrefWrongType,
                                    0,
                                    inAttributeName,
                                    theValueStr.get());
                                    
        theErr = QTSS_RemoveInstanceAttribute( inObject, theID );
        Assert(theErr == QTSS_NoErr);
        return  QTSSModuleUtils::CreateAttribute(inObject, inAttributeName, inType, inDefaultValue, inBufferLen);
    }
    return theID;
}

QTSS_AttributeID QTSSModuleUtils::CreateAttribute(QTSS_Object inObject, char* inAttributeName, QTSS_AttrDataType inType, void* inDefaultValue, uint32_t inBufferLen)
{
    QTSS_Error theErr = QTSS_AddInstanceAttribute(inObject, inAttributeName, nullptr, inType);
    Assert((theErr == QTSS_NoErr) || (theErr == QTSS_AttrNameExists));
    
    QTSS_AttributeID theID = QTSSModuleUtils::GetAttrID(inObject, inAttributeName);
    Assert(theID != qtssIllegalAttrID);
        
    //
    // Caller can pass in NULL for inDefaultValue, in which case we don't add the default
    if (inDefaultValue != nullptr)
    {
        theErr = QTSS_SetValue(inObject, theID, 0, inDefaultValue, inBufferLen);
        Assert(theErr == QTSS_NoErr);
    }
    return theID;
}

QTSS_ActionFlags QTSSModuleUtils::GetRequestActions(QTSS_RTSPRequestObject theRTSPRequest)
{
    // Don't touch write requests
    QTSS_ActionFlags action = qtssActionFlagsNoFlags;
    uint32_t len = sizeof(QTSS_ActionFlags);
    QTSS_Error theErr = ((QTSSDictionary*)theRTSPRequest)->GetValue(qtssRTSPReqAction, 0, (void*)&action, &len);
    Assert(theErr == QTSS_NoErr);
    Assert(len == sizeof(QTSS_ActionFlags));
    return action;
}

char* QTSSModuleUtils::GetMoviesRootDir_Copy(QTSS_RTSPRequestObject theRTSPRequest)
{   char*   movieRootDirStr = nullptr;
    QTSS_Error theErr = ((QTSSDictionary*)theRTSPRequest)->GetValueAsString(qtssRTSPReqRootDir, 0, &movieRootDirStr);
    Assert(theErr == QTSS_NoErr);
    return movieRootDirStr;
}

QTSS_UserProfileObject QTSSModuleUtils::GetUserProfileObject(QTSS_RTSPRequestObject theRTSPRequest)
{   QTSS_UserProfileObject theUserProfile = nullptr;
    uint32_t len = sizeof(QTSS_UserProfileObject);
    QTSS_Error theErr = ((QTSSDictionary*)theRTSPRequest)->GetValue(qtssRTSPReqUserProfile, 0, (void*)&theUserProfile, &len);
    Assert(theErr == QTSS_NoErr);
    return theUserProfile;
}

char *QTSSModuleUtils::GetUserName_Copy(QTSS_UserProfileObject inUserProfile)
{
    char*   username = nullptr;    
    (void)((QTSSDictionary*)inUserProfile)->GetValueAsString(qtssUserName, 0, &username);
    return username;
}

std::vector<std::string> QTSSModuleUtils::GetGroupsArray_Copy(QTSS_UserProfileObject inUserProfile)
{   
    if (nullptr == inUserProfile)
		return {};

	uint32_t outNumGroups;
    QTSS_Error theErr = QTSS_GetNumValues (inUserProfile,qtssUserGroups, &outNumGroups);
    if (theErr != QTSS_NoErr || outNumGroups == 0)
		return {};
 
	std::vector<std::string> result(outNumGroups);
    for (size_t index = 0; index < outNumGroups; index++)
    {   
		char *str = nullptr;
		uint32_t len = 0;
		((QTSSDictionary*)inUserProfile)->GetValuePtr(qtssUserGroups, index,(void **) &str, &len);
		result[index] = std::string(str, len);
		delete[] str;
    }   

    return result;
}

bool QTSSModuleUtils::UserInGroup(QTSS_UserProfileObject inUserProfile, char* inGroup, uint32_t inGroupLen)
{
	if (nullptr == inUserProfile || nullptr == inGroup  ||  inGroupLen == 0) 
		return false;
		
	char *userName = nullptr;
	uint32_t len = 0;
	((QTSSDictionary*)inUserProfile)->GetValuePtr(qtssUserName, 0, (void **)&userName, &len);
	if (len == 0 || userName == nullptr || userName[0] == 0) // no user to check
		return false;

	uint32_t numGroups = 0;
	QTSS_GetNumValues (inUserProfile,qtssUserGroups, &numGroups);
	if (numGroups == 0) // no groups to check
		return false;

	bool result = false;
	char* userGroup = nullptr;
	StrPtrLenDel userGroupStr; //deletes pointer in destructor
	
	for (uint32_t index = 0; index < numGroups; index++)
	{  
		userGroup = nullptr;
		((QTSSDictionary*)inUserProfile)->GetValueAsString(qtssUserGroups, index, &userGroup); //allocates string
		userGroupStr.Delete();
		userGroupStr.Set(userGroup);					
		if(userGroupStr.Equal(inGroup))
		{	
			result = true;
			break;
		}
	}   

	return result;
	
}


bool QTSSModuleUtils::AddressInList(QTSS_Object inObject, QTSS_AttributeID listID, StrPtrLen *inAddressPtr)
{
    StrPtrLenDel strDeleter;
    char*   theAttributeString = nullptr;    
    IPComponentStr inAddress(inAddressPtr);
    IPComponentStr addressFromList;
    
    if (!inAddress.Valid())
        return false;

    uint32_t numValues = 0;
    (void) QTSS_GetNumValues(inObject, listID, &numValues);
    
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

    uint32_t numValues = 0;
    (void) QTSS_GetNumValues(inObject, listID, &numValues);
    
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
    StrPtrLenDel userAgentStr;    	
    (void)((QTSSDictionary*)inParams->inClientSession)->GetValueAsString(qtssCliSesFirstUserAgent, 0, &userAgentStr.Ptr);
    userAgentStr.Set(userAgentStr.Ptr);
    
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


QTSS_Error QTSSModuleUtils::AuthorizeRequest(QTSS_RTSPRequestObject theRTSPRequest, bool* allowed, bool*foundUser, bool *authContinue)
{
    QTSS_Error theErr = QTSS_NoErr;
    //printf("QTSSModuleUtils::AuthorizeRequest allowed=%d foundUser=%d authContinue=%d\n", *allowed, *foundUser, *authContinue);
    
    if (nullptr != allowed)
        theErr = QTSS_SetValue(theRTSPRequest,qtssRTSPReqUserAllowed, 0, allowed, sizeof(bool));
    if (QTSS_NoErr != theErr)
        return theErr;
    
    if (nullptr != foundUser)
        theErr = QTSS_SetValue(theRTSPRequest,qtssRTSPReqUserFound, 0, foundUser, sizeof(bool));
    if (QTSS_NoErr != theErr)
        return theErr;  
    
    if (nullptr != authContinue)
        theErr = QTSS_SetValue(theRTSPRequest,qtssRTSPReqAuthHandled, 0, authContinue, sizeof(bool));
        
    return theErr;
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


