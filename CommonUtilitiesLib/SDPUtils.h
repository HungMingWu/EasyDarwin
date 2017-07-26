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
  File:       SDPUtils.h

  Contains:   Some static routines for dealing with SDPs


  */

#ifndef __SDPUtilsH__
#define __SDPUtilsH__

#include <vector>
#include <boost/utility/string_view.hpp>
#include "OS.h"
#include "StrPtrLen.h"
#include "StringParser.h"

class SDPLine : public StrPtrLen
{
public:
	SDPLine() = default;
	~SDPLine() override = default;

	char    GetHeaderType() { if (Ptr && Len) return this->Ptr[0]; return 0; }
};

class SDPContainer
{
	enum { kBaseLines = 20, kLineTypeArraySize = 256 };

	enum {
		kVPos = 0,
		kSPos,
		kTPos,
		kOPos
	};

	enum {
		kV = 1 << kVPos,
		kS = 1 << kSPos,
		kT = 1 << kTPos,
		kO = 1 << kOPos,
		kAllReq = kV | kS | kT | kO
	};

	std::vector<boost::string_view> fSDPLines;
	boost::string_view fSDPBuffer;
public:

	SDPContainer()
	{
		Initialize();
	}

	~SDPContainer() = default;
	void		Initialize();
	std::vector<boost::string_view> GetNonMediaLines() const;
	boost::string_view GetMediaSDP() const;
	boost::string_view GetFullSDP() const { return fSDPBuffer; }
	void        Parse();
	bool      SetSDPBuffer(boost::string_view sdpBuffer);
	bool      IsSDPBufferValid() { return fValid; }
	bool      HasReqLines() { return (bool)(fReqLines == kAllReq); }
	bool      HasLineType(char lineType) { return (bool)(lineType == fFieldStr[(uint8_t)lineType]); }
	char*       GetReqLinesArray;
	const std::vector<boost::string_view>& GetLines() const { return fSDPLines; }
	
	bool      fValid;
	uint16_t      fReqLines;

	char        fFieldStr[kLineTypeArraySize]; // 
	char*       fLineSearchTypeArray;
};



class SDPLineSorter {
	bool ValidateSessionHeader(boost::string_view theHeaderLine);
	std::string fSessionHeaders;
	std::string fMediaHeaders;

	static char sSessionOrderedLines[];// = "vosiuepcbtrzka"; // chars are order dependent: declared by rfc 2327
	static char sessionSingleLines[];//  = "vosiuepcbzk";    // return only 1 of each of these session field types
public:
	SDPLineSorter(const SDPContainer &rawSDPContainer, float adjustMediaBandwidthPercent = 1.0);

	boost::string_view GetSessionHeaders() { return fSessionHeaders; }
	boost::string_view GetMediaHeaders() { return fMediaHeaders; }
	std::string GetSortedSDPStr();
};

#endif