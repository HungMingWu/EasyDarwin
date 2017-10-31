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


void Response::send(const std::function<void(const boost::system::error_code &)> &callback) noexcept
{
	session->connection->set_timeout(timeout_content);
	auto self = this->shared_from_this(); // Keep Response instance alive through the following async_write
	boost::asio::async_write(session->connection->socket, streambuf,
		[self, callback](const boost::system::error_code &ec, size_t /*bytes_transferred*/) {
		self->session->connection->cancel_timeout();
		auto lock = self->session->connection->handler_runner->continue_lock();
		if (!lock)
			return;
		if (callback)
			callback(ec);
	});
}

CoTask RunRTSPSession(boost::asio::ip::tcp::socket socket) {
	boost::asio::streambuf buffer;
	MyRTSPRequest request;
	while (true) {
		auto result = co_await AsyncRead(socket, buffer.prepare(4));
		if (!result) {
			std::cerr << "Error when reading: " << result.Error().message() << "\n";
			break;
		}
		buffer.commit(result.Get());
		const char *firstChar = boost::asio::buffer_cast<const char*>(buffer.data());
		if (*firstChar == '$') {
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
				size_t kkk = content.size();
				result = co_await AsyncRead(
					socket, boost::asio::buffer(&content[0], content.size()));
				if (!result) {
					std::cerr << "Error when reading: " << result.Error().message() << "\n";
					break;
				} else 
				{
					int a = 1;
					buffer.consume(content_length);
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
			if (request.method == "ANNOUNCE") {
				SDPContainer checkedSDPContainer(content);
				if (!checkedSDPContainer.Parse())
				{
					//return inParams->inRTSPRequest->SendErrorResponseWithMessage(qtssUnsupportedMediaType);
				}
				CSdpCache::GetInstance()->setSdpMap("live.sdp", content);
				std::string output = fmt::format("RTSP/1.0 200 OK\r\n"
					"Cseq: {} \r\n", request.header["CSeq"]);
				result = co_await AsyncWrite(
					socket, boost::asio::buffer(output.data(), output.length()));
			}
		}
	}
}

CoTask RTSPServer::AcceptConnections() {
	while (true) {
		Result<boost::asio::ip::tcp::socket> result = co_await AsyncAccept(acceptor_);
		if (result) {
			RunRTSPSession(std::move(result.Get()));
		}
		else {
			std::cerr << "Error accepting connection: " << result.Error().message()
				<< "\n";
			break;
		}
	}
}

void RTSPServer::deal_with_packet(const std::shared_ptr<MyRTSPSession> &session)
{
	const auto *begin = boost::asio::buffer_cast<const char*>(session->request->streambuf.data());
	if (*begin == '$') {
		int a = 1;
	}
	else {
		static const boost::string_view spilt("\r\n\r\n");
		std::string
			text{ boost::asio::buffer_cast<const char*>(session->request->streambuf.data()),
			session->request->streambuf.size() };
		size_t pos = text.rfind(spilt.data());
		if (pos == std::string::npos) {
				
		}
		else {
			pos += spilt.size();
			size_t num_additional_bytes = text.length() - pos;

			if (!RequestMessage::parse(text, *session->request)) {
				if (on_error) {
					//	on_error(session->request, make_error_code::make_error_code(boost::system::errc::protocol_error));
				}
				return;
			}

			// If content, read that as well
			auto it = session->request->header.find("Content-Length");
			if (it != session->request->header.end()) {
				unsigned long long content_length = 0;
				try {
					content_length = std::stoull(it->second);
				}
				catch (const std::exception &e) {
					if (on_error) {
						//on_error(session->request, make_error_code::make_error_code(boost::system::errc::protocol_error));
					}
					return;
				}
				if (content_length <= num_additional_bytes) {
					if (session->request->method == "ANNOUNCE") {
					}
					else {
						operate_request(session);
					}
					session->request->streambuf.consume(pos + content_length);
				}
				else
				{
					int a = 1;
				}
			}
			else {
				operate_request(session);
				session->request->streambuf.consume(pos);
			}
		}
	}
}

void RTSPServer::read_request_and_content(const std::shared_ptr<MyRTSPSession> &session)
{
	constexpr size_t blocks = 2048;
	session->connection->set_timeout(config.timeout_request);
	session->connection->socket.async_read_some(session->request->streambuf.prepare(blocks),
		[this, session](const boost::system::error_code &ec, size_t bytes_transferred) {
		session->connection->cancel_timeout();
		auto lock = session->connection->handler_runner->continue_lock();
		if (!lock)
			return;
		session->request->streambuf.commit(bytes_transferred);
		if (!ec) {
			deal_with_packet(session);
			read_request_and_content(session);
		}
		else if (on_error)
			on_error(session->request, ec);
	});
}

void RTSPServer::write_response(const std::shared_ptr<MyRTSPSession> &session,
	std::function<void(std::shared_ptr<Response>, std::shared_ptr<MyRTSPRequest>)> resource_function)
{
	session->connection->set_timeout(config.timeout_content);
	auto response = std::shared_ptr<Response>(new Response(session, config.timeout_content), [this, session](Response *response_ptr) {
		auto response = std::shared_ptr<Response>(response_ptr);
		response->send([this, response, session](const boost::system::error_code &ec) {
			if (!ec) {
				if (response->close_connection_after_response)
					return;

				read_request_and_content(session);;
			}
			else if (this->on_error)
				this->on_error(response->session->request, ec);
		});
	});

	try {
		resource_function(response, session->request);
	}
	catch (const std::exception &e) {
		if (on_error) {
			//	on_error(session->request, make_error_code::make_error_code(boost::system::errc::operation_canceled));
		}
		return;
	}
}

void RTSPServer::operate_request(const std::shared_ptr<MyRTSPSession> &session)
{
	if (session->request->method == "SETUP") {
		session->FindOrCreateRTPSession();
		std::error_code ec = session->do_setup();
		if (!ec) {
			write_response(session, [session](std::shared_ptr<Response> response, std::shared_ptr<MyRTSPRequest> request) {
				auto &stream = session->fRTPSession->GetStreams().back();
				std::string transportStr = request->header["Transport"];
				std::vector<std::string> headers = spirit_direct(transportStr, ";");
				std::string output;
				for (const auto & header : headers) {
					if (header.empty()) continue;
					if (boost::istarts_with(header, "interleaved")) continue;
					output += header + ";";
				}
				output += "interleaved=" + std::to_string(stream.GetRTPChannelNum()) + "-" + 
					std::to_string(stream.GetRTCPChannelNum());
				*response << "RTSP/1.0 200 OK\r\n"
					<< "Cseq: " << request->header["CSeq"] << "\r\n"
					<< "Session: " << session->fSessionID << "\r\n"
					<< "Transport: " << output << "\r\n\r\n";
			});
		}
	} else if (session->request->method == "RECORD") {
		session->FindOrCreateRTPSession();
		if (!session->rtp_OutputSession) {
			std::error_code ec = session->do_play(nullptr);
			write_response(session, [session](std::shared_ptr<Response> response, std::shared_ptr<MyRTSPRequest> request) {
				auto &streams = session->fRTPSession->GetStreams();
				std::string transportStr = request->header["Transport"];
				std::string output;
				for (auto &stream : streams)
					output += "url=" + request->path + "/" + stream.GetStreamURL() +",";
				*response << "RTSP/1.0 200 OK\r\n"
					<< "Cseq: " << request->header["CSeq"] << "\r\n"
					<< "Session: " << session->fSessionID << "\r\n"
					<< "RTP-Info: " << output << "\r\n\r\n";
			});
		}
	}
	else {
		int a = 1;
	}
}