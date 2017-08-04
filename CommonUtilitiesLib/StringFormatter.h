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
	 File:       StringFormatter.h

	 Contains:   Utility class for formatting text to a buffer.
				 Construct object with a buffer, then call one
				 of many Put methods to write into that buffer.



 */

#ifndef __STRINGFORMATTER_H__
#define __STRINGFORMATTER_H__

#include <string.h>
#include <boost/utility/string_view.hpp>
#include "StrPtrLen.h"
#include "MyAssert.h"


 //Use a class like the ResizeableStringFormatter if you want a buffer that will dynamically grow
class StringFormatter
{
public:

	//pass in a buffer and length for writing
	StringFormatter(char* buffer, uint32_t length) : fCurrentPut(buffer),
		fStartPut(buffer),
		fEndPut(buffer + length),
		fBytesWritten(0) {}

	StringFormatter(StrPtrLen &buffer) : fCurrentPut(buffer.Ptr),
		fStartPut(buffer.Ptr),
		fEndPut(buffer.Ptr + buffer.Len),
		fBytesWritten(0) {}
	virtual ~StringFormatter() = default;

	void Set(char* buffer, uint32_t length) {
		fCurrentPut = buffer;
		fStartPut = buffer;
		fEndPut = buffer + length;
		fBytesWritten = 0;
	}

	//"erases" all data in the output stream save this number
	void        Reset(uint32_t inNumBytesToLeave = 0)
	{
		fCurrentPut = fStartPut + inNumBytesToLeave;
	}

	//Object does no bounds checking on the buffer. That is your responsibility!
	//Put truncates to the buffer size
	void        Put(const int32_t num);
	void        Put(const char* buffer, uint32_t bufferSize);
	void        Put(const char* str) { Put(str, strlen(str)); }
	void        Put(const StrPtrLen& str) { Put(str.Ptr, str.Len); }
	void        Put(const boost::string_view str) { Put(str.data(), str.length()); }
	void        PutSpace() { PutChar(' '); }
	void        PutEOL() { Put(sEOL, sEOLLen); }
	void        PutChar(char c) { Put(&c, 1); }
	void        PutTerminator() { PutChar('\0'); }

	//Writes a printf style formatted string
	bool		PutFmtStr(const char *fmt, ...);


	//the number of characters in the buffer
	inline uint32_t       GetCurrentOffset();
	inline uint32_t       GetSpaceLeft();
	inline uint32_t       GetTotalBufferSize();
	char*               GetCurrentPtr() { return fCurrentPut; }
	char*               GetBufPtr() { return fStartPut; }

	// Counts total bytes that have been written to this buffer (increments
	// even when the buffer gets reset)
	void                ResetBytesWritten() { fBytesWritten = 0; }
	uint32_t              GetBytesWritten() { return fBytesWritten; }

	inline void         PutFilePath(StrPtrLen* inPath, StrPtrLen* inFileName);
	inline void         PutFilePath(char* inPath, char* inFileName);

	//Return a NEW'd copy of the buffer as a C string
	char* GetAsCString()
	{
		StrPtrLen str(fStartPut, this->GetCurrentOffset());
		return str.GetAsCString();
	}

protected:

	//If you fill up the StringFormatter buffer, this function will get called. By
	//default, the function simply returns false.  But derived objects can clear out the data,
	//reset the buffer, and then returns true.
	//Use the ResizeableStringFormatter if you want a buffer that will dynamically grow.
	//Returns true if the buffer has been resized.
	virtual bool    BufferIsFull(char* /*inBuffer*/, uint32_t /*inBufferLen*/) { return false; }

	char*       fCurrentPut;
	char*       fStartPut;
	char*       fEndPut;

	// A way of keeping count of how many bytes have been written total
	uint32_t fBytesWritten;

	static char*    sEOL;
	static uint32_t   sEOLLen;
};

inline uint32_t StringFormatter::GetCurrentOffset()
{
	Assert(fCurrentPut >= fStartPut);
	return (uint32_t)(fCurrentPut - fStartPut);
}

inline uint32_t StringFormatter::GetSpaceLeft()
{
	Assert(fEndPut >= fCurrentPut);
	return (uint32_t)(fEndPut - fCurrentPut);
}

inline uint32_t StringFormatter::GetTotalBufferSize()
{
	Assert(fEndPut >= fStartPut);
	return (uint32_t)(fEndPut - fStartPut);
}

inline void StringFormatter::PutFilePath(StrPtrLen* inPath, StrPtrLen* inFileName)
{
	if (inPath != nullptr && inPath->Len > 0)
	{
		Put(inPath->Ptr, inPath->Len);
		if (kPathDelimiterChar != inPath->Ptr[inPath->Len - 1])
			Put(kPathDelimiterString);
	}
	if (inFileName != nullptr && inFileName->Len > 0)
		Put(inFileName->Ptr, inFileName->Len);
}

inline void StringFormatter::PutFilePath(char* inPath, char* inFileName)
{
	StrPtrLen pathStr(inPath);
	StrPtrLen fileStr(inFileName);

	PutFilePath(&pathStr, &fileStr);
}

#endif // __STRINGFORMATTER_H__

