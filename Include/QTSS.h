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


// Flags for the qtssRTSPReqAction attribute in a QTSS_RTSPRequestObject.
enum 
{
    qtssActionFlagsNoFlags      = 0x00000000,
    qtssActionFlagsRead         = 0x00000001,
    qtssActionFlagsWrite        = 0x00000002,
    qtssActionFlagsAdmin        = 0x00000004,
    qtssActionFlagsExtended     = 0x40000000,
    qtssActionQTSSExtended      = 0x80000000,
};
typedef uint32_t QTSS_ActionFlags;

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

// Authentication schemes
enum
{
    qtssAuthNone        = 0,
    qtssAuthBasic       = 1,
    qtssAuthDigest      = 2
};
typedef uint32_t  QTSS_AuthScheme;


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
    //QTSS_ServerObject parameters
    
    // These parameters ARE pre-emptive safe.
    
    qtssSvrDefaultDNSName           = 1,    //read		//char array        //The "default" DNS name of the server
    qtssSvrDefaultIPAddr            = 2,    //read		//uint32_t            //The "default" IP address of the server
    qtssSvrServerName               = 3,    //read		//char array        //Name of the server
    qtssSvrRTSPPorts                = 6,    //read		// NOT PREEMPTIVE SAFE!//UInt16         //Indexed parameter: all the ports the server is listening on

    // These parameters are NOT pre-emptive safe, they cannot be accessed
    // via. QTSS_GetValuePtr. Some exceptions noted below
    
    qtssSvrIsOutOfDescriptors       = 9,    //read		//bool            //true if the server has run out of file descriptors, false otherwise

    qtssRTPSvrNumUDPSockets         = 12,   //read      //uint32_t    //Number of UDP sockets currently being used by the server

    qtssRTPSvrTotalPackets          = 19,   //read      //uint64_t    //Total number of bytes served since startup
    
    qtssSvrModuleObjects            = 21,   //read		//this IS PREMPTIVE SAFE!  //QTSS_ModuleObject // A module object representing each module
    qtssSvrDefaultIPAddrStr         = 24,   //read      //char array    //The "default" IP address of the server as a string

    qtssSvrPreferences              = 25,   //read      //QTSS_PrefsObject  // An object representing each the server's preferences
    
    qtssSvrReliableUDPWastageInBytes= 31,   //read      //uint32_t    //Amount of data in the reliable UDP buffers being wasted
    qtssSvrConnectedUsers           = 32,   //r/w       //QTSS_Object   //List of connected user sessions (updated by modules for their sessions)

    qtssSvrNumParams                = 38
};
typedef uint32_t QTSS_ServerAttributes;

enum
{
    //QTSS_PrefsObject parameters

    // Valid in all methods. None of these are pre-emptive safe, so the version
    // of QTSS_GetAttribute that copies data must be used.
    
    // All of these parameters are read-write. 
    
    qtssPrefsRTSPTimeout					= 0,    //"rtsp_timeout"                //uint32_t    //RTSP timeout in seconds sent to the client.
    qtssPrefsRTSPSessionTimeout				= 1,    //"rtsp_session_timeout"        //uint32_t    //Amount of time in seconds the server will wait before disconnecting idle RTSP clients. 0 means no timeout
    qtssPrefsRTPSessionTimeout				= 2,    //"rtp_session_timeout"         //uint32_t    //Amount of time in seconds the server will wait before disconnecting idle RTP clients. 0 means no timeout
    qtssPrefsMaximumConnections				= 3,    //"maximum_connections"         //int32_t    //Maximum # of concurrent RTP connections allowed by the server. -1 means unlimited.
    qtssPrefsMaximumBandwidth				= 4,    //"maximum_bandwidth"           //int32_t    //Maximum amt of bandwidth the server is allowed to serve in K bits. -1 means unlimited.
    qtssPrefsMovieFolder					= 5,    //"movie_folder"           //char array    //Path to the root movie folder
    qtssPrefsRTSPIPAddr						= 6,    //"bind_ip_addr"                //char array    //IP address the server should accept RTSP connections on. 0.0.0.0 means all addresses on the machine.
    qtssPrefsBreakOnAssert					= 7,    //"break_on_assert"             //bool        //If true, the server will break in the debugger when an assert fails.
    qtssPrefsAutoRestart					= 8,    //"auto_restart"                //bool        //If true, the server will automatically restart itself if it crashes.
    qtssPrefsTotalBytesUpdate				= 9,    //"total_bytes_update"          //uint32_t    //Interval in seconds between updates of the server's total bytes and current bandwidth statistics
    qtssPrefsAvgBandwidthUpdate				= 10,   //"average_bandwidth_update"    //uint32_t    //Interval in seconds between computations of the server's average bandwidth
    qtssPrefsSafePlayDuration				= 11,   //"safe_play_duration"          //uint32_t    //Hard to explain... see streamingserver.conf
    qtssPrefsModuleFolder					= 12,   //"module_folder"               //char array    //Path to the module folder

    // There is a compiled-in error log module that loads before all the other modules
    // (so it can log errors from the get-go). It uses these prefs.
    
    qtssPrefsErrorLogName					= 13,   //"error_logfile_name"          //char array        //Name of error log file
    qtssPrefsErrorLogDir					= 14,   //"error_logfile_dir"           //char array        //Path to error log file directory
    qtssPrefsErrorRollInterval				= 15,   //"error_logfile_interval"      //uint32_t    //Interval in days between error logfile rolls
    qtssPrefsMaxErrorLogSize				= 16,   //"error_logfile_size"          //uint32_t    //Max size in bytes of the error log
    qtssPrefsScreenLogging					= 18,   //"screen_logging"              //bool        //Should the error logger echo messages to the screen?
    qtssPrefsErrorLogEnabled				= 19,   //"error_logging"               //bool        //Is error logging enabled?

    qtssPrefsDropVideoAllPacketsDelayInMsec = 20,   // "drop_all_video_delay"//int32_t // Don't send video packets later than this
    qtssPrefsStartThinningDelayInMsec       = 21,   // "start_thinning_delay"//int32_t // lateness at which we might start thinning
    qtssPrefsLargeWindowSizeInK             = 22,   // "large_window_size"	// uint32_t    //default size that will be used for high bitrate movies
    qtssPrefsWindowSizeThreshold            = 23,   // "window_size_threshold"  // uint32_t    //bitrate at which we switch to larger window size
    
    qtssPrefsMinTCPBufferSizeInBytes        = 24,   // "min_tcp_buffer_size" //uint32_t    // When streaming over TCP, this is the minimum size the TCP socket send buffer can be set to
    qtssPrefsMaxTCPBufferSizeInBytes        = 25,   // "max_tcp_buffer_size" //uint32_t    // When streaming over TCP, this is the maximum size the TCP socket send buffer can be set to
    qtssPrefsTCPSecondsToBuffer             = 26,   // "tcp_seconds_to_buffer" //Float32 // When streaming over TCP, the size of the TCP send buffer is scaled based on the bitrate of the movie. It will fit all the data that gets sent in this amount of time.
    
    qtssPrefsDefaultAuthorizationRealm      = 28,   // "default_authorization_realm" //char array   //
    
    qtssPrefsRunUserName                    = 29,   // "run_user_name"       //char array        //Run under this user's account
    qtssPrefsRunGroupName                   = 30,   // "run_group_name"      //char array        //Run under this group's account
    
    qtssPrefsSrcAddrInTransport             = 31,   // "append_source_addr_in_transport" // bool   //If true, the server will append the src address to the Transport header responses
    qtssPrefsRTSPPorts                      = 32,   // "rtsp_ports"          // UInt16   

    qtssPrefsMaxRetransDelayInMsec          = 33,   // "max_retransmit_delay" // uint32_t  //maximum interval between when a retransmit is supposed to be sent and when it actually gets sent. Lower values means smoother flow but slower server performance
    qtssPrefsSmallWindowSizeInK             = 34,   // "small_window_size"  // uint32_t    //default size that will be used for low bitrate movies
    qtssPrefsAckLoggingEnabled              = 35,   // "ack_logging_enabled"  // bool  //Debugging only: turns on detailed logging of UDP acks / retransmits
    qtssPrefsRTCPPollIntervalInMsec         = 36,   // "rtcp_poll_interval"      // uint32_t   //interval (in Msec) between poll for RTCP packets
    qtssPrefsRTCPSockRcvBufSizeInK          = 37,   // "rtcp_rcv_buf_size"   // uint32_t   //Size of the receive socket buffer for udp sockets used to receive rtcp packets
    qtssPrefsSendInterval                   = 38,   // "send_interval"  // uint32_t    //
    qtssPrefsThickAllTheWayDelayInMsec      = 39,   // "thick_all_the_way_delay"     // uint32_t   //
    qtssPrefsAltTransportIPAddr             = 40,   // "alt_transport_src_ipaddr"// char     //If empty, the server uses its own IP addr in the source= param of the transport header. Otherwise, it uses this addr.
    qtssPrefsMaxAdvanceSendTimeInSec        = 41,   // "max_send_ahead_time"     // uint32_t   //This is the farthest in advance the server will send a packet to a client that supports overbuffering.
    qtssPrefsReliableUDPSlowStart           = 42,   // "reliable_udp_slow_start" // bool   //Is reliable UDP slow start enabled?
    qtssPrefsEnableCloudPlatform            = 43,   // "enable_cloud_platform"   // bool   
    qtssPrefsAuthenticationScheme           = 44,   // "authentication_scheme" // char   //Set this to be the authentication scheme you want the server to use. "basic", "digest", and "none" are the currently supported values
    qtssPrefsDeleteSDPFilesInterval         = 45,   // "sdp_file_delete_interval_seconds" //uint32_t //Feature rem
    qtssPrefsAutoStart                      = 46,   // "auto_start" //bool //If true, streaming server likes to be started at system startup
    qtssPrefsReliableUDP                    = 47,   // "reliable_udp" //bool //If true, uses reliable udp transport if requested by the client
    qtssPrefsReliableUDPDirs                = 48,   // "reliable_udp_dirs" //CharArray
    qtssPrefsReliableUDPPrintfs             = 49,   // "reliable_udp_printfs" //bool //If enabled, server prints out interesting statistics for the reliable UDP clients
    
    qtssPrefsDropAllPacketsDelayInMsec      = 50,   // "drop_all_packets_delay" // int32_t    // don't send any packets later than this
    qtssPrefsThinAllTheWayDelayInMsec       = 51,   // "thin_all_the_way_delay" // int32_t    // thin to key frames
    qtssPrefsAlwaysThinDelayInMsec          = 52,   // "always_thin_delay" // int32_t         // we always start to thin at this point
    qtssPrefsStartThickingDelayInMsec       = 53,   // "start_thicking_delay" // int32_t      // maybe start thicking at this point
    qtssPrefsQualityCheckIntervalInMsec     = 54,   // "quality_check_interval" // uint32_t    // adjust thinnning params this often   
    qtssPrefsEnableRTSPErrorMessage         = 55,   // "RTSP_error_message" //bool // Appends a content body string error message for reported RTSP errors.
    qtssPrefsEnableRTSPDebugPrintfs         = 56,   // "RTSP_debug_printfs" //Boo1l6 // printfs incoming RTSPRequests and Outgoing RTSP responses.

    qtssPrefsEnableMonitorStatsFile         = 57,   // "enable_monitor_stats_file" //bool //write server stats to the monitor file
    qtssPrefsMonitorStatsFileIntervalSec    = 58,   // "monitor_stats_file_interval_seconds" // private
    qtssPrefsMonitorStatsFileName           = 59,   // "monitor_stats_file_name" // private

    qtssPrefsEnablePacketHeaderPrintfs      = 60,   // "enable_packet_header_printfs" //bool // RTP and RTCP printfs of outgoing packets.
    qtssPrefsPacketHeaderPrintfOptions      = 61,   // "packet_header_printf_options" //char //set of printfs to print. Form is [text option] [;]  default is "rtp;rr;sr;". This means rtp packets, rtcp sender reports, and rtcp receiver reports.
    qtssPrefsOverbufferRate                 = 62,   // "overbuffer_rate"    //Float32
    qtssPrefsMediumWindowSizeInK            = 63,   // "medium_window_size" // uint32_t    //default size that will be used for medium bitrate movies
    qtssPrefsWindowSizeMaxThreshold         = 64,   // "window_size_threshold"  // uint32_t    //bitrate at which we switch from medium to large window size
    qtssPrefsEnableRTSPServerInfo           = 65,   // "RTSP_server_info" //Boo1l6 // Adds server info to the RTSP responses.
    qtssPrefsRunNumThreads                  = 66,   // "run_num_threads" //uint32_t // if value is non-zero, will  create that many task threads; otherwise a thread will be created for each processor
    qtssPrefsPidFile                        = 67,   // "pid_file" //Char Array //path to pid file
    qtssPrefsCloseLogsOnWrite               = 68,   // "force_logs_close_on_write" //bool // force log files to close after each write.
    qtssPrefsDisableThinning                = 69,   // "disable_thinning" //bool // Usually used for performance testing. Turn off stream thinning from packet loss or stream lateness.
    qtssPrefsPlayersReqRTPHeader            = 70,   // "player_requires_rtp_header_info" //Char array //name of player to match against the player's user agent header
    qtssPrefsPlayersReqBandAdjust           = 71,   // "player_requires_bandwidth_adjustment //Char array //name of player to match against the player's user agent header
    qtssPrefsPlayersReqNoPauseTimeAdjust    = 72,   // "player_requires_no_pause_time_adjustment //Char array //name of player to match against the player's user agent header

	qtssPrefsDefaultStreamQuality           = 73,   // "default_stream_quality //UInt16 //0 is all day and best quality. Higher values are worse maximum depends on the media and the media module
    qtssPrefsPlayersReqRTPStartTimeAdjust   = 74,   // "player_requires_rtp_start_time_adjust" //Char Array //name of players to match against the player's user agent header

	qtssPrefsEnableUDPMonitor               = 75,   // "enable_udp_monitor_stream" //Boo1l6 // reflect all udp streams to the monitor ports, use an sdp to view
    qtssPrefsUDPMonitorAudioPort            = 76,   // "udp_monitor_video_port" //UInt16 // localhost destination port of reflected stream
    qtssPrefsUDPMonitorVideoPort            = 77,   // "udp_monitor_audio_port" //UInt16 // localhost destination port of reflected stream
    qtssPrefsUDPMonitorDestIPAddr           = 78,   // "udp_monitor_dest_ip"    //char array    //IP address the server should send RTP monitor reflected streams. 
    qtssPrefsUDPMonitorSourceIPAddr         = 79,   // "udp_monitor_src_ip"    //char array    //client IP address the server monitor should reflect. *.*.*.* means all client addresses.
    qtssPrefsEnableAllowGuestDefault        = 80,   // "enable_allow_guest_authorize_default" //Boo1l6 // server hint to access modules to allow guest access as the default (can be overriden in a qtaccess file or other means)
    qtssPrefsNumRTSPThreads                 = 81,   // "run_num_rtsp_threads" //uint32_t // if value is non-zero, the server will  create that many task threads; otherwise a single thread will be created.
	
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
typedef void*           QTSS_Object;
typedef void*           QTSS_ServiceFunctionArgsPtr;
typedef int32_t          QTSS_AttributeID;
typedef int32_t          QTSS_ServiceID;
typedef int64_t          QTSS_TimeVal;

typedef QTSS_Object             QTSS_RTPStreamObject;
typedef QTSS_Object             QTSS_PrefsObject;
typedef QTSS_Object             QTSS_FileObject;
typedef QTSS_Object             QTSS_ModulePrefsObject;
typedef QTSS_Object             QTSS_AttrInfoObject;
typedef QTSS_Object             QTSS_ConnectedUserObject;

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

typedef struct
{
    char outModuleName[QTSS_MAX_MODULE_NAME_LENGTH];
} QTSS_Register_Params;

class QTSSStream;

typedef struct
{
	QTSServerInterface*         inServer;           // Global dictionaries
    QTSS_PrefsObject            inPrefs;
} QTSS_Initialize_Params;

typedef struct
{
    char*                       inBuffer;
    
} QTSS_ErrorLog_Params;

typedef struct
{
    QTSS_ServerState            inNewState;
} QTSS_StateChange_Params;

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
	RTSPRequest*                inRTSPRequest;
} QTSS_RTSPAuth_Params;

typedef struct 
{
	RTSPSession*                inRTSPSession;
    RTPSession*                 inClientSession;
    char*                       inPacketData;
    uint32_t                    inPacketLen;

} QTSS_IncomingData_Params;

typedef struct
{
	RTSPSession*                inRTSPSession;
} QTSS_RTSPSession_Params;

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

typedef struct
{
	RTPSession*                 inClientSession;
    QTSS_RTPStreamObject        inRTPStream;
    void*                       inRTCPPacketData;
    uint32_t                      inRTCPPacketDataLen;
} QTSS_RTCPProcess_Params;

typedef struct
{
    char*                       inPath;
    QTSS_OpenFileFlags          inFlags;
    QTSS_Object                 inFileObject;
} QTSS_OpenFile_Params;

typedef struct
{
    QTSS_Object                 inFileObject;
    uint64_t                      inPosition;
    uint32_t                      inSize;
} QTSS_AdviseFile_Params;

typedef struct
{
    QTSS_Object                 inFileObject;
    uint64_t                      inFilePosition;
    void*                       ioBuffer;
    uint32_t                      inBufLen;
    uint32_t*                     outLenRead;
} QTSS_ReadFile_Params;

typedef struct
{
    QTSS_Object                 inFileObject;
} QTSS_CloseFile_Params;

typedef struct
{
    QTSS_Object                 inFileObject;
    QTSS_EventType              inEventMask;
} QTSS_RequestEventFile_Params;

//redis module
typedef struct
{
	char *						inStreamName;
	uint32_t						inChannel;
	uint32_t						inNumOutputs;
	uint32_t						inBitrate;
	Easy_RedisAction			inAction;
}Easy_StreamInfo_Params;

typedef struct
{
	char * inSerial;
	char * outCMSIP;
	char * outCMSPort;
}QTSS_GetAssociatedCMS_Params;

typedef struct  
{
	char * inStreanID;
	char * outresult;
}QTSS_JudgeStreamID_Params;

typedef union
{
    QTSS_ErrorLog_Params                errorParams;
    QTSS_StateChange_Params             stateChangeParams;

    QTSS_Filter_Params                  rtspFilterParams;
    QTSS_RTSPAuth_Params                rtspAthnParams;
	QTSS_StandardRTSP_Params            rtspParams;
    QTSS_RTSPSession_Params             rtspSessionClosingParams;

    QTSS_RTPSendPackets_Params          rtpSendPacketsParams;
    QTSS_RTCPProcess_Params             rtcpProcessParams;
    
    QTSS_OpenFile_Params                openFilePreProcessParams;
    QTSS_OpenFile_Params                openFileParams;
    QTSS_AdviseFile_Params              adviseFileParams;
    QTSS_ReadFile_Params                readFileParams;
    QTSS_CloseFile_Params               closeFileParams;
    QTSS_RequestEventFile_Params        reqEventFileParams;

	Easy_StreamInfo_Params              easyStreamInfoParams;
	QTSS_GetAssociatedCMS_Params	    GetAssociatedCMSParams;
	QTSS_JudgeStreamID_Params			JudgeStreamIDParams;

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
                                                                                                           
/********************************************************************/
//  QTSS_AddStaticAttribute
//
//  Adds a new static attribute to a predefined object type. All added attributes implicitly have
//  qtssAttrModeRead, qtssAttrModeWrite, and qtssAttrModePreempSafe permissions. "inUnused" should
//  always be NULL. Specify the data type and name of the attribute.
//
//  This may only be called from the QTSS_Register role.
//
//  Returns:    QTSS_NoErr
//              QTSS_OutOfState: If this function isn't being called from the Register role
//              QTSS_BadArgument:   Adding an attribute to a nonexistent object type, attribute
//                      name too long, or NULL arguments.
//              QTSS_AttrNameExists: The name must be unique.
QTSS_Error QTSS_AddStaticAttribute( QTSS_ObjectType inObjectType, char* inAttrName,
                void* inUnused, QTSS_AttrDataType inAttrDataType);
                
/********************************************************************/
//  QTSS_AddInstanceAttribute
//
//  Adds a new instance attribute to a predefined object type. All added attributes implicitly have
//  qtssAttrModeRead, qtssAttrModeWrite, and qtssAttrModePreempSafe permissions. "inUnused" should
//  always be NULL. Specify the data type and name of the attribute.
//
//  This may be called at any time.
//
//  Returns:    QTSS_NoErr
//              QTSS_OutOfState: If this function isn't being called from the Register role
//              QTSS_BadArgument:   Adding an attribute to a nonexistent object type, attribute
//                      name too long, or NULL arguments.
//              QTSS_AttrNameExists: The name must be unique.
QTSS_Error QTSS_AddInstanceAttribute(   QTSS_Object inObject, char* inAttrName,
        void* inUnused, QTSS_AttrDataType inAttrDataType);
                                        
/********************************************************************/
//  QTSS_RemoveInstanceAttribute
//
//  Removes an existing instance attribute. This may be called at any time
//
//  Returns:    QTSS_NoErr
//              QTSS_OutOfState: If this function isn't being called from the Register role
//              QTSS_BadArgument:   Bad object type.
//              QTSS_AttrDoesntExist: Bad attribute ID
QTSS_Error QTSS_RemoveInstanceAttribute(QTSS_Object inObject, QTSS_AttributeID inID);

/********************************************************************/
//  Getting attribute information
//
//  The following callbacks allow modules to discover at runtime what
//  attributes exist in which objects and object types, and discover
//  all attribute meta-data

/********************************************************************/
//  QTSS_IDForAttr
//
//  Given an attribute name, this returns its accompanying attribute ID.
//  The ID can in turn be used to retrieve the attribute value from
//  a object. This callback applies only to static attributes 
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument: Bad argument
QTSS_Error QTSS_IDForAttr(QTSS_ObjectType inObjectType, const char* inAttributeName,
                            QTSS_AttributeID* outID);

/********************************************************************/
//  QTSS_GetAttrInfoByName
//
//  Searches for an attribute with the specified name in the specified object.
//  If found, this function returns a QTSS_AttrInfoObject describing the attribute.
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument
//              QTSS_AttrDoesntExist
QTSS_Error QTSS_GetAttrInfoByName(QTSS_Object inObject, char* inAttrName,
                                    QTSS_AttrInfoObject* outAttrInfoObject);

/********************************************************************/
//  QTSS_SetValue
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument: Bad argument
//              QTSS_ReadOnly: Attribute is read only.
//              QTSS_BadIndex: Attempt to set non-0 index of attribute with a param retrieval function.
//
QTSS_Error QTSS_SetValue (QTSS_Object inObject, QTSS_AttributeID inID, uint32_t inIndex, const void* inBuffer,  uint32_t inLen);

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
//  QTSS_AddService
//
//  This function registers a service with the specified name, and
//  associates it with the specified function pointer.
//  QTSS_AddService may only be called from the QTSS_Register role
//
//  Returns:    QTSS_NoErr
//              QTSS_OutOfState: If this function isn't being called from the Register role
//              QTSS_BadArgument:   Service name too long, or NULL arguments.
QTSS_Error QTSS_AddService(const char* inServiceName, QTSS_ServiceFunctionPtr inFunctionPtr);


/********************************************************************/
//  QTSS_IDForService
//
//  Much like QTSS_IDForAttr, this resolves a service name into its
//  corresponding QTSS_ServiceID. The QTSS_ServiceID can then be used to
//  invoke the service.
//
//  Returns:    QTSS_NoErr
//              QTSS_BadArgument: Bad argument
QTSS_Error QTSS_IDForService(const char* inTag, QTSS_ServiceID* outID);

/********************************************************************/
//  QTSS_DoService
//
//  Invokes the service. Return value from this function comes from the service
//  function itself, unless the QTSS_IllegalService errorcode is returned,
//  which is returned when the QTSS_ServiceID is bad.
QTSS_Error QTSS_DoService(QTSS_ServiceID inID, QTSS_ServiceFunctionArgsPtr inArgs);

/********************************************************************/
//  BUILT-IN SERVICES
//
//  The server registers some built-in services when it starts up.
//  Here are macros for their names & descriptions of what they do

// Rereads the preferences, also causes the QTSS_RereadPrefs_Role to be invoked
#define QTSS_REREAD_PREFS_SERVICE   "RereadPreferences"

/*****************************************/
//  ASYNC I/O CALLBACKS
//
//  QTSS modules must be kind in how they use the CPU. The server doesn't
//  prevent a poorly implemented QTSS module from hogging the processing
//  capability of the server, at the expense of other modules and other clients.
//
//  It is therefore imperitive that a module use non-blocking, or async, I/O.
//  If a module were to block, say, waiting to read file data off disk, this stall
//  would affect the entire server.
//
//  This problem is resolved in QTSS API in a number of ways.
//
//  Firstly, all QTSS_StreamRefs provided to modules are non-blocking, or async.
//  Modules should be prepared to receive EWOULDBLOCK errors in response to
//  QTSS_Read, QTSS_Write, & QTSS_WriteV calls, with certain noted exceptions
//  in the case of responding to RTSP requests.
//
//  Modules that open their own file descriptors for network or file I/O can
//  create separate threads for handling I/O. In this case, these descriptors
//  can remain blocking, as long as they always block on the private module threads.
//
//  In most cases, however, creating a separate thread for I/O is not viable for the
//  kind of work the module would like to do. For instance, a module may wish
//  to respond to a RTSP DESCRIBE request, but can't immediately because constructing
//  the response would require I/O that would block.
//
//  The problem is once the module returns from the QTSS_RTSPProcess_Role, the
//  server will mistakenly consider the request handled, and move on. It won't
//  know that the module has more work to do before it finishes processing the DESCRIBE.
//
//  In this case, the module needs to tell the server to delay processing of the
//  DESCRIBE request until the file descriptor's blocking condition is lifted.
//  The module can do this by using the provided "event" callback routines.

//  Returns:    QTSS_NoErr
//              QTSS_BadArgument: Bad argument
//              QTSS_OutOfState: if this callback is made from a role that doesn't allow async I/O events
//              QTSS_RequestFailed: Not currently possible to request an event. 

QTSS_Error  QTSS_SetIdleTimer(int64_t inIdleMsec);

#endif
