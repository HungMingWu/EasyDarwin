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

#ifndef OSHeaders_H
#define OSHeaders_H
#include <limits.h>

/* Platform-specific components */
#if __MacOSX__
    
    /* Defines */
    #define _64BITARG_ "ll"
    #define _S64BITARG_ "lld"
    #define _U64BITARG_ "llu"
    
#if __LP64__
	#define _S32BITARG_ "d"
	#define _U32BITARG_ "u"
        #define _SPOINTERSIZEARG_ _S64BITARG_
        #define _UPOINTERSIZEARG_ _U64BITARG_
#else
	#define _S32BITARG_ "ld"
	#define _U32BITARG_ "lu"
        #define _SPOINTERSIZEARG_ _S32BITARG_
        #define _UPOINTERSIZEARG_ _U32BITARG_
#endif

    /* paths */
    #define kEOLString "\n"
    #define kPathDelimiterString "/"
    #define kPathDelimiterChar '/'
    #define kPartialPathBeginsWithDelimiter 0

    /* Includes */
    #include <sys/types.h>
    
    /* Constants */
    #define QT_TIME_TO_LOCAL_TIME   (-2082844800)
    #define QT_PATH_SEPARATOR       '/'


#include "/System/Library/Frameworks/CoreServices.framework/Headers/../Frameworks/CarbonCore.framework/Headers/MacTypes.h"

#if 0 // old defs we are now using MacTypes.h
    /* Typedefs */
    typedef UInt32              FourCharCode;
    typedef FourCharCode        OSType;
#endif
    
    typedef signed long		    PointerSizedInt;
    typedef unsigned long       PointerSizedUInt;
    

    #ifdef  FOUR_CHARS_TO_INT
    #error Conflicting Macro "FOUR_CHARS_TO_INT"
    #endif

    #define FOUR_CHARS_TO_INT( c1, c2, c3, c4 )  ( c1 << 24 | c2 << 16 | c3 << 8 | c4 )

    #ifdef  TW0_CHARS_TO_INT
    #error Conflicting Macro "TW0_CHARS_TO_INT"
    #endif
        
    #define TW0_CHARS_TO_INT( c1, c2 )  ( c1 << 8 | c2 )

    
#elif __linux__ || __linuxppc__ || __FreeBSD__

    #include <stdint.h>
    
    /* Defines */
    #define _64BITARG_ "q"
    #define _S64BITARG_ "lld"
    #define _U64BITARG_ "llu"
    #if __LP64__
	#define _S32BITARG_ "d"
	#define _U32BITARG_ "u"
        #define _SPOINTERSIZEARG_ _S64BITARG_
        #define _UPOINTERSIZEARG_ _U64BITARG_
    #else
	#define _S32BITARG_ "ld"
	#define _U32BITARG_ "lu"
        #define _SPOINTERSIZEARG_ _S32BITARG_
        #define _UPOINTERSIZEARG_ _U32BITARG_
    #endif

    /* paths */
    #define kEOLString "\n"
    #define kPathDelimiterString "/"
    #define kPathDelimiterChar '/'
    #define kPartialPathBeginsWithDelimiter 0

    /* Includes */
    #include <sys/types.h>
    
    /* Constants */
    #define QT_TIME_TO_LOCAL_TIME   (-2082844800)
    #define QT_PATH_SEPARATOR       '/'

    /* Typedefs */
    typedef signed long         PointerSizedInt;
    typedef unsigned long       PointerSizedUInt;
    
    typedef unsigned int	FourCharCode;
    typedef FourCharCode        OSType;

    #ifdef  FOUR_CHARS_TO_INT
    #error Conflicting Macro "FOUR_CHARS_TO_INT"
    #endif

    #define FOUR_CHARS_TO_INT( c1, c2, c3, c4 )  ( c1 << 24 | c2 << 16 | c3 << 8 | c4 )

    #ifdef  TW0_CHARS_TO_INT
    #error Conflicting Macro "TW0_CHARS_TO_INT"
    #endif
        
    #define TW0_CHARS_TO_INT( c1, c2 )  ( c1 << 8 | c2 )
	#define MAX_PATH 260 






#elif __Win32__
    
    /* Defines */
    #define _64BITARG_ "I64"
    #define _S64BITARG_ "I64d"
    #define _U64BITARG_ "I64u"
#if __LP64__
	#define _S32BITARG_ "d"
	#define _U32BITARG_ "u"
        #define _SPOINTERSIZEARG_ _S64BITARG_
        #define _UPOINTERSIZEARG_ _U64BITARG_
#else
	#define _S32BITARG_ "d"
	#define _U32BITARG_ "u"
        #define _SPOINTERSIZEARG_ _S32BITARG_
        #define _UPOINTERSIZEARG_ _U32BITARG_
#endif

    /* paths */
    #define kEOLString "\r\n"
    #define kPathDelimiterString "/"
    #define kPathDelimiterChar '/'
    #define kPartialPathBeginsWithDelimiter 0
    
    #define crypt(buf, salt) ((char*)buf)
    
    /* Includes */
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
	#include <mmsystem.h>
    #include <winsock2.h>
    #include <mswsock.h>
    #include <process.h>
    #include <ws2tcpip.h>
    #include <io.h>
    #include <direct.h>
    #include <errno.h>

    
    #define R_OK 0
    #define W_OK 1
        
    // POSIX errorcodes
	#ifndef ENOTCONN
		#define ENOTCONN 1002
	#endif
	#ifndef EADDRINUSE
		#define EADDRINUSE 1004
	#endif
	#ifndef EINPROGRESS
		#define EINPROGRESS 1007
	#endif
	#ifndef ENOBUFS
		#define ENOBUFS 1008
	#endif
	#ifndef EADDRNOTAVAIL
		#define EADDRNOTAVAIL 1009
	#endif

    // Winsock does not use iovecs
    struct iovec {
        u_long  iov_len; // this is not the POSIX definition, it is rather defined to be
        char FAR*   iov_base; // equivalent to a WSABUF for easy integration into Win32
    };
    
    /* Constants */
    #define QT_TIME_TO_LOCAL_TIME   (-2082844800)
    #define QT_PATH_SEPARATOR       '/'

    /* Typedefs */
    typedef signed long         PointerSizedInt;
    typedef unsigned long       PointerSizedUInt;
    
    typedef unsigned long       FourCharCode;
    typedef FourCharCode        OSType;

    #ifdef  FOUR_CHARS_TO_INT
    #error Conflicting Macro "FOUR_CHARS_TO_INT"
    #endif

    #define FOUR_CHARS_TO_INT( c1, c2, c3, c4 )  ( c1 << 24 | c2 << 16 | c3 << 8 | c4 )

    #ifdef  TW0_CHARS_TO_INT
    #error Conflicting Macro "TW0_CHARS_TO_INT"
    #endif
        
    #define TW0_CHARS_TO_INT( c1, c2 )  ( c1 << 8 | c2 )

#elif __sgi__
    /* Defines */
    #define _64BITARG_ "ll"
    #define _S64BITARG_ "lld"
    #define _U64BITARG_ "llu"
#if __LP64__
	#define _S32BITARG_ "d"
	#define _U32BITARG_ "u"
#else
	#define _S32BITARG_ "ld"
	#define _U32BITARG_ "lu"
#endif

    /* paths */
    #define kPathDelimiterString "/"
    #define kPathDelimiterChar '/'
    #define kPartialPathBeginsWithDelimiter 0
	#define	kEOLString "\n"

    /* Includes */
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <pthread.h>
    
    /* Constants */
    #define QT_TIME_TO_LOCAL_TIME   (-2082844800)
    #define QT_PATH_SEPARATOR       '/'

    /* Typedefs */

    typedef signed long         PointerSizedInt;
    typedef unsigned long       PointerSizedUInt;
	
	typedef unsigned long		FourCharCode;
	typedef FourCharCode		OSType;

	/* Nulled-out new() for use without memory debugging */
	/* #define NEW(t,c,v) new c v
	#define NEW_ARRAY(t,c,n) new c[n] */

    #define thread_t    pthread_t
    #define cthread_errno() errno

    #ifdef  FOUR_CHARS_TO_INT
    #error Conflicting Macro "FOUR_CHARS_TO_INT"
    #endif

    #define FOUR_CHARS_TO_INT( c1, c2, c3, c4 )  ( c1 << 24 | c2 << 16 | c3 << 8 | c4 )

    #ifdef  TW0_CHARS_TO_INT
    #error Conflicting Macro "TW0_CHARS_TO_INT"
    #endif
        
    #define TW0_CHARS_TO_INT( c1, c2 )  ( c1 << 8 | c2 )

#elif defined(sun) // && defined(sparc)

    /* Defines */
    #define _64BITARG_ "ll"
    #define _S64BITARG_ "Ild"
    #define _U64BITARG_ "llu"
#if __LP64__
	#define _S32BITARG_ "d"
	#define _U32BITARG_ "u"
#else
	#define _S32BITARG_ "ld"
	#define _U32BITARG_ "lu"
#endif

    /* paths */
    #define kPathDelimiterString "/"
    #define kPathDelimiterChar '/'
    #define kPartialPathBeginsWithDelimiter 0
    #define kEOLString "\n"

    /* Includes */
    #include <sys/types.h>
    #include <sys/byteorder.h>
    
    /* Constants */
    #define QT_TIME_TO_LOCAL_TIME   (-2082844800)
    #define QT_PATH_SEPARATOR       '/'

    /* Typedefs */

    typedef signed long         PointerSizedInt;
    typedef unsigned long       PointerSizedUInt;
    
    typedef unsigned long       FourCharCode;
    typedef FourCharCode        OSType;

    #ifdef  FOUR_CHARS_TO_INT
    #error Conflicting Macro "FOUR_CHARS_TO_INT"
    #endif

    #define FOUR_CHARS_TO_INT( c1, c2, c3, c4 )  ( c1 << 24 | c2 << 16 | c3 << 8 | c4 )

    #ifdef  TW0_CHARS_TO_INT
    #error Conflicting Macro "TW0_CHARS_TO_INT"
    #endif
        
    #define TW0_CHARS_TO_INT( c1, c2 )  ( c1 << 8 | c2 )

#elif defined(__hpux__)

    /* Defines */
    #define _64BITARG_ "ll"
    #define _S64BITARG_ "Ild"
    #define _U64BITARG_ "llu"
#if __LP64__
	#define _S32BITARG_ "d"
	#define _U32BITARG_ "u"
#else
	#define _S32BITARG_ "ld"
	#define _U32BITARG_ "lu"
#endif

    /* paths */
    #define kPathDelimiterString "/"
    #define kPathDelimiterChar '/'
    #define kPartialPathBeginsWithDelimiter 0
    #define kEOLString "\n"

    /* Includes */
    #include <sys/types.h>
    #include <sys/byteorder.h>

    /* Constants */
    #define QT_TIME_TO_LOCAL_TIME   (-2082844800)
    #define QT_PATH_SEPARATOR       '/'

    /* Typedefs */

    typedef signed long         PointerSizedInt;
    typedef unsigned long       PointerSizedUInt;

    typedef unsigned long       FourCharCode;
    typedef FourCharCode        OSType;

    #ifdef  FOUR_CHARS_TO_INT
    #error Conflicting Macro "FOUR_CHARS_TO_INT"
    #endif

    #define FOUR_CHARS_TO_INT( c1, c2, c3, c4 )  ( c1 << 24 | c2 << 16 | c3 << 8 | c4 )

    #ifdef  TW0_CHARS_TO_INT
    #error Conflicting Macro "TW0_CHARS_TO_INT"
    #endif

    #define TW0_CHARS_TO_INT( c1, c2 )  ( c1 << 8 | c2 )

#elif defined(__osf__)
    
   /* Defines */
    #define _64BITARG_ "l"
    #define _S64BITARG_ "ld"
    #define _U64BITARG_ "lu"
#if __LP64__
	#define _S32BITARG_ "ld"
	#define _U32BITARG_ "lu"
#else
	#define _S32BITARG_ "d"
	#define _U32BITARG_ "u"
#endif

    /* paths */
    #define kEOLString "\n"
    #define kPathDelimiterString "/"
    #define kPathDelimiterChar '/'
    #define kPartialPathBeginsWithDelimiter 0

    /* Includes */
    #include <sys/types.h>
    #include <machine/endian.h>
    
    /* Constants */
    #define QT_TIME_TO_LOCAL_TIME   (-2082844800)
    #define QT_PATH_SEPARATOR       '/'

    /* Typedefs */
    typedef unsigned long       PointerSizedInt;
    
    typedef unsigned int        FourCharCode;
    typedef FourCharCode        OSType;

    #ifdef  FOUR_CHARS_TO_INT
    #error Conflicting Macro "FOUR_CHARS_TO_INT"
    #endif

    #define FOUR_CHARS_TO_INT( c1, c2, c3, c4 )  ( c1 << 24 | c2 << 16 | c3 << 8 | c4 )

    #ifdef  TW0_CHARS_TO_INT
    #error Conflicting Macro "TW0_CHARS_TO_INT"
    #endif
        
    #define TW0_CHARS_TO_INT( c1, c2 )  ( c1 << 8 | c2 )


#endif

#include <stdint.h>
typedef int32_t OS_Error;

enum
{
    OS_NoErr = (OS_Error) 0,
    OS_BadURLFormat = (OS_Error) -100,
    OS_NotEnoughSpace = (OS_Error) -101
};


#endif /* OSHeaders_H */
