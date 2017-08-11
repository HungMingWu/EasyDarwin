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
	 File:       md5digest.h

	 Contains:   Provides a function to calculate the md5 digest
				 given all the authentication parameters.

 */

#ifndef _MD5DIGEST_H_
#define _MD5DIGEST_H_

#include <boost/utility/string_view.hpp>
#include "StrPtrLen.h"
#include <stdint.h>

enum {
	kHashHexLen = 32,
	kHashLen = 16
};

// HashToString allocates memory for hashStr->Ptr 
std::string HashToString(unsigned char aHash[kHashLen]);

// allocates memory for hashA1Hex16Bit->Ptr                   
void CalcMD5HA1(StrPtrLen* userName, StrPtrLen* realm, StrPtrLen* userPassword, StrPtrLen* hashA1Hex16Bit);

std::string CalcHA1(StrPtrLen* algorithm,
	StrPtrLen* userName,
	StrPtrLen* realm,
	StrPtrLen* userPassword,
	StrPtrLen* nonce,
	StrPtrLen* cNonce
);

// allocates memory to hA1->Ptr
void CalcHA1Md5Sess(StrPtrLen* hashA1Hex16Bit, StrPtrLen* nonce, StrPtrLen* cNonce, std::string* hA1);
          
std::string CalcRequestDigest(boost::string_view hA1,
	boost::string_view nonce,
	boost::string_view nonceCount,
	boost::string_view cNonce,
	boost::string_view qop,
	boost::string_view method,
	boost::string_view digestUri,
	boost::string_view hEntity
);

void to64(register char* s, register int32_t v, register int n);

// Doesn't allocate any memory. The size of the result buffer should be nbytes
void MD5Encode(char* pw, char* salt, char* result, int nbytes);

#endif
