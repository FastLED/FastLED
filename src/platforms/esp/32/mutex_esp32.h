// ok no namespace fl
// allow-include-after-namespace
#pragma once

// IWYU pragma: private

/// @file platforms/esp/32/mutex_esp32.h
/// @brief ESP32 FreeRTOS mutex implementation
///
/// This header provides ESP32-specific mutex implementations using FreeRTOS mutexes.
/// Locks are the non-throwing fl versions (platforms/shared/nothrow_lock.h).

#include "fl/stl/assert.h"
// IWYU pragma: begin_keep
#include <mutex>  // ok include - needed for std lock tag types
#include "fl/stl/noexcept.h"
// IWYU pragma: end_keep

namespace fl {
namespace platforms {

// Forward declarations
class MutexESP32;
class RecursiveMutexESP32;

// Platform implementation aliases for ESP32
using mutex = MutexESP32;
using recursive_mutex = RecursiveMutexESP32;

// unique_lock / lock_guard come from platforms/shared/nothrow_lock.h (end of
// file), not std: std::unique_lock's __throw_system_error reference pulled
// libstdc++'s locale/iostream objects into every ESP32 sketch (#4798).

// Lock constructor tag types (re-export from std; header-only, no objects)
using std::defer_lock_t;  // okay std namespace
using std::try_to_lock_t;  // okay std namespace
using std::adopt_lock_t;  // okay std namespace
using std::defer_lock;  // okay std namespace
using std::try_to_lock;  // okay std namespace
using std::adopt_lock;  // okay std namespace

// ESP32 FreeRTOS mutex wrapper
class MutexESP32 {
private:
    void* mHandle;  // SemaphoreHandle_t (opaque pointer to avoid including FreeRTOS headers)

public:
    MutexESP32() FL_NO_EXCEPT;
    ~MutexESP32();

    // Non-copyable and non-movable
    MutexESP32(const MutexESP32&) = delete;
    MutexESP32& operator=(const MutexESP32&) = delete;
    MutexESP32(MutexESP32&&) = delete;
    MutexESP32& operator=(MutexESP32&&) = delete;

    void lock() FL_NO_EXCEPT;
    void unlock() FL_NO_EXCEPT;
    bool try_lock() FL_NO_EXCEPT;
};

// ESP32 FreeRTOS recursive mutex wrapper
class RecursiveMutexESP32 {
private:
    void* mHandle;  // SemaphoreHandle_t (opaque pointer to avoid including FreeRTOS headers)

public:
    RecursiveMutexESP32() FL_NO_EXCEPT;
    ~RecursiveMutexESP32();

    // Non-copyable and non-movable
    RecursiveMutexESP32(const RecursiveMutexESP32&) = delete;
    RecursiveMutexESP32& operator=(const RecursiveMutexESP32&) = delete;
    RecursiveMutexESP32(RecursiveMutexESP32&&) = delete;
    RecursiveMutexESP32& operator=(RecursiveMutexESP32&&) = delete;

    void lock() FL_NO_EXCEPT;
    void unlock() FL_NO_EXCEPT;
    bool try_lock() FL_NO_EXCEPT;
};

// Define FASTLED_MULTITHREADED for ESP32 (has FreeRTOS)
#ifndef FASTLED_MULTITHREADED
#define FASTLED_MULTITHREADED 1
#endif

} // namespace platforms
} // namespace fl

#include "platforms/shared/nothrow_lock.h"
