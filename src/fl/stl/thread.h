#pragma once

#include "fl/stl/thread_config.h"  // For FASTLED_MULTITHREADED

// Include platform-specific thread implementation
// FASTLED_MULTITHREADED is already defined by thread_config.h
#include "platforms/thread.h"  // IWYU pragma: keep

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
