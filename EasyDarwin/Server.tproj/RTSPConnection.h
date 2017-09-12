#pragma once
#include <memory>
#include <mutex>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include "RTSPUtility.h"

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