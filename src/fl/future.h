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
/// auto result = future.try_get_result();
/// if (result.is<int>()) {
///     int value = *result.ptr<int>(); // value = 42
/// } else if (result.is<fl::FutureError>()) {
///     auto error = *result.ptr<fl::FutureError>();
///     handle_error(error.message);
/// } else {
///     // Still pending
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
///     // Check weather future non-blockingly
///     auto weather_result = weather_future.try_get_result();
///     if (weather_result.is<fl::string>()) {
///         process_weather(*weather_result.ptr<fl::string>());
///         weather_future.clear();
///     } else if (weather_result.is<fl::FutureError>()) {
///         handle_weather_error(weather_result.ptr<fl::FutureError>()->message);
///         weather_future.clear();
///     }
///     
///     // Check color future non-blockingly
///     auto color_result = color_future.try_get_result();
///     if (color_result.is<CRGB>()) {
///         update_led_color(*color_result.ptr<CRGB>());
///         color_future.clear();
///     } else if (color_result.is<fl::FutureError>()) {
///         handle_color_error(color_result.ptr<fl::FutureError>()->message);
///         color_future.clear();
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
///     auto response = api_request.try_get_result();
///     if (response.is<fl::string>()) {
///         update_leds_from_api(*response.ptr<fl::string>());
///         api_request.clear();
///     } else if (response.is<fl::FutureError>()) {
///         handle_network_error(response.ptr<fl::FutureError>()->message);
///         api_request.clear();
///     }
///     // else: still pending, try again next loop
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
/// auto result = future.try_get_result();
/// if (result.is<fl::FutureError>()) {
///     fl::string error = result.ptr<fl::FutureError>()->message; // "Network timeout"
///     handle_error(error);
/// } else if (result.is<int>()) {
///     int value = *result.ptr<int>();
///     // Handle success case
/// }
/// // else: still pending
/// @endcode
///
/// @section Visitor Pattern Support
/// For complex state handling, use the visitor pattern:
/// @code
/// auto result = future.try_get_result();
/// 
/// struct FutureVisitor {
///     void accept(const int& value) {
///         // Handle successful result
///         update_display(value);
///     }
///     void accept(const fl::FutureError& error) {
///         // Handle error case
///         show_error_message(error.message);
///     }
///     void accept(const fl::FuturePending& pending) {
///         // Handle pending case
///         show_loading_indicator();
///     }
/// };
/// 
/// FutureVisitor visitor;
/// result.visit(visitor);
/// @endcode

#include "fl/namespace.h"
#include "fl/shared_ptr.h"
#include "fl/string.h"
#include "fl/optional.h"
#include "fl/move.h"
#include "fl/mutex.h"
#include "fl/variant.h"

namespace fl {

/// Tag type representing a pending future
struct FuturePending {};

/// Error type for futures
struct FutureError {
    fl::string message;
    
    FutureError() = default;
    FutureError(const fl::string& msg) : message(msg) {}
    FutureError(const char* msg) : message(msg) {}
    FutureError(fl::string&& msg) : message(fl::move(msg)) {}
};

/// Result type that combines value, error, or pending state in one variant
template<typename T>
using FutureResult = fl::Variant<T, FutureError, FuturePending>;

/// Future state enum - forces explicit handling of all cases
enum class FutureState {
    PENDING,  // Future is still pending completion
    READY,    // Future completed successfully with value
    ERROR     // Future completed with error
};

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
    
    /// Get the current state - forces explicit state handling!
    FutureState state() const {
        if (!valid()) return FutureState::PENDING;
        return mState->get_state();
    }
    
    /// Boolean conversion - true if there's something to process (READY or ERROR)
    /// Forces users to check state() and handle both success and error cases
    explicit operator bool() const {
        if (!valid()) return false;
        FutureState s = state();
        return s == FutureState::READY || s == FutureState::ERROR;
    }
    
    /// THE KEY METHOD - Non-blocking result access with variant!
    /// Returns FutureResult<T> - contains T, FutureError, or FuturePending
    /// This is FastLED's ergonomic answer to std::future::get() - never blocks!
    FutureResult<T> try_get_result() const {
        if (!valid()) return FutureResult<T>(FuturePending{});
        return mState->try_get_result();
    }
    
    /// Legacy method - Non-blocking result access!
    /// Returns optional<T> - empty if not ready, value if ready
    /// @deprecated Use try_get_result() instead for better error handling
    fl::optional<T> try_result() const {
        if (!valid()) return fl::optional<T>();
        return mState->try_get_result_legacy();
    }
    
    /// Get error message if in error state
    const fl::string& error_message() const {
        if (!valid()) {
            static const fl::string empty_msg;
            return empty_msg;
        }
        return mState->error_message();
    }
    
    /// Clear future to invalid state (useful for reuse)
    void clear() {
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
    CompletableFutureState() : mState(FutureState::PENDING) {}
    
    /// Thread-safe state checking (consumer interface)
    FutureState get_state() const {
        fl::lock_guard<fl::mutex> lock(mMutex);
        return mState;
    }
    
    /// Non-blocking result access with variant - never blocks!
    FutureResult<T> try_get_result() const {
        fl::lock_guard<fl::mutex> lock(mMutex);
        if (mState == FutureState::READY) {
            return FutureResult<T>(mResult);
        } else if (mState == FutureState::ERROR) {
            return FutureResult<T>(FutureError(mErrorMessage));
        } else {
            return FutureResult<T>(FuturePending{});
        }
    }
    
    /// Legacy non-blocking result access - never blocks!
    fl::optional<T> try_get_result_legacy() const {
        fl::lock_guard<fl::mutex> lock(mMutex);
        if (mState == FutureState::READY) {
            return fl::optional<T>(mResult);
        }
        return fl::optional<T>();
    }
    
    /// Get error message if in error state
    const fl::string& error_message() const {
        fl::lock_guard<fl::mutex> lock(mMutex);
        if (mState == FutureState::ERROR) {
            return mErrorMessage;
        } else {
            static const fl::string empty_msg;
            return empty_msg;
        }
    }
    
    /// Complete the future with a result (producer interface)
    bool set_result(const T& result) {
        fl::lock_guard<fl::mutex> lock(mMutex);
        if (mState == FutureState::PENDING) {
            mResult = result;
            mState = FutureState::READY;
            return true;
        }
        return false; // Already completed
    }
    
    bool set_result(T&& result) {
        fl::lock_guard<fl::mutex> lock(mMutex);
        if (mState == FutureState::PENDING) {
            mResult = fl::move(result);
            mState = FutureState::READY;
            return true;
        }
        return false; // Already completed
    }
    
    /// Complete the future with an error (producer interface)
    bool set_error(const fl::string& message) {
        fl::lock_guard<fl::mutex> lock(mMutex);
        if (mState == FutureState::PENDING) {
            mErrorMessage = message;
            mState = FutureState::ERROR;
            return true;
        }
        return false; // Already completed
    }
    
    bool set_error(const char* message) {
        fl::lock_guard<fl::mutex> lock(mMutex);
        if (mState == FutureState::PENDING) {
            mErrorMessage = message;  // Implicit conversion to fl::string
            mState = FutureState::ERROR;
            return true;
        }
        return false; // Already completed
    }
    
private:
    mutable fl::mutex mMutex;  // Mutable for const methods
    FutureState mState;
    T mResult;
    fl::string mErrorMessage;
};

} // namespace detail

} // namespace fl 
