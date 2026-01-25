#pragma once

/// @file fl/stl/chrono.h
/// @brief FastLED chrono implementation - duration types for time measurements
///
/// This header provides a minimal chrono implementation compatible with std::chrono,
/// allowing FastLED to use duration types without directly depending on <chrono>.
/// The implementation is intentionally compatible with std::chrono for interoperability.

#include "fl/int.h"
#include "fl/stl/ratio.h"

namespace fl {
namespace chrono {

/// @brief Represents a time duration
/// @tparam Rep The arithmetic type representing the number of ticks
/// @tparam Period A fl::ratio representing the tick period (seconds per tick)
///
/// This is a simplified version of std::chrono::duration that provides:
/// - Storage for a duration value
/// - Implicit conversion from compatible duration types
/// - count() method to extract the tick count
///
/// Example:
/// @code
/// fl::chrono::milliseconds ms(1000);  // 1000 milliseconds
/// fl::u32 count = ms.count();          // returns 1000
/// @endcode
template<typename Rep, typename Period = fl::ratio<1>>
class duration {
public:
    using rep = Rep;
    using period = Period;

    /// Default constructor - zero duration
    constexpr duration() : mCount(0) {}

    /// Explicit constructor from tick count
    /// @param count Number of ticks
    constexpr explicit duration(const Rep& count) : mCount(count) {}

    /// Implicit conversion constructor from compatible duration types
    /// @tparam Rep2 Source tick type
    /// @tparam Period2 Source tick period
    template<typename Rep2, typename Period2>
    constexpr duration(const duration<Rep2, Period2>& d)
        : mCount(duration_cast_impl<Rep, Period, Rep2, Period2>(d.count())) {}

    /// Get the tick count
    /// @return Number of ticks stored in this duration
    constexpr Rep count() const { return mCount; }

private:
    Rep mCount;

    /// Internal duration_cast implementation
    template<typename ToRep, typename ToPeriod, typename FromRep, typename FromPeriod>
    static constexpr ToRep duration_cast_impl(FromRep count) {
        // Convert from source period to destination period
        // target_count = source_count * (FromPeriod / ToPeriod)
        // Using: target = source * FromPeriod::num * ToPeriod::den / (FromPeriod::den * ToPeriod::num)
        using ratio = fl::ratio_divide<FromPeriod, ToPeriod>;
        return static_cast<ToRep>(
            static_cast<FromRep>(count) * static_cast<FromRep>(ratio::num) / static_cast<FromRep>(ratio::den)
        );
    }
};

/// @brief Cast one duration type to another
/// @tparam ToDuration Target duration type
/// @tparam Rep Source tick type
/// @tparam Period Source tick period
/// @param d Source duration
/// @return Duration converted to target type
///
/// Example:
/// @code
/// fl::chrono::seconds s(1);
/// auto ms = fl::chrono::duration_cast<fl::chrono::milliseconds>(s);
/// // ms.count() == 1000
/// @endcode
template<typename ToDuration, typename Rep, typename Period>
constexpr ToDuration duration_cast(const duration<Rep, Period>& d) {
    return ToDuration(d);
}

// Common duration type aliases using standard SI ratios
// These are compatible with std::chrono duration types

/// Nanoseconds - duration with period of 1/1,000,000,000 seconds
using nanoseconds = duration<fl::i64, fl::nano>;

/// Microseconds - duration with period of 1/1,000,000 seconds
using microseconds = duration<fl::i64, fl::micro>;

/// Milliseconds - duration with period of 1/1,000 seconds
using milliseconds = duration<fl::i64, fl::milli>;

/// Seconds - duration with period of 1 second
using seconds = duration<fl::i64>;

/// Minutes - duration with period of 60 seconds
using minutes = duration<fl::i32, fl::ratio<60>>;

/// Hours - duration with period of 3600 seconds
using hours = duration<fl::i32, fl::ratio<3600>>;

} // namespace chrono
} // namespace fl
