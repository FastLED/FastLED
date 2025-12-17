/// @file timeout.h
/// @brief Generic timeout class for tracking elapsed time with rollover-safe arithmetic
///
/// Provides a single generic Timeout class that works with any time units.
/// The caller is responsible for providing timestamps in consistent units
/// (e.g., from micros(), millis(), or any other monotonic counter).
///
/// Handles uint32_t timestamp rollover correctly using unsigned arithmetic.

#pragma once

#include "fl/stl/stdint.h"

namespace fl {

/// @brief Generic timeout timer with rollover-safe arithmetic
///
/// Tracks elapsed time using provided timestamps. Time units are determined
/// by the caller (microseconds, milliseconds, clock ticks, etc.).
///
/// Handles uint32_t rollover correctly - works across 0xFFFFFFFF → 0x00000000 boundary.
///
/// Example with microseconds:
/// @code
/// Timeout timeout(micros(), 50);  // 50 microseconds duration
/// while (!timeout.done(micros())) {
///     // Wait for timeout to complete
/// }
/// @endcode
///
/// Example with milliseconds:
/// @code
/// Timeout timeout(millis(), 1000);  // 1 second duration
/// while (!timeout.done(millis())) {
///     // Wait for timeout to complete
/// }
/// @endcode
class Timeout {
public:
    /// @brief Default constructor - creates an already-expired timeout
    Timeout() : mStartTime(0), mDuration(0) {}

    /// @brief Construct a timeout with specified start time and duration
    /// @param start_time Start timestamp (in any consistent units)
    /// @param duration Duration (in same units as start_time)
    Timeout(uint32_t start_time, uint32_t duration)
        : mStartTime(start_time), mDuration(duration) {}

    /// @brief Check if the timeout has completed
    /// @param current_time Current timestamp (in same units as constructor)
    /// @return true if elapsed time >= duration, false otherwise
    /// @note Handles uint32_t rollover correctly via unsigned arithmetic
    bool done(uint32_t current_time) const {
        uint32_t elapsed_time = current_time - mStartTime;  // Rollover-safe
        return elapsed_time >= mDuration;
    }

    /// @brief Get elapsed time since timeout started
    /// @param current_time Current timestamp (in same units as constructor)
    /// @return Elapsed time (in same units as constructor)
    uint32_t elapsed(uint32_t current_time) const {
        return current_time - mStartTime;  // Rollover-safe
    }

    /// @brief Reset the timeout to start counting from specified time
    /// @param start_time New start timestamp
    void reset(uint32_t start_time) {
        mStartTime = start_time;
    }

    /// @brief Reset with a new start time and duration
    /// @param start_time New start timestamp
    /// @param duration New duration (in same units as start_time)
    void reset(uint32_t start_time, uint32_t duration) {
        mStartTime = start_time;
        mDuration = duration;
    }

private:
    uint32_t mStartTime;  ///< Start timestamp
    uint32_t mDuration;   ///< Timeout duration
};

}  // namespace fl
