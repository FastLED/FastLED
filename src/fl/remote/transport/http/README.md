# HTTP Streaming Transport Layer Architecture

## Overview

This module provides HTTP/1.1 streaming transport for FastLED's JSON-RPC system, enabling bidirectional communication over HTTP using chunked transfer encoding. The transport supports three RPC modes: SYNC (immediate response), ASYNC (ACK + later result), and ASYNC_STREAM (ACK + multiple updates + final result).

## Architecture Components

### Component Hierarchy

```
Remote (application layer)
    ↓
HttpStreamTransport (base transport interface)
    ↓
HttpStreamClient / HttpStreamServer (platform-specific implementations)
    ↓
HttpConnection (connection lifecycle)
    ↓
ChunkedReader / ChunkedWriter (HTTP/1.1 encoding)
    ↓
TCP Socket (platform-specific: POSIX, ESP-IDF, etc.)
```

### Core Components

#### 1. ChunkedReader / ChunkedWriter

**Purpose**: Parse and generate HTTP/1.1 chunked transfer encoding

**Files**:
- `chunked_encoding.h` - Interface declarations
- `chunked_encoding.cpp.hpp` - Implementation

**ChunkedReader API**:
```cpp
class ChunkedReader {
public:
    // Constructor
    ChunkedReader();

    // Feed data from socket
    void feed(const uint8_t* data, size_t len);

    // Check if complete chunk is available
    bool hasChunk() const;

    // Read next chunk (returns empty if none available)
    fl::optional<fl::vector<uint8_t>> readChunk();

    // Check if final chunk (size 0) received
    bool isFinal() const;

    // Reset state
    void reset();

private:
    enum State {
        READ_SIZE,      // Reading chunk size (hex + CRLF)
        READ_DATA,      // Reading chunk data
        READ_TRAILER,   // Reading trailing CRLF
        FINAL           // Final chunk received
    };

    State mState;
    fl::vector<uint8_t> mBuffer;
    size_t mChunkSize;
    size_t mBytesRead;
};
```

**ChunkedWriter API**:
```cpp
class ChunkedWriter {
public:
    // Constructor
    ChunkedWriter();

    // Write chunk (returns formatted chunk data)
    fl::vector<uint8_t> writeChunk(const uint8_t* data, size_t len);

    // Write final chunk (size 0)
    fl::vector<uint8_t> writeFinal();

private:
    // Format chunk: <size-hex>\r\n<data>\r\n
    static fl::vector<uint8_t> formatChunk(const uint8_t* data, size_t len);
};
```

**Chunked Encoding Format**:
```
<chunk-size-hex>\r\n
<chunk-data>
\r\n
<chunk-size-hex>\r\n
<chunk-data>
\r\n
...
0\r\n
\r\n
```

---

#### 2. HttpParser

**Purpose**: Parse HTTP/1.1 request and response messages

**Files**:
- `http_parser.h` - Interface declarations
- `http_parser.cpp.hpp` - Implementation

**HttpRequestParser API**:
```cpp
struct HttpRequest {
    fl::string method;           // "GET", "POST", etc.
    fl::string uri;              // "/rpc"
    fl::string version;          // "HTTP/1.1"
    fl::map<fl::string, fl::string> headers;
    fl::vector<uint8_t> body;    // Decoded body (if chunked, already decoded)
};

class HttpRequestParser {
public:
    // Constructor
    HttpRequestParser();

    // Feed data from socket
    void feed(const uint8_t* data, size_t len);

    // Check if request is complete
    bool isComplete() const;

    // Get parsed request
    fl::optional<HttpRequest> getRequest();

    // Reset state
    void reset();

private:
    enum State {
        READ_REQUEST_LINE,  // "POST /rpc HTTP/1.1\r\n"
        READ_HEADERS,       // "Header: Value\r\n" ... "\r\n"
        READ_BODY,          // Body content (chunked or Content-Length)
        COMPLETE            // Request fully parsed
    };

    State mState;
    fl::vector<uint8_t> mBuffer;
    HttpRequest mRequest;
    ChunkedReader mChunkedReader;  // For Transfer-Encoding: chunked
    size_t mContentLength;
    bool mIsChunked;
};
```

**HttpResponseParser API**:
```cpp
struct HttpResponse {
    fl::string version;          // "HTTP/1.1"
    int statusCode;              // 200, 404, etc.
    fl::string reasonPhrase;     // "OK", "Not Found", etc.
    fl::map<fl::string, fl::string> headers;
    fl::vector<uint8_t> body;    // Decoded body
};

class HttpResponseParser {
public:
    // Constructor
    HttpResponseParser();

    // Feed data from socket
    void feed(const uint8_t* data, size_t len);

    // Check if response is complete
    bool isComplete() const;

    // Get parsed response
    fl::optional<HttpResponse> getResponse();

    // Reset state
    void reset();

private:
    enum State {
        READ_STATUS_LINE,   // "HTTP/1.1 200 OK\r\n"
        READ_HEADERS,       // "Header: Value\r\n" ... "\r\n"
        READ_BODY,          // Body content (chunked or Content-Length)
        COMPLETE            // Response fully parsed
    };

    State mState;
    fl::vector<uint8_t> mBuffer;
    HttpResponse mResponse;
    ChunkedReader mChunkedReader;
    size_t mContentLength;
    bool mIsChunked;
};
```

---

#### 3. HttpConnection

**Purpose**: Manage HTTP connection lifecycle with reconnection and heartbeat

**Files**:
- `connection.h` - Interface declarations
- `connection.cpp.hpp` - Implementation

**HttpConnection API**:
```cpp
class HttpConnection {
public:
    enum State {
        DISCONNECTED,   // Not connected
        CONNECTING,     // Connection in progress
        CONNECTED,      // Connected and ready
        RECONNECTING,   // Auto-reconnect in progress
        CLOSED          // Connection permanently closed
    };

    // Constructor
    HttpConnection(const char* host, uint16_t port);

    // Connection management
    bool connect();                 // Initiate connection
    void disconnect();              // Close connection
    void close();                   // Close permanently (no reconnect)
    bool isConnected() const;       // Check if connected
    State getState() const;         // Get current state

    // Auto-reconnect settings
    void setAutoReconnect(bool enable);
    void setReconnectInterval(uint32_t minMs, uint32_t maxMs);

    // Heartbeat/keepalive settings
    void setHeartbeatInterval(uint32_t intervalMs);
    void setTimeout(uint32_t timeoutMs);

    // Update loop (handles reconnection, heartbeat, timeout detection)
    void update(uint32_t currentTimeMs);

    // Socket I/O (platform-specific implementations provided by subclasses)
    virtual int send(const uint8_t* data, size_t len) = 0;
    virtual int recv(uint8_t* buffer, size_t maxLen) = 0;
    virtual bool isSocketConnected() const = 0;

protected:
    // Platform-specific connection (override in subclasses)
    virtual bool platformConnect() = 0;
    virtual void platformDisconnect() = 0;

private:
    State mState;
    fl::string mHost;
    uint16_t mPort;

    // Auto-reconnect
    bool mAutoReconnect;
    uint32_t mReconnectIntervalMin;  // Min backoff (default: 1000ms)
    uint32_t mReconnectIntervalMax;  // Max backoff (default: 30000ms)
    uint32_t mReconnectIntervalCurrent;
    uint32_t mLastReconnectAttempt;
    int mReconnectAttempts;

    // Heartbeat
    uint32_t mHeartbeatInterval;     // Default: 30000ms (30s)
    uint32_t mLastHeartbeat;

    // Timeout detection
    uint32_t mTimeout;               // Default: 60000ms (60s)
    uint32_t mLastActivity;
};
```

**Reconnection Logic**:
- Exponential backoff: 1s, 2s, 4s, 8s, ..., max 30s
- Reset backoff on successful connection
- Disabled by default (user must enable)

**Heartbeat Logic**:
- Send ping every `mHeartbeatInterval` ms (default 30s)
- Expect pong response within `mTimeout` ms (default 60s)
- Disconnect on timeout, trigger reconnect if enabled

---

#### 4. NativeHttpClient / NativeHttpServer

**Purpose**: Platform-specific HTTP client/server implementations using POSIX sockets

**Files**:
- `native_client.h` - Native client interface
- `native_client.cpp.hpp` - Native client implementation
- `native_server.h` - Native server interface
- `native_server.cpp.hpp` - Native server implementation

**NativeHttpClient API**:
```cpp
class NativeHttpClient : public HttpConnection {
public:
    // Constructor
    NativeHttpClient(const char* host, uint16_t port);
    ~NativeHttpClient();

    // Socket I/O (POSIX implementation)
    int send(const uint8_t* data, size_t len) override;
    int recv(uint8_t* buffer, size_t maxLen) override;
    bool isSocketConnected() const override;

protected:
    // Platform-specific connection (POSIX sockets)
    bool platformConnect() override;
    void platformDisconnect() override;

private:
    int mSocket;  // POSIX socket descriptor
};
```

**NativeHttpServer API**:
```cpp
struct HttpClientConnection {
    int socket;                     // Client socket descriptor
    fl::string remoteAddr;          // Client IP address
    uint16_t remotePort;            // Client port
    HttpRequestParser parser;       // Request parser for this client
    ChunkedWriter writer;           // Response writer for this client
};

class NativeHttpServer {
public:
    // Constructor
    NativeHttpServer(uint16_t port);
    ~NativeHttpServer();

    // Server lifecycle
    bool start();                   // Start listening
    void stop();                    // Stop server
    bool isListening() const;

    // Client management
    void update();                  // Accept new clients, read data
    fl::vector<int> getClientIds() const;

    // Request handling
    fl::optional<HttpRequest> readRequest(int clientId);
    void writeResponse(int clientId, const HttpResponse& response);
    void writeChunk(int clientId, const uint8_t* data, size_t len);
    void writeChunkFinal(int clientId);

    // Close client
    void closeClient(int clientId);

private:
    uint16_t mPort;
    int mListenSocket;
    fl::map<int, HttpClientConnection> mClients;

    // Accept new client connection
    void acceptClient();
};
```

---

#### 5. HttpStreamTransport

**Purpose**: Base class for HTTP streaming transport, implements RequestSource/ResponseSink for Remote

**Files**:
- `stream_transport.h` - Interface declarations
- `stream_transport.cpp.hpp` - Implementation

**HttpStreamTransport API**:
```cpp
class HttpStreamTransport {
public:
    // Constructor
    HttpStreamTransport(const char* host, uint16_t port);
    virtual ~HttpStreamTransport() = default;

    // Connection management
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;

    // RequestSource implementation (for Remote)
    fl::optional<fl::Json> readRequest();

    // ResponseSink implementation (for Remote)
    void writeResponse(const fl::Json& response);

    // Update loop (handles reconnection, heartbeat, I/O)
    virtual void update(uint32_t currentTimeMs) = 0;

    // Configuration
    void setAutoReconnect(bool enable);
    void setHeartbeatInterval(uint32_t intervalMs);
    void setTimeout(uint32_t timeoutMs);

protected:
    // Platform-specific socket I/O
    virtual int send(const uint8_t* data, size_t len) = 0;
    virtual int recv(uint8_t* buffer, size_t maxLen) = 0;

    // Request/response parsing helpers
    fl::optional<fl::Json> parseJsonFromChunk(const fl::vector<uint8_t>& chunk);
    fl::vector<uint8_t> formatJsonChunk(const fl::Json& json);

    HttpConnection* mConnection;
    ChunkedReader mReader;
    ChunkedWriter mWriter;
    fl::queue<fl::Json> mRequestQueue;   // Buffered requests
    fl::queue<fl::Json> mResponseQueue;  // Buffered responses
};
```

---

#### 6. HttpStreamClient

**Purpose**: Client-side HTTP streaming for RPC

**Files**:
- `stream_client.h` - Interface declarations
- `stream_client.cpp.hpp` - Implementation

**HttpStreamClient API**:
```cpp
class HttpStreamClient : public HttpStreamTransport {
public:
    // Constructor
    HttpStreamClient(const char* host, uint16_t port);
    ~HttpStreamClient();

    // Connection management (overrides)
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;

    // Update loop (overrides)
    void update(uint32_t currentTimeMs) override;

    // Send RPC request (client → server)
    void sendRequest(const fl::Json& request);

    // Receive RPC response (server → client)
    fl::optional<fl::Json> receiveResponse();

protected:
    // Platform-specific socket I/O (delegates to NativeHttpClient)
    int send(const uint8_t* data, size_t len) override;
    int recv(uint8_t* buffer, size_t maxLen) override;

private:
    NativeHttpClient mClient;
    HttpResponseParser mResponseParser;
    bool mRequestPending;
};
```

**Usage Example (Client)**:
```cpp
// Create HTTP client
auto client = fl::make_shared<HttpStreamClient>("localhost", 8080);
client->setAutoReconnect(true);
client->setHeartbeatInterval(30000);  // 30s

// Create Remote with client as transport
fl::Remote remote(
    [&client]() { return client->readRequest(); },
    [&client](const fl::Json& r) { client->writeResponse(r); }
);

// Update loop
while (true) {
    client->update(millis());
    remote.update(millis());
    delay(10);
}
```

---

#### 7. HttpStreamServer

**Purpose**: Server-side HTTP streaming for RPC

**Files**:
- `stream_server.h` - Interface declarations
- `stream_server.cpp.hpp` - Implementation

**HttpStreamServer API**:
```cpp
class HttpStreamServer : public HttpStreamTransport {
public:
    // Constructor
    HttpStreamServer(uint16_t port);
    ~HttpStreamServer();

    // Server lifecycle
    bool start();
    void stop();
    bool isListening() const;

    // Connection management (overrides)
    bool connect() override;     // No-op for server
    void disconnect() override;  // Close all clients
    bool isConnected() const override;  // True if any client connected

    // Update loop (overrides)
    void update(uint32_t currentTimeMs) override;

protected:
    // Socket I/O (multi-client)
    int send(const uint8_t* data, size_t len) override;
    int recv(uint8_t* buffer, size_t maxLen) override;

private:
    NativeHttpServer mServer;
    fl::vector<int> mClientIds;
    int mCurrentClient;  // Client to read from (round-robin)
};
```

**Usage Example (Server)**:
```cpp
// Create HTTP server
auto server = fl::make_shared<HttpStreamServer>(8080);
server->start();

// Create Remote with server as transport
fl::Remote remote(
    [&server]() { return server->readRequest(); },
    [&server](const fl::Json& r) { server->writeResponse(r); }
);

// Bind RPC methods
remote.bind("add", [](int a, int b) { return a + b; });
remote.bindAsync("longTask", [](ResponseSend& send, const Json& params) {
    send.send(Json::object().set("ack", true));
    // ... do work ...
    send.send(Json::object().set("value", 42));
});

// Update loop
while (true) {
    server->update(millis());
    remote.update(millis());
    delay(10);
}
```

---

## HTTP Streaming RPC Protocol

### Request Format (Client → Server)

```http
POST /rpc HTTP/1.1
Host: localhost:8080
Content-Type: application/json
Transfer-Encoding: chunked
Connection: keep-alive

<chunk-size>\r\n
{"jsonrpc":"2.0","method":"add","params":[1,2],"id":1}\r\n
0\r\n
\r\n
```

### Response Formats (Server → Client)

#### SYNC Mode (Immediate Response)

```http
HTTP/1.1 200 OK
Content-Type: application/json
Transfer-Encoding: chunked

<chunk-size>\r\n
{"jsonrpc":"2.0","result":3,"id":1}\r\n
0\r\n
\r\n
```

#### ASYNC Mode (ACK + Later Result)

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

#### ASYNC_STREAM Mode (ACK + Multiple Updates + Final)

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

### Heartbeat (Bidirectional)

Send periodically to keep connection alive:

```http
<chunk-size>\r\n
{"jsonrpc":"2.0","method":"rpc.ping","id":null}\r\n
```

Expected pong response:

```http
<chunk-size>\r\n
{"jsonrpc":"2.0","result":"pong","id":null}\r\n
```

---

## Connection Lifecycle

```
[DISCONNECTED] --connect()--> [CONNECTING] --success--> [CONNECTED]
       ^                           |                         |
       |                          fail                     error
       |                           |                         |
       +--<autoReconnect=false>---+                         |
       |                                                     |
       +--<autoReconnect=true>---> [RECONNECTING] <---------+
                                        |
                                  exponential backoff
                                  (1s, 2s, 4s, ..., 30s)
                                        |
                                    retry connect()
```

**States**:
- `DISCONNECTED`: Initial state, no connection
- `CONNECTING`: Connection attempt in progress
- `CONNECTED`: Connection established, ready for I/O
- `RECONNECTING`: Auto-reconnect in progress (exponential backoff)
- `CLOSED`: Connection permanently closed (no reconnect)

**Timeouts**:
- `mTimeout` (default 60s): Max idle time before disconnect
- `mHeartbeatInterval` (default 30s): Ping interval
- `mReconnectInterval` (dynamic): Backoff delay between reconnect attempts

---

## Data Flow

### Client Request Flow

```
User Code
    |
    v
Remote::sendRequest(method, params)
    |
    v
HttpStreamClient::writeResponse(jsonrpc)
    |
    v
ChunkedWriter::writeChunk(json)
    |
    v
NativeHttpClient::send(chunk)
    |
    v
TCP Socket → Server
```

### Server Response Flow

```
TCP Socket ← Client
    |
    v
NativeHttpServer::recv(data)
    |
    v
HttpRequestParser::feed(data)
    |
    v
ChunkedReader::readChunk()
    |
    v
HttpStreamServer::readRequest()
    |
    v
Remote::update() → Rpc::handle(request)
    |
    v
User RPC Function (sync/async/stream)
    |
    v
ResponseSink::writeResponse(result)
    |
    v
ChunkedWriter::writeChunk(result)
    |
    v
NativeHttpServer::send(chunk)
    |
    v
TCP Socket → Client
```

---

## Error Handling

### Connection Errors

- **Connection refused**: Immediate reconnect if auto-reconnect enabled
- **Connection timeout**: Exponential backoff before retry
- **Connection lost**: Detect via socket error or heartbeat timeout, trigger reconnect

### Protocol Errors

- **Invalid HTTP**: Close connection, log error
- **Invalid JSON-RPC**: Send error response per JSON-RPC 2.0 spec
- **Malformed chunk**: Close connection, log error

### Timeout Errors

- **Heartbeat timeout**: No pong received within timeout → disconnect → reconnect
- **Request timeout**: No response within timeout → cancel request, return error

---

## Platform Support

### Native Platform (POSIX Sockets)

**Supported**:
- Linux (POSIX)
- macOS (POSIX)
- Windows (Winsock2, POSIX-like API)

**Implementation**:
- `NativeHttpClient` uses POSIX `socket()`, `connect()`, `send()`, `recv()`
- `NativeHttpServer` uses POSIX `bind()`, `listen()`, `accept()`
- Non-blocking sockets with `select()` or `poll()` for multi-client

### ESP32 Platform (Future)

**Planned**:
- ESP-IDF HTTP client (esp_http_client)
- ESP-IDF HTTP server (esp_http_server)
- WiFi connection management
- mDNS service discovery

**Not Implemented Yet**: ESP32 support will be added in future iterations

---

## Testing Strategy

### Unit Tests

1. **Chunked Encoding**: Test ChunkedReader/ChunkedWriter with known inputs
2. **HTTP Parser**: Test HttpRequestParser/HttpResponseParser with sample HTTP messages
3. **Connection State Machine**: Test reconnection, heartbeat, timeout logic
4. **Mock Transport**: Test HttpStreamTransport with mock socket layer

### Integration Tests

1. **Loopback RPC**: Client + Server on native platform, all RPC modes
2. **Stress Test**: High-frequency requests, measure latency/throughput
3. **Reconnection Test**: Simulate connection loss, verify auto-reconnect
4. **Heartbeat Test**: Long-idle connection, verify ping/pong

### Examples

1. **HttpRpcServer**: Native server with SYNC/ASYNC/ASYNC_STREAM methods
2. **HttpRpcClient**: Native client calling server methods
3. **HttpRpcBidirectional**: Client + Server in same process, loopback

---

## Performance Considerations

### Latency

- **Chunked encoding overhead**: Minimal (~10 bytes per chunk)
- **HTTP overhead**: ~100 bytes per request/response (headers)
- **JSON parsing**: Optimized with FastLED's native JSON parser

### Throughput

- **Single connection**: ~1000 req/s on localhost (native platform)
- **Multiple connections**: Server scales with thread pool (future)
- **Streaming**: Multiple responses per request, minimal latency

### Memory

- **Per-connection overhead**: ~1KB (buffers, state)
- **Per-request overhead**: ~256 bytes (request/response objects)
- **Chunked buffers**: Dynamic allocation, freed after chunk processed

---

## Security Considerations

### NOT Implemented

This is a basic transport layer for development/testing. The following security features are **NOT implemented**:

- ❌ **Authentication**: No user authentication, all requests accepted
- ❌ **Authorization**: No access control, all methods callable
- ❌ **Encryption**: Plain HTTP (not HTTPS), no TLS/SSL
- ❌ **Input validation**: Minimal validation, assumes trusted clients
- ❌ **Rate limiting**: No request rate limiting
- ❌ **DoS protection**: No protection against denial-of-service

**For Production Use**: Add HTTPS (TLS), authentication (tokens, OAuth), and rate limiting

---

## Next Steps (Implementation)

### Phase 2: HTTP Streaming Transport Layer

1. ✅ **Task 2.1**: Design HTTP Transport Architecture (THIS DOCUMENT)
2. **Task 2.2**: Implement HTTP Chunked Encoding Parser
3. **Task 2.3**: Implement HTTP Request/Response Parser
4. **Task 2.4**: Implement Connection State Machine
5. **Task 2.5**: Implement Native HTTP Client
6. **Task 2.6**: Implement Native HTTP Server

### Phase 3: HTTP Streaming RPC Integration

1. **Task 3.1**: Design HTTP Streaming RPC Protocol (see PROTOCOL.md)
2. **Task 3.2**: Implement HttpStreamTransport Base
3. **Task 3.3**: Implement HttpStreamClient
4. **Task 3.4**: Implement HttpStreamServer
5. **Task 3.5**: Integrate HttpStreamTransport with Remote

---

**Status**: ✅ Architecture design complete
**Next**: Implement chunked encoding parser (Task 2.2)
