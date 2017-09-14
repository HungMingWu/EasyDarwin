#pragma once
#include <map>
#include <mutex>
#include <string>
#include <unordered_set>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_service.hpp>
#include "RTSPUtility.h"
#include "RTSPConnection.h"
#include "MyRTSPSession.h"
#include "MyRTSPRequest.h"

class Response : public std::enable_shared_from_this<Response>, public std::ostream {
	friend class RTSPServer;
	boost::asio::streambuf streambuf;

	std::shared_ptr<MyRTSPSession> session;
	long timeout_content;

	Response(std::shared_ptr<MyRTSPSession> session, long timeout_content) noexcept : std::ostream(&streambuf), session(std::move(session)), timeout_content(timeout_content) {}

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
	void send(const std::function<void(const boost::system::error_code &)> &callback = nullptr) noexcept;
	/// If true, force server to close the connection after the response have been sent.
	///
	/// This is useful when implementing a HTTP/1.0-server sending content
	/// without specifying the content length.
	bool close_connection_after_response = false;
};

class ReflectorSession1;
class RTSPServer {
	friend class MyRTSPSession;
	boost::asio::ip::tcp::acceptor acceptor_;
	boost::asio::io_service& io_service_;
	std::mutex connections_mutex;
	std::mutex session_mutex;
	std::mutex rtp_mutex;
	std::map<std::string, std::shared_ptr<MyRTPSession>> rtpMap;
	std::map<std::string, std::shared_ptr<ReflectorSession1>> sessionMap;
	std::shared_ptr<ScopeRunner> handler_runner{ new ScopeRunner() };
	std::function<void(std::shared_ptr<MyRTSPRequest>, const boost::system::error_code &)> on_error;

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
	RTSPServer(boost::asio::io_service& io_svr) : io_service_(io_svr), acceptor_(io_svr,
		boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 10554))
	{
		accept();
	}
	void accept();
	void read_request_and_content(const std::shared_ptr<MyRTSPSession> &session);
	void write_response(const std::shared_ptr<MyRTSPSession> &session,
		std::function<void(std::shared_ptr<Response>, std::shared_ptr<MyRTSPRequest>)> resource_function);
	void operate_request(const std::shared_ptr<MyRTSPSession> &session);
};