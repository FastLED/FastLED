# Client.ino - HTTP Client Tutorial

Educational tutorial for FastLED's HTTP fetch API (`fl::fetch`) with explicit types for learning.

## Overview

This comprehensive tutorial demonstrates **4 different async approaches** for HTTP networking in FastLED:

1. **Promise-based** - JavaScript-like `.then()` and `.catch_()` callbacks
2. **Await-based** - Synchronous-style `fl::await_top_level()` pattern
3. **JSON Promise** - Promise pattern with automatic JSON parsing
4. **JSON Await** - Await pattern with JSON response handling

The example **cycles through all 4 approaches** every 10 seconds with LED feedback to help you understand different async patterns.

## Educational Focus

**All types are explicitly declared** (no `auto`) to help you understand FastLED's async type system:

- `fl::promise<T>` - Represents a future value
- `fl::result<T>` - Wraps success value or error
- `fl::response` - HTTP response with status/headers/body
- `fl::optional<T>` - May or may not contain a value
- `fl::Error` - Error information
- `fl::Json` - JSON parsing with safe access

## LED Status Indicators

| Color  | Approach           | Meaning                    |
|--------|--------------------|----------------------------|
| Green  | Promise-based      | HTTP success (.then)       |
| Blue   | Await-based        | HTTP success (await)       |
| Blue   | JSON Promise       | JSON parsed (promise)      |
| Cyan   | JSON Await         | JSON parsed (await)        |
| Red    | Any                | Network or HTTP error      |
| Yellow | JSON Promise       | Non-JSON response          |
| Orange | JSON Await         | Non-JSON response          |

## Usage

### 1. Compile the Tutorial

```bash
# For WASM (recommended - full networking)
bash compile wasm --examples Client

# For POSIX (host-based testing)
bash compile posix --examples Client
```

### 2. Run the Tutorial

**WASM (browser):**
```bash
bash run wasm Client
```

**POSIX (console):**
```bash
.build/meson-quick/examples/Client.exe
```

### 3. With Local Test Server (Optional)

For testing without internet connection:

```bash
# Terminal 1: Start local test server
uv run python examples/Net/Client/test_server.py

# Terminal 2: Modify tutorial to use localhost:8081
# (Edit ClientReal.h to change URLs from fastled.io/httpbin.org to localhost:8081)
```

## Async Approaches Explained

### APPROACH 1: Promise-based (.then/.catch)

JavaScript-like async pattern with method chaining:

```cpp
fl::promise<fl::response> promise = fl::fetch_get("http://fastled.io");
promise.then([](const fl::response& response) {
    if (response.ok()) {
        FL_WARN("Success: " << response.text());
    }
}).catch_([](const fl::Error& error) {
    FL_WARN("Error: " << error.message);
});
```

**When to use:**
- ✅ Non-blocking UI/LED updates during request
- ✅ Multiple parallel requests
- ✅ Event-driven architectures

### APPROACH 2: Await-based (fl::await_top_level)

Synchronous-style code that blocks until completion:

```cpp
fl::promise<fl::response> promise = fl::fetch_get("http://fastled.io");
fl::result<fl::response> result = fl::await_top_level(promise);

if (result.ok()) {
    const fl::response& response = result.value();
    FL_WARN("Success: " << response.text());
} else {
    FL_WARN("Error: " << result.error_message());
}
```

**When to use:**
- ✅ Simple request/response patterns
- ✅ Traditional imperative code style
- ✅ Arduino `loop()` context (safe to block)

**⚠️ CRITICAL:** `await_top_level()` blocks execution - ONLY use in `loop()`, NEVER in callbacks!

### APPROACH 3: JSON Promise

Promise pattern with automatic JSON parsing:

```cpp
fl::fetch_get("https://httpbin.org/json").then([](const fl::response& response) {
    if (response.is_json()) {
        fl::Json data = response.json();
        fl::string author = data["slideshow"]["author"] | fl::string("unknown");
        FL_WARN("Author: " << author);
    }
}).catch_([](const fl::Error& error) {
    FL_WARN("Error: " << error.message);
});
```

**Features:**
- Automatic Content-Type detection
- Safe access with default values (`|` operator)
- Nested object/array access
- No manual parsing needed

### APPROACH 4: JSON Await

Await pattern with JSON responses:

```cpp
fl::promise<fl::response> promise = fl::fetch_get("https://httpbin.org/get");
fl::result<fl::response> result = fl::await_top_level(promise);

if (result.ok() && result.value().is_json()) {
    fl::Json data = result.value().json();
    fl::string origin = data["origin"] | fl::string("unknown");
    FL_WARN("Origin: " << origin);
}
```

**Combines:**
- Synchronous-style code (await)
- Automatic JSON parsing
- Safe nested access

## API Reference

### fl::fetch_get(url, options)

Make HTTP GET request:

```cpp
// Simple GET
fl::promise<fl::response> promise = fl::fetch_get("http://example.com");

// With options (timeout, headers, etc.)
fl::fetch_options opts("");
opts.timeout(5000).header("User-Agent", "FastLED");
fl::promise<fl::response> promise = fl::fetch_get("http://example.com", opts);
```

### fl::response

HTTP response object:

**Methods:**
- `int status()` - HTTP status code (200, 404, etc.)
- `const string& status_text()` - Status text ("OK", "Not Found")
- `bool ok()` - True if status 200-299
- `const string& text()` - Response body as string
- `fl::Json json()` - Parse response as JSON (cached)
- `bool is_json()` - Check if response is JSON
- `optional<string> get_content_type()` - Get Content-Type header

### fl::promise<T>

Represents future value:

**Methods:**
- `promise<T>& then(callback)` - Success handler
- `promise<T>& catch_(callback)` - Error handler

### fl::result<T>

Wraps success or error:

**Methods:**
- `bool ok()` - Check if successful
- `const T& value()` - Get success value (throws if error)
- `const string& error_message()` - Get error message

### fl::Json

JSON object with safe access:

**Methods:**
- `Json operator[](key)` - Access nested value
- `T operator|(default)` - Get value or default
- `bool contains(key)` - Check if key exists
- `bool is_array()` - Check if JSON array
- `size_t size()` - Array/object size

## Tutorial Behavior

The example **automatically cycles** through all 4 approaches:

```
0-10s:  Approach 1 (Promise)        → Green LEDs on success
10-20s: Approach 2 (Await)          → Blue LEDs on success
20-30s: Approach 3 (JSON Promise)   → Blue LEDs on success
30-40s: Approach 4 (JSON Await)     → Cyan LEDs on success
40s+:   Repeats from Approach 1
```

Watch the serial output and LED colors to understand each approach!

## Platform Support

- ✅ **WASM** - Full networking via browser fetch API
- ✅ **POSIX** (Linux, macOS) - Native sockets
- ✅ **Windows** - Native sockets with fcntl emulation
- ⚠️ **ESP32** - Limited support (different networking API)
- ❌ **Arduino** - No networking (mock responses only)

## Common Patterns

### Error Handling

**Promise-based:**
```cpp
fetch_get(url).then([](const fl::response& r) {
    if (!r.ok()) {
        FL_WARN("HTTP error: " << r.status());
    }
}).catch_([](const fl::Error& e) {
    FL_WARN("Network error: " << e.message);
});
```

**Await-based:**
```cpp
fl::result<fl::response> result = fl::await_top_level(fetch_get(url));
if (!result.ok()) {
    FL_WARN("Request failed: " << result.error_message());
}
```

### Custom Headers

```cpp
fl::fetch_options opts("");
opts.header("Authorization", "Bearer token123")
    .header("X-Custom", "value")
    .timeout(10000);  // 10 seconds

fl::fetch_get("http://api.example.com/data", opts);
```

### Multiple Parallel Requests

```cpp
// Start multiple requests (non-blocking)
fl::fetch_get("http://api1.com/data").then(handle_api1);
fl::fetch_get("http://api2.com/data").then(handle_api2);
fl::fetch_get("http://api3.com/data").then(handle_api3);

// All requests run in parallel!
```

## Troubleshooting

### WASM: "Network request failed"

**Cause:** CORS policy blocking request

**Solution:** Use CORS-enabled endpoints or run local test server:
```bash
uv run python examples/Net/Client/test_server.py
```

### POSIX/Windows: "Connection refused"

**Cause:** Firewall or no internet connection

**Solutions:**
1. Check firewall settings
2. Use local test server (localhost bypasses firewall)
3. Try different URL

### "await_top_level blocked forever"

**Cause:** Used `await_top_level()` in callback context

**Solution:** Only use `await_top_level()` in `loop()`, use `.then()` in callbacks

## Next Steps

- **Validation Testing**: See `ClientValidation.ino` for automated test suite
- **Server Tutorial**: See `Server.ino` for HTTP server example
- **Advanced**: Combine fetch with LED effects (e.g., weather-reactive LEDs)
- **Production**: Add retry logic, caching, rate limiting

## See Also

- **ClientValidation.ino** - Automated test suite for fetch API
- **Server.ino** - HTTP server tutorial
- **src/fl/fetch.h** - Fetch API source code
- **docs/agents/cpp-standards.md** - C++ coding standards
