#pragma once

// Include platform-specific semaphore implementation
// This will define FASTLED_MULTITHREADED based on platform capabilities
#include "platforms/semaphore.h"  // IWYU pragma: keep

namespace fl {

// Bind platform implementations to fl:: namespace

/// @brief Counting semaphore abstraction for FastLED
///
/// This is a platform-independent counting semaphore wrapper that delegates to:
/// - fl::platforms::counting_semaphore (std::counting_semaphore or mutex/cv-based) on multithreaded platforms
/// - fl::platforms::CountingSemaphoreFake (synchronous, non-blocking) on single-threaded platforms
///
/// Usage:
/// ```cpp
/// fl::counting_semaphore<10> sem(5);  // Max value 10, initial count 5
/// sem.acquire();   // Decrement count (blocks if count == 0 on multithreaded platforms)
/// sem.release();   // Increment count
/// ```
template<ptrdiff_t LeastMaxValue = 1>
using counting_semaphore = fl::platforms::counting_semaphore<LeastMaxValue>;

/// @brief Binary semaphore abstraction for FastLED
///
/// A counting semaphore with a maximum value of 1. Commonly used for signaling.
///
/// Usage:
/// ```cpp
/// fl::binary_semaphore sem(0);  // Initial count 0
/// sem.release();  // Signal
/// sem.acquire();  // Wait for signal
/// ```
using binary_semaphore = fl::platforms::binary_semaphore;

} // namespace fl
