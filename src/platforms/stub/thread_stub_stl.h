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

// FASTLED_MULTITHREADED is defined by fl/stl/thread_config.h
// This file provides the STL-based thread implementation for multithreaded platforms

namespace fl {
namespace platforms {

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

    // Sleep functions
    template<typename Rep, typename Period>
    inline void sleep_for(const std::chrono::duration<Rep, Period>& sleep_duration) {  // okay std namespace
        std::this_thread::sleep_for(sleep_duration);  // okay std namespace
    }

    template<typename Clock, typename Duration>
    inline void sleep_until(const std::chrono::time_point<Clock, Duration>& sleep_time) {  // okay std namespace
        std::this_thread::sleep_until(sleep_time);  // okay std namespace
    }
} // namespace this_thread

} // namespace platforms
} // namespace fl
