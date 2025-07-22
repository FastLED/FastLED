# FastLED Simple HTTP Client Design for Emscripten

## Overview

This document outlines the design for a simple HTTP client implementation for FastLED's WASM platform using Emscripten's native fetch API. The design prioritizes simplicity and direct mapping to JavaScript `fetch()` capabilities, avoiding complex features that can't be reliably supported in the browser environment.

## Problem Statement

The current FastLED WASM platform attempts to implement full socket functionality using WebSocket proxies, which introduces:
- Complex WebSocket proxy server requirements
- Thread coordination issues
- Unreliable connection management
- Over-engineered architecture for simple HTTP needs

## Solution: Emscripten Fetch-Based HTTP Client

Replace the complex socket implementation with a simple HTTP client that directly uses Emscripten's fetch API, providing a clean interface that mirrors JavaScript `fetch()` functionality.

## Architecture

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   C++ FastLED   │    │  Emscripten      │    │   Browser       │
│   HTTP Client   │ ◄─► │  Fetch API       │ ◄─► │   fetch()       │
│                 │    │  (native)        │    │   (native)      │
└─────────────────┘    └──────────────────┘    └─────────────────┘
```

### Benefits
- ✅ **No proxy required** - uses browser's native networking
- ✅ **Simple architecture** - direct fetch() calls
- ✅ **Better error handling** - HTTP status codes vs socket errors
- ✅ **Built-in async support** - works with Asyncify
- ✅ **CORS-compliant** - follows browser security model
- ✅ **Automatic features** - compression, redirects, connection pooling

## API Design

### 1. Core HTTP Client Class

```cpp
namespace fl {

class WasmHttpClient {
public:
    /// Request options (mirrors fetch() RequestInit)
    struct RequestOptions {
        fl::optional<HttpMethod> method;                    // GET, POST, etc.
        fl::map<fl::string, fl::string> headers;           // Request headers
        fl::optional<fl::string> body;                     // Request body (text)
        fl::optional<fl::vector<fl::u8>> body_binary;      // Request body (binary)
        
        // Fetch-equivalent options
        fl::optional<fl::string> mode;          // "cors", "no-cors", "same-origin"
        fl::optional<fl::string> credentials;   // "omit", "same-origin", "include" 
        fl::optional<fl::string> cache;         // "default", "no-store", "reload"
        fl::optional<fl::string> redirect;      // "follow", "error", "manual"
        fl::optional<fl::string> referrer;      // Referrer URL
        fl::optional<bool> keepalive;           // Keep connection alive
        
        // Additional practical options
        fl::optional<fl::u32> timeout_ms;       // Request timeout
        fl::optional<fl::size> max_response_size; // Max response size
    };

    // Basic HTTP methods
    fl::future<Response> get(const fl::string& url, const RequestOptions& options = {});
    fl::future<Response> post(const fl::string& url, const RequestOptions& options = {});
    fl::future<Response> put(const fl::string& url, const RequestOptions& options = {});
    fl::future<Response> delete_(const fl::string& url, const RequestOptions& options = {});
    fl::future<Response> head(const fl::string& url, const RequestOptions& options = {});
    
    // Generic fetch (like JavaScript fetch)
    fl::future<Response> fetch(const fl::string& url, const RequestOptions& options = {});

    // Convenience methods
    fl::future<Response> post_json(const fl::string& url, const fl::string& json);
    fl::future<Response> post_form(const fl::string& url, const fl::map<fl::string, fl::string>& form_data);
    fl::future<Response> post_text(const fl::string& url, const fl::string& text, const fl::string& content_type = "text/plain");
};

} // namespace fl
```

### 2. Request Builder (Fluent API)

```cpp
class RequestBuilder {
public:
    explicit RequestBuilder(const fl::string& url);
    
    // HTTP method
    RequestBuilder& method(HttpMethod method);
    RequestBuilder& get();
    RequestBuilder& post();
    RequestBuilder& put();
    RequestBuilder& delete_();
    
    // Headers
    RequestBuilder& header(const fl::string& name, const fl::string& value);
    RequestBuilder& headers(const fl::map<fl::string, fl::string>& headers);
    
    // Body content
    RequestBuilder& body(const fl::string& body);
    RequestBuilder& body(fl::span<const fl::u8> body);
    RequestBuilder& json(const fl::string& json);
    RequestBuilder& form(const fl::map<fl::string, fl::string>& form_data);
    
    // Fetch options
    RequestBuilder& mode(const fl::string& mode);
    RequestBuilder& credentials(const fl::string& credentials);
    RequestBuilder& cache(const fl::string& cache);
    RequestBuilder& redirect(const fl::string& redirect);
    RequestBuilder& referrer(const fl::string& referrer);
    RequestBuilder& keepalive(bool keepalive);
    
    // Additional options
    RequestBuilder& timeout(fl::u32 timeout_ms);
    RequestBuilder& max_response_size(fl::size max_size);
    
    // Execute
    fl::future<Response> send();
};
```

### 3. Simple Global Functions

```cpp
// Simple functions for basic use cases
fl::future<Response> fetch_get(const fl::string& url);
fl::future<Response> fetch_post_json(const fl::string& url, const fl::string& json);
fl::future<Response> fetch_post_form(const fl::string& url, const fl::map<fl::string, fl::string>& form_data);
fl::future<Response> fetch(const fl::string& url, const WasmHttpClient::RequestOptions& options = {});
```

## Implementation Strategy

### 1. Emscripten Fetch Integration

The implementation uses Emscripten's `emscripten_fetch` API:

```cpp
fl::future<Response> WasmHttpClient::send_request(const fl::string& url, const RequestOptions& options) {
    // Configure emscripten fetch attributes
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    
    // Set HTTP method
    HttpMethod method = options.method.value_or(HttpMethod::GET);
    strcpy(attr.requestMethod, to_string(method).c_str());
    
    // Configure attributes
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    
    // Set timeout
    if (options.timeout_ms) {
        attr.timeoutMSecs = *options.timeout_ms;
    }
    
    // Build headers array
    fl::vector<const char*> header_array;
    for (const auto& header : options.headers) {
        header_array.push_back(header.first.c_str());
        header_array.push_back(header.second.c_str());
    }
    header_array.push_back(nullptr);
    attr.requestHeaders = header_array.data();
    
    // Set body data
    if (options.body) {
        attr.requestData = options.body->c_str();
        attr.requestDataSize = options.body->size();
    }
    
    // Create promise/future for async handling
    auto promise = fl::make_promise<Response>();
    auto future = promise.get_future();
    
    // Set callbacks and start request
    attr.userData = new fl::promise<Response>(fl::move(promise));
    attr.onsuccess = on_fetch_success;
    attr.onerror = on_fetch_error;
    
    emscripten_fetch(&attr, url.c_str());
    return future;
}
```

### 2. Response Handling

```cpp
static void on_fetch_success(emscripten_fetch_t* fetch) {
    auto promise_ptr = static_cast<fl::promise<Response>*>(fetch->userData);
    
    try {
        Response response(static_cast<HttpStatusCode>(fetch->status));
        
        // Set body
        if (fetch->data && fetch->numBytes > 0) {
            fl::vector<fl::u8> body_data(fetch->data, fetch->data + fetch->numBytes);
            response.set_body(body_data);
        }
        
        promise_ptr->set_value(fl::move(response));
    } catch (...) {
        promise_ptr->set_exception(std::current_exception());
    }
    
    delete promise_ptr;
    emscripten_fetch_close(fetch);
}

static void on_fetch_error(emscripten_fetch_t* fetch) {
    auto promise_ptr = static_cast<fl::promise<Response>*>(fetch->userData);
    
    fl::string error = "HTTP request failed";
    if (fetch->status != 0) {
        error += " with status " + fl::to_string(fetch->status);
    }
    
    promise_ptr->set_exception(std::make_exception_ptr(std::runtime_error(error.c_str())));
    
    delete promise_ptr;
    emscripten_fetch_close(fetch);
}
```

## Usage Examples

### 1. Simple GET Request

```cpp
void simple_get_example() {
    auto response_future = fl::fetch_get("https://api.github.com/repos/FastLED/FastLED");
    
    // Check in loop() for completion
    if (response_future.is_ready()) {
        auto result = response_future.try_get_result();
        if (result.is<fl::Response>()) {
            auto response = *result.ptr<fl::Response>();
            if (response.is_success()) {
                FL_WARN("GitHub API response: " << response.get_body_text());
            }
        }
    }
}
```

### 2. JSON API Communication

```cpp
class WeatherService {
private:
    fl::WasmHttpClient mClient;
    
public:
    fl::future<fl::string> get_weather(const fl::string& city) {
        fl::string url = "https://api.openweathermap.org/data/2.5/weather";
        
        return fl::RequestBuilder(url)
            .get()
            .header("Accept", "application/json")
            .header("Authorization", "Bearer " + get_api_key())
            .timeout(10000)
            .send()
            .then([](fl::Response response) -> fl::string {
                if (response.is_success()) {
                    return response.get_body_text();
                }
                return "";
            });
    }
    
    fl::future<bool> upload_sensor_data(float temperature, float humidity) {
        fl::string json_data = fl::string("{") +
            "\"temperature\":" + fl::to_string(temperature) + "," +
            "\"humidity\":" + fl::to_string(humidity) + "," +
            "\"timestamp\":" + fl::to_string(fl::time()) +
            "}";
            
        return mClient.post_json("https://api.iot.com/sensors", json_data)
            .then([](fl::Response response) -> bool {
                return response.is_success();
            });
    }
};
```

### 3. Form Data Submission

```cpp
void submit_device_status() {
    fl::map<fl::string, fl::string> form_data = {
        {"device_id", "fastled_controller_001"},
        {"status", "online"},
        {"led_count", "100"},
        {"firmware_version", "2.1.0"}
    };
    
    auto response_future = fl::fetch_post_form("https://api.fastled.io/devices/status", form_data);
}
```

### 4. Request Builder for Complex Scenarios

```cpp
void advanced_api_call() {
    auto response_future = fl::RequestBuilder("https://api.example.com/data")
        .post()
        .header("Content-Type", "application/json")
        .header("Authorization", "Bearer " + get_auth_token())
        .header("X-Device-ID", get_device_id())
        .json("{\"sensor_type\": \"temperature\", \"value\": 23.5}")
        .timeout(15000)
        .mode("cors")
        .credentials("include")
        .send();
}
```

## Build Configuration

### Updated build_flags.toml

```toml
[linking.sketch]
flags = [
    # Remove complex WebSocket proxy configuration:
    # "-lwebsocket.js",                        
    # "-sWEBSOCKET_SUBPROTOCOL=binary",       
    # "-sWEBSOCKET_URL='ws://localhost:8089/wss_socket_proxy'",
    
    # Enable simple fetch support:
    "-sFETCH=1",                             # Enable Emscripten fetch API
    
    # Keep existing core flags:
    "-sMODULARIZE=1",                        # Create factory function
    "-sEXPORT_NAME=fastled",                 # Export name
    "-sALLOW_MEMORY_GROWTH=1",               # Dynamic memory
    "-sINITIAL_MEMORY=134217728",            # 128 MB initial memory
    "-sASYNCIFY=1",                          # Enable async support
    "-sEXIT_RUNTIME=0",                      # Keep runtime alive
    "-sFILESYSTEM=0",                        # Disable filesystem
    
    # ... other existing flags
]
```

## File Structure

```
src/platforms/wasm/
├── http_client_wasm.h           # Main HTTP client interface
├── http_client_wasm.cpp         # Implementation using emscripten_fetch
└── fetch_transport.cpp         # Integration with existing HTTP transport system
```

## Integration with Existing FastLED HTTP System

The WASM HTTP client integrates with FastLED's existing HTTP transport system:

```cpp
// src/platforms/wasm/fetch_transport.cpp
class WasmFetchTransport : public Transport {
public:
    fl::future<Response> send_request(const Request& request) override {
        WasmHttpClient::RequestOptions options;
        options.method = request.get_method();
        
        // Convert FastLED Request to RequestOptions
        for (const auto& header : request.headers().get_all()) {
            options.headers[header.first] = header.second;
        }
        
        if (request.has_body()) {
            options.body = request.get_body_text();
        }
        
        return mClient.fetch(request.get_url(), options);
    }
    
    bool supports_scheme(const fl::string& scheme) const override {
        return scheme == "http" || scheme == "https";
    }

private:
    WasmHttpClient mClient;
};
```

## What's NOT Included (Removed Complexity)

This design deliberately excludes features that are either:
1. Not supported by browser fetch API
2. Better handled at application layer
3. Overly complex for WASM environment

### Removed Features:
❌ **Custom retry logic** - Application layer concern  
❌ **Upload progress callbacks** - Not reliably available in fetch API  
❌ **Request/response interceptors** - Framework layer concern  
❌ **Batch request handling** - Application layer concern  
❌ **Complex authentication helpers** - Just use headers  
❌ **Download streaming** - Complex and rarely needed  
❌ **Connection pooling management** - Browser handles this  
❌ **Socket-level configuration** - Not applicable to fetch  

## Migration Benefits

### Simplified Architecture:
- **Remove:** 29+ files in complex socket implementation
- **Remove:** WebSocket proxy server requirements
- **Remove:** Thread coordination complexity
- **Add:** 3 clean HTTP client files
- **Add:** Direct browser networking integration

### Better Reliability:
- Native browser fetch API (battle-tested)
- Automatic CORS handling  
- Built-in redirect support
- Automatic compression (gzip/deflate)
- Better error reporting with HTTP status codes
- No proxy server dependencies

### Easier Development:
- No proxy server setup required
- Works with any HTTP endpoint
- Standard HTTP semantics
- Better debugging in browser dev tools
- Matches JavaScript fetch() behavior exactly

## Testing Strategy

### Unit Tests:
```cpp
TEST_CASE("Simple GET request") {
    auto response_future = fl::fetch_get("https://httpbin.org/get");
    // Test async completion and response parsing
}

TEST_CASE("JSON POST request") {
    auto response_future = fl::fetch_post_json("https://httpbin.org/post", "{\"test\": true}");
    // Test JSON content-type and body handling
}

TEST_CASE("Form data submission") {
    fl::map<fl::string, fl::string> form_data = {{"key", "value"}};
    auto response_future = fl::fetch_post_form("https://httpbin.org/post", form_data);
    // Test form encoding and submission
}
```

### Integration Tests:
- Real HTTP endpoints (httpbin.org)
- Error condition handling
- Timeout behavior
- Large response handling

## Conclusion

This design provides a clean, simple HTTP client that directly maps to JavaScript `fetch()` capabilities without artificial complexity. It leverages the browser's native networking stack for reliability while providing a familiar C++ interface that integrates seamlessly with FastLED's existing HTTP architecture.

The design prioritizes:
- **Simplicity** over feature completeness
- **Reliability** over advanced features  
- **Browser compatibility** over custom implementations
- **Ease of use** over configuration flexibility

This approach provides exactly what most FastLED WASM applications need: simple, reliable HTTP communication for APIs, IoT data submission, and web service integration. 
