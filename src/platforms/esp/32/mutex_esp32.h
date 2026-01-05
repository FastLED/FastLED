// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/esp/32/mutex_esp32.h
/// @brief ESP32 FreeRTOS mutex implementation
///
/// This header provides ESP32-specific mutex implementations using FreeRTOS mutexes.
/// For ESP32, we use std::unique_lock for full compatibility with condition variables.

#include "fl/stl/assert.h"
#include <mutex>  // ok include - needed for std::unique_lock

namespace fl {
namespace platforms {

// Forward declarations
class MutexESP32;
class RecursiveMutexESP32;

// Platform implementation aliases for ESP32
using mutex = MutexESP32;
using recursive_mutex = RecursiveMutexESP32;

// Use std::unique_lock for full compatibility with std::condition_variable
template<typename Mutex>
using unique_lock = std::unique_lock<Mutex>;  // okay std namespace

// Lock constructor tag types (re-export from std)
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
    MutexESP32();
    ~MutexESP32();

    // Non-copyable and non-movable
    MutexESP32(const MutexESP32&) = delete;
    MutexESP32& operator=(const MutexESP32&) = delete;
    MutexESP32(MutexESP32&&) = delete;
    MutexESP32& operator=(MutexESP32&&) = delete;

    void lock();
    void unlock();
    bool try_lock();
};

// ESP32 FreeRTOS recursive mutex wrapper
class RecursiveMutexESP32 {
private:
    void* mHandle;  // SemaphoreHandle_t (opaque pointer to avoid including FreeRTOS headers)

public:
    RecursiveMutexESP32();
    ~RecursiveMutexESP32();

    // Non-copyable and non-movable
    RecursiveMutexESP32(const RecursiveMutexESP32&) = delete;
    RecursiveMutexESP32& operator=(const RecursiveMutexESP32&) = delete;
    RecursiveMutexESP32(RecursiveMutexESP32&&) = delete;
    RecursiveMutexESP32& operator=(RecursiveMutexESP32&&) = delete;

    void lock();
    void unlock();
    bool try_lock();
};

// Define FASTLED_MULTITHREADED for ESP32 (has FreeRTOS)
#ifndef FASTLED_MULTITHREADED
#define FASTLED_MULTITHREADED 1
#endif

} // namespace platforms
} // namespace fl
