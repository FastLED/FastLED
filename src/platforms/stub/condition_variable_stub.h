// ok no namespace fl
// allow-include-after-namespace
#pragma once

// IWYU pragma: private

/// @file platforms/stub/condition_variable_stub.h
/// @brief Stub/fallback condition variable implementation
///
/// This header provides a condition variable implementation for platforms without
/// custom implementations. Uses std::condition_variable when available (C++11+),
/// or provides a fake implementation for single-threaded environments.

#include "fl/stl/assert.h"
#include "fl/stl/thread_config.h"  // For FASTLED_MULTITHREADED

// Use FASTLED_MULTITHREADED for detection (consistent with mutex_stub.h)
// Previously checked __cplusplus >= 201103L which could get out of sync with
// mutex detection on platforms like WASM without pthreads.
#if FASTLED_MULTITHREADED
    #include <condition_variable>  // ok include
    #define FASTLED_STUB_HAS_CONDITION_VARIABLE 1
#else
    #define FASTLED_STUB_HAS_CONDITION_VARIABLE 0
#endif

namespace fl {
namespace platforms {

#if FASTLED_STUB_HAS_CONDITION_VARIABLE

//=============================================================================
// C++11+ Implementation: Use std::condition_variable
//=============================================================================

// Directly use std::condition_variable for compatibility
using condition_variable = std::condition_variable;  // okay std namespace
using cv_status = std::cv_status;  // okay std namespace

// Define FASTLED_MULTITHREADED for platforms with std::condition_variable
#ifndef FASTLED_MULTITHREADED
#define FASTLED_MULTITHREADED 1
#endif

#else

//=============================================================================
// Fake Implementation for Single-Threaded Environments
//=============================================================================

// Forward declarations
template<typename Mutex> class unique_lock;

// cv_status enum for fake implementation
enum class cv_status {
    no_timeout,
    timeout
};

/// @brief Fake condition variable for single-threaded mode
///
/// In single-threaded mode, there is no actual waiting or notification
/// since there's only one thread of execution. Wait operations will
/// trigger assertions as they would cause deadlocks.
class ConditionVariableFake {
public:
    ConditionVariableFake() = default;

    // Non-copyable and non-movable
    ConditionVariableFake(const ConditionVariableFake&) = delete;
    ConditionVariableFake& operator=(const ConditionVariableFake&) = delete;
    ConditionVariableFake(ConditionVariableFake&&) = delete;
    ConditionVariableFake& operator=(ConditionVariableFake&&) = delete;

    /// @brief Notify operations are no-ops in single-threaded mode
    void notify_one() noexcept {
        // No-op in single-threaded mode
    }

    void notify_all() noexcept {
        // No-op in single-threaded mode
    }

    /// @brief Wait would deadlock in single-threaded mode
    template<typename Mutex>
    void wait(unique_lock<Mutex>& lock) {
        FL_ASSERT(false, "ConditionVariableFake::wait() called in single-threaded mode would deadlock");
    }

    template<typename Mutex, typename Predicate>
    void wait(unique_lock<Mutex>& lock, Predicate pred) {
        FL_ASSERT(false, "ConditionVariableFake::wait(pred) called in single-threaded mode would deadlock");
    }

    /// @brief wait_for would deadlock in single-threaded mode
    /// Templated on Duration to avoid requiring <chrono> include in fake implementation
    template<typename Mutex, typename Duration>
    cv_status wait_for(unique_lock<Mutex>& lock, const Duration& timeout_duration) {
        FL_ASSERT(false, "ConditionVariableFake::wait_for() called in single-threaded mode would deadlock");
        return cv_status::timeout;
    }
};

using condition_variable = ConditionVariableFake;

// Define FASTLED_MULTITHREADED as 0 for fake implementation
#ifndef FASTLED_MULTITHREADED
#define FASTLED_MULTITHREADED 0
#endif

#endif // FASTLED_STUB_HAS_CONDITION_VARIABLE

} // namespace platforms
} // namespace fl
