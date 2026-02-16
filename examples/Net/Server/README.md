# Network.ino - HTTP Server Example

Demonstrates a minimal HTTP server for FastLED using the `fl::HttpServer` API.

## Features

- **Clean API**: Route handlers using `fl::function<>` callbacks
- **Non-blocking**: Integrates seamlessly with Arduino `loop()`
- **Type-safe**: `http_request` and `http_response` classes
- **Visual feedback**: LED colors indicate server state
- **Educational**: Clear example of HTTP server patterns

## Routes

| Method | Path      | Description                          |
|--------|-----------|--------------------------------------|
| GET    | /         | Returns "Hello from FastLED!"        |
| GET    | /status   | Returns LED status as JSON           |
| POST   | /color    | Sets LED color from JSON body        |
| GET    | /ping     | Health check (returns "pong")        |

## LED Status Indicators

| Color          | State            | Meaning                    |
|----------------|------------------|----------------------------|
| Blue (pulse)   | STARTING         | Server initializing        |
| Green (solid)  | RUNNING          | Server ready               |
| Cyan (flash)   | REQUEST_RECEIVED | HTTP request received      |
| Purple (flash) | RESPONDED        | Response sent              |
| Red (solid)    | ERROR            | Server failed to start     |

## Usage

### 1. Compile the Example

```bash
bash compile posix --examples Network
```

### 2. Run the Server

```bash
.build/meson-quick/examples/Network.exe
```

Expected output:
```
HTTP Server Example
Server started on http://localhost:8080/
```

### 3. Test with Python Client

```bash
uv run python examples/Network/test_client.py
```

Output:
```
FastLED Network Example Test Client
Server: http://localhost:8080

Waiting for server at http://localhost:8080/ping...
✓ Server is ready!

Sending 5 requests to http://localhost:8080/...

Request 1: ✓ 200 (2.3 ms)
  Response: 'Hello from FastLED!'
...

┏━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━┓
┃ Metric            ┃ Value    ┃
┡━━━━━━━━━━━━━━━━━━━╇━━━━━━━━━━┩
│ Total Requests    │ 5        │
│ Successful        │ 5 (100%) │
│ Failed            │ 0 (0%)   │
│ Min Response Time │ 1.8 ms   │
│ Max Response Time │ 3.2 ms   │
│ Avg Response Time │ 2.4 ms   │
└───────────────────┴──────────┘

✓ All tests passed!
```

### 4. Test with curl

```bash
# GET /
curl http://localhost:8080/
# Output: Hello from FastLED!

# GET /status
curl http://localhost:8080/status
# Output: {"num_leds":10,"brightness":64,"uptime_ms":12345}

# POST /color (set LEDs to red)
curl -X POST http://localhost:8080/color -d '{"r":255,"g":0,"b":0}'
# Output: Color updated

# GET /ping
curl http://localhost:8080/ping
# Output: pong
```

## Code Example

```cpp
#include "fl/net/http/server.h"
#include <FastLED.h>

fl::HttpServer server;

void setup() {
    // Register GET route
    server.get("/", [](const fl::http_request& req) {
        return fl::http_response::ok("Hello from FastLED!\n");
    });

    // Register POST route with JSON response
    server.post("/api/data", [](const fl::http_request& req) {
        fl::Json response = fl::Json::object();
        response.set("status", "ok");
        response.set("message", "Data received");

        return fl::http_response::ok().json(response);
    });

    // Start server on port 8080
    if (server.start(8080)) {
        Serial.println("Server started!");
    }
}

void loop() {
    // Process pending HTTP requests (non-blocking)
    server.update();

    // Regular FastLED operations
    FastLED.show();
    delay(10);
}
```

## API Reference

### HttpServer

**Methods:**
- `bool start(int port = 8080)` - Start server on specified port
- `void stop()` - Stop server and close connections
- `size_t update()` - Process pending requests (call in `loop()`)
- `void get(path, handler)` - Register GET route
- `void post(path, handler)` - Register POST route
- `void put(path, handler)` - Register PUT route
- `void del(path, handler)` - Register DELETE route
- `bool is_running()` - Check if server is running
- `int port()` - Get server port
- `string last_error()` - Get last error message

### http_request

**Methods:**
- `const string& method()` - HTTP method ("GET", "POST", etc.)
- `const string& path()` - Request path ("/api/status")
- `const string& body()` - Request body (for POST/PUT)
- `optional<string> header(name)` - Get header by name
- `optional<string> query(param)` - Get query parameter
- `bool is_get()` - Check if method is GET
- `bool is_post()` - Check if method is POST

### http_response

**Methods:**
- `http_response& status(int code)` - Set HTTP status code
- `http_response& header(name, value)` - Add HTTP header
- `http_response& body(content)` - Set response body
- `http_response& json(data)` - Set JSON response

**Factory Methods:**
- `http_response::ok(body)` - 200 OK response
- `http_response::not_found()` - 404 Not Found
- `http_response::bad_request(msg)` - 400 Bad Request
- `http_response::internal_error(msg)` - 500 Internal Server Error

## Platform Support

- ✅ **POSIX** (Linux, macOS) - Full support
- ✅ **Windows** - Full support via fcntl emulation
- ❌ **ESP32** - Not yet supported (different networking API)
- ❌ **Arduino** - Not supported (no networking stack)
- ❌ **WASM** - Not supported (browser doesn't allow servers)

## Technical Details

### Non-Blocking I/O

The HTTP server uses non-blocking sockets to integrate with FastLED's event loop:

- **No select()/poll()**: Compatible with WASM constraints
- **MSG_DONTWAIT flag**: Non-blocking recv() calls
- **fcntl(O_NONBLOCK)**: Non-blocking socket mode
- **Sequential polling**: Simple client socket iteration

### HTTP/1.0 Protocol

Minimal HTTP/1.0 implementation for simplicity:

- **Request parsing**: Method, path, headers, body
- **Response generation**: Status line, headers, body
- **Connection: close**: One request per connection
- **Content-Length**: Automatic header generation

### Route Matching

Simple exact-match routing (no regex or wildcards):

```cpp
server.get("/api/status", handler);  // Matches "/api/status" only
```

For advanced routing (wildcards, parameters), consider extending the `HttpServer` class.

## Troubleshooting

### Server won't start

```
ERROR: Failed to start server
Error: Failed to bind to port
```

**Solution**: Port 8080 may be in use. Try a different port:
```cpp
server.start(8081);
```

Or kill the process using port 8080:
```bash
# Linux/macOS
lsof -ti:8080 | xargs kill -9

# Windows
netstat -ano | findstr :8080
taskkill /PID <PID> /F
```

### Connection refused

**Solution**: Ensure the server is running before testing:
```bash
# Terminal 1: Run server
.build/meson-quick/examples/Network.exe

# Terminal 2: Run test client
uv run python examples/Network/test_client.py
```

### Firewall blocking connections

**Solution**: Allow incoming connections on port 8080 in your firewall settings.

## Next Steps

- **Add authentication**: Implement API key validation
- **WebSocket upgrade**: Add WebSocket support for real-time updates
- **File serving**: Serve static HTML files
- **SSL/TLS**: Add HTTPS support
- **Rate limiting**: Prevent abuse with request throttling
- **Logging**: Add request/response logging

## See Also

- **NetTest.ino**: HTTP client example using `fl::fetch`
- **src/fl/net/http/server.h**: HTTP server API documentation
- **docs/agents/cpp-standards.md**: C++ coding standards
