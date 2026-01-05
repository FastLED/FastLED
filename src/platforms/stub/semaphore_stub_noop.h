// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/stub/semaphore_stub_noop.h
/// @brief Stub platform semaphore no-op implementation
///
/// This header provides a fake semaphore implementation for single-threaded stub platforms.
/// The semaphore operations are simplified since there's no actual threading.

#include "fl/stl/cstddef.h"

// Forward declare std::chrono types to avoid including <chrono> in header
namespace std {
    namespace chrono {
        template<typename Rep, typename Period> class duration;
        template<typename Clock, typename Duration> class time_point;
    }
}

namespace fl {
namespace platforms {

// Fake semaphore for single-threaded mode
template <ptrdiff_t LeastMaxValue = 1>
class CountingSemaphoreFake {
private:
    ptrdiff_t mCount;

public:
    explicit CountingSemaphoreFake(ptrdiff_t desired) : mCount(desired) {
        FL_ASSERT(desired >= 0 && desired <= LeastMaxValue,
                 "CountingSemaphoreFake: initial count out of range");
    }

    // Non-copyable and non-movable
    CountingSemaphoreFake(const CountingSemaphoreFake&) = delete;
    CountingSemaphoreFake& operator=(const CountingSemaphoreFake&) = delete;
    CountingSemaphoreFake(CountingSemaphoreFake&&) = delete;
    CountingSemaphoreFake& operator=(CountingSemaphoreFake&&) = delete;

    ~CountingSemaphoreFake() = default;

    void release(ptrdiff_t update = 1) {
        FL_ASSERT(update >= 0, "CountingSemaphoreFake: release update must be non-negative");
        FL_ASSERT(mCount + update <= LeastMaxValue,
                 "CountingSemaphoreFake: release would exceed max value");
        mCount += update;
    }

    void acquire() {
        // In single-threaded mode, waiting would deadlock
        FL_ASSERT(mCount > 0,
                 "CountingSemaphoreFake: acquire() with count=0 would deadlock in single-threaded mode");
        --mCount;
    }

    bool try_acquire() {
        if (mCount > 0) {
            --mCount;
            return true;
        }
        return false;
    }

    // Timed acquire methods are no-ops in single-threaded mode
    // (same behavior as try_acquire since there's no waiting)
    template<class Rep, class Period>
    bool try_acquire_for(const std::chrono::duration<Rep, Period>&) {  // okay std namespace
        return try_acquire();
    }

    template<class Clock, class Duration>
    bool try_acquire_until(const std::chrono::time_point<Clock, Duration>&) {  // okay std namespace
        return try_acquire();
    }

    static constexpr ptrdiff_t max() noexcept {
        return LeastMaxValue;
    }
};

// Platform implementation aliases for single-threaded mode
template<ptrdiff_t LeastMaxValue = 1>
using counting_semaphore = CountingSemaphoreFake<LeastMaxValue>;

using binary_semaphore = CountingSemaphoreFake<1>;

// Define FASTLED_MULTITHREADED for single-threaded platforms
#ifndef FASTLED_MULTITHREADED
#define FASTLED_MULTITHREADED 0
#endif

} // namespace platforms
} // namespace fl
