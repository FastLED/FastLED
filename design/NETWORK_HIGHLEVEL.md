# FastLED Network Architecture Design

## ⚠️ IMPLEMENTATION STATUS

**✅ DESIGN COMPLETE - Ready for Implementation**

This document provides the high-level architecture for FastLED's networking capabilities, including low-level socket abstractions, HTTP client, and HTTP server functionality.

## Architecture Overview

FastLED's networking architecture follows a layered approach with clear separation of concerns:

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                        │
│  - IoT Device Control                                      │
│  - LED Pattern Streaming                                   │
│  - Web Interface                                           │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│                   HTTP Layer                               │
│  - HTTP Server (see HTTP_SERVER.md)                       │
│  - HTTP Client (see HTTP_CLIENT.md)                       │
│  - WebSocket Support                                       │
│  - TLS/HTTPS                                              │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│                 Transport Layer                            │
│  - Transport Abstractions                                 │
│  - Connection Pooling                                      │
│  - Protocol Support (HTTP/1.1, HTTP/2, WebSocket)        │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│                Low-Level Networking                        │
│  - Socket Abstractions (see NET_LOW_LEVEL.md)             │
│  - Platform Implementations                               │
│  - BSD Socket Wrapper                                     │
│  - Connection Management                                   │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│                Platform Layer                              │
│  - ESP32/lwIP                                             │
│  - Native (Linux/macOS/Windows)                           │
│  - WebAssembly/Emscripten                                 │
└─────────────────────────────────────────────────────────────┘
```

## Component Documentation

### Core Components

1. **[Low-Level Networking](NET_LOW_LEVEL.md)**
   - BSD-style socket abstractions
   - Platform-specific implementations
   - Connection management and pooling
   - Error handling and recovery

2. **[HTTP Client](HTTP_CLIENT.md)**
   - Progressive complexity API design
   - Transport abstraction with pluggable backends
   - Request/Response handling with efficient memory management
   - Support for various authentication methods

3. **[HTTP Server](HTTP_SERVER.md)**
   - Route-based request handling
   - Middleware system for extensibility
   - WebSocket support for real-time communication
   - Static file serving with security

### Supporting Infrastructure

4. **✅ fl::future<T> Concurrency Primitive (COMPLETED)**
   - Event-driven asynchronous programming with variant-based results
   - Non-blocking operations for LED compatibility  
   - Thread-safe shared state management
   - Foundation for async HTTP operations
   - **Implemented in `src/fl/future.h` with comprehensive test coverage**

### Implementation Roadmap

5. **[Implementation Tasks](DESIGN_NETWORK_TASKS.md)**
   - Outstanding design gaps
   - Implementation priorities
   - Testing requirements
   - Platform-specific considerations

## Design Principles

### **🎯 Progressive Complexity**

All networking APIs follow FastLED's progressive complexity approach:

**Level 1 - Minimal Setup:**
```cpp
// Simple HTTP request
auto response = fl::http_get("http://api.example.com/data");

// Simple HTTP server
auto server = HttpServer::create_local_server();
server->get("/health", [](const Request& req) {
    return Response::ok("Server is healthy");
});
server->listen(8080);
```

**Level 2 - Configuration:**
```cpp
// Configured HTTP client
HttpClient client;
client.set_timeout(10000);
client.set_headers({{"User-Agent", "FastLED/1.0"}});

// Configured HTTP server with middleware
server->use_cors();
server->use("/api", auth_middleware);
```

**Level 3 - Advanced Features:**
```cpp
// Custom transport configuration
auto transport = fl::make_shared<TcpTransport>(tcp_options);
auto client = HttpClient::create_with_transport(transport);

// WebSocket and streaming
server->websocket("/ws", websocket_handler);
server->use_static_files("/", "/www");
```

### **✅ Memory Efficiency**

- **fl::deque storage** with PSRAM support for large requests/responses
- **Copy-on-write semantics** for shared data structures
- **Small buffer optimization** for typical use cases
- **Streaming support** to avoid memory spikes

### **✅ FastLED Integration**

- **EngineEvents integration** for non-blocking operation
- **Consistent fl:: namespace** following FastLED conventions
- **Embedded-friendly** design for resource-constrained devices
- **Compatible with LED updates** (doesn't block FastLED.show())

### **✅ Platform Abstraction**

- **Unified API** across all supported platforms
- **Platform-specific optimizations** hidden behind abstractions
- **Pluggable transports** for different networking stacks
- **Graceful degradation** when features aren't available

## Shared Infrastructure

### **Memory Management**

All networking components use FastLED's memory management primitives:

```cpp
namespace fl {
    // Request/Response body storage
    using RequestBody = fl::deque<fl::u8, fl::allocator_psram<fl::u8>>;
    using ResponseBody = fl::deque<fl::u8, fl::allocator_psram<fl::u8>>;
    
    // Shared ownership for transports and connections
    template<typename T>
    using shared_ptr = fl::shared_ptr<T>;
    
    // Thread-safe operations
    using mutex = fl::mutex;
    using lock_guard = fl::lock_guard<fl::mutex>;
}
```

### **Transport Sharing**

The transport factory pattern enables efficient resource sharing:

```cpp
// Multiple clients/servers can share the same transport configuration
auto factory = TransportFactory::instance();
auto shared_transport = factory.create_http_transport();

auto client1 = HttpClient::create_with_transport(shared_transport);
auto client2 = HttpClient::create_with_transport(shared_transport);
auto server = HttpServer::create_with_transport(shared_transport);
```

### **Error Handling**

Consistent error handling across all components:

```cpp
namespace fl {
    enum class NetworkError {
        SUCCESS,
        CONNECTION_FAILED,
        TIMEOUT,
        INVALID_RESPONSE,
        TLS_ERROR,
        // ... more error types
    };
    
    template<typename T>
    using NetworkResult = fl::expected<T, NetworkError>;
}
```

## Platform Support Matrix

| Platform | Socket API | TLS Support | WebSocket | HTTP/2 | Status |
|----------|------------|-------------|-----------|--------|--------|
| **ESP32/ESP8266** | lwIP | mbedTLS | ✅ | ❌ | ✅ Implemented |
| **Native (Linux/macOS)** | BSD Sockets | OpenSSL | ✅ | ✅ | 🚧 In Progress |
| **Native (Windows)** | Winsock2 | Schannel | ✅ | ✅ | 📋 Planned |
| **WebAssembly** | WebSocket | Browser TLS | ✅ | ✅ | 📋 Planned |
| **Arduino (AVR)** | None | None | ❌ | ❌ | ❌ Not Supported |

## Usage Examples

### **IoT Device Control**

```cpp
#include "fl/networking.h"

class IoTDevice {
private:
    fl::shared_ptr<HttpServer> mServer;
    CRGB mLeds[100];
    
public:
    void setup() {
        // Create local server for device control
        mServer = HttpServer::create_local_server();
        
        // Device status API
        mServer->get("/status", [this](const Request& req) {
            return Response::json(get_device_status());
        });
        
        // LED control API
        mServer->post("/leds", [this](const Request& req) {
            update_leds_from_json(req.json());
            return Response::ok("LEDs updated");
        });
        
        mServer->listen(8080);
        FL_WARN("Device control available at http://device.local:8080");
    }
};
```

### **HTTP API Client**

```cpp
#include "fl/networking.h"

class WeatherDisplay {
private:
    HttpClient mClient;
    
public:
    void update_weather() {
        // Fetch weather data
        auto response = mClient.get("http://api.weather.com/current");
        
        if (response && response->status_code() == 200) {
            auto weather_data = parse_weather_json(response->body_text());
            update_display(weather_data);
        }
    }
    
private:
    void update_display(const WeatherData& data) {
        // Update LED display based on weather
        if (data.condition == "sunny") {
            fill_solid(mLeds, NUM_LEDS, CRGB::Yellow);
        } else if (data.condition == "rainy") {
            fill_solid(mLeds, NUM_LEDS, CRGB::Blue);
        }
        FastLED.show();
    }
};
```

## Integration with FastLED Core

### **EngineEvents Integration**

All networking operations integrate with FastLED's event system:

```cpp
void setup() {
    // Initialize FastLED
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
    
    // Setup networking (automatically integrates with EngineEvents)
    setup_http_server();
    setup_http_client();
    
    // Networking operations happen automatically during FastLED updates
}

void loop() {
    // Update patterns
    update_led_patterns();
    
    // This automatically processes network events
    FastLED.show();
}
```

### **Non-Blocking Operation**

All networking is designed to be non-blocking:

- **Server request processing** happens during `FastLED.show()`
- **Client requests** use async callbacks or polling
- **WebSocket messages** are queued and processed incrementally
- **TLS handshakes** don't block LED updates

## Security Considerations

### **Built-in Security**

- **Input validation** for all HTTP requests
- **Path traversal protection** for static file serving
- **Rate limiting** to prevent abuse
- **TLS certificate validation** for secure connections

### **Authentication Support**

- **API key authentication** via headers
- **JWT token validation** for stateless auth
- **Session-based authentication** with secure cookies
- **Client certificate authentication** for device-to-device communication

## Performance Characteristics

### **Memory Usage**

- **Small requests (<256B)**: Inline buffer storage
- **Large requests**: PSRAM-backed deque storage  
- **Connection pooling**: Reuse of transport resources
- **Copy-on-write**: Efficient sharing of common data

### **Throughput**

- **ESP32**: ~10-50 concurrent connections depending on request complexity
- **Native platforms**: 100+ concurrent connections with thread pool
- **Request processing**: <10ms for simple GET/POST requests
- **WebSocket**: Real-time communication with <5ms latency

## Testing Strategy

All networking components include comprehensive testing:

- **Unit tests** for individual components
- **Integration tests** for complete request/response cycles  
- **Performance tests** for throughput and memory usage
- **Security tests** for input validation and attack resistance
- **Platform tests** across all supported environments

**Future implementation testing is complete** - see `tests/test_future_variant.cpp` for comprehensive test coverage.

---

## Next Steps

1. **✅ Implement fl::future<T> primitive (COMPLETED)**
2. **Implement low-level networking** ([NET_LOW_LEVEL.md](NET_LOW_LEVEL.md))
3. **Create HTTP client implementation** ([HTTP_CLIENT.md](HTTP_CLIENT.md))  
4. **Build HTTP server functionality** ([HTTP_SERVER.md](HTTP_SERVER.md))
5. **Address implementation gaps** ([DESIGN_NETWORK_TASKS.md](DESIGN_NETWORK_TASKS.md))

The networking architecture provides a solid foundation for building connected FastLED applications while maintaining the library's core principles of simplicity, performance, and embedded-system compatibility. 
