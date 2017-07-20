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
	 File:       StrPtrLen.h

	 Contains:   Definition of class that tracks a string ptr and a length.
				 Note: this is NOT a string container class! It is a string PTR container
				 class. It therefore does not copy the string and store it internally. If
				 you deallocate the string to which this object points to, and continue
				 to use it, you will be in deep doo-doo.

				 It is also non-encapsulating, basically a struct with some simple methods.


 */

#ifndef __STRPTRLEN_H__
#define __STRPTRLEN_H__

#include <string.h>
#include <stdint.h>
#include "OSHeaders.h"
#include <ctype.h> 
#include "MyAssert.h"
#include "SafeStdLib.h"

#define STRPTRLENTESTING 0

class StrPtrLen
{
public:

	//CONSTRUCTORS/DESTRUCTOR
	//These are so tiny they can all be inlined
	StrPtrLen() : Ptr(nullptr), Len(0) {}
	StrPtrLen(char* sp) : Ptr(sp), Len(sp != nullptr ? strlen(sp) : 0) {}
	StrPtrLen(char* sp, uint32_t len) : Ptr(sp), Len(len) {}
	virtual ~StrPtrLen() {}

	//OPERATORS:
	bool Equal(const StrPtrLen &compare) const;
	bool EqualIgnoreCase(const char* compare, const uint32_t len) const;
	bool EqualIgnoreCase(const StrPtrLen &compare) const { return EqualIgnoreCase(compare.Ptr, compare.Len); }
	bool Equal(const char* compare) const;
	bool NumEqualIgnoreCase(const char* compare, const uint32_t len) const;

	void Delete() { delete[] Ptr; Ptr = nullptr; Len = 0; }
	char* ToUpper() { for (uint32_t x = 0; x < Len; x++) Ptr[x] = toupper(Ptr[x]); return Ptr; }

	char* FindStringCase(char* queryCharStr, StrPtrLen* resultStr, bool caseSensitive) const;

	char* FindString(StrPtrLen* queryStr, StrPtrLen* outResultStr)
	{
		Assert(queryStr != nullptr);   Assert(queryStr->Ptr != nullptr); Assert(0 == queryStr->Ptr[queryStr->Len]);
		return FindStringCase(queryStr->Ptr, outResultStr, true);
	}

	char* FindStringIgnoreCase(StrPtrLen* queryStr, StrPtrLen* outResultStr)
	{
		Assert(queryStr != nullptr);   Assert(queryStr->Ptr != nullptr); Assert(0 == queryStr->Ptr[queryStr->Len]);
		return FindStringCase(queryStr->Ptr, outResultStr, false);
	}

	char* FindString(StrPtrLen* queryStr)
	{
		Assert(queryStr != nullptr);   Assert(queryStr->Ptr != nullptr); Assert(0 == queryStr->Ptr[queryStr->Len]);
		return FindStringCase(queryStr->Ptr, nullptr, true);
	}

	char* FindStringIgnoreCase(StrPtrLen* queryStr)
	{
		Assert(queryStr != nullptr);   Assert(queryStr->Ptr != nullptr); Assert(0 == queryStr->Ptr[queryStr->Len]);
		return FindStringCase(queryStr->Ptr, nullptr, false);
	}

	char* FindString(char* queryCharStr) { return FindStringCase(queryCharStr, nullptr, true); }
	char* FindStringIgnoreCase(char* queryCharStr) { return FindStringCase(queryCharStr, nullptr, false); }
	char* FindString(char* queryCharStr, StrPtrLen* outResultStr) { return FindStringCase(queryCharStr, outResultStr, true); }
	char* FindStringIgnoreCase(char* queryCharStr, StrPtrLen* outResultStr) { return FindStringCase(queryCharStr, outResultStr, false); }

	char* FindString(StrPtrLen& query, StrPtrLen* outResultStr) { return FindString(&query, outResultStr); }
	char* FindStringIgnoreCase(StrPtrLen& query, StrPtrLen* outResultStr) { return FindStringIgnoreCase(&query, outResultStr); }
	char* FindString(StrPtrLen& query) { return FindString(&query); }
	char* FindStringIgnoreCase(StrPtrLen& query) { return FindStringIgnoreCase(&query); }

	StrPtrLen& operator=(const StrPtrLen& newStr)
	{
		Ptr = newStr.Ptr; Len = newStr.Len;
		return *this;
	}
	StrPtrLen(const StrPtrLen& newStr) { Ptr = newStr.Ptr; Len = newStr.Len; }
	char operator[](int i) { /*Assert(i<Len);i*/ return Ptr[i]; }
	void Set(char* inPtr, uint32_t inLen) { Ptr = inPtr; Len = inLen; }
	void Set(char* inPtr) { Ptr = inPtr; Len = (inPtr) ? ::strlen(inPtr) : 0; }

	//This is a non-encapsulating interface. The class allows you to access its
	//data.
	char*       Ptr;
	uint32_t      Len;

	// convert to a "NEW'd" zero terminated char array
	char*   GetAsCString() const;
	void    PrintStr();
	void    PrintStr(char *appendStr);
	void    PrintStr(char* prependStr, char *appendStr);

	void    PrintStrEOL(char* stopStr = nullptr, char *appendStr = nullptr); //replace chars x0A and x0D with \r and \n

	//Utility function
	uint32_t    TrimTrailingWhitespace();
	uint32_t    TrimLeadingWhitespace();

	uint32_t  RemoveWhitespace();
	void  TrimWhitespace() { TrimLeadingWhitespace(); TrimTrailingWhitespace(); }

#if STRPTRLENTESTING
	static bool   Test();
#endif

private:

	static uint8_t    sCaseInsensitiveMask[];
	static uint8_t    sNonPrintChars[];
};



class StrPtrLenDel : public StrPtrLen
{
public:
	StrPtrLenDel() : StrPtrLen() {}
	StrPtrLenDel(char* sp) : StrPtrLen(sp) {}
	StrPtrLenDel(char* sp, uint32_t len) : StrPtrLen(sp, len) {}
	~StrPtrLenDel() { Delete(); }
};

#endif // __STRPTRLEN_H__
