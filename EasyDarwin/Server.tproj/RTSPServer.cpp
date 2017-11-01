#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include "RTSPServer.h"
#include "RTSPSession.h"
#include "SDPUtils.h"
#include "sdpCache.h"
#include "MyRTSPRequest.h"
#include "MyRTPSession.h"
#include "MyRTPStream.h"
#include <fmt/format.h>
#include "Uri.h"

static std::string getStreamIdFromUri(std::string_view uri, size_t removeDepthFromEnd = 0)
{
	std::string pathname = Uri::Parse(std::string(uri)).Path.substr(1);
	if (pathname.back() == '/') pathname.pop_back();
	for (size_t count = 0; count < removeDepthFromEnd; count++)
	{
		size_t pos = pathname.find_last_of('/');
		if (pos == std::string::npos) break;
		pathname = pathname.substr(0, pos);
	}
	return pathname;
}

template <typename S>
static std::vector<std::string> spirit_direct(const S& input, char const* delimiter)
{
	namespace qi = boost::spirit::qi;
	std::vector<std::string> result;
	if (!qi::parse(input.begin(), input.end(),
		qi::raw[*(qi::char_ - qi::char_(delimiter))] % qi::char_(delimiter), result))
		result.push_back(std::string(input));
	return result;
}

CoTask RunRTSPSession(RTSPServer &server, boost::asio::ip::tcp::socket socket, MyRTSPSession *session) {
	boost::asio::streambuf buffer;
	MyRTSPRequest request;
	while (true) {
		auto result = co_await AsyncRead(socket, buffer.prepare(4));
		if (!result) {
			std::cerr << "Error when reading: " << result.Error().message() << "\n";
			break;
		}
		const char *firstChar = boost::asio::buffer_cast<const char*>(buffer.data());
		buffer.commit(result.Get());
		if (*firstChar == '$') {
			uint8_t packetChannel = (uint8_t)firstChar[1];
			uint16_t packetDataLen = *(uint16_t *)(firstChar + 2);
			packetDataLen = ntohs(packetDataLen);
			result = co_await AsyncRead(socket, buffer.prepare(packetDataLen));
			if (!result) {
				std::cerr << "Error when reading: " << result.Error().message() << "\n";
				break;
			}
			else {
				int a = 1;
			}
		}
		else {
			result = co_await AsyncReadUntil(socket, buffer);
			if (!result) {
				std::cerr << "Error when reading: " << result.Error().message() << "\n";
				break;
			}
			std::string text{ boost::asio::buffer_cast<const char*>(buffer.data()), result.Get() };
			std::string content;
			buffer.consume(result.Get());
			if (!RequestMessage::parse(text, request)) {
				return;
			}
			auto it = request.header.find("Content-Length");
			if (it != request.header.end()) {
				unsigned long long content_length = 0;
				try {
					content_length = std::stoull(it->second);
				}
				catch (const std::exception &e) {
					return;
				}
				content.resize(content_length);
				result = co_await AsyncRead(
					socket, boost::asio::buffer(&content[0], content.length()));
				if (!result) {
					std::cerr << "Error when reading: " << result.Error().message() << "\n";
					break;
				} 
				else 
				{
					buffer.consume(content_length);
					if (request.method == "ANNOUNCE") {
						std::string streamID = getStreamIdFromUri(request.path);
						AVStream *stream = nullptr;
						if (stream != nullptr) {
							stream->reset();
							//this.rtpParser.clearUnorderedPacketBuffer(stream.id);
						}
						else {
							stream = new AVStream(streamID);
							stream->type = StreamType::LIVE;
						}
						SDPContainer checkedSDPContainer(content);
						if (!checkedSDPContainer.Parse())
						{
							//return inParams->inRTSPRequest->SendErrorResponseWithMessage(qtssUnsupportedMediaType);
						}
						CSdpCache::GetInstance()->setSdpMap("live.sdp", content);
						std::string output = fmt::format("RTSP/1.0 200 OK\r\n"
							"Cseq: {} \r\n\r\n", request.header["CSeq"]);
						result = co_await AsyncWrite(
							socket, boost::asio::buffer(output.data(), output.length()));
						if (!result) {
							std::cerr << "Error when writing: " << result.Error().message() << "\n";
							break;
						}
					}
				}
				
			}
			if (request.method == "OPTIONS") {
				std::string output = 
					fmt::format("RTSP/1.0 200 OK\r\n"
						        "Cseq: {} \r\n"
            					"Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, OPTIONS, ANNOUNCE, RECORD\r\n\r\n", request.header["CSeq"]);
				result = co_await AsyncWrite(
					socket, boost::asio::buffer(output));
				if (!result) {
					std::cerr << "Error when writing: " << result.Error().message() << "\n";
					break;
				}
			}
			else if (request.method == "SETUP") {
				session->FindOrCreateRTPSession(request);
				std::error_code ec = session->do_setup(request);
				if (!ec) {
					std::string modifyTransport = [session, &request]() {
						auto &stream = session->fRTPSession->GetStreams().back();
						std::string transportStr = request.header["Transport"];
						std::vector<std::string> headers = spirit_direct(transportStr, ";");
						std::string output;
						for (const auto & header : headers) {
							if (header.empty()) continue;
							if (boost::istarts_with(header, "interleaved")) continue;
							output += header + ";";
						}
						output += "interleaved=" + std::to_string(stream.GetRTPChannelNum()) + "-" +
							std::to_string(stream.GetRTCPChannelNum());
						return std::move(output);
					}();
					std::string output = fmt::format("RTSP/1.0 200 OK\r\n"
							"Cseq: {}\r\n"
							"Session: {}\r\n"
							"Transport: {}\r\n\r\n", request.header["CSeq"], session->fSessionID, modifyTransport);
					result = co_await AsyncWrite(
						socket, boost::asio::buffer(output));
					if (!result) {
						std::cerr << "Error when writing: " << result.Error().message() << "\n";
						break;
					}
				}
			}
			if (request.method == "RECORD") {
				session->FindOrCreateRTPSession(request);
				if (!session->rtp_OutputSession) {
					std::error_code ec = session->do_play(nullptr);
					std::string streamsStr = [session, &request]() {
						auto &streams = session->fRTPSession->GetStreams();
						std::string output;
						for (auto &stream : streams)
							output += "url=" + request.path + "/" + stream.GetStreamURL() + ",";
						return std::move(output);
					}();
					auto output = fmt::format("RTSP/1.0 200 OK\r\n"
							"Cseq: {}\r\n"
							"Session: {}\r\n"
							"RTP-Info: {}\r\n\r\n", request.header["CSeq"], session->fSessionID, streamsStr);
					result = co_await AsyncWrite(
						socket, boost::asio::buffer(output.data(), output.length()));
					if (!result) {
						std::cerr << "Error when writing: " << result.Error().message() << "\n";
						break;
					}
				}
			}
		}
	}
}

CoTask RTSPServer::AcceptConnections() {
	while (true) {
		Result<boost::asio::ip::tcp::socket> result = co_await AsyncAccept(acceptor_);
		if (result) {
			RunRTSPSession(*this, std::move(result.Get()), new MyRTSPSession(*this));
		}
		else {
			std::cerr << "Error accepting connection: " << result.Error().message()
				<< "\n";
			break;
		}
	}
}