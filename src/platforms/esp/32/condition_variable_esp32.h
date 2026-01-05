// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/esp/32/condition_variable_esp32.h
/// @brief ESP32 FreeRTOS condition variable implementation
///
/// This header provides ESP32-specific condition variable implementation using FreeRTOS primitives.
/// Implements a condition variable compatible with the C++11 std::condition_variable interface.

#include "fl/stl/assert.h"

// CRITICAL: Include mutex_esp32.h BEFORE opening namespace to avoid polluting std::
// mutex_esp32.h includes <mutex> which defines types in global std:: namespace
#include "mutex_esp32.h"

namespace fl {
namespace platforms {

// Forward declarations
class ConditionVariableESP32;

// Platform implementation alias for ESP32
using condition_variable = ConditionVariableESP32;

// cv_status enum for wait_for and wait_until return values
enum class cv_status {
    no_timeout,
    timeout
};

/// @brief ESP32 FreeRTOS condition variable wrapper
///
/// This class provides a condition variable implementation using FreeRTOS task notifications.
/// Compatible with the C++11 std::condition_variable interface.
///
/// Implementation note: FreeRTOS does not have native condition variables, so we implement
/// them using task notifications and a wait queue. This implementation supports:
/// - Multiple waiting tasks
/// - notify_one() and notify_all()
/// - Predicate-based wait operations
/// - Timed wait operations
class ConditionVariableESP32 {
private:
    void* mMutex;      // Internal mutex for wait queue synchronization
    void* mWaitQueue;  // Queue handle for waiting tasks (opaque pointer)

public:
    /// @brief Construct a condition variable
    ConditionVariableESP32();

    ~ConditionVariableESP32();

    // Non-copyable and non-movable
    ConditionVariableESP32(const ConditionVariableESP32&) = delete;
    ConditionVariableESP32& operator=(const ConditionVariableESP32&) = delete;
    ConditionVariableESP32(ConditionVariableESP32&&) = delete;
    ConditionVariableESP32& operator=(ConditionVariableESP32&&) = delete;

    /// @brief Notify one waiting thread
    void notify_one() noexcept;

    /// @brief Notify all waiting threads
    void notify_all() noexcept;

    /// @brief Wait on the condition variable
    /// @tparam Mutex The mutex type (must be compatible with fl::platforms::mutex)
    /// @param lock The unique_lock that must be locked by the current thread
    template<typename Mutex>
    void wait(unique_lock<Mutex>& lock);

    /// @brief Wait on the condition variable with a predicate
    /// @tparam Mutex The mutex type
    /// @tparam Predicate A callable that returns bool
    /// @param lock The unique_lock that must be locked by the current thread
    /// @param pred Predicate that returns false if waiting should continue
    template<typename Mutex, typename Predicate>
    void wait(unique_lock<Mutex>& lock, Predicate pred);

    /// @brief Wait with a timeout
    /// @tparam Mutex The mutex type
    /// @tparam Rep Duration representation type
    /// @tparam Period Duration period type
    /// @param lock The unique_lock that must be locked by the current thread
    /// @param rel_time Maximum time to wait
    /// @return cv_status::timeout if the timeout expired, cv_status::no_timeout otherwise
    template<typename Mutex, typename Rep, typename Period>
    cv_status wait_for(unique_lock<Mutex>& lock,
                       const std::chrono::duration<Rep, Period>& rel_time);  // okay std namespace

    /// @brief Wait with a timeout and predicate
    /// @tparam Mutex The mutex type
    /// @tparam Rep Duration representation type
    /// @tparam Period Duration period type
    /// @tparam Predicate A callable that returns bool
    /// @param lock The unique_lock that must be locked by the current thread
    /// @param rel_time Maximum time to wait
    /// @param pred Predicate that returns false if waiting should continue
    /// @return true if pred() is true, false if timeout expired
    template<typename Mutex, typename Rep, typename Period, typename Predicate>
    bool wait_for(unique_lock<Mutex>& lock,
                  const std::chrono::duration<Rep, Period>& rel_time,  // okay std namespace
                  Predicate pred);

    /// @brief Wait until an absolute time point
    /// @tparam Mutex The mutex type
    /// @tparam Clock Clock type
    /// @tparam Duration Duration type
    /// @param lock The unique_lock that must be locked by the current thread
    /// @param abs_time Absolute time point to wait until
    /// @return cv_status::timeout if the timeout expired, cv_status::no_timeout otherwise
    template<typename Mutex, typename Clock, typename Duration>
    cv_status wait_until(unique_lock<Mutex>& lock,
                         const std::chrono::time_point<Clock, Duration>& abs_time);  // okay std namespace

    /// @brief Wait until an absolute time point with predicate
    /// @tparam Mutex The mutex type
    /// @tparam Clock Clock type
    /// @tparam Duration Duration type
    /// @tparam Predicate A callable that returns bool
    /// @param lock The unique_lock that must be locked by the current thread
    /// @param abs_time Absolute time point to wait until
    /// @param pred Predicate that returns false if waiting should continue
    /// @return true if pred() is true, false if timeout expired
    template<typename Mutex, typename Clock, typename Duration, typename Predicate>
    bool wait_until(unique_lock<Mutex>& lock,
                    const std::chrono::time_point<Clock, Duration>& abs_time,  // okay std namespace
                    Predicate pred);
};

// Define FASTLED_MULTITHREADED for ESP32 (has FreeRTOS)
#ifndef FASTLED_MULTITHREADED
#define FASTLED_MULTITHREADED 1
#endif

} // namespace platforms
} // namespace fl
