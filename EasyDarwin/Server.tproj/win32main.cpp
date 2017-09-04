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

#include <thread>
#include <iostream>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/streambuf.hpp>
#include "RunServer.h"
#include "QTSServer.h"
#include "RTSPRequest.h"

boost::asio::io_service io_service;

 // Data
static uint16_t sPort = 0; //port can be set on the command line
static QTSS_ServerState sInitialState = qtssRunningState;
class RTSPSession {
	boost::asio::ip::tcp::socket socket_;
	enum { max_length = 1024 };
	char data_[max_length];
	int state{ 0 };
	boost::asio::streambuf request_;
public:
	RTSPSession(boost::asio::io_service& io_service)
		: socket_(io_service)
	{
	}
	boost::asio::ip::tcp::socket& socket()
	{
		return socket_;
	}
	void start()
	{
		boost::asio::async_read_until(socket_, request_, "\r\n\r\n",
			std::bind(&RTSPSession::handle_read_headers, this,
				std::placeholders::_1, std::placeholders::_2));
#if 0
		socket_.async_read_some(boost::asio::buffer(data_, max_length),
			std::bind(&RTSPSession::handle_read, this,
				std::placeholders::_1,
				std::placeholders::_2));
#endif
	}

	void handle_read_headers(const boost::system::error_code& err, size_t bytes_transferred)
	{
		if (!err)
		{
			size_t num_additional_bytes = request_.size() - bytes_transferred;
			size_t content_length = (state == 1) ? 498 : 0;
			if (content_length > num_additional_bytes) {
				boost::asio::streambuf::const_buffers_type bufs = request_.data();
				std::string requestStr(boost::asio::buffers_begin(bufs), boost::asio::buffers_begin(bufs) + bytes_transferred);
				request_.consume(bytes_transferred);
				boost::asio::async_read(socket_, request_,
					boost::asio::transfer_exactly(content_length - num_additional_bytes), 
					[this](const boost::system::error_code& ec, size_t /*bytes_transferred*/) {
					if (!ec) {
						std::cout << "aaa\n";
						std::istream response_stream(&request_);
						std::stringstream ss;
						ss << response_stream.rdbuf();
						std::cout << ss.str();
						prepare();
						boost::asio::async_write(socket_,
							boost::asio::buffer(data_, strlen(data_)),
							std::bind(&RTSPSession::handle_write, this,
								std::placeholders::_1));
					}
					else {

					}
				});
			}
			else {
				std::istream response_stream(&request_);
				std::string header;
				while (std::getline(response_stream, header) && header != "\r")
					std::cout << header << "\n";
				std::cout << "\n";
				prepare();
				boost::asio::async_write(socket_,
					boost::asio::buffer(data_, strlen(data_)),
					std::bind(&RTSPSession::handle_write, this,
						std::placeholders::_1));
			}
		}
		else
		{
		}
	}

	void prepare()
	{
		if (state == 0) {
			// OPTION
			strcpy(data_, "RTSP/1.0 200 OK\r\nCseq: 1\r\nPublic: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, OPTIONS, ANNOUNCE, RECORD\r\n\r\n");
		} else if (state == 1) {
			// Announce
			strcpy(data_, "RTSP/1.0 200 OK\r\nCseq: 2\r\n\r\n");
		}
		else if (state == 2) {
			// Setup
			strcpy(data_, "RTSP/1.0 200 OK\r\n"
				"Cseq: 3\r\n"
				"Cache-Control: no-cache\r\n"
				"Session: 972255884303327207\r\n"
				"Date: Mon, 04 Sep 2017 07:20:12 GMT\r\n"
				"Expires: Mon, 04 Sep 2017 07:20:12 GMT\r\n"
				"Transport: RTP/AVP/TCP;unicast;mode=record;;interleaved=0-1\r\n\r\n");
		}
		state++;
	}
	void handle_write(const boost::system::error_code& error)
	{
		if (!error)
		{
			boost::asio::async_read_until(socket_, request_, "\r\n\r\n",
				std::bind(&RTSPSession::handle_read_headers, this,
					std::placeholders::_1, std::placeholders::_2));
		}
		else
		{
			delete this;
		}
	}
};
class RTSPListener {
	boost::asio::ip::tcp::acceptor acceptor_;
	boost::asio::io_service& io_service_;

	void handle_accept(RTSPSession* new_session,
		const boost::system::error_code& error)
	{
		if (!error)
		{
			new_session->start();
			new_session = new RTSPSession(io_service_);
			acceptor_.async_accept(new_session->socket(),
				std::bind(&RTSPListener::handle_accept, this, new_session,
					std::placeholders::_1));
		}
		else
		{
			delete new_session;
		}
	}
public:
	RTSPListener(boost::asio::io_service &io_svr) : io_service_(io_svr), acceptor_(io_svr,
		boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 10554)) 
	{
		RTSPSession* new_session = new RTSPSession(io_service_);

		acceptor_.async_accept(new_session->socket(),
			std::bind(&RTSPListener::handle_accept, this, new_session,
				std::placeholders::_1));
	}
};
int main(int argc, char * argv[])
{
	RTSPListener listener(io_service);
	std::thread t([&] {
		boost::asio::io_service::work work(io_service);
		io_service.run();
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