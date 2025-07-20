# FastLED fl::future<T> API Design

## ⚠️ IMPLEMENTATION STATUS

**✅ DESIGN COMPLETE - Ready for Implementation**

This document defines the `fl::future<T>` concurrency primitive for FastLED, providing event-driven, non-blocking programming patterns perfect for embedded systems and LED applications.

## Design Goals

- **Event-Driven**: Perfect for `setup()` + `loop()` + callbacks pattern
- **Non-Blocking**: Never interferes with LED updates or real-time operations  
- **Embedded-Friendly**: Minimal memory footprint and zero overhead when possible
- **Thread-Safe**: Automatic adaptation to single or multi-threaded environments
- **FastLED Style**: Follow existing coding conventions and patterns
- **RAII**: Automatic resource management for futures and shared state
- **Type Safety**: Leverage C++ type system for compile-time safety

## Core Design Philosophy

FastLED uses an **event-driven** approach rather than async/await patterns:
- **No coroutines** - Arduino/embedded systems don't support them
- **No blocking operations** - LED updates must remain responsive
- **Completable futures** - single object with dual producer/consumer interface
- **Copy-on-write semantics** - efficient sharing via `fl::string` and `fl::shared_ptr`

## fl::future<T> Implementation

### **1. Core Interface**

```cpp
namespace fl {

/// Completable future implementation for FastLED
/// Combines consumer and producer interfaces in one object
/// Perfect for event-driven systems without async/await!
template<typename T>
class future {
public:
    /// Default constructor - creates invalid future
    future();
    
    /// Create a valid, pending future
    static future<T> create();
    
    /// Move constructor and assignment (non-copyable like std::future)
    future(future&& other) noexcept;
    future& operator=(future&& other) noexcept;
    
    /// Non-copyable
    future(const future&) = delete;
    future& operator=(const future&) = delete;
    
    // ========== CONSUMER INTERFACE (USER-FACING) ==========
    
    /// Check if future is valid (has shared state)
    bool valid() const;
    
    /// Check if result is ready (non-blocking)
    bool is_ready() const;
    
    /// Check if future has error
    bool has_error() const;
    
    /// Check if future is still pending
    bool is_pending() const;
    
    /// THE KEY METHOD - Non-blocking result access!
    /// Returns optional<T> - empty if not ready, value if ready
    /// This is FastLED's answer to std::future::get() - never blocks!
    fl::optional<T> try_result() const;
    
    /// Get error message if in error state
    const fl::string& error_message() const;
    
    /// Reset future to invalid state (useful for reuse)
    void reset();
    
    // ========== PRODUCER INTERFACE (INTERNAL USE) ==========
    
    /// Complete the future with a result (used by networking library)
    bool complete_with_value(const T& value);
    bool complete_with_value(T&& value);
    
    /// Complete the future with an error (used by networking library)
    bool complete_with_error(const fl::string& error_message);
    bool complete_with_error(const char* error_message);
    
private:
    fl::shared_ptr<detail::CompletableFutureState<T>> mState;
};

} // namespace fl
```

### **2. Shared State Implementation**

```cpp
namespace fl {
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
```

### **3. Convenience Functions**

```cpp
namespace fl {

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

} // namespace fl
```

## Usage Patterns

### **1. Basic Non-Blocking Operations**

```cpp
#include "fl/future.h"

// Create a future for async operation
fl::future<int> calculate_result() {
    auto future = fl::future<int>::create();
    
    // Start async calculation (in background or via events)
    start_background_calculation(future);
    
    return future;
}

void loop() {
    static auto calculation = calculate_result();
    
    // Non-blocking check in main loop
    if (calculation.is_ready()) {
        auto result = calculation.try_result();
        if (result) {
            FL_WARN("Calculation complete: " << *result);
            calculation.reset(); // Reset for next use
        }
    } else if (calculation.has_error()) {
        FL_WARN("Calculation failed: " << calculation.error_message());
        calculation.reset();
    }
    
    // LED updates continue normally
    FastLED.show();
}
```

### **2. HTTP Client Integration**

```cpp
#include "fl/networking/http_client.h"
#include "fl/future.h"

class WeatherService {
private:
    HttpClient mClient;
    fl::future<Response> mWeatherFuture;
    
public:
    void start_weather_update() {
        // Start async HTTP request
        mWeatherFuture = mClient.get_async("http://api.weather.com/current");
    }
    
    void update() {
        // Non-blocking check for weather data
        if (mWeatherFuture.is_ready()) {
            auto response = mWeatherFuture.try_result();
            if (response && response->status_code() == 200) {
                process_weather_data(response->body_text());
            }
            mWeatherFuture.reset(); // Reset for next request
        } else if (mWeatherFuture.has_error()) {
            FL_WARN("Weather request failed: " << mWeatherFuture.error_message());
            mWeatherFuture.reset();
        }
    }
    
private:
    void process_weather_data(const fl::string& json) {
        // Update LED display based on weather
        FL_WARN("Weather data: " << json);
    }
};

// Usage in main loop
WeatherService weather;

void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
    weather.start_weather_update();
}

void loop() {
    // Update weather (non-blocking)
    weather.update();
    
    // Update LED patterns
    update_led_patterns();
    
    // Show LEDs (networking happens during this)
    FastLED.show();
    
    // Start new weather request every 30 seconds
    EVERY_N_SECONDS(30) {
        weather.start_weather_update();
    }
}
```

### **3. Event-Driven Pattern Processing**

```cpp
#include "fl/future.h"

class PatternLoader {
private:
    fl::future<fl::vector<CRGB>> mPatternFuture;
    
public:
    void load_pattern_async(const fl::string& pattern_name) {
        auto future = fl::future<fl::vector<CRGB>>::create();
        
        // Start loading pattern in background
        load_pattern_from_file(pattern_name, future);
        
        mPatternFuture = fl::move(future);
    }
    
    bool check_pattern_ready() {
        if (mPatternFuture.is_ready()) {
            auto pattern_data = mPatternFuture.try_result();
            if (pattern_data) {
                apply_pattern_to_leds(*pattern_data);
                mPatternFuture.reset();
                return true;
            }
        } else if (mPatternFuture.has_error()) {
            FL_WARN("Pattern load failed: " << mPatternFuture.error_message());
            mPatternFuture.reset();
        }
        return false;
    }
    
private:
    void load_pattern_from_file(const fl::string& name, fl::future<fl::vector<CRGB>>& future) {
        // Simulate async file loading (would use actual file system)
        static int load_counter = 0;
        load_counter++;
        
        if (load_counter >= 10) { // Simulate load completion after 10 frames
            fl::vector<CRGB> pattern_data;
            pattern_data.resize(100);
            
            // Fill with some pattern data
            for (int i = 0; i < 100; i++) {
                pattern_data[i] = CHSV(i * 2, 255, 255);
            }
            
            future.complete_with_value(fl::move(pattern_data));
            load_counter = 0;
        }
    }
    
    void apply_pattern_to_leds(const fl::vector<CRGB>& pattern) {
        FL_WARN("Applying new pattern with " << pattern.size() << " colors");
        // Apply pattern to actual LEDs
    }
};
```

### **4. Multiple Async Operations**

```cpp
#include "fl/future.h"

class MultiAsyncExample {
private:
    fl::future<fl::string> mSensorFuture;
    fl::future<fl::string> mApiResponse;
    fl::future<int> mCalculation;
    
public:
    void start_all_operations() {
        // Start multiple async operations
        mSensorFuture = read_sensor_async();
        mApiResponse = fetch_api_data_async();
        mCalculation = calculate_something_async();
    }
    
    void update() {
        // Check all futures non-blockingly
        check_sensor_data();
        check_api_response();
        check_calculation();
    }
    
private:
    void check_sensor_data() {
        if (mSensorFuture.is_ready()) {
            auto data = mSensorFuture.try_result();
            if (data) {
                FL_WARN("Sensor data: " << *data);
                process_sensor_data(*data);
            }
            mSensorFuture.reset();
        }
    }
    
    void check_api_response() {
        if (mApiResponse.is_ready()) {
            auto response = mApiResponse.try_result();
            if (response) {
                FL_WARN("API response: " << *response);
                process_api_response(*response);
            }
            mApiResponse.reset();
        }
    }
    
    void check_calculation() {
        if (mCalculation.is_ready()) {
            auto result = mCalculation.try_result();
            if (result) {
                FL_WARN("Calculation result: " << *result);
                process_calculation_result(*result);
            }
            mCalculation.reset();
        }
    }
    
    // Placeholder async operation starters
    fl::future<fl::string> read_sensor_async() {
        return fl::make_ready_future(fl::string("sensor_data_123"));
    }
    
    fl::future<fl::string> fetch_api_data_async() {
        return fl::make_ready_future(fl::string("{\"api\": \"data\"}"));
    }
    
    fl::future<int> calculate_something_async() {
        return fl::make_ready_future(42);
    }
    
    void process_sensor_data(const fl::string& data) { /* Process sensor data */ }
    void process_api_response(const fl::string& response) { /* Process API response */ }
    void process_calculation_result(int result) { /* Process calculation */ }
};
```

## Why This Design is Perfect for FastLED

### **Event-Driven, Not Async/Await**

**FastLED Pattern:**
- FastLED uses `setup()` + `loop()` + callbacks  
- No coroutines, no `await` keyword, no suspension points
- **Completable future** is the right pattern for this model

**Single Object, Dual Interface:**
- **Consumer side** (users): `try_result()`, `is_ready()`, etc.
- **Producer side** (libraries): `complete_with_value()`, `complete_with_error()`
- **No promises needed** - just one clean object

### **Automatic Thread Safety**

- Uses existing `fl::mutex` infrastructure
- **Zero overhead** on single-threaded platforms (default)
- **Full thread safety** when `FASTLED_MULTITHREADED=1`
- **No performance penalty** for embedded systems

### **Memory Efficient**

- **Shared state** with `fl::shared_ptr` for efficient copying
- **Copy-on-write** semantics for strings and complex data
- **Small footprint** - only allocates when futures are created
- **RAII cleanup** - automatic resource management

### **Integration with FastLED Patterns**

```cpp
void loop() {
    // Check async operations (non-blocking)
    check_network_requests();
    check_sensor_readings();
    check_pattern_loading();
    
    // Update LED patterns (normal FastLED code)
    update_led_animations();
    
    // Show LEDs (async work happens during this call via EngineEvents)
    FastLED.show();
}
```

## Implementation Location

The `fl::future<T>` implementation will be located at:

```
src/fl/
├── future.h                 # Main fl::future<T> interface
├── future.cpp               # Implementation details
└── detail/
    └── future_state.h       # CompletableFutureState implementation
```

## Dependencies

The `fl::future<T>` implementation depends on existing FastLED infrastructure:

- `fl::mutex` and `fl::lock_guard` for thread safety
- `fl::shared_ptr` for shared state management
- `fl::optional<T>` for non-blocking result access
- `fl::string` for error messages
- `fl::move` for efficient value semantics

## Testing Requirements

Comprehensive testing for the future implementation:

```cpp
// tests/test_future.cpp
void test_future_basic_usage() {
    // Test creation, completion, and retrieval
}

void test_future_thread_safety() {
    // Test concurrent access from multiple threads
}

void test_future_error_handling() {
    // Test error completion and retrieval
}

void test_future_with_networking() {
    // Test integration with HTTP client/server
}
```

The `fl::future<T>` design provides the foundation for all asynchronous operations in FastLED while maintaining the library's core principles of simplicity, performance, and embedded-system compatibility. 
