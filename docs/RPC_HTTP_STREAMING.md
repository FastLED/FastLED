# HTTP Streaming RPC Migration Guide

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Migration from Serial Transport](#migration-from-serial-transport)
4. [API Reference](#api-reference)
5. [RPC Modes Explained](#rpc-modes-explained)
6. [Advanced Topics](#advanced-topics)
7. [Troubleshooting](#troubleshooting)

---

## Overview

### What is HTTP Streaming RPC?

HTTP Streaming RPC is a bidirectional communication protocol for FastLED that uses:
- **HTTP/1.1** for transport layer
- **Chunked Transfer Encoding** for streaming messages
- **JSON-RPC 2.0** for remote procedure calls
- **Long-lived connections** with automatic reconnection

This enables real-time communication between FastLED devices and web-based or desktop applications over standard HTTP.

### Benefits vs Serial Transport

| Feature | Serial | HTTP Streaming |
|---------|--------|----------------|
| **Network-based** | No | Yes (WiFi/Ethernet) |
| **Multiple clients** | No | Yes |
| **Bidirectional streaming** | Yes | Yes |
| **Standard protocol** | Custom | HTTP/1.1 + JSON-RPC 2.0 |
| **Web browser support** | No | Yes (Fetch API) |
| **Automatic reconnection** | No | Yes |
| **Heartbeat/keepalive** | No | Yes |
| **Firewall-friendly** | N/A | Yes (uses port 80/8080) |

### When to Use Each Mode

**Use Serial Transport when:**
- Direct USB/UART connection
- Single controller ↔ device communication
- No network available
- Low latency critical (< 1ms)

**Use HTTP Streaming when:**
- Network-based communication (WiFi/Ethernet)
- Multiple clients need to connect
- Browser-based control panels
- Integration with web services
- Remote monitoring/control

---

## Quick Start

### Server Setup (FastLED Device)

```cpp
#include <FastLED.h>
#include "fl/remote/remote.h"
#include "fl/remote/rpc/response_send.h"
#include "fl/remote/transport/http/stream_server.h"

// Include implementations
#include "fl/remote/transport/http/stream_server.cpp.hpp"
#include "fl/remote/transport/http/stream_transport.cpp.hpp"
#include "fl/remote/transport/http/connection.cpp.hpp"
#include "fl/remote/transport/http/chunked_encoding.cpp.hpp"
#include "fl/remote/transport/http/http_parser.cpp.hpp"
#include "fl/remote/transport/http/native_server.cpp.hpp"

#define SERVER_PORT 8080

void setup() {
    // Create HTTP server transport
    auto transport = fl::make_shared<fl::HttpStreamServer>(SERVER_PORT);

    // Create Remote instance
    fl::Remote remote(
        [&transport]() { return transport->readRequest(); },
        [&transport](const fl::Json& r) { transport->writeResponse(r); }
    );

    // Register SYNC method
    remote.bind("add", [](int a, int b) { return a + b; });

    // Register ASYNC method
    remote.bindAsync("longTask", [](fl::ResponseSend& response, int duration) {
        // Task runs in background, sends result later
        setTimeout([&response, duration]() {
            response.send(duration * 2);
        }, duration);
    }, fl::RpcMode::ASYNC);

    Serial.println("HTTP RPC Server started on port 8080");
}

void loop() {
    transport->update(millis());  // Handle network I/O
    remote.update(millis());      // Process RPC requests
}
```

### Client Setup (Python/curl/Browser)

**Python Example:**
```python
import requests
import json

url = "http://localhost:8080/rpc"
headers = {
    "Content-Type": "application/json",
    "Transfer-Encoding": "chunked"
}

# SYNC mode: immediate response
response = requests.post(url, headers=headers, json={
    "jsonrpc": "2.0",
    "method": "add",
    "params": [2, 3],
    "id": 1
})
print(response.json())  # {"jsonrpc":"2.0","result":5,"id":1}
```

**curl Example:**
```bash
curl -X POST http://localhost:8080/rpc \
  -H "Content-Type: application/json" \
  -H "Transfer-Encoding: chunked" \
  -d '{"jsonrpc":"2.0","method":"add","params":[2,3],"id":1}'
```

**Browser Fetch API:**
```javascript
fetch('http://localhost:8080/rpc', {
    method: 'POST',
    headers: {
        'Content-Type': 'application/json'
    },
    body: JSON.stringify({
        jsonrpc: '2.0',
        method: 'add',
        params: [2, 3],
        id: 1
    })
})
.then(response => response.json())
.then(data => console.log(data.result));  // 5
```

---

## Migration from Serial Transport

### Side-by-Side Code Comparison

#### Serial Transport (Old)

```cpp
#include "fl/remote/remote.h"
#include "fl/remote/transport/serial.h"

// Create Serial-based Remote
fl::Remote remote = fl::Remote::createSerial(Serial);

// Register method (SYNC only)
remote.bind("add", [](int a, int b) { return a + b; });

// Update loop
void loop() {
    remote.update(millis());
}
```

#### HTTP Streaming Transport (New)

```cpp
#include "fl/remote/remote.h"
#include "fl/remote/transport/http/stream_server.h"
// ... include .cpp.hpp files

#define SERVER_PORT 8080

// Create HTTP server transport
auto transport = fl::make_shared<fl::HttpStreamServer>(SERVER_PORT);

// Create Remote with callbacks
fl::Remote remote(
    [&transport]() { return transport->readRequest(); },
    [&transport](const fl::Json& r) { transport->writeResponse(r); }
);

// Register SYNC method (same API)
remote.bind("add", [](int a, int b) { return a + b; });

// Register ASYNC method (new capability)
remote.bindAsync("longTask", [](fl::ResponseSend& response, int duration) {
    setTimeout([&response, duration]() {
        response.send(duration * 2);
    }, duration);
}, fl::RpcMode::ASYNC);

// Update loop (add transport update)
void loop() {
    transport->update(millis());  // NEW: transport layer update
    remote.update(millis());
}
```

### Transport Layer Changes

| Component | Serial | HTTP Streaming |
|-----------|--------|----------------|
| **Include files** | `transport/serial.h` | `transport/http/stream_server.h` + `.cpp.hpp` files |
| **Constructor** | `Remote::createSerial(Serial)` | Callback-based: `Remote([]{...}, []{...})` |
| **Transport object** | None (Serial built-in) | `HttpStreamServer` or `HttpStreamClient` |
| **Update loop** | `remote.update()` only | `transport->update()` + `remote.update()` |
| **Configuration** | Serial baud rate | Port number + heartbeat interval |

### Callback Updates

#### Before (Serial):
```cpp
// No transport object, Remote reads directly from Serial
fl::Remote remote = fl::Remote::createSerial(Serial);
```

#### After (HTTP):
```cpp
// Transport object manages I/O, Remote uses callbacks
auto transport = fl::make_shared<fl::HttpStreamServer>(8080);
fl::Remote remote(
    [&transport]() { return transport->readRequest(); },
    [&transport](const fl::Json& r) { transport->writeResponse(r); }
);
```

### Migration Checklist

- [ ] **Update includes**: Add HTTP transport headers and `.cpp.hpp` files
- [ ] **Create transport**: Replace `Remote::createSerial()` with `HttpStreamServer`/`HttpStreamClient`
- [ ] **Update constructor**: Use callback-based `Remote()` constructor
- [ ] **Configure port**: Choose port number (8080 recommended)
- [ ] **Add transport update**: Call `transport->update(millis())` in loop
- [ ] **Test SYNC mode**: Verify existing methods work (should be 1:1 compatible)
- [ ] **Migrate ASYNC methods**: Convert to `bindAsync()` with `ResponseSend&`
- [ ] **Test ASYNC modes**: Verify ACK + result pattern works
- [ ] **Add reconnection handling**: Use `setOnConnect()`/`setOnDisconnect()` callbacks
- [ ] **Test heartbeat**: Verify connection stays alive during idle periods
- [ ] **Test error handling**: Verify timeouts and disconnects are handled gracefully

---

## API Reference

### HttpStreamServer Class

Server-side HTTP streaming transport for native platforms (POSIX sockets).

#### Constructor
```cpp
HttpStreamServer(uint16_t port, uint32_t heartbeatIntervalMs = 30000);
```
- `port`: TCP port to listen on (e.g., 8080)
- `heartbeatIntervalMs`: Heartbeat interval in milliseconds (default: 30s)

#### Methods
```cpp
// Connection management
bool connect();           // Start listening (called automatically)
void disconnect();        // Stop server and close all clients
bool isConnected() const; // Returns true if any client connected

// RequestSource/ResponseSink implementation
fl::optional<fl::Json> readRequest();
void writeResponse(const fl::Json& response);

// Update loop (MUST call in loop())
void update(uint32_t currentTimeMs);

// Configuration
void setHeartbeatInterval(uint32_t intervalMs);
void setTimeout(uint32_t timeoutMs);
uint32_t getHeartbeatInterval() const;
uint32_t getTimeout() const;

// Callbacks
void setOnConnect(StateCallback callback);
void setOnDisconnect(StateCallback callback);
```

#### Example
```cpp
auto transport = fl::make_shared<fl::HttpStreamServer>(8080);
transport->setHeartbeatInterval(60000); // 60s heartbeat
transport->setTimeout(120000);          // 120s timeout
transport->setOnConnect([]() { Serial.println("Client connected"); });

void loop() {
    transport->update(millis());
    remote.update(millis());
}
```

---

### HttpStreamClient Class

Client-side HTTP streaming transport for native platforms.

#### Constructor
```cpp
HttpStreamClient(const char* host, uint16_t port, uint32_t heartbeatIntervalMs = 30000);
```
- `host`: Server hostname/IP (e.g., "localhost" or "192.168.1.100")
- `port`: Server port (e.g., 8080)
- `heartbeatIntervalMs`: Heartbeat interval in milliseconds (default: 30s)

#### Methods
```cpp
// Connection management
bool connect();           // Connect to server
void disconnect();        // Close connection
bool isConnected() const; // Returns true if connected

// RequestSource/ResponseSink implementation
fl::optional<fl::Json> readRequest();
void writeResponse(const fl::Json& response);

// Update loop (MUST call in loop())
void update(uint32_t currentTimeMs);

// Configuration
void setHeartbeatInterval(uint32_t intervalMs);
void setTimeout(uint32_t timeoutMs);
uint32_t getHeartbeatInterval() const;
uint32_t getTimeout() const;

// Callbacks
void setOnConnect(StateCallback callback);
void setOnDisconnect(StateCallback callback);
```

#### Example
```cpp
auto transport = fl::make_shared<fl::HttpStreamClient>("localhost", 8080);
transport->setOnConnect([]() { Serial.println("Connected to server"); });
transport->setOnDisconnect([]() { Serial.println("Disconnected from server"); });

void loop() {
    transport->update(millis());
    remote.update(millis());
}
```

---

### HttpStreamTransport Base Class

Abstract base class for HTTP streaming transports. Both `HttpStreamServer` and `HttpStreamClient` inherit from this class.

#### Protected Interface (for subclasses)
```cpp
// Subclasses implement platform-specific I/O
virtual int sendData(const uint8_t* data, size_t length) = 0;
virtual int recvData(uint8_t* buffer, size_t maxLength) = 0;
virtual bool connect() = 0;
virtual void disconnect() = 0;
virtual bool isConnected() const = 0;
```

#### Features
- Automatic chunked encoding/decoding
- Heartbeat protocol (ping/pong)
- Timeout detection
- Connection state callbacks
- JSON-RPC message filtering (removes heartbeat messages)

---

### Configuration Options

#### Heartbeat Interval
Controls how often heartbeat messages are sent to keep connection alive.

```cpp
transport->setHeartbeatInterval(30000);  // 30 seconds (default)
transport->setHeartbeatInterval(60000);  // 60 seconds
```

**Recommendation**: 30-60 seconds for most applications.

#### Timeout
Controls how long to wait before declaring connection dead.

```cpp
transport->setTimeout(60000);   // 60 seconds (default)
transport->setTimeout(120000);  // 120 seconds
```

**Recommendation**: 2x heartbeat interval (e.g., 60s timeout with 30s heartbeat).

#### Reconnection Strategy
Handled automatically by `HttpConnection` with exponential backoff:
- Initial delay: 1 second
- Max delay: 30 seconds
- Backoff: 2x after each failure
- Max attempts: Unlimited (configurable)

```cpp
// Reconnection is automatic, but you can track state:
transport->setOnDisconnect([]() {
    Serial.println("Disconnected, auto-reconnect will trigger");
});
transport->setOnConnect([]() {
    Serial.println("Reconnected successfully");
});
```

---

## RPC Modes Explained

HTTP Streaming RPC supports three modes: SYNC, ASYNC, and ASYNC_STREAM. Each mode has different response patterns.

### SYNC Mode: Immediate Response

**Use case**: Simple request/response with immediate result.

**Flow**:
1. Client sends request
2. Server processes immediately
3. Server sends response
4. Client receives result

**Code Example (Server)**:
```cpp
// Register SYNC method
remote.bind("add", [](int a, int b) {
    return a + b;  // Result sent immediately
});
```

**Code Example (Client)**:
```bash
curl -X POST http://localhost:8080/rpc \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"add","params":[2,3],"id":1}'

# Response (immediate):
# {"jsonrpc":"2.0","result":5,"id":1}
```

**Protocol**:
```
Client → Server: {"jsonrpc":"2.0","method":"add","params":[2,3],"id":1}
Server → Client: {"jsonrpc":"2.0","result":5,"id":1}
```

---

### ASYNC Mode: ACK + Delayed Result

**Use case**: Long-running tasks where immediate response is not possible.

**Flow**:
1. Client sends request
2. Server sends ACK immediately
3. Server processes in background
4. Server sends result when ready
5. Client receives result

**Code Example (Server)**:
```cpp
// Register ASYNC method
remote.bindAsync("longTask", [](fl::ResponseSend& response, int duration) {
    // Send ACK immediately (automatic)

    // Simulate long task
    setTimeout([&response, duration]() {
        int result = duration * 2;
        response.send(result);  // Send result when ready
    }, duration);
}, fl::RpcMode::ASYNC);
```

**Code Example (Client - Python with streaming)**:
```python
import requests

response = requests.post(
    "http://localhost:8080/rpc",
    headers={"Content-Type": "application/json"},
    json={"jsonrpc":"2.0","method":"longTask","params":[1000],"id":2},
    stream=True  # Enable streaming
)

# Read chunks as they arrive
for line in response.iter_lines():
    if line:
        data = json.loads(line)
        if "ack" in data:
            print("ACK received:", data)
        elif "result" in data:
            print("Result received:", data)
```

**Protocol**:
```
Client → Server: {"jsonrpc":"2.0","method":"longTask","params":[1000],"id":2}
Server → Client: {"ack":true}
[... 1000ms delay ...]
Server → Client: {"jsonrpc":"2.0","result":2000,"id":2}
```

---

### ASYNC_STREAM Mode: ACK + Multiple Updates + Final

**Use case**: Progressive tasks with incremental updates (progress bars, data streaming).

**Flow**:
1. Client sends request
2. Server sends ACK immediately
3. Server sends multiple updates
4. Server sends final result with `stop: true`
5. Client receives all updates + final

**Code Example (Server)**:
```cpp
// Register ASYNC_STREAM method
remote.bindAsync("streamData", [](fl::ResponseSend& response, int count) {
    // Send ACK immediately (automatic)

    // Send progressive updates
    for (int i = 0; i < count; i++) {
        setTimeout([&response, i]() {
            response.sendUpdate(i);  // Send update
        }, i * 100);
    }

    // Send final result
    setTimeout([&response, count]() {
        response.sendFinal(count);  // Send final with "stop: true"
    }, count * 100);
}, fl::RpcMode::ASYNC_STREAM);
```

**Code Example (Client - Python with streaming)**:
```python
import requests

response = requests.post(
    "http://localhost:8080/rpc",
    headers={"Content-Type": "application/json"},
    json={"jsonrpc":"2.0","method":"streamData","params":[5],"id":3},
    stream=True
)

for line in response.iter_lines():
    if line:
        data = json.loads(line)
        if "ack" in data:
            print("ACK:", data)
        elif "update" in data:
            print("Update:", data["update"])
        elif "stop" in data and data["stop"]:
            print("Final result:", data["value"])
            break
```

**Protocol**:
```
Client → Server: {"jsonrpc":"2.0","method":"streamData","params":[5],"id":3}
Server → Client: {"ack":true}
Server → Client: {"update":0}
Server → Client: {"update":1}
Server → Client: {"update":2}
Server → Client: {"update":3}
Server → Client: {"update":4}
Server → Client: {"value":5,"stop":true}
```

---

## Advanced Topics

### Connection Management

#### Detecting Connection State

```cpp
auto transport = fl::make_shared<fl::HttpStreamServer>(8080);

transport->setOnConnect([]() {
    Serial.println("Client connected");
    // Optional: send welcome message, reset state, etc.
});

transport->setOnDisconnect([]() {
    Serial.println("Client disconnected");
    // Optional: save state, clean up resources, etc.
});

void loop() {
    transport->update(millis());

    if (transport->isConnected()) {
        // Process normally
        remote.update(millis());
    } else {
        // Show "waiting for connection" on LEDs
        FastLED.showColor(CRGB::Red);
    }
}
```

#### Manual Connection Control

```cpp
// Server: disconnect all clients
transport->disconnect();

// Client: disconnect and reconnect
transport->disconnect();
delay(1000);
transport->connect();
```

---

### Error Handling

#### Connection Errors

```cpp
auto transport = fl::make_shared<fl::HttpStreamClient>("localhost", 8080);

if (!transport->connect()) {
    Serial.println("Failed to connect to server");
    // Retry logic or fallback behavior
}

transport->setOnDisconnect([]() {
    Serial.println("Connection lost, auto-reconnect will trigger");
});
```

#### RPC Errors

HTTP transport preserves JSON-RPC 2.0 error responses:

```cpp
// Server: method not found (automatic)
// Client sends: {"jsonrpc":"2.0","method":"unknown","params":[],"id":1}
// Server responds: {"jsonrpc":"2.0","error":{"code":-32601,"message":"Method not found"},"id":1}

// Server: custom error
remote.bind("divide", [](int a, int b) -> fl::Json {
    if (b == 0) {
        return fl::Json::object({
            {"jsonrpc", "2.0"},
            {"error", fl::Json::object({
                {"code", -32000},
                {"message", "Division by zero"}
            })},
            {"id", nullptr}  // Client provides ID
        });
    }
    return a / b;
});
```

#### Timeout Handling

```cpp
// Configure timeout
transport->setTimeout(60000);  // 60 seconds

// Timeout is detected in transport->update()
// Triggers onDisconnect callback automatically
transport->setOnDisconnect([]() {
    Serial.println("Connection timeout detected");
});
```

---

### Reconnection Strategies

#### Default Behavior
- Automatic reconnection with exponential backoff
- Initial delay: 1 second
- Max delay: 30 seconds
- Unlimited retry attempts

#### Custom Reconnection Logic

```cpp
int reconnectAttempts = 0;
const int MAX_RECONNECT_ATTEMPTS = 5;

transport->setOnDisconnect([&]() {
    reconnectAttempts++;

    if (reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
        Serial.println("Max reconnection attempts reached, giving up");
        // Optional: fallback to different transport or offline mode
    } else {
        Serial.printf("Reconnecting... (attempt %d/%d)\n",
                      reconnectAttempts, MAX_RECONNECT_ATTEMPTS);
    }
});

transport->setOnConnect([&]() {
    reconnectAttempts = 0;  // Reset on successful connection
    Serial.println("Connected successfully");
});
```

---

### Performance Tuning

#### Reducing Latency

```cpp
// Reduce heartbeat interval (more frequent keepalives)
transport->setHeartbeatInterval(10000);  // 10 seconds

// Reduce timeout (faster failure detection)
transport->setTimeout(20000);  // 20 seconds
```

#### Reducing Bandwidth

```cpp
// Increase heartbeat interval (fewer keepalive messages)
transport->setHeartbeatInterval(120000);  // 2 minutes

// Increase timeout (tolerate longer idle periods)
transport->setTimeout(240000);  // 4 minutes
```

#### Multiple Clients (Server Only)

```cpp
// HttpStreamServer automatically handles multiple clients
auto transport = fl::make_shared<fl::HttpStreamServer>(8080);

// Responses are broadcast to ALL connected clients
// Each client receives all RPC responses

// To send to specific client, use custom routing:
remote.bind("broadcast", [](const fl::Json& message) {
    // All clients receive this response
    return message;
});
```

---

## Troubleshooting

### Connection Failures

**Symptom**: Client cannot connect to server.

**Checklist**:
- [ ] Server is running (`transport->connect()` called in `setup()`)
- [ ] Port number matches (client and server use same port)
- [ ] Firewall allows connections on the port
- [ ] Server is listening on correct network interface
- [ ] Client hostname/IP is correct (use `localhost` for same machine)

**Debug**:
```cpp
// Server
auto transport = fl::make_shared<fl::HttpStreamServer>(8080);
if (!transport->connect()) {
    Serial.println("Failed to start server on port 8080");
    Serial.println("Check: port already in use, firewall, permissions");
}

// Client
auto transport = fl::make_shared<fl::HttpStreamClient>("localhost", 8080);
transport->setOnConnect([]() { Serial.println("Connected!"); });
transport->setOnDisconnect([]() { Serial.println("Disconnected!"); });
```

---

### Timeout Issues

**Symptom**: Connection drops after idle period.

**Cause**: Timeout is shorter than heartbeat interval.

**Fix**: Ensure timeout is at least 2x heartbeat interval.

```cpp
// WRONG: timeout < heartbeat
transport->setHeartbeatInterval(60000);  // 60s
transport->setTimeout(30000);            // 30s (too short!)

// CORRECT: timeout ≥ 2x heartbeat
transport->setHeartbeatInterval(60000);  // 60s
transport->setTimeout(120000);           // 120s (safe)
```

---

### Heartbeat Not Working

**Symptom**: Connection drops even though both sides are active.

**Checklist**:
- [ ] `transport->update(millis())` called in `loop()`
- [ ] Heartbeat interval is reasonable (not too short or too long)
- [ ] Timeout is longer than heartbeat interval
- [ ] Network is stable (no packet loss)

**Debug**:
```cpp
// Add logging to track heartbeat
transport->setOnConnect([]() {
    Serial.printf("Connected, heartbeat every %d ms\n",
                  transport->getHeartbeatInterval());
});

// Monitor heartbeat in update loop
uint32_t lastHeartbeat = 0;
void loop() {
    uint32_t now = millis();
    transport->update(now);

    if (now - lastHeartbeat > transport->getHeartbeatInterval()) {
        Serial.println("Heartbeat sent");
        lastHeartbeat = now;
    }

    remote.update(now);
}
```

---

### Port Binding Errors

**Symptom**: Server fails to start with "address already in use" error.

**Cause**: Port is already bound by another process.

**Fix**:
1. Kill existing process using the port:
   ```bash
   # Linux/macOS
   lsof -ti:8080 | xargs kill -9

   # Windows
   netstat -ano | findstr :8080
   taskkill /PID <PID> /F
   ```

2. Use a different port:
   ```cpp
   auto transport = fl::make_shared<fl::HttpStreamServer>(8081);  // Use 8081 instead
   ```

3. Add error handling:
   ```cpp
   auto transport = fl::make_shared<fl::HttpStreamServer>(8080);
   if (!transport->connect()) {
       Serial.println("Port 8080 in use, trying 8081...");
       transport = fl::make_shared<fl::HttpStreamServer>(8081);
       transport->connect();
   }
   ```

---

### Chunked Encoding Errors

**Symptom**: Malformed chunk size or invalid chunk data.

**Cause**: Client or server not following HTTP/1.1 chunked encoding spec.

**Fix**: Ensure clients use proper chunked encoding format:

```python
# Python: Use requests with stream=True
import requests

response = requests.post(
    "http://localhost:8080/rpc",
    headers={"Content-Type": "application/json"},
    json={"jsonrpc":"2.0","method":"add","params":[2,3],"id":1},
    stream=True  # IMPORTANT: enables chunked encoding
)
```

```bash
# curl: Use --data instead of --data-binary
curl -X POST http://localhost:8080/rpc \
  -H "Content-Type: application/json" \
  -H "Transfer-Encoding: chunked" \
  -d '{"jsonrpc":"2.0","method":"add","params":[2,3],"id":1}'
```

---

### Memory Leaks

**Symptom**: Memory usage grows over time.

**Cause**: Shared pointers not cleaned up, or circular references.

**Fix**: Use weak pointers and ensure proper cleanup:

```cpp
// WRONG: Circular reference
auto transport = fl::make_shared<fl::HttpStreamServer>(8080);
auto remote = fl::make_shared<fl::Remote>(...);
transport->setOnConnect([remote]() {  // Captures shared_ptr, circular ref
    remote->update(0);
});

// CORRECT: Use weak_ptr or raw pointer
auto transport = fl::make_shared<fl::HttpStreamServer>(8080);
fl::Remote remote(...);
transport->setOnConnect([&remote]() {  // Captures by reference, no circular ref
    remote.update(0);
});
```

**Debug**: Use AddressSanitizer (ASAN) to detect leaks:

```bash
bash test --docker --cpp loopback  # Runs with ASAN enabled
```

---

## See Also

- [Protocol Specification](../src/fl/remote/transport/http/PROTOCOL.md) - Detailed HTTP streaming RPC protocol
- [Architecture Document](../src/fl/remote/ARCHITECTURE.md) - fl::Remote architecture overview
- [HttpRpcServer Example](../examples/HttpRpcServer/HttpRpcServer.ino) - Complete server example
- [HttpRpcClient Example](../examples/HttpRpcClient/HttpRpcClient.ino) - Complete client example
- [HttpRpcBidirectional Example](../examples/HttpRpcBidirectional/HttpRpcBidirectional.ino) - Bidirectional example

---

## Feedback and Contributions

Found an issue or have a question? Please file an issue at:
- https://github.com/FastLED/FastLED/issues

Contributions welcome! See CONTRIBUTING.md for guidelines.
