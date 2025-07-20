# FastLED Low-Level Networking Design

## ⚠️ IMPLEMENTATION STATUS

**✅ DESIGN COMPLETE - Stub Implementation COMPLETED**

This document defines the low-level networking abstractions for FastLED, including BSD-style socket wrappers, platform-specific implementations, and connection management. These provide the foundation for HTTP client/server functionality.

**🎯 COMPLETED IMPLEMENTATIONS:**
- ✅ **Base socket interfaces** (Socket, ServerSocket) - Fully implemented
- ✅ **Socket factory pattern** - Working with platform registration
- ✅ **Stub platform implementation** - Complete with testing
- ✅ **Comprehensive testing** - All socket functionality validated
- 🟡 **Native/ESP32 platforms** - Design ready, implementation pending

## Design Goals

- **Consistent API**: Uniform interface across all supported platforms
- **Platform Abstraction**: Hide platform-specific implementation details
- **RAII**: Automatic resource management for sockets and connections
- **Type Safety**: Leverage C++ type system for compile-time safety
- **Embedded-Friendly**: Work well on resource-constrained devices
- **Non-Blocking**: Support non-blocking I/O for LED update compatibility

## Core Socket Abstractions

### **1. Base Socket Interface**

```cpp
namespace fl {

/// IP version preference for socket operations
enum class IpVersion {
    IPV4_ONLY,  ///< Use IPv4 only
    IPV6_ONLY,  ///< Use IPv6 only  
    AUTO        ///< Prefer IPv6, fallback to IPv4
};

/// Socket error enumeration
enum class SocketError {
    SUCCESS,
    CONNECTION_FAILED,
    CONNECTION_TIMEOUT,
    CONNECTION_REFUSED,
    NETWORK_UNREACHABLE,
    PERMISSION_DENIED,
    ADDRESS_IN_USE,
    INVALID_ADDRESS,
    SOCKET_ERROR,
    TLS_ERROR,
    PROTOCOL_ERROR,
    UNKNOWN_ERROR
};

/// Socket state enumeration
enum class SocketState {
    CLOSED,
    CONNECTING,
    CONNECTED,
    LISTENING,
    CLOSING,
    ERROR
};

/// Base socket interface - platform-agnostic socket operations
class Socket {
public:
    virtual ~Socket() = default;
    
    /// Connection management
    virtual SocketError connect(const fl::string& host, int port) = 0;
    virtual SocketError connect_async(const fl::string& host, int port) = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    virtual SocketState get_state() const = 0;
    
    /// Data I/O
    virtual fl::size read(fl::span<fl::u8> buffer) = 0;
    virtual fl::size write(fl::span<const fl::u8> data) = 0;
    virtual fl::size available() const = 0;
    virtual void flush() = 0;
    
    /// Non-blocking I/O support
    virtual bool has_data_available() const = 0;
    virtual bool can_write() const = 0;
    virtual void set_non_blocking(bool non_blocking) = 0;
    virtual bool is_non_blocking() const = 0;
    
    /// Socket configuration
    virtual void set_timeout(fl::u32 timeout_ms) = 0;
    virtual fl::u32 get_timeout() const = 0;
    virtual void set_keep_alive(bool enable) = 0;
    virtual void set_nodelay(bool enable) = 0;
    
    /// Connection info
    virtual fl::string remote_address() const = 0;
    virtual int remote_port() const = 0;
    virtual fl::string local_address() const = 0;
    virtual int local_port() const = 0;
    
    /// Error handling
    virtual SocketError get_last_error() const = 0;
    virtual fl::string get_error_message() const = 0;
    
    /// Socket options (advanced)
    virtual bool set_socket_option(int level, int option, const void* value, fl::size value_size) = 0;
    virtual bool get_socket_option(int level, int option, void* value, fl::size* value_size) = 0;
    
    /// Platform-specific handle access (when needed)
    virtual int get_socket_handle() const = 0;
    
protected:
    /// Internal state management
    virtual void set_state(SocketState state) = 0;
    virtual void set_error(SocketError error, const fl::string& message = "") = 0;
};

/// Server socket interface for accepting connections
class ServerSocket {
public:
    virtual ~ServerSocket() = default;
    
    /// Server lifecycle
    virtual SocketError bind(const fl::string& address, int port) = 0;
    virtual SocketError listen(int backlog = 5) = 0;
    virtual void close() = 0;
    virtual bool is_listening() const = 0;
    
    /// Accept connections
    virtual fl::shared_ptr<Socket> accept() = 0;
    virtual fl::vector<fl::shared_ptr<Socket>> accept_multiple(fl::size max_connections = 10) = 0;
    virtual bool has_pending_connections() const = 0;
    
    /// Server configuration
    virtual void set_reuse_address(bool enable) = 0;
    virtual void set_reuse_port(bool enable) = 0;  // Linux/BSD only
    virtual void set_non_blocking(bool non_blocking) = 0;
    
    /// Server info
    virtual fl::string bound_address() const = 0;
    virtual int bound_port() const = 0;
    virtual fl::size max_connections() const = 0;
    virtual fl::size current_connections() const = 0;
    
    /// Error handling
    virtual SocketError get_last_error() const = 0;
    virtual fl::string get_error_message() const = 0;
    
    /// Platform-specific handle access
    virtual int get_socket_handle() const = 0;
    
protected:
    virtual void set_error(SocketError error, const fl::string& message = "") = 0;
};

} // namespace fl
```

### **2. Socket Factory Interface**

```cpp
namespace fl {

/// Socket creation options
struct SocketOptions {
    IpVersion ip_version = IpVersion::AUTO;
    bool enable_keepalive = true;
    bool enable_nodelay = true;
    fl::u32 connect_timeout_ms = 10000;
    fl::u32 read_timeout_ms = 5000;
    fl::u32 write_timeout_ms = 5000;
    fl::size buffer_size = 8192;
    bool enable_reuse_addr = true;
    bool enable_reuse_port = false;
    
    /// Generate hash for caching
    fl::size hash() const {
        fl::size h = 0;
        h ^= fl::hash<int>{}(static_cast<int>(ip_version));
        h ^= fl::hash<bool>{}(enable_keepalive);
        h ^= fl::hash<bool>{}(enable_nodelay);
        h ^= fl::hash<fl::u32>{}(connect_timeout_ms);
        h ^= fl::hash<fl::u32>{}(read_timeout_ms);
        h ^= fl::hash<fl::u32>{}(write_timeout_ms);
        h ^= fl::hash<fl::size>{}(buffer_size);
        return h;
    }
};

/// Socket factory for creating platform-specific socket implementations
class SocketFactory {
public:
    /// Create client socket for outgoing connections
    static fl::shared_ptr<Socket> create_client_socket(const SocketOptions& options = {});
    
    /// Create server socket for accepting connections
    static fl::shared_ptr<ServerSocket> create_server_socket(const SocketOptions& options = {});
    
    /// Check platform capabilities
    static bool supports_ipv6();
    static bool supports_tls();
    static bool supports_non_blocking_connect();
    static bool supports_socket_reuse();
    
    /// Platform-specific factory registration
    static void register_platform_factory(fl::function<fl::shared_ptr<Socket>(const SocketOptions&)> client_factory,
                                          fl::function<fl::shared_ptr<ServerSocket>(const SocketOptions&)> server_factory);
    
private:
    static fl::function<fl::shared_ptr<Socket>(const SocketOptions&)> sClientFactory;
    static fl::function<fl::shared_ptr<ServerSocket>(const SocketOptions&)> sServerFactory;
};

} // namespace fl
```

## Platform Implementations

### **1. Native BSD Socket Implementation**

```cpp
namespace fl {

/// Native BSD socket implementation (Linux/macOS/Windows)
class BsdSocket : public Socket {
public:
    explicit BsdSocket(const SocketOptions& options = {});
    ~BsdSocket() override;
    
    // Socket interface implementation
    SocketError connect(const fl::string& host, int port) override;
    SocketError connect_async(const fl::string& host, int port) override;
    void disconnect() override;
    bool is_connected() const override;
    SocketState get_state() const override;
    
    fl::size read(fl::span<fl::u8> buffer) override;
    fl::size write(fl::span<const fl::u8> data) override;
    fl::size available() const override;
    void flush() override;
    
    bool has_data_available() const override;
    bool can_write() const override;
    void set_non_blocking(bool non_blocking) override;
    bool is_non_blocking() const override;
    
    void set_timeout(fl::u32 timeout_ms) override;
    fl::u32 get_timeout() const override;
    void set_keep_alive(bool enable) override;
    void set_nodelay(bool enable) override;
    
    fl::string remote_address() const override;
    int remote_port() const override;
    fl::string local_address() const override;
    int local_port() const override;
    
    SocketError get_last_error() const override;
    fl::string get_error_message() const override;
    
    bool set_socket_option(int level, int option, const void* value, fl::size value_size) override;
    bool get_socket_option(int level, int option, void* value, fl::size* value_size) override;
    
    int get_socket_handle() const override { return mSocketFd; }
    
protected:
    void set_state(SocketState state) override;
    void set_error(SocketError error, const fl::string& message = "") override;
    
private:
    const SocketOptions mOptions;
    int mSocketFd = -1;
    SocketState mState = SocketState::CLOSED;
    SocketError mLastError = SocketError::SUCCESS;
    fl::string mErrorMessage;
    bool mIsNonBlocking = false;
    
    // Platform-specific helpers
    SocketError create_socket(int domain, int type, int protocol);
    SocketError resolve_address(const fl::string& host, int port, sockaddr_storage& addr, socklen_t& addr_len);
    void configure_socket_options();
    void cleanup();
    
    // Error conversion
    SocketError system_error_to_socket_error(int error_code);
    fl::string system_error_to_string(int error_code);
};

/// Native BSD server socket implementation
class BsdServerSocket : public ServerSocket {
public:
    explicit BsdServerSocket(const SocketOptions& options = {});
    ~BsdServerSocket() override;
    
    // ServerSocket interface implementation
    SocketError bind(const fl::string& address, int port) override;
    SocketError listen(int backlog = 5) override;
    void close() override;
    bool is_listening() const override;
    
    fl::shared_ptr<Socket> accept() override;
    fl::vector<fl::shared_ptr<Socket>> accept_multiple(fl::size max_connections = 10) override;
    bool has_pending_connections() const override;
    
    void set_reuse_address(bool enable) override;
    void set_reuse_port(bool enable) override;
    void set_non_blocking(bool non_blocking) override;
    
    fl::string bound_address() const override;
    int bound_port() const override;
    fl::size max_connections() const override;
    fl::size current_connections() const override;
    
    SocketError get_last_error() const override;
    fl::string get_error_message() const override;
    
    int get_socket_handle() const override { return mSocketFd; }
    
protected:
    void set_error(SocketError error, const fl::string& message = "") override;
    
private:
    const SocketOptions mOptions;
    int mSocketFd = -1;
    bool mIsListening = false;
    fl::string mBoundAddress;
    int mBoundPort = 0;
    int mBacklog = 5;
    SocketError mLastError = SocketError::SUCCESS;
    fl::string mErrorMessage;
    fl::vector<fl::shared_ptr<Socket>> mActiveConnections;
    
    // Platform-specific helpers
    SocketError create_server_socket(const fl::string& address, int port);
    void configure_server_options();
    void cleanup();
    fl::shared_ptr<Socket> create_client_socket_from_fd(int client_fd);
};

} // namespace fl
```

### **2. ESP32/lwIP Implementation**

```cpp
namespace fl {

/// ESP32 lwIP socket implementation
class LwipSocket : public Socket {
public:
    explicit LwipSocket(const SocketOptions& options = {});
    ~LwipSocket() override;
    
    // Socket interface implementation
    SocketError connect(const fl::string& host, int port) override;
    SocketError connect_async(const fl::string& host, int port) override;
    void disconnect() override;
    bool is_connected() const override;
    SocketState get_state() const override;
    
    fl::size read(fl::span<fl::u8> buffer) override;
    fl::size write(fl::span<const fl::u8> data) override;
    fl::size available() const override;
    void flush() override;
    
    bool has_data_available() const override;
    bool can_write() const override;
    void set_non_blocking(bool non_blocking) override;
    bool is_non_blocking() const override;
    
    void set_timeout(fl::u32 timeout_ms) override;
    fl::u32 get_timeout() const override;
    void set_keep_alive(bool enable) override;
    void set_nodelay(bool enable) override;
    
    fl::string remote_address() const override;
    int remote_port() const override;
    fl::string local_address() const override;
    int local_port() const override;
    
    SocketError get_last_error() const override;
    fl::string get_error_message() const override;
    
    bool set_socket_option(int level, int option, const void* value, fl::size value_size) override;
    bool get_socket_option(int level, int option, void* value, fl::size* value_size) override;
    
    int get_socket_handle() const override { return mSocketFd; }
    
protected:
    void set_state(SocketState state) override;
    void set_error(SocketError error, const fl::string& message = "") override;
    
private:
    const SocketOptions mOptions;
    int mSocketFd = -1;
    SocketState mState = SocketState::CLOSED;
    SocketError mLastError = SocketError::SUCCESS;
    fl::string mErrorMessage;
    bool mIsNonBlocking = false;
    
    // ESP32-specific helpers
    SocketError create_lwip_socket();
    void configure_lwip_options();
    SocketError lwip_connect(const fl::string& host, int port, bool async);
    void cleanup();
    
    // WiFi integration
    bool ensure_wifi_connected();
    SocketError wait_for_wifi_connection(fl::u32 timeout_ms);
    
    // Error conversion
    SocketError lwip_error_to_socket_error(int lwip_error);
    fl::string lwip_error_to_string(int lwip_error);
};

/// ESP32 lwIP server socket implementation  
class LwipServerSocket : public ServerSocket {
public:
    explicit LwipServerSocket(const SocketOptions& options = {});
    ~LwipServerSocket() override;
    
    // ServerSocket interface implementation  
    SocketError bind(const fl::string& address, int port) override;
    SocketError listen(int backlog = 5) override;
    void close() override;
    bool is_listening() const override;
    
    fl::shared_ptr<Socket> accept() override;
    fl::vector<fl::shared_ptr<Socket>> accept_multiple(fl::size max_connections = 10) override;
    bool has_pending_connections() const override;
    
    void set_reuse_address(bool enable) override;
    void set_reuse_port(bool enable) override;
    void set_non_blocking(bool non_blocking) override;
    
    fl::string bound_address() const override;
    int bound_port() const override;
    fl::size max_connections() const override;
    fl::size current_connections() const override;
    
    SocketError get_last_error() const override;
    fl::string get_error_message() const override;
    
    int get_socket_handle() const override { return mSocketFd; }
    
protected:
    void set_error(SocketError error, const fl::string& message = "") override;
    
private:
    const SocketOptions mOptions;
    int mSocketFd = -1;
    bool mIsListening = false;
    fl::string mBoundAddress;
    int mBoundPort = 0;
    int mBacklog = 5;
    SocketError mLastError = SocketError::SUCCESS;
    fl::string mErrorMessage;
    fl::vector<fl::shared_ptr<Socket>> mActiveConnections;
    
    // ESP32-specific helpers
    SocketError create_lwip_server_socket();
    void configure_lwip_server_options();
    void cleanup();
    fl::shared_ptr<Socket> create_client_socket_from_lwip_fd(int client_fd);
    
    // Memory management for ESP32
    void manage_connection_memory();
    bool can_accept_new_connection();
};

} // namespace fl
```

### **3. WebAssembly/Emscripten Implementation**

```cpp
namespace fl {

/// WebAssembly socket implementation using WebSocket
class WasmSocket : public Socket {
public:
    explicit WasmSocket(const SocketOptions& options = {});
    ~WasmSocket() override;
    
    // Socket interface implementation
    SocketError connect(const fl::string& host, int port) override;
    SocketError connect_async(const fl::string& host, int port) override;
    void disconnect() override;
    bool is_connected() const override;
    SocketState get_state() const override;
    
    fl::size read(fl::span<fl::u8> buffer) override;
    fl::size write(fl::span<const fl::u8> data) override;
    fl::size available() const override;
    void flush() override;
    
    bool has_data_available() const override;
    bool can_write() const override;
    void set_non_blocking(bool non_blocking) override;
    bool is_non_blocking() const override;
    
    void set_timeout(fl::u32 timeout_ms) override;
    fl::u32 get_timeout() const override;
    void set_keep_alive(bool enable) override;
    void set_nodelay(bool enable) override;
    
    fl::string remote_address() const override;
    int remote_port() const override;
    fl::string local_address() const override;
    int local_port() const override;
    
    SocketError get_last_error() const override;
    fl::string get_error_message() const override;
    
    bool set_socket_option(int level, int option, const void* value, fl::size value_size) override;
    bool get_socket_option(int level, int option, void* value, fl::size* value_size) override;
    
    int get_socket_handle() const override { return mWebSocketId; }
    
protected:
    void set_state(SocketState state) override;
    void set_error(SocketError error, const fl::string& message = "") override;
    
private:
    const SocketOptions mOptions;
    int mWebSocketId = -1;
    SocketState mState = SocketState::CLOSED;
    SocketError mLastError = SocketError::SUCCESS;
    fl::string mErrorMessage;
    fl::string mRemoteHost;
    int mRemotePort = 0;
    
    // WebSocket message queue
    fl::deque<fl::u8> mReceiveBuffer;
    fl::deque<fl::u8> mSendBuffer;
    
    // WebSocket integration
    SocketError create_websocket_connection(const fl::string& host, int port);
    void setup_websocket_callbacks();
    void process_websocket_messages();
    void cleanup();
    
    // Message handling
    void on_websocket_open();
    void on_websocket_message(const fl::u8* data, fl::size length);
    void on_websocket_error(const fl::string& error);
    void on_websocket_close();
    
    // HTTP-over-WebSocket protocol implementation
    fl::string build_websocket_url(const fl::string& host, int port);
    void send_websocket_frame(fl::span<const fl::u8> data);
};

/// WebAssembly server socket implementation (limited functionality)
class WasmServerSocket : public ServerSocket {
public:
    explicit WasmServerSocket(const SocketOptions& options = {});
    ~WasmServerSocket() override;
    
    // Limited server functionality in WebAssembly
    SocketError bind(const fl::string& address, int port) override;
    SocketError listen(int backlog = 5) override;
    void close() override;
    bool is_listening() const override;
    
    fl::shared_ptr<Socket> accept() override;
    fl::vector<fl::shared_ptr<Socket>> accept_multiple(fl::size max_connections = 10) override;
    bool has_pending_connections() const override;
    
    void set_reuse_address(bool enable) override;
    void set_reuse_port(bool enable) override;
    void set_non_blocking(bool non_blocking) override;
    
    fl::string bound_address() const override;
    int bound_port() const override;
    fl::size max_connections() const override;
    fl::size current_connections() const override;
    
    SocketError get_last_error() const override;
    fl::string get_error_message() const override;
    
    int get_socket_handle() const override { return -1; }
    
protected:
    void set_error(SocketError error, const fl::string& message = "") override;
    
private:
    const SocketOptions mOptions;
    SocketError mLastError = SocketError::SUCCESS;
    fl::string mErrorMessage;
    
    // WebAssembly server limitations
    void log_server_limitation(const fl::string& operation);
};

} // namespace fl
```

## Connection Management

### **Connection Pool**

```cpp
namespace fl {

/// Connection pool for reusing socket connections
class ConnectionPool {
public:
    /// Connection pool configuration
    struct Config {
        fl::size max_connections_per_host = 5;
        fl::size max_total_connections = 50;
        fl::u32 idle_timeout_ms = 30000;
        fl::u32 max_lifetime_ms = 300000;  // 5 minutes
        bool enable_keep_alive = true;
        fl::u32 keep_alive_interval_ms = 60000;
    };
    
    explicit ConnectionPool(const Config& config = {});
    ~ConnectionPool();
    
    /// Get connection for host/port (reuse or create new)
    fl::shared_ptr<Socket> get_connection(const fl::string& host, int port, const SocketOptions& options = {});
    
    /// Return connection to pool (for reuse)
    void return_connection(fl::shared_ptr<Socket> socket, const fl::string& host, int port);
    
    /// Force close all connections to a host
    void close_connections_to_host(const fl::string& host, int port);
    
    /// Close all idle connections
    void cleanup_idle_connections();
    
    /// Pool statistics
    struct Stats {
        fl::size total_connections;
        fl::size active_connections;
        fl::size idle_connections;
        fl::size connections_created;
        fl::size connections_reused;
        fl::size connections_closed;
    };
    Stats get_stats() const;
    
    /// Configure pool limits
    void set_max_connections_per_host(fl::size max_connections);
    void set_max_total_connections(fl::size max_connections);
    void set_idle_timeout(fl::u32 timeout_ms);
    
private:
    const Config mConfig;
    
    /// Connection entry in pool
    struct PoolEntry {
        fl::shared_ptr<Socket> socket;
        fl::u32 last_used_time;
        fl::u32 created_time;
        bool is_active;
        fl::string host;
        int port;
    };
    
    fl::vector<PoolEntry> mConnections;
    mutable fl::mutex mMutex;
    
    // Statistics
    mutable fl::mutex mStatsMutex;
    Stats mStats;
    
    // Internal management
    PoolEntry* find_available_connection(const fl::string& host, int port);
    bool can_create_new_connection(const fl::string& host, int port);
    void cleanup_expired_connections();
    void remove_connection(fl::size index);
    fl::string make_connection_key(const fl::string& host, int port);
    
    // Background cleanup task
    void start_cleanup_task();
    void stop_cleanup_task();
    bool mCleanupTaskRunning = false;
};

/// Global connection pool instance
ConnectionPool& get_global_connection_pool();

} // namespace fl
```

### **Network Event System**

```cpp
namespace fl {

/// Network event types
enum class NetworkEventType {
    CONNECTION_ESTABLISHED,
    CONNECTION_LOST,
    DATA_RECEIVED,
    DATA_SENT,
    ERROR_OCCURRED
};

/// Network event data
struct NetworkEvent {
    NetworkEventType type;
    fl::shared_ptr<Socket> socket;
    fl::string host;
    int port;
    fl::size bytes_transferred;
    SocketError error;
    fl::string message;
    fl::u32 timestamp;
};

/// Network event listener interface
class NetworkEventListener {
public:
    virtual ~NetworkEventListener() = default;
    virtual void on_network_event(const NetworkEvent& event) = 0;
};

/// Network event manager for monitoring socket events
class NetworkEventManager {
public:
    static NetworkEventManager& instance();
    
    /// Event listener management
    void add_listener(fl::shared_ptr<NetworkEventListener> listener);
    void remove_listener(fl::shared_ptr<NetworkEventListener> listener);
    
    /// Event dispatching
    void dispatch_event(const NetworkEvent& event);
    void dispatch_connection_established(fl::shared_ptr<Socket> socket, const fl::string& host, int port);
    void dispatch_connection_lost(fl::shared_ptr<Socket> socket, const fl::string& host, int port);
    void dispatch_data_received(fl::shared_ptr<Socket> socket, fl::size bytes);
    void dispatch_data_sent(fl::shared_ptr<Socket> socket, fl::size bytes);
    void dispatch_error(fl::shared_ptr<Socket> socket, SocketError error, const fl::string& message);
    
    /// Event statistics
    struct EventStats {
        fl::size total_events;
        fl::size connection_events;
        fl::size data_events;
        fl::size error_events;
        fl::u32 last_event_time;
    };
    EventStats get_stats() const;
    
    /// Enable/disable event processing
    void set_enabled(bool enabled);
    bool is_enabled() const;
    
private:
    NetworkEventManager() = default;
    
    fl::vector<fl::shared_ptr<NetworkEventListener>> mListeners;
    mutable fl::mutex mListenersMutex;
    
    EventStats mStats;
    mutable fl::mutex mStatsMutex;
    bool mEnabled = true;
    
    void update_stats(NetworkEventType type);
};

} // namespace fl
```

## Platform-Specific Initialization

### **Factory Registration**

```cpp
namespace fl {

/// Platform-specific socket factory registration
class SocketPlatform {
public:
    /// Initialize platform-specific socket implementation
    static void initialize();
    
    /// Cleanup platform resources
    static void cleanup();
    
    /// Get platform capabilities
    static bool supports_server_sockets();
    static bool supports_non_blocking_io();
    static bool supports_ipv6();
    static bool supports_unix_sockets();
    
    /// Platform identification
    static fl::string get_platform_name();
    static fl::string get_networking_stack();
    
private:
    static bool sInitialized;
    
    // Platform-specific initialization
    static void initialize_native_platform();
    static void initialize_esp32_platform();
    static void initialize_wasm_platform();
    static void initialize_stub_platform();
    
    // Factory registration helpers
    static void register_native_factories();
    static void register_esp32_factories();
    static void register_wasm_factories();
    static void register_stub_factories();
};

/// Automatic platform initialization
class SocketPlatformInitializer {
public:
    SocketPlatformInitializer() {
        SocketPlatform::initialize();
    }
    
    ~SocketPlatformInitializer() {
        SocketPlatform::cleanup();
    }
};

// Global initializer (automatic platform setup)
static SocketPlatformInitializer g_socket_platform_init;

} // namespace fl
```

## Testing Support

### **Mock Socket Implementation**

```cpp
namespace fl {

/// Mock socket for testing (no actual network I/O)
class MockSocket : public Socket {
public:
    explicit MockSocket(const SocketOptions& options = {});
    ~MockSocket() override = default;
    
    // Mock implementation of Socket interface
    SocketError connect(const fl::string& host, int port) override;
    SocketError connect_async(const fl::string& host, int port) override;
    void disconnect() override;
    bool is_connected() const override;
    SocketState get_state() const override;
    
    fl::size read(fl::span<fl::u8> buffer) override;
    fl::size write(fl::span<const fl::u8> data) override;
    fl::size available() const override;
    void flush() override;
    
    bool has_data_available() const override;
    bool can_write() const override;
    void set_non_blocking(bool non_blocking) override;
    bool is_non_blocking() const override;
    
    void set_timeout(fl::u32 timeout_ms) override;
    fl::u32 get_timeout() const override;
    void set_keep_alive(bool enable) override;
    void set_nodelay(bool enable) override;
    
    fl::string remote_address() const override;
    int remote_port() const override;
    fl::string local_address() const override;
    int local_port() const override;
    
    SocketError get_last_error() const override;
    fl::string get_error_message() const override;
    
    bool set_socket_option(int level, int option, const void* value, fl::size value_size) override;
    bool get_socket_option(int level, int option, void* value, fl::size* value_size) override;
    
    int get_socket_handle() const override;
    
    /// Test control methods
    void set_mock_response(fl::span<const fl::u8> data);
    void set_mock_error(SocketError error, const fl::string& message = "");
    void set_mock_connection_delay(fl::u32 delay_ms);
    void simulate_connection_loss();
    void simulate_slow_network(fl::u32 bytes_per_second);
    
    /// Test inspection methods
    fl::vector<fl::u8> get_sent_data() const;
    fl::size get_bytes_sent() const;
    fl::size get_bytes_received() const;
    fl::size get_connection_attempts() const;
    
protected:
    void set_state(SocketState state) override;
    void set_error(SocketError error, const fl::string& message = "") override;
    
private:
    const SocketOptions mOptions;
    SocketState mState = SocketState::CLOSED;
    SocketError mLastError = SocketError::SUCCESS;
    fl::string mErrorMessage;
    fl::string mRemoteHost;
    int mRemotePort = 0;
    
    // Mock data
    fl::vector<fl::u8> mMockResponse;
    fl::vector<fl::u8> mSentData;
    fl::size mResponseOffset = 0;
    fl::u32 mConnectionDelay = 0;
    fl::u32 mBytesPerSecond = 0;  // 0 = unlimited
    
    // Test statistics
    fl::size mConnectionAttempts = 0;
    fl::size mBytesSent = 0;
    fl::size mBytesReceived = 0;
    
    // Mock behavior helpers
    void simulate_network_delay();
    fl::size calculate_transfer_rate(fl::size requested_bytes);
};

/// Mock server socket for testing
class MockServerSocket : public ServerSocket {
public:
    explicit MockServerSocket(const SocketOptions& options = {});
    ~MockServerSocket() override = default;
    
    // Mock implementation of ServerSocket interface
    SocketError bind(const fl::string& address, int port) override;
    SocketError listen(int backlog = 5) override;
    void close() override;
    bool is_listening() const override;
    
    fl::shared_ptr<Socket> accept() override;
    fl::vector<fl::shared_ptr<Socket>> accept_multiple(fl::size max_connections = 10) override;
    bool has_pending_connections() const override;
    
    void set_reuse_address(bool enable) override;
    void set_reuse_port(bool enable) override;
    void set_non_blocking(bool non_blocking) override;
    
    fl::string bound_address() const override;
    int bound_port() const override;
    fl::size max_connections() const override;
    fl::size current_connections() const override;
    
    SocketError get_last_error() const override;
    fl::string get_error_message() const override;
    
    int get_socket_handle() const override;
    
    /// Test control methods
    void add_pending_connection(fl::shared_ptr<MockSocket> client_socket);
    void set_mock_error(SocketError error, const fl::string& message = "");
    void simulate_connection_limit();
    
    /// Test inspection methods
    fl::size get_total_connections_accepted() const;
    fl::size get_pending_connection_count() const;
    
protected:
    void set_error(SocketError error, const fl::string& message = "") override;
    
private:
    const SocketOptions mOptions;
    bool mIsListening = false;
    fl::string mBoundAddress = "127.0.0.1";
    int mBoundPort = 0;
    int mBacklog = 5;
    SocketError mLastError = SocketError::SUCCESS;
    fl::string mErrorMessage;
    
    fl::vector<fl::shared_ptr<MockSocket>> mPendingConnections;
    fl::size mTotalConnectionsAccepted = 0;
    bool mSimulateConnectionLimit = false;
};

/// Register mock socket factories for testing
void register_mock_socket_factories();

} // namespace fl
```

## Implementation Files

The low-level networking implementation will be organized as follows:

```
src/fl/networking/
├── socket.h                 # Base socket interfaces
├── socket_factory.h         # Socket factory and registration
├── connection_pool.h        # Connection pooling
├── network_events.h         # Event system
├── platform/
│   ├── bsd_socket.h/.cpp   # Native BSD socket implementation
│   ├── lwip_socket.h/.cpp  # ESP32 lwIP implementation
│   ├── wasm_socket.h/.cpp  # WebAssembly implementation
│   └── mock_socket.h/.cpp  # Testing mock implementation
└── detail/
    ├── socket_platform.h/.cpp  # Platform initialization
    └── network_utils.h/.cpp     # Common utilities
```

## Integration Points

### **With HTTP Client/Server**

The low-level socket abstractions provide the foundation for HTTP functionality:

```cpp
// HTTP client uses socket factory
auto socket = SocketFactory::create_client_socket(socket_options);
auto transport = fl::make_shared<TcpTransport>(socket);
auto http_client = HttpClient::create_with_transport(transport);

// HTTP server uses server socket factory  
auto server_socket = SocketFactory::create_server_socket(socket_options);
auto server_transport = fl::make_shared<TcpServerTransport>(server_socket);
auto http_server = HttpServer::create_with_transport(server_transport);
```

### **With FastLED EngineEvents**

Socket operations integrate with FastLED's event system for non-blocking operation:

```cpp
class NetworkEventPump : public EngineEvents::Listener {
public:
    void onEndFrame() override {
        // Process network events during FastLED.show()
        process_socket_events();
        cleanup_idle_connections();
        dispatch_network_events();
    }
    
private:
    void process_socket_events() {
        // Non-blocking socket I/O processing
        for (auto& socket : active_sockets) {
            if (socket->has_data_available()) {
                process_socket_data(socket);
            }
        }
    }
};
```

The low-level networking design provides a solid, platform-agnostic foundation for all FastLED networking functionality while maintaining compatibility with embedded systems and FastLED's non-blocking operation requirements. 
