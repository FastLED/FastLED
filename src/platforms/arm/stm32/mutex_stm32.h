// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/arm/stm32/mutex_stm32.h
/// @brief STM32 FreeRTOS mutex implementation
///
/// This header provides STM32-specific mutex implementations using FreeRTOS mutexes.
/// For STM32 with FreeRTOS, we use std::unique_lock for full compatibility with condition variables.

#include "fl/stl/assert.h"
#include <mutex>  // ok include - needed for std::unique_lock

namespace fl {
namespace platforms {

// Forward declarations
class MutexSTM32;
class RecursiveMutexSTM32;

// Platform implementation aliases for STM32
using mutex = MutexSTM32;
using recursive_mutex = RecursiveMutexSTM32;

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

// STM32 FreeRTOS mutex wrapper
class MutexSTM32 {
private:
    void* mHandle;  // SemaphoreHandle_t (opaque pointer to avoid including FreeRTOS headers)

public:
    MutexSTM32();
    ~MutexSTM32();

    // Non-copyable and non-movable
    MutexSTM32(const MutexSTM32&) = delete;
    MutexSTM32& operator=(const MutexSTM32&) = delete;
    MutexSTM32(MutexSTM32&&) = delete;
    MutexSTM32& operator=(MutexSTM32&&) = delete;

    void lock();
    void unlock();
    bool try_lock();
};

// STM32 FreeRTOS recursive mutex wrapper
class RecursiveMutexSTM32 {
private:
    void* mHandle;  // SemaphoreHandle_t (opaque pointer to avoid including FreeRTOS headers)

public:
    RecursiveMutexSTM32();
    ~RecursiveMutexSTM32();

    // Non-copyable and non-movable
    RecursiveMutexSTM32(const RecursiveMutexSTM32&) = delete;
    RecursiveMutexSTM32& operator=(const RecursiveMutexSTM32&) = delete;
    RecursiveMutexSTM32(RecursiveMutexSTM32&&) = delete;
    RecursiveMutexSTM32& operator=(RecursiveMutexSTM32&&) = delete;

    void lock();
    void unlock();
    bool try_lock();
};

// Define FASTLED_MULTITHREADED for STM32 with FreeRTOS
#ifndef FASTLED_MULTITHREADED
#define FASTLED_MULTITHREADED 1
#endif

} // namespace platforms
} // namespace fl
