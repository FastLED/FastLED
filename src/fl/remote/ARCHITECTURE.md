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
     │ (transport/serial.h)
     └────────┬────────┘
              │
         ┌───▼────┐
         │Transport│  <- Transport Layer
         │(serial) │
         └─────────┘
```

## Layers

### 1. Transport Layer (`fl/remote/transport/`)

**Purpose**: Serial I/O operations with zero-copy optimizations

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

**Key Property**: Generic I/O layer - works with any JSON protocol

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

### Example: Adding HTTP Transport

```cpp
// In fl/remote/transport/http.h
inline fl::function<fl::optional<fl::Json>()>
createHttpRequestSource(const char* url) {
    return [url]() {
        // Transport: Read HTTP request body
        fl::string body = http_read_body(url);

        // Parse JSON directly (no protocol transformation needed)
        return fl::Json::parse(body);
    };
}
```

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
├── remote.h              # High-level RPC server
├── remote.cpp.hpp
├── rpc/
│   ├── rpc.h             # RPC function binding and dispatch
│   ├── rpc.cpp.hpp
│   ├── server.h          # RequestSource/ResponseSink types
│   └── _build.hpp
└── transport/
    ├── serial.h          # Serial I/O + factory functions
    ├── serial.cpp.hpp    # formatJsonResponse implementation
    └── _build.hpp
```

## Protocol Format

The protocol uses a simplified JSON-RPC-inspired format (NOT strict JSON-RPC 2.0):

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

## Migration Notes

See `examples/Validation/MIGRATION_JsonRpcTransport.md` for migration guide from old API.

**Key Changes:**
- Removed protocol normalization layer (no more `normalizeJsonRpcRequest()`)
- Removed schema filtering (no more `filterSchemaResponse()`)
- Renamed `FlSerialIn` → `SerialReader`, `FlSerialOut` → `SerialWriter`
- Added zero-copy string_view optimizations
- Simplified to direct JSON parsing
