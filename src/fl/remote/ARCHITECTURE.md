# fl/remote Architecture

## Overview

The `fl/remote` module provides an ad-hoc JSON-RPC-inspired server for FastLED with a clean separation between **transport** (I/O) and **application logic** (RPC dispatch).

**Note:** This is NOT strict JSON-RPC 2.0, but a simplified ad-hoc protocol inspired by JSON-RPC.

## Layer Architecture

```
┌───────────────────────────────────┐
│  fl::Remote (RPC Server)          │  <- Application Layer
│  - Method registration            │
│  - Request dispatch               │
│  - Scheduling                     │
└─────────────┬─────────────────────┘
              │
     ┌────────┴────────┐
     │ Factory Functions│  <- Composition Layer
     │ (or Callbacks)   │
     └────────┬────────┘
              │
         ┌───▼────────────────────────┐
         │ Transport Layer            │
         │ ┌─────────┐  ┌──────────┐ │
         │ │ Serial  │  │   HTTP   │ │
         │ └─────────┘  └──────────┘ │
         └────────────────────────────┘
```

## Layers

### 1. Transport Layer (`fl/remote/transport/`)

**Purpose**: I/O operations for different transport mechanisms (Serial, HTTP, etc.)

#### Serial Transport (`transport/serial/`)

**Files:**
- `serial.h` - Public API for serial I/O
- `serial.cpp.hpp` - Implementation of serialization functions

**Functions:**
- `readSerialLine<T>(serial)` - Read line from any serial-like object
- `writeSerialLine<T>(serial, str)` - Write line to any serial-like object
- `formatJsonResponse(json, prefix)` - Serialize JSON to compact string with prefix

**Optimizations:**
- **Zero-copy prefix stripping**: Uses `fl::string_view` to avoid allocations
- **Zero-copy trimming**: Pointer arithmetic instead of string copies
- **Compact JSON**: Strips newlines for serial transmission
- **Single allocation**: Only copies once for final JSON parsing

#### HTTP Streaming Transport (`transport/http/`)

**Purpose**: HTTP/1.1 streaming with chunked transfer encoding for bidirectional JSON-RPC

**Files:**
- `stream_transport.h/.cpp.hpp` - Base class for HTTP streaming
- `stream_server.h/.cpp.hpp` - Server-side implementation
- `stream_client.h/.cpp.hpp` - Client-side implementation
- `chunked_encoding.h/.cpp.hpp` - HTTP chunked encoding parser
- `http_parser.h/.cpp.hpp` - HTTP request/response parser
- `connection.h/.cpp.hpp` - Connection lifecycle management
- `native_server.h/.cpp.hpp` - Native platform server (POSIX sockets)
- `native_client.h/.cpp.hpp` - Native platform client (POSIX sockets)
- `PROTOCOL.md` - Full protocol specification

**Classes:**

**`HttpStreamTransport` (abstract base)**:
- Implements `RequestSource` and `ResponseSink` interfaces
- Manages HTTP connection lifecycle (abstract connect/disconnect)
- Handles chunked encoding/decoding (ChunkedReader/ChunkedWriter)
- Implements heartbeat/keepalive (configurable interval, default 30s)
- Provides timeout detection (configurable, default 60s)
- Connection state callbacks (onConnect/onDisconnect)

**`HttpStreamServer`**:
- Extends `HttpStreamTransport`
- Accepts incoming HTTP POST requests to `/rpc` endpoint
- Sends chunked HTTP responses with proper headers
- Handles multiple concurrent clients (broadcast to all clients)
- Uses `NativeHttpServer` for socket I/O

**`HttpStreamClient`**:
- Extends `HttpStreamTransport`
- Sends HTTP POST requests to `/rpc` endpoint
- Receives HTTP responses with chunked encoding
- Supports automatic reconnection through base class
- Uses `NativeHttpClient` for socket I/O

**RPC Modes Supported**:
- **SYNC**: Regular HTTP request/response (immediate result)
- **ASYNC**: ACK immediately over stream, result later
- **ASYNC_STREAM**: ACK + multiple streaming updates until `sendFinal()`

**Protocol Features**:
- **HTTP/1.1**: Standard HTTP protocol
- **Chunked Transfer Encoding**: Streaming message framing
- **JSON-RPC 2.0**: Full specification compliance
- **Long-lived connections**: Persistent connections with keep-alive
- **Heartbeat protocol**: Automatic ping/pong every N seconds (configurable)
- **Auto-reconnection**: Exponential backoff (1s → 2s → 4s → max 30s)
- **Timeout detection**: Disconnects if no data received within timeout

**Key Property**: Network-based transport with automatic reconnection and heartbeat

### 2. Composition Layer (Factory Functions in `transport/serial.h`)

**Purpose**: Combine transport I/O with JSON parsing for fl::Remote

**Functions:**
- `createSerialRequestSource(prefix)` - Composes:
  1. Read line from serial (blocking)
  2. Strip prefix using `string_view` (zero-copy)
  3. Trim whitespace using `string_view` (zero-copy)
  4. Parse JSON (single allocation)
  5. Return `fl::optional<fl::Json>`

- `createSerialResponseSink(prefix)` - Composes:
  1. Serialize JSON to compact string
  2. Add prefix to response
  3. Write line to serial

**Key Property**: Factory functions return callbacks for fl::Remote constructor

### 3. Application Layer (`fl/remote/remote.h`)

**Purpose**: High-level RPC server

**Functionality:**
- Method registration (`bind()`)
- Request routing and dispatch
- Response generation
- Scheduled execution

## Design Rationale

### Why Separate Transport and Application Logic?

1. **Reusability**: Transport layer can be used for different protocols (custom JSON, JSONL, etc.)
2. **Testability**: I/O functions are easily testable with mock objects
3. **Flexibility**: Easy to add new transports (HTTP, WebSocket, UART, etc.)
4. **Performance**: Zero-copy optimizations minimize allocations

### Zero-Copy Optimization

The transport layer uses `fl::string_view` to avoid unnecessary string allocations:

```cpp
// OLD: 3 allocations
fl::string input = line.value();           // Copy 1
input = input.substr(prefixLen);           // Copy 2
input = input.trim();                      // Copy 3

// NEW: 1 allocation
fl::string_view view = line.value();       // Zero-copy reference
view.remove_prefix(prefixLen);             // Pointer arithmetic
// ... trim using remove_prefix/remove_suffix
fl::string input(view);                    // Single copy for JSON parsing
```

### Example: Using HTTP Streaming Transport

```cpp
#include "fl/remote/remote.h"
#include "fl/remote/transport/http/stream_server.h"

// Create HTTP server transport
auto transport = fl::make_shared<fl::HttpStreamServer>(8080);

// Create Remote with callbacks
fl::Remote remote(
    [&transport]() { return transport->readRequest(); },
    [&transport](const fl::Json& r) { transport->writeResponse(r); }
);

// Register SYNC method
remote.bind("add", [](int a, int b) { return a + b; });

// Register ASYNC method
remote.bindAsync("longTask", [](fl::ResponseSend& response, int duration) {
    // ACK sent automatically, result sent later
    setTimeout([&response, duration]() {
        response.send(duration * 2);
    }, duration);
}, fl::RpcMode::ASYNC);

// Update loop
void loop() {
    transport->update(millis());  // Handle HTTP I/O
    remote.update(millis());      // Process RPC requests
}
```

See [`docs/RPC_HTTP_STREAMING.md`](../../docs/RPC_HTTP_STREAMING.md) for complete migration guide.

### Example: Custom JSONL Protocol

```cpp
// Custom JSONL streaming protocol
inline fl::function<fl::optional<fl::Json>()>
createJsonlRequestSource() {
    return []() {
        // Reuse transport layer for I/O
        fl::SerialReader serial;
        auto line = readSerialLine(serial);
        if (!line.has_value()) return fl::nullopt;

        // Parse JSON directly
        return fl::Json::parse(line.value());
    };
}
```

## File Organization

```
fl/remote/
├── remote.h                          # High-level RPC server
├── remote.cpp.hpp
├── rpc/
│   ├── rpc.h                         # RPC function binding and dispatch
│   ├── rpc.cpp.hpp
│   ├── rpc_mode.h                    # RPC modes (SYNC, ASYNC, ASYNC_STREAM)
│   ├── rpc_registry.h                # RPC method registry
│   ├── response_send.h               # Response API for ASYNC methods
│   ├── response_aware_traits.h       # Type traits for ResponseSend detection
│   ├── response_aware_binding.h      # Invoker for response-aware methods
│   ├── server.h                      # RequestSource/ResponseSink types
│   └── _build.hpp
└── transport/
    ├── serial.h                      # Serial I/O + factory functions
    ├── serial.cpp.hpp                # formatJsonResponse implementation
    ├── http/                         # HTTP streaming transport
    │   ├── README.md                 # Architecture overview
    │   ├── PROTOCOL.md               # Protocol specification
    │   ├── stream_transport.h        # Base class for HTTP streaming
    │   ├── stream_transport.cpp.hpp
    │   ├── stream_server.h           # Server-side implementation
    │   ├── stream_server.cpp.hpp
    │   ├── stream_client.h           # Client-side implementation
    │   ├── stream_client.cpp.hpp
    │   ├── chunked_encoding.h        # Chunked transfer encoding
    │   ├── chunked_encoding.cpp.hpp
    │   ├── http_parser.h             # HTTP request/response parser
    │   ├── http_parser.cpp.hpp
    │   ├── connection.h              # Connection lifecycle
    │   ├── connection.cpp.hpp
    │   ├── native_server.h           # Native server (POSIX)
    │   ├── native_server.cpp.hpp
    │   ├── native_client.h           # Native client (POSIX)
    │   ├── native_client.cpp.hpp
    │   └── _build.hpp
    └── _build.hpp
```

## Protocol Formats

### Serial Transport Protocol

The serial transport uses a simplified JSON-RPC-inspired format (NOT strict JSON-RPC 2.0):

**Request:**
```json
{
  "method": "functionName",
  "params": [arg1, arg2, ...],
  "id": 123,
  "timestamp": 1000  // Optional, for scheduled execution
}
```

**Response:**
```json
{
  "jsonrpc": "2.0",
  "result": value,
  "id": 123
}
```

**Error Response:**
```json
{
  "jsonrpc": "2.0",
  "error": {"code": -32601, "message": "Method not found"},
  "id": 123
}
```

### HTTP Streaming Protocol

The HTTP transport uses **full JSON-RPC 2.0 compliance** with three RPC modes:

#### SYNC Mode (Immediate Response)
```
Client → Server: POST /rpc
{"jsonrpc":"2.0","method":"add","params":[2,3],"id":1}

Server → Client:
{"jsonrpc":"2.0","result":5,"id":1}
```

#### ASYNC Mode (ACK + Result)
```
Client → Server: POST /rpc
{"jsonrpc":"2.0","method":"longTask","params":[1000],"id":2}

Server → Client (immediate):
{"ack":true}

Server → Client (later):
{"jsonrpc":"2.0","result":2000,"id":2}
```

#### ASYNC_STREAM Mode (ACK + Updates + Final)
```
Client → Server: POST /rpc
{"jsonrpc":"2.0","method":"streamData","params":[5],"id":3}

Server → Client (immediate):
{"ack":true}

Server → Client (streaming updates):
{"update":0}
{"update":1}
{"update":2}
{"update":3}
{"update":4}

Server → Client (final):
{"value":5,"stop":true}
```

**Protocol Details**: See [`transport/http/PROTOCOL.md`](transport/http/PROTOCOL.md) for complete specification.

**Migration Guide**: See [`docs/RPC_HTTP_STREAMING.md`](../../docs/RPC_HTTP_STREAMING.md) for migration from Serial to HTTP.

## RPC Modes and Response Handling

FastLED's RPC system supports three modes for different use cases:

### SYNC Mode (Default)

**Use case**: Simple request/response with immediate result.

**Registration**:
```cpp
remote.bind("add", [](int a, int b) {
    return a + b;  // Result sent immediately
});
```

**Flow**: Request → Process → Response

### ASYNC Mode

**Use case**: Long-running tasks where immediate response is not possible.

**Registration**:
```cpp
remote.bindAsync("longTask", [](fl::ResponseSend& response, int duration) {
    // ACK sent automatically

    // Process in background
    setTimeout([&response, duration]() {
        response.send(duration * 2);  // Send result when ready
    }, duration);
}, fl::RpcMode::ASYNC);
```

**Flow**: Request → ACK → [processing] → Response

### ASYNC_STREAM Mode

**Use case**: Progressive tasks with incremental updates (progress bars, data streaming).

**Registration**:
```cpp
remote.bindAsync("streamData", [](fl::ResponseSend& response, int count) {
    // ACK sent automatically

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

**Flow**: Request → ACK → [updates] → Final Response

### ResponseSend API

The `ResponseSend` class provides three methods for ASYNC/ASYNC_STREAM modes:

```cpp
class ResponseSend {
public:
    // ASYNC: Send final result
    void send(const fl::Json& result);

    // ASYNC_STREAM: Send update (no "stop" marker)
    void sendUpdate(const fl::Json& update);

    // ASYNC_STREAM: Send final result (with "stop: true")
    void sendFinal(const fl::Json& result);
};
```

**Note**: The ACK is sent automatically by the framework when a response-aware method is invoked. You only need to call `send()`, `sendUpdate()`, or `sendFinal()` for the actual results.

## Migration Notes

### From Old API (fastled3)

See `examples/Validation/MIGRATION_JsonRpcTransport.md` for migration guide from old API.

**Key Changes:**
- Removed protocol normalization layer (no more `normalizeJsonRpcRequest()`)
- Removed schema filtering (no more `filterSchemaResponse()`)
- Renamed `FlSerialIn` → `SerialReader`, `FlSerialOut` → `SerialWriter`
- Added zero-copy string_view optimizations
- Simplified to direct JSON parsing

### From Serial to HTTP Transport

See [`docs/RPC_HTTP_STREAMING.md`](../../docs/RPC_HTTP_STREAMING.md) for complete migration guide.

**Key Changes:**
- Replace `Remote::createSerial()` with callback-based constructor
- Create `HttpStreamServer` or `HttpStreamClient` transport object
- Add `transport->update(millis())` to loop
- Configure heartbeat/timeout settings
- Use connection state callbacks for reconnection handling

## Sequence Diagrams

### SYNC Mode (Immediate Response)

```
┌──────┐                 ┌────────┐
│Client│                 │Server  │
└──┬───┘                 └───┬────┘
   │ {"method":"add",       │
   │  "params":[2,3],"id":1}│
   ├──────────────────────>│
   │                        │ (process)
   │ {"result":5,"id":1}    │
   │<───────────────────────┤
   │                        │
```

### ASYNC Mode (ACK + Result)

```
┌──────┐                 ┌────────┐
│Client│                 │Server  │
└──┬───┘                 └───┬────┘
   │ {"method":"longTask",  │
   │  "params":[1000],"id":2}│
   ├──────────────────────>│
   │ {"ack":true}           │
   │<───────────────────────┤
   │                        │
   │                        │ (background processing)
   │                        │
   │ {"result":2000,"id":2} │
   │<───────────────────────┤
   │                        │
```

### ASYNC_STREAM Mode (ACK + Updates + Final)

```
┌──────┐                 ┌────────┐
│Client│                 │Server  │
└──┬───┘                 └───┬────┘
   │ {"method":"streamData",│
   │  "params":[3],"id":3}  │
   ├──────────────────────>│
   │ {"ack":true}           │
   │<───────────────────────┤
   │                        │
   │ {"update":0}           │
   │<───────────────────────┤
   │ {"update":1}           │
   │<───────────────────────┤
   │ {"update":2}           │
   │<───────────────────────┤
   │ {"value":3,"stop":true}│
   │<───────────────────────┤
   │                        │
```

## Component Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                         fl::Remote                                  │
│  ┌───────────────────────────────────────────────────────────────┐ │
│  │                    RPC Registry                                │ │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────────┐                │ │
│  │  │   SYNC   │  │  ASYNC   │  │ASYNC_STREAM  │                │ │
│  │  │ Methods  │  │ Methods  │  │   Methods    │                │ │
│  │  └──────────┘  └──────────┘  └──────────────┘                │ │
│  └───────────────────────────────────────────────────────────────┘ │
│                             │                                       │
│                             │ (dispatch)                            │
│                             ▼                                       │
│  ┌───────────────────────────────────────────────────────────────┐ │
│  │                  Response Handling                             │ │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────────┐                │ │
│  │  │ Direct   │  │   ACK +  │  │  ACK + Updates│                │ │
│  │  │ Response │  │  Result  │  │    + Final   │                │ │
│  │  └──────────┘  └──────────┘  └──────────────┘                │ │
│  └───────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
                             │
                             │ (RequestSource / ResponseSink)
                             ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      Transport Layer                                │
│                                                                     │
│  ┌──────────────────┐              ┌──────────────────────────┐   │
│  │ Serial Transport │              │   HTTP Streaming         │   │
│  │                  │              │                          │   │
│  │ - readSerialLine │              │ - HttpStreamServer       │   │
│  │ - writeSerialLine│              │ - HttpStreamClient       │   │
│  │ - Zero-copy      │              │ - Chunked encoding       │   │
│  │   optimization   │              │ - Heartbeat/keepalive    │   │
│  │                  │              │ - Auto-reconnection      │   │
│  └──────────────────┘              │ - Multi-client support   │   │
│                                     └──────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```
