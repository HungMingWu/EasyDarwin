#include "coroutine_wrappers.h"
#include <boost/asio.hpp>

namespace internal {
	void IOAction::HandleDone::operator()(boost::system::error_code error,
		std::size_t bytes_transferred) {
		if (error) {
			io_action->result_ = error;
		}
		else {
			io_action->result_ = bytes_transferred;
		}
		handle.resume();
	}
}  // namespace internal

AsyncRead::AsyncRead(boost::asio::ip::tcp::socket& socket, boost::asio::mutable_buffer buffer)
	: socket_(socket), buffer_(buffer) {}

void AsyncRead::await_suspend(std::experimental::coroutine_handle<> handle) {
	boost::asio::async_read(socket_, boost::asio::buffer(buffer_),
		HandleDone{ this, handle });
}

AsyncReadUntil::AsyncReadUntil(boost::asio::ip::tcp::socket& socket, boost::asio::streambuf & buffer)
	: socket_(socket), buffer_(buffer) {}

void AsyncReadUntil::await_suspend(std::experimental::coroutine_handle<> handle) {
	boost::asio::async_read_until(socket_, buffer_, "\r\n\r\n",
		HandleDone{ this, handle });
}

AsyncWrite::AsyncWrite(boost::asio::ip::tcp::socket& socket, boost::asio::const_buffer buffer)
	: socket_(socket), buffer_(buffer) {}

void AsyncWrite::await_suspend(std::experimental::coroutine_handle<> handle) {
	boost::asio::async_write(socket_, boost::asio::buffer(buffer_),
		HandleDone{ this, handle });
}

AsyncAccept::AsyncAccept(boost::asio::ip::tcp::acceptor& acceptor)
	: acceptor_(acceptor), socket_(acceptor.get_io_service()) {}

void AsyncAccept::await_suspend(std::experimental::coroutine_handle<> handle) {
	acceptor_.async_accept(socket_, [this, handle](boost::system::error_code error) mutable {
		if (error) {
			result_.contents_ = error;
		}
		else {
			result_.contents_ = std::move(socket_);
		}
		handle.resume();
	});
}