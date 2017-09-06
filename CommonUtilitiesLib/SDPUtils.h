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
	std::vector<boost::string_view> fSDPLines;
	boost::string_view fSDPBuffer;
public:
	SDPContainer(boost::string_view sdpBuffer);
	~SDPContainer() = default;
	std::vector<boost::string_view> GetNonMediaLines() const;
	boost::string_view GetMediaSDP() const;
	bool Parse();
	const std::vector<boost::string_view>& GetLines() const { return fSDPLines; }
};

std::string SortSDPLine(const SDPContainer &rawSDPContainer);
#endif