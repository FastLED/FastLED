# fl/remote/transport - JSON-RPC Transport Layer

Cross-platform transport implementations for `fl::Remote` JSON-RPC communication.

## Overview

The transport layer provides **generic I/O** for serial communication. It is **protocol-agnostic** and can work with any JSON-based protocol, not just JSON-RPC.

Factory functions compose the transport layer with the JSON-RPC protocol layer (from `fl/remote/rpc/protocol.h`) to create `RequestSource` and `ResponseSink` callbacks for `fl::Remote`.

**Design Goals:**
- **Separation of Concerns**: Transport (I/O) vs Protocol (JSON-RPC logic)
- **Testability**: Mock serial objects for unit testing
- **Portability**: Works across all FastLED platforms
- **Flexibility**: Easy to add new transports (HTTP, WebSocket) or protocols

## Architecture

```
┌─────────────────────────────────────────┐
│  fl::Remote (RPC Server)                │
│  - Method registration                  │
│  - JSON-RPC dispatch                    │
│  - Scheduling                           │
└──────────────┬─────────────┬────────────┘
               │             │
        RequestSource   ResponseSink
               │             │
         ┌─────┴─────────────┴─────┐
         │  Factory Functions       │  <- Compose layers
         │  (serial.h)              │
         └─────┬──────────────┬─────┘
               │              │
    ┌──────────┴─────┐ ┌─────┴──────────┐
    │ Protocol Layer │ │ Protocol Layer │
    │ (rpc/protocol) │ │ (rpc/protocol) │
    │ - Normalize    │ │ - Filter       │
    │   requests     │ │   schemas      │
    └────────┬───────┘ └───────┬────────┘
             │                 │
    ┌────────┴───────┐ ┌───────┴────────┐
    │ Transport Layer│ │ Transport Layer│
    │ (serial.h)     │ │ (serial.h)     │
    │ - Read line    │ │ - Write line   │
    │ - Parse JSON   │ │ - Serialize    │
    └────────────────┘ └────────────────┘
```

## Serial Transport

### Basic Usage

```cpp
#include "fl/remote/transport/serial.h"
#include "fl/remote/remote.h"

// Create transport callbacks
auto source = fl::createSerialRequestSource();
auto sink = fl::createSerialResponseSink("REMOTE: ");

// Create Remote with serial transport
fl::Remote remote(source, sink);

// Register RPC methods
remote.bind("ping", []() {
    return "pong";
});

// Main loop
void loop() {
    remote.update(millis());  // Pull + process + push
}
```

### Factory Functions

#### `createSerialRequestSource(prefix = "")`
Creates a `RequestSource` callback that:
1. Reads lines from `fl::available()` / `fl::read()`
2. Strips optional prefix from input
3. Parses JSON-RPC requests
4. Returns `optional<Json>` (nullopt if no data)

```cpp
auto source = fl::createSerialRequestSource();
```

#### `createSerialResponseSink(prefix = "REMOTE: ")`
Creates a `ResponseSink` callback that:
1. Formats JSON-RPC responses
2. Prepends optional prefix
3. Writes to `fl::println()`

```cpp
auto sink = fl::createSerialResponseSink("RESULT: ");
```

#### `createSerialTransport(responsePrefix, requestPrefix)`
Creates both callbacks in one call:

```cpp
auto [source, sink] = fl::createSerialTransport("REMOTE: ");
fl::Remote remote(source, sink);
```

### Custom Serial Adapters

For non-fl:: serial sources (e.g., Arduino Serial on specific platforms):

```cpp
struct ArduinoSerialIn {
    int available() const { return Serial.available(); }
    int read() { return Serial.read(); }
};

struct ArduinoSerialOut {
    void println(const char* str) { Serial.println(str); }
};

// Use template functions directly
auto source = []{
    ArduinoSerialIn serial;
    auto line = fl::readSerialLine(serial);
    if (!line.has_value()) return fl::nullopt;
    return fl::parseJsonRpcRequest(line.value());
};

auto sink = [](const fl::Json& response) {
    ArduinoSerialOut serial;
    auto formatted = fl::formatJsonRpcResponse(response, "REMOTE: ");
    fl::writeSerialLine(serial, formatted);
};

fl::Remote remote(source, sink);
```

## Pure Functions (Unit Testable)

### `formatJsonResponse(response, prefix)`
Generic JSON serialization to single-line string with optional prefix:

```cpp
fl::Json response = fl::Json::object();
response.set("success", true);
auto formatted = fl::formatJsonResponse(response, "REMOTE: ");
// formatted = "REMOTE: {\"success\":true}"
```

**Note:** This is a generic JSON function, not JSON-RPC specific. Works with any JSON object.

## JSON-RPC Protocol Functions

The JSON-RPC specific functions have moved to `fl/remote/rpc/protocol.h`:

### `normalizeJsonRpcRequest(json)` (in rpc/protocol.h)
Transforms old JSON-RPC format to standard 2.0:
- Old: `{"function": "test", "args": [...]}`
- New: `{"method": "test", "params": [...]}`

```cpp
#include "fl/remote/rpc/protocol.h"

fl::Json oldFormat = fl::Json::parse(R"({"function": "ping", "args": []})").value();
fl::Json normalized = fl::normalizeJsonRpcRequest(oldFormat);
// normalized = {"method": "ping", "params": []}
```

### `filterSchemaResponse(response)` (in rpc/protocol.h)
Filters schema responses to prevent stack overflow on constrained platforms:

```cpp
#include "fl/remote/rpc/protocol.h"

// Detects large schema responses (rpc.discover) and replaces with minimal message
fl::Json response = getRpcDiscoverResponse();
fl::Json filtered = fl::filterSchemaResponse(response);
// If response contains "schema", "openrpc", or "methods" keys,
// returns minimal response: {"jsonrpc": "2.0", "id": ..., "result": {"message": "...", "methodCount": N}}
// Otherwise returns original response unchanged
```

**Use case:** ESP32-C6, ESP8266, and other platforms with limited stack can overflow when serializing large JSON-RPC schema responses.

## Template Functions (Generic I/O)

### `readSerialLine<SerialIn>(serial, delimiter)`
Reads line from any serial-like object with `available()` and `read()`:

```cpp
MockSerialIn mock({"hello", "world"});
auto line = fl::readSerialLine(mock);
// line = "hello"
```

### `writeSerialLine<SerialOut>(serial, str)`
Writes line to any serial-like object with `println()`:

```cpp
MockSerialOut mock;
fl::writeSerialLine(mock, "test");
// mock output = "test\n"
```

## Testing

```cpp
#include "fl/remote/transport/serial.h"
#include <gtest/gtest.h>

TEST(Transport, ParseRequest) {
    fl::string input = R"({"method": "status", "params": []})";
    auto request = fl::parseJsonRpcRequest(input);

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->get("method").as_string().value(), "status");
}

TEST(Transport, FormatResponse) {
    fl::Json response = fl::Json::object();
    response.set("result", "ok");

    auto formatted = fl::transport::formatJsonRpcResponse(response, "RPC: ");

    EXPECT_TRUE(formatted.find("RPC: ") == 0);
    EXPECT_TRUE(formatted.find("\"result\":\"ok\"") != fl::string::npos);
}

TEST(Transport, MockSerial) {
    struct MockSerial {
        fl::string buffer = "test\n";
        size_t pos = 0;

        int available() const { return buffer.size() - pos; }
        int read() { return pos < buffer.size() ? buffer[pos++] : -1; }
        void println(const char* s) { output += s; output += '\n'; }

        fl::string output;
    };

    MockSerial mock;
    auto line = fl::readSerialLine(mock);
    EXPECT_EQ(line.value(), "test");

    fl::writeSerialLine(mock, "response");
    EXPECT_EQ(mock.output, "response\n");
}
```

## Adding New Transports

To add a new transport (e.g., HTTP, WebSocket):

1. Create `src/fl/remote/transport/http.h`
2. Implement factory functions:
   ```cpp
   namespace fl {
       fl::function<fl::optional<fl::Json>()> createHttpRequestSource(const char* url);
       fl::function<void(const fl::Json&)> createHttpResponseSink(const char* url);
   }
   ```
3. Reuse parsing functions: `parseJsonRpcRequest`, `formatJsonRpcResponse`
4. Add to `_build.hpp`

## Platform Support

The serial transport uses `fl::available()`, `fl::read()`, and `fl::println()` which work on:
- ✅ AVR (Arduino Uno, Mega, etc.)
- ✅ ESP32 (all variants)
- ✅ ESP8266
- ✅ STM32 (all families)
- ✅ Teensy (3.x, 4.x)
- ✅ SAMD21/SAMD51
- ✅ RP2040
- ✅ Host (native builds)
- ✅ WASM (browser)

## Files

- **serial.h** - Public API (factory functions, templates)
- **serial.cpp.hpp** - Implementation (pure parsing functions)
- **_build.hpp** - Build integration
- **README.md** - This file

## See Also

- `fl/remote/remote.h` - JSON-RPC server
- `fl/remote/rpc/server.h` - RequestSource and ResponseSink types
- `examples/Validation/ValidationRemote.cpp` - Usage example
