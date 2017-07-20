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
	 File:       StringParser.h

	 Contains:   A couple of handy utilities for parsing a stream.



 */


#ifndef __STRINGPARSER_H__
#define __STRINGPARSER_H__

#include "StrPtrLen.h"
#include "MyAssert.h"

#define STRINGPARSERTESTING 0


class StringParser
{
public:

	StringParser(StrPtrLen* inStream)
		: fStartGet(inStream == nullptr ? nullptr : inStream->Ptr),
		fEndGet(inStream == nullptr ? nullptr : inStream->Ptr + inStream->Len),
		fCurLineNumber(1),
		fStream(inStream)
	{
	}
	~StringParser() {}

	// Built-in masks for common stop conditions
	static uint8_t sDigitMask[];      // stop when you hit a digit
	static uint8_t sWordMask[];       // stop when you hit a word
	static uint8_t sEOLMask[];        // stop when you hit an eol
	static uint8_t sEOLWhitespaceMask[]; // stop when you hit an EOL or whitespace
	static uint8_t sEOLWhitespaceQueryMask[]; // stop when you hit an EOL, ? or whitespace

	static uint8_t sWhitespaceMask[]; // skip over whitespace


	//GetBuffer:
	//Returns a pointer to the string object
	StrPtrLen*      GetStream() { return fStream; }

	//Expect:
	//These functions consume the given token/word if it is in the stream.
	//If not, they return false.
	//In all other situations, true is returned.
	//NOTE: if these functions return an error, the object goes into a state where
	//it cannot be guarenteed to function correctly.
	bool          Expect(char stopChar);
	bool          ExpectEOL();

	//Returns the next word
	void            ConsumeWord(StrPtrLen* outString = nullptr)
	{
		ConsumeUntil(outString, sNonWordMask);
	}

	//Returns all the data before inStopChar
	void            ConsumeUntil(StrPtrLen* outString, char inStopChar);

	//Returns whatever integer is currently in the stream
	uint32_t          ConsumeInteger(StrPtrLen* outString = nullptr);
	float         ConsumeFloat();
	float         ConsumeNPT();

	//Keeps on going until non-whitespace
	void            ConsumeWhitespace()
	{
		ConsumeUntil(nullptr, sWhitespaceMask);
	}

	//Assumes 'stop' is a 255-char array of booleans. Set this array
	//to a mask of what the stop characters are. true means stop character.
	//You may also pass in one of the many prepackaged masks defined above.
	void            ConsumeUntil(StrPtrLen* spl, uint8_t* stop);


	//+ rt 8.19.99
	//returns whatever is avaliable until non-whitespace
	void            ConsumeUntilWhitespace(StrPtrLen* spl = nullptr)
	{
		ConsumeUntil(spl, sEOLWhitespaceMask);
	}

	void            ConsumeUntilDigit(StrPtrLen* spl = nullptr)
	{
		ConsumeUntil(spl, sDigitMask);
	}

	void			ConsumeLength(StrPtrLen* spl, int32_t numBytes);

	void			ConsumeEOL(StrPtrLen* outString);

	//GetThru:
	//Works very similar to ConsumeUntil except that it moves past the stop token,
	//and if it can't find the stop token it returns false
	inline bool       GetThru(StrPtrLen* spl, char stop);
	inline bool       GetThruEOL(StrPtrLen* spl);
	inline bool       ParserIsEmpty(StrPtrLen* outString);
	//Returns the current character, doesn't move past it.
	inline char     PeekFast() { if (fStartGet) return *fStartGet; else return '\0'; }
	char operator[](int i) { Assert((fStartGet + i) < fEndGet); return fStartGet[i]; }

	//Returns some info about the stream
	uint32_t          GetDataParsedLen()
	{
		Assert(fStartGet >= fStream->Ptr); return (uint32_t)(fStartGet - fStream->Ptr);
	}
	uint32_t          GetDataReceivedLen()
	{
		Assert(fEndGet >= fStream->Ptr); return (uint32_t)(fEndGet - fStream->Ptr);
	}
	uint32_t          GetDataRemaining()
	{
		Assert(fEndGet >= fStartGet); return (uint32_t)(fEndGet - fStartGet);
	}
	char*           GetCurrentPosition() { return fStartGet; }
	int         GetCurrentLineNumber() { return fCurLineNumber; }

	// A utility for extracting quotes from the start and end of a parsed
	// string. (Warning: Do not call this method if you allocated your own  
	// pointer for the Ptr field of the StrPtrLen class.) - [sfu]
	// 
	// Not sure why this utility is here and not in the StrPtrLen class - [jm]
	static void UnQuote(StrPtrLen* outString);


#if STRINGPARSERTESTING
	static bool       Test();
#endif

private:

	void        advanceMark();

	//built in masks for some common stop conditions
	static uint8_t sNonWordMask[];

	char*       fStartGet;
	char*       fEndGet;
	int         fCurLineNumber;
	StrPtrLen*  fStream;

};


bool StringParser::GetThru(StrPtrLen* outString, char inStopChar)
{
	ConsumeUntil(outString, inStopChar);
	return Expect(inStopChar);
}

bool StringParser::GetThruEOL(StrPtrLen* outString)
{
	ConsumeUntil(outString, sEOLMask);
	return ExpectEOL();
}

bool StringParser::ParserIsEmpty(StrPtrLen* outString)
{
	if (nullptr == fStartGet || nullptr == fEndGet)
	{
		if (nullptr != outString)
		{
			outString->Ptr = nullptr;
			outString->Len = 0;
		}

		return true;
	}

	Assert(fStartGet <= fEndGet);

	return false; // parser ok to parse
}


#endif // __STRINGPARSER_H__
