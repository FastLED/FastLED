// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/stub/condition_variable_stub.h
/// @brief Stub/fallback condition variable implementation
///
/// This header provides a condition variable implementation for platforms without
/// custom implementations. Uses std::condition_variable when available (C++11+),
/// or provides a fake implementation for single-threaded environments.

#include "fl/stl/assert.h"

// Check if we have C++11 threading support
#if __cplusplus >= 201103L && !defined(FASTLED_STUB_NO_STD_THREAD)
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

    // Note: wait_for and wait_until are not implemented in fake version
    // as they would require timing facilities and are not commonly used
    // in single-threaded scenarios
};

using condition_variable = ConditionVariableFake;

// Define FASTLED_MULTITHREADED as 0 for fake implementation
#ifndef FASTLED_MULTITHREADED
#define FASTLED_MULTITHREADED 0
#endif

#endif // FASTLED_STUB_HAS_CONDITION_VARIABLE

} // namespace platforms
} // namespace fl
