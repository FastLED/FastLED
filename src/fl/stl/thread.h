#pragma once

#include "fl/has_include.h"

// Include platform-specific thread implementation
// This will define FASTLED_MULTITHREADED based on platform capabilities
// Platforms can override FASTLED_MULTITHREADED detection in platforms/thread.h
#include "platforms/thread.h"  // IWYU pragma: keep

#ifndef FASTLED_MULTITHREADED
// Default detection: enable multithreading for testing/stub builds with pthread support
// WASM uses this same profile (stub-compatible: pthread support for testing/simulation)
#if defined(FASTLED_TESTING) && FL_HAS_INCLUDE(<pthread.h>)
#define FASTLED_MULTITHREADED 1
#else
#define FASTLED_MULTITHREADED 0
#endif
#endif  // FASTLED_MULTITHREADED

// Define FASTLED_USE_THREAD_LOCAL based on FASTLED_MULTITHREADED
#ifndef FASTLED_USE_THREAD_LOCAL
#define FASTLED_USE_THREAD_LOCAL FASTLED_MULTITHREADED
#endif  // FASTLED_USE_THREAD_LOCAL

namespace fl {

// Bind platform implementations to fl:: namespace

/// @brief Thread abstraction for FastLED
///
/// This is a platform-independent thread wrapper that delegates to:
/// - fl::platforms::thread (std::thread wrapper) on multithreaded platforms
/// - fl::platforms::ThreadFake (synchronous execution) on single-threaded platforms
///
/// Usage:
/// ```cpp
/// fl::thread t([]() {
///     // Thread work here
/// });
/// t.join();
/// ```
using thread = fl::platforms::thread;

/// @brief Thread ID type
using thread_id = fl::platforms::thread_id;

/// @brief this_thread namespace for thread-specific operations
namespace this_thread {
    using fl::platforms::this_thread::get_id;
    using fl::platforms::this_thread::yield;
    using fl::platforms::this_thread::sleep_for;
    using fl::platforms::this_thread::sleep_until;
} // namespace this_thread

} // namespace fl
