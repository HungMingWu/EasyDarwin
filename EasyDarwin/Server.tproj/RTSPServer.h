#pragma once
#include <math.h>
#include <map>
#include <mutex>
#include <string>
#include <unordered_set>
#include <optional>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_service.hpp>
#include "MyRTSPSession.h"
#include "MyRTSPRequest.h"
#include "coroutine_wrappers.h"

enum class StreamType {
	LIVE,
	RECORD
};

struct AVStream {
	std::string id;
	std::string spsString;
	std::string ppsString;
	std::string spropParameterSets;
	double videoFrameRate;
	size_t audioPeriodSize;
	bool isVideoStarted;
	bool isAudioStarted;
	std::optional<size_t> videoWidth;
	std::optional<size_t> videoHeight;
	std::optional<size_t> videoProfileLevelId;
	std::optional<size_t> videoAVCLevel;
	std::optional<size_t> videoAVCProfile;
	std::optional<size_t> audioClockRate;
	std::optional<size_t> audioSampleRate;
	std::optional<size_t> audioChannels;
	std::optional<size_t> audioObjectType;
	std::optional<size_t> timeAtVideoStart;
	std::optional<size_t> timeAtAudioStart;
	std::optional<size_t> spsNALUnit;
	std::optional<size_t> ppsNALUnit;
	std::optional<StreamType> type;
	std::vector<void *> rtspClients;
	size_t videoSequenceNumber;
	size_t audioSequenceNumber;
	void *rtspUploadingClient;
	double videoRTPTimestampInterval;
	double audioRTPTimestampInterval;
	std::optional<size_t> lastVideoRTPTimestamp;
	std::optional<size_t> lastAudioRTPTimestamp;
	AVStream(std::string_view id_) : id(id_) {
		reset();
		resetStreamParams();
	}
	void reset() {
		audioClockRate = std::nullopt;
		audioSampleRate = std::nullopt;
		audioChannels = std::nullopt;
		audioPeriodSize = 1024;
		audioObjectType = std::nullopt;
		videoWidth = std::nullopt;
		videoHeight = std::nullopt;
		videoProfileLevelId = std::nullopt;
		videoFrameRate = 30.0;
		videoAVCLevel = std::nullopt;
		videoAVCProfile = std::nullopt;
		isVideoStarted = false;
		isAudioStarted = false;
		timeAtVideoStart = std::nullopt;
		timeAtAudioStart = std::nullopt;
		spsString.clear();
		ppsString.clear();
		spsNALUnit = std::nullopt;
		ppsNALUnit = std::nullopt;
		spropParameterSets.clear();
		type = std::nullopt;
	}
	void resetStreamParams()
	{
		rtspUploadingClient = nullptr;
		videoSequenceNumber = 0;
		audioSequenceNumber = 0;
		lastVideoRTPTimestamp = std::nullopt;
		lastAudioRTPTimestamp = std::nullopt;
		videoRTPTimestampInterval = round(90000.0 / videoFrameRate);
		audioRTPTimestampInterval = audioPeriodSize;
	}
};

class MyReflectorSession;
class RTSPServer {
	friend class MyRTSPSession;
	boost::asio::ip::tcp::acceptor acceptor_;
	boost::asio::io_service& io_service_;
	std::mutex connections_mutex;
	std::mutex session_mutex;
	std::mutex rtp_mutex;
	std::map<std::string, std::shared_ptr<MyRTPSession>> rtpMap;
	std::map<std::string, std::shared_ptr<MyReflectorSession>> sessionMap;
	std::function<void(std::shared_ptr<MyRTSPRequest>, const boost::system::error_code &)> on_error;
	class Config {
	public:
		/// Port number to use. Defaults to 80 for HTTP and 443 for HTTPS.
		unsigned short port;
		/// If io_service is not set, number of threads that the server will use when start() is called.
		/// Defaults to 1 thread.
		size_t thread_pool_size = 1;
		/// Timeout on request handling. Defaults to 5 seconds.
		long timeout_request = 5;
		/// Timeout on content handling. Defaults to 300 seconds.
		long timeout_content = 300;
		/// IPv4 address in dotted decimal form or IPv6 address in hexadecimal notation.
		/// If empty, the address will be any address.
		std::string address;
		/// Set to false to avoid binding the socket to an address that is already in use. Defaults to true.
		bool reuse_address = true;
	};
	/// Set before calling start().
	Config config;
public:
	RTSPServer(boost::asio::io_service& io_svr) : io_service_(io_svr), acceptor_(io_svr,
		boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 10554))
	{
		AcceptConnections();
	}
	CoTask AcceptConnections();
};