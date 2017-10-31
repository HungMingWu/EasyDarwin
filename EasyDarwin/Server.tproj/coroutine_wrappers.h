#pragma once

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <experimental/coroutine>
#include <type_traits>
#include <system_error>
#include <variant>

// Wraps a result so that it can fail with a boost::system::error_code instead.
// Example usage:
//   Result<int> result = Foo();
//   if (result) {
//     DoSomethingWith(result.Get());
//   } else {
//     std::cerr << "An error occurred: " << result.Error().message() << "\n";
//   }
template <typename T,
	typename = std::enable_if_t<!std::is_same_v<T, std::error_code>>>
	class Result {
	public:
		Result() = default;
		Result(boost::system::error_code error) : contents_(error) {}
		Result(T value) : contents_(std::move(value)) {}
		operator bool() const { return std::holds_alternative<T>(contents_); }
		T& Get() { return std::get<T>(contents_); }
		boost::system::error_code Error() {
			if (auto* error = std::get_if<boost::system::error_code>(&contents_)) {
				return *error;
			}
			else {
				return {};
			}
		}
		std::variant<boost::system::error_code, T> contents_;
};

namespace internal {

	class IOAction {
	public:
		bool await_ready() const { return false; }
		Result<std::size_t> await_resume() { return std::move(result_); }

	protected:
		struct HandleDone {
			IOAction* io_action;
			std::experimental::coroutine_handle<> handle;
			void operator()(boost::system::error_code error, std::size_t bytes_transferred);
		};

	private:
		Result<std::size_t> result_;
	};

}  // namespace internal


class AsyncRead : public internal::IOAction {
public:
	AsyncRead(boost::asio::ip::tcp::socket& socket, boost::asio::mutable_buffer buffer);
	void await_suspend(std::experimental::coroutine_handle<> handle);

private:
	boost::asio::ip::tcp::socket& socket_;
	boost::asio::mutable_buffer buffer_;
};

class AsyncReadUntil : public internal::IOAction {
public:
	AsyncReadUntil(boost::asio::ip::tcp::socket& socket, boost::asio::streambuf & uffer);
	void await_suspend(std::experimental::coroutine_handle<> handle);

private:
	boost::asio::ip::tcp::socket& socket_;
	boost::asio::streambuf &buffer_;
};

class AsyncWrite : public internal::IOAction {
public:
	AsyncWrite(boost::asio::ip::tcp::socket& socket, boost::asio::const_buffer buffer);
	void await_suspend(std::experimental::coroutine_handle<> handle);

private:
	boost::asio::ip::tcp::socket& socket_;
	boost::asio::const_buffer buffer_;
};

class AsyncAccept {
public:
	explicit AsyncAccept(boost::asio::ip::tcp::acceptor& acceptor);
	bool await_ready() const { return false; }
	void await_suspend(std::experimental::coroutine_handle<> handle);
	Result<boost::asio::ip::tcp::socket> await_resume() { return std::move(result_); }

private:
	boost::asio::ip::tcp::acceptor& acceptor_;
	boost::asio::ip::tcp::socket socket_;
	Result<boost::asio::ip::tcp::socket> result_;
};

struct CoTask {
	struct promise_type {
		auto get_return_object() { return CoTask{}; }
		auto initial_suspend() { return std::experimental::suspend_never{}; }
		auto final_suspend() { return std::experimental::suspend_never{}; }
		void unhandled_exception() { std::terminate(); }
		void return_void() {}
	};
};
