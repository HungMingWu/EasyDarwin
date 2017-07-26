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

#include <algorithm>
#include "SDPUtils.h"

#include "StrPtrLen.h"
#include "ResizeableStringFormatter.h"
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

void SDPContainer::Parse()
{
	char*	    validChars = "vosiuepcbtrzkam";
	char        nameValueSeparator = '=';

	bool      valid = true;

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

		fFieldStr[(uint8_t)firstChar] = firstChar;
		switch (firstChar)
		{
		case 'v': fReqLines |= kV;
			break;

		case 's': fReqLines |= kS;
			break;

		case 't': fReqLines |= kT;
			break;

		case 'o': fReqLines |= kO;
			break;

		}

		lineParser.ConsumeUntil(&fieldName, nameValueSeparator);
		if ((fieldName.Len != 1) || (::strchr(validChars, fieldName.Ptr[0]) == nullptr))
		{
			valid = false; // line doesn't begin with one of the valid characters followed by an "="
			break;
		}

		if (!lineParser.Expect(nameValueSeparator))
		{
			valid = false; // line doesn't have the "=" after the first char
			break;
		}

		lineParser.ConsumeUntil(&space, StringParser::sWhitespaceMask);

		if (space.Len != 0)
		{
			valid = false; // line has whitespace after the "=" 
			break;
		}
		boost::string_view lineView(line.Ptr, line.Len);
		if (lineView.empty()) continue;
		fSDPLines.push_back(lineView);
	}

	if (fSDPLines.empty()) // didn't add any lines
	{
		valid = false;
	}
	fValid = valid;

}

void SDPContainer::Initialize()
{
	fValid = false;
	fReqLines = 0;
	::memset(fFieldStr, sizeof(fFieldStr), 0);
}

bool SDPContainer::SetSDPBuffer(boost::string_view sdpBuffer)
{
	Initialize();
	if (!sdpBuffer.empty())
	{
		fSDPBuffer = sdpBuffer;
		Parse();
	}

	return IsSDPBufferValid();
}

bool SDPLineSorter::ValidateSessionHeader(boost::string_view theHeaderLine)
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


char SDPLineSorter::sSessionOrderedLines[] = "vosiuepcbtrzka"; // chars are order dependent: declared by rfc 2327
char SDPLineSorter::sessionSingleLines[] = "vtosiuepcbzk";    // return only 1 of each of these session field types
static boost::string_view sEOL("\r\n");
static boost::string_view sMaxBandwidthTag("b=AS:");

SDPLineSorter::SDPLineSorter(const SDPContainer &rawSDPContainer, float adjustMediaBandwidthPercent)
{
	boost::string_view theMediaSDP = rawSDPContainer.GetMediaSDP();
	if (!theMediaSDP.empty())
	{
		StrPtrLen theMediaV(const_cast<char *>(theMediaSDP.data()), theMediaSDP.length());
		StringParser sdpParser(&theMediaV);
		SDPLine sdpLine;
		bool foundLine = false;
		bool newMediaSection = true;

		while (sdpParser.GetDataRemaining() > 0)
		{
			foundLine = sdpParser.GetThruEOL(&sdpLine);
			if (!foundLine)
			{
				break;
			}
			if (sdpLine.GetHeaderType() == 'm')
				newMediaSection = true;

			if (('b' == sdpLine.GetHeaderType()) && (1.0 != adjustMediaBandwidthPercent))
			{
				StringParser bLineParser(&sdpLine);
				bLineParser.ConsumeUntilDigit();
				auto bandwidth = (uint32_t)(.5 + (adjustMediaBandwidthPercent * (float)bLineParser.ConsumeInteger()));
				if (bandwidth < 1)
					bandwidth = 1;

				char bandwidthStr[10];
				snprintf(bandwidthStr, sizeof(bandwidthStr) - 1, "%"   _U32BITARG_   "", bandwidth);
				bandwidthStr[sizeof(bandwidthStr) - 1] = 0;

				fMediaHeaders += std::string(sMaxBandwidthTag);
				fMediaHeaders += bandwidthStr;
			}
			else
				fMediaHeaders += std::string(sdpLine.Ptr, sdpLine.Len);

			fMediaHeaders += std::string(sEOL);
		}
	}

	std::vector<boost::string_view> fSessionSDP = rawSDPContainer.GetNonMediaLines();

	//printf("\nSession raw Lines:\n"); fSessionSDPContainer.PrintAllLines();

	int16_t numHeaderTypes = sizeof(SDPLineSorter::sSessionOrderedLines) - 1;

	for (int16_t fieldTypeIndex = 0; fieldTypeIndex < numHeaderTypes; fieldTypeIndex++)
	{
		std::vector<boost::string_view> theHeaderLines = 
			FindHeaderTypeLines(fSessionSDP, SDPLineSorter::sSessionOrderedLines[fieldTypeIndex]);

		for (const auto &line : theHeaderLines)
		{
			bool addLine = this->ValidateSessionHeader(line);
			if (addLine)
			{
				fSessionHeaders += std::string(line);
				fSessionHeaders += std::string(sEOL);
			}

			if (nullptr != ::strchr(sessionSingleLines, line[0])) // allow 1 of this type: use first found
				break; // move on to next line type

		}
	}
}

std::string SDPLineSorter::GetSortedSDPStr()
{
	return fSessionHeaders + fMediaHeaders;
}


