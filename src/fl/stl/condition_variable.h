#pragma once

#include "fl/stl/thread.h"
#include "fl/stl/mutex.h"
#include "fl/stl/assert.h"

#if FASTLED_MULTITHREADED
#include <condition_variable>  // ok include
#endif

namespace fl {

// Forward declaration for single-threaded mode
template <typename T> class ConditionVariableFake;

#if FASTLED_MULTITHREADED
// Alias to std::condition_variable for full compatibility with standard library
using condition_variable = std::condition_variable;  // okay std namespace

// Also provide cv_status enum for compatibility
using cv_status = std::cv_status;  // okay std namespace
#else
// Fake condition_variable for single-threaded mode
// In single-threaded mode, there is no actual waiting or notification
// since there's only one thread of execution
template <typename T> class ConditionVariableFake {
  public:
    ConditionVariableFake() = default;

    // Non-copyable and non-movable
    ConditionVariableFake(const ConditionVariableFake&) = delete;
    ConditionVariableFake& operator=(const ConditionVariableFake&) = delete;
    ConditionVariableFake(ConditionVariableFake&&) = delete;
    ConditionVariableFake& operator=(ConditionVariableFake&&) = delete;

    // Wait operations - in single-threaded mode, these are no-ops
    // since there's no other thread to signal us
    void notify_one() noexcept {
        // No-op in single-threaded mode
    }

    void notify_all() noexcept {
        // No-op in single-threaded mode
    }

    void wait(unique_lock<mutex>& lock) {
        // In single-threaded mode, waiting would deadlock
        // since no other thread can signal us
        FL_ASSERT(false, "ConditionVariableFake::wait() called in single-threaded mode would deadlock");
    }

    template<typename Predicate>
    void wait(unique_lock<mutex>& lock, Predicate pred) {
        // In single-threaded mode, waiting would deadlock
        FL_ASSERT(false, "ConditionVariableFake::wait(pred) called in single-threaded mode would deadlock");
    }

    // Note: wait_for and wait_until are not implemented in fake version
    // as they would require timing facilities and are not commonly used
    // in this codebase's single-threaded scenarios
};

using condition_variable = ConditionVariableFake<void>;

// Fake cv_status enum for single-threaded mode
enum class cv_status {
    no_timeout,
    timeout
};

#endif // FASTLED_MULTITHREADED

} // namespace fl
