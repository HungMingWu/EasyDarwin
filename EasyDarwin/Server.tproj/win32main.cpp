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
	 File:       win32main.cpp
	 Contains:   main function to drive streaming server on win32.
 */

#include <mutex>
#include <thread>
#include <iostream>
#include <unordered_set>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/streambuf.hpp>
#include "SDPUtils.h"
#include "sdpCache.h"
#include "RunServer.h"
#include "QTSServer.h"
#include "RTSPRequest.h"

std::shared_ptr<boost::asio::io_service> io_service = std::make_shared<boost::asio::io_service>();

 // Data
static uint16_t sPort = 0; //port can be set on the command line
static QTSS_ServerState sInitialState = qtssRunningState;

#ifdef __SSE2__
#include <emmintrin.h>
inline void spin_loop_pause() noexcept { _mm_pause(); }
#elif defined(_MSC_VER) && _MSC_VER >= 1800 && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
inline void spin_loop_pause() noexcept { _mm_pause(); }
#else
inline void spin_loop_pause() noexcept {}
#endif

class ScopeRunner {
	/// Scope count that is set to -1 if scopes are to be canceled
	std::atomic<long> count{ 0 };

public:
	class SharedLock {
		friend class ScopeRunner;
		std::atomic<long> &count;
		SharedLock(std::atomic<long> &count) noexcept : count(count) {}
		SharedLock &operator=(const SharedLock &) = delete;
		SharedLock(const SharedLock &) = delete;

	public:
		~SharedLock() noexcept {
			count.fetch_sub(1);
		}
	};

	ScopeRunner() noexcept = default;

	/// Returns nullptr if scope should be exited, or a shared lock otherwise
	std::unique_ptr<SharedLock> continue_lock() noexcept {
		long expected = count;
		while (expected >= 0 && !count.compare_exchange_weak(expected, expected + 1))
			spin_loop_pause();

		if (expected < 0)
			return nullptr;
		else
			return std::unique_ptr<SharedLock>(new SharedLock(count));
	}

	/// Blocks until all shared locks are released, then prevents future shared locks
	void stop() noexcept {
		long expected = 0;
		while (!count.compare_exchange_weak(expected, -1)) {
			if (expected < 0)
				return;
			expected = 0;
			spin_loop_pause();
		}
	}
};

class Connection : public std::enable_shared_from_this<Connection> {
public:
	template <typename... Args>
	Connection(std::shared_ptr<ScopeRunner> handler_runner, Args &&... args) noexcept : handler_runner(std::move(handler_runner)), socket(std::forward<Args>(args)...) {}

	std::shared_ptr<ScopeRunner> handler_runner;

	boost::asio::ip::tcp::socket socket;
	std::mutex socket_close_mutex;

	std::unique_ptr<boost::asio::steady_timer> timer;

	void close() noexcept {
		boost::system::error_code ec;
		std::unique_lock<std::mutex> lock(socket_close_mutex); // The following operations seems to be needed to run sequentially
		socket.lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
		socket.lowest_layer().close(ec);
	}

	void set_timeout(long seconds) noexcept {
		if (seconds == 0) {
			timer = nullptr;
			return;
		}

		timer = std::make_unique<boost::asio::steady_timer>(socket.get_io_service());
		timer->expires_from_now(std::chrono::seconds(seconds));
		auto self = this->shared_from_this();
		timer->async_wait([self](const boost::system::error_code &ec) {
			if (!ec)
				self->close();
		});
	}

	void cancel_timeout() noexcept {
		if (timer) {
			boost::system::error_code ec;
			timer->cancel(ec);
		}
	}
};

class Session {
public:
	Session(std::shared_ptr<Connection> connection) noexcept : connection(std::move(connection)) {
		try {
			//auto remote_endpoint = connection->socket.lowest_layer().remote_endpoint();
			//request = std::make_shared<RTSPRequest1>(remote_endpoint.address().to_string(), remote_endpoint.port());
			request = std::make_shared<RTSPRequest1>();
		}
		catch (...) {
			request = std::make_shared<RTSPRequest1>();
		}
	}

	std::shared_ptr<Connection> connection;
	std::shared_ptr<RTSPRequest1> request;
};

class Response : public std::enable_shared_from_this<Response>, public std::ostream {
	friend class RTSPServer;
	boost::asio::streambuf streambuf;

	std::shared_ptr<Session> session;
	long timeout_content;

	Response(std::shared_ptr<Session> session, long timeout_content) noexcept : std::ostream(&streambuf), session(std::move(session)), timeout_content(timeout_content) {}

	template <typename size_type>
	void write_header(const CaseInsensitiveMap &header, size_type size) {
		bool content_length_written = false;
		bool chunked_transfer_encoding = false;
		for (auto &field : header) {
			if (!content_length_written && case_insensitive_equal(field.first, "content-length"))
				content_length_written = true;
			else if (!chunked_transfer_encoding && case_insensitive_equal(field.first, "transfer-encoding") && case_insensitive_equal(field.second, "chunked"))
				chunked_transfer_encoding = true;

			*this << field.first << ": " << field.second << "\r\n";
		}
		if (!content_length_written && !chunked_transfer_encoding && !close_connection_after_response)
			*this << "Content-Length: " << size << "\r\n\r\n";
		else
			*this << "\r\n";
	}

public:
	size_t size() noexcept {
		return streambuf.size();
	}

	/// Use this function if you need to recursively send parts of a longer message
	void send(const std::function<void(const boost::system::error_code &)> &callback = nullptr) noexcept {
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
#if 0
	/// Write directly to stream buffer using std::ostream::write
	void write(const char_type *ptr, std::streamsize n) {
		std::ostream::write(ptr, n);
	}

	/// Convenience function for writing status line, potential header fields, and empty content
	void write(StatusCode status_code = StatusCode::success_ok, const CaseInsensitiveMultimap &header = CaseInsensitiveMultimap()) {
		*this << "HTTP/1.1 " << SimpleWeb::status_code(status_code) << "\r\n";
		write_header(header, 0);
	}

	/// Convenience function for writing status line, header fields, and content
	void write(StatusCode status_code, const std::string &content, const CaseInsensitiveMultimap &header = CaseInsensitiveMultimap()) {
		*this << "HTTP/1.1 " << SimpleWeb::status_code(status_code) << "\r\n";
		write_header(header, content.size());
		if (!content.empty())
			*this << content;
	}

	/// Convenience function for writing status line, header fields, and content
	void write(StatusCode status_code, std::istream &content, const CaseInsensitiveMultimap &header = CaseInsensitiveMultimap()) {
		*this << "HTTP/1.1 " << SimpleWeb::status_code(status_code) << "\r\n";
		content.seekg(0, std::ios::end);
		auto size = content.tellg();
		content.seekg(0, std::ios::beg);
		write_header(header, size);
		if (size)
			*this << content.rdbuf();
	}

	/// Convenience function for writing success status line, header fields, and content
	void write(const std::string &content, const CaseInsensitiveMultimap &header = CaseInsensitiveMultimap()) {
		write(StatusCode::success_ok, content, header);
	}

	/// Convenience function for writing success status line, header fields, and content
	void write(std::istream &content, const CaseInsensitiveMultimap &header = CaseInsensitiveMultimap()) {
		write(StatusCode::success_ok, content, header);
	}
	/// Convenience function for writing success status line, header fields, and content
	void write(std::istream &content, const CaseInsensitiveMultimap &header = CaseInsensitiveMultimap()) {
		write(StatusCode::success_ok, content, header);
	}

	/// Convenience function for writing success status line, and header fields
	void write(const CaseInsensitiveMultimap &header) {
		write(StatusCode::success_ok, std::string(), header);
	}
#endif
	/// If true, force server to close the connection after the response have been sent.
	///
	/// This is useful when implementing a HTTP/1.0-server sending content
	/// without specifying the content length.
	bool close_connection_after_response = false;
};

class RTSPServer {
	boost::asio::ip::tcp::acceptor acceptor_;
	std::shared_ptr<boost::asio::io_service> io_service_;
	std::mutex connections_mutex;
	std::shared_ptr<ScopeRunner> handler_runner{ new ScopeRunner() };
	std::function<void(std::shared_ptr<RTSPRequest1>, const boost::system::error_code &)> on_error;

	std::unordered_set<Connection *> connections;
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

	template <typename... Args>
	std::shared_ptr<Connection> create_connection(Args &&... args) noexcept {
		auto connection = std::shared_ptr<Connection>(new Connection(handler_runner, std::forward<Args>(args)...),
			[this](Connection *connection) {
				{
					std::unique_lock<std::mutex> lock(connections_mutex);
					auto it = connections.find(connection);
					if (it != connections.end())
						connections.erase(it);
				}
				delete connection;
		});
		{
			std::unique_lock<std::mutex> lock(connections_mutex);
			connections.emplace(connection.get());
		}
		return connection;
	}

public:
	RTSPServer(std::shared_ptr<boost::asio::io_service> io_svr) : io_service_(io_svr), acceptor_(*io_svr,
		boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 10554)) 
	{
		accept();
	}
	void accept() {
		auto session = std::make_shared<Session>(create_connection(*io_service));

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
	void read_request_and_content(const std::shared_ptr<Session> &session) {
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
	void write_response(const std::shared_ptr<Session> &session,
		std::function<void(std::shared_ptr<Response>, std::shared_ptr<RTSPRequest1>)> resource_function) {
		session->connection->set_timeout(config.timeout_content);
		auto response = std::shared_ptr<Response>(new Response(session, config.timeout_content), [this](Response *response_ptr) {
			auto response = std::shared_ptr<Response>(response_ptr);
			response->send([this, response](const boost::system::error_code &ec) {
				if (!ec) {
					if (response->close_connection_after_response)
						return;

					auto new_session = std::make_shared<Session>(response->session->connection);
					read_request_and_content(new_session);;
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
	void operate_request(const std::shared_ptr<Session> &session) {
		if (session->request->method == "OPTIONS") {
			write_response(session, [](std::shared_ptr<Response> response, std::shared_ptr<RTSPRequest1> request) {
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
			write_response(session, [](std::shared_ptr<Response> response, std::shared_ptr<RTSPRequest1> request) {
				*response << "RTSP/1.0 200 OK\r\n"
					      << "Cseq: " << request->header["CSeq"] << "\r\n\r\n";
			});
		}
		else if (session->request->method == "SETUP") {
			std::string sdp = session->request->content.string();
			write_response(session, [](std::shared_ptr<Response> response, std::shared_ptr<RTSPRequest1> request) {
				*response << "RTSP/1.0 200 OK\r\n"
					      << "Cseq: " << request->header["CSeq"] << "\r\n"
					      << "Cache-Control: no-cache\r\n"
					      << "Session: 972255884303327207\r\n"
					      << "Date: Mon, 04 Sep 2017 07:20:12 GMT\r\n"
					      << "Expires: Mon, 04 Sep 2017 07:20:12 GMT\r\n"
					      << "Transport: RTP/AVP/TCP;unicast;mode=record;;interleaved=0-1\r\n\r\n";
			});
		}
	}
};
int main(int argc, char * argv[])
{
	RTSPServer listener(io_service);
	std::thread t([&] {
		boost::asio::io_service::work work(*io_service);
		io_service->run();
	});
	t.detach();

	char sAbsolutePath[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, sAbsolutePath);
	//First thing to do is to read command-line arguments.

	//
	// Start Win32 DLLs
	WORD wsVersion = MAKEWORD(1, 1);
	WSADATA wsData;
	(void)::WSAStartup(wsVersion, &wsData);

	::StartServer(sPort, sInitialState, false, sAbsolutePath); // No stats update interval for now
	::RunServer();
	::exit(0);

	return (0);
}