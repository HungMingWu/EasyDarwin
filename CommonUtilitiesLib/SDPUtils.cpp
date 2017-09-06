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

#include <initializer_list>
#include <vector>
#include <algorithm>
#include "SDPUtils.h"

#include "StrPtrLen.h"
#include "StringParser.h"

static std::vector<boost::string_view> 
FindHeaderTypeLines(const std::vector<boost::string_view> &fSDPLines, char id)
{
	std::vector<boost::string_view> result;
	std::copy_if(fSDPLines.begin(), fSDPLines.end(), std::back_inserter(result),
		[id](const boost::string_view hdr) {
		return hdr[0] == id;
	});
	return result;
}

std::vector<boost::string_view> SDPContainer::GetNonMediaLines() const
{
	std::vector<boost::string_view> result;
	for (const auto &line : fSDPLines) {
		if (line[0] == 'm') break;
		result.push_back(line);
	}
	return result;
}

boost::string_view SDPContainer::GetMediaSDP() const
{
	auto it = std::find_if(fSDPLines.begin(), fSDPLines.end(), [](const boost::string_view hdr) {
		return hdr[0] == 'm';
	});
	if (it == fSDPLines.end()) return {};
	auto spiltLine = *it;
	return boost::string_view(spiltLine.data(), 
		fSDPBuffer.length() - (size_t)(spiltLine.data() - fSDPBuffer.data()));
}

SDPContainer::SDPContainer(boost::string_view sdpBuffer) :fSDPBuffer(sdpBuffer)
{
	Parse();
}

bool SDPContainer::Parse()
{
	char*	    validChars = "vosiuepcbtrzkam";
	char        nameValueSeparator = '=';

	StrPtrLen fSDPBufferV(const_cast<char *>(fSDPBuffer.data()), fSDPBuffer.length());
	StringParser	sdpParser(&fSDPBufferV);
	StrPtrLen		line;
	StrPtrLen 		fieldName;
	StrPtrLen		space;
	bool          foundLine = false;

	while (sdpParser.GetDataRemaining() != 0)
	{
		foundLine = sdpParser.GetThruEOL(&line);  // Read each line  
		if (!foundLine)
		{
			break;
		}
		StringParser lineParser(&line);

		lineParser.ConsumeWhitespace();//skip over leading whitespace
		if (lineParser.GetDataRemaining() == 0) // must be an empty line
			continue;

		char firstChar = lineParser.PeekFast();
		if (firstChar == '\0')
			continue; //skip over blank lines

		lineParser.ConsumeUntil(&fieldName, nameValueSeparator);
		if ((fieldName.Len != 1) || (::strchr(validChars, fieldName.Ptr[0]) == nullptr))
			return false; // line doesn't begin with one of the valid characters followed by an "="

		if (!lineParser.Expect(nameValueSeparator))
			return false; // line doesn't have the "=" after the first char

		lineParser.ConsumeUntil(&space, StringParser::sWhitespaceMask);

		if (space.Len != 0)
			return false; // line has whitespace after the "=" 

		boost::string_view lineView(line.Ptr, line.Len);
		if (lineView.empty()) continue;
		fSDPLines.push_back(lineView);
	}

	return !fSDPLines.empty();
}

static bool ValidateSessionHeader(boost::string_view theHeaderLine, boost::string_view fSessionHeaders)
{
	if (theHeaderLine.empty())
		return false;

	// check for a duplicate range line.
	bool found1 = theHeaderLine.find("a=range") != boost::string_view::npos;
	bool found2 = fSessionHeaders.find("a=range") != std::string::npos;
	if (found1 && found2)
		return false;

	return true;
}

// chars are order dependent: declared by rfc 2327
static std::initializer_list<char> SDPLineSessionOrdered = { 
	'v', 'o', 's', 'i', 'u', 'e', 'p', 'c', 'b', 't', 'r', 'z', 'k', 'a' 
};

// return only 1 of each of these session field types
static std::initializer_list<char> SDPLineSessionSingle = {
	'v', 't', 'o', 's', 'i', 'u', 'e', 'p', 'c', 'b', 'z', 'k'
};

static boost::string_view sEOL("\r\n");

std::string SortSDPLine(const SDPContainer &rawSDPContainer)
{
	std::string fSessionHeaders, fMediaHeaders(rawSDPContainer.GetMediaSDP());
	std::vector<boost::string_view> fSessionSDP = rawSDPContainer.GetNonMediaLines();

	//printf("\nSession raw Lines:\n"); fSessionSDPContainer.PrintAllLines();

	for (const auto &fieldType : SDPLineSessionOrdered)
	{
		std::vector<boost::string_view> theHeaderLines = FindHeaderTypeLines(fSessionSDP, fieldType);

		for (const auto &line : theHeaderLines)
		{
			if (ValidateSessionHeader(line, fSessionHeaders))
				fSessionHeaders += std::string(line) + std::string(sEOL);;

			// allow 1 of this type: use first found
			// move on to next line type
			auto it = std::find(begin(SDPLineSessionSingle), end(SDPLineSessionSingle), line[0]);
			if (it != end(SDPLineSessionSingle))
				break;
		}
	}
	return std::move(fSessionHeaders) + std::move(fMediaHeaders);
}