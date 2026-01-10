#pragma once

/// @file fl/stl/mutex.h
/// @brief Platform-independent mutex interface
///
/// This header provides fl::mutex, fl::recursive_mutex, and fl::unique_lock by
/// delegating to platform-specific implementations in platforms/mutex.h.
///
/// The platform layer handles all platform-specific details including:
/// - FASTLED_MULTITHREADED detection and definition
/// - Mutex implementation (FreeRTOS, pthread, std::mutex, or no-op)
/// - unique_lock implementation (std::unique_lock or custom)
/// - Lock constructor tag types (defer_lock, try_to_lock, adopt_lock)

#include "platforms/mutex.h"  // IWYU pragma: keep

namespace fl {

// Bind platform implementations to fl:: namespace
using mutex = fl::platforms::mutex;
using recursive_mutex = fl::platforms::recursive_mutex;

// Bind unique_lock from platform layer
using fl::platforms::unique_lock;

// Bring lock constructor tags into fl:: namespace
using fl::platforms::defer_lock;
using fl::platforms::try_to_lock;
using fl::platforms::adopt_lock;
using fl::platforms::defer_lock_t;
using fl::platforms::try_to_lock_t;
using fl::platforms::adopt_lock_t;

} // namespace fl
