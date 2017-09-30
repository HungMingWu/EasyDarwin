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
void RTSPServer::accept()
{
	auto session = std::make_shared<MyRTSPSession>(*this, create_connection(io_service_));

	acceptor_.async_accept(session->connection->socket, [this, session](const boost::system::error_code &ec) {
		auto lock = session->connection->handler_runner->continue_lock();
		if (!lock)
			return;

		// Immediately start accepting a new connection (unless io_service has been stopped)
		if (ec != boost::asio::error::operation_aborted)
			this->accept();

		if (!ec) {
			boost::asio::ip::tcp::no_delay option(true);
			boost::system::error_code ec;
			session->connection->socket.set_option(option, ec);

			read_request_and_content(session);
		}
		else if (on_error)
			on_error(session->request, ec);
	});
}

void RTSPServer::read_request_and_content(const std::shared_ptr<MyRTSPSession> &session)
{
	session->connection->set_timeout(config.timeout_request);
	boost::asio::async_read_until(session->connection->socket, session->request->streambuf, "\r\n\r\n",
		[this, session](const boost::system::error_code &ec, size_t bytes_transferred) {
		session->connection->cancel_timeout();
		auto lock = session->connection->handler_runner->continue_lock();
		if (!lock)
			return;
		if (!ec) {
			// request->streambuf.size() is not necessarily the same as bytes_transferred, from Boost-docs:
			// "After a successful async_read_until operation, the streambuf may contain additional data beyond the delimiter"
			// The chosen solution is to extract lines from the stream directly when parsing the header. What is left of the
			// streambuf (maybe some bytes of the content) is appended to in the async_read-function below (for retrieving content).
			size_t num_additional_bytes = session->request->streambuf.size() - bytes_transferred;

			if (!RequestMessage::parse(session->request->content, *session->request)) {
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
				if (content_length > num_additional_bytes) {
					session->connection->set_timeout(config.timeout_content);
					boost::asio::async_read(session->connection->socket, session->request->streambuf, boost::asio::transfer_exactly(content_length - num_additional_bytes),
						[this, session](const boost::system::error_code &ec, size_t /*bytes_transferred*/) {
						session->connection->cancel_timeout();
						auto lock = session->connection->handler_runner->continue_lock();
						if (!lock)
							return;
						if (!ec)
							operate_request(session);
						else if (on_error)
							on_error(session->request, ec);
					});
				}
				else {
					operate_request(session);
				}
			}
			else {
				operate_request(session);
			}
		}
		else if (on_error)
			on_error(session->request, ec);
	});
}

void RTSPServer::read_data_packet(const std::shared_ptr<MyRTSPSession> &session)
{
	session->connection->set_timeout(config.timeout_request);
	boost::asio::async_read(session->connection->socket, session->request->streambuf, boost::asio::transfer_at_least(4),
		[this, session](const boost::system::error_code &ec, size_t bytes_transferred) {
		session->connection->cancel_timeout();
		auto lock = session->connection->handler_runner->continue_lock();
		if (!lock)
			return;
		if (!ec) {
			std::vector<char> s{ buffers_begin(session->request->streambuf.data()),
					buffers_end(session->request->streambuf.data()) };
			session->request->streambuf.consume(s.size());
			auto* dataLenP = (uint16_t*)&s[0];
			size_t interleavedPacketLen = ntohs(dataLenP[1]) + 4;
			if (interleavedPacketLen > s.size()) {

			}
			else {
				session->process_rtppacket(&s[0], s.size());
			}
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

void RTSPServer::write_response1(const std::shared_ptr<MyRTSPSession> &session,
	std::function<void(std::shared_ptr<Response>, std::shared_ptr<MyRTSPRequest>)> resource_function)
{
	session->connection->set_timeout(config.timeout_content);
	auto response = std::shared_ptr<Response>(new Response(session, config.timeout_content), [this, session](Response *response_ptr) {
		auto response = std::shared_ptr<Response>(response_ptr);
		response->send([this, response, session](const boost::system::error_code &ec) {
			if (!ec) {
				if (response->close_connection_after_response)
					return;

				read_data_packet(session);;
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
	if (session->request->method == "OPTIONS") {
		write_response(session, [](std::shared_ptr<Response> response, std::shared_ptr<MyRTSPRequest> request) {
			*response << "RTSP/1.0 200 OK\r\n"
				<< "Cseq: " << request->header["CSeq"] << "\r\n"
				<< "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, OPTIONS, ANNOUNCE, RECORD\r\n\r\n";
		});
	}
	else if (session->request->method == "ANNOUNCE") {
		std::string sdp = session->request->content.string();
		SDPContainer checkedSDPContainer(sdp);
		if (!checkedSDPContainer.Parse())
		{
			//return inParams->inRTSPRequest->SendErrorResponseWithMessage(qtssUnsupportedMediaType);
		}
		CSdpCache::GetInstance()->setSdpMap("live.sdp", sdp);
		write_response(session, [](std::shared_ptr<Response> response, std::shared_ptr<MyRTSPRequest> request) {
			*response << "RTSP/1.0 200 OK\r\n"
				<< "Cseq: " << request->header["CSeq"] << "\r\n\r\n";
		});
	}
	else if (session->request->method == "SETUP") {
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
			write_response1(session, [session](std::shared_ptr<Response> response, std::shared_ptr<MyRTSPRequest> request) {
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