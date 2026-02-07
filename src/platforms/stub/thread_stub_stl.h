// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/stub/thread_stub_stl.h
/// @brief STL-based thread implementation for multithreaded platforms
///
/// This header provides full std::thread support for platforms with pthread.
/// It wraps std::thread and related functionality in the fl::platforms namespace.

// Standard library includes must be OUTSIDE namespaces
#include <thread>  // ok include
#include "fl/stl/chrono.h"
#include "platforms/win/is_win.h"

#ifdef FL_IS_WIN
    // Forward declare Windows Sleep function to avoid including windows.h
    // which pulls in GDI headers that define ERROR macro
    extern "C" __declspec(dllimport) void __stdcall Sleep(unsigned long dwMilliseconds);
#else
    #include <time.h>
    #include <errno.h>
#endif

// FASTLED_MULTITHREADED is defined by fl/stl/thread_config.h
// This file provides the STL-based thread implementation for multithreaded platforms

namespace fl {
namespace platforms {
namespace detail {

/// @brief Native sleep implementation using platform APIs
/// @tparam Rep The arithmetic type representing the number of ticks
/// @tparam Period A fl::ratio representing the tick period
template<typename Rep, typename Period>
inline void native_sleep(const fl::chrono::duration<Rep, Period>& sleep_duration) {
    // Convert to nanoseconds
    auto ns = fl::chrono::duration_cast<fl::chrono::nanoseconds>(sleep_duration).count();

    if (ns <= 0) {
        return;
    }

#ifdef FL_IS_WIN
    // Windows: Sleep takes milliseconds
    // Convert nanoseconds to milliseconds (round up)
    unsigned long ms = static_cast<unsigned long>((ns + 999999) / 1000000);
    Sleep(ms);
#else
    // POSIX: Use nanosleep
    struct timespec ts;
    ts.tv_sec = ns / 1000000000LL;
    ts.tv_nsec = ns % 1000000000LL;

    // Handle EINTR (interrupted by signal) by retrying
    while (::nanosleep(&ts, &ts) == -1 && errno == EINTR) {
        // Continue with remaining time in ts
    }
#endif
}

} // namespace detail

/// @brief Thread wrapper for multithreaded platforms
///
/// This is a simple alias to std::thread for platforms with full threading support.
/// It provides the complete std::thread API including construction with functions,
/// join(), detach(), get_id(), etc.
using thread = std::thread;  // okay std namespace

/// @brief Thread ID type for multithreaded platforms
using thread_id = std::thread::id;  // okay std namespace

/// @brief this_thread namespace for thread-specific operations
namespace this_thread {
    using std::this_thread::get_id;  // okay std namespace
    using std::this_thread::yield;   // okay std namespace

    // Sleep functions - fl::chrono::duration overloads
    template<typename Rep, typename Period>
    inline void sleep_for(const fl::chrono::duration<Rep, Period>& sleep_duration) {
        fl::platforms::detail::native_sleep(sleep_duration);
    }

    // Sleep functions - fl::chrono::time_point overloads
    template<typename Rep, typename Period>
    inline void sleep_until(const fl::chrono::duration<Rep, Period>& wake_time) {
        // For time_point support, we would need fl::chrono::time_point and fl::chrono::clock
        // For now, this is a placeholder - implement when needed
        (void)wake_time;  // Suppress unused parameter warning
        static_assert(sizeof(Rep) == 0, "sleep_until not yet implemented for fl::chrono::time_point");
    }
} // namespace this_thread

} // namespace platforms
} // namespace fl
