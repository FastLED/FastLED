# HTTP Streaming RPC Migration Agent Loop

## Overview

Migrate async RPC system from fastled3 to fastled9 with HTTP streaming transport layer using chunked transfer encoding for bidirectional JSON-RPC communication.

## Key Requirements

- **Transport**: HTTP/1.1 Chunked Transfer Encoding for bidirectional JSON-RPC
- **Connection**: Long-lived, auto-reconnecting with heartbeat/keepalive
- **RPC Modes**:
  - `SYNC`: Regular HTTP request/response
  - `ASYNC`: ACK immediately over stream, result later
  - `ASYNC_STREAM`: ACK + multiple streaming updates until `sendFinal()`
- **Platform**: Native platform (not ESP32) for testing, mock HTTP client/server
- **Spec**: Full JSON-RPC 2.0 compliance
- **Testing**: Unit tests with mocks, examples on native with loopback

---

## Phase 1: Port Missing Async/Stream Features from fastled3

### Task 1.1: Port ResponseSend API
**File**: `src/fl/remote/rpc/response_send.h`

Port `ResponseSend` class from fastled3 to fastled9:
- Copy `src/fl/remote/rpc/response_send.h` from fastled3
- Provides `send()`, `sendUpdate()`, `sendFinal()` methods
- Automatically attaches request ID to responses
- Used by ASYNC/ASYNC_STREAM functions

**Verification**:
```bash
bash test --cpp response_send
```

### Task 1.2: Port Response-Aware Traits
**File**: `src/fl/remote/rpc/response_aware_traits.h`

Port type traits for detecting `ResponseSend&` first parameter:
- Copy `src/fl/remote/rpc/response_aware_traits.h` from fastled3
- Provides `response_aware_signature<Sig>` trait
- Strips `ResponseSend&` from function signature for JSON param matching
- Example: `void(ResponseSend&, int, int)` → JSON params: `[int, int]`

**Verification**:
```bash
bash test --cpp response_aware_traits
```

### Task 1.3: Port Response-Aware Binding/Invoker
**File**: `src/fl/remote/rpc/response_aware_binding.h`

Port `ResponseAwareInvoker` for ASYNC/ASYNC_STREAM methods:
- Copy `src/fl/remote/rpc/response_aware_binding.h` from fastled3
- Creates `ResponseSend` instance with request ID + response sink
- Invokes user function with `ResponseSend&` + JSON params
- Handles ACK sending for ASYNC/ASYNC_STREAM modes

**Verification**:
```bash
bash test --cpp response_aware_binding
```

### Task 1.4: Update Rpc::bindAsync() Method
**File**: `src/fl/remote/rpc/rpc.h`, `src/fl/remote/rpc/rpc.cpp.hpp`

Port `bindAsync()` method from fastled3 Remote to fastled9 Rpc:
- Add `bindAsync()` method signature to `rpc.h`
- Implement in `rpc.cpp.hpp`
- Signature: `void(ResponseSend&, const Json&)`
- Supports `RpcMode::ASYNC` and `RpcMode::ASYNC_STREAM`

**Verification**:
```bash
bash test --cpp rpc_bind_async
```

### Task 1.5: Update Remote::bindAsync() Wrapper
**File**: `src/fl/remote/remote.h`, `src/fl/remote/remote.cpp.hpp`

Port `bindAsync()` wrapper method from fastled3:
- Add `bindAsync()` method to `remote.h` (line ~88 after `bind()`)
- Forward to `mRpc.bindAsync()`
- Keep same signature as Rpc::bindAsync()

**Verification**:
```bash
bash test --cpp remote_bind_async
```

### Task 1.6: Update Remote Stream Support
**Files**: `src/fl/remote/remote.h`, `src/fl/remote/remote.cpp.hpp`

Port streaming methods from fastled3:
- Add `sendAsyncResponse()` method (line ~164)
- Add `sendStreamUpdate()` method (line ~175)
- Update `mRpc.sendStreamUpdate()` forwarding

**Verification**:
```bash
bash test --cpp remote_stream
```

---

## Phase 2: HTTP Streaming Transport Layer

### Task 2.1: Design HTTP Transport Architecture
**File**: `src/fl/remote/transport/http/README.md`

Create architecture document:
- **HttpStreamTransport**: Base class for HTTP streaming
- **HttpChunkedReader**: Reads chunked transfer encoding
- **HttpChunkedWriter**: Writes chunked transfer encoding
- **HttpConnection**: Connection state management (open/closed/reconnecting)
- **HttpStreamClient**: Client-side HTTP streaming
- **HttpStreamServer**: Server-side HTTP streaming (native only)

**Deliverable**: Architecture diagram + API design

### Task 2.2: Implement HTTP Chunked Encoding Parser
**Files**:
- `src/fl/remote/transport/http/chunked_encoding.h`
- `src/fl/remote/transport/http/chunked_encoding.cpp.hpp`

Implement chunked transfer encoding parser:
- **ChunkedReader**: Parse incoming chunks
  - Read chunk size (hex) + CRLF
  - Read chunk data
  - Read trailing CRLF
  - Handle chunk extensions (optional)
  - Detect final chunk (size 0)
- **ChunkedWriter**: Format outgoing chunks
  - Write chunk size (hex) + CRLF
  - Write chunk data
  - Write trailing CRLF
  - Write final chunk when done

**Verification**:
```bash
bash test --cpp chunked_encoding
```

### Task 2.3: Implement HTTP Request/Response Parser
**Files**:
- `src/fl/remote/transport/http/http_parser.h`
- `src/fl/remote/transport/http/http_parser.cpp.hpp`

Minimal HTTP/1.1 parser for RPC:
- **HttpRequestParser**: Parse HTTP requests
  - Method, URI, HTTP version
  - Headers (Content-Type, Transfer-Encoding, Connection)
  - Body (chunked or Content-Length)
- **HttpResponseParser**: Parse HTTP responses
  - Status code, reason phrase
  - Headers
  - Body (chunked or Content-Length)

**Verification**:
```bash
bash test --cpp http_parser
```

### Task 2.4: Implement Connection State Machine
**Files**:
- `src/fl/remote/transport/http/connection.h`
- `src/fl/remote/transport/http/connection.cpp.hpp`

Connection lifecycle management:
- **States**: `DISCONNECTED`, `CONNECTING`, `CONNECTED`, `RECONNECTING`, `CLOSED`
- **Auto-reconnect**: Exponential backoff (1s, 2s, 4s, max 30s)
- **Heartbeat**: Periodic ping/pong (configurable interval, default 30s)
- **Keepalive**: TCP keepalive settings
- **Timeout**: Detect dead connections (configurable, default 60s)

**Verification**:
```bash
bash test --cpp http_connection
```

### Task 2.5: Implement Native HTTP Client
**Files**:
- `src/fl/remote/transport/http/native_client.h`
- `src/fl/remote/transport/http/native_client.cpp.hpp`

Native platform HTTP client (POSIX sockets):
- **TCP Socket**: Connect to server (blocking or non-blocking)
- **Request Sending**: Send HTTP request with chunked encoding
- **Response Reading**: Read HTTP response with chunked decoding
- **Error Handling**: Connection failures, timeouts

**Verification**:
```bash
bash test --cpp http_native_client
```

### Task 2.6: Implement Native HTTP Server
**Files**:
- `src/fl/remote/transport/http/native_server.h`
- `src/fl/remote/transport/http/native_server.cpp.hpp`

Native platform HTTP server (POSIX sockets):
- **TCP Listener**: Accept incoming connections
- **Request Handling**: Parse incoming requests
- **Response Sending**: Send responses with chunked encoding
- **Multi-client**: Handle multiple concurrent connections

**Verification**:
```bash
bash test --cpp http_native_server
```

---

## Phase 3: HTTP Streaming RPC Integration

### Task 3.1: Design HTTP Streaming RPC Protocol
**File**: `src/fl/remote/transport/http/PROTOCOL.md`

Define JSON-RPC over HTTP streaming protocol:

**Request Format** (client → server):
```http
POST /rpc HTTP/1.1
Host: localhost:8080
Content-Type: application/json
Transfer-Encoding: chunked
Connection: keep-alive

<chunk-size>\r\n
{"jsonrpc":"2.0","method":"add","params":[1,2],"id":1}\r\n
<chunk-size>\r\n
...
0\r\n
\r\n
```

**Response Format** (server → client):

**SYNC mode** (immediate response):
```http
HTTP/1.1 200 OK
Content-Type: application/json
Transfer-Encoding: chunked

<chunk-size>\r\n
{"jsonrpc":"2.0","result":3,"id":1}\r\n
0\r\n
\r\n
```

**ASYNC mode** (ACK + later result):
```http
HTTP/1.1 200 OK
Content-Type: application/json
Transfer-Encoding: chunked

<chunk-size>\r\n
{"jsonrpc":"2.0","result":{"ack":true},"id":1}\r\n
<chunk-size>\r\n
{"jsonrpc":"2.0","result":{"value":42},"id":1}\r\n
0\r\n
\r\n
```

**ASYNC_STREAM mode** (ACK + multiple updates + final):
```http
HTTP/1.1 200 OK
Content-Type: application/json
Transfer-Encoding: chunked

<chunk-size>\r\n
{"jsonrpc":"2.0","result":{"ack":true},"id":1}\r\n
<chunk-size>\r\n
{"jsonrpc":"2.0","result":{"update":10},"id":1}\r\n
<chunk-size>\r\n
{"jsonrpc":"2.0","result":{"update":20},"id":1}\r\n
<chunk-size>\r\n
{"jsonrpc":"2.0","result":{"value":100,"stop":true},"id":1}\r\n
0\r\n
\r\n
```

**Heartbeat** (bidirectional):
```http
<chunk-size>\r\n
{"jsonrpc":"2.0","method":"rpc.ping","id":null}\r\n
<chunk-size>\r\n
{"jsonrpc":"2.0","result":"pong","id":null}\r\n
```

**Deliverable**: Protocol specification document

### Task 3.2: Implement HttpStreamTransport Base
**Files**:
- `src/fl/remote/transport/http/stream_transport.h`
- `src/fl/remote/transport/http/stream_transport.cpp.hpp`

Base class for HTTP streaming transport:
- Implements `RequestSource` and `ResponseSink` for `Remote`
- Manages HTTP connection lifecycle
- Handles chunked encoding/decoding
- Implements heartbeat/keepalive
- Provides reconnection logic

**API**:
```cpp
class HttpStreamTransport {
public:
    // Constructor
    HttpStreamTransport(const char* host, uint16_t port);

    // Connection management
    bool connect();
    void disconnect();
    bool isConnected() const;

    // RequestSource implementation (for Remote)
    fl::optional<fl::Json> readRequest();

    // ResponseSink implementation (for Remote)
    void writeResponse(const fl::Json& response);

    // Update loop (handles reconnection, heartbeat)
    void update(uint32_t currentTimeMs);

private:
    HttpConnection mConnection;
    ChunkedReader mReader;
    ChunkedWriter mWriter;
    uint32_t mLastHeartbeat;
    uint32_t mHeartbeatInterval;
};
```

**Verification**:
```bash
bash test --cpp http_stream_transport
```

### Task 3.3: Implement HttpStreamClient
**Files**:
- `src/fl/remote/transport/http/stream_client.h`
- `src/fl/remote/transport/http/stream_client.cpp.hpp`

Client-side HTTP streaming for RPC:
- Extends `HttpStreamTransport`
- Sends RPC requests over chunked HTTP POST
- Receives RPC responses over chunked HTTP response
- Handles SYNC/ASYNC/ASYNC_STREAM modes

**Verification**:
```bash
bash test --cpp http_stream_client
```

### Task 3.4: Implement HttpStreamServer
**Files**:
- `src/fl/remote/transport/http/stream_server.h`
- `src/fl/remote/transport/http/stream_server.cpp.hpp`

Server-side HTTP streaming for RPC:
- Extends `HttpStreamTransport`
- Accepts incoming HTTP POST requests
- Sends chunked HTTP responses
- Handles multiple concurrent clients

**Verification**:
```bash
bash test --cpp http_stream_server
```

### Task 3.5: Integrate HttpStreamTransport with Remote
**Files**:
- `src/fl/remote/remote.h`
- `src/fl/remote/remote.cpp.hpp`

Add HTTP streaming support to Remote:
- Add constructor overload for `HttpStreamTransport`
- Automatically set request source/sink from transport
- Handle transport lifecycle in `update()`

**Example Usage**:
```cpp
// Client side
auto transport = fl::make_shared<HttpStreamClient>("localhost", 8080);
fl::Remote remote(
    [&transport]() { return transport->readRequest(); },
    [&transport](const fl::Json& r) { transport->writeResponse(r); }
);

// Update loop
while (true) {
    transport->update(millis());
    remote.update(millis());
}
```

**Verification**:
```bash
bash test --cpp remote_http_stream
```

---

## Phase 4: Testing Infrastructure

### Task 4.1: Create Mock HTTP Server/Client
**Files**:
- `tests/fl/remote/http/mock_http_server.h`
- `tests/fl/remote/http/mock_http_client.h`

Mock implementations for unit testing:
- **MockHttpServer**: In-memory HTTP server
  - Accept "connections" (function calls)
  - Store requests, serve responses
  - No actual sockets
- **MockHttpClient**: In-memory HTTP client
  - Send "requests" (function calls)
  - Receive responses from queue
  - No actual sockets

**Verification**:
```bash
bash test --cpp mock_http
```

### Task 4.2: Write Unit Tests for HTTP Transport
**File**: `tests/fl/remote/http/http_transport.cpp`

Comprehensive unit tests:
- Chunked encoding/decoding
- HTTP request/response parsing
- Connection state machine
- Reconnection logic
- Heartbeat/keepalive
- Error handling

**Verification**:
```bash
bash test --cpp http_transport
```

### Task 4.3: Write Integration Tests for RPC over HTTP
**File**: `tests/fl/remote/http/rpc_http_stream.cpp`

Integration tests with mock HTTP:
- **SYNC mode**: Immediate response
- **ASYNC mode**: ACK + later result
- **ASYNC_STREAM mode**: ACK + multiple updates + final
- **Heartbeat**: Ping/pong during idle
- **Reconnection**: Auto-reconnect on disconnect
- **Error cases**: Timeout, connection failure

**Verification**:
```bash
bash test --cpp rpc_http_stream
```

### Task 4.4: Write Loopback Tests (Native Platform)
**File**: `tests/fl/remote/http/loopback.cpp`

Real loopback tests on native platform:
- Start HTTP server on `localhost:8080`
- Connect HTTP client to server
- Send RPC requests client → server
- Receive responses server → client
- Test all three RPC modes (SYNC, ASYNC, ASYNC_STREAM)

**Verification**:
```bash
bash test --cpp http_loopback
```

---

## Phase 5: Examples and Documentation

### Task 5.1: Create Example: Simple HTTP RPC Server
**File**: `examples/HttpRpcServer/HttpRpcServer.ino`

Native example (compiles with meson):
- Starts HTTP server on port 8080
- Binds RPC methods (SYNC, ASYNC, ASYNC_STREAM)
- Handles incoming requests
- Demonstrates ResponseSend API

**Example Methods**:
```cpp
// SYNC: Immediate response
remote.bind("add", [](int a, int b) { return a + b; });

// ASYNC: ACK immediately, result later
remote.bindAsync("longTask", [](ResponseSend& send, const Json& params) {
    send.send(Json::object().set("ack", true));  // ACK
    // ... do work ...
    send.send(Json::object().set("value", 42));  // Result
});

// ASYNC_STREAM: Multiple updates
remote.bindAsync("streamData", [](ResponseSend& send, const Json& params) {
    send.send(Json::object().set("ack", true));  // ACK
    for (int i = 0; i < 10; i++) {
        send.sendUpdate(Json::object().set("progress", i * 10));
    }
    send.sendFinal(Json::object().set("done", true));
}, fl::RpcMode::ASYNC_STREAM);
```

**Run**:
```bash
uv run test.py --examples HttpRpcServer --full
```

### Task 5.2: Create Example: HTTP RPC Client
**File**: `examples/HttpRpcClient/HttpRpcClient.ino`

Native example (compiles with meson):
- Connects to HTTP server on `localhost:8080`
- Sends RPC requests (SYNC, ASYNC, ASYNC_STREAM)
- Receives responses
- Demonstrates response handling

**Run**:
```bash
uv run test.py --examples HttpRpcClient --full
```

### Task 5.3: Create Example: Bidirectional HTTP RPC
**File**: `examples/HttpRpcBidirectional/HttpRpcBidirectional.ino`

Native example demonstrating full duplex:
- Server AND client in same process
- Server accepts connections, binds methods
- Client connects to server, calls methods
- Demonstrates all RPC modes over HTTP streaming

**Run**:
```bash
uv run test.py --examples HttpRpcBidirectional --full
```

### Task 5.4: Write Migration Guide
**File**: `docs/RPC_HTTP_STREAMING.md`

Documentation:
- Overview of HTTP streaming RPC
- How to migrate from Serial transport to HTTP
- API reference for `HttpStreamTransport`
- Examples of SYNC/ASYNC/ASYNC_STREAM modes
- Troubleshooting guide (reconnection, heartbeat)

**Deliverable**: Complete migration guide

### Task 5.5: Update ARCHITECTURE.md
**File**: `src/fl/remote/ARCHITECTURE.md`

Update architecture document:
- Add HTTP streaming transport layer
- Update diagrams with HTTP components
- Document protocol specification
- Add sequence diagrams for SYNC/ASYNC/ASYNC_STREAM

**Deliverable**: Updated architecture document

---

## Phase 6: Validation and Cleanup

### Task 6.1: Run All Tests
**Commands**:
```bash
bash test --cpp remote
bash test --cpp rpc
bash test --cpp http
bash test --examples HttpRpc*
```

**Success Criteria**: All tests pass

### Task 6.2: Run Code Review
**Command**:
```bash
/code_review
```

**Success Criteria**: No linting errors, code follows FastLED standards

### Task 6.3: Performance Testing
**File**: `tests/profile/profile_http_rpc.cpp`

Profile HTTP RPC performance:
- Measure request/response latency
- Measure throughput (requests/second)
- Compare SYNC vs ASYNC vs ASYNC_STREAM

**Run**:
```bash
bash profile http_rpc --docker --iterations 100
```

### Task 6.4: Memory Testing
**Command**:
```bash
bash test --docker --cpp http_loopback
```

**Success Criteria**:
- No memory leaks (ASAN/LSAN clean)
- No buffer overflows
- Stable memory usage during long-running tests

### Task 6.5: Create Migration Checklist
**File**: `docs/MIGRATION_CHECKLIST.md`

Checklist for users migrating from Serial to HTTP:
- [ ] Update transport layer (Serial → HTTP)
- [ ] Configure host/port
- [ ] Add reconnection handling
- [ ] Test SYNC/ASYNC/ASYNC_STREAM modes
- [ ] Verify heartbeat/keepalive
- [ ] Test error handling (timeouts, disconnects)

**Deliverable**: User-facing checklist

---

## Success Criteria

### Phase 1 Complete ✓
- [ ] All response_send, response_aware_* files ported
- [ ] bindAsync() method works in Rpc and Remote
- [ ] Stream support (sendAsyncResponse, sendStreamUpdate) works
- [ ] All unit tests pass

### Phase 2 Complete ✓
- [ ] Chunked encoding parser works (read/write)
- [ ] HTTP request/response parser works
- [ ] Connection state machine works (reconnect, heartbeat)
- [ ] Native HTTP client/server work on loopback
- [ ] All unit tests pass

### Phase 3 Complete ✓
- [ ] HttpStreamTransport integrates with Remote
- [ ] SYNC mode works over HTTP
- [ ] ASYNC mode works (ACK + result)
- [ ] ASYNC_STREAM mode works (ACK + updates + final)
- [ ] Heartbeat/keepalive works
- [ ] Reconnection works on disconnect
- [ ] All integration tests pass

### Phase 4 Complete ✓
- [ ] Mock HTTP server/client work
- [ ] Unit tests pass for HTTP transport
- [ ] Integration tests pass for RPC over HTTP
- [ ] Loopback tests pass on native platform

### Phase 5 Complete ✓
- [ ] HttpRpcServer example compiles and runs
- [ ] HttpRpcClient example compiles and runs
- [ ] HttpRpcBidirectional example compiles and runs
- [ ] Migration guide complete
- [ ] ARCHITECTURE.md updated

### Phase 6 Complete ✓
- [ ] All tests pass (unit + integration + examples)
- [ ] Code review passes (no linting errors)
- [ ] Performance benchmarks recorded
- [ ] Memory testing clean (no leaks, no overflows)
- [ ] Migration checklist complete

---

## Agent Loop Execution

Execute phases sequentially. For each task:

1. **Read Context**: Review relevant files from fastled3 and fastled9
2. **Implement**: Write/port code following FastLED standards
3. **Test**: Write unit tests, run `bash test`
4. **Verify**: Check output, fix errors
5. **Document**: Update comments, READMEs, architecture docs
6. **Review**: Run `/code_review`, fix issues
7. **Next Task**: Move to next task in sequence

**Parallel Execution**: Tasks within a phase can run in parallel if independent.

**Blocking Dependencies**:
- Phase 2 depends on Phase 1 (needs ResponseSend for ASYNC)
- Phase 3 depends on Phase 2 (needs HTTP transport)
- Phase 4 depends on Phase 3 (needs RPC integration)
- Phase 5 depends on Phase 4 (needs working tests)
- Phase 6 depends on Phase 5 (needs examples)

**Estimated Time**: ~2-3 days of agent work (assuming 8 hours/day)

---

## Notes for Human Review

- **HTTP Server Choice**: Using native POSIX sockets for simplicity, can port to ESP-IDF later
- **Chunked Encoding**: Standard HTTP/1.1, easy to test with curl/netcat
- **JSON-RPC 2.0**: Full spec compliance, includes notifications, batch requests
- **Reconnection**: Exponential backoff prevents server overload
- **Heartbeat**: Detects dead connections, prevents zombie clients
- **Testing**: Mock-first approach enables fast iteration without hardware

**Questions?** Ask before starting Phase 1.
