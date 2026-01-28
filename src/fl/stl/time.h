#pragma once

/// @file time.h
/// @brief Universal timing functions for FastLED
///
/// This header provides universal `fl::millis()` and `fl::micros()` functions that work consistently
/// across all FastLED-supported platforms. It abstracts away platform-specific
/// timing implementations and provides a clean, testable API.
///
/// @section Basic Usage
/// @code
/// #include "fl/stl/time.h"
///
/// void loop() {
///     fl::u32 now = fl::millis();
///
///     // Use timing for animations
///     static fl::u32 last_update = 0;
///     if (now - last_update >= 16) { // ~60 FPS
///         update_animation();
///         last_update = now;
///     }
/// }
/// @endcode
///
/// @section Testing Support
/// For unit testing, time injection is available:
/// @code
/// #ifdef FASTLED_TESTING
/// #include "fl/stl/time.h"
///
/// TEST(AnimationTest, TimingTest) {
///     fl::MockTimeProvider mock(1000);
///     fl::inject_time_provider(mock);
///
///     EXPECT_EQ(fl::millis(), 1000);
///
///     mock.advance(16);
///     EXPECT_EQ(fl::millis(), 1016);
///
///     fl::clear_time_provider();
/// }
/// #endif
/// @endcode

#include "fl/int.h"

#ifdef FASTLED_TESTING
#include "fl/stl/function.h"
#endif

namespace fl {

/// Universal millisecond timer - returns milliseconds since system startup
///
/// This function provides consistent timing across all FastLED platforms:
/// - Arduino/AVR: Hardware timer-based millis()
/// - ESP32/ESP8266: ESP-IDF timing functions
/// - ARM: Platform-specific ARM timers
/// - WASM: JavaScript-based timing
/// - Native/Testing: std::chrono implementation
/// - Stub: Fake timer for testing
///
/// @return Number of milliseconds since the system started
/// @note Wraps around approximately every 49.7 days (2^32 milliseconds)
/// @note This function is designed to be zero-overhead - it compiles to a direct
///       platform call in optimized builds
///
/// @section Platform Behavior
/// - **Consistent**: All platforms return milliseconds since startup/initialization
/// - **Monotonic**: Time always increases (except on wraparound)
/// - **Resolution**: 1 millisecond resolution on all platforms
/// - **Wraparound**: Consistent wraparound behavior at 2^32 milliseconds
///
/// @section Usage Examples
/// @code
/// // Basic timing
/// fl::u32 start = fl::millis();
/// do_work();
/// fl::u32 elapsed = fl::millis() - start;
///
/// // Animation timing
/// static fl::u32 last_frame = 0;
/// fl::u32 now = fl::millis();
/// if (now - last_frame >= 16) { // 60 FPS
///     render_frame();
///     last_frame = now;
/// }
///
/// // Timeout handling
/// fl::u32 timeout = fl::millis() + 5000; // 5 second timeout
/// while (fl::millis() < timeout && !is_complete()) {
///     process_step();
/// }
/// @endcode
fl::u32 millis();

/// Universal microsecond timer - returns microseconds since system startup
///
/// This function provides consistent high-resolution timing across all FastLED platforms:
/// - Arduino/AVR: Hardware timer-based micros()
/// - ESP32/ESP8266: ESP-IDF timing functions
/// - ARM: Platform-specific ARM timers
/// - WASM: JavaScript-based timing with microsecond precision
/// - Native/Testing: std::chrono implementation
/// - Stub: Fake timer for testing
///
/// @return Number of microseconds since the system started
/// @note Wraps around approximately every 71.6 minutes (2^32 microseconds)
/// @note This function is designed to be zero-overhead - it compiles to a direct
///       platform call in optimized builds
///
/// @section Platform Behavior
/// - **Consistent**: All platforms return microseconds since startup/initialization
/// - **Monotonic**: Time always increases (except on wraparound)
/// - **Resolution**: 1 microsecond resolution on all platforms (platform-dependent precision)
/// - **Wraparound**: Consistent wraparound behavior at 2^32 microseconds
///
/// @section Usage Examples
/// @code
/// // Precise timing measurement
/// fl::u32 start = fl::micros();
/// fast_operation();
/// fl::u32 elapsed = fl::micros() - start;
///
/// // High-precision delay
/// fl::u32 target = fl::micros() + 100; // 100 microseconds
/// while (fl::micros() < target) {
///     // Busy wait
/// }
/// @endcode
fl::u32 micros();

/// 64-bit millisecond timer - returns milliseconds since system startup without wraparound
///
/// This function provides a 64-bit millisecond counter that never wraps around in any
/// practical timeframe (584 million years). It's built on top of fl::millis() and
/// automatically detects and corrects for 32-bit wraparound.
///
/// The implementation uses a function-local static for thread-safe accumulation on
/// platforms that support it. On platforms with native 64-bit timers, this function
/// may be optimized to use the native implementation directly.
///
/// @return Number of milliseconds since system startup as a 64-bit value
/// @note Thread-safe on platforms with proper static initialization guards
/// @note No wraparound in practical usage (584+ million years)
/// @note Slightly higher overhead than millis() due to wraparound detection
///
/// @section Usage Examples
/// @code
/// // Long-running timing without wraparound concerns
/// fl::u64 start = fl::millis64();
/// long_running_operation();
/// fl::u64 elapsed = fl::millis64() - start;
///
/// // Safe duration tracking across 49+ day boundaries
/// static fl::u64 session_start = fl::millis64();
/// fl::u64 uptime = fl::millis64() - session_start; // Never wraps
///
/// // Timestamps for logging and analytics
/// fl::u64 event_time = fl::millis64();
/// log_event("operation_complete", event_time);
/// @endcode
fl::u64 millis64();

/// Alias for millis64() - returns 64-bit millisecond time
///
/// This function is an alias for fl::millis64() and provides a convenient
/// shorter name for getting the 64-bit millisecond timer value.
///
/// @return Number of milliseconds since system startup as a 64-bit value
/// @note Inline function - zero overhead
/// @see millis64() for full documentation
inline fl::u64 time() {
    return fl::millis64();
}

#ifdef FASTLED_TESTING

/// Type alias for time provider functions used in testing
using time_provider_t = fl::function<fl::u32()>;

/// Inject a custom time provider for testing
///
/// This function allows unit tests to control the timing returned by fl::millis().
/// Once injected, all calls to fl::millis() will use the provided function instead
/// of the platform's native timing.
///
/// @param provider Function that returns the current time in milliseconds
/// @note Only available in testing builds (when FASTLED_TESTING is defined)
/// @note Thread-safe: Uses appropriate locking in multi-threaded environments
///
/// @section Example Usage
/// @code
/// fl::MockTimeProvider mock(1000);
/// fl::inject_time_provider(mock);
///
/// ASSERT_EQ(fl::millis(), 1000);
///
/// mock.advance(500);
/// ASSERT_EQ(fl::millis(), 1500);
///
/// fl::clear_time_provider(); // Restore normal timing
/// @endcode
void inject_time_provider(const time_provider_t& provider);

/// Clear the injected time provider and restore platform default timing
///
/// After calling this function, fl::millis() will return to using the platform's
/// native timing implementation.
///
/// @note Only available in testing builds (when FASTLED_TESTING is defined)
/// @note Thread-safe: Uses appropriate locking in multi-threaded environments
/// @note Safe to call multiple times or when no provider is injected
void clear_time_provider();

/// Reset the millis64() internal state for testing
///
/// This function resets the internal accumulation state of millis64() to allow
/// tests to start with a clean state. Should be called between test cases that
/// use MockTimeProvider to ensure consistent behavior.
///
/// @note Only available in testing builds (when FASTLED_TESTING is defined)
/// @note Thread-safe: Uses appropriate locking in multi-threaded environments
void millis64_reset();

/// Mock time provider for controlled testing
///
/// This class provides a controllable time source for unit testing. It maintains
/// an internal time value that can be advanced manually or set to specific values.
///
/// @section Example Usage
/// @code
/// fl::MockTimeProvider mock(1000); // Start at 1000ms
/// fl::inject_time_provider(mock);
///
/// // Test timing-dependent code
/// EXPECT_EQ(fl::millis(), 1000);
///
/// mock.advance(100); // Advance by 100ms
/// EXPECT_EQ(fl::millis(), 1100);
///
/// mock.set_time(5000); // Jump to 5000ms
/// EXPECT_EQ(fl::millis(), 5000);
///
/// fl::clear_time_provider();
/// @endcode
class MockTimeProvider {
public:
    /// Constructor with initial time value
    /// @param initial_time Starting time in milliseconds (default: 0)
    explicit MockTimeProvider(fl::u32 initial_time = 0);
    
    /// Advance the mock time by the specified amount
    /// @param milliseconds Number of milliseconds to advance
    void advance(fl::u32 milliseconds);
    
    /// Set the mock time to a specific value
    /// @param milliseconds New time value in milliseconds
    void set_time(fl::u32 milliseconds);
    
    /// Get the current mock time
    /// @return Current time in milliseconds
    fl::u32 current_time() const;
    
    /// Function call operator for use with inject_time_provider()
    /// @return Current time in milliseconds
    fl::u32 operator()() const;

private:
    fl::u32 mCurrentTime;
};

#endif // FASTLED_TESTING

} // namespace fl 
