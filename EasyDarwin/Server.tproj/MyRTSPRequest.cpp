#include <algorithm>
#include <iterator>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/utility/string_view.hpp>
#include <boost/spirit/include/qi.hpp>
#include "MyRTSPRequest.h"

namespace qi = boost::spirit::qi;

template <typename S>
static std::vector<std::string> spirit_direct(const S& input, char const* delimiter)
{
	std::vector<std::string> result;
	if (!qi::parse(input.begin(), input.end(),
		qi::raw[*(qi::char_ - qi::char_(delimiter))] % qi::char_(delimiter), result))
		result.push_back(std::string(input));
	return result;
}

bool RequestMessage::ParseNetworkModeSubHeader(boost::string_view inSubHeader, MyRTSPRequest& req)
{
	static boost::string_view sUnicast("unicast");
	static boost::string_view sMulticast("multiicast");

	if (boost::iequals(inSubHeader, sUnicast))
	{
		req.fNetworkMode = qtssRTPNetworkModeUnicast;
		return true;
	}

	if (boost::iequals(inSubHeader, sMulticast))
	{
		req.fNetworkMode = qtssRTPNetworkModeMulticast;
		return true;
	}

	return false;
}

bool RequestMessage::ParseModeSubHeader(boost::string_view inModeSubHeader, MyRTSPRequest& req)
{
	static boost::string_view sModeSubHeader("mode");
	static boost::string_view sReceiveMode("receive");
	static boost::string_view sRecordMode("record");

	std::string name, mode;
	bool r = qi::phrase_parse(inModeSubHeader.cbegin(), inModeSubHeader.cend(),
		*(qi::alpha - "=") >> "=" >> *(qi::char_), qi::ascii::blank,
		name, mode);
	if (r && boost::iequals(name, sModeSubHeader)) {
		if (boost::iequals(mode, sReceiveMode) || boost::iequals(mode, sRecordMode))
			req.fTransportMode = qtssRTPTransportModeRecord;
	}
	return r;
}

bool RequestMessage::parse_setup(MyRTSPRequest& req)
{
	std::vector<std::string> tokens = spirit_direct(req.header["Transport"], ";");
	for (const auto &subHeader : tokens)
	{
		// Extract the relevent information from the relevent subheader.
		// So far we care about 3 sub-headers

		if (!ParseNetworkModeSubHeader(subHeader, req))
		{
			switch (subHeader[0])
			{
			case 'r':	// rtp/avp/??? Is this tcp or udp?
			case 'R':   // RTP/AVP/??? Is this TCP or UDP?
			{
				if (boost::iequals(subHeader, "RTP/AVP/TCP"))
					req.fTransportType = qtssRTPTransportTypeTCP;
				break;
			}
			case 'm':   //mode sub-header
			case 'M':   //mode sub-header
			{
				if (!ParseModeSubHeader(subHeader, req))
					return false;
			}
			}
		}
	}
	return true;
}

std::string MyRTSPRequest::GetFileDigit()
{
	static std::string digits("0123456789");
	std::size_t found = path.find_last_not_of(digits);
	if (found != std::string::npos)
		return path.substr(found + 1);
	else return {};
}