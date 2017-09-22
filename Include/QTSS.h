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

/**********************************/
// ILLEGAL ATTRIBUTE ID
enum
{
    qtssIllegalAttrID               = -1,
    qtssIllegalServiceID            = -1
};

//*********************************/
// QTSS DON'T CALL SENDPACKETS AGAIN
// If this time is specified as the next packet time when returning
// from QTSS_SendPackets_Role, the module won't get called again in
// that role until another QTSS_Play is issued
enum
{
    qtssDontCallSendPacketsAgain    = -1
};

// DATA TYPES
enum
{
    qtssAttrDataTypeUnknown         = 0,
    qtssAttrDataTypeCharArray       = 1,
    qtssAttrDataTypeBool16          = 2,
    qtssAttrDataTypeSInt16          = 3,
    qtssAttrDataTypeUInt16          = 4,
    qtssAttrDataTypeint32_t          = 5,
    qtssAttrDataTypeUInt32          = 6,
    qtssAttrDataTypeint64_t          = 7,
    qtssAttrDataTypeuint64_t          = 8,
    qtssAttrDataTypeQTSS_Object     = 9,
    qtssAttrDataTypeQTSS_StreamRef  = 10,
    qtssAttrDataTypeFloat32         = 11,
    qtssAttrDataTypeFloat64         = 12,
    qtssAttrDataTypeVoidPointer     = 13,
    qtssAttrDataTypeTimeVal         = 14,
    
    qtssAttrDataTypeNumTypes        = 15
};
typedef uint32_t QTSS_AttrDataType;

enum
{
    qtssAttrModeRead                = 1,
    qtssAttrModeWrite               = 2,
    qtssAttrModePreempSafe          = 4,
    qtssAttrModeInstanceAttrAllowed = 8,
    qtssAttrModeCacheable           = 16,
    qtssAttrModeDelete              = 32
};
typedef uint32_t QTSS_AttrPermission;

/**********************************/
//BUILT IN SERVER ATTRIBUTES

//The server maintains many attributes internally, and makes these available to plug-ins.
//Each value is a standard attribute, with a name and everything. Plug-ins may resolve the id's of
//these values by name if they'd like, but in the initialize role they will receive a struct of
//all the ids of all the internally maintained server parameters. This enumerated type block defines the indexes
//in that array for the id's.

enum
{
    //QTSS_RTPStreamObject parameters. All of these are preemptive safe.
    
    // All of these parameters come out of the last RTCP packet received on this stream.
    // If the corresponding field in the last RTCP packet was blank, the attribute value will be 0.
    
    // Address & network related parameters
    
    qtssRTPStrNumParams             = 40

};
typedef uint32_t QTSS_RTPStreamAttributes;

enum
{
    qtssCliSesNumParams             = 38    
};
typedef uint32_t QTSS_ClientSessionAttributes;

enum
{
    //QTSS_RTSPSessionObject parameters
    
    //Valid in any role that receives a QTSS_RTSPSessionObject   
    qtssRTSPSesNumParams    = 15
};
typedef uint32_t QTSS_RTSPSessionAttributes;

enum 
{
    //All text names are identical to the enumerated type names

    //QTSS_RTSPRequestObject parameters. All of these are pre-emptive safe parameters

    //Available in every role that receives the QTSS_RTSPRequestObject
    //Available in every method that receives the QTSS_RTSPRequestObject except for the QTSS_FilterMethod
    qtssRTSPReqNumParams            = 43
    
};
typedef uint32_t QTSS_RTSPRequestAttributes;

enum
{
    qtssSvrNumParams                = 38
};
typedef uint32_t QTSS_ServerAttributes;

enum
{
    //QTSS_PrefsObject parameters

    // Valid in all methods. None of these are pre-emptive safe, so the version
    // of QTSS_GetAttribute that copies data must be used.
    
    // All of these parameters are read-write. 
    
    qtssPrefsMaximumConnections				= 3,    //"maximum_connections"         //int32_t    //Maximum # of concurrent RTP connections allowed by the server. -1 means unlimited.
    qtssPrefsBreakOnAssert					= 7,    //"break_on_assert"             //bool        //If true, the server will break in the debugger when an assert fails.
    qtssPrefsAutoRestart					= 8,    //"auto_restart"                //bool        //If true, the server will automatically restart itself if it crashes.
    qtssPrefsModuleFolder					= 12,   //"module_folder"               //char array    //Path to the module folder

    // There is a compiled-in error log module that loads before all the other modules
    // (so it can log errors from the get-go). It uses these prefs.
    
    qtssPrefsErrorLogName					= 13,   //"error_logfile_name"          //char array        //Name of error log file
    qtssPrefsErrorLogDir					= 14,   //"error_logfile_dir"           //char array        //Path to error log file directory
    qtssPrefsErrorRollInterval				= 15,   //"error_logfile_interval"      //uint32_t    //Interval in days between error logfile rolls
    qtssPrefsMaxErrorLogSize				= 16,   //"error_logfile_size"          //uint32_t    //Max size in bytes of the error log
    qtssPrefsScreenLogging					= 18,   //"screen_logging"              //bool        //Should the error logger echo messages to the screen?
    qtssPrefsErrorLogEnabled				= 19,   //"error_logging"               //bool        //Is error logging enabled?
            
    qtssPrefsSrcAddrInTransport             = 31,   // "append_source_addr_in_transport" // bool   //If true, the server will append the src address to the Transport header responses

    qtssPrefsAckLoggingEnabled              = 35,   // "ack_logging_enabled"  // bool  //Debugging only: turns on detailed logging of UDP acks / retransmits
    qtssPrefsRTCPPollIntervalInMsec         = 36,   // "rtcp_poll_interval"      // uint32_t   //interval (in Msec) between poll for RTCP packets

    qtssPrefsEnableCloudPlatform            = 43,   // "enable_cloud_platform"   // bool   

    qtssPrefsDeleteSDPFilesInterval         = 45,   // "sdp_file_delete_interval_seconds" //uint32_t //Feature rem
    qtssPrefsAutoStart                      = 46,   // "auto_start" //bool //If true, streaming server likes to be started at system startup
    
    qtssPrefsEnableRTSPErrorMessage         = 55,   // "RTSP_error_message" //bool // Appends a content body string error message for reported RTSP errors.
    qtssPrefsEnableRTSPDebugPrintfs         = 56,   // "RTSP_debug_printfs" //Boo1l6 // printfs incoming RTSPRequests and Outgoing RTSP responses.

    qtssPrefsEnableMonitorStatsFile         = 57,   // "enable_monitor_stats_file" //bool //write server stats to the monitor file
    qtssPrefsMonitorStatsFileIntervalSec    = 58,   // "monitor_stats_file_interval_seconds" // private
    qtssPrefsMonitorStatsFileName           = 59,   // "monitor_stats_file_name" // private

    qtssPrefsEnablePacketHeaderPrintfs      = 60,   // "enable_packet_header_printfs" //bool // RTP and RTCP printfs of outgoing packets.

    qtssPrefsPidFile                        = 67,   // "pid_file" //Char Array //path to pid file
    qtssPrefsCloseLogsOnWrite               = 68,   // "force_logs_close_on_write" //bool // force log files to close after each write.
	
	easyPrefsHTTPServiceLanPort				= 82,	// "service_lan_port"	//UInt16
	easyPrefsHTTPServiceWanPort				= 83,	// "service_wan_port"	//UInt16

	easyPrefsServiceWANIPAddr				= 84,	// "service_wan_ip"		//char array
	easyPrefsRTSPWANPort					= 85,	// "rtsp_wan_port"		//UInt16

	qtssPrefsNumParams                      = 86
};

typedef uint32_t QTSS_PrefsAttributes;

enum
{
    //QTSS_TextMessagesObject parameters
    
    // All of these parameters are read-only, char*'s, and preemptive-safe.
    
    qtssMsgNoMessage                = 0,    //"NoMessage"
    qtssMsgNoURLInRequest           = 1,
    qtssMsgBadRTSPMethod            = 2,
    qtssMsgNoRTSPVersion            = 3,
    qtssMsgNoRTSPInURL              = 4,
    qtssMsgURLTooLong               = 5,
    qtssMsgURLInBadFormat           = 6,
    qtssMsgNoColonAfterHeader       = 7,
    qtssMsgNoEOLAfterHeader         = 8,
    qtssMsgRequestTooLong           = 9,
    qtssMsgNoModuleFolder           = 10,
    qtssMsgCouldntListen            = 11,
    qtssMsgInitFailed               = 12,
    qtssMsgNotConfiguredForIP       = 13,
    qtssMsgDefaultRTSPAddrUnavail   = 14,
    qtssMsgBadModule                = 15,
    qtssMsgRegFailed                = 16,
    qtssMsgRefusingConnections      = 17,
    qtssMsgTooManyClients           = 18,
    qtssMsgTooMuchThruput           = 19,
    qtssMsgNoSessionID              = 20,
    qtssMsgFileNameTooLong          = 21,
    qtssMsgNoClientPortInTransport  = 22,
    qtssMsgRTPPortMustBeEven        = 23,
    qtssMsgRTCPPortMustBeOneBigger  = 24,
    qtssMsgOutOfPorts               = 25,
    qtssMsgNoModuleForRequest       = 26,
    qtssMsgAltDestNotAllowed        = 27,
    qtssMsgCantSetupMulticast       = 28,
    qtssListenPortInUse             = 29,
    qtssListenPortAccessDenied      = 30,
    qtssListenPortError             = 31,
    qtssMsgBadBase64                = 32,
    qtssMsgSomePortsFailed          = 33,
    qtssMsgNoPortsSucceeded         = 34,
    qtssMsgCannotCreatePidFile      = 35,
    qtssMsgCannotSetRunUser         = 36,
    qtssMsgCannotSetRunGroup        = 37,
    qtssMsgNoSesIDOnDescribe        = 38,
    qtssServerPrefMissing           = 39,
    qtssServerPrefWrongType         = 40,
    qtssMsgCantWriteFile            = 41,
    qtssMsgSockBufSizesTooLarge     = 42,
    qtssMsgBadFormat                = 43,
    qtssMsgNumParams                = 44
    
};
typedef uint32_t QTSS_TextMessagesAttributes;

enum
{
    //QTSS_FileObject parameters
    
    // All of these parameters are preemptive-safe.
    
    qtssFlObjStream                 = 0,    // read // QTSS_FileStream. Stream ref for this file object
    qtssFlObjFileSysModuleName      = 1,    // read // char array. Name of the file system module handling this file object
    qtssFlObjLength                 = 2,    // r/w  // uint64_t. Length of the file
    qtssFlObjPosition               = 3,    // read // uint64_t. Current position of the file pointer in the file.
    qtssFlObjModDate                = 4,    // r/w  // QTSS_TimeVal. Date & time of last modification

    qtssFlObjNumParams              = 5
};
typedef uint32_t QTSS_FileObjectAttributes;

enum
{
    //QTSS_ModuleObject parameters
    
    qtssModDesc                 = 1,    //r/w       //not preemptive-safe   //char array        //Text description of what the module does
    qtssModVersion              = 2,    //r/w       //not preemptive-safe   //uint32_t            //Version of the module. uint32_t format should be 0xMM.m.v.bbbb M=major version m=minor version v=very minor version b=build #
    qtssModRoles                = 3,    //read      //preemptive-safe       //QTSS_Role         //List of all the roles this module has registered for.
    qtssModPrefs                = 4,    //read      //preemptive-safe       //QTSS_ModulePrefsObject //An object containing as attributes the preferences for this module
    qtssModAttributes           = 5,    //read      //preemptive-safe       //QTSS_Object
            
    qtssModNumParams            = 6
};
typedef uint32_t QTSS_ModuleObjectAttributes;

enum
{
    //QTSS_AttrInfoObject parameters
    
    // All of these parameters are preemptive-safe.

    qtssAttrName                    = 0,    //read //char array             //Attribute name
    qtssAttrID                      = 1,    //read //QTSS_AttributeID       //Attribute ID
    qtssAttrDataType                = 2,    //read //QTSS_AttrDataType      //Data type
    qtssAttrPermissions             = 3,    //read //QTSS_AttrPermission    //Permissions

    qtssAttrInfoNumParams           = 4
};
typedef uint32_t QTSS_AttrInfoObjectAttributes;

enum
{
    //QTSS_ConnectedUserObject parameters
    
    //All of these are preemptive safe
    
    qtssConnectionType                  = 0,    //read      //char array    // type of user connection (e.g. "RTP reflected")
    qtssConnectionCreateTimeInMsec      = 1,    //read      //QTSS_TimeVal  //Time in milliseconds the session was created.
    qtssConnectionTimeConnectedInMsec   = 2,    //read      //QTSS_TimeVal  //Time in milliseconds the session was created.
    qtssConnectionBytesSent             = 3,    //read      //uint32_t        //Number of RTP bytes sent so far on this session.
    qtssConnectionMountPoint            = 4,    //read      //char array    //Presentation URL for this session. This URL is the "base" URL for the session. RTSP requests to this URL are assumed to affect all streams on the session.
    qtssConnectionHostName              = 5,    //read      //char array    //host name for this request

    qtssConnectionSessRemoteAddrStr     = 6,    //read      //char array        //IP address addr of client, in dotted-decimal format.
    qtssConnectionSessLocalAddrStr      = 7,    //read      //char array        //Ditto, in dotted-decimal format.

    qtssConnectionCurrentBitRate        = 8,    //read          //uint32_t    //Current bit rate of all the streams on this session. This is not an average. In bits per second.
    qtssConnectionPacketLossPercent     = 9,    //read          //Float32   //Current percent loss as a fraction. .5 = 50%. This is not an average.

    qtssConnectionTimeStorage           = 10,   //read          //QTSS_TimeVal  //Internal, use qtssConnectionTimeConnectedInMsec above

    qtssConnectionNumParams             = 11
};
typedef uint32_t QTSS_ConnectedUserObjectAttributes;


/********************************************************************/
// QTSS API ROLES
//
// Each role represents a unique situation in which a module may be
// invoked. Modules must specify which roles they want to be invoked for. 

enum
{
    //RTSP-specific
    QTSS_RTSPFilter_Role =           FOUR_CHARS_TO_INT('f', 'i', 'l', 't'), //filt //Filter all RTSP requests before the server parses them
    QTSS_RTSPAuthenticate_Role =     FOUR_CHARS_TO_INT('a', 't', 'h', 'n'), //athn //Authenticate the RTSP request username.
                                        //Modules may opt to "steal" the request and return a client response.
    QTSS_RTSPRequest_Role =          FOUR_CHARS_TO_INT('r', 'e', 'q', 'u'), //requ //Process an RTSP request & send client response
    QTSS_RTSPSessionClosing_Role =   FOUR_CHARS_TO_INT('s', 'e', 's', 'c'), //sesc //RTSP session is going away

    //RTP-specific
    QTSS_RTPSendPackets_Role =			FOUR_CHARS_TO_INT('s', 'e', 'n', 'd'), //send //Send RTP packets to the client
    
    //RTCP-specific
    QTSS_RTCPProcess_Role =				FOUR_CHARS_TO_INT('r', 't', 'c', 'p'), //rtcp //Process all RTCP packets sent to the server

    //File system roles
    QTSS_OpenFilePreProcess_Role =		FOUR_CHARS_TO_INT('o', 'p', 'p', 'r'),  //oppr
    QTSS_OpenFile_Role =				FOUR_CHARS_TO_INT('o', 'p', 'f', 'l'),  //opfl
    QTSS_AdviseFile_Role =				FOUR_CHARS_TO_INT('a', 'd', 'f', 'l'),  //adfl
    QTSS_ReadFile_Role =				FOUR_CHARS_TO_INT('r', 'd', 'f', 'l'),  //rdfl
    QTSS_CloseFile_Role =				FOUR_CHARS_TO_INT('c', 'l', 'f', 'l'),  //clfl
    QTSS_RequestEventFile_Role =		FOUR_CHARS_TO_INT('r', 'e', 'f', 'l'),  //refl

	//EasyHLSModule
	Easy_HLSOpen_Role	=				FOUR_CHARS_TO_INT('h', 'l', 's', 'o'),  //hlso
	Easy_HLSClose_Role	=				FOUR_CHARS_TO_INT('h', 'l', 's', 'c'),  //hlsc
    
	//EasyCMSModule
	Easy_CMSFreeStream_Role	=			FOUR_CHARS_TO_INT('e', 'f', 's', 'r'),  //efsr

	//EasyRedisModule
	Easy_RedisSetRTSPLoad_Role =		FOUR_CHARS_TO_INT('c', 'r', 'n', 'r'),	//crnr
	Easy_RedisUpdateStreamInfo_Role =	FOUR_CHARS_TO_INT('a', 'p', 'n', 'r'),	//apnr
	Easy_RedisTTL_Role =				FOUR_CHARS_TO_INT('t', 't', 'l', 'r'),	//ttlr
	Easy_RedisGetAssociatedCMS_Role =	FOUR_CHARS_TO_INT('g', 'a', 'c', 'r'),	//gacr
	Easy_RedisJudgeStreamID_Role =		FOUR_CHARS_TO_INT('j', 's', 'i', 'r'),	//jsir

	//RESTful
	Easy_LiveDeviceStream_Role =		FOUR_CHARS_TO_INT('l', 'd', 's', 'r'),	//ldsr
};
typedef uint32_t QTSS_Role;


//***********************************************/
// TYPEDEFS

typedef void*           QTSS_StreamRef;
typedef void*           QTSS_ServiceFunctionArgsPtr;
typedef int32_t          QTSS_AttributeID;
typedef int32_t          QTSS_ServiceID;
typedef int64_t          QTSS_TimeVal;

typedef QTSS_StreamRef          QTSS_FileStream;
typedef QTSS_StreamRef          QTSS_RTSPSessionStream;
typedef QTSS_StreamRef          QTSS_RTSPRequestStream;
typedef QTSS_StreamRef          QTSS_RTPStreamStream;
typedef QTSS_StreamRef          QTSS_SocketStream;

typedef QTSS_RTSPStatusCode QTSS_SessionStatusCode;

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
	RTSPRequest*                inRTSPRequest;
    char**                      outNewRequest;

} QTSS_Filter_Params;

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
    QTSS_Filter_Params                  rtspFilterParams;
	QTSS_StandardRTSP_Params            rtspParams;

    QTSS_RTPSendPackets_Params          rtpSendPacketsParams;

} QTSS_RoleParams, *QTSS_RoleParamPtr;

typedef struct
{
    void*                           packetData;
    QTSS_TimeVal                    packetTransmitTime;
    QTSS_TimeVal                    suggestedWakeupTime;
} QTSS_PacketStruct;


/********************************************************************/
// ENTRYPOINTS & FUNCTION TYPEDEFS

// MAIN ENTRYPOINT FOR MODULES
//
// Every QTSS API must implement two functions: a main entrypoint, and a dispatch
// function. The main entrypoint gets called by the server at startup to do some
// initialization. Your main entrypoint must follow the convention established below
//
// QTSS_Error mymodule_main(void* inPrivateArgs)
// {
//      return _stublibrary_main(inPrivateArgs, MyDispatchFunction);
// }
//
//

typedef QTSS_Error (*QTSS_MainEntryPointPtr)(void* inPrivateArgs);
typedef QTSS_Error (*QTSS_DispatchFuncPtr)(QTSS_Role inRole, QTSS_RoleParamPtr inParamBlock);
                                                                                                                                                                   

/*****************************************/
//  SERVICES
//
//  Oftentimes modules have functionality that they want accessable from other
//  modules. An example of this might be a logging module that allows other
//  modules to write messages to the log.
//
//  Modules can use the following callbacks to register and invoke "services".
//  Adding & finding services works much like adding & finding attributes in
//  an object. A service has a name. In order to invoke a service, the calling
//  module must know the name of the service and resolve that name into an ID.
//
//  Each service has a parameter block format that is specific to that service.
//  Modules that are exporting services should carefully document the services they
//  export, and modules calling services should take care to fail gracefully
//  if the service isn't present or returns an error.

typedef QTSS_Error (*QTSS_ServiceFunctionPtr)(QTSS_ServiceFunctionArgsPtr);

/********************************************************************/
//  BUILT-IN SERVICES
//
//  The server registers some built-in services when it starts up.
//  Here are macros for their names & descriptions of what they do

// Rereads the preferences, also causes the QTSS_RereadPrefs_Role to be invoked
#define QTSS_REREAD_PREFS_SERVICE   "RereadPreferences"

#endif
