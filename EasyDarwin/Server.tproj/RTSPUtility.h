#pragma once

#ifdef __SSE2__
#include <emmintrin.h>
inline void spin_loop_pause() noexcept { _mm_pause(); }
#elif defined(_MSC_VER) && _MSC_VER >= 1800 && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
inline void spin_loop_pause() noexcept { _mm_pause(); }
#else
inline void spin_loop_pause() noexcept {}
#endif

#include <atomic>

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