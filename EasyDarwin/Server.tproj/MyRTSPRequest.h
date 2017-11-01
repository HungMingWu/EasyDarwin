#pragma once
#include <array>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <boost/asio/streambuf.hpp>
#include <boost/utility/string_view.hpp>
#include "QTSS.h"

inline bool case_insensitive_equal(const std::string &str1, const std::string &str2) noexcept {
	return str1.size() == str2.size() &&
		std::equal(str1.begin(), str1.end(), str2.begin(), [](char a, char b) {
		return tolower(a) == tolower(b);
	});
}
class CaseInsensitiveEqual {
public:
	bool operator()(const std::string &str1, const std::string &str2) const noexcept {
		return case_insensitive_equal(str1, str2);
	}
};

// Based on https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x/2595226#2595226
class CaseInsensitiveHash {
public:
	size_t operator()(const std::string &str) const noexcept {
		size_t h = 0;
		std::hash<int> hash;
		for (auto c : str)
			h ^= hash(tolower(c)) + 0x9e3779b9 + (h << 6) + (h >> 2);
		return h;
	}
};

typedef std::unordered_map<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual> CaseInsensitiveMap;
class RTSPHeader {
public:
	/// Parse header fields
	static void parse(std::istream &stream, CaseInsensitiveMap &header) noexcept {
		std::string line;
		getline(stream, line);
		size_t param_end;
		while ((param_end = line.find(':')) != std::string::npos) {
			size_t value_start = param_end + 1;
			if (value_start < line.size()) {
				if (line[value_start] == ' ')
					value_start++;
				if (value_start < line.size())
					header.emplace(line.substr(0, param_end), line.substr(value_start, line.size() - value_start - 1));
			}

			getline(stream, line);
		}
	}
};

class RequestMessage;
class MyRTSPSession;

class MyRTSPRequest {
	friend class RTSPServer;
	friend class RequestMessage;
	friend class MyRTSPSession;
public:
	std::string method, path, query_string, rtsp_version;
	std::string remote_endpoint_address;
	unsigned short remote_endpoint_port;
	CaseInsensitiveMap header;
	QTSS_RTPNetworkMode fNetworkMode;
	QTSS_RTPTransportType fTransportType;
	QTSS_RTPTransportMode fTransportMode;
	uint16_t fSetUpServerPort{ 0 };

	// -1 not in request, 0 off, 1 on
	int32_t                      fEnableDynamicRateState;
public:
	MyRTSPRequest(const std::string &remote_endpoint_address = std::string(), 
		          unsigned short remote_endpoint_port = 0) noexcept
		: remote_endpoint_address(remote_endpoint_address), remote_endpoint_port(remote_endpoint_port) {}

	~MyRTSPRequest() = default;
	bool IsPushRequest() { return (fTransportMode == qtssRTPTransportModeRecord) ? true : false; }
	std::string GetFileDigit() const;
	uint16_t GetSetUpServerPort() const { return fSetUpServerPort; }
	void SetUpServerPort(uint16_t port) { fSetUpServerPort = port; }
	QTSS_RTPTransportType       GetTransportType() { return fTransportType; }
	QTSS_RTPNetworkMode         GetNetworkMode() { return fNetworkMode; }
	int32_t                     GetDynamicRateState() { return fEnableDynamicRateState; }
	std::string GetFileName() const;
};

class RequestMessage {
	static bool ParseNetworkModeSubHeader(boost::string_view inModeSubHeader, MyRTSPRequest& req);
	static bool ParseModeSubHeader(boost::string_view inModeSubHeader, MyRTSPRequest& req);
	static bool parse_setup(MyRTSPRequest& req);
public:
	/// Parse request line and header fields
	static bool parse(std::string &string, MyRTSPRequest& req) noexcept {
		std::istringstream stream(string);
		req.header.clear();
		std::string line;
		getline(stream, line);
		size_t method_end;
		if ((method_end = line.find(' ')) != std::string::npos) {
			req.method = line.substr(0, method_end);

			size_t query_start = std::string::npos;
			size_t path_and_query_string_end = std::string::npos;
			for (size_t i = method_end + 1; i < line.size(); ++i) {
				if (line[i] == '?' && (i + 1) < line.size())
					query_start = i + 1;
				else if (line[i] == ' ') {
					path_and_query_string_end = i;
					break;
				}
			}
			if (path_and_query_string_end != std::string::npos) {
				if (query_start != std::string::npos) {
					req.path = line.substr(method_end + 1, query_start - method_end - 2);
					req.query_string = line.substr(query_start, path_and_query_string_end - query_start);
				}
				else
					req.path = line.substr(method_end + 1, path_and_query_string_end - method_end - 1);

				size_t protocol_end;
				if ((protocol_end = line.find('/', path_and_query_string_end + 1)) != std::string::npos) {
					if (line.compare(path_and_query_string_end + 1, protocol_end - path_and_query_string_end - 1, "RTSP") != 0)
						return false;
					req.rtsp_version = line.substr(protocol_end + 1, line.size() - protocol_end - 2);
				}
				else
					return false;

				RTSPHeader::parse(stream, req.header);
				auto it = req.header.find("CSeq");
				if (it == req.header.end())
					return false;
				if (req.method == "SETUP")
					return parse_setup(req);
			}
			else
				return false;
		}
		else
			return false;
		return true;
	}
};
