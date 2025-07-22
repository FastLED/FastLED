# FastLED WASM JavaScript Fetch API

## Overview

This module provides a simple, fluent HTTP fetch API for FastLED WASM applications using **native JavaScript fetch()** with async callbacks to C++. The implementation allows JavaScript to handle HTTP requests and call back into async C++ functions when responses arrive.

## Features

- **Fluent API**: Clean, chainable interface (`fetch.get(url).response(callback)`)
- **JavaScript Native**: Uses browser's native `fetch()` for maximum compatibility
- **Async Callbacks**: JavaScript can call back into C++ async functions
- **Error Handling**: Comprehensive error reporting for network and HTTP errors
- **CORS Support**: Works with CORS-enabled endpoints
- **Memory Safe**: Automatic cleanup of callbacks and resources

## Usage

### Basic Example

```cpp
#include "FastLED.h"

void setup() {
    // Make an HTTP GET request
    fl::fetch.get("https://httpbin.org/json")
        .response([](const fl::string& content) {
            FL_WARN("Response received: " << content);
        });
}

void loop() {
    // FastLED loop continues normally
    // Fetch responses are handled asynchronously via JavaScript callbacks
}
```

### Error Handling

```cpp
fl::fetch.get("https://invalid-url-that-fails.com/test")
    .response([](const fl::string& content) {
        if (content.startsWith("Fetch Error:")) {
            FL_WARN("Request failed: " << content);
        } else {
            FL_WARN("Request succeeded: " << content);
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
- **`js_fetch_async()`**: EM_JS function that performs JavaScript fetch
- **`js_fetch_success_callback()`**: C++ function called by JavaScript on success
- **`js_fetch_error_callback()`**: C++ function called by JavaScript on error

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

## Limitations

- **GET requests only**: Currently supports only HTTP GET method
- **Text responses**: Optimized for text/JSON responses (binary support possible)
- **Single request**: One request at a time per instance (by design for simplicity)
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
