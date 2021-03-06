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
    File:       QTSSRTSPProtocol.h

    Contains:   Constant & Enum definitions for RTSP protocol type parts
                of QTSS API.

    
*/

#ifndef QTSS_RTSPPROTOCOL_H
#define QTSS_RTSPPROTOCOL_H
#include "OSHeaders.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


enum
{
    qtssDescribeMethod      = 0, 
    qtssSetupMethod         = 1,
    qtssTeardownMethod      = 2,
    qtssNumVIPMethods       = 3,

    qtssPlayMethod          = 3,
    qtssPauseMethod         = 4,
    qtssOptionsMethod       = 5,
    qtssAnnounceMethod      = 6,
    qtssGetParameterMethod  = 7,
    qtssSetParameterMethod  = 8,
    qtssRedirectMethod      = 9,
    qtssRecordMethod        = 10,
    
    qtssNumMethods          = 11,
    qtssIllegalMethod       = 11
    
};
typedef uint32_t QTSS_RTSPMethod;


enum
{
    //These are the common request headers (optimized)
    qtssAcceptHeader            = 0,
    qtssCSeqHeader              = 1,
    qtssUserAgentHeader         = 2,
    qtssTransportHeader         = 3,
    qtssSessionHeader           = 4,
    qtssRangeHeader             = 5,
    qtssNumVIPHeaders           = 6,
    
    //Other request headers
    qtssAcceptEncodingHeader    = 6,
    qtssAcceptLanguageHeader    = 7,
    qtssAuthorizationHeader     = 8,        
    qtssBandwidthHeader         = 9,
    qtssBlockSizeHeader         = 10,
    qtssCacheControlHeader      = 11,
    qtssConferenceHeader        = 12,       
    qtssConnectionHeader        = 13,
    qtssContentBaseHeader       = 14,
    qtssContentEncodingHeader   = 15,
    qtssContentLanguageHeader   = 16,
    qtssContentLengthHeader     = 17,
    qtssContentLocationHeader   = 18,
    qtssContentTypeHeader       = 19,
    qtssDateHeader              = 20,
    qtssExpiresHeader           = 21,
    qtssFromHeader              = 22,
    qtssHostHeader              = 23,
    qtssIfMatchHeader           = 24,
    qtssIfModifiedSinceHeader   = 25,
    qtssLastModifiedHeader      = 26,
    qtssLocationHeader          = 27,
    qtssProxyAuthenticateHeader = 28,
    qtssProxyRequireHeader      = 29,
    qtssRefererHeader           = 30,
    qtssRetryAfterHeader        = 31,
    qtssRequireHeader           = 32,
    qtssRTPInfoHeader           = 33,
    qtssScaleHeader             = 34,
    qtssSpeedHeader             = 35,
    qtssTimestampHeader         = 36,
    qtssVaryHeader              = 37,
    qtssViaHeader               = 38,
    qtssNumRequestHeaders       = 39,
    
    //Additional response headers
    qtssAllowHeader             = 39,
    qtssPublicHeader            = 40,
    qtssUnsupportedHeader       = 41,
    qtssWWWAuthenticateHeader   = 42,
    qtssSameAsLastHeader        = 43,
    
    //Newly added headers
    qtssExtensionHeaders        = 44,
    
    qtssXRetransmitHeader       = 44,
    qtssXAcceptRetransmitHeader = 45,
    qtssXRTPMetaInfoHeader      = 46,
    qtssXTransportOptionsHeader = 47,
    qtssXPacketRangeHeader      = 48,
    qtssXPreBufferHeader        = 49,
	qtssXDynamicRateHeader      = 50,
	qtssXAcceptDynamicRateHeader= 51,
		
	qtssNumHeaders				= 53,
	qtssIllegalHeader 			= 53
    
};
typedef uint32_t QTSS_RTSPHeader;


enum
{
    qtssContinue                        = 0,        //100
    qtssSuccessOK                       = 1,        //200
    qtssSuccessCreated                  = 2,        //201
    qtssSuccessAccepted                 = 3,        //202
    qtssSuccessNoContent                = 4,        //203
    qtssSuccessPartialContent           = 5,        //204
    qtssSuccessLowOnStorage             = 6,        //250
    qtssMultipleChoices                 = 7,        //300
    qtssRedirectPermMoved               = 8,        //301
    qtssRedirectTempMoved               = 9,        //302
    qtssRedirectSeeOther                = 10,       //303

    qtssUseProxy                        = 12,       //305
    qtssClientBadRequest                = 13,       //400
    qtssClientUnAuthorized              = 14,       //401
    qtssPaymentRequired                 = 15,       //402
    qtssClientForbidden                 = 16,       //403
    qtssClientNotFound                  = 17,       //404
    qtssClientMethodNotAllowed          = 18,       //405
    qtssNotAcceptable                   = 19,       //406
    qtssProxyAuthenticationRequired     = 20,       //407
    qtssRequestTimeout                  = 21,       //408
    qtssClientConflict                  = 22,       //409
    qtssGone                            = 23,       //410
    qtssLengthRequired                  = 24,       //411
    qtssPreconditionFailed              = 25,       //412
    qtssRequestEntityTooLarge           = 26,       //413
    qtssRequestURITooLarge              = 27,       //414
    qtssUnsupportedMediaType            = 28,       //415
    qtssClientParameterNotUnderstood    = 29,       //451
    qtssClientConferenceNotFound        = 30,       //452
    qtssClientNotEnoughBandwidth        = 31,       //453
    qtssClientSessionNotFound           = 32,       //454
    qtssClientMethodNotValidInState     = 33,       //455
    qtssClientHeaderFieldNotValid       = 34,       //456
    qtssClientInvalidRange              = 35,       //457
    qtssClientReadOnlyParameter         = 36,       //458
    qtssClientAggregateOptionNotAllowed = 37,       //459
    qtssClientAggregateOptionAllowed    = 38,       //460
    qtssClientUnsupportedTransport      = 39,       //461
    qtssClientDestinationUnreachable    = 40,       //462
    qtssServerInternal                  = 41,       //500
    qtssServerNotImplemented            = 42,       //501
    qtssServerBadGateway                = 43,       //502
    qtssServerUnavailable               = 44,       //503
    qtssServerGatewayTimeout            = 45,       //505
    qtssRTSPVersionNotSupported         = 46,       //504
    qtssServerOptionNotSupported        = 47,       //551
    qtssNumStatusCodes                  = 48
    
};
typedef uint32_t QTSS_RTSPStatusCode;

#ifdef __cplusplus
}
#endif

#endif
