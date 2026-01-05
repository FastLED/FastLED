// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/stub/semaphore_stub_stl.h
/// @brief Stub platform semaphore implementation using STL primitives
///
/// This header provides real semaphore implementations for multithreaded stub platforms,
/// routing to either C++20 std::semaphore or a mutex/cv-based implementation.

#include "fl/has_include.h"
#include "fl/stl/cstddef.h"

// Check for C++20 semaphore support
#if __cplusplus >= 202002L && FL_HAS_INCLUDE(<semaphore>)
    #include <semaphore>  // ok include
    #define FL_HAS_STD_SEMAPHORE 1
#else
    #include "fl/stl/mutex.h"
    #include "fl/stl/condition_variable.h"
    #define FL_HAS_STD_SEMAPHORE 0
#endif

// Forward declare std::chrono types to avoid including <chrono> in header
namespace std {
    namespace chrono {
        template<typename Rep, typename Period> class duration;
        template<typename Clock, typename Duration> class time_point;
    }
}

namespace fl {
namespace platforms {

#if FL_HAS_STD_SEMAPHORE
    // C++20: Alias to std::semaphore for full compatibility with standard library
    template<ptrdiff_t LeastMaxValue = 1>
    using CountingSemaphoreReal = std::counting_semaphore<LeastMaxValue>;  // okay std namespace

    using BinarySemaphoreReal = std::binary_semaphore;  // okay std namespace
#else
    // Pre-C++20: Implement using mutex + condition_variable
    template<ptrdiff_t LeastMaxValue = 1>
    class CountingSemaphoreReal {
    private:
        fl::mutex mMutex;
        fl::condition_variable mCv;
        ptrdiff_t mCount;

    public:
        explicit CountingSemaphoreReal(ptrdiff_t desired) : mCount(desired) {
            FL_ASSERT(desired >= 0 && desired <= LeastMaxValue,
                     "CountingSemaphoreReal: initial count out of range");
        }

        // Non-copyable and non-movable
        CountingSemaphoreReal(const CountingSemaphoreReal&) = delete;
        CountingSemaphoreReal& operator=(const CountingSemaphoreReal&) = delete;
        CountingSemaphoreReal(CountingSemaphoreReal&&) = delete;
        CountingSemaphoreReal& operator=(CountingSemaphoreReal&&) = delete;

        ~CountingSemaphoreReal() = default;

        void release(ptrdiff_t update = 1) {
            FL_ASSERT(update >= 0, "CountingSemaphoreReal: release update must be non-negative");
            fl::unique_lock<fl::mutex> lock(mMutex);
            FL_ASSERT(mCount + update <= LeastMaxValue,
                     "CountingSemaphoreReal: release would exceed max value");
            mCount += update;
            if (update == 1) {
                mCv.notify_one();
            } else {
                mCv.notify_all();
            }
        }

        void acquire() {
            fl::unique_lock<fl::mutex> lock(mMutex);
            mCv.wait(lock, [this]{ return mCount > 0; });
            --mCount;
        }

        bool try_acquire() {
            fl::unique_lock<fl::mutex> lock(mMutex);
            if (mCount > 0) {
                --mCount;
                return true;
            }
            return false;
        }

        template<class Rep, class Period>
        bool try_acquire_for(const std::chrono::duration<Rep, Period>& rel_time) {  // okay std namespace
            fl::unique_lock<fl::mutex> lock(mMutex);
            if (mCv.wait_for(lock, rel_time, [this]{ return mCount > 0; })) {
                --mCount;
                return true;
            }
            return false;
        }

        template<class Clock, class Duration>
        bool try_acquire_until(const std::chrono::time_point<Clock, Duration>& abs_time) {  // okay std namespace
            fl::unique_lock<fl::mutex> lock(mMutex);
            if (mCv.wait_until(lock, abs_time, [this]{ return mCount > 0; })) {
                --mCount;
                return true;
            }
            return false;
        }

        static constexpr ptrdiff_t max() noexcept {
            return LeastMaxValue;
        }
    };

    using BinarySemaphoreReal = CountingSemaphoreReal<1>;
#endif

// Platform implementation aliases for multithreaded mode
template<ptrdiff_t LeastMaxValue = 1>
using counting_semaphore = CountingSemaphoreReal<LeastMaxValue>;

using binary_semaphore = BinarySemaphoreReal;

// Define FASTLED_MULTITHREADED for multithreaded platforms
#ifndef FASTLED_MULTITHREADED
#define FASTLED_MULTITHREADED 1
#endif

} // namespace platforms
} // namespace fl
