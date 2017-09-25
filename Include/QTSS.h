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

#ifndef QTSS_H
#define QTSS_H
#include "OSHeaders.h"
#include <string>
#include "QTSSRTSPProtocol.h"

#ifndef __Win32__
#include <sys/uio.h>
#endif

#define QTSS_MAX_MODULE_NAME_LENGTH     64
#define QTSS_MAX_SESSION_ID_LENGTH      64
#define QTSS_MAX_ATTRIBUTE_NAME_SIZE    64
#define QTSS_MAX_URL_LENGTH				512
#define QTSS_MAX_NAME_LENGTH			128
#define QTSS_MAX_REQUEST_BUFFER_SIZE	2*1024
#define EASY_ACCENCODER_BUFFER_SIZE_LEN	16*1024*4
#define QTSS_MAX_ATTRIBUTE_NUMS			128

#define EASY_KEY_SPLITER				"-"

//*******************************
// ENUMERATED TYPES

/**********************************/
// Error Codes

enum
{
    QTSS_NoErr              = 0,
    QTSS_RequestFailed      = -1,
    QTSS_Unimplemented      = -2,
    QTSS_RequestArrived     = -3,
    QTSS_OutOfState         = -4,
    QTSS_NotAModule         = -5,
    QTSS_WrongVersion       = -6,
    QTSS_IllegalService     = -7,
    QTSS_BadIndex           = -8,
    QTSS_ValueNotFound      = -9,
    QTSS_BadArgument        = -10,
    QTSS_ReadOnly           = -11,
    QTSS_NotPreemptiveSafe  = -12,
    QTSS_NotEnoughSpace     = -13,
    QTSS_WouldBlock         = -14,
    QTSS_NotConnected       = -15,
    QTSS_FileNotFound       = -16,
    QTSS_NoMoreData         = -17,
    QTSS_AttrDoesntExist    = -18,
    QTSS_AttrNameExists     = -19,
    QTSS_InstanceAttrsNotAllowed= -20,
	QTSS_UnknowAudioCoder   =-21
};
typedef int32_t QTSS_Error;

// QTSS_AddStreamFlags used in the QTSS_AddStream Callback function
enum
{
    qtssASFlagsNoFlags              = 0x00000000,
    qtssASFlagsAllowDestination     = 0x00000001,
    qtssASFlagsForceInterleave      = 0x00000002,
    qtssASFlagsDontUseSlowStart     = 0x00000004,
    qtssASFlagsForceUDPTransport    = 0x00000008
};
typedef uint32_t QTSS_AddStreamFlags;

// QTSS_PlayFlags used in the QTSS_Play Callback function.
enum 
{
    qtssPlayFlagsSendRTCP           = 0x00000010,   // have the server generate RTCP Sender Reports 
    qtssPlayFlagsAppendServerInfo   = 0x00000020    // have the server append the server info APP packet to your RTCP Sender Reports
};
typedef uint32_t QTSS_PlayFlags;

// Flags for QTSS_Write when writing to a QTSS_ClientSessionObject.
enum 
{
    qtssWriteFlagsNoFlags           = 0x00000000,
    qtssWriteFlagsIsRTP             = 0x00000001,
    qtssWriteFlagsIsRTCP            = 0x00000002,   
    qtssWriteFlagsWriteBurstBegin   = 0x00000004,
    qtssWriteFlagsBufferData        = 0x00000008
};
typedef uint32_t QTSS_WriteFlags;

// Flags for QTSS_SendStandardRTSPResponse
enum
{
    qtssPlayRespWriteTrackInfo      = 0x00000001,
    qtssSetupRespDontWriteSSRC      = 0x00000002
};

enum
{
	easyRedisActionDelete		= 0,
	easyRedisActionSet			= 1
};
typedef uint32_t Easy_RedisAction;

/**********************************/
// RTP SESSION STATES
//
// Is this session playing, paused, or what?
enum
{
    qtssPausedState         = 0,
    qtssPlayingState        = 1
};
typedef uint32_t QTSS_RTPSessionState;

//*********************************/
// CLIENT SESSION CLOSING REASON
//
// Why is this Client going away?
enum
{
    qtssCliSesCloseClientTeardown       = 0, // QTSS_Teardown was called on this session
    qtssCliSesCloseTimeout              = 1, // Server is timing this session out
    qtssCliSesCloseClientDisconnect     = 2  // Client disconnected.
};
typedef uint32_t QTSS_CliSesClosingReason;

// CLIENT SESSION TEARDOWN REASON
//
//  An attribute in the QTSS_ClientSessionObject 
//
//  When calling QTSS_Teardown, a module should specify the QTSS_CliSesTeardownReason in the QTSS_ClientSessionObject 
//  if the tear down was not a client request.
//  
enum
{
    qtssCliSesTearDownClientRequest             = 0,
    qtssCliSesTearDownUnsupportedMedia          = 1,
    qtssCliSesTearDownServerShutdown            = 2,
    qtssCliSesTearDownServerInternalErr         = 3,
    qtssCliSesTearDownBroadcastEnded            = 4 // A broadcast the client was watching ended
    
};
typedef uint32_t  QTSS_CliSesTeardownReason;

// Events
enum
{
    QTSS_ReadableEvent      = 1,
    QTSS_WriteableEvent     = 2
};
typedef uint32_t  QTSS_EventType;

/**********************************/
// RTSP SESSION TYPES
//
// Is this a normal RTSP session or an RTSP / HTTP session?
enum
{
    qtssRTSPSession         = 0,
    qtssRTSPHTTPSession     = 1,
    qtssRTSPHTTPInputSession= 2 //The input half of an RTSPHTTP session. These session types are usually very short lived.
};
typedef uint32_t QTSS_RTSPSessionType;

/**********************************/
//
// What type of RTP transport is being used for the RTP stream?
enum
{
    qtssRTPTransportTypeUDP         = 0,
    qtssRTPTransportTypeReliableUDP = 1,
    qtssRTPTransportTypeTCP         = 2,
    qtssRTPTransportTypeUnknown     = 3
};
typedef uint32_t QTSS_RTPTransportType;

/**********************************/
//
// What type of RTP network mode is being used for the RTP stream?
// unicast | multicast (mutually exclusive)
enum
{
    qtssRTPNetworkModeDefault       = 0, // not declared
    qtssRTPNetworkModeMulticast     = 1,
    qtssRTPNetworkModeUnicast       = 2
};
typedef uint32_t QTSS_RTPNetworkMode;

/**********************************/
//
// The transport mode in a SETUP request
enum
{
    qtssRTPTransportModePlay        = 0,
    qtssRTPTransportModeRecord      = 1
};
typedef uint32_t QTSS_RTPTransportMode;

/**********************************/
// PAYLOAD TYPES
//
// When a module adds an RTP stream to a client session, it must specify
// the stream's payload type. This is so that other modules can find out
// this information in a generalized fashion. Here are the currently
// defined payload types
enum
{
    qtssUnknownPayloadType  = 0,
    qtssVideoPayloadType    = 1,
    qtssAudioPayloadType    = 2
};
typedef uint32_t QTSS_RTPPayloadType;

/**********************************/
// QTSS API OBJECT TYPES
enum
{
    qtssDynamicObjectType           = FOUR_CHARS_TO_INT('d', 'y', 'm', 'c'), //dymc
    qtssRTPStreamObjectType         = FOUR_CHARS_TO_INT('r', 's', 't', 'o'), //rsto
    qtssClientSessionObjectType     = FOUR_CHARS_TO_INT('c', 's', 'e', 'o'), //cseo
    qtssRTSPSessionObjectType       = FOUR_CHARS_TO_INT('s', 's', 'e', 'o'), //sseo
    qtssRTSPRequestObjectType       = FOUR_CHARS_TO_INT('s', 'r', 'q', 'o'), //srqo
    qtssRTSPHeaderObjectType        = FOUR_CHARS_TO_INT('s', 'h', 'd', 'o'), //shdo
    qtssServerObjectType            = FOUR_CHARS_TO_INT('s', 'e', 'r', 'o'), //sero
    qtssPrefsObjectType             = FOUR_CHARS_TO_INT('p', 'r', 'f', 'o'), //prfo
    qtssTextMessagesObjectType      = FOUR_CHARS_TO_INT('t', 'x', 't', 'o'), //txto
    qtssFileObjectType              = FOUR_CHARS_TO_INT('f', 'i', 'l', 'e'), //file
    qtssModuleObjectType            = FOUR_CHARS_TO_INT('m', 'o', 'd', 'o'), //modo
    qtssModulePrefsObjectType       = FOUR_CHARS_TO_INT('m', 'o', 'd', 'p'), //modp
    qtssAttrInfoObjectType          = FOUR_CHARS_TO_INT('a', 't', 't', 'r'), //attr

    qtssConnectedUserObjectType     = FOUR_CHARS_TO_INT('c', 'u', 's', 'r'), //cusr
	easyHTTPSessionObjectType		= FOUR_CHARS_TO_INT('e', 'h', 's', 'o')  //ehso
    
};
typedef uint32_t QTSS_ObjectType;

enum
{
    qtssOpenFileNoFlags =       0,
    qtssOpenFileAsync =         1,  // File stream will be asynchronous (read may return QTSS_WouldBlock)
    qtssOpenFileReadAhead =     2   // File stream will be used for a linear read through the file.
};
typedef uint32_t QTSS_OpenFileFlags;


/**********************************/
// SERVER STATES
//
//  An attribute in the QTSS_ServerObject returns the server state
//  as a QTSS_ServerState. Modules may also set the server state.
//
//  Setting the server state to qtssFatalErrorState, or qtssShuttingDownState
//  will cause the server to quit.
//
//  Setting the state to qtssRefusingConnectionsState will cause the server
//  to start refusing new connections.
enum
{
    qtssStartingUpState             = 0,
    qtssRunningState                = 1,
    qtssRefusingConnectionsState    = 2,
    qtssFatalErrorState             = 3,//a fatal error has occurred, not shutting down yet
    qtssShuttingDownState           = 4,
    qtssIdleState                   = 5 // Like refusing connections state, but will also kill any currently connected clients
};
typedef uint32_t QTSS_ServerState;

//***********************************************/
// TYPEDEFS

typedef int64_t          QTSS_TimeVal;


class RTSPRequest;
class RTPSession;
class RTSPSession;
class QTSServerInterface;
//***********************************************/
// ROLE PARAMETER BLOCKS
//
// Each role has a unique set of parameters that get passed
// to the module.

class QTSSStream;

typedef struct
{
	QTSServerInterface*         inServer;           // Global dictionaries
} QTSS_Initialize_Params;

typedef struct 
{
	RTSPSession*                inRTSPSession;
    RTSPRequest*                inRTSPRequest;
	RTPSession*                 inClientSession;

} QTSS_StandardRTSP_Params;

typedef struct 
{
	RTSPSession*                inRTSPSession;
    RTPSession*                 inClientSession;
    char*                       inPacketData;
    uint32_t                    inPacketLen;

} QTSS_IncomingData_Params;

typedef struct
{
    RTPSession*                     inClientSession;
    QTSS_TimeVal                    inCurrentTime;
    QTSS_TimeVal                    outNextPacketTime;
} QTSS_RTPSendPackets_Params;

typedef struct
{
	RTPSession*                     inClientSession;
    QTSS_CliSesClosingReason        inReason;
} QTSS_ClientSessionClosing_Params;


typedef union
{
	QTSS_StandardRTSP_Params            rtspParams;

    QTSS_RTPSendPackets_Params          rtpSendPacketsParams;

} QTSS_RoleParams, *QTSS_RoleParamPtr;

typedef struct
{
    void*                           packetData;
    QTSS_TimeVal                    packetTransmitTime;
    QTSS_TimeVal                    suggestedWakeupTime;
} QTSS_PacketStruct;
                                                                                                                                                                 
#endif
