# FastLED WASM JavaScript Fetch API

## Overview

This module provides a simple, fluent HTTP fetch API for FastLED WASM applications using **native JavaScript fetch()** with async callbacks to C++. The implementation allows JavaScript to handle HTTP requests and call back into async C++ functions when responses arrive.

## Features

- **Cross-Platform API**: Unified `fl/fetch.h` header works on WASM and Arduino platforms
- **Simple Function Interface**: `fl::fetch(url, callback)` - no platform-specific code needed
- **Fluent API**: Advanced `fetch.get(url).response(callback)` interface for WASM builds
- **Concurrent Requests**: Full support for multiple simultaneous HTTP requests (WASM only)
- **Thread Safety**: Uses mutex-protected request ID system for safe concurrent access
- **JavaScript Native**: Uses browser's native `fetch()` for maximum compatibility
- **Async Callbacks**: JavaScript can call back into C++ async functions
- **Error Handling**: Comprehensive error reporting for network and HTTP errors
- **CORS Support**: Works with CORS-enabled endpoints
- **Memory Safe**: Automatic cleanup of callbacks and resources with no race conditions

## Usage

### Cross-Platform API (Recommended)

```cpp
#include "fl/fetch.h"

void setup() {
    // Simple cross-platform fetch - works on WASM and Arduino!
    fl::fetch("https://httpbin.org/json", [](const fl::response& resp) {
        if (resp.ok()) {
            FL_WARN("✅ Success: " << resp.text());
        } else {
            FL_WARN("❌ Error: " << resp.text());
        }
    });
}

void loop() {
    // FastLED loop continues normally
    // On WASM: Fetch responses are handled asynchronously via JavaScript
    // On Arduino: Immediate error response (no network blocking)
}
```

### WASM-Specific API (Advanced)

```cpp
#include "platforms/wasm/js_fetch.h"  // WASM only

void setup() {
    // WASM-only fluent API
    fl::fetch.get("https://httpbin.org/json")
        .response([](const fl::response& resp) {
            FL_WARN("Response received: " << resp.text());
        });
}
```

### Concurrent Requests

```cpp
void setup() {
    // Multiple requests can be made simultaneously - each gets a unique ID
    
    fl::fetch.get("https://httpbin.org/json")
        .response([](const fl::response& resp) {
            FL_WARN("Request 1 completed: " << resp.text());
        });
    
    fl::fetch.get("https://fastled.io")
        .response([](const fl::response& resp) {
            FL_WARN("Request 2 completed: " << resp.text());
        });
    
    fl::fetch.get("https://httpbin.org/uuid")
        .response([](const fl::response& resp) {
            FL_WARN("Request 3 completed: " << resp.text());
        });
    
    // All three requests run concurrently - responses can arrive in any order
    // No race conditions or lost callbacks!
}
```

### Error Handling

```cpp
fl::fetch.get("https://invalid-url-that-fails.com/test")
    .response([](const fl::response& resp) {
        if (!resp.ok()) {
            FL_WARN("Request failed: " << resp.text());
        } else {
            FL_WARN("Request succeeded: " << resp.text());
        }
    });
```

## Technical Details

### Architecture

1. **C++ Request**: `fl::fetch.get(url).response(callback)` stores callback and calls JavaScript
2. **JavaScript Fetch**: `EM_JS` function uses native `fetch()` API
3. **Async Response**: JavaScript calls back to C++ via exported functions
4. **C++ Callback**: Original user callback is invoked with response data

### Implementation Components

- **`js_fetch.h`**: Header with fluent API classes (`Fetch`, `FetchRequest`)
- **`js_fetch.cpp`**: Implementation with EM_JS bridge and C++ callbacks
- **`js_fetch_async()`**: EM_JS function that performs JavaScript fetch with request ID
- **`js_fetch_success_callback()`**: C++ function called by JavaScript on success (with request ID)
- **`js_fetch_error_callback()`**: C++ function called by JavaScript on error (with request ID)

### Concurrent Request Architecture

The fetch system uses a **unique request ID approach** with a **singleton callback manager** to enable multiple simultaneous requests:

1. **Request ID Generation**: Each `fetch.get().response()` call generates a unique `uint32_t` request ID
2. **Singleton Management**: A private `FetchCallbackManager` singleton handles all callback storage using `fl::Singleton<T>`
3. **Thread-Safe Storage**: Callbacks are stored directly in `fl::hash_map<uint32_t, FetchResponseCallback>` with move semantics
4. **JavaScript Coordination**: The EM_JS function receives the request ID and passes it to the JavaScript fetch
5. **Response Routing**: JavaScript calls back to C++ with the request ID, allowing proper callback retrieval
6. **Atomic Cleanup**: `takeCallback()` atomically moves and returns the callback via `fl::optional`, preventing race conditions

```cpp
// Internal singleton class for managing fetch callbacks (private to .cpp file)
class FetchCallbackManager {
public:
    uint32_t generateRequestId();
    void storeCallback(uint32_t request_id, FetchResponseCallback callback);  // move semantics
    fl::optional<FetchResponseCallback> takeCallback(uint32_t request_id);    // move semantics
private:
    fl::hash_map<uint32_t, FetchResponseCallback> mPendingCallbacks;  // direct storage
    fl::mutex mCallbacksMutex;
    uint32_t mNextRequestId;
};

// Accessed via: fl::Singleton<FetchCallbackManager>::instance()
```

**Benefits over Global Callback:**
- ✅ **Unlimited concurrent requests** (was: only 1 at a time)
- ✅ **No race conditions** (was: 2nd request deleted 1st callback)
- ✅ **Thread-safe access** (was: unsafe global state)
- ✅ **Modern C++ memory management** (move semantics, no raw pointers/new/delete)
- ✅ **Exception-safe** (RAII via fl::function and fl::optional)
- ✅ **Same fluent API** (no breaking changes)

### JavaScript Integration

The implementation uses these key patterns:

```javascript
// EM_JS function in C++
EM_JS(void, js_fetch_async, (const char* url), {
    fetch(UTF8ToString(url))
        .then(response => response.text())
        .then(text => {
            // Call back to C++
            Module._js_fetch_success_callback(stringToUTF8OnStack(text));
        })
        .catch(error => {
            Module._js_fetch_error_callback(stringToUTF8OnStack(error.message));
        });
});
```

```cpp
// C++ callback functions exported to JavaScript
extern "C" EMSCRIPTEN_KEEPALIVE void js_fetch_success_callback(const char* content);
extern "C" EMSCRIPTEN_KEEPALIVE void js_fetch_error_callback(const char* error_message);
```

## Requirements

### Build Configuration

The following Emscripten flags are required in `build_flags.toml`:

```toml
# Export callback functions for JavaScript
"-sEXPORTED_FUNCTIONS=[..., '_js_fetch_success_callback', '_js_fetch_error_callback']"

# Threading support for async operations
"-pthread"
"-sUSE_PTHREADS=1" 
"-sPROXY_TO_PTHREAD"

# Asyncify for async function support
"-sASYNCIFY=1"
"-sASYNCIFY_EXPORTS=['_main','_extern_setup','_extern_loop']"
```

### Browser Requirements

- Modern browser with native `fetch()` support
- JavaScript enabled
- CORS configuration for cross-origin requests

## Example Projects

- **`examples/NetTest/NetTest.ino`**: Comprehensive network testing with multiple fetch scenarios

## Platform Support

- **WASM/Browser Only**: HTTP fetch is only available in WASM builds running in web browsers
- **Arduino/Embedded**: Returns immediate error response (HTTP 501 "Not Implemented")

## Limitations

- **GET requests only**: Currently supports only HTTP GET method
- **Text responses**: Optimized for text/JSON responses (binary support possible)
- **WASM builds only**: No HTTP functionality on Arduino or other embedded platforms
- **CORS dependent**: Cross-origin requests require proper CORS headers

## Error Types

- **Network Errors**: Connection failures, timeouts, DNS resolution failures
- **HTTP Errors**: 4xx/5xx status codes with status text
- **JavaScript Errors**: Malformed URLs, browser security restrictions

All errors are delivered to the same callback with "Fetch Error:" prefix for easy identification.

## Implementation Notes

- **Memory Management**: Callbacks are automatically cleaned up after execution
- **Thread Safety**: Uses single callback storage (one request at a time)
- **Performance**: Zero-copy string handling where possible
- **Debugging**: Comprehensive logging via `FL_WARN` for troubleshooting 
