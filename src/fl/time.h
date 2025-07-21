#pragma once

/// @file time.h
/// @brief Universal timing functions for FastLED
///
/// This header provides a universal `fl::time()` function that works consistently
/// across all FastLED-supported platforms. It abstracts away platform-specific
/// timing implementations and provides a clean, testable API.
///
/// @section Basic Usage
/// @code
/// #include "fl/time.h"
/// 
/// void loop() {
///     fl::u32 now = fl::time();
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
/// #include "fl/time.h"
/// 
/// TEST(AnimationTest, TimingTest) {
///     fl::MockTimeProvider mock(1000);
///     fl::inject_time_provider(mock);
///     
///     EXPECT_EQ(fl::time(), 1000);
///     
///     mock.advance(16);
///     EXPECT_EQ(fl::time(), 1016);
///     
///     fl::clear_time_provider();
/// }
/// #endif
/// @endcode

#include "fl/namespace.h"
#include "fl/int.h"

#ifdef FASTLED_TESTING
#include "fl/function.h"
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
/// fl::u32 start = fl::time();
/// do_work();
/// fl::u32 elapsed = fl::time() - start;
/// 
/// // Animation timing
/// static fl::u32 last_frame = 0;
/// fl::u32 now = fl::time();
/// if (now - last_frame >= 16) { // 60 FPS
///     render_frame();
///     last_frame = now;
/// }
/// 
/// // Timeout handling
/// fl::u32 timeout = fl::time() + 5000; // 5 second timeout
/// while (fl::time() < timeout && !is_complete()) {
///     process_step();
/// }
/// @endcode
fl::u32 time();

#ifdef FASTLED_TESTING

/// Type alias for time provider functions used in testing
using time_provider_t = fl::function<fl::u32()>;

/// Inject a custom time provider for testing
/// 
/// This function allows unit tests to control the timing returned by fl::time().
/// Once injected, all calls to fl::time() will use the provided function instead
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
/// ASSERT_EQ(fl::time(), 1000);
/// 
/// mock.advance(500);
/// ASSERT_EQ(fl::time(), 1500);
/// 
/// fl::clear_time_provider(); // Restore normal timing
/// @endcode
void inject_time_provider(const time_provider_t& provider);

/// Clear the injected time provider and restore platform default timing
/// 
/// After calling this function, fl::time() will return to using the platform's
/// native timing implementation.
///
/// @note Only available in testing builds (when FASTLED_TESTING is defined)
/// @note Thread-safe: Uses appropriate locking in multi-threaded environments
/// @note Safe to call multiple times or when no provider is injected
void clear_time_provider();

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
/// EXPECT_EQ(fl::time(), 1000);
/// 
/// mock.advance(100); // Advance by 100ms
/// EXPECT_EQ(fl::time(), 1100);
/// 
/// mock.set_time(5000); // Jump to 5000ms
/// EXPECT_EQ(fl::time(), 5000);
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
