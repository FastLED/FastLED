#pragma once

/// @file future.h
/// @brief Event-driven, non-blocking concurrency primitives for FastLED
///
/// The fl::future<T> API provides completable futures perfect for Arduino's setup() + loop()
/// programming model. Unlike async/await patterns, futures work seamlessly with event-driven
/// embedded systems without blocking LED updates.
///
/// @section Key Features
/// - **Event-Driven**: Perfect for setup() + loop() + callbacks pattern
/// - **Non-Blocking**: Never interferes with LED updates or real-time operations  
/// - **Thread-Safe**: Automatic adaptation to single/multi-threaded environments
/// - **Embedded-Friendly**: Minimal memory footprint, zero overhead when possible
///
/// @section Basic Usage
/// @code
/// // Create a pending future
/// auto future = fl::future<int>::create();
/// 
/// // Complete with value (producer side)
/// future.complete_with_value(42);
/// 
/// // Check result non-blockingly (consumer side)
/// if (future) { // operator bool() - true when ready
///     auto result = future.try_result();
///     if (!result.empty()) {
///         int value = *result.ptr(); // value = 42
///     }
/// }
/// @endcode
///
/// @section Event-Driven Pattern
/// The key advantage is non-blocking operation in the main loop:
/// @code
/// fl::future<fl::string> weather_future;
/// fl::future<CRGB> color_future;
/// 
/// void setup() {
///     FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
///     weather_future = start_weather_request();
///     color_future = start_color_calculation();
/// }
/// 
/// void loop() {
///     // Check futures non-blockingly  
///     if (weather_future) { // Concise syntax using operator bool()
///         auto result = weather_future.try_result();
///         if (!result.empty()) process_weather(*result.ptr());
///         weather_future.reset();
///     }
///     
///     if (color_future) { // operator bool() returns true when ready
///         auto result = color_future.try_result();
///         if (!result.empty()) update_led_color(*result.ptr());
///         color_future.reset();
///     }
///     
///     // LEDs update smoothly - never blocked!
///     FastLED.show();
/// }
/// @endcode
///
/// @section Real-World Applications
/// Perfect for HTTP requests, file I/O, sensor readings, and audio processing:
/// @code
/// // HTTP client integration
/// void loop() {
///     static auto api_request = http_client.get_async("http://api.example.com/data");
///     
///     if (api_request) { // Simple boolean check - true when ready
///         auto response = api_request.try_result();
///         if (!response.empty()) update_leds_from_api(*response.ptr());
///         api_request.reset();
///     }
///     
///     FastLED.show(); // Never blocked by network I/O!
/// }
/// @endcode
///
/// @section Error Handling
/// @code
/// auto future = fl::future<int>::create();
/// future.complete_with_error("Network timeout");
/// 
/// if (future.has_error()) {
///     fl::string error = future.error_message(); // "Network timeout"
///     handle_error(error);
/// }
/// @endcode

#include "fl/namespace.h"
#include "fl/shared_ptr.h"
#include "fl/string.h"
#include "fl/optional.h"
#include "fl/move.h"
#include "fl/mutex.h"

namespace fl {

// Forward declaration for shared state
namespace detail {
    template<typename T>
    class CompletableFutureState;
}

/// Completable future implementation for FastLED
/// Combines consumer and producer interfaces in one object
/// Perfect for event-driven systems without async/await!
template<typename T>
class future {
public:
    /// Default constructor - creates invalid future
    future() : mState(nullptr) {}
    
    /// Create a valid, pending future
    static future<T> create() {
        auto state = fl::make_shared<detail::CompletableFutureState<T>>();
        future<T> f;
        f.mState = state;
        return f;
    }
    
    /// Move constructor (non-copyable like std::future)
    future(future&& other) noexcept : mState(fl::move(other.mState)) {
        other.mState.reset();
    }
    
    /// Move assignment operator
    future& operator=(future&& other) noexcept {
        if (this != &other) {
            mState = fl::move(other.mState);
            other.mState.reset();
        }
        return *this;
    }
    
    /// Non-copyable
    future(const future&) = delete;
    future& operator=(const future&) = delete;
    
    // ========== CONSUMER INTERFACE (USER-FACING) ==========
    
    /// Check if future is valid (has shared state)
    bool valid() const {
        return mState != nullptr;
    }
    
    /// Check if result is ready (non-blocking)
    bool is_ready() const {
        if (!valid()) return false;
        return mState->is_ready();
    }
    
    /// Check if future has error
    bool has_error() const {
        if (!valid()) return false;
        return mState->has_error();
    }
    
    /// Check if future is still pending
    bool is_pending() const {
        if (!valid()) return false;
        return mState->is_pending();
    }
    
    /// Boolean conversion operator - returns true if future is ready
    /// Allows for simple if (future) { ... } syntax
    explicit operator bool() const {
        return is_ready();
    }
    
    /// THE KEY METHOD - Non-blocking result access!
    /// Returns optional<T> - empty if not ready, value if ready
    /// This is FastLED's answer to std::future::get() - never blocks!
    fl::optional<T> try_result() const {
        if (!valid()) return fl::optional<T>();
        return mState->try_get_result();
    }
    
    /// Get error message if in error state
    const fl::string& error_message() const {
        if (!valid()) {
            static const fl::string empty_msg;
            return empty_msg;
        }
        return mState->error_message();
    }
    
    /// Reset future to invalid state (useful for reuse)
    void reset() {
        mState.reset();
    }
    
    // ========== PRODUCER INTERFACE (INTERNAL USE) ==========
    
    /// Complete the future with a result (used by networking library)
    bool complete_with_value(const T& value) {
        if (!valid()) return false;
        return mState->set_result(value);
    }
    
    bool complete_with_value(T&& value) {
        if (!valid()) return false;
        return mState->set_result(fl::move(value));
    }
    
    /// Complete the future with an error (used by networking library)
    bool complete_with_error(const fl::string& error_message) {
        if (!valid()) return false;
        return mState->set_error(error_message);
    }
    
    bool complete_with_error(const char* error_message) {
        if (!valid()) return false;
        return mState->set_error(error_message);
    }
    
private:
    fl::shared_ptr<detail::CompletableFutureState<T>> mState;
};

/// Convenience function to create a ready future
template<typename T>
future<T> make_ready_future(T value) {
    auto f = future<T>::create();
    f.complete_with_value(fl::move(value));
    return f;
}

/// Convenience function to create an error future
template<typename T>
future<T> make_error_future(const fl::string& error_message) {
    auto f = future<T>::create();
    f.complete_with_error(error_message);
    return f;
}

/// Convenience function to create an error future (const char* overload)
template<typename T>
future<T> make_error_future(const char* error_message) {
    auto f = future<T>::create();
    f.complete_with_error(error_message);
    return f;
}

/// Convenience function to create an invalid/empty future
template<typename T>
future<T> make_invalid_future() {
    return future<T>();
}

// ============================================================================
// IMPLEMENTATION DETAILS
// ============================================================================

namespace detail {

/// Thread-safe shared state for completable future
/// Uses fl::mutex (fake or real based on FASTLED_MULTITHREADED)
template<typename T>
class CompletableFutureState {
public:
    enum State { PENDING, READY, ERROR };
    
    CompletableFutureState() : mState(PENDING) {}
    
    /// Thread-safe state checking (consumer interface)
    bool is_ready() const {
        fl::lock_guard<fl::mutex> lock(mMutex);
        return mState == READY;
    }
    
    bool has_error() const {
        fl::lock_guard<fl::mutex> lock(mMutex);
        return mState == ERROR;
    }
    
    bool is_pending() const {
        fl::lock_guard<fl::mutex> lock(mMutex);
        return mState == PENDING;
    }
    
    /// Non-blocking result access - never blocks!
    fl::optional<T> try_get_result() const {
        fl::lock_guard<fl::mutex> lock(mMutex);
        if (mState == READY) {
            return fl::optional<T>(mResult);
        }
        return fl::optional<T>();
    }
    
    /// Get error message if in error state
    const fl::string& error_message() const {
        fl::lock_guard<fl::mutex> lock(mMutex);
        if (mState == ERROR) {
            return mErrorMessage;
        } else {
            static const fl::string empty_msg;
            return empty_msg;
        }
    }
    
    /// Complete the future with a result (producer interface)
    bool set_result(const T& result) {
        fl::lock_guard<fl::mutex> lock(mMutex);
        if (mState == PENDING) {
            mResult = result;
            mState = READY;
            return true;
        }
        return false; // Already completed
    }
    
    bool set_result(T&& result) {
        fl::lock_guard<fl::mutex> lock(mMutex);
        if (mState == PENDING) {
            mResult = fl::move(result);
            mState = READY;
            return true;
        }
        return false; // Already completed
    }
    
    /// Complete the future with an error (producer interface)
    bool set_error(const fl::string& message) {
        fl::lock_guard<fl::mutex> lock(mMutex);
        if (mState == PENDING) {
            mErrorMessage = message;
            mState = ERROR;
            return true;
        }
        return false; // Already completed
    }
    
    bool set_error(const char* message) {
        fl::lock_guard<fl::mutex> lock(mMutex);
        if (mState == PENDING) {
            mErrorMessage = message;  // Implicit conversion to fl::string
            mState = ERROR;
            return true;
        }
        return false; // Already completed
    }
    
private:
    mutable fl::mutex mMutex;  // Mutable for const methods
    State mState;
    T mResult;
    fl::string mErrorMessage;
};

} // namespace detail

} // namespace fl 
