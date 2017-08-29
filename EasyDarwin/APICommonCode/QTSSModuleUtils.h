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
    File:       QTSSModuleUtils.h

    Contains:   Utility routines for modules to use.
                    
*/


#ifndef _QTSS_MODULE_UTILS_H_
#define _QTSS_MODULE_UTILS_H_

#include <boost/utility/string_view.hpp>
#include <stdlib.h>
#include <string>
#include <vector>
#include "QTSS.h"
#include "StrPtrLen.h"
#include "RTPMetaInfoPacket.h"

class QTSSStream;
class QTSSUserProfile;
class QTSSModuleUtils
{

public:
	
        // compatibiltiy features for certain players  
        enum    {  
                    kRequiresRTPInfoSeqAndTime  = 0, 
                    kAdjustBandwidth            = 1,
                    kDisablePauseAdjustedRTPTime= 2,
                    kDelayRTPStreamsUntilAfterRTSPResponse = 3,
                };
    
      
        static void     Initialize(QTSServerInterface* inServer);
    
        // Read the complete contents of the file at inPath into the StrPtrLen.
        // This function allocates memory for the file data.
        static QTSS_Error   ReadEntireFile(char* inPath, StrPtrLen* outData, QTSS_TimeVal inModDate = -1, QTSS_TimeVal* outModDate = nullptr);
                                                                                    
        //
        // This function does 2 things:
        // 1.   Compares the enabled fields in the field ID array with the fields in the
        //      x-RTP-Meta-Info header. Turns off the fields in the array that aren't in the request.
        //
        // 2.   Appends the x-RTP-Meta-Info header to the response, using the proper
        //      fields from the array, as well as the IDs provided in the array
        static QTSS_Error   AppendRTPMetaInfoHeader(RTSPRequest* inRequest,
                                                        StrPtrLen* inRTPMetaInfoHeader,
                                                        RTPMetaInfoPacket::FieldID* inFieldIDArray);

        // This function sends an error to the RTSP client. You must provide a
        // status code for the error, and a text message ID to describe the error.
        //
        // It always returns QTSS_RequestFailed.

        static QTSS_Error   SendErrorResponse(RTSPRequest* inRequest,
                                              QTSS_RTSPStatusCode inStatusCode);
														
		// This function sends an error to the RTSP client. You don't have to provide
		// a text message ID, but instead you need to provide the error message in a
		// string
		// 
		// It always returns QTSS_RequestFailed
		static QTSS_Error	SendErrorResponseWithMessage(RTSPRequest* inRequest,
														QTSS_RTSPStatusCode inStatusCode);

        // Sends and HTTP 1.1 error message with an error message in HTML if errorMessage != NULL.
        // The session must be flagged by KillSession set to true to kill.
        // Use the QTSS_RTSPStatusCodes for the inStatusCode, for now they are the same as HTTP.
        //
		// It always returns QTSS_RequestFailed
        static QTSS_Error	SendHTTPErrorResponse(RTSPRequest* inRequest,
													QTSS_SessionStatusCode inStatusCode,
                                                    bool inKillSession,
                                                    char *errorMessage);

        //Modules most certainly don't NEED to use this function, but it is awfully handy
        //if they want to take advantage of it. Using the SDP data provided in the iovec,
        //this function sends a standard describe response.
        //NOTE: THE FIRST ENTRY OF THE IOVEC MUST BE EMPTY!!!!
        static void SendDescribeResponse(RTSPRequest* inRequest,
                                                    iovec* describeData,
                                                    uint32_t inNumVectors,
                                                    uint32_t inTotalLength);
                                                                                                                                                                                               
        //
        //
        //
        /// Get the type of request. Returns qtssActionFlagsNoFlags on failure.
        //  Result is a bitmap of flags
        //      
        static boost::string_view  GetUserName(QTSSUserProfile* inUserProfile);
        static bool UserInGroup(QTSSUserProfile* inUserProfile, boost::string_view inGroup);
 
        static bool AddressInList(QTSS_Object inObject, QTSS_AttributeID listID, StrPtrLen *theAddressPtr);
 
        static bool FindStringInAttributeList(QTSS_Object inObject, QTSS_AttributeID listID, StrPtrLen *inStrPtr);

        static bool HavePlayerProfile(QTSS_PrefsObject inPrefObjectToCheck, QTSS_StandardRTSP_Params* inParams, uint32_t feature);
        
        static QTSS_Error AuthorizeRequest(RTSPRequest* theRTSPRequest, bool allowed, bool haveUser,bool authContinue);
        
         
    private:
        static QTSServerInterface*      sServer;
};


class IPComponentStr
{
    public:
    enum { kNumComponents = 4 };
    
    StrPtrLen   fAddressComponent[kNumComponents];
    bool      fIsValid{false};
    static IPComponentStr sLocalIPCompStr;

    IPComponentStr() {}
    IPComponentStr(char *theAddress);
    IPComponentStr(StrPtrLen *sourceStrPtr);
    
inline  StrPtrLen*  GetComponent(uint16_t which);
        bool      Equal(IPComponentStr *testAddressPtr);
        bool      Set(StrPtrLen *theAddressStrPtr);
        bool      Valid() { return fIsValid; }
inline  bool      IsLocal();

};


bool  IPComponentStr::IsLocal()
{
    if (this->Equal(&sLocalIPCompStr))
        return true;
    
    return false;
}

StrPtrLen* IPComponentStr::GetComponent(uint16_t which) 
{
   if (which < IPComponentStr::kNumComponents) 
        return &fAddressComponent[which]; 
   
   Assert(0);
   return nullptr; 
}

#endif //_QTSS_MODULE_UTILS_H_
