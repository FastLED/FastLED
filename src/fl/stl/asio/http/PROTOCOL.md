# HTTP Streaming RPC Protocol Specification

## Overview

This document specifies the protocol for JSON-RPC 2.0 communication over HTTP/1.1 with chunked transfer encoding. The protocol supports three RPC modes (SYNC, ASYNC, ASYNC_STREAM) with long-lived, bidirectional streaming connections.

## Protocol Version

- **JSON-RPC**: 2.0 (full spec compliance)
- **HTTP**: 1.1
- **Transfer Encoding**: chunked (bidirectional streaming)
- **Connection**: keep-alive (persistent connections)

---

## HTTP Request Format (Client → Server)

All RPC requests are sent as HTTP POST to the `/rpc` endpoint:

```http
POST /rpc HTTP/1.1
Host: <server-address>
Content-Type: application/json
Transfer-Encoding: chunked
Connection: keep-alive

<chunk-size-hex>\r\n
<json-rpc-request>\r\n
<chunk-size-hex>\r\n
<json-rpc-request>\r\n
...
0\r\n
\r\n
```

### Headers

| Header | Value | Required | Description |
|--------|-------|----------|-------------|
| `Content-Type` | `application/json` | Yes | JSON-RPC payload |
| `Transfer-Encoding` | `chunked` | Yes | Enables streaming |
| `Connection` | `keep-alive` | Yes | Persistent connection |
| `Host` | `<server:port>` | Yes | Server address |

### JSON-RPC Request Format

Follows [JSON-RPC 2.0 specification](https://www.jsonrpc.org/specification):

```json
{
  "jsonrpc": "2.0",
  "method": "<method-name>",
  "params": <params-array-or-object>,
  "id": <request-id>
}
```

**Fields**:
- `jsonrpc` (string, required): Must be `"2.0"`
- `method` (string, required): RPC method name
- `params` (array/object, optional): Method parameters
- `id` (number/string/null, required): Request identifier
  - `null` for notifications (no response expected)
  - Non-null for requests requiring response

---

## HTTP Response Format (Server → Client)

All RPC responses are sent over a single long-lived HTTP response with chunked encoding:

```http
HTTP/1.1 200 OK
Content-Type: application/json
Transfer-Encoding: chunked
Connection: keep-alive

<chunk-size-hex>\r\n
<json-rpc-response>\r\n
<chunk-size-hex>\r\n
<json-rpc-response>\r\n
...
0\r\n
\r\n
```

### Headers

| Header | Value | Required | Description |
|--------|-------|----------|-------------|
| `Content-Type` | `application/json` | Yes | JSON-RPC payload |
| `Transfer-Encoding` | `chunked` | Yes | Enables streaming |
| `Connection` | `keep-alive` | Yes | Persistent connection |

### JSON-RPC Response Format

#### Success Response

```json
{
  "jsonrpc": "2.0",
  "result": <result-value>,
  "id": <request-id>
}
```

#### Error Response

```json
{
  "jsonrpc": "2.0",
  "error": {
    "code": <error-code>,
    "message": "<error-message>",
    "data": <optional-error-data>
  },
  "id": <request-id>
}
```

**Standard Error Codes** (JSON-RPC 2.0):
- `-32700`: Parse error (invalid JSON)
- `-32600`: Invalid request (invalid JSON-RPC structure)
- `-32601`: Method not found
- `-32602`: Invalid params
- `-32603`: Internal error
- `-32000` to `-32099`: Server-defined errors

---

## RPC Modes

### SYNC Mode (Immediate Response)

**Behavior**: Server processes request and returns result immediately.

**Request Example**:
```http
POST /rpc HTTP/1.1
Host: localhost:8080
Content-Type: application/json
Transfer-Encoding: chunked
Connection: keep-alive

2F\r\n
{"jsonrpc":"2.0","method":"add","params":[1,2],"id":1}\r\n
0\r\n
\r\n
```

**Response Example**:
```http
HTTP/1.1 200 OK
Content-Type: application/json
Transfer-Encoding: chunked
Connection: keep-alive

27\r\n
{"jsonrpc":"2.0","result":3,"id":1}\r\n
0\r\n
\r\n
```

**Flow Diagram**:
```
Client                Server
  |                     |
  |---Request---------->|
  |                     | Process
  |<--Result------------|
  |                     |
```

---

### ASYNC Mode (ACK + Later Result)

**Behavior**: Server sends immediate ACK, then processes request and sends result later.

**Request Example**:
```http
POST /rpc HTTP/1.1
Host: localhost:8080
Content-Type: application/json
Transfer-Encoding: chunked
Connection: keep-alive

36\r\n
{"jsonrpc":"2.0","method":"longTask","params":{},"id":2}\r\n
0\r\n
\r\n
```

**Response Example**:
```http
HTTP/1.1 200 OK
Content-Type: application/json
Transfer-Encoding: chunked
Connection: keep-alive

30\r\n
{"jsonrpc":"2.0","result":{"ack":true},"id":2}\r\n
2D\r\n
{"jsonrpc":"2.0","result":{"value":42},"id":2}\r\n
0\r\n
\r\n
```

**ACK Object**:
```json
{"ack": true}
```

**Result Object**:
```json
{"value": <final-result>}
```

**Flow Diagram**:
```
Client                Server
  |                     |
  |---Request---------->|
  |<--ACK---------------|
  |                     | Process (async)
  |<--Result------------|
  |                     |
```

---

### ASYNC_STREAM Mode (ACK + Updates + Final)

**Behavior**: Server sends immediate ACK, then streams multiple updates, and finally sends result with `stop` marker.

**Request Example**:
```http
POST /rpc HTTP/1.1
Host: localhost:8080
Content-Type: application/json
Transfer-Encoding: chunked
Connection: keep-alive

39\r\n
{"jsonrpc":"2.0","method":"streamData","params":{},"id":3}\r\n
0\r\n
\r\n
```

**Response Example**:
```http
HTTP/1.1 200 OK
Content-Type: application/json
Transfer-Encoding: chunked
Connection: keep-alive

30\r\n
{"jsonrpc":"2.0","result":{"ack":true},"id":3}\r\n
30\r\n
{"jsonrpc":"2.0","result":{"update":10},"id":3}\r\n
30\r\n
{"jsonrpc":"2.0","result":{"update":20},"id":3}\r\n
30\r\n
{"jsonrpc":"2.0","result":{"update":30},"id":3}\r\n
3C\r\n
{"jsonrpc":"2.0","result":{"value":100,"stop":true},"id":3}\r\n
0\r\n
\r\n
```

**ACK Object**:
```json
{"ack": true}
```

**Update Object** (sent 0+ times):
```json
{"update": <progress-value>}
```

**Final Object** (sent exactly once):
```json
{"value": <final-result>, "stop": true}
```

**Flow Diagram**:
```
Client                Server
  |                     |
  |---Request---------->|
  |<--ACK---------------|
  |                     | Process (streaming)
  |<--Update------------|
  |<--Update------------|
  |<--Update------------|
  |<--Final+Stop--------|
  |                     |
```

**IMPORTANT**: The `stop` marker MUST be present in the final response to indicate end of stream.

---

## Heartbeat Protocol

To prevent connection timeouts and detect dead connections, both client and server send periodic heartbeat messages.

### Ping (Request)

Sent by either party to check connection liveness:

```json
{"jsonrpc":"2.0","method":"rpc.ping","id":null}
```

**Notes**:
- `id` is `null` (notification, no response required if unimplemented)
- Method name `rpc.ping` is reserved for heartbeat

### Pong (Response)

Optional response to ping (if implemented):

```json
{"jsonrpc":"2.0","result":"pong","id":null}
```

**Notes**:
- If server implements `rpc.ping`, it SHOULD respond with `"pong"`
- If server does not implement `rpc.ping`, it MAY ignore the notification

### Heartbeat Timing

**Default Configuration**:
- **Heartbeat Interval**: 30 seconds
- **Timeout**: 60 seconds (2x heartbeat interval)
- **Reconnect on Timeout**: Yes

**Behavior**:
1. Client/Server sends ping every 30 seconds if no other data sent
2. If no data received for 60 seconds, connection is considered dead
3. Client attempts reconnection with exponential backoff

---

## Connection Lifecycle

### Initial Connection

```
Client                Server
  |                     |
  |---TCP Connect------>|
  |<--TCP Accept--------|
  |                     |
  |---HTTP POST /rpc--->|
  |<--HTTP 200 OK-------|
  |                     |
  | (connection established, streaming begins)
```

### Normal Operation

```
Client                Server
  |<====Requests=======>|
  |<====Responses=======|
  |<====Heartbeats======|
  | (bidirectional streaming)
```

### Reconnection

**Trigger**: Connection lost (timeout, network error, server restart)

**Exponential Backoff**:
- Attempt 1: 1 second delay
- Attempt 2: 2 seconds delay
- Attempt 3: 4 seconds delay
- Attempt 4: 8 seconds delay
- Attempt 5+: 30 seconds delay (max)

**Flow**:
```
Client                Server
  |                     |
  | (connection lost)   X
  |                     |
  | (wait 1s)           |
  |---Reconnect-------->|
  | (failed)            X
  |                     |
  | (wait 2s)           |
  |---Reconnect-------->|
  |<--Connected---------|
  |                     |
  | (streaming resumes) |
```

**Max Reconnection Attempts**: Configurable (default: unlimited)

---

## Error Handling

### Connection Errors

**Scenarios**:
- TCP connection refused
- DNS resolution failure
- Network timeout

**Client Behavior**:
- Log error
- Attempt reconnection with exponential backoff
- Fire `onDisconnect()` callback

### HTTP Errors

**Non-200 Status Codes**:
- `4xx` (Client Error): Log and report to user
- `5xx` (Server Error): Log and attempt reconnection
- Other: Treat as connection error

**Malformed HTTP**:
- Invalid status line
- Missing required headers
- Invalid chunked encoding

**Client Behavior**:
- Close connection
- Attempt reconnection

### JSON-RPC Errors

**Parse Errors** (-32700):
```json
{
  "jsonrpc": "2.0",
  "error": {
    "code": -32700,
    "message": "Parse error",
    "data": "Invalid JSON at position 42"
  },
  "id": null
}
```

**Method Not Found** (-32601):
```json
{
  "jsonrpc": "2.0",
  "error": {
    "code": -32601,
    "message": "Method not found",
    "data": "add"
  },
  "id": 1
}
```

**Invalid Params** (-32602):
```json
{
  "jsonrpc": "2.0",
  "error": {
    "code": -32602,
    "message": "Invalid params",
    "data": "Expected 2 parameters, got 1"
  },
  "id": 1
}
```

---

## Security Considerations

### Transport Security

**Production Deployments SHOULD**:
- Use HTTPS (TLS/SSL) for all connections
- Validate server certificates
- Use mutual TLS for client authentication

**This Specification**:
- Describes HTTP (not HTTPS) for simplicity
- Implementation MUST support HTTPS upgrade path

### Authentication

**Not Specified**: This protocol does not define authentication mechanisms.

**Recommendations**:
- Use HTTP Basic Auth for simple scenarios
- Use OAuth 2.0 for production systems
- Use API keys in custom headers
- Implement JSON-RPC method-level authorization

### Rate Limiting

**Not Specified**: This protocol does not define rate limiting.

**Recommendations**:
- Implement server-side rate limiting per client IP
- Use HTTP 429 (Too Many Requests) status code
- Include `Retry-After` header with backoff time

---

## Implementation Requirements

### Client Requirements

**MUST**:
- Send HTTP POST to `/rpc` endpoint
- Include required headers (`Content-Type`, `Transfer-Encoding`, `Connection`)
- Use chunked encoding for requests
- Parse chunked encoding in responses
- Implement heartbeat (ping)
- Implement reconnection with exponential backoff
- Handle all three RPC modes (SYNC, ASYNC, ASYNC_STREAM)

**SHOULD**:
- Implement timeout detection (default 60s)
- Log connection state changes
- Provide connection state callbacks (`onConnect`, `onDisconnect`)

**MAY**:
- Implement custom heartbeat intervals
- Implement max reconnection attempts limit
- Support batch requests (multiple JSON-RPC requests in single HTTP request)

### Server Requirements

**MUST**:
- Accept HTTP POST on `/rpc` endpoint
- Parse chunked encoding in requests
- Send chunked encoding in responses
- Support all three RPC modes (SYNC, ASYNC, ASYNC_STREAM)
- Send proper JSON-RPC error responses
- Handle multiple concurrent clients

**SHOULD**:
- Implement heartbeat timeout detection
- Close idle connections after timeout
- Log client connections/disconnections
- Implement graceful shutdown (close all connections)

**MAY**:
- Implement `rpc.ping` method (respond to heartbeat)
- Support batch requests
- Implement rate limiting per client

---

## Example Implementations

### SYNC Request/Response

**Client Code** (pseudocode):
```cpp
// Connect
auto client = HttpStreamClient("localhost", 8080);
client.connect();

// Send SYNC request
Json request = Json::object()
    .set("jsonrpc", "2.0")
    .set("method", "add")
    .set("params", Json::array().add(1).add(2))
    .set("id", 1);
client.writeRequest(request);

// Receive response
auto response = client.readResponse();
// response = {"jsonrpc":"2.0","result":3,"id":1}
```

**Server Code** (pseudocode):
```cpp
// Bind method
remote.bind("add", [](int a, int b) { return a + b; });

// Accept connection
auto server = HttpStreamServer(8080);
server.listen();

// Handle request
auto request = server.readRequest();
auto response = remote.handle(request);
server.writeResponse(response);
```

---

### ASYNC Request/Response

**Client Code** (pseudocode):
```cpp
// Send ASYNC request
Json request = Json::object()
    .set("jsonrpc", "2.0")
    .set("method", "longTask")
    .set("params", Json::object())
    .set("id", 2);
client.writeRequest(request);

// Receive ACK
auto ack = client.readResponse();
// ack = {"jsonrpc":"2.0","result":{"ack":true},"id":2}

// Receive result (later)
auto result = client.readResponse();
// result = {"jsonrpc":"2.0","result":{"value":42},"id":2}
```

**Server Code** (pseudocode):
```cpp
// Bind ASYNC method
remote.bindAsync("longTask", [](ResponseSend& send, const Json& params) {
    // Send ACK immediately
    send.send(Json::object().set("ack", true));

    // Do work asynchronously
    std::thread([&send]() {
        sleep(5);  // Simulate long task
        send.send(Json::object().set("value", 42));
    }).detach();
}, RpcMode::ASYNC);
```

---

### ASYNC_STREAM Request/Response

**Client Code** (pseudocode):
```cpp
// Send ASYNC_STREAM request
Json request = Json::object()
    .set("jsonrpc", "2.0")
    .set("method", "streamData")
    .set("params", Json::object())
    .set("id", 3);
client.writeRequest(request);

// Receive ACK
auto ack = client.readResponse();
// ack = {"jsonrpc":"2.0","result":{"ack":true},"id":3}

// Receive updates
while (true) {
    auto update = client.readResponse();
    if (update["result"]["stop"].as_bool()) {
        // Final result
        // update = {"jsonrpc":"2.0","result":{"value":100,"stop":true},"id":3}
        break;
    } else {
        // Progress update
        // update = {"jsonrpc":"2.0","result":{"update":10},"id":3}
    }
}
```

**Server Code** (pseudocode):
```cpp
// Bind ASYNC_STREAM method
remote.bindAsync("streamData", [](ResponseSend& send, const Json& params) {
    // Send ACK immediately
    send.send(Json::object().set("ack", true));

    // Stream updates
    std::thread([&send]() {
        for (int i = 0; i < 10; i++) {
            send.sendUpdate(Json::object().set("update", i * 10));
            sleep(1);
        }
        send.sendFinal(Json::object().set("value", 100));
    }).detach();
}, RpcMode::ASYNC_STREAM);
```

---

## Protocol Extensions

### Batch Requests (Optional)

Multiple requests in single HTTP POST:

**Request**:
```json
[
  {"jsonrpc":"2.0","method":"add","params":[1,2],"id":1},
  {"jsonrpc":"2.0","method":"subtract","params":[5,3],"id":2}
]
```

**Response**:
```json
[
  {"jsonrpc":"2.0","result":3,"id":1},
  {"jsonrpc":"2.0","result":2,"id":2}
]
```

### Notifications (No Response)

Request with `id: null`:

**Request**:
```json
{"jsonrpc":"2.0","method":"log","params":["Hello"],"id":null}
```

**Response**: None (notification, no response expected)

---

## Compliance Checklist

### JSON-RPC 2.0 Compliance

- [x] Support `jsonrpc: "2.0"` field in all requests/responses
- [x] Support `method` field in requests
- [x] Support `params` field (array or object)
- [x] Support `id` field (number, string, or null)
- [x] Support `result` field in success responses
- [x] Support `error` field in error responses
- [x] Support standard error codes (-32700 to -32603)
- [x] Support notifications (`id: null`)
- [ ] Support batch requests (optional)

### HTTP/1.1 Compliance

- [x] Support HTTP POST method
- [x] Support required headers (Content-Type, Transfer-Encoding, Connection)
- [x] Support chunked transfer encoding (read and write)
- [x] Support keep-alive connections
- [x] Support HTTP status codes (200, 4xx, 5xx)

### Custom Features

- [x] SYNC mode (immediate response)
- [x] ASYNC mode (ACK + later result)
- [x] ASYNC_STREAM mode (ACK + updates + final)
- [x] Heartbeat protocol (ping/pong)
- [x] Reconnection with exponential backoff
- [x] Connection timeout detection

---

## References

- [JSON-RPC 2.0 Specification](https://www.jsonrpc.org/specification)
- [HTTP/1.1 Specification (RFC 7230)](https://datatracker.ietf.org/doc/html/rfc7230)
- [Chunked Transfer Encoding (RFC 7230 Section 4.1)](https://datatracker.ietf.org/doc/html/rfc7230#section-4.1)

---

## Changelog

- **2026-02-16**: Initial protocol specification (v1.0)
