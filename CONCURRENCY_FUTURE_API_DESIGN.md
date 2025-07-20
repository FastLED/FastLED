# FastLED Concurrency & Future API Design

## Overview

This document outlines the design for FastLED's concurrency primitives, with a focus on the **completable future** implementation that enables non-blocking, event-driven programming patterns perfect for embedded systems.

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

# fl::future Implementation

The FastLED concurrency API uses a custom **completable future** implementation that's perfectly designed for event-driven systems without async/await:

## Complete fl::future Design with Shared String Semantics

```cpp
// src/fl/future.h
#pragma once

#include "fl/mutex.h"
#include "fl/optional.h"
#include "fl/move.h"
#include "fl/type_traits.h"
#include "fl/assert.h"
#include "fl/shared_ptr.h"
#include "fl/string.h"

namespace fl {

namespace detail {

/// Shared state for completable future
/// Thread-safe using fl::mutex (fake or real based on FASTLED_MULTITHREADED)
template<typename T>
class CompletableFutureState {
public:
    enum State { PENDING, READY, ERROR };
    
    CompletableFutureState() : mState(PENDING) {}
    
    // Thread-safe state checking (consumer interface)
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
    
    /// Complete the future with an error (const char* convenience overload)
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
        return future<T>(fl::make_shared<detail::CompletableFutureState<T>>());
    }
    
    /// Move constructor
    future(future&& other) noexcept : mState(fl::move(other.mState)) {}
    
    /// Move assignment
    future& operator=(future&& other) noexcept {
        if (this != &other) {
            mState = fl::move(other.mState);
        }
        return *this;
    }
    
    /// Non-copyable (like std::future)
    future(const future&) = delete;
    future& operator=(const future&) = delete;
    
    // ========== CONSUMER INTERFACE (USER-FACING) ==========
    
    /// Check if future is valid (has shared state)
    bool valid() const {
        return mState != nullptr;
    }
    
    /// Check if result is ready (non-blocking)
    bool is_ready() const {
        return valid() && mState->is_ready();
    }
    
    /// Check if future has error
    bool has_error() const {
        return valid() && mState->has_error();
    }
    
    /// Check if future is still pending
    bool is_pending() const {
        return valid() && mState->is_pending();
    }
    
    /// THE KEY METHOD - Non-blocking result access!
    /// Returns optional<T> - empty if not ready, value if ready
    /// This is FastLED's answer to std::future::get() - never blocks!
    fl::optional<T> try_result() const {
        if (!valid()) {
            return fl::optional<T>();
        }
        return mState->try_get_result();
    }
    
    /// Get error message if in error state
    const fl::string& error_message() const {
        if (!valid()) {
            static const fl::string invalid_msg("Invalid future");
            return invalid_msg;
        }
        return mState->error_message();
    }
    
    /// Reset future to invalid state (useful for reuse)
    void reset() {
        mState.reset();
    }
    
    // ========== PRODUCER INTERFACE (INTERNAL USE) ==========
    
    /// Complete the future with a result (used by networking library)
    /// Returns true if completion was successful, false if already completed
    bool complete_with_value(const T& value) {
        return valid() ? mState->set_result(value) : false;
    }
    
    bool complete_with_value(T&& value) {
        return valid() ? mState->set_result(fl::move(value)) : false;
    }
    
    /// Complete the future with an error (used by networking library)
    /// Returns true if completion was successful, false if already completed
    bool complete_with_error(const fl::string& error_message) {
        return valid() ? mState->set_error(error_message) : false;
    }
    
    /// Complete the future with an error (const char* convenience overload)
    bool complete_with_error(const char* error_message) {
        return valid() ? mState->set_error(error_message) : false;
    }
    
private:
    /// Constructor from shared state (used by create())
    explicit future(fl::shared_ptr<detail::CompletableFutureState<T>> state) 
        : mState(fl::move(state)) {}
    
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

} // namespace fl 
```

## Why This Is Perfect for FastLED

### Event-Driven, Not Async/Await

**FastLED Pattern:**
- FastLED uses `setup()` + `loop()` + callbacks  
- No coroutines, no `await` keyword, no suspension points
- **Completable future** is the right pattern for this model

**Single Object, Dual Interface:**
- **Consumer side** (users): `try_result()`, `is_ready()`, etc.
- **Producer side** (libraries): `complete_with_value()`, `complete_with_error()`
- **No promises needed** - just one clean object

**Automatic Thread Safety:**
- Uses existing `fl::mutex` infrastructure
- Zero overhead on single-threaded platforms (default)
- Full thread safety when `FASTLED_MULTITHREADED=1`

### Memory Efficiency with Shared String Semantics

**fl::string Benefits:**
- **Copy-on-write semantics**: Copying `fl::string` only copies a shared_ptr (8-16 bytes)
- **Small string optimization**: Strings ≤64 bytes stored inline (no heap allocation)  
- **Shared ownership**: Multiple strings can share the same buffer efficiently
- **Automatic memory management**: RAII cleanup via shared_ptr
- **Thread-safe sharing**: Safe to share between threads with copy-on-write

**Shared Error Messages:**
```cpp
// Shared error messages across multiple futures - only ONE copy in memory!
fl::string network_timeout_error("Connection timeout after 30 seconds");

auto future1 = fl::make_error_future<Response>(network_timeout_error);
auto future2 = fl::make_error_future<Response>(network_timeout_error); 
auto future3 = fl::make_error_future<Response>(network_timeout_error);
// All three futures share the same error string buffer via copy-on-write!
```

## Integration Examples

### Simplified Library Integration

```cpp
class AsyncLibrary {
private:
    // Store futures for completion (much simpler than promises!)
    fl::vector<fl::future<Response>> mActiveFutures;
    
public:
    fl::future<Response> start_operation(const char* url) {
        // Create pending future
        auto future = fl::future<Response>::create();
        
        // Store for completion later (copy is fine - shared state)
        fl::size future_index = mActiveFutures.size();
        mActiveFutures.push_back(future);
        
        // Start async operation
        start_background_operation(url, future_index);
        
        return future; // Return immediately (non-blocking)
    }
    
private:
    void on_operation_complete(fl::size future_index, const Response& response) {
        // Complete the future (thread-safe)
        if (future_index < mActiveFutures.size()) {
            mActiveFutures[future_index].complete_with_value(response);
        }
    }
    
    void on_operation_error(fl::size future_index, const fl::string& error) {
        // Complete with error (thread-safe)
        if (future_index < mActiveFutures.size()) {
            mActiveFutures[future_index].complete_with_error(error);
        }
    }
};
```

## Usage Patterns

### Basic Non-Blocking Pattern

```cpp
fl::future<Response> api_future;

void setup() {
    // Start async operation
    api_future = async_library.start_operation("http://api.example.com/data");
}

void loop() {
    // Check if result is ready (never blocks!)
    if (api_future.is_ready()) {
        auto result = api_future.try_result();
        if (result) {
            FL_WARN("Got response: " << result->data());
        }
        api_future.reset(); // Clear for reuse
    }
    
    FastLED.show(); // Always responsive
}
```

### Error Handling with Shared String Benefits

```cpp
void loop() {
    if (api_future.is_ready()) {
        auto result = api_future.try_result();
        if (result) {
            // Efficient string sharing - can store response without copy!
            fl::string response_data = result->text(); // Cheap shared_ptr copy
            FL_WARN("Success: " << response_data);
            cache_response(response_data);             // Share same buffer
        }
    } else if (api_future.has_error()) {
        // Error message sharing - can store/log efficiently
        const fl::string& error = api_future.error_message(); 
        FL_WARN("Error: " << error);
        log_error_to_storage(error);  // Shared buffer - no extra memory!
        api_future.reset();
    }
}
```

### Multiple Futures Pattern

```cpp
fl::vector<fl::future<Response>> pending_requests;

void setup() {
    // Start multiple async operations
    pending_requests.push_back(client.get("http://weather.api.com/current"));
    pending_requests.push_back(client.get("http://sensor.api.com/readings"));
    pending_requests.push_back(client.get("http://config.api.com/settings"));
}

void loop() {
    // Check all futures in parallel - never blocks!
    for (auto it = pending_requests.begin(); it != pending_requests.end();) {
        if (it->is_ready()) {
            auto result = it->try_result();
            if (result) {
                FL_WARN("Request completed successfully");
                
                // Efficient response sharing - can store/cache without memory waste
                fl::string response_text = result->text(); // Shared buffer copy
                process_response(*result);
                cache_response_data(response_text);        // Same buffer shared
            }
            it = pending_requests.erase(it);
        } else if (it->has_error()) {
            // Error message can be stored/logged efficiently via shared buffer
            const fl::string& error = it->error_message();
            FL_WARN("Request failed: " << error);
            log_error_for_debugging(error);  // Shared buffer - no copy needed!
            it = pending_requests.erase(it);
        } else {
            ++it;
        }
    }
    
    FastLED.show();
}
```

## Comparison: Before vs After

### ❌ Before (Promise/Future Pair)

```cpp
// Internal complexity - promises everywhere
fl::promise<Response> promise;
auto future = promise.get_future();
mActivePromises.push_back(fl::move(promise));
// Later: mActivePromises[i].set_value(response);
```

### ✅ After (Completable Future)

```cpp
// Much simpler - just one object
auto future = fl::future<Response>::create();
mActiveFutures.push_back(future);
// Later: mActiveFutures[i].complete_with_value(response);
```

## Memory Efficiency

### Minimal Footprint

- **Future object**: ~8-16 bytes (just a shared_ptr)
- **Shared state**: ~32-64 bytes (result + metadata)
- **No extra promise objects** taking up memory
- **Automatic cleanup** via RAII

### Factory Pattern with Shared String Ownership

```cpp
// Clean factory methods for common cases
auto ready_future = fl::make_ready_future(response);

// Error futures with shared string ownership - multiple futures can share error messages!
fl::string timeout_error("Network timeout after 30 seconds");
auto future1 = fl::make_error_future<Response>(timeout_error);     // Shared buffer
auto future2 = fl::make_error_future<Response>(timeout_error);     // Same buffer!
auto future3 = fl::make_error_future<Response>("Network timeout"); // Convenience overload

// Invalid future
auto invalid_future = fl::make_invalid_future<Response>();
```

## Platform-Specific Optimizations

### AVR/Arduino (Single-threaded)

```cpp
// Mutex operations become no-ops (zero overhead)
fl::lock_guard<fl::mutex> lock(mMutex); // Compiles to nothing
```

### ESP32/Testing (Multi-threaded)

```cpp
// Real mutex operations when FASTLED_MULTITHREADED=1
fl::lock_guard<fl::mutex> lock(mMutex); // Real std::recursive_mutex
```

## Advanced Patterns

### Future Composition

```cpp
class ComposedOperation {
private:
    fl::future<Response> mStep1Future;
    fl::future<Response> mStep2Future;
    fl::future<Response> mFinalFuture;
    
public:
    fl::future<Response> start_multi_step_operation() {
        mFinalFuture = fl::future<Response>::create();
        
        // Start first step
        mStep1Future = library.start_step1();
        
        return mFinalFuture;
    }
    
    void check_progress() {
        if (mStep1Future.is_ready() && !mStep2Future.valid()) {
            // Step 1 complete, start step 2
            auto step1_result = mStep1Future.try_result();
            if (step1_result) {
                mStep2Future = library.start_step2(*step1_result);
            }
        }
        
        if (mStep2Future.is_ready()) {
            // Final step complete
            auto step2_result = mStep2Future.try_result();
            if (step2_result) {
                mFinalFuture.complete_with_value(*step2_result);
            } else {
                mFinalFuture.complete_with_error(mStep2Future.error_message());
            }
        }
    }
};
```

### Future Pool Management

```cpp
class FuturePool {
private:
    fl::vector<fl::future<Response>> mActiveFutures;
    fl::size mMaxConcurrent = 4;
    
public:
    bool try_add_future(fl::future<Response> future) {
        cleanup_completed_futures();
        
        if (mActiveFutures.size() < mMaxConcurrent) {
            mActiveFutures.push_back(fl::move(future));
            return true;
        }
        return false; // Pool full
    }
    
    void pump_all() {
        for (auto& future : mActiveFutures) {
            if (future.is_ready()) {
                auto result = future.try_result();
                if (result) {
                    handle_success(*result);
                }
            } else if (future.has_error()) {
                handle_error(future.error_message());
            }
        }
        cleanup_completed_futures();
    }
    
private:
    void cleanup_completed_futures() {
        mActiveFutures.erase(
            fl::remove_if(mActiveFutures.begin(), mActiveFutures.end(),
                         [](const fl::future<Response>& f) { 
                             return !f.is_pending(); 
                         }),
            mActiveFutures.end()
        );
    }
};
```

## 🎯 Result: Perfect for Embedded Event-Driven Systems

This **completable future with shared string semantics** design is exactly what FastLED needs:

1. **✅ No promises** - eliminates unnecessary complexity
2. **✅ Single object** - easier to understand and use
3. **✅ Event-driven** - perfect for `setup()`/`loop()` pattern
4. **✅ Non-blocking** - never interferes with LED updates
5. **✅ Thread-safe** - automatic adaptation to platform threading
6. **✅ Memory efficient** - shared buffers + copy-on-write for embedded systems
7. **✅ Shared ownership** - `fl::string` references can be stored/shared efficiently
8. **✅ FastLED-style** - follows existing patterns and conventions

### Key Design Wins

- **Users call `try_result()` and get shared string responses** - no API complexity
- **Error messages shared efficiently** across multiple futures and callbacks  
- **Response text uses shared buffers** - store, cache, log without memory waste
- **Automatic string management** - no manual memory handling needed
- **Thread-safe sharing** - copy-on-write handles concurrent access safely

**Perfect for embedded systems** - all the benefits of enterprise async programming with **zero overhead** when single-threaded and **shared memory efficiency** when multi-threaded! 🚀 
