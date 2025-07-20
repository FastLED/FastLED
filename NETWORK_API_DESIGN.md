# FastLED Networking API Design

## ⚠️ IMPLEMENTATION STATUS

**✅ DESIGN COMPLETE - Ready for Implementation**

| Component | Status | Notes |
|-----------|--------|-------|
| **fl::future<T>** | ✅ **DESIGNED** | Design complete in [CONCURRENCY_FUTURE_API_DESIGN.md](./CONCURRENCY_FUTURE_API_DESIGN.md) |
| **HttpClient** | ✅ **DESIGNED** | ✅ Complete API design with transport abstraction |
| **Response** | ✅ **DESIGNED** | ✅ Complete design uses `fl::deque` for memory efficiency |
| **Request** | ✅ **DESIGNED** | ✅ Internal request object design complete |
| **Socket** | ✅ **DESIGNED** | ✅ BSD-style socket wrapper design complete |
| **NetworkEventPump** | ✅ **DESIGNED** | ✅ EngineEvents integration design complete |
| **Transport System** | ✅ **DESIGNED** | ✅ IPv4/IPv6, TLS verification, connection pooling |
| **TLS Security** | ✅ **DESIGNED** | ✅ Certificate management, verification policies |
| **Memory Management** | ✅ **DESIGNED** | ✅ PSRAM integration, response chunking |

**✅ INFRASTRUCTURE READY:**
- `fl::deque` - ✅ **Implemented** - Memory-efficient response storage
- `fl::string` - ✅ **Implemented** - Copy-on-write semantics  
- `fl::span` - ✅ **Implemented** - Safe data access
- `fl::vector`, `fl::shared_ptr`, `fl::mutex` - ✅ **Implemented**

**🎯 NEXT STEPS:**
1. Implement `fl::future<T>` from the concurrency design
2. Create the `Response` class with `fl::deque` storage
3. Implement basic HTTP client with future integration
4. Add platform-specific socket implementations

**📝 RECENT UPDATES:**
- ✅ **Updated Response class design** to use `fl::deque<fl::u8, fl::allocator_psram<fl::u8>>` instead of `fl::vector<fl::u8>` for memory-efficient response storage
- ✅ **Added PSRAM allocator integration** using `fl::allocator_psram<fl::u8>` to utilize external RAM when available
- ✅ **Added comprehensive API** for getting data out of PSRAM-backed deque in standard, friendly ways:
  - Iterator-based access (`for (const fl::u8& byte : response)`)
  - Chunk processing (`process_chunks()`)
  - Text access with copy-on-write (`text()`)
  - Contiguous buffer access when needed (`to_vector()`, `copy_to_buffer()`)
  - Direct deque access for advanced users (`body_deque()`)
- ✅ **Added memory efficiency analysis** comparing `fl::vector` vs `fl::deque+PSRAM` for HTTP responses
- ✅ **Updated internal request management** to use PSRAM-backed `fl::deque` for response buffers
- ✅ **Added real-world embedded examples** showing PSRAM benefits on ESP32S3-class devices (8MB+ external RAM)

## Overview

This document outlines the design for a FastLED networking API that provides BSD-style socket functionality under the `fl::` namespace. The API is designed to work across multiple platforms including native systems, ESP32 (via lwIP), and WebAssembly/Emscripten (via WebSocket/WebRTC).

## Design Goals

- **Consistent API**: Uniform interface across all supported platforms
- **FastLED Style**: Follow FastLED coding conventions and patterns
- **Platform Abstraction**: Hide platform-specific implementation details
- **Type Safety**: Leverage C++ type system for compile-time safety
- **RAII**: Automatic resource management for sockets and connections
- **Header-Only**: Minimize build complexity where possible
- **Embedded-Friendly**: Work well on resource-constrained devices
- **Memory Efficient**: Use `fl::deque` to avoid response memory fragmentation

# HIGH-LEVEL NETWORKING API

## Overview

The high-level networking API provides HTTP client functionality with both async callback and future-based interfaces. It's designed to be non-blocking and integrate seamlessly with FastLED's event system.

## Core Components

### 1. Transport Abstraction (IPv4/IPv6 and Connection Management)

The FastLED HTTP client uses a pluggable transport system inspired by Python's httpx library, allowing fine-grained control over networking behavior including IPv4/IPv6 preferences, connection pooling, and protocol options.

```cpp
namespace fl {

/// Certificate and hostname verification policy
enum class TransportVerify {
    AUTO,        ///< Smart verification (default) - verify for HTTPS, skip for HTTP/localhost
    NO,          ///< Disable all verification (development only)
    YES          ///< Force verification for all connections (maximum security)
};

/// Network protocol preferences
enum class IpVersion {
    AUTO,        ///< Prefer IPv6, fallback to IPv4 (default)
    IPV4_ONLY,   ///< Use IPv4 only
    IPV6_ONLY,   ///< Use IPv6 only
    IPV4_FIRST,  ///< Try IPv4 first, fallback to IPv6
    IPV6_FIRST   ///< Try IPv6 first, fallback to IPv4 (same as AUTO)
};

/// HTTP version preferences
enum class HttpVersion {
    AUTO,        ///< Negotiate best available (HTTP/2 -> HTTP/1.1)
    HTTP_1_0,    ///< Force HTTP/1.0
    HTTP_1_1,    ///< Force HTTP/1.1 (default for embedded)
    HTTP_2       ///< Force HTTP/2 (if available)
};

/// Connection limits and pooling configuration
struct ConnectionLimits {
    fl::size max_keepalive_connections = 10;    ///< Maximum persistent connections
    fl::size max_connections_per_host = 5;      ///< Max connections per hostname
    fl::u32 keepalive_timeout_ms = 60000;       ///< Keep connection alive for 60s
    fl::u32 connection_timeout_ms = 10000;      ///< Connection establishment timeout
    fl::u32 read_timeout_ms = 30000;            ///< Read timeout for responses
    fl::u32 write_timeout_ms = 30000;           ///< Write timeout for requests
};

/// Base transport interface for network connections (immutable)
class Transport {
public:
    virtual ~Transport() = default;
    
    /// Connect to a remote host
    /// @param hostname the hostname or IP address to connect to
    /// @param port the port number
    /// @param is_tls whether this connection should use TLS
    /// @returns unique socket connection or nullptr on failure
    virtual fl::unique_ptr<Socket> connect(const char* hostname, int port, bool is_tls) const = 0;
    
    /// Get transport statistics
    virtual fl::size get_active_connections() const = 0;
    virtual fl::size get_total_connections_created() const = 0;
    
    /// Close all pooled connections
    virtual void close_all_connections() const = 0;
    
    /// Check if transport supports the given protocol
    virtual bool supports_protocol(const char* scheme) const = 0; // "http", "https"
    
    /// Get transport configuration hash for interning
    virtual fl::size get_config_hash() const = 0;
    
    /// Compare transport configurations for equality
    virtual bool equals(const Transport& other) const = 0;
    
protected:
    // Platform-specific socket creation
    virtual fl::unique_ptr<Socket> create_socket(IpVersion ip_version) const = 0;
    virtual bool resolve_hostname(const char* hostname, IpVersion ip_version, 
                                  fl::string& resolved_ip) const = 0;
};

/// Transport factory with weak pointer interning for efficient sharing
class TransportFactory {
public:
    /// Get singleton instance
    static TransportFactory& instance() {
        static TransportFactory factory;
        return factory;
    }
    
    /// Create or get shared TCP transport with specific options
    /// @note Uses weak pointer interning - identical configurations share the same transport
    fl::shared_ptr<const TcpTransport> create_tcp_transport(const TcpTransport::Options& options = {}) {
        return get_or_create_transport<TcpTransport>(options);
    }
    
    /// Create or get shared local transport
    fl::shared_ptr<const LocalTransport> create_local_transport() {
        return get_or_create_transport<LocalTransport>();
    }
    
    /// Create or get shared production transport  
    fl::shared_ptr<const ProductionTransport> create_production_transport() {
        return get_or_create_transport<ProductionTransport>();
    }
    
    /// Create or get shared development transport
    fl::shared_ptr<const DevelopmentTransport> create_development_transport() {
        return get_or_create_transport<DevelopmentTransport>();
    }
    
    /// Factory methods for common transport configurations
    
    /// Get IPv4-only transport (shared instance)
    fl::shared_ptr<const TcpTransport> create_ipv4_only() {
        TcpTransport::Options opts;
        opts.ip_version = IpVersion::IPV4_ONLY;
        opts.verify = TransportVerify::AUTO;
        return create_tcp_transport(opts);
    }
    
    /// Get IPv6-only transport (shared instance)
    fl::shared_ptr<const TcpTransport> create_ipv6_only() {
        TcpTransport::Options opts;
        opts.ip_version = IpVersion::IPV6_ONLY;
        opts.verify = TransportVerify::AUTO;
        return create_tcp_transport(opts);
    }
    
    /// Get dual-stack transport (shared instance)
    fl::shared_ptr<const TcpTransport> create_dual_stack() {
        TcpTransport::Options opts;
        opts.ip_version = IpVersion::AUTO;
        opts.verify = TransportVerify::AUTO;
        return create_tcp_transport(opts);
    }
    
    /// Get unverified transport for development (shared instance)
    fl::shared_ptr<const TcpTransport> create_unverified() {
        TcpTransport::Options opts;
        opts.verify = TransportVerify::NO;
        opts.allow_self_signed = true;
        return create_tcp_transport(opts);
    }
    
    /// Get strict verification transport (shared instance)
    fl::shared_ptr<const TcpTransport> create_strict_verification() {
        TcpTransport::Options opts;
        opts.verify = TransportVerify::YES;
        opts.verify_hostname = true;
        opts.allow_self_signed = false;
        return create_tcp_transport(opts);
    }
    
    /// Clear expired weak references (called periodically)
    void cleanup_expired_references() {
        fl::lock_guard<fl::mutex> lock(mMutex);
        
        for (auto it = mTransportCache.begin(); it != mTransportCache.end();) {
            if (it->second.expired()) {
                it = mTransportCache.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    /// Get cache statistics
    struct CacheStats {
        fl::size active_transports;
        fl::size cached_configs;
        fl::size cache_hits;
        fl::size cache_misses;
    };
    
    CacheStats get_cache_stats() const {
        fl::lock_guard<fl::mutex> lock(mMutex);
        
        fl::size active = 0;
        for (const auto& pair : mTransportCache) {
            if (!pair.second.expired()) {
                active++;
            }
        }
        
        return CacheStats{
            .active_transports = active,
            .cached_configs = mTransportCache.size(),
            .cache_hits = mCacheHits,
            .cache_misses = mCacheMisses
        };
    }

private:
    /// Generic transport creation with interning
    template<typename TransportType, typename... Args>
    fl::shared_ptr<const TransportType> get_or_create_transport(Args&&... args) {
        // Create temporary transport to get config hash
        TransportType temp_transport(fl::forward<Args>(args)...);
        fl::size config_hash = temp_transport.get_config_hash();
        
        fl::lock_guard<fl::mutex> lock(mMutex);
        
        // Check if we already have this configuration
        auto it = mTransportCache.find(config_hash);
        if (it != mTransportCache.end()) {
            if (auto shared = it->second.lock()) {
                // Found existing transport with same config
                mCacheHits++;
                return fl::static_pointer_cast<const TransportType>(shared);
            } else {
                // Weak pointer expired, remove it
                mTransportCache.erase(it);
            }
        }
        
        // Create new transport instance
        auto transport = fl::make_shared<const TransportType>(fl::forward<Args>(args)...);
        
        // Store weak reference for future interning
        mTransportCache[config_hash] = transport;
        mCacheMisses++;
        
        return transport;
    }
    
    // Cache for weak pointer interning
    mutable fl::mutex mMutex;
    fl::unordered_map<fl::size, fl::weak_ptr<const Transport>> mTransportCache;
    mutable fl::size mCacheHits = 0;
    mutable fl::size mCacheMisses = 0;
};

/// Standard TCP transport with IPv4/IPv6 support and smart verification (immutable)
class TcpTransport : public Transport {
public:
    /// Transport configuration options (immutable after construction)
    struct Options {
        IpVersion ip_version = IpVersion::AUTO;      ///< IPv4/IPv6 preference
        HttpVersion http_version = HttpVersion::HTTP_1_1; ///< HTTP version
        TransportVerify verify = TransportVerify::AUTO; ///< Certificate verification policy (default: AUTO)
        ConnectionLimits limits;                     ///< Connection pooling limits
        fl::optional<fl::string> local_address;      ///< Bind to specific local IP
        fl::optional<int> local_port;                ///< Bind to specific local port
        bool enable_keepalive = true;                ///< Enable HTTP keep-alive
        bool enable_nodelay = true;                  ///< Disable Nagle's algorithm (TCP_NODELAY)
        fl::size socket_buffer_size = 8192;          ///< Socket send/receive buffer size
        
        // Advanced socket options
        bool enable_so_reuseaddr = true;             ///< Allow address reuse
        fl::optional<fl::u32> so_rcvtimeo_ms;        ///< Socket receive timeout
        fl::optional<fl::u32> so_sndtimeo_ms;        ///< Socket send timeout
        
        // TLS-specific options (when verify != NO)
        bool verify_hostname = true;                 ///< Verify hostname matches certificate
        bool allow_self_signed = false;             ///< Allow self-signed certificates
        fl::string ca_bundle_path;                   ///< Path to CA certificate bundle
        fl::vector<fl::string> pinned_certificates;  ///< SHA256 fingerprints of pinned certs
        
        /// Generate hash for configuration comparison
        fl::size hash() const {
            fl::size h = 0;
            h ^= fl::hash<int>{}(static_cast<int>(ip_version));
            h ^= fl::hash<int>{}(static_cast<int>(http_version));
            h ^= fl::hash<int>{}(static_cast<int>(verify));
            h ^= fl::hash<fl::size>{}(limits.max_keepalive_connections);
            h ^= fl::hash<fl::u32>{}(limits.connection_timeout_ms);
            h ^= fl::hash<bool>{}(enable_keepalive);
            h ^= fl::hash<bool>{}(enable_nodelay);
            h ^= fl::hash<fl::size>{}(socket_buffer_size);
            h ^= fl::hash<bool>{}(verify_hostname);
            h ^= fl::hash<bool>{}(allow_self_signed);
            if (local_address) {
                h ^= fl::hash<fl::string>{}(*local_address);
            }
            if (local_port) {
                h ^= fl::hash<int>{}(*local_port);
            }
            h ^= fl::hash<fl::string>{}(ca_bundle_path);
            for (const auto& cert : pinned_certificates) {
                h ^= fl::hash<fl::string>{}(cert);
            }
            return h;
        }
        
        /// Compare options for equality
        bool operator==(const Options& other) const {
            return ip_version == other.ip_version &&
                   http_version == other.http_version &&
                   verify == other.verify &&
                   limits.max_keepalive_connections == other.limits.max_keepalive_connections &&
                   limits.connection_timeout_ms == other.limits.connection_timeout_ms &&
                   enable_keepalive == other.enable_keepalive &&
                   enable_nodelay == other.enable_nodelay &&
                   socket_buffer_size == other.socket_buffer_size &&
                   verify_hostname == other.verify_hostname &&
                   allow_self_signed == other.allow_self_signed &&
                   local_address == other.local_address &&
                   local_port == other.local_port &&
                   ca_bundle_path == other.ca_bundle_path &&
                   pinned_certificates == other.pinned_certificates;
        }
    };
    
    /// Create transport with specific options (immutable after construction)
    explicit TcpTransport(const Options& options = {}) : mOptions(options) {
        // Initialize connection pool and other resources
        initialize_connection_pool();
    }
    
    /// Destructor
    ~TcpTransport() override {
        // Cleanup is handled automatically by connection pool RAII
    }
    
    // Transport interface implementation (all const methods)
    fl::unique_ptr<Socket> connect(const char* hostname, int port, bool is_tls) const override;
    fl::size get_active_connections() const override;
    fl::size get_total_connections_created() const override;
    void close_all_connections() const override;
    bool supports_protocol(const char* scheme) const override;
    
    /// Get configuration hash for interning
    fl::size get_config_hash() const override {
        return mOptions.hash();
    }
    
    /// Compare transport configurations for equality
    bool equals(const Transport& other) const override {
        if (const auto* tcp_other = dynamic_cast<const TcpTransport*>(&other)) {
            return mOptions == tcp_other->mOptions;
        }
        return false;
    }
    
    /// Get current transport options (immutable)
    const Options& get_options() const { return mOptions; }
    
    /// Verification decision logic for TransportVerify::AUTO
    /// @param hostname the hostname being connected to
    /// @param is_tls whether this is a TLS connection
    /// @returns true if verification should be performed
    bool should_verify_connection(const char* hostname, bool is_tls) const {
        switch (mOptions.verify) {
            case TransportVerify::NO:
                return false;
                
            case TransportVerify::YES:
                return true;
                
            case TransportVerify::AUTO:
            default:
                // Smart verification logic
                if (!is_tls) {
                    // HTTP connections - no verification needed
                    return false;
                }
                
                // HTTPS connections - check if localhost/private
                fl::string host(hostname);
                if (is_localhost_or_private(host)) {
                    // Local/private HTTPS - skip verification for development
                    return false;
                }
                
                // Public HTTPS - always verify
                return true;
        }
    }
    
    /// Connection pool statistics
    struct PoolStats {
        fl::size active_connections;
        fl::size pooled_connections;
        fl::size total_created;
        fl::size total_reused;
        fl::size total_closed;
        fl::u32 oldest_connection_age_ms;
    };
    PoolStats get_pool_stats() const;

protected:
    fl::unique_ptr<Socket> create_socket(IpVersion ip_version) const override;
    bool resolve_hostname(const char* hostname, IpVersion ip_version, 
                          fl::string& resolved_ip) const override;

private:
    const Options mOptions;  ///< Immutable configuration
    
    // Connection pool management (mutable for const interface)
    mutable fl::shared_ptr<ConnectionPool> mConnectionPool;
    
    /// Check if hostname is localhost or private network
    bool is_localhost_or_private(const fl::string& hostname) const {
        return hostname == "localhost" || 
               hostname == "127.0.0.1" || 
               hostname == "::1" ||
               hostname.starts_with("127.") ||
               hostname.starts_with("192.168.") ||
               hostname.starts_with("10.") ||
               hostname.starts_with("172.16.") ||
               hostname.ends_with(".local");
    }
    
    /// Initialize connection pool (called once during construction)
    void initialize_connection_pool();
    
    // Connection pool operations (const interface)
    fl::unique_ptr<Socket> get_pooled_connection(const char* hostname, int port, bool is_tls) const;
    void return_connection_to_pool(fl::unique_ptr<Socket> socket, const char* hostname, 
                                   int port, bool is_tls) const;
    void cleanup_expired_connections() const;
    
    // Socket configuration (const interface)
    void configure_socket(Socket* socket) const;
    void set_socket_timeouts(Socket* socket) const;
    void set_socket_options(Socket* socket) const;
    void configure_tls_verification(Socket* socket, const char* hostname, bool is_tls) const;
};

/// Specialized transports for specific use cases

/// Local-only transport (localhost, 127.0.0.1, ::1) - no verification needed
class LocalTransport : public TcpTransport {
public:
    LocalTransport() : TcpTransport(create_local_options()) {}
    
    bool supports_protocol(const char* scheme) const override {
        // Only allow connections to localhost
        return true; // Will be validated in connect()
    }
    
    fl::unique_ptr<Socket> connect(const char* hostname, int port, bool is_tls) override {
        // Validate hostname is localhost-only
        if (!is_localhost(hostname)) {
            FL_WARN("LocalTransport: Rejecting non-localhost connection to " << hostname);
            return nullptr;
        }
        return TcpTransport::connect(hostname, port, is_tls);
    }
    
private:
    static Options create_local_options() {
        Options opts;
        opts.ip_version = IpVersion::AUTO;
        opts.verify = TransportVerify::NO;  // Localhost doesn't need verification
        opts.limits.max_connections_per_host = 20; // Allow more local connections
        return opts;
    }
    
    bool is_localhost(const char* hostname) const {
        fl::string host(hostname);
        return host == "localhost" || 
               host == "127.0.0.1" || 
               host == "::1" ||
               host.starts_with("127.") ||
               host.starts_with("192.168.") ||
               host.starts_with("10.") ||
               (host.starts_with("172.") && is_private_class_b(host));
    }
    
    bool is_private_class_b(const fl::string& ip) const {
        // Check if IP is in 172.16.0.0/12 range
        // Simplified check for private networks
        return true; // Placeholder implementation
    }
};

/// Production transport with strict verification
class ProductionTransport : public TcpTransport {
public:
    ProductionTransport() : TcpTransport(create_production_options()) {}
    
    bool supports_protocol(const char* scheme) const override {
        // Only allow HTTPS in production
        return fl::string(scheme) == "https";
    }
    
private:
    static Options create_production_options() {
        Options opts;
        opts.verify = TransportVerify::YES;  // Always verify in production
        opts.verify_hostname = true;
        opts.allow_self_signed = false;
        opts.limits.connection_timeout_ms = 15000;  // Longer timeout for remote servers
        return opts;
    }
};

/// Development transport with no verification
class DevelopmentTransport : public TcpTransport {
public:
    DevelopmentTransport() : TcpTransport(create_development_options()) {}
    
private:
    static Options create_development_options() {
        Options opts;
        opts.verify = TransportVerify::NO;   // No verification for development
        opts.allow_self_signed = true;      // Allow self-signed certs
        opts.limits.connection_timeout_ms = 5000;  // Faster timeout for local dev
        return opts;
    }
};

} // namespace fl
```

### 2. HTTP Client with Transport Configuration

```cpp
namespace fl {

class HttpClient : public EngineEvents::Listener {
public:
    /// Create HTTP client with default TCP transport
    HttpClient() : HttpClient(fl::make_unique<TcpTransport>()) {}
    
    /// Create HTTP client with custom transport
    explicit HttpClient(fl::unique_ptr<Transport> transport) 
        : mTransport(fl::move(transport)) {}
    
    /// Destructor
    ~HttpClient();
    
    /// Factory methods for common transport configurations
    
    /// Create client optimized for local development (localhost only)
    /// @code
    /// auto client = HttpClient::create_local_client();
    /// client.get("http://localhost:8080/api");  // Fast local connections
    /// @endcode
    static fl::unique_ptr<HttpClient> create_local_client() {
        return fl::make_unique<HttpClient>(fl::make_unique<LocalTransport>());
    }
    
    /// Create client with IPv4-only transport
    /// @code
    /// auto client = HttpClient::create_ipv4_client();
    /// client.get("http://api.example.com/data");  // Forces IPv4
    /// @endcode
    static fl::unique_ptr<HttpClient> create_ipv4_client() {
        return fl::make_unique<HttpClient>(TcpTransport::create_ipv4_only());
    }
    
    /// Create client with IPv6-only transport
    /// @code
    /// auto client = HttpClient::create_ipv6_client();
    /// client.get("http://api.example.com/data");  // Forces IPv6
    /// @endcode
    static fl::unique_ptr<HttpClient> create_ipv6_client() {
        return fl::make_unique<HttpClient>(TcpTransport::create_ipv6_only());
    }
    
    /// Create client with dual-stack transport (IPv6 preferred, IPv4 fallback)
    /// @code
    /// auto client = HttpClient::create_dual_stack_client();
    /// client.get("http://api.example.com/data");  // Try IPv6 first, fallback to IPv4
    /// @endcode
    static fl::unique_ptr<HttpClient> create_dual_stack_client() {
        return fl::make_unique<HttpClient>(TcpTransport::create_dual_stack());
    }
    
    /// Create client with custom transport configuration
    /// @code
    /// TcpTransport::Options opts;
    /// opts.ip_version = IpVersion::IPV4_FIRST;
    /// opts.local_address = "192.168.1.100";  // Bind to specific local IP
    /// opts.limits.max_connections_per_host = 2;
    /// opts.limits.keepalive_timeout_ms = 30000;
    /// 
    /// auto transport = fl::make_unique<TcpTransport>(opts);
    /// auto client = HttpClient::create_with_transport(fl::move(transport));
    /// @endcode
    static fl::unique_ptr<HttpClient> create_with_transport(fl::unique_ptr<Transport> transport) {
        return fl::make_unique<HttpClient>(fl::move(transport));
    }
    
    /// Configure transport options for existing client
    /// @code
    /// TcpTransport::Options opts;
    /// opts.ip_version = IpVersion::IPV6_ONLY;
    /// opts.limits.connection_timeout_ms = 5000;
    /// client.configure_transport(opts);
    /// @endcode
    void configure_transport(const TcpTransport::Options& options) {
        if (auto* tcp_transport = dynamic_cast<TcpTransport*>(mTransport.get())) {
            tcp_transport->set_options(options);
        }
    }
    
    /// Get transport statistics
    /// @code
    /// auto stats = client.get_transport_stats();
    /// FL_WARN("Active connections: " << stats.active_connections);
    /// FL_WARN("Total created: " << stats.total_created);
    /// @endcode
    struct TransportStats {
        fl::size active_connections;
        fl::size total_connections_created;
        fl::string transport_type;
        fl::optional<TcpTransport::PoolStats> pool_stats; // Only for TcpTransport
    };
    TransportStats get_transport_stats() const {
        TransportStats stats;
        stats.active_connections = mTransport->get_active_connections();
        stats.total_connections_created = mTransport->get_total_connections_created();
        
        if (auto* tcp_transport = dynamic_cast<TcpTransport*>(mTransport.get())) {
            stats.transport_type = "TcpTransport";
            stats.pool_stats = tcp_transport->get_pool_stats();
        } else if (dynamic_cast<LocalTransport*>(mTransport.get())) {
            stats.transport_type = "LocalTransport";
        } else {
            stats.transport_type = "CustomTransport";
        }
        
        return stats;
    }
    
    /// Close all transport connections (useful for network changes)
    /// @code
    /// // WiFi reconnected, close old connections
    /// client.close_transport_connections();
    /// @endcode
    void close_transport_connections() {
        mTransport->close_all_connections();
    }

    // ========== HTTP API (unchanged - same simple interface) ==========
    
    /// HTTP GET request with customizable timeout and headers (returns future)
    fl::future<Response> get(const char* url, 
                             uint32_t timeout_ms = 5000,
                             fl::span<const fl::pair<fl::string, fl::string>> headers = {});
    
    /// HTTP POST request with customizable timeout and headers (returns future)
    fl::future<Response> post(const char* url, 
                              fl::span<const fl::u8> data,
                              uint32_t timeout_ms = 5000,
                              fl::span<const fl::pair<fl::string, fl::string>> headers = {});
    
    /// Async GET request with callback
    void get_async(const char* url, 
                   fl::function<void(const Response&)> callback,
                   uint32_t timeout_ms = 5000,
                   fl::span<const fl::pair<fl::string, fl::string>> headers = {});
    
    /// Async POST request with callback
    void post_async(const char* url, 
                    fl::span<const fl::u8> data,
                    fl::function<void(const Response&)> callback,
                    uint32_t timeout_ms = 5000,
                    fl::span<const fl::pair<fl::string, fl::string>> headers = {});
    
    // ... (all other HTTP methods remain unchanged)

private:
    fl::unique_ptr<Transport> mTransport;
    // ... (rest of private members)
};

} // namespace fl
```

### 3. Transport Usage Examples (Inspired by Python httpx)

The transport abstraction allows the same fine-grained control as Python's httpx library:

#### **Example 1: IPv4/IPv6 Configuration (Similar to httpx)**

**Python httpx equivalent:**
```python
import httpx

# IPv4 only
transport_ipv4 = httpx.HTTPTransport(local_address="0.0.0.0")
client_ipv4 = httpx.Client(transport=transport_ipv4)

# IPv6 only  
transport_ipv6 = httpx.HTTPTransport(local_address="::")
client_ipv6 = httpx.Client(transport=transport_ipv6)

# Dual stack (IPv6 preferred)
transport_dual = httpx.HTTPTransport()  # Default behavior
client_dual = httpx.Client(transport=transport_dual)
```

**FastLED C++ equivalent:**
```cpp
#include "fl/networking/http_client.h"

using namespace fl;

void setup_network_clients() {
    // IPv4 only client
    auto ipv4_client = HttpClient::create_ipv4_client();
    ipv4_client->get("http://api.example.com/data");  // Forces IPv4
    
    // IPv6 only client
    auto ipv6_client = HttpClient::create_ipv6_client();
    ipv6_client->get("http://api.example.com/data");  // Forces IPv6
    
    // Dual stack client (IPv6 preferred, IPv4 fallback)
    auto dual_client = HttpClient::create_dual_stack_client();
    dual_client->get("http://api.example.com/data");  // Try IPv6 first
    
    // Local development client (localhost only)
    auto local_client = HttpClient::create_local_client();
    local_client->get("http://localhost:8080/api");   // Fast local connections
}
```

#### **Example 2: Custom Transport Configuration**

**Python httpx equivalent:**
```python
import httpx

# Custom transport with specific options
transport = httpx.HTTPTransport(
    http1=True,
    http2=False,
    verify=True,
    limits=httpx.Limits(
        max_keepalive_connections=20,
        max_connections=100,
        keepalive_expiry=30.0
    ),
    local_address="192.168.1.100"  # Bind to specific local IP
)

client = httpx.Client(transport=transport)
```

**FastLED C++ equivalent:**
```cpp
void setup_custom_transport() {
    // Configure transport options
    TcpTransport::Options transport_opts;
    transport_opts.ip_version = IpVersion::IPV4_FIRST;  // Prefer IPv4
    transport_opts.local_address = "192.168.1.100";    // Bind to specific local IP
    transport_opts.limits.max_keepalive_connections = 20;
    transport_opts.limits.max_connections_per_host = 5;
    transport_opts.limits.keepalive_timeout_ms = 30000;  // 30 seconds
    transport_opts.limits.connection_timeout_ms = 5000;  // 5 second connect timeout
    transport_opts.enable_nodelay = true;               // Disable Nagle's algorithm
    transport_opts.socket_buffer_size = 16384;          // 16KB socket buffer
    
    // Create transport and client
    auto transport = fl::make_unique<TcpTransport>(transport_opts);
    auto client = HttpClient::create_with_transport(fl::move(transport));
    
    // Use client normally - transport handles connection details
    auto future = client->get("http://api.example.com/data", 10000, {
        {"Authorization", "Bearer api_token"}
    });
}
```

#### **Example 3: IoT Device with Network Flexibility**

```cpp
class IoTDevice {
private:
    fl::unique_ptr<HttpClient> mClient;
    fl::string mApiToken;
    
public:
    void initialize_networking(bool prefer_ipv6 = false, bool local_dev = false) {
        if (local_dev) {
            // Development mode - local only, fast connections
            mClient = HttpClient::create_local_client();
            FL_WARN("Using local transport for development");
            
        } else if (prefer_ipv6) {
            // Production IPv6 preferred
            TcpTransport::Options opts;
            opts.ip_version = IpVersion::IPV6_FIRST;
            opts.limits.max_connections_per_host = 3;  // Conservative for embedded
            opts.limits.connection_timeout_ms = 15000; // Longer timeout for remote
            
            auto transport = fl::make_unique<TcpTransport>(opts);
            mClient = HttpClient::create_with_transport(fl::move(transport));
            FL_WARN("Using IPv6-preferred transport");
            
        } else {
            // Production IPv4 preferred (more compatible)
            TcpTransport::Options opts;
            opts.ip_version = IpVersion::IPV4_FIRST;
            opts.limits.max_connections_per_host = 2;  // Conservative for embedded
            opts.limits.connection_timeout_ms = 10000;
            opts.enable_keepalive = true;              // Reuse connections
            
            auto transport = fl::make_unique<TcpTransport>(opts);
            mClient = HttpClient::create_with_transport(fl::move(transport));
            FL_WARN("Using IPv4-preferred transport");
        }
    }
    
    void send_telemetry_data(fl::span<const fl::u8> sensor_data) {
        // Same API regardless of transport configuration!
        auto future = mClient->post("https://iot.api.com/telemetry", sensor_data, 15000, {
            {"Authorization", "Bearer " + mApiToken},
            {"Content-Type", "application/octet-stream"},
            {"X-Device-ID", get_device_id()}
        });
        
        // Check transport status
        auto stats = mClient->get_transport_stats();
        FL_WARN("Transport: " << stats.transport_type);
        FL_WARN("Active connections: " << stats.active_connections);
        
        if (stats.pool_stats) {
            FL_WARN("Connection reuse: " << stats.pool_stats->total_reused);
        }
    }
    
    void handle_network_change() {
        // WiFi reconnected or network interface changed
        FL_WARN("Network change detected - closing old connections");
        mClient->close_transport_connections();
        
        // New connections will use updated network configuration automatically
    }
    
    void print_network_diagnostics() {
        auto stats = mClient->get_transport_stats();
        
        FL_WARN("=== Network Transport Diagnostics ===");
        FL_WARN("Transport Type: " << stats.transport_type);
        FL_WARN("Active Connections: " << stats.active_connections);
        FL_WARN("Total Created: " << stats.total_connections_created);
        
        if (stats.pool_stats) {
            auto& pool = *stats.pool_stats;
            FL_WARN("Pooled Connections: " << pool.pooled_connections);
            FL_WARN("Connections Reused: " << pool.total_reused);
            FL_WARN("Connections Closed: " << pool.total_closed);
            FL_WARN("Oldest Connection Age: " << pool.oldest_connection_age_ms << "ms");
        }
    }
};

// Usage
IoTDevice device;

void setup() {
    bool prefer_ipv6 = false;      // Set based on your network
    bool development_mode = false; // Set to true for local testing
    
    device.initialize_networking(prefer_ipv6, development_mode);
    device.authenticate("your_api_token");
}

void loop() {
    // Send sensor data every 30 seconds
    EVERY_N_SECONDS(30) {
        fl::vector<fl::u8> sensor_data = collect_sensor_readings();
        device.send_telemetry_data(sensor_data);
    }
    
    // Print diagnostics every 5 minutes
    EVERY_N_SECONDS(300) {
        device.print_network_diagnostics();
    }
    
    // Handle network interface changes
    if (wifi_status_changed()) {
        device.handle_network_change();
    }
    
    FastLED.show();
}
```

#### **Example 4: Multi-Client Architecture (Different Transports for Different Services)**

```cpp
class MultiServiceDevice {
private:
    fl::unique_ptr<HttpClient> mLocalClient;   // For local dashboard/debugging
    fl::unique_ptr<HttpClient> mCloudClient;   // For cloud telemetry
    fl::unique_ptr<HttpClient> mUpdateClient;  // For firmware updates
    
public:
    void initialize_clients() {
        // Local client - fast, localhost only
        mLocalClient = HttpClient::create_local_client();
        
        // Cloud client - IPv4 preferred for compatibility, connection pooling
        TcpTransport::Options cloud_opts;
        cloud_opts.ip_version = IpVersion::IPV4_FIRST;
        cloud_opts.limits.max_connections_per_host = 3;
        cloud_opts.limits.keepalive_timeout_ms = 120000;  // 2 minutes
        cloud_opts.enable_keepalive = true;
        
        auto cloud_transport = fl::make_unique<TcpTransport>(cloud_opts);
        mCloudClient = HttpClient::create_with_transport(fl::move(cloud_transport));
        
        // Update client - single connection, longer timeouts
        TcpTransport::Options update_opts;
        update_opts.ip_version = IpVersion::AUTO;
        update_opts.limits.max_connections_per_host = 1;
        update_opts.limits.connection_timeout_ms = 30000;  // 30s for slow update servers
        update_opts.limits.read_timeout_ms = 300000;       // 5 minutes for large files
        
        auto update_transport = fl::make_unique<TcpTransport>(update_opts);
        mUpdateClient = HttpClient::create_with_transport(fl::move(update_transport));
    }
    
    void send_local_status() {
        // Local dashboard - fast, no auth needed
        mLocalClient->post_async("http://localhost:8080/status", get_status_data(),
            [](const Response& resp) {
                FL_WARN("Local status: " << (resp.ok() ? "OK" : "FAILED"));
            });
    }
    
    void send_cloud_telemetry() {
        // Cloud service - with auth, retry logic
        mCloudClient->post_async("https://cloud.api.com/telemetry", get_telemetry_data(), 
            [](const Response& resp) {
                if (resp.ok()) {
                    FL_WARN("Cloud telemetry uploaded successfully");
                } else {
                    FL_WARN("Cloud upload failed: " << resp.status_code());
                    // Could trigger retry logic here
                }
            }, 15000, {
                {"Authorization", "Bearer cloud_token"},
                {"Content-Type", "application/json"}
            });
    }
    
    void check_for_updates() {
        // Update service - long timeout, progress monitoring
        mUpdateClient->get_stream_async("https://updates.firmware.com/latest.bin", {
            {"Authorization", "Bearer update_token"},
            {"Accept", "application/octet-stream"}
        },
        // Chunk callback - process firmware data as it arrives
        [this](fl::span<const fl::u8> chunk, bool is_final) -> bool {
            write_firmware_chunk(chunk);
            
            if (is_final) {
                FL_WARN("Firmware download complete");
                verify_and_install_firmware();
            }
            
            return !should_abort_update();  // Continue unless user aborts
        },
        // Complete callback
        [](int status, const fl::string& error) {
            if (status == 200) {
                FL_WARN("Firmware update ready for installation");
            } else {
                FL_WARN("Firmware download failed: " << error);
            }
        });
    }
    
    void print_all_transport_stats() {
        FL_WARN("=== Multi-Client Transport Statistics ===");
        
        auto local_stats = mLocalClient->get_transport_stats();
        FL_WARN("Local Client (" << local_stats.transport_type << "):");
        FL_WARN("  Active: " << local_stats.active_connections);
        FL_WARN("  Total: " << local_stats.total_connections_created);
        
        auto cloud_stats = mCloudClient->get_transport_stats();
        FL_WARN("Cloud Client (" << cloud_stats.transport_type << "):");
        FL_WARN("  Active: " << cloud_stats.active_connections);
        FL_WARN("  Total: " << cloud_stats.total_connections_created);
        if (cloud_stats.pool_stats) {
            FL_WARN("  Reused: " << cloud_stats.pool_stats->total_reused);
        }
        
        auto update_stats = mUpdateClient->get_transport_stats();
        FL_WARN("Update Client (" << update_stats.transport_type << "):");
        FL_WARN("  Active: " << update_stats.active_connections);
        FL_WARN("  Total: " << update_stats.total_connections_created);
    }
};
```

#### **Example 5: Dynamic Transport Reconfiguration**

```cpp
class AdaptiveNetworkClient {
private:
    fl::unique_ptr<HttpClient> mClient;
    bool mIpv6Available = false;
    bool mIpv4Available = false;
    
public:
    void probe_network_capabilities() {
        FL_WARN("Probing network capabilities...");
        
        // Test IPv4 connectivity
        auto ipv4_client = HttpClient::create_ipv4_client();
        auto ipv4_future = ipv4_client->get("http://httpbin.org/ip", 5000);
        
        // Test IPv6 connectivity
        auto ipv6_client = HttpClient::create_ipv6_client();
        auto ipv6_future = ipv6_client->get("http://httpbin.org/ip", 5000);
        
        // Wait for both tests (simplified for example)
        delay(6000);
        
        if (ipv4_future.is_ready()) {
            auto result = ipv4_future.try_result();
            mIpv4Available = result && result->ok();
            FL_WARN("IPv4 available: " << (mIpv4Available ? "YES" : "NO"));
        }
        
        if (ipv6_future.is_ready()) {
            auto result = ipv6_future.try_result();
            mIpv6Available = result && result->ok();
            FL_WARN("IPv6 available: " << (mIpv6Available ? "YES" : "NO"));
        }
        
        // Configure optimal transport based on results
        configure_optimal_transport();
    }
    
    void configure_optimal_transport() {
        TcpTransport::Options opts;
        
        if (mIpv6Available && mIpv4Available) {
            // Both available - prefer IPv6
            opts.ip_version = IpVersion::IPV6_FIRST;
            FL_WARN("Configuring dual-stack transport (IPv6 preferred)");
            
        } else if (mIpv6Available) {
            // IPv6 only
            opts.ip_version = IpVersion::IPV6_ONLY;
            FL_WARN("Configuring IPv6-only transport");
            
        } else if (mIpv4Available) {
            // IPv4 only
            opts.ip_version = IpVersion::IPV4_ONLY;
            FL_WARN("Configuring IPv4-only transport");
            
        } else {
            // No connectivity - use localhost only
            FL_WARN("No external connectivity - using localhost-only transport");
            mClient = HttpClient::create_local_client();
            return;
        }
        
        // Configure connection limits based on available protocols
        opts.limits.max_connections_per_host = mIpv6Available ? 4 : 2;
        opts.limits.connection_timeout_ms = 10000;
        opts.enable_keepalive = true;
        
        auto transport = fl::make_unique<TcpTransport>(opts);
        mClient = HttpClient::create_with_transport(fl::move(transport));
    }
    
    void reconfigure_on_network_change() {
        FL_WARN("Network change detected - reprobing capabilities");
        
        // Close existing connections
        if (mClient) {
            mClient->close_transport_connections();
        }
        
        // Re-probe network and reconfigure
        probe_network_capabilities();
    }
};
```

### Transport Architecture Benefits

**🎯 Key Advantages of the Transport Abstraction:**

1. **📡 Protocol Flexibility**: Easy IPv4/IPv6 configuration without changing application code
2. **🔧 Connection Management**: Built-in connection pooling and keep-alive support  
3. **⚡ Performance Tuning**: Configurable timeouts, buffer sizes, and socket options
4. **🏠 Environment Adaptation**: Different transports for local vs. production environments
5. **📊 Monitoring**: Transport-level statistics and connection diagnostics
6. **🔄 Dynamic Reconfiguration**: Change transport behavior without recreating HttpClient
7. **🧪 Testing**: Easy to mock transports for unit testing
8. **🌐 Future-Proof**: Ready for HTTP/2, proxies, custom protocols

**Python httpx Comparison:**
- ✅ **Same flexibility** as httpx.HTTPTransport configuration
- ✅ **Same API patterns** for client creation with custom transports  
- ✅ **Same connection pooling** concepts and limits
- ✅ **Same transport statistics** and monitoring capabilities
- ✅ **Better type safety** with C++ compile-time checking
- ✅ **Better performance** with zero-cost abstractions and RAII

The transport abstraction provides **enterprise-grade networking flexibility** while maintaining FastLED's **simple, embedded-friendly design**! 🚀

### 4. HTTP Client with Header Support

```cpp
namespace fl {

class HttpClient : public EngineEvents::Listener {
public:
    /// Create HTTP client with default TCP transport
    HttpClient() : HttpClient(fl::make_unique<TcpTransport>()) {}
    
    /// Create HTTP client with custom transport
    explicit HttpClient(fl::unique_ptr<Transport> transport) 
        : mTransport(fl::move(transport)) {}
    
    /// Destructor
    ~HttpClient();
    
    /// Factory methods for common transport configurations
    
    /// Create client optimized for local development (localhost only)
    /// @code
    /// auto client = HttpClient::create_local_client();
    /// client.get("http://localhost:8080/api");  // Fast local connections
    /// @endcode
    static fl::unique_ptr<HttpClient> create_local_client() {
        return fl::make_unique<HttpClient>(fl::make_unique<LocalTransport>());
    }
    
    /// Create client with IPv4-only transport
    /// @code
    /// auto client = HttpClient::create_ipv4_client();
    /// client.get("http://api.example.com/data");  // Forces IPv4
    /// @endcode
    static fl::unique_ptr<HttpClient> create_ipv4_client() {
        return fl::make_unique<HttpClient>(TcpTransport::create_ipv4_only());
    }
    
    /// Create client with IPv6-only transport
    /// @code
    /// auto client = HttpClient::create_ipv6_client();
    /// client.get("http://api.example.com/data");  // Forces IPv6
    /// @endcode
    static fl::unique_ptr<HttpClient> create_ipv6_client() {
        return fl::make_unique<HttpClient>(TcpTransport::create_ipv6_only());
    }
    
    /// Create client with dual-stack transport (IPv6 preferred, IPv4 fallback)
    /// @code
    /// auto client = HttpClient::create_dual_stack_client();
    /// client.get("http://api.example.com/data");  // Try IPv6 first, fallback to IPv4
    /// @endcode
    static fl::unique_ptr<HttpClient> create_dual_stack_client() {
        return fl::make_unique<HttpClient>(TcpTransport::create_dual_stack());
    }
    
    /// Create client with custom transport configuration
    /// @code
    /// TcpTransport::Options opts;
    /// opts.ip_version = IpVersion::IPV4_FIRST;
    /// opts.local_address = "192.168.1.100";  // Bind to specific local IP
    /// opts.limits.max_connections_per_host = 2;
    /// opts.limits.keepalive_timeout_ms = 30000;
    /// 
    /// auto transport = fl::make_unique<TcpTransport>(opts);
    /// auto client = HttpClient::create_with_transport(fl::move(transport));
    /// @endcode
    static fl::unique_ptr<HttpClient> create_with_transport(fl::unique_ptr<Transport> transport) {
        return fl::make_unique<HttpClient>(fl::move(transport));
    }
    
    /// Configure transport options for existing client
    /// @code
    /// TcpTransport::Options opts;
    /// opts.ip_version = IpVersion::IPV6_ONLY;
    /// opts.limits.connection_timeout_ms = 5000;
    /// client.configure_transport(opts);
    /// @endcode
    void configure_transport(const TcpTransport::Options& options) {
        if (auto* tcp_transport = dynamic_cast<TcpTransport*>(mTransport.get())) {
            tcp_transport->set_options(options);
        }
    }
    
    /// Get transport statistics
    /// @code
    /// auto stats = client.get_transport_stats();
    /// FL_WARN("Active connections: " << stats.active_connections);
    /// FL_WARN("Total created: " << stats.total_created);
    /// @endcode
    struct TransportStats {
        fl::size active_connections;
        fl::size total_connections_created;
        fl::string transport_type;
        fl::optional<TcpTransport::PoolStats> pool_stats; // Only for TcpTransport
    };
    TransportStats get_transport_stats() const {
        TransportStats stats;
        stats.active_connections = mTransport->get_active_connections();
        stats.total_connections_created = mTransport->get_total_connections_created();
        
        if (auto* tcp_transport = dynamic_cast<TcpTransport*>(mTransport.get())) {
            stats.transport_type = "TcpTransport";
            stats.pool_stats = tcp_transport->get_pool_stats();
        } else if (dynamic_cast<LocalTransport*>(mTransport.get())) {
            stats.transport_type = "LocalTransport";
        } else {
            stats.transport_type = "CustomTransport";
        }
        
        return stats;
    }
    
    /// Close all transport connections (useful for network changes)
    /// @code
    /// // WiFi reconnected, close old connections
    /// client.close_transport_connections();
    /// @endcode
    void close_transport_connections() {
        mTransport->close_all_connections();
    }

    // ========== HTTP API (unchanged - same simple interface) ==========
    
    /// HTTP GET request with customizable timeout and headers (returns future)
    fl::future<Response> get(const char* url, 
                             uint32_t timeout_ms = 5000,
                             fl::span<const fl::pair<fl::string, fl::string>> headers = {});
    
    /// HTTP POST request with customizable timeout and headers (returns future)
    fl::future<Response> post(const char* url, 
                              fl::span<const fl::u8> data,
                              uint32_t timeout_ms = 5000,
                              fl::span<const fl::pair<fl::string, fl::string>> headers = {});
    
    /// Async GET request with callback
    void get_async(const char* url, 
                   fl::function<void(const Response&)> callback,
                   uint32_t timeout_ms = 5000,
                   fl::span<const fl::pair<fl::string, fl::string>> headers = {});
    
    /// Async POST request with callback
    void post_async(const char* url, 
                    fl::span<const fl::u8> data,
                    fl::function<void(const Response&)> callback,
                    uint32_t timeout_ms = 5000,
                    fl::span<const fl::pair<fl::string, fl::string>> headers = {});
    
    // ... (all other HTTP methods remain unchanged)

private:
    fl::unique_ptr<Transport> mTransport;
    // ... (rest of private members)
};

} // namespace fl
```

### 4. Network Event Pumping Integration

```cpp
namespace fl {

class NetworkEventPump : public EngineEvents::Listener {
public:
    static NetworkEventPump& instance() {
        static NetworkEventPump pump;
        return pump;
    }
    
    void register_client(HttpClient* client) {
        auto it = fl::find(mClients.begin(), mClients.end(), client);
        if (it == mClients.end()) {
            mClients.push_back(client);
        }
    }
    
    void unregister_client(HttpClient* client) {
        auto it = fl::find(mClients.begin(), mClients.end(), client);
        if (it != mClients.end()) {
            mClients.erase(it);
        }
    }
    
    // Manual pumping for users who don't want EngineEvents integration
    static void pump_all_clients() {
        instance().pump_clients();
    }

private:
    NetworkEventPump() {
        EngineEvents::addListener(this, 10); // Low priority, run after UI
    }
    
    ~NetworkEventPump() {
        EngineEvents::removeListener(this);
    }
    
    // EngineEvents::Listener interface - pump after frame is drawn
    void onEndFrame() override {
        pump_clients();
    }
    
    void pump_clients() {
        for (auto* client : mClients) {
            client->pump_network_events();
        }
    }
    
    fl::vector<HttpClient*> mClients;
};

} // namespace fl
```

## Usage Examples

### 1. **Simplified API with Defaults (EASIEST)**

```cpp
#include "fl/networking/http_client.h"

using namespace fl;

HttpClient client;

void setup() {
    // SIMPLEST usage - just the URL! (uses 5s timeout, no headers)
    auto health_future = client.get("http://localhost:8899/health");
    
    // Simple POST with just URL and data
    fl::vector<fl::u8> sensor_data = get_sensor_readings();
    auto upload_future = client.post("http://localhost:8899/upload", sensor_data);
    
    // Add custom timeout when needed
    auto slow_api_future = client.get("http://slow-api.com/data", 15000);
    
    // Add headers when needed (using default 5s timeout)
    auto auth_future = client.get("http://api.weather.com/current", 5000, {
        {"Authorization", "Bearer weather_api_key"},
        {"Accept", "application/json"},
        {"User-Agent", "WeatherStation/1.0"}
    });
    
    // Store futures for checking in loop()
    pending_futures.push_back(fl::move(health_future));
    pending_futures.push_back(fl::move(upload_future));
    pending_futures.push_back(fl::move(slow_api_future));
    pending_futures.push_back(fl::move(auth_future));
}

void loop() {
    // Check all pending requests
    for (auto it = pending_futures.begin(); it != pending_futures.end();) {
        if (it->is_ready()) {
            auto result = it->try_result();
            if (result && result->ok()) {
                FL_WARN("Success: " << result->text());
            }
            it = pending_futures.erase(it);
        } else {
            ++it;
        }
    }
    
    FastLED.show();
}
```

### 2. **Callback API with Defaults**

```cpp
void setup() {
    // Ultra-simple callback API - just URL and callback!
    client.get_async("http://localhost:8899/status", [](const Response& response) {
        if (response.ok()) {
            FL_WARN("Status: " << response.text());
        }
    });
    
    // Simple POST with just URL, data, and callback
    fl::vector<fl::u8> telemetry = collect_telemetry();
    client.post_async("http://localhost:8899/telemetry", telemetry, [](const Response& response) {
        FL_WARN("Telemetry upload: " << response.status_code());
    });
    
    // Add headers when needed (using default timeout)
    client.get_async("http://api.example.com/user", [](const Response& response) {
        if (response.ok()) {
            FL_WARN("User data: " << response.text());
        }
    }, 5000, {
        {"Authorization", "Bearer user_token"},
        {"Accept", "application/json"}
    });
    
    // Custom timeout with headers
    client.post_async("http://slow-api.com/upload", telemetry, [](const Response& response) {
        FL_WARN("Slow upload: " << response.status_code());
    }, 20000, {
        {"Content-Type", "application/octet-stream"},
        {"X-Telemetry-Version", "1.0"}
    });
}

void loop() {
    // Callbacks fire automatically via EngineEvents
    FastLED.show();
}
```

### 3. **Mixed API Usage**

```cpp
void setup() {
    // No headers - simplest case
    auto simple_future = client.get("http://httpbin.org/json");
    
    // With headers using vector (when you need to build headers dynamically)
    fl::vector<fl::pair<fl::string, fl::string>> dynamic_headers;
    dynamic_headers.push_back({"Authorization", "Bearer " + get_current_token()});
    if (device_has_location()) {
        dynamic_headers.push_back({"X-Location", get_location_string()});
    }
    auto dynamic_future = client.get("http://api.example.com/location", dynamic_headers);
    
    // With headers using initializer list (when headers are known at compile time)
    auto static_future = client.get("http://api.example.com/config", {
        {"Authorization", "Bearer config_token"},
        {"Accept", "application/json"},
        {"Cache-Control", "no-cache"}
    });
}
```

### 4. **Real-World IoT Example**

```cpp
class IoTDevice {
private:
    HttpClient mClient;
    fl::string mDeviceToken;
    fl::string mDeviceId;
    
public:
    void authenticate(const char* token) { mDeviceToken = token; }
    void setDeviceId(const char* id) { mDeviceId = id; }
    
    void upload_sensor_data(fl::span<const fl::u8> data) {
        // Simple local upload (no auth needed)
        if (use_local_server) {
            auto future = mClient.post("http://192.168.1.100:8080/sensors", data);
            sensor_upload_future = fl::move(future);
            return;
        }
        
        // Cloud upload with custom timeout and auth headers
        auto future = mClient.post("https://iot.example.com/sensors", data, 10000, {
            {"Authorization", "Bearer " + mDeviceToken},
            {"Content-Type", "application/octet-stream"},
            {"X-Device-ID", mDeviceId},
            {"X-Timestamp", fl::to_string(millis())}
        });
        
        sensor_upload_future = fl::move(future);
    }
    
    void get_device_config() {
        // Local config (simple)
        if (use_local_server) {
            auto future = mClient.get("http://192.168.1.100:8080/config");
            config_future = fl::move(future);
            return;
        }
        
        // Cloud config with auth
        auto future = mClient.get("https://iot.example.com/device/config", 5000, {
            {"Authorization", "Bearer " + mDeviceToken},
            {"X-Device-ID", mDeviceId}
        });
        
        config_future = fl::move(future);
    }
    
    void ping_health_check() {
        // Simplest possible API usage!
        auto future = mClient.get("http://localhost:8899/health");
        health_future = fl::move(future);
    }
    
    void check_responses() {
        if (sensor_upload_future.is_ready()) {
            auto result = sensor_upload_future.try_result();
            FL_WARN("Sensor upload: " << (result && result->ok() ? "OK" : "FAILED"));
            sensor_upload_future = fl::future<Response>();
        }
        
        if (config_future.is_ready()) {
            auto result = config_future.try_result();
            if (result && result->ok()) {
                FL_WARN("Config: " << result->text());
                // Parse and apply config...
            }
            config_future = fl::future<Response>();
        }
        
        if (health_future.is_ready()) {
            auto result = health_future.try_result();
            FL_WARN("Health: " << (result && result->ok() ? "UP" : "DOWN"));
            health_future = fl::future<Response>();
        }
    }
    
private:
    fl::future<Response> sensor_upload_future;
    fl::future<Response> config_future;
    fl::future<Response> health_future;
    bool use_local_server = true; // Start with local, fall back to cloud
};

// Usage
IoTDevice device;

void setup() {
    device.authenticate("your_device_token");
    device.setDeviceId("fastled-sensor-001");
    
    // Health check every 10 seconds
    EVERY_N_SECONDS(10) {
        device.ping_health_check();
    }
    
    // Upload sensor reading every 30 seconds
    EVERY_N_SECONDS(30) {
        fl::vector<fl::u8> sensor_data = {temperature, humidity, light_level};
        device.upload_sensor_data(sensor_data);
    }
    
    // Get updated config every 5 minutes
    EVERY_N_SECONDS(300) {
        device.get_device_config();
    }
}

void loop() {
    device.check_responses();
    FastLED.show();
}
```

### 5. **Advanced: Multiple API Endpoints**

```cpp
class WeatherStationAPI {
private:
    HttpClient mClient;
    
public:
    void send_weather_data(float temp, float humidity, float pressure) {
        fl::vector<uint8_t> data;
        data.push_back(static_cast<uint8_t>(temp));
        data.push_back(static_cast<uint8_t>(humidity));
        data.push_back(static_cast<uint8_t>(pressure));
        
        // Multiple APIs with different headers - all using initializer lists!
        
        // Send to primary weather service
        mClient.post_async("https://weather.primary.com/upload", data, {
            {"Authorization", "Bearer primary_key"},
            {"Content-Type", "application/octet-stream"},
            {"X-Station-ID", "station-001"}
        }, [](const Response& response) {
            FL_WARN("Primary upload: " << response.status_code());
        });
        
        // Send to backup weather service  
        mClient.post_async("https://weather.backup.com/data", data, {
            {"API-Key", "backup_api_key"},
            {"Content-Type", "application/octet-stream"},
            {"Station-Name", "FastLED Weather Station"}
        }, [](const Response& response) {
            FL_WARN("Backup upload: " << response.status_code());
        });
        
        // Send to local logger
        mClient.post_async("http://192.168.1.100/log", data, {
            {"Content-Type", "application/octet-stream"}
        }, [](const Response& response) {
            FL_WARN("Local log: " << response.status_code());
        });
    }
};
```

### 6. **Streaming Downloads (Memory-Efficient)**

```cpp
class DataLogger {
private:
    HttpClient mClient;
    fl::size mTotalBytes = 0;
    
public:
    void download_large_dataset() {
        // Download 100MB dataset with only 1KB RAM buffer
        mTotalBytes = 0;
        
        mClient.get_stream_async("https://api.example.com/large-dataset", {
            {"Authorization", "Bearer " + api_token},
            {"Accept", "application/octet-stream"}
        }, 
        // Chunk callback - called for each piece of data
        [this](fl::span<const uint8_t> chunk, bool is_final) -> bool {
            // Process chunk immediately (only ~1KB in memory at a time)
            process_data_chunk(chunk);
            
            mTotalBytes += chunk.size();
            
            if (mTotalBytes % 10000 == 0) {
                FL_WARN("Downloaded: " << mTotalBytes << " bytes");
            }
            
            if (is_final) {
                FL_WARN("Download complete! Total: " << mTotalBytes << " bytes");
            }
            
            // Return true to continue, false to abort
            return true;
        },
        // Complete callback - called when done or error
        [this](int status_code, const char* error_msg) {
            if (status_code == 200) {
                FL_WARN("Stream completed successfully");
                finalize_data_processing();
            } else {
                FL_WARN("Stream error " << status_code << ": " << error_msg);
                cleanup_partial_data();
            }
        });
    }
    
private:
    void process_data_chunk(fl::span<const uint8_t> chunk) {
        // Write directly to SD card, process incrementally, etc.
        // Only this small chunk is in RAM at any time!
        for (const uint8_t& byte : chunk) {
            // Process each byte...
            analyze_sensor_data(byte);
        }
    }
    
    void finalize_data_processing() {
        FL_WARN("All data processed with minimal RAM usage!");
    }
    
    void cleanup_partial_data() {
        FL_WARN("Cleaning up after failed download");
    }
};
```

### 7. **Streaming Uploads (Sensor Data Logging)**

```cpp
class SensorDataUploader {
private:
    HttpClient mClient;
    fl::size mDataIndex = 0;
    fl::vector<uint8_t> mSensorHistory; // Large accumulated data
    
public:
    void upload_accumulated_data() {
        mDataIndex = 0;
        
        mClient.post_stream("https://api.example.com/sensor-bulk", {
            {"Content-Type", "application/octet-stream"},
            {"Authorization", "Bearer " + upload_token},
            {"X-Data-Source", "weather-station-001"}
        },
        // Data provider callback - called when client needs more data
        [this](fl::span<uint8_t> buffer) -> fl::size {
            fl::size remaining = mSensorHistory.size() - mDataIndex;
            fl::size to_copy = fl::min(buffer.size(), remaining);
            
            if (to_copy > 0) {
                // Copy next chunk into the provided buffer
                fl::memcpy(buffer.data(), 
                          mSensorHistory.data() + mDataIndex, 
                          to_copy);
                mDataIndex += to_copy;
                
                FL_WARN("Uploaded chunk: " << to_copy << " bytes");
            }
            
            // Return 0 when no more data (signals end of stream)
            return to_copy;
        },
        // Complete callback
        [this](int status_code, const char* error_msg) {
            if (status_code == 200) {
                FL_WARN("All sensor data uploaded successfully!");
                mSensorHistory.clear(); // Free memory
            } else {
                FL_WARN("Upload failed: " << status_code << " - " << error_msg);
                // Could retry later...
            }
        });
    }
    
    void add_sensor_reading(float temperature, float humidity) {
        // Accumulate data over time
        uint8_t temp_byte = static_cast<uint8_t>(temperature);
        uint8_t humidity_byte = static_cast<uint8_t>(humidity);
        
        mSensorHistory.push_back(temp_byte);
        mSensorHistory.push_back(humidity_byte);
        
        // Upload when we have enough data
        if (mSensorHistory.size() > 1000) {
            upload_accumulated_data();
        }
    }
};
```

### 8. **Real-Time Data Processing Stream**

```cpp
class RealTimeProcessor {
private:
    HttpClient mClient;
    fl::vector<float> mMovingAverage;
    fl::size mWindowSize = 100;
    
public:
    void start_realtime_stream() {
        // Connect to real-time sensor data stream
        mClient.get_stream_async("https://api.realtime.com/sensor-feed", {
            {"Authorization", "Bearer realtime_token"},
            {"Accept", "application/octet-stream"},
            {"X-Stream-Type", "continuous"}
        },
        // Process each chunk of real-time data
        [this](fl::span<const uint8_t> chunk, bool is_final) -> bool {
            // Convert binary data to sensor readings
            for (fl::size i = 0; i + 3 < chunk.size(); i += 4) {
                float sensor_value = parse_float_from_bytes(chunk.subspan(i, 4));
                
                // Update moving average
                mMovingAverage.push_back(sensor_value);
                if (mMovingAverage.size() > mWindowSize) {
                    mMovingAverage.erase(mMovingAverage.begin());
                }
                
                // Process in real-time
                process_sensor_value(sensor_value);
                
                // Update LED display based on current average
                update_led_visualization();
            }
            
            // Continue streaming unless we want to stop
            return !should_stop_stream();
        },
        // Handle stream completion or errors
        [](int status_code, const char* error_msg) {
            if (status_code != 200) {
                FL_WARN("Stream error: " << status_code << " - " << error_msg);
                // Could automatically reconnect here
            }
        });
    }
    
private:
    float parse_float_from_bytes(fl::span<const uint8_t> bytes) {
        // Convert 4 bytes to float (handle endianness)
        union { uint32_t i; float f; } converter;
        converter.i = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
        return converter.f;
    }
    
    void process_sensor_value(float value) {
        // Real-time processing logic
        if (value > threshold) {
            trigger_alert();
        }
    }
    
    void update_led_visualization() {
        float avg = calculate_average();
        // Update FastLED display based on current data
        update_leds_with_value(avg);
    }
    
    bool should_stop_stream() {
        // Logic to determine when to stop streaming
        return false; // Run continuously in this example
    }
};
```

### 9. **Secure HTTPS Communication**

```cpp
class SecureIoTDevice {
private:
    HttpClient mClient;
    fl::string mDeviceId;
    fl::string mApiToken;
    
public:
    void setup_security() {
        // Configure TLS for maximum security
        HttpClient::TlsConfig tls_config;
        tls_config.verify_certificates = true;      // Always verify server certs
        tls_config.verify_hostname = true;          // Verify hostname matches cert
        tls_config.allow_self_signed = false;       // No self-signed certs
        tls_config.require_tls_v12_or_higher = true; // Modern TLS only
        mClient.set_tls_config(tls_config);
        
        // Load trusted root certificates
        int cert_count = mClient.load_ca_bundle(HttpClient::CA_BUNDLE_IOT_COMMON);
        FL_WARN("Loaded " << cert_count << " trusted certificates");
        
        // Pin certificate for critical API endpoint (optional, highest security)
        mClient.pin_certificate("api.critical-iot.com", 
                                "b5d3c2a1e4f7g8h9i0j1k2l3m4n5o6p7q8r9s0t1u2v3w4x5y6z7a8b9c0d1e2f3");
        
        // Save certificates to persistent storage
        mClient.save_certificates("/spiffs/ca_certs.bin");
    }
    
    void send_secure_telemetry() {
        fl::vector<uint8_t> encrypted_data = encrypt_sensor_readings();
        
        // All HTTPS calls automatically use TLS with configured security
        auto future = mClient.post("https://api.secure-iot.com/telemetry", encrypted_data, {
            {"Authorization", "Bearer " + mApiToken},
            {"Content-Type", "application/octet-stream"},
            {"X-Device-ID", mDeviceId},
            {"X-Security-Level", "high"}
        });
        
        // Check TLS connection details
        if (future.is_ready()) {
            auto result = future.try_result();
            if (result) {
                auto tls_info = mClient.get_last_tls_info();
                FL_WARN("TLS Version: " << tls_info.tls_version);
                FL_WARN("Cipher: " << tls_info.cipher_suite);
                FL_WARN("Cert Verified: " << (tls_info.certificate_verified ? "YES" : "NO"));
                FL_WARN("Hostname Verified: " << (tls_info.hostname_verified ? "YES" : "NO"));
            }
        }
    }
    
    void handle_certificate_update() {
        // Download and verify new certificate bundle
        mClient.get_stream_async("https://ca-updates.iot-service.com/latest-certs", {
            {"Authorization", "Bearer " + mApiToken},
            {"Accept", "application/x-pem-file"}
        },
        [this](fl::span<const uint8_t> chunk, bool is_final) -> bool {
            // Process certificate data as it arrives
            process_certificate_chunk(chunk);
            
            if (is_final) {
                // Validate and install new certificates
                if (validate_certificate_bundle()) {
                    mClient.clear_certificates();
                    install_new_certificates();
                    mClient.save_certificates("/spiffs/ca_certs.bin");
                    FL_WARN("Certificate bundle updated successfully");
                } else {
                    FL_WARN("Certificate validation failed - keeping old certificates");
                }
            }
            return true;
        },
        [](int status, const char* msg) {
            FL_WARN("Certificate update complete: " << status);
        });
    }
    
private:
    fl::vector<uint8_t> encrypt_sensor_readings() {
        // Your encryption logic here
        return fl::vector<uint8_t>{1, 2, 3, 4}; // Placeholder
    }
    
    void process_certificate_chunk(fl::span<const uint8_t> chunk) {
        // Accumulate certificate data
    }
    
    bool validate_certificate_bundle() {
        // Verify certificate signatures, validity dates, etc.
        return true; // Placeholder
    }
    
    void install_new_certificates() {
        // Install validated certificates
    }
};
```

### 10. **Development vs Production Security**

```cpp
class ConfigurableSecurityClient {
private:
    HttpClient mClient;
    bool mDevelopmentMode;
    
public:
    void setup(bool development_mode = false) {
        mDevelopmentMode = development_mode;
        
        HttpClient::TlsConfig tls_config;
        
        if (development_mode) {
            // Development settings - more permissive
            tls_config.verify_certificates = false;     // Allow dev servers
            tls_config.verify_hostname = false;         // Allow localhost
            tls_config.allow_self_signed = true;        // Allow self-signed certs
            tls_config.require_tls_v12_or_higher = false; // Allow older TLS for testing
            
            FL_WARN("DEVELOPMENT MODE: TLS verification disabled!");
            FL_WARN("DO NOT USE IN PRODUCTION!");
            
        } else {
            // Production settings - maximum security
            tls_config.verify_certificates = true;
            tls_config.verify_hostname = true;
            tls_config.allow_self_signed = false;
            tls_config.require_tls_v12_or_higher = true;
            
            // Load production certificate bundle
            mClient.load_ca_bundle(HttpClient::CA_BUNDLE_MOZILLA);
            
            // Pin critical certificates
            pin_production_certificates();
            
            FL_WARN("PRODUCTION MODE: Full TLS verification enabled");
        }
        
        mClient.set_tls_config(tls_config);
    }
    
    void test_api_connection() {
        const char* api_url = mDevelopmentMode ? 
            "https://localhost:8443/api/test" :      // Dev server
            "https://api.production.com/test";       // Production server
            
        mClient.get_async(api_url, {
            {"Authorization", "Bearer " + get_api_token()},
            {"User-Agent", "FastLED-Device/1.0"}
        }, [this](const Response& response) {
            if (response.ok()) {
                auto tls_info = mClient.get_last_tls_info();
                FL_WARN("API test successful");
                FL_WARN("Security level: " << (mDevelopmentMode ? "DEV" : "PRODUCTION"));
                FL_WARN("TLS: " << tls_info.tls_version << " with " << tls_info.cipher_suite);
            } else {
                FL_WARN("API test failed: " << response.status_code());
                if (!mDevelopmentMode) {
                    FL_WARN("Check certificate configuration");
                }
            }
        });
    }
    
private:
    void pin_production_certificates() {
        // Pin certificates for critical production services
        mClient.pin_certificate("api.production.com", 
                                "a1b2c3d4e5f6...production_cert_sha256");
        mClient.pin_certificate("cdn.production.com", 
                                "f6e5d4c3b2a1...cdn_cert_sha256");
    }
    
    fl::string get_api_token() {
        return mDevelopmentMode ? "dev_token_123" : "prod_token_xyz";
    }
};
```

### 11. **Certificate Management & Updates**

```cpp
class CertificateManager {
private:
    HttpClient* mClient;
    fl::string mCertStoragePath;
    
public:
    CertificateManager(HttpClient* client, const char* storage_path = "/spiffs/certs.bin") 
        : mClient(client), mCertStoragePath(storage_path) {}
    
    void initialize() {
        // Try loading saved certificates first
        int loaded = mClient->load_certificates(mCertStoragePath.c_str());
        
        if (loaded == 0) {
            FL_WARN("No saved certificates found, loading defaults");
            load_default_certificates();
        } else {
            FL_WARN("Loaded " << loaded << " certificates from storage");
            
            // Check if certificates need updating (once per day)
            EVERY_N_HOURS(24) {
                check_for_certificate_updates();
            }
        }
        
        // Display certificate statistics
        print_certificate_stats();
    }
    
    void load_default_certificates() {
        // Load minimal set for common IoT services
        int count = mClient->load_ca_bundle(HttpClient::CA_BUNDLE_IOT_COMMON);
        FL_WARN("Loaded " << count << " default certificates");
        
        // Add custom root CA if needed
        add_custom_root_ca();
        
        // Save for next boot
        mClient->save_certificates(mCertStoragePath.c_str());
    }
    
    void add_custom_root_ca() {
        // Add your organization's root CA
        const char* custom_ca = R"(
-----BEGIN CERTIFICATE-----
MIIDXTCCAkWgAwIBAgIJAKX1+... (your custom root CA)
...certificate content...
-----END CERTIFICATE-----
)";
        
        if (mClient->add_root_certificate(custom_ca, "Company Root CA")) {
            FL_WARN("Added custom root CA");
        } else {
            FL_WARN("Failed to add custom root CA");
        }
    }
    
    void check_for_certificate_updates() {
        FL_WARN("Checking for certificate updates...");
        
        // Use HTTPS to download certificate updates (secured by existing certs)
        mClient->get_async("https://ca-updates.yourservice.com/latest", {
            {"User-Agent", "FastLED-CertUpdater/1.0"},
            {"Accept", "application/json"}
        }, [this](const Response& response) {
            if (response.ok()) {
                // Parse JSON response to check for updates
                if (parse_update_info(response.text())) {
                    download_certificate_updates();
                }
            } else {
                FL_WARN("Certificate update check failed: " << response.status_code());
            }
        });
    }
    
    void download_certificate_updates() {
        FL_WARN("Downloading certificate updates...");
        
        fl::vector<uint8_t> cert_data;
        
        mClient->get_stream_async("https://ca-updates.yourservice.com/bundle.pem", {
            {"Authorization", "Bearer " + get_update_token()}
        },
        [&cert_data](fl::span<const uint8_t> chunk, bool is_final) -> bool {
            // Accumulate certificate data
            for (const uint8_t& byte : chunk) {
                cert_data.push_back(byte);
            }
            
            if (is_final) {
                FL_WARN("Downloaded " << cert_data.size() << " bytes of certificate data");
            }
            return true;
        },
        [this, &cert_data](int status, const char* msg) {
            if (status == 200) {
                process_certificate_update(cert_data);
            } else {
                FL_WARN("Certificate download failed: " << status << " - " << msg);
            }
        });
    }
    
    void print_certificate_stats() {
        auto stats = mClient->get_certificate_stats();
        FL_WARN("Certificate Statistics:");
        FL_WARN("  Certificates: " << stats.certificate_count);
        FL_WARN("  Total size: " << stats.total_certificate_size << " bytes");
        FL_WARN("  Pinned certs: " << stats.pinned_certificate_count);
        FL_WARN("  Capacity: " << stats.max_certificate_capacity);
    }
    
private:
    bool parse_update_info(const char* json_response) {
        // Parse JSON to check version, availability, etc.
        // Return true if update is needed
        return false; // Placeholder
    }
    
    fl::string get_update_token() {
        // Return authentication token for certificate updates
        return "cert_update_token_123";
    }
    
    void process_certificate_update(const fl::vector<uint8_t>& cert_data) {
        // Validate signature, parse certificates, update store
        FL_WARN("Processing certificate update...");
        
        // For demo, just save the new certificates
        // In practice, you'd validate signatures, check dates, etc.
        if (validate_certificate_signature(cert_data)) {
            backup_current_certificates();
            install_new_certificates(cert_data);
            mClient->save_certificates(mCertStoragePath.c_str());
            FL_WARN("Certificates updated successfully");
        } else {
            FL_WARN("Certificate validation failed - update rejected");
        }
    }
    
    bool validate_certificate_signature(const fl::vector<uint8_t>& cert_data) {
        // Verify certificate bundle signature
        return true; // Placeholder
    }
    
    void backup_current_certificates() {
        // Backup current certificates before updating
        fl::string backup_path = mCertStoragePath + ".backup";
        mClient->save_certificates(backup_path.c_str());
    }
    
    void install_new_certificates(const fl::vector<uint8_t>& cert_data) {
        // Clear existing and install new certificates
        mClient->clear_certificates();
        
        // Parse and add each certificate from the bundle
        // (In practice, you'd parse the PEM format)
        
        FL_WARN("New certificates installed");
    }
};
```

## TLS Implementation Details

### **Platform-Specific TLS Support**

#### **ESP32: mbedTLS Integration**

```cpp
#ifdef ESP32
// Internal implementation uses ESP-IDF mbedTLS
class ESP32TlsTransport : public TlsTransport {
    mbedtls_ssl_context ssl_ctx;
    mbedtls_net_context net_ctx;
    mbedtls_ssl_config ssl_config;
    mbedtls_x509_crt ca_chain;
    
public:
    bool connect(const char* hostname, int port) override {
        // Use ESP32 mbedTLS for TLS connections
        mbedtls_ssl_init(&ssl_ctx);
        mbedtls_ssl_config_init(&ssl_config);
        mbedtls_x509_crt_init(&ca_chain);
        
        // Configure for TLS client
        mbedtls_ssl_config_defaults(&ssl_config, MBEDTLS_SSL_IS_CLIENT,
                                   MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
        
        // Set CA certificates
        mbedtls_ssl_conf_ca_chain(&ssl_config, &ca_chain, nullptr);
        
        // Enable hostname verification
        mbedtls_ssl_conf_authmode(&ssl_config, MBEDTLS_SSL_VERIFY_REQUIRED);
        
        return perform_handshake(hostname, port);
    }
};
#endif
```

#### **WebAssembly: Browser TLS**

```cpp
#ifdef __EMSCRIPTEN__
// Use browser's native TLS implementation
class EmscriptenTlsTransport : public TlsTransport {
public:
    bool connect(const char* hostname, int port) override {
        // Browser handles TLS automatically for HTTPS URLs
        // Certificate validation done by browser's certificate store
        
        EM_ASM({
            // JavaScript code to handle TLS connection
            const url = UTF8ToString($0) + ":" + $1;
            console.log("Connecting to: " + url);
            
            // Browser's fetch() API handles TLS automatically
        }, hostname, port);
        
        return true; // Browser handles all TLS details
    }
    
    // Certificate pinning via browser APIs
    void pin_certificate(const char* hostname, const char* sha256) override {
        EM_ASM({
            const host = UTF8ToString($0);
            const fingerprint = UTF8ToString($1);
            
            // Use browser's SubResource Integrity or other mechanisms
            console.log("Pinning certificate for " + host + ": " + fingerprint);
        }, hostname, sha256);
    }
};
#endif
```

#### **Native Platforms: OpenSSL/mbedTLS**

```cpp
#if !defined(ESP32) && !defined(__EMSCRIPTEN__)
// Use system OpenSSL or bundled mbedTLS
class NativeTlsTransport : public TlsTransport {
    SSL_CTX* ssl_ctx = nullptr;
    SSL* ssl = nullptr;
    X509_STORE* cert_store = nullptr;
    
public:
    bool connect(const char* hostname, int port) override {
        // Initialize OpenSSL
        SSL_library_init();
        ssl_ctx = SSL_CTX_new(TLS_client_method());
        
        // Load CA certificates
        SSL_CTX_set_cert_store(ssl_ctx, cert_store);
        
        // Enable hostname verification
        SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, nullptr);
        
        return perform_ssl_handshake(hostname, port);
    }
};
#endif
```

### **Memory Management for Certificates**

#### **Compressed Certificate Storage**

```cpp
class CompressedCertificateStore {
private:
    struct CertificateEntry {
        fl::string name;
        fl::vector<uint8_t> compressed_cert;
        fl::string sha256_fingerprint;
        uint32_t expiry_timestamp;
    };
    
    fl::vector<CertificateEntry> mCertificates;
    fl::size mMaxStorageSize = 32768; // 32KB default
    
public:
    bool add_certificate(const char* pem_cert, const char* name) {
        // Parse PEM certificate
        auto cert_der = parse_pem_to_der(pem_cert);
        
        // Compress using simple RLE or zlib if available
        auto compressed = compress_certificate(cert_der);
        
        // Check if we have space
        if (get_total_size() + compressed.size() > mMaxStorageSize) {
            FL_WARN("Certificate store full, removing oldest");
            remove_oldest_certificate();
        }
        
        CertificateEntry entry;
        entry.name = name ? name : "Unknown";
        entry.compressed_cert = fl::move(compressed);
        entry.sha256_fingerprint = calculate_sha256(cert_der);
        entry.expiry_timestamp = extract_expiry_date(cert_der);
        
        mCertificates.push_back(fl::move(entry));
        return true;
    }
    
    fl::vector<uint8_t> get_certificate(const fl::string& fingerprint) {
        for (const auto& entry : mCertificates) {
            if (entry.sha256_fingerprint == fingerprint) {
                return decompress_certificate(entry.compressed_cert);
            }
        }
        return fl::vector<uint8_t>();
    }
    
private:
    fl::vector<uint8_t> compress_certificate(const fl::vector<uint8_t>& cert_der) {
        // Simple compression for certificates (they compress well)
        return cert_der; // Placeholder - use RLE or zlib
    }
    
    fl::vector<uint8_t> decompress_certificate(const fl::vector<uint8_t>& compressed) {
        return compressed; // Placeholder
    }
    
    fl::size get_total_size() const {
        fl::size total = 0;
        for (const auto& cert : mCertificates) {
            total += cert.compressed_cert.size();
        }
        return total;
    }
    
    void remove_oldest_certificate() {
        if (!mCertificates.empty()) {
            mCertificates.erase(mCertificates.begin());
        }
    }
};
```

### **TLS Error Handling**

```cpp
enum class TlsError {
    SUCCESS = 0,
    CERTIFICATE_VERIFICATION_FAILED,
    HOSTNAME_VERIFICATION_FAILED,
    CIPHER_NEGOTIATION_FAILED,
    HANDSHAKE_TIMEOUT,
    CERTIFICATE_EXPIRED,
    CERTIFICATE_NOT_TRUSTED,
    TLS_VERSION_NOT_SUPPORTED,
    MEMORY_ALLOCATION_FAILED,
    NETWORK_CONNECTION_FAILED
};

const char* tls_error_to_string(TlsError error) {
    switch (error) {
        case TlsError::SUCCESS: return "Success";
        case TlsError::CERTIFICATE_VERIFICATION_FAILED: return "Certificate verification failed";
        case TlsError::HOSTNAME_VERIFICATION_FAILED: return "Hostname verification failed";
        case TlsError::CIPHER_NEGOTIATION_FAILED: return "Cipher negotiation failed";
        case TlsError::HANDSHAKE_TIMEOUT: return "TLS handshake timeout";
        case TlsError::CERTIFICATE_EXPIRED: return "Certificate expired";
        case TlsError::CERTIFICATE_NOT_TRUSTED: return "Certificate not trusted";
        case TlsError::TLS_VERSION_NOT_SUPPORTED: return "TLS version not supported";
        case TlsError::MEMORY_ALLOCATION_FAILED: return "Memory allocation failed";
        case TlsError::NETWORK_CONNECTION_FAILED: return "Network connection failed";
        default: return "Unknown TLS error";
    }
}

// Enhanced error reporting in callbacks
client.get_async("https://api.example.com/data", {},
    [](const Response& response) {
        if (!response.ok()) {
            auto tls_info = client.get_last_tls_info();
            if (tls_info.is_secure && !tls_info.certificate_verified) {
                FL_WARN("TLS Error: Certificate verification failed");
                FL_WARN("Server cert: " << tls_info.server_certificate_sha256);
                FL_WARN("Consider adding root CA or pinning certificate");
            }
        }
    }
);
```

This comprehensive TLS design provides **enterprise-grade security** while maintaining FastLED's **simple, embedded-friendly approach**! 🔒

### **API Design Philosophy: Progressive Complexity**

**🎯 Start Simple, Add Complexity Only When Needed:**

The FastLED HTTP client follows a **progressive complexity** design where the simplest use cases require the least code:

**Level 1 - Minimal (Just URL):**
```cpp
client.get("http://localhost:8899/health");                    // Uses 5s timeout, no headers
client.post("http://localhost:8899/upload", sensor_data);     // Uses 5s timeout, no headers
```

**Level 2 - Custom Timeout:**
```cpp
client.get("http://slow-api.com/data", 15000);                // 15s timeout, no headers
client.post("http://slow-api.com/upload", data, 20000);       // 20s timeout, no headers
```

**Level 3 - Add Headers (Common Case):**
```cpp
client.get("http://api.com/data", 5000, {                     // 5s timeout, with headers
    {"Authorization", "Bearer token"}
});
client.post("http://api.com/upload", data, 10000, {           // 10s timeout, with headers
    {"Content-Type", "application/json"}
});
```

**Benefits of This Approach:**
- **✅ Zero barrier to entry** - health checks and simple APIs work with minimal code
- **✅ Sensible defaults** - 5 second timeout works for most local/fast APIs
- **✅ Incremental complexity** - add timeouts/headers only when actually needed
- **✅ Familiar C++ pattern** - default parameters are a well-understood C++ idiom

### **Why fl::string Shared Buffer Semantics Are Perfect for Networking**

**fl::string Benefits from Codebase Analysis:**
- **Copy-on-write semantics**: Copying `fl::string` only copies a shared_ptr (8-16 bytes)
- **Small string optimization**: Strings ≤64 bytes stored inline (no heap allocation)  
- **Shared ownership**: Multiple strings can share the same buffer efficiently
- **Automatic memory management**: RAII cleanup via shared_ptr
- **Thread-safe sharing**: Safe to share between threads with copy-on-write

**Strategic API Design Decisions:**

**✅ Use `const fl::string&` for:**
- **Return values** that might be stored/shared (response text, error messages)
- **Error messages** in callbacks (efficient sharing across multiple futures)
- **HTTP headers** that might be cached or logged
- **Large content** that benefits from shared ownership
- **Any data the receiver might store**

**✅ Keep `const char*` for:**
- **URLs** (usually string literals, consumed immediately)
- **HTTP methods** ("GET", "POST" - compile-time constants)
- **Immediate consumption** parameters (no storage needed)
- **Legacy C-style access** when raw char* is required

**Memory Efficiency Examples:**

```cpp
// Shared error messages across multiple futures - only ONE copy in memory!
fl::string network_timeout_error("Connection timeout after 30 seconds");

auto future1 = fl::make_error_future<Response>(network_timeout_error);
auto future2 = fl::make_error_future<Response>(network_timeout_error); 
auto future3 = fl::make_error_future<Response>(network_timeout_error);
// All three futures share the same error string buffer via copy-on-write!

// Response text sharing
void handle_response(const Response& response) {
    // Efficient sharing - no string copy!
    fl::string cached_response = response.text(); // Cheap shared_ptr copy
    
    // Can store, log, display, etc. without memory waste
    log_response(cached_response);
    display_on_screen(cached_response);
    cache_for_later(cached_response);
    // All share the same underlying buffer!
}

// Error message propagation in callbacks
client.get_stream_async("http://api.example.com/data", {},
    [](fl::span<const fl::u8> chunk, bool is_final) { return true; },
    [](int status, const fl::string& error_msg) {
        // Can efficiently store/share error message
        last_error = error_msg;        // Cheap copy
        log_error(error_msg);          // Can share same buffer
        display_error(error_msg);      // No extra memory used
        // Single string buffer shared across all uses!
    }
);
```

**Embedded System Benefits:**

1. **Memory Conservation**: Shared buffers reduce memory fragmentation
2. **Cache Efficiency**: Multiple references to same data improve cache locality
3. **Copy Performance**: Copy-on-write makes string passing nearly free
4. **RAII Safety**: Automatic cleanup prevents memory leaks
5. **Thread Safety**: Safe sharing between threads without manual synchronization

### **API Design Pattern Summary**

The FastLED networking API uses consistent naming patterns to make the right choice obvious:

**📊 Complete API Overview:**

| **Method Pattern** | **Returns** | **Use Case** | **Example** |
|-------------------|-------------|--------------|-------------|
| `get(url, timeout=5000, headers={})` | `fl::future<Response>` | Single HTTP request, check result later | Weather data, API calls |
| `get_async(url, callback, timeout=5000, headers={})` | `void` | Single HTTP request, immediate callback | Fire-and-forget requests |
| `get_stream_async(url, chunk_cb, done_cb)` | `void` | Large data, real-time streaming | File downloads, sensor feeds |
| `post(url, data, timeout=5000, headers={})` | `fl::future<Response>` | Single upload, check result later | Sensor data upload |
| `post_async(url, data, callback, timeout=5000, headers={})` | `void` | Single upload, immediate callback | Quick data submission |
| `post_stream_async(url, provider, done_cb)` | `void` | Large uploads, chunked sending | Log files, large datasets |

**🎯 When to Use Each Pattern:**

**✅ Use `get()` / `post()` (Futures) When:**
- You need to check the result later in `loop()`
- You want to store/cache the response
- You need error handling flexibility
- You're doing multiple concurrent requests

**✅ Use `get_async()` / `post_async()` (Callbacks) When:**
- You want immediate response processing
- The response is simple (success/failure)
- You don't need to store the response
- Fire-and-forget pattern is acceptable

**✅ Use `get_stream_async()` / `post_stream_async()` (Streaming) When:**
- Data is too large for memory (>1KB on embedded)
- You need real-time processing (sensor feeds)
- Progress monitoring is important
- Memory efficiency is critical

**🔧 Practical Decision Guide:**

```cpp
// ✅ Perfect for futures - check result when convenient
auto health_future = client.get("http://localhost:8899/health");        // Simplest!
auto weather_future = client.get("http://weather.api.com/current", 10000, {
    {"Authorization", "Bearer token"}
});
// Later in loop(): if (weather_future.is_ready()) { ... }

// ✅ Perfect for callbacks - immediate simple processing  
client.get_async("http://api.com/ping", [](const Response& r) {
    FL_WARN("API status: " << (r.ok() ? "UP" : "DOWN"));
});                                                                      // Uses defaults!

// ✅ Perfect for streaming - large data with progress
client.get_stream_async("http://api.com/dataset.bin", {},
    [](fl::span<const fl::u8> chunk, bool final) { 
        process_chunk(chunk); 
        return true; 
    },
    [](int status, const fl::string& error) { 
        FL_WARN("Download: " << status); 
    }
);
```

**💡 Pro Tips:**
- **Start simple**: Most APIs work with just `client.get("http://localhost:8899/health")`
- **Add complexity gradually**: Add timeout/headers only when needed
- **Mix and match**: Use futures for some requests, callbacks for others
- **Error sharing**: `fl::string` errors can be stored/shared efficiently  
- **Memory efficiency**: Streaming APIs prevent memory exhaustion
- **Thread safety**: All patterns work safely with FastLED's threading model

## 🎯 **Key API Improvements Summary**

**✅ What Changed:**
- **Default timeout**: All methods now default to 5000ms (5 seconds)
- **Default headers**: All methods now default to empty headers (`{}`)
- **Removed overloads**: No separate initializer_list overloads (fl::span handles both)
- **Simplified signatures**: Most common case is now just URL + data (for POST)
- **Progressive complexity**: Add timeout/headers only when actually needed

**✅ Usage Comparison:**

| **Before (Verbose)** | **After (Simple)** |
|---------------------|-------------------|
| `get(url, span<headers>)` | `get(url)` |
| `get(url, {})` | `get(url)` |
| `get_async(url, {}, callback)` | `get_async(url, callback)` |
| `post(url, data, span<headers>)` | `post(url, data)` |
| Multiple overloads for headers | Single method handles all cases |

**✅ Benefits:**
- **✅ Zero learning curve** - health checks work with minimal code
- **✅ Faster prototyping** - test APIs without configuring timeouts/headers
- **✅ Fewer errors** - sensible defaults prevent common timeout issues
- **✅ Simplified API** - single method handles vectors, arrays, and initializer lists
- **✅ Backward compatible** - advanced usage still works exactly the same

**✅ Perfect For:**
- **Health checks**: `client.get("http://localhost:8899/health")`
- **Local APIs**: `client.post("http://192.168.1.100/data", sensor_data)`
- **Simple testing**: Quick API exploration without boilerplate
- **Development**: Focus on logic, not HTTP configuration

## 🔧 **fl::span Header Conversion Magic**

The `headers` parameter uses `fl::span<const fl::pair<fl::string, fl::string>>` which **automatically converts** from:

```cpp
// ✅ Initializer lists (most common)
client.get("http://api.com/data", 5000, {
    {"Authorization", "Bearer token"},
    {"Content-Type", "application/json"}
});

// ✅ fl::vector (when building headers dynamically)
fl::vector<fl::pair<fl::string, fl::string>> headers;
headers.push_back({"Authorization", get_token()});
client.get("http://api.com/data", 5000, headers);

// ✅ C-style arrays (when headers are static)
fl::pair<fl::string, fl::string> auth_headers[] = {
    {"Authorization", "Bearer static_token"}
};
client.get("http://api.com/data", 5000, auth_headers);

// ✅ Empty headers (uses default)
client.get("http://api.com/data"); // No headers needed
```

**Result**: **One method signature handles all use cases** - no overload explosion! 🎯

### **Response Object API Improvements with fl::deque**

The Response object has been optimized for FastLED's patterns and embedded use, with **fl::deque** providing memory-efficient storage:

**✅ Memory-Efficient Binary Data Access:**
```cpp
// Iterator-based access (most memory efficient with fl::deque)
for (const fl::u8& byte : response) {
    process_byte(byte);  // Direct deque iteration - no memory allocation!
}

// Chunk-based processing (perfect for large responses)
response.process_chunks(512, [](fl::span<const fl::u8> chunk) {
    write_to_sd_card(chunk);  // Process 512-byte chunks efficiently
    return true;  // Continue processing
});

// Direct deque access (advanced users)
const fl::deque<fl::u8, fl::allocator_psram<fl::u8>>& raw_deque = response.body_deque();

// When you absolutely need contiguous memory (use sparingly!)
fl::vector<fl::u8> contiguous_buffer = response.to_vector();

// Copy to existing buffer (avoids allocation)
fl::u8 my_buffer[1024];
fl::size copied = response.copy_to_buffer(my_buffer, 1024);
```

**✅ Text Content Access (with Copy-on-Write Efficiency):**
```cpp
// Primary interface - fl::string with copy-on-write semantics
fl::string response_text = response.text();  // Cheap copy, shared buffer!

// Can store, cache, log efficiently
cache_response(response_text);    // Same buffer shared
log_response(response_text);      // No extra memory used
display_text(response_text);      // Efficient sharing

// Legacy C-style access when needed
const char* c_str = response.c_str();
fl::size len = response.text_length();
```

**🎯 Key Benefits of fl::deque Design:**

1. **fl::deque<fl::u8> storage** instead of fl::vector:
   - **No memory fragmentation**: Data stored in chunks, not one massive allocation
   - **Streaming-friendly**: Perfect for receiving HTTP responses piece-by-piece
   - **Memory-efficient growth**: Grows incrementally without copying existing data
   - **Iterator compatibility**: Works seamlessly with range-based for loops

2. **Multiple access patterns** for different use cases:
   - **Iterator-based**: Most memory efficient, perfect for streaming processing
   - **Chunk-based**: Process large responses in manageable pieces  
   - **Text access**: Copy-on-write fl::string for efficient text handling
   - **Buffer copy**: When APIs require contiguous memory

3. **Consistent fl:: types** throughout:
   - **fl::u8** instead of uint8_t for consistency
   - **fl::size** instead of size_t for embedded compatibility
   - **fl::string** for shared buffer semantics
   - **fl::deque** for memory-efficient response storage

**Real-World Usage Benefits:**
```cpp
void handle_large_response(const Response& resp) {
    if (resp.ok()) {
        // Memory-efficient processing - no huge allocations!
        
        // Option 1: Stream processing (most efficient)
        resp.process_chunks(1024, [](fl::span<const fl::u8> chunk) {
            parse_json_chunk(chunk);  // Parse as data arrives
            return true;
        });
        
        // Option 2: Text access with shared ownership
        fl::string response_text = resp.text();  // Copy-on-write
        cache_response(response_text);           // Share same buffer
        log_response(response_text);             // No extra memory
        
        // Option 3: Byte-by-byte processing (zero allocation)
        for (const fl::u8& byte : resp) {
            update_checksum(byte);  // Process without copying
        }
        
        // Option 4: Copy only when absolutely needed
        if (need_contiguous_buffer()) {
            fl::vector<fl::u8> buffer = resp.to_vector();  // Only when necessary
            legacy_api_call(buffer.data(), buffer.size());
        }
    }
}
```

**Embedded Memory Comparison with PSRAM:**
```cpp
// Scenario: 100KB HTTP response on ESP32S3 with 8MB PSRAM

// ❌ With fl::vector (internal RAM): 
// - Needs 100KB contiguous internal RAM
// - Potential 200KB during reallocation
// - High risk of out-of-memory crash
// - Uses precious internal RAM for data storage

// ✅ With PSRAM-backed fl::deque:
// - Uses ~100KB in PSRAM chunks  
// - No reallocation copying
// - Zero fragmentation risk
// - Preserves internal RAM for time-critical operations
// - Can handle responses up to several MB
// - Automatic fallback to internal RAM if PSRAM unavailable
// - Perfect for streaming processing from external memory
```

## **Complete fl::Response Interface with fl::deque**

### **Usage Examples (Starting Simple → Advanced)**

#### **1. Simple Text Response Processing**

```cpp
#include "fl/networking/http_client.h"

using namespace fl;

void simple_text_example() {
    HttpClient client;
    
    // Simple API call
    auto future = client.get("http://api.weather.com/current");
    
    // Check result in loop
    if (future.is_ready()) {
        auto result = future.try_result();
        if (result && result->ok()) {
            // Simple text access - copy-on-write fl::string
            fl::string weather_data = result->text();
            FL_WARN("Weather: " << weather_data);
            
            // Can store/share efficiently
            cache_weather_data(weather_data);  // Shared buffer
            display_on_leds(weather_data);     // No extra memory
        }
    }
}
```

#### **2. Memory-Efficient Large Response Processing**

```cpp
void large_response_example() {
    HttpClient client;
    
    // Download large dataset
    auto future = client.get("http://data.api.com/sensor-logs", 30000);
    
    if (future.is_ready()) {
        auto result = future.try_result();
        if (result && result->ok()) {
            // Option 1: Stream processing (most memory efficient)
            result->process_chunks(1024, [](fl::span<const fl::u8> chunk) {
                // Process 1KB chunks - only 1KB in memory at a time!
                parse_sensor_data_chunk(chunk);
                write_to_sd_card(chunk);
                return true;  // Continue processing
            });
            
            FL_WARN("Processed " << result->size() << " bytes with minimal RAM");
        }
    }
}
```

#### **3. Byte-by-Byte Processing (Zero Allocation)**

```cpp
void binary_data_example() {
    HttpClient client;
    
    auto future = client.get("http://firmware.server.com/update.bin");
    
    if (future.is_ready()) {
        auto result = future.try_result();
        if (result && result->ok()) {
            // Zero-allocation byte processing
            fl::u32 checksum = 0;
            fl::size byte_count = 0;
            
            for (const fl::u8& byte : *result) {
                checksum ^= byte;      // Update checksum
                byte_count++;
                
                // Can also process firmware bytes directly
                if (byte_count % 256 == 0) {
                    write_firmware_page_to_flash();
                }
            }
            
            FL_WARN("Checksum: 0x" << fl::hex << checksum);
            FL_WARN("Processed " << byte_count << " bytes with zero allocation");
        }
    }
}
```

#### **4. JSON Processing with Chunked Parsing**

```cpp
class JsonStreamProcessor {
private:
    fl::string mPartialJson;
    fl::size mBraceDepth = 0;
    
public:
    void process_json_response(const Response& response) {
        if (!response.ok()) return;
        
        // Process JSON as it arrives - perfect for large JSON arrays
        response.process_chunks(512, [this](fl::span<const fl::u8> chunk) {
            // Convert chunk to string
            fl::string chunk_str(reinterpret_cast<const char*>(chunk.data()), chunk.size());
            mPartialJson += chunk_str;
            
            // Parse complete JSON objects as they arrive
            parse_complete_json_objects();
            
            return true;  // Continue processing
        });
        
        // Handle any remaining partial JSON
        if (!mPartialJson.empty()) {
            parse_final_json_fragment();
        }
    }
    
private:
    void parse_complete_json_objects() {
        fl::size start = 0;
        
        for (fl::size i = 0; i < mPartialJson.size(); ++i) {
            if (mPartialJson[i] == '{') {
                mBraceDepth++;
            } else if (mPartialJson[i] == '}') {
                mBraceDepth--;
                
                if (mBraceDepth == 0) {
                    // Complete JSON object found
                    fl::string json_obj = mPartialJson.substr(start, i - start + 1);
                    process_single_json_object(json_obj);
                    start = i + 1;
                }
            }
        }
        
        // Keep only the incomplete part
        if (start < mPartialJson.size()) {
            mPartialJson = mPartialJson.substr(start);
        } else {
            mPartialJson.clear();
        }
    }
    
    void process_single_json_object(const fl::string& json) {
        FL_WARN("Processing JSON object: " << json.substr(0, 50) << "...");
        // Parse with your favorite JSON library
    }
    
    void parse_final_json_fragment() {
        if (!mPartialJson.empty()) {
            FL_WARN("Final JSON fragment: " << mPartialJson);
        }
    }
};

void json_streaming_example() {
    HttpClient client;
    JsonStreamProcessor processor;
    
    auto future = client.get("http://api.com/large-json-array");
    
    if (future.is_ready()) {
        auto result = future.try_result();
        if (result) {
            processor.process_json_response(*result);
        }
    }
}
```

#### **5. PSRAM Optimization on ESP32S3**

```cpp
#ifdef ESP32
void esp32_psram_example() {
    HttpClient client;
    
    // Configure client to use PSRAM for large responses
    HttpClient::MemoryConfig mem_config;
    mem_config.prefer_psram = true;           // Use PSRAM when available
    mem_config.psram_threshold = 4096;       // Use PSRAM for responses >4KB
    mem_config.max_internal_ram = 2048;      // Keep small responses in fast RAM
    client.set_memory_config(mem_config);
    
    // Download large image or dataset
    auto future = client.get("http://camera.local/image.jpg", 60000);
    
    if (future.is_ready()) {
        auto result = future.try_result();
        if (result && result->ok()) {
            FL_WARN("Downloaded " << result->size() << " bytes");
            FL_WARN("Memory type: " << (result->uses_psram() ? "PSRAM" : "Internal"));
            
            // Process large image data efficiently
            result->process_chunks(2048, [](fl::span<const fl::u8> chunk) {
                // Process 2KB image chunks - data stays in PSRAM
                decode_jpeg_chunk(chunk);
                return true;
            });
            
            // PSRAM memory automatically freed when Response destroyed
        }
    }
}
#endif
```

#### **6. Mixed Access Patterns in One Response**

```cpp
void mixed_access_example() {
    HttpClient client;
    
    auto future = client.get("http://api.com/mixed-data");
    
    if (future.is_ready()) {
        auto result = future.try_result();
        if (result && result->ok()) {
            // Multiple ways to access the same response data
            
            // 1. Get response info
            FL_WARN("Status: " << result->status_code());
            FL_WARN("Size: " << result->size() << " bytes");
            FL_WARN("Content-Type: " << result->header("Content-Type"));
            
            // 2. Quick text check (for small responses)
            if (result->size() < 1024) {
                fl::string text = result->text();
                FL_WARN("Small response text: " << text);
            }
            
            // 3. Chunk processing for analysis
            fl::size line_count = 0;
            result->process_chunks(256, [&line_count](fl::span<const fl::u8> chunk) {
                for (const fl::u8& byte : chunk) {
                    if (byte == '\n') line_count++;
                }
                return true;
            });
            FL_WARN("Found " << line_count << " lines");
            
            // 4. Binary processing if needed
            if (result->header("Content-Type").find("application/octet-stream") != fl::string::npos) {
                fl::u32 checksum = 0;
                for (const fl::u8& byte : *result) {
                    checksum = (checksum << 1) ^ byte;  // Simple checksum
                }
                FL_WARN("Binary checksum: 0x" << fl::hex << checksum);
            }
            
            // 5. Copy to buffer only when absolutely necessary
            if (need_contiguous_buffer_for_legacy_api()) {
                fl::vector<fl::u8> buffer = result->to_vector();
                legacy_api_call(buffer.data(), buffer.size());
                FL_WARN("Copied to contiguous buffer for legacy API");
            }
        }
    }
}
```

### **Complete fl::Response Interface Specification**

```cpp
namespace fl {

/// HTTP response object with memory-efficient fl::deque storage
class Response {
public:
    /// Response status and metadata
    int status_code() const { return mStatusCode; }
    bool ok() const { return mStatusCode >= 200 && mStatusCode < 300; }
    const fl::string& status_text() const { return mStatusText; }
    
    /// Response headers
    const fl::string& header(const fl::string& name) const;
    const fl::string& header(const char* name) const;
    bool has_header(const fl::string& name) const;
    bool has_header(const char* name) const;
    
    /// Get all headers as key-value pairs
    fl::span<const fl::pair<fl::string, fl::string>> headers() const;
    
    /// Response body size and properties
    fl::size size() const { return mBody.size(); }
    bool empty() const { return mBody.empty(); }
    
    /// Memory management info
    bool uses_psram() const { return mUsesPsram; }
    fl::size memory_footprint() const;  // Total memory used including headers
    
    // ========== PRIMARY INTERFACES ==========
    
    /// Text access (copy-on-write fl::string for efficient sharing)
    /// @note Creates fl::string from response data - efficient for text content
    /// @note Uses copy-on-write semantics - multiple copies share same buffer
    fl::string text() const;
    
    /// Legacy C-style text access
    /// @note Returns pointer to internal buffer - valid until Response destroyed
    /// @warning Only valid if response contains text data
    const char* c_str() const;
    fl::size text_length() const;
    
    /// Iterator-based access (most memory efficient)
    /// @note Direct iteration over response body - zero allocation
    /// @note Works with both inlined and deque storage transparently
    /// @code
    /// for (const fl::u8& byte : response) {
    ///     process_byte(byte);  // Zero allocation access
    /// }
    /// @endcode
    using const_iterator = ResponseBody::const_iterator;
    const_iterator begin() const { return mBody.begin(); }
    const_iterator end() const { return mBody.end(); }
    const_iterator cbegin() const { return mBody.cbegin(); }
    const_iterator cend() const { return mBody.cend(); }
    
    /// Chunk-based processing (ideal for large responses)
    /// @param chunk_size size of each chunk to process (typically 512-4096 bytes)
    /// @param processor function called for each chunk: bool processor(fl::span<const fl::u8> chunk)
    /// @returns true if all chunks processed, false if processor returned false
    /// @note Processes response in manageable chunks without loading all into memory
    /// @note Optimized for both small inlined and large deque storage
    /// @code
    /// response.process_chunks(1024, [](fl::span<const fl::u8> chunk) {
    ///     write_to_sd_card(chunk);  // Process 1KB at a time
    ///     return true;  // Continue processing
    /// });
    /// @endcode
    bool process_chunks(fl::size chunk_size, 
                        fl::function<bool(fl::span<const fl::u8>)> processor) const;
    
    // ========== ADVANCED INTERFACES ==========
    
    /// Direct body access (for advanced users)
    /// @note Provides direct access to underlying storage (inlined vector or deque)
    /// @warning Advanced usage only - prefer iterator or chunk interfaces
    const ResponseBody& body() const { return mBody; }
    
    /// Copy to contiguous buffer (use sparingly!)
    /// @note Creates new fl::vector with all response data
    /// @warning Potentially large allocation - use only when absolutely necessary
    /// @warning May fail on embedded systems with insufficient memory
    /// @note Optimized path for small inlined responses (direct copy)
    fl::vector<fl::u8> to_vector() const;
    
    /// Copy to existing buffer
    /// @param buffer destination buffer to copy into
    /// @param buffer_size size of destination buffer
    /// @returns number of bytes actually copied (may be less than response size)
    /// @note Safe alternative to to_vector() - no allocation
    /// @note Efficient for both small and large responses
    fl::size copy_to_buffer(fl::u8* buffer, fl::size buffer_size) const;
    
    /// Copy range to existing buffer
    /// @param start_offset byte offset to start copying from
    /// @param buffer destination buffer
    /// @param buffer_size size of destination buffer
    /// @returns number of bytes actually copied
    fl::size copy_range_to_buffer(fl::size start_offset, fl::u8* buffer, fl::size buffer_size) const;
    
    // ========== UTILITY METHODS ==========
    
    /// Find byte pattern in response
    /// @param pattern bytes to search for
    /// @returns offset of first match, or fl::string::npos if not found
    fl::size find_bytes(fl::span<const fl::u8> pattern) const;
    
    /// Find text pattern in response (treats response as text)
    /// @param text string to search for
    /// @returns offset of first match, or fl::string::npos if not found
    fl::size find_text(const fl::string& text) const;
    fl::size find_text(const char* text) const;
    
    /// Get byte at specific offset
    /// @param offset byte offset into response
    /// @returns byte value, or 0 if offset out of range
    fl::u8 at(fl::size offset) const;
    fl::u8 operator[](fl::size offset) const { return at(offset); }
    
    /// Get span of bytes at specific range
    /// @param start_offset starting byte offset
    /// @param length number of bytes to include
    /// @returns span covering the requested range (may be shorter if out of bounds)
    /// @note Span is only valid while Response object exists
    /// @note Works efficiently with both inlined and deque storage
    fl::span<const fl::u8> subspan(fl::size start_offset, fl::size length = fl::string::npos) const;
    
    /// Response validation
    bool is_valid() const { return mStatusCode > 0; }
    bool is_json() const;        // Checks Content-Type header
    bool is_text() const;        // Checks Content-Type header
    bool is_binary() const;      // Checks Content-Type header
    
    /// Storage optimization info
    bool is_inlined() const { return mBody.is_inlined(); }
    bool is_deque_storage() const { return mBody.is_deque(); }
    static constexpr fl::size INLINE_BUFFER_SIZE = 256;  ///< Size of inline buffer for small responses
    
    /// Error information
    const fl::string& error_message() const { return mErrorMessage; }
    bool has_error() const { return !mErrorMessage.empty(); }
    
    // ========== CONSTRUCTION (Internal Use) ==========
    
    /// Create empty response
    Response() = default;
    
    /// Create error response
    static Response error(int status_code, const fl::string& error_msg);
    static Response error(int status_code, const char* error_msg);
    
    /// Create successful response (used internally by HttpClient)
    static Response success(int status_code, 
                           const fl::string& status_text,
                           fl::vector<fl::pair<fl::string, fl::string>> headers,
                           ResponseBody body,
                           bool uses_psram = false);
    
    // ========== MEMORY CONFIGURATION ==========
    
    /// Memory configuration for response storage
    struct MemoryConfig {
        bool prefer_psram = true;           ///< Use PSRAM when available
        fl::size inline_threshold = 256;    ///< Use inline storage for responses smaller than this
        fl::size psram_threshold = 2048;    ///< Use PSRAM for deque storage larger than this
        fl::size max_internal_ram = 4096;   ///< Maximum internal RAM to use for responses
        bool allow_large_responses = true;  ///< Allow responses larger than max_internal_ram
    };
    
    /// Set memory configuration (affects future responses)
    static void set_default_memory_config(const MemoryConfig& config);
    static const MemoryConfig& get_default_memory_config();

private:
    // Response metadata
    int mStatusCode = 0;
    fl::string mStatusText;
    fl::vector<fl::pair<fl::string, fl::string>> mHeaders;
    fl::string mErrorMessage;
    
    // Response body - hybrid storage with small buffer optimization
    ResponseBody mBody;
    bool mUsesPsram = false;
    
    // Cached text representation (lazy-loaded)
    mutable fl::optional<fl::string> mCachedText;
    
    // Header lookup optimization
    mutable fl::unordered_map<fl::string, fl::string> mHeaderMap;
    mutable bool mHeaderMapBuilt = false;
    
    // Memory management
    static MemoryConfig sDefaultMemoryConfig;
    
    // Internal helpers
    void build_header_map() const;
    const fl::string& find_header(const fl::string& name) const;
    
    // Friends for internal construction
    friend class HttpClient;
    friend class HttpRequest;  // Internal request class
};

/// Hybrid response body storage: inlined vector for small responses, deque for large
/// @note Optimizes for the common case of small HTTP responses (≤256 bytes)
/// @note Automatically spills to PSRAM-backed deque for larger responses
class ResponseBody {
public:
    static constexpr fl::size INLINE_SIZE = 256;  ///< Inline buffer size for small responses
    
    /// Iterator type that works with both storage modes
    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = fl::u8;
        using difference_type = std::ptrdiff_t;
        using pointer = const fl::u8*;
        using reference = const fl::u8&;
        
        const_iterator() = default;
        
        // Constructor for inline storage
        const_iterator(const fl::u8* ptr) : mInlinePtr(ptr), mIsInline(true) {}
        
        // Constructor for deque storage
        const_iterator(const fl::deque<fl::u8, fl::allocator_psram<fl::u8>>::const_iterator& it) 
            : mDequeIt(it), mIsInline(false) {}
        
        reference operator*() const {
            return mIsInline ? *mInlinePtr : *mDequeIt;
        }
        
        pointer operator->() const {
            return mIsInline ? mInlinePtr : &(*mDequeIt);
        }
        
        const_iterator& operator++() {
            if (mIsInline) {
                ++mInlinePtr;
            } else {
                ++mDequeIt;
            }
            return *this;
        }
        
        const_iterator operator++(int) {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        
        bool operator==(const const_iterator& other) const {
            if (mIsInline != other.mIsInline) return false;
            return mIsInline ? (mInlinePtr == other.mInlinePtr) : (mDequeIt == other.mDequeIt);
        }
        
        bool operator!=(const const_iterator& other) const {
            return !(*this == other);
        }
        
    private:
        union {
            const fl::u8* mInlinePtr;
            fl::deque<fl::u8, fl::allocator_psram<fl::u8>>::const_iterator mDequeIt;
        };
        bool mIsInline = true;
    };
    
    /// Default constructor - starts with inline storage
    ResponseBody() : mIsInline(true), mSize(0) {
        // Initialize inline buffer
        new (&mInlineBuffer) fl::array<fl::u8, INLINE_SIZE>();
    }
    
    /// Destructor
    ~ResponseBody() {
        if (!mIsInline) {
            mDequeStorage.~deque();
        }
    }
    
    /// Copy constructor
    ResponseBody(const ResponseBody& other) : mIsInline(other.mIsInline), mSize(other.mSize) {
        if (mIsInline) {
            new (&mInlineBuffer) fl::array<fl::u8, INLINE_SIZE>(other.mInlineBuffer);
        } else {
            new (&mDequeStorage) fl::deque<fl::u8, fl::allocator_psram<fl::u8>>(other.mDequeStorage);
        }
    }
    
    /// Move constructor
    ResponseBody(ResponseBody&& other) noexcept : mIsInline(other.mIsInline), mSize(other.mSize) {
        if (mIsInline) {
            new (&mInlineBuffer) fl::array<fl::u8, INLINE_SIZE>(fl::move(other.mInlineBuffer));
        } else {
            new (&mDequeStorage) fl::deque<fl::u8, fl::allocator_psram<fl::u8>>(fl::move(other.mDequeStorage));
        }
    }
    
    /// Assignment operators
    ResponseBody& operator=(const ResponseBody& other) {
        if (this != &other) {
            this->~ResponseBody();
            new (this) ResponseBody(other);
        }
        return *this;
    }
    
    ResponseBody& operator=(ResponseBody&& other) noexcept {
        if (this != &other) {
            this->~ResponseBody();
            new (this) ResponseBody(fl::move(other));
        }
        return *this;
    }
    
    /// Add data to response body
    /// @note Automatically switches from inline to deque storage when needed
    void append(fl::span<const fl::u8> data) {
        if (mIsInline && mSize + data.size() <= INLINE_SIZE) {
            // Still fits in inline buffer
            fl::memcpy(mInlineBuffer.data() + mSize, data.data(), data.size());
            mSize += data.size();
        } else {
            // Need to switch to deque storage
            switch_to_deque();
            for (const fl::u8& byte : data) {
                mDequeStorage.push_back(byte);
            }
            mSize += data.size();
        }
    }
    
    /// Add single byte
    void push_back(fl::u8 byte) {
        if (mIsInline && mSize < INLINE_SIZE) {
            mInlineBuffer[mSize++] = byte;
        } else {
            if (mIsInline) {
                switch_to_deque();
            }
            mDequeStorage.push_back(byte);
            ++mSize;
        }
    }
    
    /// Size of response body
    fl::size size() const { return mSize; }
    bool empty() const { return mSize == 0; }
    
    /// Storage mode queries
    bool is_inlined() const { return mIsInline; }
    bool is_deque() const { return !mIsInline; }
    
    /// Iterator access
    const_iterator begin() const {
        if (mIsInline) {
            return const_iterator(mInlineBuffer.data());
        } else {
            return const_iterator(mDequeStorage.begin());
        }
    }
    
    const_iterator end() const {
        if (mIsInline) {
            return const_iterator(mInlineBuffer.data() + mSize);
        } else {
            return const_iterator(mDequeStorage.end());
        }
    }
    
    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }
    
    /// Direct access to underlying storage (advanced users)
    fl::span<const fl::u8> as_span() const {
        if (mIsInline) {
            return fl::span<const fl::u8>(mInlineBuffer.data(), mSize);
        } else {
            // For deque, we need to copy to contiguous memory - this is expensive!
            // Advanced users should prefer iterator access
            static thread_local fl::vector<fl::u8> temp_buffer;
            temp_buffer.clear();
            temp_buffer.reserve(mSize);
            for (const fl::u8& byte : mDequeStorage) {
                temp_buffer.push_back(byte);
            }
            return fl::span<const fl::u8>(temp_buffer.data(), temp_buffer.size());
        }
    }
    
    /// Get byte at offset (bounds checked)
    fl::u8 at(fl::size offset) const {
        if (offset >= mSize) return 0;
        
        if (mIsInline) {
            return mInlineBuffer[offset];
        } else {
            // For deque, we need to iterate (slower for random access)
            auto it = mDequeStorage.begin();
            fl::advance(it, offset);
            return *it;
        }
    }
    
private:
    /// Switch from inline storage to deque storage
    void switch_to_deque() {
        if (!mIsInline) return;  // Already switched
        
        // Save current inline data
        fl::array<fl::u8, INLINE_SIZE> temp_data = mInlineBuffer;
        fl::size temp_size = mSize;
        
        // Destroy inline buffer and construct deque
        mInlineBuffer.~array();
        new (&mDequeStorage) fl::deque<fl::u8, fl::allocator_psram<fl::u8>>();
        
        // Copy data from inline buffer to deque
        for (fl::size i = 0; i < temp_size; ++i) {
            mDequeStorage.push_back(temp_data[i]);
        }
        
        mIsInline = false;
        // mSize remains the same
    }
    
    /// Storage mode flag
    bool mIsInline;
    fl::size mSize;
    
    /// Union for storage - either inline buffer or deque
    union {
        fl::array<fl::u8, INLINE_SIZE> mInlineBuffer;
        fl::deque<fl::u8, fl::allocator_psram<fl::u8>> mDequeStorage;
    };
};

} // namespace fl
```

### **Memory Efficiency Analysis: fl::deque vs fl::vector**

#### **Memory Usage Comparison**

| **Scenario** | **fl::vector<fl::u8>** | **fl::deque<fl::u8> + PSRAM** | **Improvement** |
|--------------|------------------------|-------------------------------|-----------------|
| **10KB Response** | 10KB internal RAM | 10KB PSRAM chunks | **Saves 10KB internal RAM** |
| **100KB Response** | 100KB internal RAM (may OOM) | 100KB PSRAM chunks | **Prevents OOM crash** |
| **1MB Response** | Impossible (OOM) | 1MB PSRAM chunks | **Enables large downloads** |
| **Reallocation** | 2x during growth | No reallocation | **50% memory savings** |
| **Fragmentation** | High (large blocks) | Low (small chunks) | **Better memory health** |

#### **Performance Benefits on ESP32S3**

```cpp
// Real-world ESP32S3 example with 8MB PSRAM
void esp32s3_performance_demo() {
    HttpClient client;
    
    // Download 2MB dataset - impossible with fl::vector in internal RAM!
    auto future = client.get("http://data.server.com/large-dataset.bin", 120000);
    
    if (future.is_ready()) {
        auto result = future.try_result();
        if (result && result->ok()) {
            FL_WARN("Downloaded " << result->size() << " bytes to PSRAM");
            
            // Process in 4KB chunks - only 4KB in internal RAM at any time
            fl::size processed = 0;
            result->process_chunks(4096, [&processed](fl::span<const fl::u8> chunk) {
                // Process chunk in fast internal RAM
                process_sensor_data_chunk(chunk);
                processed += chunk.size();
                
                if (processed % 100000 == 0) {
                    FL_WARN("Processed " << processed << " bytes");
                }
                return true;
            });
            
            FL_WARN("Complete! Used minimal internal RAM for 2MB processing");
        }
    }
}
```

### **Integration with HTTP Client**

#### **Internal Request Management**

```cpp
namespace fl {

// Internal class used by HttpClient
class HttpRequest {
private:
    fl::string mUrl;
    fl::string mMethod;
    fl::vector<fl::pair<fl::string, fl::string>> mHeaders;
    fl::deque<fl::u8, fl::allocator_psram<fl::u8>> mResponseBuffer;  // PSRAM-backed
    Response::MemoryConfig mMemoryConfig;
    
public:
    // Configure memory usage for this request
    void set_memory_config(const Response::MemoryConfig& config) {
        mMemoryConfig = config;
        configure_response_allocator();
    }
    
    // Add response data as it arrives from network
    void append_response_data(fl::span<const fl::u8> data) {
        // Efficiently append to deque - no reallocation!
        for (const fl::u8& byte : data) {
            mResponseBuffer.push_back(byte);
        }
    }
    
    // Build final Response object
    Response build_response(int status_code, const fl::string& status_text) {
        return Response::success(
            status_code,
            status_text,
            fl::move(mHeaders),
            fl::move(mResponseBuffer),  // Transfer ownership efficiently
            mMemoryConfig.prefer_psram
        );
    }
    
private:
    void configure_response_allocator() {
        // Configure PSRAM allocator based on memory config
        if (mMemoryConfig.prefer_psram) {
            // Set up PSRAM allocator for response buffer
            mResponseBuffer = fl::deque<fl::u8, fl::allocator_psram<fl::u8>>();
        }
    }
};

} // namespace fl
```

This complete specification provides:

✅ **Simple interface** for common use cases  
✅ **Memory-efficient design** with fl::deque + PSRAM  
✅ **Multiple access patterns** for different scenarios  
✅ **Zero-allocation options** for memory-constrained environments  
✅ **Copy-on-write text access** with fl::string  
✅ **Advanced features** for power users  
✅ **ESP32 PSRAM optimization** for large responses  
✅ **Embedded-friendly design** throughout  

The `fl::deque` design enables FastLED to handle **large HTTP responses efficiently** while maintaining **simple APIs** for common use cases! 🚀

### **TransportVerify Usage Examples**

#### **1. Smart Verification (AUTO - Default Behavior)**

```cpp
void smart_verification_example() {
    // Default behavior - smart verification
    HttpClient client;  // Uses TransportVerify::AUTO by default
    
    // HTTP connections - no verification (fast and simple)
    auto http_future = client.get("http://api.example.com/data");
    
    // HTTPS to localhost/private - no verification (development friendly)
    auto local_https_future = client.get("https://localhost:8443/api");
    auto private_https_future = client.get("https://192.168.1.100:8443/api");
    auto dev_server_future = client.get("https://dev.example.local/api");
    
    // HTTPS to public servers - full verification (secure)
    auto public_https_future = client.get("https://api.github.com/user");
    
    FL_WARN("Smart verification automatically handles different scenarios:");
    FL_WARN("- HTTP: no verification (fast)");
    FL_WARN("- HTTPS localhost/private: no verification (dev-friendly)");
    FL_WARN("- HTTPS public: full verification (secure)");
}
```

#### **2. Development Configuration (NO Verification)**

```cpp
void development_setup() {
    // Development mode - disable all verification
    auto dev_client = HttpClient::create_with_transport(
        TcpTransport::create_unverified()
    );
    
    // OR configure existing client
    TcpTransport::Options dev_opts;
    dev_opts.verify = TransportVerify::NO;
    dev_opts.allow_self_signed = true;
    
    HttpClient client;
    client.configure_transport(dev_opts);
    
    // All connections work without verification
    client.get("https://self-signed.local/api");      // Works!
    client.get("https://expired-cert.local/api");     // Works!
    client.get("https://wrong-hostname.local/api");   // Works!
    
    FL_WARN("DEVELOPMENT MODE: All TLS verification disabled!");
    FL_WARN("DO NOT USE IN PRODUCTION!");
}
```

#### **3. Production Configuration (YES Verification)**

```cpp
void production_setup() {
    // Production mode - strict verification for everything
    auto prod_client = HttpClient::create_with_transport(
        TcpTransport::create_strict_verification()
    );
    
    // OR configure existing client
    TcpTransport::Options prod_opts;
    prod_opts.verify = TransportVerify::YES;
    prod_opts.verify_hostname = true;
    prod_opts.allow_self_signed = false;
    
    HttpClient client;
    client.configure_transport(prod_opts);
    
    // Only properly verified connections work
    client.get("https://api.trusted.com/data");       // Works if cert valid
    // client.get("http://api.trusted.com/data");     // Would fail! (no TLS)
    // client.get("https://localhost:8443/api");      // Would fail! (verification required)
    
    FL_WARN("PRODUCTION MODE: Strict verification for all connections");
}
```

#### **4. Environment-Specific Configuration**

```cpp
class ConfigurableClient {
private:
    HttpClient mClient;
    bool mIsProduction;
    
public:
    void initialize(bool production_mode) {
        mIsProduction = production_mode;
        
        TcpTransport::Options opts;
        
        if (production_mode) {
            // Production: strict verification
            opts.verify = TransportVerify::YES;
            opts.verify_hostname = true;
            opts.allow_self_signed = false;
            opts.limits.connection_timeout_ms = 15000;  // Longer for remote servers
            
            FL_WARN("Initialized in PRODUCTION mode");
            FL_WARN("- All connections require valid TLS certificates");
            FL_WARN("- Hostname verification enabled");
            FL_WARN("- Self-signed certificates rejected");
            
        } else {
            // Development: smart verification  
            opts.verify = TransportVerify::AUTO;
            opts.allow_self_signed = true;
            opts.limits.connection_timeout_ms = 5000;   // Faster for local dev
            
            FL_WARN("Initialized in DEVELOPMENT mode");
            FL_WARN("- Smart verification (HTTPS public servers verified)");
            FL_WARN("- Local/private HTTPS allowed without verification");
            FL_WARN("- Self-signed certificates allowed");
        }
        
        auto transport = fl::make_unique<TcpTransport>(opts);
        mClient = HttpClient::create_with_transport(fl::move(transport));
    }
    
    void test_various_endpoints() {
        // Test different types of endpoints
        
        // HTTP endpoint (always works)
        mClient.get_async("http://httpbin.org/json", [this](const Response& r) {
            FL_WARN("HTTP endpoint: " << (r.ok() ? "SUCCESS" : "FAILED"));
        });
        
        // HTTPS public endpoint (verified in production and AUTO mode)
        mClient.get_async("https://api.github.com/zen", [this](const Response& r) {
            FL_WARN("HTTPS public: " << (r.ok() ? "SUCCESS" : "FAILED"));
            if (!r.ok() && mIsProduction) {
                FL_WARN("  (May fail in production if certificate issues)");
            }
        });
        
        // HTTPS localhost (works in dev/AUTO, fails in production/YES)
        mClient.get_async("https://localhost:8443/health", [this](const Response& r) {
            FL_WARN("HTTPS localhost: " << (r.ok() ? "SUCCESS" : "FAILED"));
            if (!r.ok() && mIsProduction) {
                FL_WARN("  (Expected failure in production mode)");
            }
        });
        
        // Self-signed HTTPS (only works in development NO mode)
        mClient.get_async("https://self-signed.local/api", [this](const Response& r) {
            FL_WARN("Self-signed HTTPS: " << (r.ok() ? "SUCCESS" : "FAILED"));
            if (!r.ok()) {
                FL_WARN("  (Expected failure unless verification disabled)");
            }
        });
    }
    
    void print_verification_status() {
        auto stats = mClient.get_transport_stats();
        FL_WARN("=== Transport Verification Status ===");
        FL_WARN("Mode: " << (mIsProduction ? "PRODUCTION" : "DEVELOPMENT"));
        FL_WARN("Transport: " << stats.transport_type);
        FL_WARN("Active connections: " << stats.active_connections);
        
        // Get verification policy from transport
        // (In real implementation, you'd have a method to query this)
        FL_WARN("Verification policy: " << get_verification_policy_string());
    }
    
private:
    const char* get_verification_policy_string() const {
        if (mIsProduction) {
            return "YES (strict verification for all connections)";
        } else {
            return "AUTO (smart verification - public HTTPS verified, local/HTTP allowed)";
        }
    }
};

// Usage
ConfigurableClient client;

void setup() {
    bool is_production = false;  // Set based on your deployment
    
    #ifdef PRODUCTION_BUILD
        is_production = true;
    #endif
    
    client.initialize(is_production);
    client.test_various_endpoints();
}

void loop() {
    EVERY_N_SECONDS(60) {
        client.print_verification_status();
    }
    
    FastLED.show();
}
```

#### **5. Certificate Pinning with Verification**

```cpp
void certificate_pinning_example() {
    TcpTransport::Options opts;
    opts.verify = TransportVerify::YES;  // Enable strict verification
    opts.verify_hostname = true;
    
    // Pin specific certificates for critical APIs
    opts.pinned_certificates = {
        "b5d3c2a1e4f7g8h9i0j1k2l3m4n5o6p7q8r9s0t1u2v3w4x5y6z7a8b9c0d1e2f3",  // api.critical.com
        "f3e2d1c0b9a8g7h6i5j4k3l2m1n0o9p8q7r6s5t4u3v2w1x0y9z8a7b6c5d4e3f2"   // cdn.critical.com
    };
    
    auto transport = fl::make_unique<TcpTransport>(opts);
    auto client = HttpClient::create_with_transport(fl::move(transport));
    
    // These will only work if certificates match pinned fingerprints
    client.get_async("https://api.critical.com/data", [](const Response& r) {
        if (r.ok()) {
            FL_WARN("Pinned certificate verified successfully");
        } else {
            FL_WARN("Certificate pinning failed - potential MITM attack!");
        }
    });
}
```

### **Benefits of TransportVerify::AUTO (Default)**

**🎯 Smart Defaults for Different Environments:**

1. **🌐 Public HTTPS**: Full verification (secure by default)
2. **🏠 Local HTTPS**: No verification (development-friendly) 
3. **📡 HTTP**: No verification (appropriate for plaintext)
4. **🔒 Private Networks**: No verification (corporate environment friendly)

**✅ Perfect for:**
- **Development workflows** - works with self-signed certs on localhost
- **Production deployment** - secure by default for public APIs
- **Mixed environments** - handles internal and external APIs correctly
- **IoT devices** - flexible for local config + cloud telemetry

**⚡ Zero Configuration Required:**
```cpp
HttpClient client;  // Uses smart verification automatically!

// These all work with optimal security for each scenario:
client.get("http://localhost:8080/config");           // Fast, no verification needed
client.get("https://localhost:8443/admin");           // Local dev, no verification
client.get("https://api.github.com/user");            // Public API, full verification
client.get("https://192.168.1.100:8443/local-api");   // Private network, no verification
```

The `TransportVerify::AUTO` default provides **maximum security where it matters** while remaining **development-friendly** for local and private network scenarios! 🚀

### 2. HTTP Client with Transport Configuration

```cpp
namespace fl {

class HttpClient : public EngineEvents::Listener {
public:
    /// Create HTTP client with default TCP transport
    HttpClient() : HttpClient(fl::make_unique<TcpTransport>()) {}
    
    /// Create HTTP client with custom transport
    explicit HttpClient(fl::unique_ptr<Transport> transport) 
        : mTransport(fl::move(transport)) {}
    
    /// Destructor
    ~HttpClient();
    
    /// Factory methods for common transport configurations
    
    /// Create client optimized for local development (localhost only)
    /// @code
    /// auto client = HttpClient::create_local_client();
    /// client.get("http://localhost:8080/api");  // Fast local connections
    /// @endcode
    static fl::unique_ptr<HttpClient> create_local_client() {
        return fl::make_unique<HttpClient>(fl::make_unique<LocalTransport>());
    }
    
    /// Create client with IPv4-only transport
    /// @code
    /// auto client = HttpClient::create_ipv4_client();
    /// client.get("http://api.example.com/data");  // Forces IPv4
    /// @endcode
    static fl::unique_ptr<HttpClient> create_ipv4_client() {
        return fl::make_unique<HttpClient>(TcpTransport::create_ipv4_only());
    }
    
    /// Create client with IPv6-only transport
    /// @code
    /// auto client = HttpClient::create_ipv6_client();
    /// client.get("http://api.example.com/data");  // Forces IPv6
    /// @endcode
    static fl::unique_ptr<HttpClient> create_ipv6_client() {
        return fl::make_unique<HttpClient>(TcpTransport::create_ipv6_only());
    }
    
    /// Create client with dual-stack transport (IPv6 preferred, IPv4 fallback)
    /// @code
    /// auto client = HttpClient::create_dual_stack_client();
    /// client.get("http://api.example.com/data");  // Try IPv6 first, fallback to IPv4
    /// @endcode
    static fl::unique_ptr<HttpClient> create_dual_stack_client() {
        return fl::make_unique<HttpClient>(TcpTransport::create_dual_stack());
    }
    
    /// Create client with custom transport configuration
    /// @code
    /// TcpTransport::Options opts;
    /// opts.ip_version = IpVersion::IPV4_FIRST;
    /// opts.local_address = "192.168.1.100";  // Bind to specific local IP
    /// opts.limits.max_connections_per_host = 2;
    /// opts.limits.keepalive_timeout_ms = 30000;
    /// 
    /// auto transport = fl::make_unique<TcpTransport>(opts);
    /// auto client = HttpClient::create_with_transport(fl::move(transport));
    /// @endcode
    static fl::unique_ptr<HttpClient> create_with_transport(fl::unique_ptr<Transport> transport) {
        return fl::make_unique<HttpClient>(fl::move(transport));
    }
    
    /// Configure transport options for existing client
    /// @code
    /// TcpTransport::Options opts;
    /// opts.ip_version = IpVersion::IPV6_ONLY;
    /// opts.limits.connection_timeout_ms = 5000;
    /// client.configure_transport(opts);
    /// @endcode
    void configure_transport(const TcpTransport::Options& options) {
        if (auto* tcp_transport = dynamic_cast<TcpTransport*>(mTransport.get())) {
            tcp_transport->set_options(options);
        }
    }
    
    /// Get transport statistics
    /// @code
    /// auto stats = client.get_transport_stats();
    /// FL_WARN("Active connections: " << stats.active_connections);
    /// FL_WARN("Total created: " << stats.total_created);
    /// @endcode
    struct TransportStats {
        fl::size active_connections;
        fl::size total_connections_created;
        fl::string transport_type;
        fl::optional<TcpTransport::PoolStats> pool_stats; // Only for TcpTransport
    };
    TransportStats get_transport_stats() const {
        TransportStats stats;
        stats.active_connections = mTransport->get_active_connections();
        stats.total_connections_created = mTransport->get_total_connections_created();
        
        if (auto* tcp_transport = dynamic_cast<TcpTransport*>(mTransport.get())) {
            stats.transport_type = "TcpTransport";
            stats.pool_stats = tcp_transport->get_pool_stats();
        } else if (dynamic_cast<LocalTransport*>(mTransport.get())) {
            stats.transport_type = "LocalTransport";
        } else {
            stats.transport_type = "CustomTransport";
        }
        
        return stats;
    }
    
    /// Close all transport connections (useful for network changes)
    /// @code
    /// // WiFi reconnected, close old connections
    /// client.close_transport_connections();
    /// @endcode
    void close_transport_connections() {
        mTransport->close_all_connections();
    }

    // ========== HTTP API (unchanged - same simple interface) ==========
    
    /// HTTP GET request with customizable timeout and headers (returns future)
    fl::future<Response> get(const char* url, 
                             uint32_t timeout_ms = 5000,
                             fl::span<const fl::pair<fl::string, fl::string>> headers = {});
    
    /// HTTP POST request with customizable timeout and headers (returns future)
    fl::future<Response> post(const char* url, 
                              fl::span<const fl::u8> data,
                              uint32_t timeout_ms = 5000,
                              fl::span<const fl::pair<fl::string, fl::string>> headers = {});
    
    /// Async GET request with callback
    void get_async(const char* url, 
                   fl::function<void(const Response&)> callback,
                   uint32_t timeout_ms = 5000,
                   fl::span<const fl::pair<fl::string, fl::string>> headers = {});
    
    /// Async POST request with callback
    void post_async(const char* url, 
                    fl::span<const fl::u8> data,
                    fl::function<void(const Response&)> callback,
                    uint32_t timeout_ms = 5000,
                    fl::span<const fl::pair<fl::string, fl::string>> headers = {});
    
    // ... (all other HTTP methods remain unchanged)

private:
    fl::unique_ptr<Transport> mTransport;
    // ... (rest of private members)
};

} // namespace fl
```

### 4. Network Event Pumping Integration

```cpp
namespace fl {

class NetworkEventPump : public EngineEvents::Listener {
public:
    static NetworkEventPump& instance() {
        static NetworkEventPump pump;
        return pump;
    }
    
    void register_client(HttpClient* client) {
        auto it = fl::find(mClients.begin(), mClients.end(), client);
        if (it == mClients.end()) {
            mClients.push_back(client);
        }
    }
    
    void unregister_client(HttpClient* client) {
        auto it = fl::find(mClients.begin(), mClients.end(), client);
        if (it != mClients.end()) {
            mClients.erase(it);
        }
    }
    
    // Manual pumping for users who don't want EngineEvents integration
    static void pump_all_clients() {
        instance().pump_clients();
    }

private:
    NetworkEventPump() {
        EngineEvents::addListener(this, 10); // Low priority, run after UI
    }
    
    ~NetworkEventPump() {
        EngineEvents::removeListener(this);
    }
    
    // EngineEvents::Listener interface - pump after frame is drawn
    void onEndFrame() override {
        pump_clients();
    }
    
    void pump_clients() {
        for (auto* client : mClients) {
            client->pump_network_events();
        }
    }
    
    fl::vector<HttpClient*> mClients;
};

} // namespace fl
```

## Usage Examples

### 1. **Simplified API with Defaults (EASIEST)**

```cpp
#include "fl/networking/http_client.h"

using namespace fl;

HttpClient client;

void setup() {
    // SIMPLEST usage - just the URL! (uses 5s timeout, no headers)
    auto health_future = client.get("http://localhost:8899/health");
    
    // Simple POST with just URL and data
    fl::vector<fl::u8> sensor_data = get_sensor_readings();
    auto upload_future = client.post("http://localhost:8899/upload", sensor_data);
    
    // Add custom timeout when needed
    auto slow_api_future = client.get("http://slow-api.com/data", 15000);
    
    // Add headers when needed (using default 5s timeout)
    auto auth_future = client.get("http://api.weather.com/current", 5000, {
        {"Authorization", "Bearer weather_api_key"},
        {"Accept", "application/json"},
        {"User-Agent", "WeatherStation/1.0"}
    });
    
    // Store futures for checking in loop()
    pending_futures.push_back(fl::move(health_future));
    pending_futures.push_back(fl::move(upload_future));
    pending_futures.push_back(fl::move(slow_api_future));
    pending_futures.push_back(fl::move(auth_future));
}

void loop() {
    // Check all pending requests
    for (auto it = pending_futures.begin(); it != pending_futures.end();) {
        if (it->is_ready()) {
            auto result = it->try_result();
            if (result && result->ok()) {
                FL_WARN("Success: " << result->text());
            }
            it = pending_futures.erase(it);
        } else {
            ++it;
        }
    }
    
    FastLED.show();
}
```

### 2. **Callback API with Defaults**

```cpp
void setup() {
    // Ultra-simple callback API - just URL and callback!
    client.get_async("http://localhost:8899/status", [](const Response& response) {
        if (response.ok()) {
            FL_WARN("Status: " << response.text());
        }
    });
    
    // Simple POST with just URL, data, and callback
    fl::vector<fl::u8> telemetry = collect_telemetry();
    client.post_async("http://localhost:8899/telemetry", telemetry, [](const Response& response) {
        FL_WARN("Telemetry upload: " << response.status_code());
    });
    
    // Add headers when needed (using default timeout)
    client.get_async("http://api.example.com/user", [](const Response& response) {
        if (response.ok()) {
            FL_WARN("User data: " << response.text());
        }
    }, 5000, {
        {"Authorization", "Bearer user_token"},
        {"Accept", "application/json"}
    });
    
    // Custom timeout with headers
    client.post_async("http://slow-api.com/upload", telemetry, [](const Response& response) {
        FL_WARN("Slow upload: " << response.status_code());
    }, 20000, {
        {"Content-Type", "application/octet-stream"},
        {"X-Telemetry-Version", "1.0"}
    });
}

void loop() {
    // Callbacks fire automatically via EngineEvents
    FastLED.show();
}
```

### 3. **Mixed API Usage**

```cpp
void setup() {
    // No headers - simplest case
    auto simple_future = client.get("http://httpbin.org/json");
    
    // With headers using vector (when you need to build headers dynamically)
    fl::vector<fl::pair<fl::string, fl::string>> dynamic_headers;
    dynamic_headers.push_back({"Authorization", "Bearer " + get_current_token()});
    if (device_has_location()) {
        dynamic_headers.push_back({"X-Location", get_location_string()});
    }
    auto dynamic_future = client.get("http://api.example.com/location", dynamic_headers);
    
    // With headers using initializer list (when headers are known at compile time)
    auto static_future = client.get("http://api.example.com/config", {
        {"Authorization", "Bearer config_token"},
        {"Accept", "application/json"},
        {"Cache-Control", "no-cache"}
    });
}
```

### 4. **Real-World IoT Example**

```cpp
class IoTDevice {
private:
    HttpClient mClient;
    fl::string mDeviceToken;
    fl::string mDeviceId;
    
public:
    void authenticate(const char* token) { mDeviceToken = token; }
    void setDeviceId(const char* id) { mDeviceId = id; }
    
    void upload_sensor_data(fl::span<const fl::u8> data) {
        // Simple local upload (no auth needed)
        if (use_local_server) {
            auto future = mClient.post("http://192.168.1.100:8080/sensors", data);
            sensor_upload_future = fl::move(future);
            return;
        }
        
        // Cloud upload with custom timeout and auth headers
        auto future = mClient.post("https://iot.example.com/sensors", data, 10000, {
            {"Authorization", "Bearer " + mDeviceToken},
            {"Content-Type", "application/octet-stream"},
            {"X-Device-ID", mDeviceId},
            {"X-Timestamp", fl::to_string(millis())}
        });
        
        sensor_upload_future = fl::move(future);
    }
    
    void get_device_config() {
        // Local config (simple)
        if (use_local_server) {
            auto future = mClient.get("http://192.168.1.100:8080/config");
            config_future = fl::move(future);
            return;
        }
        
        // Cloud config with auth
        auto future = mClient.get("https://iot.example.com/device/config", 5000, {
            {"Authorization", "Bearer " + mDeviceToken},
            {"X-Device-ID", mDeviceId}
        });
        
        config_future = fl::move(future);
    }
    
    void ping_health_check() {
        // Simplest possible API usage!
        auto future = mClient.get("http://localhost:8899/health");
        health_future = fl::move(future);
    }
    
    void check_responses() {
        if (sensor_upload_future.is_ready()) {
            auto result = sensor_upload_future.try_result();
            FL_WARN("Sensor upload: " << (result && result->ok() ? "OK" : "FAILED"));
            sensor_upload_future = fl::future<Response>();
        }
        
        if (config_future.is_ready()) {
            auto result = config_future.try_result();
            if (result && result->ok()) {
                FL_WARN("Config: " << result->text());
                // Parse and apply config...
            }
            config_future = fl::future<Response>();
        }
        
        if (health_future.is_ready()) {
            auto result = health_future.try_result();
            FL_WARN("Health: " << (result && result->ok() ? "UP" : "DOWN"));
            health_future = fl::future<Response>();
        }
    }
    
private:
    fl::future<Response> sensor_upload_future;
    fl::future<Response> config_future;
    fl::future<Response> health_future;
    bool use_local_server = true; // Start with local, fall back to cloud
};

// Usage
IoTDevice device;

void setup() {
    device.authenticate("your_device_token");
    device.setDeviceId("fastled-sensor-001");
    
    // Health check every 10 seconds
    EVERY_N_SECONDS(10) {
        device.ping_health_check();
    }
    
    // Upload sensor reading every 30 seconds
    EVERY_N_SECONDS(30) {
        fl::vector<fl::u8> sensor_data = {temperature, humidity, light_level};
        device.upload_sensor_data(sensor_data);
    }
    
    // Get updated config every 5 minutes
    EVERY_N_SECONDS(300) {
        device.get_device_config();
    }
}

void loop() {
    device.check_responses();
    FastLED.show();
}
```

### 5. **Advanced: Multiple API Endpoints**

```cpp
class WeatherStationAPI {
private:
    HttpClient mClient;
    
public:
    void send_weather_data(float temp, float humidity, float pressure) {
        fl::vector<uint8_t> data;
        data.push_back(static_cast<uint8_t>(temp));
        data.push_back(static_cast<uint8_t>(humidity));
        data.push_back(static_cast<uint8_t>(pressure));
        
        // Multiple APIs with different headers - all using initializer lists!
        
        // Send to primary weather service
        mClient.post_async("https://weather.primary.com/upload", data, {
            {"Authorization", "Bearer primary_key"},
            {"Content-Type", "application/octet-stream"},
            {"X-Station-ID", "station-001"}
        }, [](const Response& response) {
            FL_WARN("Primary upload: " << response.status_code());
        });
        
        // Send to backup weather service  
        mClient.post_async("https://weather.backup.com/data", data, {
            {"API-Key", "backup_api_key"},
            {"Content-Type", "application/octet-stream"},
            {"Station-Name", "FastLED Weather Station"}
        }, [](const Response& response) {
            FL_WARN("Backup upload: " << response.status_code());
        });
        
        // Send to local logger
        mClient.post_async("http://192.168.1.100/log", data, {
            {"Content-Type", "application/octet-stream"}
        }, [](const Response& response) {
            FL_WARN("Local log: " << response.status_code());
        });
    }
};
```

### 6. **Streaming Downloads (Memory-Efficient)**

```cpp
class DataLogger {
private:
    HttpClient mClient;
    fl::size mTotalBytes = 0;
    
public:
    void download_large_dataset() {
        // Download 100MB dataset with only 1KB RAM buffer
        mTotalBytes = 0;
        
        mClient.get_stream_async("https://api.example.com/large-dataset", {
            {"Authorization", "Bearer " + api_token},
            {"Accept", "application/octet-stream"}
        }, 
        // Chunk callback - called for each piece of data
        [this](fl::span<const uint8_t> chunk, bool is_final) -> bool {
            // Process chunk immediately (only ~1KB in memory at a time)
            process_data_chunk(chunk);
            
            mTotalBytes += chunk.size();
            
            if (mTotalBytes % 10000 == 0) {
                FL_WARN("Downloaded: " << mTotalBytes << " bytes");
            }
            
            if (is_final) {
                FL_WARN("Download complete! Total: " << mTotalBytes << " bytes");
            }
            
            // Return true to continue, false to abort
            return true;
        },
        // Complete callback - called when done or error
        [this](int status_code, const char* error_msg) {
            if (status_code == 200) {
                FL_WARN("Stream completed successfully");
                finalize_data_processing();
            } else {
                FL_WARN("Stream error " << status_code << ": " << error_msg);
                cleanup_partial_data();
            }
        });
    }
    
private:
    void process_data_chunk(fl::span<const uint8_t> chunk) {
        // Write directly to SD card, process incrementally, etc.
        // Only this small chunk is in RAM at any time!
        for (const uint8_t& byte : chunk) {
            // Process each byte...
            analyze_sensor_data(byte);
        }
    }
    
    void finalize_data_processing() {
        FL_WARN("All data processed with minimal RAM usage!");
    }
    
    void cleanup_partial_data() {
        FL_WARN("Cleaning up after failed download");
    }
};
```

### 7. **Streaming Uploads (Sensor Data Logging)**

```cpp
class SensorDataUploader {
private:
    HttpClient mClient;
    fl::size mDataIndex = 0;
    fl::vector<uint8_t> mSensorHistory; // Large accumulated data
    
public:
    void upload_accumulated_data() {
        mDataIndex = 0;
        
        mClient.post_stream("https://api.example.com/sensor-bulk", {
            {"Content-Type", "application/octet-stream"},
            {"Authorization", "Bearer " + upload_token},
            {"X-Data-Source", "weather-station-001"}
        },
        // Data provider callback - called when client needs more data
        [this](fl::span<uint8_t> buffer) -> fl::size {
            fl::size remaining = mSensorHistory.size() - mDataIndex;
            fl::size to_copy = fl::min(buffer.size(), remaining);
            
            if (to_copy > 0) {
                // Copy next chunk into the provided buffer
                fl::memcpy(buffer.data(), 
                          mSensorHistory.data() + mDataIndex, 
                          to_copy);
                mDataIndex += to_copy;
                
                FL_WARN("Uploaded chunk: " << to_copy << " bytes");
            }
            
            // Return 0 when no more data (signals end of stream)
            return to_copy;
        },
        // Complete callback
        [this](int status_code, const char* error_msg) {
            if (status_code == 200) {
                FL_WARN("All sensor data uploaded successfully!");
                mSensorHistory.clear(); // Free memory
            } else {
                FL_WARN("Upload failed: " << status_code << " - " << error_msg);
                // Could retry later...
            }
        });
    }
    
    void add_sensor_reading(float temperature, float humidity) {
        // Accumulate data over time
        uint8_t temp_byte = static_cast<uint8_t>(temperature);
        uint8_t humidity_byte = static_cast<uint8_t>(humidity);
        
        mSensorHistory.push_back(temp_byte);
        mSensorHistory.push_back(humidity_byte);
        
        // Upload when we have enough data
        if (mSensorHistory.size() > 1000) {
            upload_accumulated_data();
        }
    }
};
```

### 8. **Real-Time Data Processing Stream**

```cpp
class RealTimeProcessor {
private:
    HttpClient mClient;
    fl::vector<float> mMovingAverage;
    fl::size mWindowSize = 100;
    
public:
    void start_realtime_stream() {
        // Connect to real-time sensor data stream
        mClient.get_stream_async("https://api.realtime.com/sensor-feed", {
            {"Authorization", "Bearer realtime_token"},
            {"Accept", "application/octet-stream"},
            {"X-Stream-Type", "continuous"}
        },
        // Process each chunk of real-time data
        [this](fl::span<const uint8_t> chunk, bool is_final) -> bool {
            // Convert binary data to sensor readings
            for (fl::size i = 0; i + 3 < chunk.size(); i += 4) {
                float sensor_value = parse_float_from_bytes(chunk.subspan(i, 4));
                
                // Update moving average
                mMovingAverage.push_back(sensor_value);
                if (mMovingAverage.size() > mWindowSize) {
                    mMovingAverage.erase(mMovingAverage.begin());
                }
                
                // Process in real-time
                process_sensor_value(sensor_value);
                
                // Update LED display based on current average
                update_led_visualization();
            }
            
            // Continue streaming unless we want to stop
            return !should_stop_stream();
        },
        // Handle stream completion or errors
        [](int status_code, const char* error_msg) {
            if (status_code != 200) {
                FL_WARN("Stream error: " << status_code << " - " << error_msg);
                // Could automatically reconnect here
            }
        });
    }
    
private:
    float parse_float_from_bytes(fl::span<const uint8_t> bytes) {
        // Convert 4 bytes to float (handle endianness)
        union { uint32_t i; float f; } converter;
        converter.i = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
        return converter.f;
    }
    
    void process_sensor_value(float value) {
        // Real-time processing logic
        if (value > threshold) {
            trigger_alert();
        }
    }
    
    void update_led_visualization() {
        float avg = calculate_average();
        // Update FastLED display based on current data
        update_leds_with_value(avg);
    }
    
    bool should_stop_stream() {
        // Logic to determine when to stop streaming
        return false; // Run continuously in this example
    }
};
```

### 9. **Secure HTTPS Communication**

```cpp
class SecureIoTDevice {
private:
    HttpClient mClient;
    fl::string mDeviceId;
    fl::string mApiToken;
    
public:
    void setup_security() {
        // Configure TLS for maximum security
        HttpClient::TlsConfig tls_config;
        tls_config.verify_certificates = true;      // Always verify server certs
        tls_config.verify_hostname = true;          // Verify hostname matches cert
        tls_config.allow_self_signed = false;       // No self-signed certs
        tls_config.require_tls_v12_or_higher = true; // Modern TLS only
        mClient.set_tls_config(tls_config);
        
        // Load trusted root certificates
        int cert_count = mClient.load_ca_bundle(HttpClient::CA_BUNDLE_IOT_COMMON);
        FL_WARN("Loaded " << cert_count << " trusted certificates");
        
        // Pin certificate for critical API endpoint (optional, highest security)
        mClient.pin_certificate("api.critical-iot.com", 
                                "b5d3c2a1e4f7g8h9i0j1k2l3m4n5o6p7q8r9s0t1u2v3w4x5y6z7a8b9c0d1e2f3");
        
        // Save certificates to persistent storage
        mClient.save_certificates("/spiffs/ca_certs.bin");
    }
    
    void send_secure_telemetry() {
        fl::vector<uint8_t> encrypted_data = encrypt_sensor_readings();
        
        // All HTTPS calls automatically use TLS with configured security
        auto future = mClient.post("https://api.secure-iot.com/telemetry", encrypted_data, {
            {"Authorization", "Bearer " + mApiToken},
            {"Content-Type", "application/octet-stream"},
            {"X-Device-ID", mDeviceId},
            {"X-Security-Level", "high"}
        });
        
        // Check TLS connection details
        if (future.is_ready()) {
            auto result = future.try_result();
            if (result) {
                auto tls_info = mClient.get_last_tls_info();
                FL_WARN("TLS Version: " << tls_info.tls_version);
                FL_WARN("Cipher: " << tls_info.cipher_suite);
                FL_WARN("Cert Verified: " << (tls_info.certificate_verified ? "YES" : "NO"));
                FL_WARN("Hostname Verified: " << (tls_info.hostname_verified ? "YES" : "NO"));
            }
        }
    }
    
    void handle_certificate_update() {
        // Download and verify new certificate bundle
        mClient.get_stream_async("https://ca-updates.iot-service.com/latest-certs", {
            {"Authorization", "Bearer " + mApiToken},
            {"Accept", "application/x-pem-file"}
        },
        [this](fl::span<const uint8_t> chunk, bool is_final) -> bool {
            // Process certificate data as it arrives
            process_certificate_chunk(chunk);
            
            if (is_final) {
                // Validate and install new certificates
                if (validate_certificate_bundle()) {
                    mClient.clear_certificates();
                    install_new_certificates();
                    mClient.save_certificates("/spiffs/ca_certs.bin");
                    FL_WARN("Certificate bundle updated successfully");
                } else {
                    FL_WARN("Certificate validation failed - keeping old certificates");
                }
            }
            return true;
        },
        [](int status, const char* msg) {
            FL_WARN("Certificate update complete: " << status);
        });
    }
    
private:
    fl::vector<uint8_t> encrypt_sensor_readings() {
        // Your encryption logic here
        return fl::vector<uint8_t>{1, 2, 3, 4}; // Placeholder
    }
    
    void process_certificate_chunk(fl::span<const uint8_t> chunk) {
        // Accumulate certificate data
    }
    
    bool validate_certificate_bundle() {
        // Verify certificate signatures, validity dates, etc.
        return true; // Placeholder
    }
    
    void install_new_certificates() {
        // Install validated certificates
    }
};
```

### 10. **Development vs Production Security**

```cpp
class ConfigurableSecurityClient {
private:
    HttpClient mClient;
    bool mDevelopmentMode;
    
public:
    void setup(bool development_mode = false) {
        mDevelopmentMode = development_mode;
        
        HttpClient::TlsConfig tls_config;
        
        if (development_mode) {
            // Development settings - more permissive
            tls_config.verify_certificates = false;     // Allow dev servers
            tls_config.verify_hostname = false;         // Allow localhost
            tls_config.allow_self_signed = true;        // Allow self-signed certs
            tls_config.require_tls_v12_or_higher = false; // Allow older TLS for testing
            
            FL_WARN("DEVELOPMENT MODE: TLS verification disabled!");
            FL_WARN("DO NOT USE IN PRODUCTION!");
            
        } else {
            // Production settings - maximum security
            tls_config.verify_certificates = true;
            tls_config.verify_hostname = true;
            tls_config.allow_self_signed = false;
            tls_config.require_tls_v12_or_higher = true;
            
            // Load production certificate bundle
            mClient.load_ca_bundle(HttpClient::CA_BUNDLE_MOZILLA);
            
            // Pin critical certificates
            pin_production_certificates();
            
            FL_WARN("PRODUCTION MODE: Full TLS verification enabled");
        }
        
        mClient.set_tls_config(tls_config);
    }
    
    void test_api_connection() {
        const char* api_url = mDevelopmentMode ? 
            "https://localhost:8443/api/test" :      // Dev server
            "https://api.production.com/test";       // Production server
            
        mClient.get_async(api_url, {
            {"Authorization", "Bearer " + get_api_token()},
            {"User-Agent", "FastLED-Device/1.0"}
        }, [this](const Response& response) {
            if (response.ok()) {
                auto tls_info = mClient.get_last_tls_info();
                FL_WARN("API test successful");
                FL_WARN("Security level: " << (mDevelopmentMode ? "DEV" : "PRODUCTION"));
                FL_WARN("TLS: " << tls_info.tls_version << " with " << tls_info.cipher_suite);
            } else {
                FL_WARN("API test failed: " << response.status_code());
                if (!mDevelopmentMode) {
                    FL_WARN("Check certificate configuration");
                }
            }
        });
    }
    
private:
    void pin_production_certificates() {
        // Pin certificates for critical production services
        mClient.pin_certificate("api.production.com", 
                                "a1b2c3d4e5f6...production_cert_sha256");
        mClient.pin_certificate("cdn.production.com", 
                                "f6e5d4c3b2a1...cdn_cert_sha256");
    }
    
    fl::string get_api_token() {
        return mDevelopmentMode ? "dev_token_123" : "prod_token_xyz";
    }
};
```

### 11. **Certificate Management & Updates**

```cpp
class CertificateManager {
private:
    HttpClient* mClient;
    fl::string mCertStoragePath;
    
public:
    CertificateManager(HttpClient* client, const char* storage_path = "/spiffs/certs.bin") 
        : mClient(client), mCertStoragePath(storage_path) {}
    
    void initialize() {
        // Try loading saved certificates first
        int loaded = mClient->load_certificates(mCertStoragePath.c_str());
        
        if (loaded == 0) {
            FL_WARN("No saved certificates found, loading defaults");
            load_default_certificates();
        } else {
            FL_WARN("Loaded " << loaded << " certificates from storage");
            
            // Check if certificates need updating (once per day)
            EVERY_N_HOURS(24) {
                check_for_certificate_updates();
            }
        }
        
        // Display certificate statistics
        print_certificate_stats();
    }
    
    void load_default_certificates() {
        // Load minimal set for common IoT services
        int count = mClient->load_ca_bundle(HttpClient::CA_BUNDLE_IOT_COMMON);
        FL_WARN("Loaded " << count << " default certificates");
        
        // Add custom root CA if needed
        add_custom_root_ca();
        
        // Save for next boot
        mClient->save_certificates(mCertStoragePath.c_str());
    }
    
    void add_custom_root_ca() {
        // Add your organization's root CA
        const char* custom_ca = R"(
-----BEGIN CERTIFICATE-----
MIIDXTCCAkWgAwIBAgIJAKX1+... (your custom root CA)
...certificate content...
-----END CERTIFICATE-----
)";
        
        if (mClient->add_root_certificate(custom_ca, "Company Root CA")) {
            FL_WARN("Added custom root CA");
        } else {
            FL_WARN("Failed to add custom root CA");
        }
    }
    
    void check_for_certificate_updates() {
        FL_WARN("Checking for certificate updates...");
        
        // Use HTTPS to download certificate updates (secured by existing certs)
        mClient->get_async("https://ca-updates.yourservice.com/latest", {
            {"User-Agent", "FastLED-CertUpdater/1.0"},
            {"Accept", "application/json"}
        }, [this](const Response& response) {
            if (response.ok()) {
                // Parse JSON response to check for updates
                if (parse_update_info(response.text())) {
                    download_certificate_updates();
                }
            } else {
                FL_WARN("Certificate update check failed: " << response.status_code());
            }
        });
    }
    
    void download_certificate_updates() {
        FL_WARN("Downloading certificate updates...");
        
        fl::vector<uint8_t> cert_data;
        
        mClient->get_stream_async("https://ca-updates.yourservice.com/bundle.pem", {
            {"Authorization", "Bearer " + get_update_token()}
        },
        [&cert_data](fl::span<const uint8_t> chunk, bool is_final) -> bool {
            // Accumulate certificate data
            for (const uint8_t& byte : chunk) {
                cert_data.push_back(byte);
            }
            
            if (is_final) {
                FL_WARN("Downloaded " << cert_data.size() << " bytes of certificate data");
            }
            return true;
        },
        [this, &cert_data](int status, const char* msg) {
            if (status == 200) {
                process_certificate_update(cert_data);
            } else {
                FL_WARN("Certificate download failed: " << status << " - " << msg);
            }
        });
    }
    
    void print_certificate_stats() {
        auto stats = mClient->get_certificate_stats();
        FL_WARN("Certificate Statistics:");
        FL_WARN("  Certificates: " << stats.certificate_count);
        FL_WARN("  Total size: " << stats.total_certificate_size << " bytes");
        FL_WARN("  Pinned certs: " << stats.pinned_certificate_count);
        FL_WARN("  Capacity: " << stats.max_certificate_capacity);
    }
    
private:
    bool parse_update_info(const char* json_response) {
        // Parse JSON to check version, availability, etc.
        // Return true if update is needed
        return false; // Placeholder
    }
    
    fl::string get_update_token() {
        // Return authentication token for certificate updates
        return "cert_update_token_123";
    }
    
    void process_certificate_update(const fl::vector<uint8_t>& cert_data) {
        // Validate signature, parse certificates, update store
        FL_WARN("Processing certificate update...");
        
        // For demo, just save the new certificates
        // In practice, you'd validate signatures, check dates, etc.
        if (validate_certificate_signature(cert_data)) {
            backup_current_certificates();
            install_new_certificates(cert_data);
            mClient->save_certificates(mCertStoragePath.c_str());
            FL_WARN("Certificates updated successfully");
        } else {
            FL_WARN("Certificate validation failed - update rejected");
        }
    }
    
    bool validate_certificate_signature(const fl::vector<uint8_t>& cert_data) {
        // Verify certificate bundle signature
        return true; // Placeholder
    }
    
    void backup_current_certificates() {
        // Backup current certificates before updating
        fl::string backup_path = mCertStoragePath + ".backup";
        mClient->save_certificates(backup_path.c_str());
    }
    
    void install_new_certificates(const fl::vector<uint8_t>& cert_data) {
        // Clear existing and install new certificates
        mClient->clear_certificates();
        
        // Parse and add each certificate from the bundle
        // (In practice, you'd parse the PEM format)
        
        FL_WARN("New certificates installed");
    }
};
```

## TLS Implementation Details

### **Platform-Specific TLS Support**

#### **ESP32: mbedTLS Integration**

```cpp
#ifdef ESP32
// Internal implementation uses ESP-IDF mbedTLS
class ESP32TlsTransport : public TlsTransport {
    mbedtls_ssl_context ssl_ctx;
    mbedtls_net_context net_ctx;
    mbedtls_ssl_config ssl_config;
    mbedtls_x509_crt ca_chain;
    
public:
    bool connect(const char* hostname, int port) override {
        // Use ESP32 mbedTLS for TLS connections
        mbedtls_ssl_init(&ssl_ctx);
        mbedtls_ssl_config_init(&ssl_config);
        mbedtls_x509_crt_init(&ca_chain);
        
        // Configure for TLS client
        mbedtls_ssl_config_defaults(&ssl_config, MBEDTLS_SSL_IS_CLIENT,
                                   MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
        
        // Set CA certificates
        mbedtls_ssl_conf_ca_chain(&ssl_config, &ca_chain, nullptr);
        
        // Enable hostname verification
        mbedtls_ssl_conf_authmode(&ssl_config, MBEDTLS_SSL_VERIFY_REQUIRED);
        
        return perform_handshake(hostname, port);
    }
};
#endif
```

#### **WebAssembly: Browser TLS**

```cpp
#ifdef __EMSCRIPTEN__
// Use browser's native TLS implementation
class EmscriptenTlsTransport : public TlsTransport {
public:
    bool connect(const char* hostname, int port) override {
        // Browser handles TLS automatically for HTTPS URLs
        // Certificate validation done by browser's certificate store
        
        EM_ASM({
            // JavaScript code to handle TLS connection
            const url = UTF8ToString($0) + ":" + $1;
            console.log("Connecting to: " + url);
            
            // Browser's fetch() API handles TLS automatically
        }, hostname, port);
        
        return true; // Browser handles all TLS details
    }
    
    // Certificate pinning via browser APIs
    void pin_certificate(const char* hostname, const char* sha256) override {
        EM_ASM({
            const host = UTF8ToString($0);
            const fingerprint = UTF8ToString($1);
            
            // Use browser's SubResource Integrity or other mechanisms
            console.log("Pinning certificate for " + host + ": " + fingerprint);
        }, hostname, sha256);
    }
};
#endif
```

#### **Native Platforms: OpenSSL/mbedTLS**

```cpp
#if !defined(ESP32) && !defined(__EMSCRIPTEN__)
// Use system OpenSSL or bundled mbedTLS
class NativeTlsTransport : public TlsTransport {
    SSL_CTX* ssl_ctx = nullptr;
    SSL* ssl = nullptr;
    X509_STORE* cert_store = nullptr;
    
public:
    bool connect(const char* hostname, int port) override {
        // Initialize OpenSSL
        SSL_library_init();
        ssl_ctx = SSL_CTX_new(TLS_client_method());
        
        // Load CA certificates
        SSL_CTX_set_cert_store(ssl_ctx, cert_store);
        
        // Enable hostname verification
        SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, nullptr);
        
        return perform_ssl_handshake(hostname, port);
    }
};
#endif
```

### **Memory Management for Certificates**

#### **Compressed Certificate Storage**

```cpp
class CompressedCertificateStore {
private:
    struct CertificateEntry {
        fl::string name;
        fl::vector<uint8_t> compressed_cert;
        fl::string sha256_fingerprint;
        uint32_t expiry_timestamp;
    };
    
    fl::vector<CertificateEntry> mCertificates;
    fl::size mMaxStorageSize = 32768; // 32KB default
    
public:
    bool add_certificate(const char* pem_cert, const char* name) {
        // Parse PEM certificate
        auto cert_der = parse_pem_to_der(pem_cert);
        
        // Compress using simple RLE or zlib if available
        auto compressed = compress_certificate(cert_der);
        
        // Check if we have space
        if (get_total_size() + compressed.size() > mMaxStorageSize) {
            FL_WARN("Certificate store full, removing oldest");
            remove_oldest_certificate();
        }
        
        CertificateEntry entry;
        entry.name = name ? name : "Unknown";
        entry.compressed_cert = fl::move(compressed);
        entry.sha256_fingerprint = calculate_sha256(cert_der);
        entry.expiry_timestamp = extract_expiry_date(cert_der);
        
        mCertificates.push_back(fl::move(entry));
        return true;
    }
    
    fl::vector<uint8_t> get_certificate(const fl::string& fingerprint) {
        for (const auto& entry : mCertificates) {
            if (entry.sha256_fingerprint == fingerprint) {
                return decompress_certificate(entry.compressed_cert);
            }
        }
        return fl::vector<uint8_t>();
    }
    
private:
    fl::vector<uint8_t> compress_certificate(const fl::vector<uint8_t>& cert_der) {
        // Simple compression for certificates (they compress well)
        return cert_der; // Placeholder - use RLE or zlib
    }
    
    fl::vector<uint8_t> decompress_certificate(const fl::vector<uint8_t>& compressed) {
        return compressed; // Placeholder
    }
    
    fl::size get_total_size() const {
        fl::size total = 0;
        for (const auto& cert : mCertificates) {
            total += cert.compressed_cert.size();
        }
        return total;
    }
    
    void remove_oldest_certificate() {
        if (!mCertificates.empty()) {
            mCertificates.erase(mCertificates.begin());
        }
    }
};
```

### **TLS Error Handling**

```cpp
enum class TlsError {
    SUCCESS = 0,
    CERTIFICATE_VERIFICATION_FAILED,
    HOSTNAME_VERIFICATION_FAILED,
    CIPHER_NEGOTIATION_FAILED,
    HANDSHAKE_TIMEOUT,
    CERTIFICATE_EXPIRED,
    CERTIFICATE_NOT_TRUSTED,
    TLS_VERSION_NOT_SUPPORTED,
    MEMORY_ALLOCATION_FAILED,
    NETWORK_CONNECTION_FAILED
};

const char* tls_error_to_string(TlsError error) {
    switch (error) {
        case TlsError::SUCCESS: return "Success";
        case TlsError::CERTIFICATE_VERIFICATION_FAILED: return "Certificate verification failed";
        case TlsError::HOSTNAME_VERIFICATION_FAILED: return "Hostname verification failed";
        case TlsError::CIPHER_NEGOTIATION_FAILED: return "Cipher negotiation failed";
        case TlsError::HANDSHAKE_TIMEOUT: return "TLS handshake timeout";
        case TlsError::CERTIFICATE_EXPIRED: return "Certificate expired";
        case TlsError::CERTIFICATE_NOT_TRUSTED: return "Certificate not trusted";
        case TlsError::TLS_VERSION_NOT_SUPPORTED: return "TLS version not supported";
        case TlsError::MEMORY_ALLOCATION_FAILED: return "Memory allocation failed";
        case TlsError::NETWORK_CONNECTION_FAILED: return "Network connection failed";
        default: return "Unknown TLS error";
    }
}

// Enhanced error reporting in callbacks
client.get_async("https://api.example.com/data", {},
    [](const Response& response) {
        if (!response.ok()) {
            auto tls_info = client.get_last_tls_info();
            if (tls_info.is_secure && !tls_info.certificate_verified) {
                FL_WARN("TLS Error: Certificate verification failed");
                FL_WARN("Server cert: " << tls_info.server_certificate_sha256);
                FL_WARN("Consider adding root CA or pinning certificate");
            }
        }
    }
);
```

This comprehensive TLS design provides **enterprise-grade security** while maintaining FastLED's **simple, embedded-friendly approach**! 🔒

### **API Design Philosophy: Progressive Complexity**

**🎯 Start Simple, Add Complexity Only When Needed:**

The FastLED HTTP client follows a **progressive complexity** design where the simplest use cases require the least code:

**Level 1 - Minimal (Just URL):**
```cpp
client.get("http://localhost:8899/health");                    // Uses 5s timeout, no headers
client.post("http://localhost:8899/upload", sensor_data);     // Uses 5s timeout, no headers
```

**Level 2 - Custom Timeout:**
```cpp
client.get("http://slow-api.com/data", 15000);                // 15s timeout, no headers
client.post("http://slow-api.com/upload", data, 20000);       // 20s timeout, no headers
```

**Level 3 - Add Headers (Common Case):**
```cpp
client.get("http://api.com/data", 5000, {                     // 5s timeout, with headers
    {"Authorization", "Bearer token"}
});
client.post("http://api.com/upload", data, 10000, {           // 10s timeout, with headers
    {"Content-Type", "application/json"}
});
```

**Benefits of This Approach:**
- **✅ Zero barrier to entry** - health checks and simple APIs work with minimal code
- **✅ Sensible defaults** - 5 second timeout works for most local/fast APIs
- **✅ Incremental complexity** - add timeouts/headers only when actually needed
- **✅ Familiar C++ pattern** - default parameters are a well-understood C++ idiom

### **Why fl::string Shared Buffer Semantics Are Perfect for Networking**

**fl::string Benefits from Codebase Analysis:**
- **Copy-on-write semantics**: Copying `fl::string` only copies a shared_ptr (8-16 bytes)
- **Small string optimization**: Strings ≤64 bytes stored inline (no heap allocation)  
- **Shared ownership**: Multiple strings can share the same buffer efficiently
- **Automatic memory management**: RAII cleanup via shared_ptr
- **Thread-safe sharing**: Safe to share between threads with copy-on-write

**Strategic API Design Decisions:**

**✅ Use `const fl::string&` for:**
- **Return values** that might be stored/shared (response text, error messages)
- **Error messages** in callbacks (efficient sharing across multiple futures)
- **HTTP headers** that might be cached or logged
- **Large content** that benefits from shared ownership
- **Any data the receiver might store**

**✅ Keep `const char*` for:**
- **URLs** (usually string literals, consumed immediately)
- **HTTP methods** ("GET", "POST" - compile-time constants)
- **Immediate consumption** parameters (no storage needed)
- **Legacy C-style access** when raw char* is required

**Memory Efficiency Examples:**

```cpp
// Shared error messages across multiple futures - only ONE copy in memory!
fl::string network_timeout_error("Connection timeout after 30 seconds");

auto future1 = fl::make_error_future<Response>(network_timeout_error);
auto future2 = fl::make_error_future<Response>(network_timeout_error); 
auto future3 = fl::make_error_future<Response>(network_timeout_error);
// All three futures share the same error string buffer via copy-on-write!

// Response text sharing
void handle_response(const Response& response) {
    // Efficient sharing - no string copy!
    fl::string cached_response = response.text(); // Cheap shared_ptr copy
    
    // Can store, log, display, etc. without memory waste
    log_response(cached_response);
    display_on_screen(cached_response);
    cache_for_later(cached_response);
    // All share the same underlying buffer!
}

// Error message propagation in callbacks
client.get_stream_async("http://api.example.com/data", {},
    [](fl::span<const fl::u8> chunk, bool is_final) { return true; },
    [](int status, const fl::string& error_msg) {
        // Can efficiently store/share error message
        last_error = error_msg;        // Cheap copy
        log_error(error_msg);          // Can share same buffer
        display_error(error_msg);      // No extra memory used
        // Single string buffer shared across all uses!
    }
);
```

**Embedded System Benefits:**

1. **Memory Conservation**: Shared buffers reduce memory fragmentation
2. **Cache Efficiency**: Multiple references to same data improve cache locality
3. **Copy Performance**: Copy-on-write makes string passing nearly free
4. **RAII Safety**: Automatic cleanup prevents memory leaks
5. **Thread Safety**: Safe sharing between threads without manual synchronization

### **API Design Pattern Summary**

The FastLED networking API uses consistent naming patterns to make the right choice obvious:

**📊 Complete API Overview:**

| **Method Pattern** | **Returns** | **Use Case** | **Example** |
|-------------------|-------------|--------------|-------------|
| `get(url, timeout=5000, headers={})` | `fl::future<Response>` | Single HTTP request, check result later | Weather data, API calls |
| `get_async(url, callback, timeout=5000, headers={})` | `void` | Single HTTP request, immediate callback | Fire-and-forget requests |
| `get_stream_async(url, chunk_cb, done_cb)` | `void` | Large data, real-time streaming | File downloads, sensor feeds |
| `post(url, data, timeout=5000, headers={})` | `fl::future<Response>` | Single upload, check result later | Sensor data upload |
| `post_async(url, data, callback, timeout=5000, headers={})` | `void` | Single upload, immediate callback | Quick data submission |
| `post_stream_async(url, provider, done_cb)` | `void` | Large uploads, chunked sending | Log files, large datasets |

**🎯 When to Use Each Pattern:**

**✅ Use `get()` / `post()` (Futures) When:**
- You need to check the result later in `loop()`
- You want to store/cache the response
- You need error handling flexibility
- You're doing multiple concurrent requests

**✅ Use `get_async()` / `post_async()` (Callbacks) When:**
- You want immediate response processing
- The response is simple (success/failure)
- You don't need to store the response
- Fire-and-forget pattern is acceptable

**✅ Use `get_stream_async()` / `post_stream_async()` (Streaming) When:**
- Data is too large for memory (>1KB on embedded)
- You need real-time processing (sensor feeds)
- Progress monitoring is important
- Memory efficiency is critical

**🔧 Practical Decision Guide:**

```cpp
// ✅ Perfect for futures - check result when convenient
auto health_future = client.get("http://localhost:8899/health");        // Simplest!
auto weather_future = client.get("http://weather.api.com/current", 10000, {
    {"Authorization", "Bearer token"}
});
// Later in loop(): if (weather_future.is_ready()) { ... }

// ✅ Perfect for callbacks - immediate simple processing  
client.get_async("http://api.com/ping", [](const Response& r) {
    FL_WARN("API status: " << (r.ok() ? "UP" : "DOWN"));
});                                                                      // Uses defaults!

// ✅ Perfect for streaming - large data with progress
client.get_stream_async("http://api.com/dataset.bin", {},
    [](fl::span<const fl::u8> chunk, bool final) { 
        process_chunk(chunk); 
        return true; 
    },
    [](int status, const fl::string& error) { 
        FL_WARN("Download: " << status); 
    }
);
```

**💡 Pro Tips:**
- **Start simple**: Most APIs work with just `client.get("http://localhost:8899/health")`
- **Add complexity gradually**: Add timeout/headers only when needed
- **Mix and match**: Use futures for some requests, callbacks for others
- **Error sharing**: `fl::string` errors can be stored/shared efficiently  
- **Memory efficiency**: Streaming APIs prevent memory exhaustion
- **Thread safety**: All patterns work safely with FastLED's threading model

## 🎯 **Key API Improvements Summary**

**✅ What Changed:**
- **Default timeout**: All methods now default to 5000ms (5 seconds)
- **Default headers**: All methods now default to empty headers (`{}`)
- **Removed overloads**: No separate initializer_list overloads (fl::span handles both)
- **Simplified signatures**: Most common case is now just URL + data (for POST)
- **Progressive complexity**: Add timeout/headers only when actually needed

**✅ Usage Comparison:**

| **Before (Verbose)** | **After (Simple)** |
|---------------------|-------------------|
| `get(url, span<headers>)` | `get(url)` |
| `get(url, {})` | `get(url)` |
| `get_async(url, {}, callback)` | `get_async(url, callback)` |
| `post(url, data, span<headers>)` | `post(url, data)` |
| Multiple overloads for headers | Single method handles all cases |

**✅ Benefits:**
- **✅ Zero learning curve** - health checks work with minimal code
- **✅ Faster prototyping** - test APIs without configuring timeouts/headers
- **✅ Fewer errors** - sensible defaults prevent common timeout issues
- **✅ Simplified API** - single method handles vectors, arrays, and initializer lists
- **✅ Backward compatible** - advanced usage still works exactly the same

**✅ Perfect For:**
- **Health checks**: `client.get("http://localhost:8899/health")`
- **Local APIs**: `client.post("http://192.168.1.100/data", sensor_data)`
- **Simple testing**: Quick API exploration without boilerplate
- **Development**: Focus on logic, not HTTP configuration

## 🔧 **fl::span Header Conversion Magic**

The `headers` parameter uses `fl::span<const fl::pair<fl::string, fl::string>>` which **automatically converts** from:

```cpp
// ✅ Initializer lists (most common)
client.get("http://api.com/data", 5000, {
    {"Authorization", "Bearer token"},
    {"Content-Type", "application/json"}
});

// ✅ fl::vector (when building headers dynamically)
fl::vector<fl::pair<fl::string, fl::string>> headers;
headers.push_back({"Authorization", get_token()});
client.get("http://api.com/data", 5000, headers);

// ✅ C-style arrays (when headers are static)
fl::pair<fl::string, fl::string> auth_headers[] = {
    {"Authorization", "Bearer static_token"}
};
client.get("http://api.com/data", 5000, auth_headers);

// ✅ Empty headers (uses default)
client.get("http://api.com/data"); // No headers needed
```

**Result**: **One method signature handles all use cases** - no overload explosion! 🎯

### **Response Object API Improvements with fl::deque**

The Response object has been optimized for FastLED's patterns and embedded use, with **fl::deque** providing memory-efficient storage:

**✅ Memory-Efficient Binary Data Access:**
```cpp
// Iterator-based access (most memory efficient with fl::deque)
for (const fl::u8& byte : response) {
    process_byte(byte);  // Direct deque iteration - no memory allocation!
}

// Chunk-based processing (perfect for large responses)
response.process_chunks(512, [](fl::span<const fl::u8> chunk) {
    write_to_sd_card(chunk);  // Process 512-byte chunks efficiently
    return true;  // Continue processing
});

// Direct deque access (advanced users)
const fl::deque<fl::u8, fl::allocator_psram<fl::u8>>& raw_deque = response.body_deque();

// When you absolutely need contiguous memory (use sparingly!)
fl::vector<fl::u8> contiguous_buffer = response.to_vector();

// Copy to existing buffer (avoids allocation)
fl::u8 my_buffer[1024];
fl::size copied = response.copy_to_buffer(my_buffer, 1024);
```

**✅ Text Content Access (with Copy-on-Write Efficiency):**
```cpp
// Primary interface - fl::string with copy-on-write semantics
fl::string response_text = response.text();  // Cheap copy, shared buffer!

// Can store, cache, log efficiently
cache_response(response_text);    // Same buffer shared
log_response(response_text);      // No extra memory used
display_text(response_text);      // Efficient sharing

// Legacy C-style access when needed
const char* c_str = response.c_str();
fl::size len = response.text_length();
```

**🎯 Key Benefits of fl::deque Design:**

1. **fl::deque<fl::u8> storage** instead of fl::vector:
   - **No memory fragmentation**: Data stored in chunks, not one massive allocation
   - **Streaming-friendly**: Perfect for receiving HTTP responses piece-by-piece
   - **Memory-efficient growth**: Grows incrementally without copying existing data
   - **Iterator compatibility**: Works seamlessly with range-based for loops

2. **Multiple access patterns** for different use cases:
   - **Iterator-based**: Most memory efficient, perfect for streaming processing
   - **Chunk-based**: Process large responses in manageable pieces  
   - **Text access**: Copy-on-write fl::string for efficient text handling
   - **Buffer copy**: When APIs require contiguous memory

3. **Consistent fl:: types** throughout:
   - **fl::u8** instead of uint8_t for consistency
   - **fl::size** instead of size_t for embedded compatibility
   - **fl::string** for shared buffer semantics
   - **fl::deque** for memory-efficient response storage

**Real-World Usage Benefits:**
```cpp
void handle_large_response(const Response& resp) {
    if (resp.ok()) {
        // Memory-efficient processing - no huge allocations!
        
        // Option 1: Stream processing (most efficient)
        resp.process_chunks(1024, [](fl::span<const fl::u8> chunk) {
            parse_json_chunk(chunk);  // Parse as data arrives
            return true;
        });
        
        // Option 2: Text access with shared ownership
        fl::string response_text = resp.text();  // Copy-on-write
        cache_response(response_text);           // Share same buffer
        log_response(response_text);             // No extra memory
        
        // Option 3: Byte-by-byte processing (zero allocation)
        for (const fl::u8& byte : resp) {
            update_checksum(byte);  // Process without copying
        }
        
        // Option 4: Copy only when absolutely needed
        if (need_contiguous_buffer()) {
            fl::vector<fl::u8> buffer = resp.to_vector();  // Only when necessary
            legacy_api_call(buffer.data(), buffer.size());
        }
    }
}
```

**Embedded Memory Comparison with PSRAM:**
```cpp
// Scenario: 100KB HTTP response on ESP32S3 with 8MB PSRAM

// ❌ With fl::vector (internal RAM): 
// - Needs 100KB contiguous internal RAM
// - Potential 200KB during reallocation
// - High risk of out-of-memory crash
// - Uses precious internal RAM for data storage

// ✅ With PSRAM-backed fl::deque:
// - Uses ~100KB in PSRAM chunks  
// - No reallocation copying
// - Zero fragmentation risk
// - Preserves internal RAM for time-critical operations
// - Can handle responses up to several MB
// - Automatic fallback to internal RAM if PSRAM unavailable
// - Perfect for streaming processing from external memory
```

## **Complete fl::Response Interface with fl::deque**

### **Usage Examples (Starting Simple → Advanced)**

#### **1. Simple Text Response Processing**

```cpp
#include "fl/networking/http_client.h"

using namespace fl;

void simple_text_example() {
    HttpClient client;
    
    // Simple API call
    auto future = client.get("http://api.weather.com/current");
    
    // Check result in loop
    if (future.is_ready()) {
        auto result = future.try_result();
        if (result && result->ok()) {
            // Simple text access - copy-on-write fl::string
            fl::string weather_data = result->text();
            FL_WARN("Weather: " << weather_data);
            
            // Can store/share efficiently
            cache_weather_data(weather_data);  // Shared buffer
            display_on_leds(weather_data);     // No extra memory
        }
    }
}
```

#### **2. Memory-Efficient Large Response Processing**

```cpp
void large_response_example() {
    HttpClient client;
    
    // Download large dataset
    auto future = client.get("http://data.api.com/sensor-logs", 30000);
    
    if (future.is_ready()) {
        auto result = future.try_result();
        if (result && result->ok()) {
            // Option 1: Stream processing (most memory efficient)
            result->process_chunks(1024, [](fl::span<const fl::u8> chunk) {
                // Process 1KB chunks - only 1KB in memory at a time!
                parse_sensor_data_chunk(chunk);
                write_to_sd_card(chunk);
                return true;  // Continue processing
            });
            
            FL_WARN("Processed " << result->size() << " bytes with minimal RAM");
        }
    }
}
```

#### **3. Byte-by-Byte Processing (Zero Allocation)**

```cpp
void binary_data_example() {
    HttpClient client;
    
    auto future = client.get("http://firmware.server.com/update.bin");
    
    if (future.is_ready()) {
        auto result = future.try_result();
        if (result && result->ok()) {
            // Zero-allocation byte processing
            fl::u32 checksum = 0;
            fl::size byte_count = 0;
            
            for (const fl::u8& byte : *result) {
                checksum ^= byte;      // Update checksum
                byte_count++;
                
                // Can also process firmware bytes directly
                if (byte_count % 256 == 0) {
                    write_firmware_page_to_flash();
                }
            }
            
            FL_WARN("Checksum: 0x" << fl::hex << checksum);
            FL_WARN("Processed " << byte_count << " bytes with zero allocation");
        }
    }
}
```

#### **4. JSON Processing with Chunked Parsing**

```cpp
class JsonStreamProcessor {
private:
    fl::string mPartialJson;
    fl::size mBraceDepth = 0;
    
public:
    void process_json_response(const Response& response) {
        if (!response.ok()) return;
        
        // Process JSON as it arrives - perfect for large JSON arrays
        response.process_chunks(512, [this](fl::span<const fl::u8> chunk) {
            // Convert chunk to string
            fl::string chunk_str(reinterpret_cast<const char*>(chunk.data()), chunk.size());
            mPartialJson += chunk_str;
            
            // Parse complete JSON objects as they arrive
            parse_complete_json_objects();
            
            return true;  // Continue processing
        });
        
        // Handle any remaining partial JSON
        if (!mPartialJson.empty()) {
            parse_final_json_fragment();
        }
    }
    
private:
    void parse_complete_json_objects() {
        fl::size start = 0;
        
        for (fl::size i = 0; i < mPartialJson.size(); ++i) {
            if (mPartialJson[i] == '{') {
                mBraceDepth++;
            } else if (mPartialJson[i] == '}') {
                mBraceDepth--;
                
                if (mBraceDepth == 0) {
                    // Complete JSON object found
                    fl::string json_obj = mPartialJson.substr(start, i - start + 1);
                    process_single_json_object(json_obj);
                    start = i + 1;
                }
            }
        }
        
        // Keep only the incomplete part
        if (start < mPartialJson.size()) {
            mPartialJson = mPartialJson.substr(start);
        } else {
            mPartialJson.clear();
        }
    }
    
    void process_single_json_object(const fl::string& json) {
        FL_WARN("Processing JSON object: " << json.substr(0, 50) << "...");
        // Parse with your favorite JSON library
    }
    
    void parse_final_json_fragment() {
        if (!mPartialJson.empty()) {
            FL_WARN("Final JSON fragment: " << mPartialJson);
        }
    }
};

void json_streaming_example() {
    HttpClient client;
    JsonStreamProcessor processor;
    
    auto future = client.get("http://api.com/large-json-array");
    
    if (future.is_ready()) {
        auto result = future.try_result();
        if (result) {
            processor.process_json_response(*result);
        }
    }
}
```

#### **5. PSRAM Optimization on ESP32S3**

```cpp
#ifdef ESP32
void esp32_psram_example() {
    HttpClient client;
    
    // Configure client to use PSRAM for large responses
    HttpClient::MemoryConfig mem_config;
    mem_config.prefer_psram = true;           // Use PSRAM when available
    mem_config.psram_threshold = 4096;       // Use PSRAM for responses >4KB
    mem_config.max_internal_ram = 2048;      // Keep small responses in fast RAM
    client.set_memory_config(mem_config);
    
    // Download large image or dataset
    auto future = client.get("http://camera.local/image.jpg", 60000);
    
    if (future.is_ready()) {
        auto result = future.try_result();
        if (result && result->ok()) {
            FL_WARN("Downloaded " << result->size() << " bytes");
            FL_WARN("Memory type: " << (result->uses_psram() ? "PSRAM" : "Internal"));
            
            // Process large image data efficiently
            result->process_chunks(2048, [](fl::span<const fl::u8> chunk) {
                // Process 2KB image chunks - data stays in PSRAM
                decode_jpeg_chunk(chunk);
                return true;
            });
            
            // PSRAM memory automatically freed when Response destroyed
        }
    }
}
#endif
```

#### **6. Mixed Access Patterns in One Response**

```cpp
void mixed_access_example() {
    HttpClient client;
    
    auto future = client.get("http://api.com/mixed-data");
    
    if (future.is_ready()) {
        auto result = future.try_result();
        if (result && result->ok()) {
            // Multiple ways to access the same response data
            
            // 1. Get response info
            FL_WARN("Status: " << result->status_code());
            FL_WARN("Size: " << result->size() << " bytes");
            FL_WARN("Content-Type: " << result->header("Content-Type"));
            
            // 2. Quick text check (for small responses)
            if (result->size() < 1024) {
                fl::string text = result->text();
                FL_WARN("Small response text: " << text);
            }
            
            // 3. Chunk processing for analysis
            fl::size line_count = 0;
            result->process_chunks(256, [&line_count](fl::span<const fl::u8> chunk) {
                for (const fl::u8& byte : chunk) {
                    if (byte == '\n') line_count++;
                }
                return true;
            });
            FL_WARN("Found " << line_count << " lines");
            
            // 4. Binary processing if needed
            if (result->header("Content-Type").find("application/octet-stream") != fl::string::npos) {
                fl::u32 checksum = 0;
                for (const fl::u8& byte : *result) {
                    checksum = (checksum << 1) ^ byte;  // Simple checksum
                }
                FL_WARN("Binary checksum: 0x" << fl::hex << checksum);
            }
            
            // 5. Copy to buffer only when absolutely necessary
            if (need_contiguous_buffer_for_legacy_api()) {
                fl::vector<fl::u8> buffer = result->to_vector();
                legacy_api_call(buffer.data(), buffer.size());
                FL_WARN("Copied to contiguous buffer for legacy API");
            }
        }
    }
}
```

### **Complete fl::Response Interface Specification**

```cpp
namespace fl {

/// HTTP response object with memory-efficient fl::deque storage
class Response {
public:
    /// Response status and metadata
    int status_code() const { return mStatusCode; }
    bool ok() const { return mStatusCode >= 200 && mStatusCode < 300; }
    const fl::string& status_text() const { return mStatusText; }
    
    /// Response headers
    const fl::string& header(const fl::string& name) const;
    const fl::string& header(const char* name) const;
    bool has_header(const fl::string& name) const;
    bool has_header(const char* name) const;
    
    /// Get all headers as key-value pairs
    fl::span<const fl::pair<fl::string, fl::string>> headers() const;
    
    /// Response body size and properties
    fl::size size() const { return mBody.size(); }
    bool empty() const { return mBody.empty(); }
    
    /// Memory management info
    bool uses_psram() const { return mUsesPsram; }
    fl::size memory_footprint() const;  // Total memory used including headers
    
    // ========== PRIMARY INTERFACES ==========
    
    /// Text access (copy-on-write fl::string for efficient sharing)
    /// @note Creates fl::string from response data - efficient for text content
    /// @note Uses copy-on-write semantics - multiple copies share same buffer
    fl::string text() const;
    
    /// Legacy C-style text access
    /// @note Returns pointer to internal buffer - valid until Response destroyed
    /// @warning Only valid if response contains text data
    const char* c_str() const;
    fl::size text_length() const;
    
    /// Iterator-based access (most memory efficient)
    /// @note Direct iteration over response body - zero allocation
    /// @note Works with both inlined and deque storage transparently
    /// @code
    /// for (const fl::u8& byte : response) {
    ///     process_byte(byte);  // Zero allocation access
    /// }
    /// @endcode
    using const_iterator = ResponseBody::const_iterator;
    const_iterator begin() const { return mBody.begin(); }
    const_iterator end() const { return mBody.end(); }
    const_iterator cbegin() const { return mBody.cbegin(); }
    const_iterator cend() const { return mBody.cend(); }
    
    /// Chunk-based processing (ideal for large responses)
    /// @param chunk_size size of each chunk to process (typically 512-4096 bytes)
    /// @param processor function called for each chunk: bool processor(fl::span<const fl::u8> chunk)
    /// @returns true if all chunks processed, false if processor returned false
    /// @note Processes response in manageable chunks without loading all into memory
    /// @note Optimized for both small inlined and large deque storage
    /// @code
    /// response.process_chunks(1024, [](fl::span<const fl::u8> chunk) {
    ///     write_to_sd_card(chunk);  // Process 1KB at a time
    ///     return true;  // Continue processing
    /// });
    /// @endcode
    bool process_chunks(fl::size chunk_size, 
                        fl::function<bool(fl::span<const fl::u8>)> processor) const;
    
    // ========== ADVANCED INTERFACES ==========
    
    /// Direct body access (for advanced users)
    /// @note Provides direct access to underlying storage (inlined vector or deque)
    /// @warning Advanced usage only - prefer iterator or chunk interfaces
    const ResponseBody& body() const { return mBody; }
    
    /// Copy to contiguous buffer (use sparingly!)
    /// @note Creates new fl::vector with all response data
    /// @warning Potentially large allocation - use only when absolutely necessary
    /// @warning May fail on embedded systems with insufficient memory
    /// @note Optimized path for small inlined responses (direct copy)
    fl::vector<fl::u8> to_vector() const;
    
    /// Copy to existing buffer
    /// @param buffer destination buffer to copy into
    /// @param buffer_size size of destination buffer
    /// @returns number of bytes actually copied (may be less than response size)
    /// @note Safe alternative to to_vector() - no allocation
    /// @note Efficient for both small and large responses
    fl::size copy_to_buffer(fl::u8* buffer, fl::size buffer_size) const;
    
    /// Copy range to existing buffer
    /// @param start_offset byte offset to start copying from
    /// @param buffer destination buffer
    /// @param buffer_size size of destination buffer
    /// @returns number of bytes actually copied
    fl::size copy_range_to_buffer(fl::size start_offset, fl::u8* buffer, fl::size buffer_size) const;
    
    // ========== UTILITY METHODS ==========
    
    /// Find byte pattern in response
    /// @param pattern bytes to search for
    /// @returns offset of first match, or fl::string::npos if not found
    fl::size find_bytes(fl::span<const fl::u8> pattern) const;
    
    /// Find text pattern in response (treats response as text)
    /// @param text string to search for
    /// @returns offset of first match, or fl::string::npos if not found
    fl::size find_text(const fl::string& text) const;
    fl::size find_text(const char* text) const;
    
    /// Get byte at specific offset
    /// @param offset byte offset into response
    /// @returns byte value, or 0 if offset out of range
    fl::u8 at(fl::size offset) const;
    fl::u8 operator[](fl::size offset) const { return at(offset); }
    
    /// Get span of bytes at specific range
    /// @param start_offset starting byte offset
    /// @param length number of bytes to include
    /// @returns span covering the requested range (may be shorter if out of bounds)
    /// @note Span is only valid while Response object exists
    /// @note Works efficiently with both inlined and deque storage
    fl::span<const fl::u8> subspan(fl::size start_offset, fl::size length = fl::string::npos) const;
    
    /// Response validation
    bool is_valid() const { return mStatusCode > 0; }
    bool is_json() const;        // Checks Content-Type header
    bool is_text() const;        // Checks Content-Type header
    bool is_binary() const;      // Checks Content-Type header
    
    /// Storage optimization info
    bool is_inlined() const { return mBody.is_inlined(); }
    bool is_deque_storage() const { return mBody.is_deque(); }
    static constexpr fl::size INLINE_BUFFER_SIZE = 256;  ///< Size of inline buffer for small responses
    
    /// Error information
    const fl::string& error_message() const { return mErrorMessage; }
    bool has_error() const { return !mErrorMessage.empty(); }
    
    // ========== CONSTRUCTION (Internal Use) ==========
    
    /// Create empty response
    Response() = default;
    
    /// Create error response
    static Response error(int status_code, const fl::string& error_msg);
    static Response error(int status_code, const char* error_msg);
    
    /// Create successful response (used internally by HttpClient)
    static Response success(int status_code, 
                           const fl::string& status_text,
                           fl::vector<fl::pair<fl::string, fl::string>> headers,
                           ResponseBody body,
                           bool uses_psram = false);
    
    // ========== MEMORY CONFIGURATION ==========
    
    /// Memory configuration for response storage
    struct MemoryConfig {
        bool prefer_psram = true;           ///< Use PSRAM when available
        fl::size inline_threshold = 256;    ///< Use inline storage for responses smaller than this
        fl::size psram_threshold = 2048;    ///< Use PSRAM for deque storage larger than this
        fl::size max_internal_ram = 4096;   ///< Maximum internal RAM to use for responses
        bool allow_large_responses = true;  ///< Allow responses larger than max_internal_ram
    };
    
    /// Set memory configuration (affects future responses)
    static void set_default_memory_config(const MemoryConfig& config);
    static const MemoryConfig& get_default_memory_config();

private:
    // Response metadata
    int mStatusCode = 0;
    fl::string mStatusText;
    fl::vector<fl::pair<fl::string, fl::string>> mHeaders;
    fl::string mErrorMessage;
    
    // Response body - hybrid storage with small buffer optimization
    ResponseBody mBody;
    bool mUsesPsram = false;
    
    // Cached text representation (lazy-loaded)
    mutable fl::optional<fl::string> mCachedText;
    
    // Header lookup optimization
    mutable fl::unordered_map<fl::string, fl::string> mHeaderMap;
    mutable bool mHeaderMapBuilt = false;
    
    // Memory management
    static MemoryConfig sDefaultMemoryConfig;
    
    // Internal helpers
    void build_header_map() const;
    const fl::string& find_header(const fl::string& name) const;
    
    // Friends for internal construction
    friend class HttpClient;
    friend class HttpRequest;  // Internal request class
};

/// Hybrid response body storage: inlined vector for small responses, deque for large
/// @note Optimizes for the common case of small HTTP responses (≤256 bytes)
/// @note Automatically spills to PSRAM-backed deque for larger responses
class ResponseBody {
public:
    static constexpr fl::size INLINE_SIZE = 256;  ///< Inline buffer size for small responses
    
    /// Iterator type that works with both storage modes
    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = fl::u8;
        using difference_type = std::ptrdiff_t;
        using pointer = const fl::u8*;
        using reference = const fl::u8&;
        
        const_iterator() = default;
        
        // Constructor for inline storage
        const_iterator(const fl::u8* ptr) : mInlinePtr(ptr), mIsInline(true) {}
        
        // Constructor for deque storage
        const_iterator(const fl::deque<fl::u8, fl::allocator_psram<fl::u8>>::const_iterator& it) 
            : mDequeIt(it), mIsInline(false) {}
        
        reference operator*() const {
            return mIsInline ? *mInlinePtr : *mDequeIt;
        }
        
        pointer operator->() const {
            return mIsInline ? mInlinePtr : &(*mDequeIt);
        }
        
        const_iterator& operator++() {
            if (mIsInline) {
                ++mInlinePtr;
            } else {
                ++mDequeIt;
            }
            return *this;
        }
        
        const_iterator operator++(int) {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        
        bool operator==(const const_iterator& other) const {
            if (mIsInline != other.mIsInline) return false;
            return mIsInline ? (mInlinePtr == other.mInlinePtr) : (mDequeIt == other.mDequeIt);
        }
        
        bool operator!=(const const_iterator& other) const {
            return !(*this == other);
        }
        
    private:
        union {
            const fl::u8* mInlinePtr;
            fl::deque<fl::u8, fl::allocator_psram<fl::u8>>::const_iterator mDequeIt;
        };
        bool mIsInline = true;
    };
    
    /// Default constructor - starts with inline storage
    ResponseBody() : mIsInline(true), mSize(0) {
        // Initialize inline buffer
        new (&mInlineBuffer) fl::array<fl::u8, INLINE_SIZE>();
    }
    
    /// Destructor
    ~ResponseBody() {
        if (!mIsInline) {
            mDequeStorage.~deque();
        }
    }
    
    /// Copy constructor
    ResponseBody(const ResponseBody& other) : mIsInline(other.mIsInline), mSize(other.mSize) {
        if (mIsInline) {
            new (&mInlineBuffer) fl::array<fl::u8, INLINE_SIZE>(other.mInlineBuffer);
        } else {
            new (&mDequeStorage) fl::deque<fl::u8, fl::allocator_psram<fl::u8>>(other.mDequeStorage);
        }
    }
    
    /// Move constructor
    ResponseBody(ResponseBody&& other) noexcept : mIsInline(other.mIsInline), mSize(other.mSize) {
        if (mIsInline) {
            new (&mInlineBuffer) fl::array<fl::u8, INLINE_SIZE>(fl::move(other.mInlineBuffer));
        } else {
            new (&mDequeStorage) fl::deque<fl::u8, fl::allocator_psram<fl::u8>>(fl::move(other.mDequeStorage));
        }
    }
    
    /// Assignment operators
    ResponseBody& operator=(const ResponseBody& other) {
        if (this != &other) {
            this->~ResponseBody();
            new (this) ResponseBody(other);
        }
        return *this;
    }
    
    ResponseBody& operator=(ResponseBody&& other) noexcept {
        if (this != &other) {
            this->~ResponseBody();
            new (this) ResponseBody(fl::move(other));
        }
        return *this;
    }
    
    /// Add data to response body
    /// @note Automatically switches from inline to deque storage when needed
    void append(fl::span<const fl::u8> data) {
        if (mIsInline && mSize + data.size() <= INLINE_SIZE) {
            // Still fits in inline buffer
            fl::memcpy(mInlineBuffer.data() + mSize, data.data(), data.size());
            mSize += data.size();
        } else {
            // Need to switch to deque storage
            switch_to_deque();
            for (const fl::u8& byte : data) {
                mDequeStorage.push_back(byte);
            }
            mSize += data.size();
        }
    }
    
    /// Add single byte
    void push_back(fl::u8 byte) {
        if (mIsInline && mSize < INLINE_SIZE) {
            mInlineBuffer[mSize++] = byte;
        } else {
            if (mIsInline) {
                switch_to_deque();
            }
            mDequeStorage.push_back(byte);
            ++mSize;
        }
    }
    
    /// Size of response body
    fl::size size() const { return mSize; }
    bool empty() const { return mSize == 0; }
    
    /// Storage mode queries
    bool is_inlined() const { return mIsInline; }
    bool is_deque() const { return !mIsInline; }
    
    /// Iterator access
    const_iterator begin() const {
        if (mIsInline) {
            return const_iterator(mInlineBuffer.data());
        } else {
            return const_iterator(mDequeStorage.begin());
        }
    }
    
    const_iterator end() const {
        if (mIsInline) {
            return const_iterator(mInlineBuffer.data() + mSize);
        } else {
            return const_iterator(mDequeStorage.end());
        }
    }
    
    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }
    
    /// Direct access to underlying storage (advanced users)
    fl::span<const fl::u8> as_span() const {
        if (mIsInline) {
            return fl::span<const fl::u8>(mInlineBuffer.data(), mSize);
        } else {
            // For deque, we need to copy to contiguous memory - this is expensive!
            // Advanced users should prefer iterator access
            static thread_local fl::vector<fl::u8> temp_buffer;
            temp_buffer.clear();
            temp_buffer.reserve(mSize);
            for (const fl::u8& byte : mDequeStorage) {
                temp_buffer.push_back(byte);
            }
            return fl::span<const fl::u8>(temp_buffer.data(), temp_buffer.size());
        }
    }
    
    /// Get byte at offset (bounds checked)
    fl::u8 at(fl::size offset) const {
        if (offset >= mSize) return 0;
        
        if (mIsInline) {
            return mInlineBuffer[offset];
        } else {
            // For deque, we need to iterate (slower for random access)
            auto it = mDequeStorage.begin();
            fl::advance(it, offset);
            return *it;
        }
    }
    
private:
    /// Switch from inline storage to deque storage
    void switch_to_deque() {
        if (!mIsInline) return;  // Already switched
        
        // Save current inline data
        fl::array<fl::u8, INLINE_SIZE> temp_data = mInlineBuffer;
        fl::size temp_size = mSize;
        
        // Destroy inline buffer and construct deque
        mInlineBuffer.~array();
        new (&mDequeStorage) fl::deque<fl::u8, fl::allocator_psram<fl::u8>>();
        
        // Copy data from inline buffer to deque
        for (fl::size i = 0; i < temp_size; ++i) {
            mDequeStorage.push_back(temp_data[i]);
        }
        
        mIsInline = false;
        // mSize remains the same
    }
    
    /// Storage mode flag
    bool mIsInline;
    fl::size mSize;
    
    /// Union for storage - either inline buffer or deque
    union {
        fl::array<fl::u8, INLINE_SIZE> mInlineBuffer;
        fl::deque<fl::u8, fl::allocator_psram<fl::u8>> mDequeStorage;
    };
};

} // namespace fl
```

### **Memory Efficiency Analysis: fl::deque vs fl::vector**

#### **Memory Usage Comparison**

| **Scenario** | **fl::vector<fl::u8>** | **fl::deque<fl::u8> + PSRAM** | **Improvement** |
|--------------|------------------------|-------------------------------|-----------------|
| **10KB Response** | 10KB internal RAM | 10KB PSRAM chunks | **Saves 10KB internal RAM** |
| **100KB Response** | 100KB internal RAM (may OOM) | 100KB PSRAM chunks | **Prevents OOM crash** |
| **1MB Response** | Impossible (OOM) | 1MB PSRAM chunks | **Enables large downloads** |
| **Reallocation** | 2x during growth | No reallocation | **50% memory savings** |
| **Fragmentation** | High (large blocks) | Low (small chunks) | **Better memory health** |

#### **Performance Benefits on ESP32S3**

```cpp
// Real-world ESP32S3 example with 8MB PSRAM
void esp32s3_performance_demo() {
    HttpClient client;
    
    // Download 2MB dataset - impossible with fl::vector in internal RAM!
    auto future = client.get("http://data.server.com/large-dataset.bin", 120000);
    
    if (future.is_ready()) {
        auto result = future.try_result();
        if (result && result->ok()) {
            FL_WARN("Downloaded " << result->size() << " bytes to PSRAM");
            
            // Process in 4KB chunks - only 4KB in internal RAM at any time
            fl::size processed = 0;
            result->process_chunks(4096, [&processed](fl::span<const fl::u8> chunk) {
                // Process chunk in fast internal RAM
                process_sensor_data_chunk(chunk);
                processed += chunk.size();
                
                if (processed % 100000 == 0) {
                    FL_WARN("Processed " << processed << " bytes");
                }
                return true;
            });
            
            FL_WARN("Complete! Used minimal internal RAM for 2MB processing");
        }
    }
}
```

### **Integration with HTTP Client**

#### **Internal Request Management**

```cpp
namespace fl {

// Internal class used by HttpClient
class HttpRequest {
private:
    fl::string mUrl;
    fl::string mMethod;
    fl::vector<fl::pair<fl::string, fl::string>> mHeaders;
    fl::deque<fl::u8, fl::allocator_psram<fl::u8>> mResponseBuffer;  // PSRAM-backed
    Response::MemoryConfig mMemoryConfig;
    
public:
    // Configure memory usage for this request
    void set_memory_config(const Response::MemoryConfig& config) {
        mMemoryConfig = config;
        configure_response_allocator();
    }
    
    // Add response data as it arrives from network
    void append_response_data(fl::span<const fl::u8> data) {
        // Efficiently append to deque - no reallocation!
        for (const fl::u8& byte : data) {
            mResponseBuffer.push_back(byte);
        }
    }
    
    // Build final Response object
    Response build_response(int status_code, const fl::string& status_text) {
        return Response::success(
            status_code,
            status_text,
            fl::move(mHeaders),
            fl::move(mResponseBuffer),  // Transfer ownership efficiently
            mMemoryConfig.prefer_psram
        );
    }
    
private:
    void configure_response_allocator() {
        // Configure PSRAM allocator based on memory config
        if (mMemoryConfig.prefer_psram) {
            // Set up PSRAM allocator for response buffer
            mResponseBuffer = fl::deque<fl::u8, fl::allocator_psram<fl::u8>>();
        }
    }
};

} // namespace fl
```

This complete specification provides:

✅ **Simple interface** for common use cases  
✅ **Memory-efficient design** with fl::deque + PSRAM  
✅ **Multiple access patterns** for different scenarios  
✅ **Zero-allocation options** for memory-constrained environments  
✅ **Copy-on-write text access** with fl::string  
✅ **Advanced features** for power users  
✅ **ESP32 PSRAM optimization** for large responses  
✅ **Embedded-friendly design** throughout  

The `fl::deque` design enables FastLED to handle **large HTTP responses efficiently** while maintaining **simple APIs** for common use cases! 🚀

### **TransportVerify Usage Examples**

#### **1. Smart Verification (AUTO - Default Behavior)**

```cpp
void smart_verification_example() {
    // Default behavior - smart verification
    HttpClient client;  // Uses TransportVerify::AUTO by default
    
    // HTTP connections - no verification (fast and simple)
    auto http_future = client.get("http://api.example.com/data");
    
    // HTTPS to localhost/private - no verification (development friendly)
    auto local_https_future = client.get("https://localhost:8443/api");
    auto private_https_future = client.get("https://192.168.1.100:8443/api");
    auto dev_server_future = client.get("https://dev.example.local/api");
    
    // HTTPS to public servers - full verification (secure)
    auto public_https_future = client.get("https://api.github.com/user");
    
    FL_WARN("Smart verification automatically handles different scenarios:");
    FL_WARN("- HTTP: no verification (fast)");
    FL_WARN("- HTTPS localhost/private: no verification (dev-friendly)");
    FL_WARN("- HTTPS public: full verification (secure)");
}
```

#### **2. Development Configuration (NO Verification)**

```cpp
void development_setup() {
    // Development mode - disable all verification
    auto dev_client = HttpClient::create_with_transport(
        TcpTransport::create_unverified()
    );
    
    // OR configure existing client
    TcpTransport::Options dev_opts;
    dev_opts.verify = TransportVerify::NO;
    dev_opts.allow_self_signed = true;
    
    HttpClient client;
    client.configure_transport(dev_opts);
    
    // All connections work without verification
    client.get("https://self-signed.local/api");      // Works!
    client.get("https://expired-cert.local/api");     // Works!
    client.get("https://wrong-hostname.local/api");   // Works!
    
    FL_WARN("DEVELOPMENT MODE: All TLS verification disabled!");
    FL_WARN("DO NOT USE IN PRODUCTION!");
}
```

#### **3. Production Configuration (YES Verification)**

```cpp
void production_setup() {
    // Production mode - strict verification for everything
    auto prod_client = HttpClient::create_with_transport(
        TcpTransport::create_strict_verification()
    );
    
    // OR configure existing client
    TcpTransport::Options prod_opts;
    prod_opts.verify = TransportVerify::YES;
    prod_opts.verify_hostname = true;
    prod_opts.allow_self_signed = false;
    
    HttpClient client;
    client.configure_transport(prod_opts);
    
    // Only properly verified connections work
    client.get("https://api.trusted.com/data");       // Works if cert valid
    // client.get("http://api.trusted.com/data");     // Would fail! (no TLS)
    // client.get("https://localhost:8443/api");      // Would fail! (verification required)
    
    FL_WARN("PRODUCTION MODE: Strict verification for all connections");
}
```

#### **4. Environment-Specific Configuration**

```cpp
class ConfigurableClient {
private:
    HttpClient mClient;
    bool mIsProduction;
    
public:
    void initialize(bool production_mode) {
        mIsProduction = production_mode;
        
        TcpTransport::Options opts;
        
        if (production_mode) {
            // Production: strict verification
            opts.verify = TransportVerify::YES;
            opts.verify_hostname = true;
            opts.allow_self_signed = false;
            opts.limits.connection_timeout_ms = 15000;  // Longer for remote servers
            
            FL_WARN("Initialized in PRODUCTION mode");
            FL_WARN("- All connections require valid TLS certificates");
            FL_WARN("- Hostname verification enabled");
            FL_WARN("- Self-signed certificates rejected");
            
        } else {
            // Development: smart verification  
            opts.verify = TransportVerify::AUTO;
            opts.allow_self_signed = true;
            opts.limits.connection_timeout_ms = 5000;   // Faster for local dev
            
            FL_WARN("Initialized in DEVELOPMENT mode");
            FL_WARN("- Smart verification (HTTPS public servers verified)");
            FL_WARN("- Local/private HTTPS allowed without verification");
            FL_WARN("- Self-signed certificates allowed");
        }
        
        auto transport = fl::make_unique<TcpTransport>(opts);
        mClient = HttpClient::create_with_transport(fl::move(transport));
    }
    
    void test_various_endpoints() {
        // Test different types of endpoints
        
        // HTTP endpoint (always works)
        mClient.get_async("http://httpbin.org/json", [this](const Response& r) {
            FL_WARN("HTTP endpoint: " << (r.ok() ? "SUCCESS" : "FAILED"));
        });
        
        // HTTPS public endpoint (verified in production and AUTO mode)
        mClient.get_async("https://api.github.com/zen", [this](const Response& r) {
            FL_WARN("HTTPS public: " << (r.ok() ? "SUCCESS" : "FAILED"));
            if (!r.ok() && mIsProduction) {
                FL_WARN("  (May fail in production if certificate issues)");
            }
        });
        
        // HTTPS localhost (works in dev/AUTO, fails in production/YES)
        mClient.get_async("https://localhost:8443/health", [this](const Response& r) {
            FL_WARN("HTTPS localhost: " << (r.ok() ? "SUCCESS" : "FAILED"));
            if (!r.ok() && mIsProduction) {
                FL_WARN("  (Expected failure in production mode)");
            }
        });
        
        // Self-signed HTTPS (only works in development NO mode)
        mClient.get_async("https://self-signed.local/api", [this](const Response& r) {
            FL_WARN("Self-signed HTTPS: " << (r.ok() ? "SUCCESS" : "FAILED"));
            if (!r.ok()) {
                FL_WARN("  (Expected failure unless verification disabled)");
            }
        });
    }
    
    void print_verification_status() {
        auto stats = mClient.get_transport_stats();
        FL_WARN("=== Transport Verification Status ===");
        FL_WARN("Mode: " << (mIsProduction ? "PRODUCTION" : "DEVELOPMENT"));
        FL_WARN("Transport: " << stats.transport_type);
        FL_WARN("Active connections: " << stats.active_connections);
        
        // Get verification policy from transport
        // (In real implementation, you'd have a method to query this)
        FL_WARN("Verification policy: " << get_verification_policy_string());
    }
    
private:
    const char* get_verification_policy_string() const {
        if (mIsProduction) {
            return "YES (strict verification for all connections)";
        } else {
            return "AUTO (smart verification - public HTTPS verified, local/HTTP allowed)";
        }
    }
};

// Usage
ConfigurableClient client;

void setup() {
    bool is_production = false;  // Set based on your deployment
    
    #ifdef PRODUCTION_BUILD
        is_production = true;
    #endif
    
    client.initialize(is_production);
    client.test_various_endpoints();
}

void loop() {
    EVERY_N_SECONDS(60) {
        client.print_verification_status();
    }
    
    FastLED.show();
}
```

#### **5. Certificate Pinning with Verification**

```cpp
void certificate_pinning_example() {
    TcpTransport::Options opts;
    opts.verify = TransportVerify::YES;  // Enable strict verification
    opts.verify_hostname = true;
    
    // Pin specific certificates for critical APIs
    opts.pinned_certificates = {
        "b5d3c2a1e4f7g8h9i0j1k2l3m4n5o6p7q8r9s0t1u2v3w4x5y6z7a8b9c0d1e2f3",  // api.critical.com
        "f3e2d1c0b9a8g7h6i5j4k3l2m1n0o9p8q7r6s5t4u3v2w1x0y9z8a7b6c5d4e3f2"   // cdn.critical.com
    };
    
    auto transport = fl::make_unique<TcpTransport>(opts);
    auto client = HttpClient::create_with_transport(fl::move(transport));
    
    // These will only work if certificates match pinned fingerprints
    client.get_async("https://api.critical.com/data", [](const Response& r) {
        if (r.ok()) {
            FL_WARN("Pinned certificate verified successfully");
        } else {
            FL_WARN("Certificate pinning failed - potential MITM attack!");
        }
    });
}
```

### **Benefits of TransportVerify::AUTO (Default)**

**🎯 Smart Defaults for Different Environments:**

1. **🌐 Public HTTPS**: Full verification (secure by default)
2. **🏠 Local HTTPS**: No verification (development-friendly) 
3. **📡 HTTP**: No verification (appropriate for plaintext)
4. **🔒 Private Networks**: No verification (corporate environment friendly)

**✅ Perfect for:**
- **Development workflows** - works with self-signed certs on localhost
- **Production deployment** - secure by default for public APIs
- **Mixed environments** - handles internal and external APIs correctly
- **IoT devices** - flexible for local config + cloud telemetry

**⚡ Zero Configuration Required:**
```cpp
HttpClient client;  // Uses smart verification automatically!

// These all work with optimal security for each scenario:
client.get("http://localhost:8080/config");           // Fast, no verification needed
client.get("https://localhost:8443/admin");           // Local dev, no verification
client.get("https://api.github.com/user");            // Public API, full verification
client.get("https://192.168.1.100:8443/local-api");   // Private network, no verification
```

The `TransportVerify::AUTO` default provides **maximum security where it matters** while remaining **development-friendly** for local and private network scenarios! 🚀

### 2. HTTP Client with Transport Configuration

```cpp
namespace fl {

class HttpClient : public EngineEvents::Listener {
public:
    /// Create HTTP client with default TCP transport
    HttpClient() : HttpClient(fl::make_unique<TcpTransport>()) {}
    
    /// Create HTTP client with custom transport
    explicit HttpClient(fl::unique_ptr<Transport> transport) 
        : mTransport(fl::move(transport)) {}
    
    /// Destructor
    ~HttpClient();
    
    /// Factory methods for common transport configurations
    
    /// Create client optimized for local development (localhost only)
    /// @code
    /// auto client = HttpClient::create_local_client();
    /// client.get("http://localhost:8080/api");  // Fast local connections
    /// @endcode
    static fl::unique_ptr<HttpClient> create_local_client() {
        return fl::make_unique<HttpClient>(fl::make_unique<LocalTransport>());
    }
    
    /// Create client with IPv4-only transport
    /// @code
    /// auto client = HttpClient::create_ipv4_client();
    /// client.get("http://api.example.com/data");  // Forces IPv4
    /// @endcode
    static fl::unique_ptr<HttpClient> create_ipv4_client() {
        return fl::make_unique<HttpClient>(TcpTransport::create_ipv4_only());
    }
    
    /// Create client with IPv6-only transport
    /// @code
    /// auto client = HttpClient::create_ipv6_client();
    /// client.get("http://api.example.com/data");  // Forces IPv6
    /// @endcode
    static fl::unique_ptr<HttpClient> create_ipv6_client() {
        return fl::make_unique<HttpClient>(TcpTransport::create_ipv6_only());
    }
    
    /// Create client with dual-stack transport (IPv6 preferred, IPv4 fallback)
    /// @code
    /// auto client = HttpClient::create_dual_stack_client();
    /// client.get("http://api.example.com/data");  // Try IPv6 first, fallback to IPv4
    /// @endcode
    static fl::unique_ptr<HttpClient> create_dual_stack_client() {
        return fl::make_unique<HttpClient>(TcpTransport::create_dual_stack());
    }
    
    /// Create client with custom transport configuration
    /// @code
    /// TcpTransport::Options opts;
    /// opts.ip_version = IpVersion::IPV4_FIRST;
    /// opts.local_address = "192.168.1.100";  // Bind to specific local IP
    /// opts.limits.max_connections_per_host = 2;
    /// opts.limits.keepalive_timeout_ms = 30000;
    /// 
    /// auto transport = fl::make_unique<TcpTransport>(opts);
    /// auto client = HttpClient::create_with_transport(fl::move(transport));
    /// @endcode
    static fl::unique_ptr<HttpClient> create_with_transport(fl::unique_ptr<Transport> transport) {
        return fl::make_unique<HttpClient>(fl::move(transport));
    }
    
    /// Configure transport options for existing client
    /// @code
    /// TcpTransport::Options opts;
    /// opts.ip_version = IpVersion::IPV6_ONLY;
    /// opts.limits.connection_timeout_ms = 5000;
    /// client.configure_transport(opts);
    /// @endcode
    void configure_transport(const TcpTransport::Options& options) {
        if (auto* tcp_transport = dynamic_cast<TcpTransport*>(mTransport.get())) {
            tcp_transport->set_options(options);
        }
    }
    
    /// Get transport statistics
    /// @code
    /// auto stats = client.get_transport_stats();
    /// FL_WARN("Active connections: " << stats.active_connections);
    /// FL_WARN("Total created: " << stats.total_created);
    /// @endcode
    struct TransportStats {
        fl::size active_connections;
        fl::size total_connections_created;
        fl::string transport_type;
        fl::optional<TcpTransport::PoolStats> pool_stats; // Only for TcpTransport
    };
    TransportStats get_transport_stats() const {
        TransportStats stats;
        stats.active_connections = mTransport->get_active_connections();
        stats.total_connections_created = mTransport->get_total_connections_created();
        
        if (auto* tcp_transport = dynamic_cast<TcpTransport*>(mTransport.get())) {
            stats.transport_type = "TcpTransport";
            stats.pool_stats = tcp_transport->get_pool_stats();
        } else if (dynamic_cast<LocalTransport*>(mTransport.get())) {
            stats.transport_type = "LocalTransport";
        } else {
            stats.transport_type = "CustomTransport";
        }
        
        return stats;
    }
    
    /// Close all transport connections (useful for network changes)
    /// @code
    /// // WiFi reconnected, close old connections
    /// client.close_transport_connections();
    /// @endcode
    void close_transport_connections() {
        mTransport->close_all_connections();
    }

    // ========== HTTP API (unchanged - same simple interface) ==========
    
    /// HTTP GET request with customizable timeout and headers (returns future)
    fl::future<Response> get(const char* url, 
                             uint32_t timeout_ms = 5000,
                             fl::span<const fl::pair<fl::string, fl::string>> headers = {});
    
    /// HTTP POST request with customizable timeout and headers (returns future)
    fl::future<Response> post(const char* url, 
                              fl::span<const fl::u8> data,
                              uint32_t timeout_ms = 5000,
                              fl::span<const fl::pair<fl::string, fl::string>> headers = {});
    
    /// Async GET request with callback
    void get_async(const char* url, 
                   fl::function<void(const Response&)> callback,
                   uint32_t timeout_ms = 5000,
                   fl::span<const fl::pair<fl::string, fl::string>> headers = {});
    
    /// Async POST request with callback
    void post_async(const char* url, 
                    fl::span<const fl::u8> data,
                    fl::function<void(const Response&)> callback,
                    uint32_t timeout_ms = 5000,
                    fl::span<const fl::pair<fl::string, fl::string>> headers = {});
    
    // ... (all other HTTP methods remain unchanged)

private:
    fl::unique_ptr<Transport> mTransport;
    // ... (rest of private members)
};

} // namespace fl
```

### 4. Network Event Pumping Integration

```cpp
namespace fl {

class NetworkEventPump : public EngineEvents::Listener {
public:
    static NetworkEventPump& instance() {
        static NetworkEventPump pump;
        return pump;
    }
    
    void register_client(HttpClient* client) {
        auto it = fl::find(mClients.begin(), mClients.end(), client);
        if (it == mClients.end()) {
            mClients.push_back(client);
        }
    }
    
    void unregister_client(HttpClient* client) {
        auto it = fl::find(mClients.begin(), mClients.end(), client);
        if (it != mClients.end()) {
            mClients.erase(it);
        }
    }
    
    // Manual pumping for users who don't want EngineEvents integration
    static void pump_all_clients() {
        instance().pump_clients();
    }

private:
    NetworkEventPump() {
        EngineEvents::addListener(this, 10); // Low priority, run after UI
    }
    
    ~NetworkEventPump() {
        EngineEvents::removeListener(this);
    }
    
    // EngineEvents::Listener interface - pump after frame is drawn
    void onEndFrame() override {
        pump_clients();
    }
    
    void pump_clients() {
        for (auto* client : mClients) {
            client->pump_network_events();
        }
    }
    
    fl::vector<HttpClient*> mClients;
};

} // namespace fl
```

## Usage Examples

### 1. **Simplified API with Defaults (EASIEST)**

```cpp
#include "fl/networking/http_client.h"

using namespace fl;

HttpClient client;

void setup() {
    // SIMPLEST usage - just the URL! (uses 5s timeout, no headers)
    auto health_future = client.get("http://localhost:8899/health");
    
    // Simple POST with just URL and data
    fl::vector<fl::u8> sensor_data = get_sensor_readings();
    auto upload_future = client.post("http://localhost:8899/upload", sensor_data);
    
    // Add custom timeout when needed
    auto slow_api_future = client.get("http://slow-api.com/data", 15000);
    
    // Add headers when needed (using default 5s timeout)
    auto auth_future = client.get("http://api.weather.com/current", 5000, {
        {"Authorization", "Bearer weather_api_key"},
        {"Accept", "application/json"},
        {"User-Agent", "WeatherStation/1.0"}
    });
    
    // Store futures for checking in loop()
    pending_futures.push_back(fl::move(health_future));
    pending_futures.push_back(fl::move(upload_future));
    pending_futures.push_back(fl::move(slow_api_future));
    pending_futures.push_back(fl::move(auth_future));
}

void loop() {
    // Check all pending requests
    for (auto it = pending_futures.begin(); it != pending_futures.end();) {
        if (it->is_ready()) {
            auto result = it->try_result();
            if (result && result->ok()) {
                FL_WARN("Success: " << result->text());
            }
            it = pending_futures.erase(it);
        } else {
            ++it;
        }
    }
    
    FastLED.show();
}
```

### 2. **Callback API with Defaults**

```cpp
void setup() {
    // Ultra-simple callback API - just URL and callback!
    client.get_async("http://localhost:8899/status", [](const Response& response) {
        if (response.ok()) {
            FL_WARN("Status: " << response.text());
        }
    });
    
    // Simple POST with just URL, data, and callback
    fl::vector<fl::u8> telemetry = collect_telemetry();
    client.post_async("http://localhost:8899/telemetry", telemetry, [](const Response& response) {
        FL_WARN("Telemetry upload: " << response.status_code());
    });
    
    // Add headers when needed (using default timeout)
    client.get_async("http://api.example.com/user", [](const Response& response) {
        if (response.ok()) {
            FL_WARN("User data: " << response.text());
        }
    }, 5000, {
        {"Authorization", "Bearer user_token"},
        {"Accept", "application/json"}
    });
    
    // Custom timeout with headers
    client.post_async("http://slow-api.com/upload", telemetry, [](const Response& response) {
        FL_WARN("Slow upload: " << response.status_code());
    }, 20000, {
        {"Content-Type", "application/octet-stream"},
        {"X-Telemetry-Version", "1.0"}
    });
}

void loop() {
    // Callbacks fire automatically via EngineEvents
    FastLED.show();
}
```

### 3. **Mixed API Usage**

```cpp
void setup() {
    // No headers - simplest case
    auto simple_future = client.get("http://httpbin.org/json");
    
    // With headers using vector (when you need to build headers dynamically)
    fl::vector<fl::pair<fl::string, fl::string>> dynamic_headers;
    dynamic_headers.push_back({"Authorization", "Bearer " + get_current_token()});
    if (device_has_location()) {
        dynamic_headers.push_back({"X-Location", get_location_string()});
    }
    auto dynamic_future = client.get("http://api.example.com/location", dynamic_headers);
    
    // With headers using initializer list (when headers are known at compile time)
    auto static_future = client.get("http://api.example.com/config", {
        {"Authorization", "Bearer config_token"},
        {"Accept", "application/json"},
        {"Cache-Control", "no-cache"}
    });
}
```

### 4. **Real-World IoT Example**

```cpp
class IoTDevice {
private:
    HttpClient mClient;
    fl::string mDeviceToken;
    fl::string mDeviceId;
    
public:
    void authenticate(const char* token) { mDeviceToken = token; }
    void setDeviceId(const char* id) { mDeviceId = id; }
    
    void upload_sensor_data(fl::span<const fl::u8> data) {
        // Simple local upload (no auth needed)
        if (use_local_server) {
            auto future = mClient.post("http://192.168.1.100:8080/sensors", data);
            sensor_upload_future = fl::move(future);
            return;
        }
        
        // Cloud upload with custom timeout and auth headers
        auto future = mClient.post("https://iot.example.com/sensors", data, 10000, {
            {"Authorization", "Bearer " + mDeviceToken},
            {"Content-Type", "application/octet-stream"},
            {"X-Device-ID", mDeviceId},
            {"X-Timestamp", fl::to_string(millis())}
        });
        
        sensor_upload_future = fl::move(future);
    }
    
    void get_device_config() {
        // Local config (simple)
        if (use_local_server) {
            auto future = mClient.get("http://192.168.1.100:8080/config");
            config_future = fl::move(future);
            return;
        }
        
        // Cloud config with auth
        auto future = mClient.get("https://iot.example.com/device/config", 5000, {
            {"Authorization", "Bearer " + mDeviceToken},
            {"X-Device-ID", mDeviceId}
        });
        
        config_future = fl::move(future);
    }
    
    void ping_health_check() {
        // Simplest possible API usage!
        auto future = mClient.get("http://localhost:8899/health");
        health_future = fl::move(future);
    }
    
    void check_responses() {
        if (sensor_upload_future.is_ready()) {
            auto result = sensor_upload_future.try_result();
            FL_WARN("Sensor upload: " << (result && result->ok() ? "OK" : "FAILED"));
            sensor_upload_future = fl::future<Response>();
        }
        
        if (config_future.is_ready()) {
            auto result = config_future.try_result();
            if (result && result->ok()) {
                FL_WARN("Config: " << result->text());
                // Parse and apply config...
            }
            config_future = fl::future<Response>();
        }
        
        if (health_future.is_ready()) {
            auto result = health_future.try_result();
            FL_WARN("Health: " << (result && result->ok() ? "UP" : "DOWN"));
            health_future = fl::future<Response>();
        }
    }
    
private:
    fl::future<Response> sensor_upload_future;
    fl::future<Response> config_future;
    fl::future<Response> health_future;
    bool use_local_server = true; // Start with local, fall back to cloud
};

// Usage
IoTDevice device;

void setup() {
    device.authenticate("your_device_token");
    device.setDeviceId("fastled-sensor-001");
    
    // Health check every 10 seconds
    EVERY_N_SECONDS(10) {
        device.ping_health_check();
    }
    
    // Upload sensor reading every 30 seconds
    EVERY_N_SECONDS(30) {
        fl::vector<fl::u8> sensor_data = {temperature, humidity, light_level};
        device.upload_sensor_data(sensor_data);
    }
    
    // Get updated config every 5 minutes
    EVERY_N_SECONDS(300) {
        device.get_device_config();
    }
}

void loop() {
    device.check_responses();
    FastLED.show();
}
```

### 5. **Advanced: Multiple API Endpoints**

```cpp
class WeatherStationAPI {
private:
    HttpClient mClient;
    
public:
    void send_weather_data(float temp, float humidity, float pressure) {
        fl::vector<uint8_t> data;
        data.push_back(static_cast<uint8_t>(temp));
        data.push_back(static_cast<uint8_t>(humidity));
        data.push_back(static_cast<uint8_t>(pressure));
        
        // Multiple APIs with different headers - all using initializer lists!
        
        // Send to primary weather service
        mClient.post_async("https://weather.primary.com/upload", data, {
            {"Authorization", "Bearer primary_key"},
            {"Content-Type", "application/octet-stream"},
            {"X-Station-ID", "station-001"}
        }, [](const Response& response) {
            FL_WARN("Primary upload: " << response.status_code());
        });
        
        // Send to backup weather service  
        mClient.post_async("https://weather.backup.com/data", data, {
            {"API-Key", "backup_api_key"},
            {"Content-Type", "application/octet-stream"},
            {"Station-Name", "FastLED Weather Station"}
        }, [](const Response& response) {
            FL_WARN("Backup upload: " << response.status_code());
        });
        
        // Send to local logger
        mClient.post_async("http://192.168.1.100/log", data, {
            {"Content-Type", "application/octet-stream"}
        }, [](const Response& response) {
            FL_WARN("Local log: " << response.status_code());
        });
    }
};
```

### 6. **Streaming Downloads (Memory-Efficient)**

```cpp
class DataLogger {
private:
    HttpClient mClient;
    fl::size mTotalBytes = 0;
    
public:
    void download_large_dataset() {
        // Download 100MB dataset with only 1KB RAM buffer
        mTotalBytes = 0;
        
        mClient.get_stream_async("https://api.example.com/large-dataset", {
            {"Authorization", "Bearer " + api_token},
            {"Accept", "application/octet-stream"}
        }, 
        // Chunk callback - called for each piece of data
        [this](fl::span<const uint8_t> chunk, bool is_final) -> bool {
            // Process chunk immediately (only ~1KB in memory at a time)
            process_data_chunk(chunk);
            
            mTotalBytes += chunk.size();
            
            if (mTotalBytes % 10000 == 0) {
                FL_WARN("Downloaded: " << mTotalBytes << " bytes");
            }
            
            if (is_final) {
                FL_WARN("Download complete! Total: " << mTotalBytes << " bytes");
            }
            
            // Return true to continue, false to abort
            return true;
        },
        // Complete callback - called when done or error
        [this](int status_code, const char* error_msg) {
            if (status_code == 200) {
                FL_WARN("Stream completed successfully");
                finalize_data_processing();
            } else {
                FL_WARN("Stream error " << status_code << ": " << error_msg);
                cleanup_partial_data();
            }
        });
    }
    
private:
    void process_data_chunk(fl::span<const uint8_t> chunk) {
        // Write directly to SD card, process incrementally, etc.
        // Only this small chunk is in RAM at any time!
        for (const uint8_t& byte : chunk) {
            // Process each byte...
            analyze_sensor_data(byte);
        }
    }
    
    void finalize_data_processing() {
        FL_WARN("All data processed with minimal RAM usage!");
    }
    
    void cleanup_partial_data() {
        FL_WARN("Cleaning up after failed download");
    }
};
```

### 7. **Streaming Uploads (Sensor Data Logging)**

```cpp
class SensorDataUploader {
private:
    HttpClient mClient;
    fl::size mDataIndex = 0;
    fl::vector<uint8_t> mSensorHistory; // Large accumulated data
    
public:
    void upload_accumulated_data() {
        mDataIndex = 0;
        
        mClient.post_stream("https://api.example.com/sensor-bulk", {
            {"Content-Type", "application/octet-stream"},
            {"Authorization", "Bearer " + upload_token},
            {"X-Data-Source", "weather-station-001"}
        },
        // Data provider callback - called when client needs more data
        [this](fl::span<uint8_t> buffer) -> fl::size {
            fl::size remaining = mSensorHistory.size() - mDataIndex;
            fl::size to_copy = fl::min(buffer.size(), remaining);
            
            if (to_copy > 0) {
                // Copy next chunk into the provided buffer
                fl::memcpy(buffer.data(), 
                          mSensorHistory.data() + mDataIndex, 
                          to_copy);
                mDataIndex += to_copy;
                
                FL_WARN("Uploaded chunk: " << to_copy << " bytes");
            }
            
            // Return 0 when no more data (signals end of stream)
            return to_copy;
        },
        // Complete callback
        [this](int status_code, const char* error_msg) {
            if (status_code == 200) {
                FL_WARN("All sensor data uploaded successfully!");
                mSensorHistory.clear(); // Free memory
            } else {
                FL_WARN("Upload failed: " << status_code << " - " << error_msg);
                // Could retry later...
            }
        });
    }
    
    void add_sensor_reading(float temperature, float humidity) {
        // Accumulate data over time
        uint8_t temp_byte = static_cast<uint8_t>(temperature);
        uint8_t humidity_byte = static_cast<uint8_t>(humidity);
        
        mSensorHistory.push_back(temp_byte);
        mSensorHistory.push_back(humidity_byte);
        
        // Upload when we have enough data
        if (mSensorHistory.size() > 1000) {
            upload_accumulated_data();
        }
    }
};
```

### 8. **Real-Time Data Processing Stream**

```cpp
class RealTimeProcessor {
private:
    HttpClient mClient;
    fl::vector<float> mMovingAverage;
    fl::size mWindowSize = 100;
    
public:
    void start_realtime_stream() {
        // Connect to real-time sensor data stream
        mClient.get_stream_async("https://api.realtime.com/sensor-feed", {
            {"Authorization", "Bearer realtime_token"},
            {"Accept", "application/octet-stream"},
            {"X-Stream-Type", "continuous"}
        },
        // Process each chunk of real-time data
        [this](fl::span<const uint8_t> chunk, bool is_final) -> bool {
            // Convert binary data to sensor readings
            for (fl::size i = 0; i + 3 < chunk.size(); i += 4) {
                float sensor_value = parse_float_from_bytes(chunk.subspan(i, 4));
                
                // Update moving average
                mMovingAverage.push_back(sensor_value);
                if (mMovingAverage.size() > mWindowSize) {
                    mMovingAverage.erase(mMovingAverage.begin());
                }
                
                // Process in real-time
                process_sensor_value(sensor_value);
                
                // Update LED display based on current average
                update_led_visualization();
            }
            
            // Continue streaming unless we want to stop
            return !should_stop_stream();
        },
        // Handle stream completion or errors
        [](int status_code, const char* error_msg) {
            if (status_code != 200) {
                FL_WARN("Stream error: " << status_code << " - " << error_msg);
                // Could automatically reconnect here
            }
        });
    }
    
private:
    float parse_float_from_bytes(fl::span<const uint8_t> bytes) {
        // Convert 4 bytes to float (handle endianness)
        union { uint32_t i; float f; } converter;
        converter.i = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
        return converter.f;
    }
    
    void process_sensor_value(float value) {
        // Real-time processing logic
        if (value > threshold) {
            trigger_alert();
        }
    }
    
    void update_led_visualization() {
        float avg = calculate_average();
        // Update FastLED display based on current data
        update_leds_with_value(avg);
    }
    
    bool should_stop_stream() {
        // Logic to determine when to stop streaming
        return false; // Run continuously in this example
    }
};
```

### 9. **Secure HTTPS Communication**

```cpp
class SecureIoTDevice {
private:
    HttpClient mClient;
    fl::string mDeviceId;
    fl::string mApiToken;
    
public:
    void setup_security() {
        // Configure TLS for maximum security
        HttpClient::TlsConfig tls_config;
        tls_config.verify_certificates = true;      // Always verify server certs
        tls_config.verify_hostname = true;          // Verify hostname matches cert
        tls_config.allow_self_signed = false;       // No self-signed certs
        tls_config.require_tls_v12_or_higher = true; // Modern TLS only
        mClient.set_tls_config(tls_config);
        
        // Load trusted root certificates
        int cert_count = mClient.load_ca_bundle(HttpClient::CA_BUNDLE_IOT_COMMON);
        FL_WARN("Loaded " << cert_count << " trusted certificates");
        
        // Pin certificate for critical API endpoint (optional, highest security)
        mClient.pin_certificate("api.critical-iot.com", 
                                "b5d3c2a1e4f7g8h9i0j1k2l3m4n5o6p7q8r9s0t1u2v3w4x5y6z7a8b9c0d1e2f3");
        
        // Save certificates to persistent storage
        mClient.save_certificates("/spiffs/ca_certs.bin");
    }
    
    void send_secure_telemetry() {
        fl::vector<uint8_t> encrypted_data = encrypt_sensor_readings();
        
        // All HTTPS calls automatically use TLS with configured security
        auto future = mClient.post("https://api.secure-iot.com/telemetry", encrypted_data, {
            {"Authorization", "Bearer " + mApiToken},
            {"Content-Type", "application/octet-stream"},
            {"X-Device-ID", mDeviceId},
            {"X-Security-Level", "high"}
        });
        
        // Check TLS connection details
        if (future.is_ready()) {
            auto result = future.try_result();
            if (result) {
                auto tls_info = mClient.get_last_tls_info();
                FL_WARN("TLS Version: " << tls_info.tls_version);
                FL_WARN("Cipher: " << tls_info.cipher_suite);
                FL_WARN("Cert Verified: " << (tls_info.certificate_verified ? "YES" : "NO"));
                FL_WARN("Hostname Verified: " << (tls_info.hostname_verified ? "YES" : "NO"));
            }
        }
    }
    
    void handle_certificate_update() {
        // Download and verify new certificate bundle
        mClient.get_stream_async("https://ca-updates.iot-service.com/latest-certs", {
            {"Authorization", "Bearer " + mApiToken},
            {"Accept", "application/x-pem-file"}
        },
        [this](fl::span<const uint8_t> chunk, bool is_final) -> bool {
            // Process certificate data as it arrives
            process_certificate_chunk(chunk);
            
            if (is_final) {
                // Validate and install new certificates
                if (validate_certificate_bundle()) {
                    mClient.clear_certificates();
                    install_new_certificates();
                    mClient.save_certificates("/spiffs/ca_certs.bin");
                    FL_WARN("Certificate bundle updated successfully");
                } else {
                    FL_WARN("Certificate validation failed - keeping old certificates");
                }
            }
            return true;
        },
        [](int status, const char* msg) {
            FL_WARN("Certificate update complete: " << status);
        });
    }
    
private:
    fl::vector<uint8_t> encrypt_sensor_readings() {
        // Your encryption logic here
        return fl::vector<uint8_t>{1, 2, 3, 4}; // Placeholder
    }
    
    void process_certificate_chunk(fl::span<const uint8_t> chunk) {
        // Accumulate certificate data
    }
    
    bool validate_certificate_bundle() {
        // Verify certificate signatures, validity dates, etc.
        return true; // Placeholder
    }
    
    void install_new_certificates() {
        // Install validated certificates
    }
};
```

### 10. **Development vs Production Security**

```cpp
class ConfigurableSecurityClient {
private:
    HttpClient mClient;
    bool mDevelopmentMode;
    
public:
    void setup(bool development_mode = false) {
        mDevelopmentMode = development_mode;
        
        HttpClient::TlsConfig tls_config;
        
        if (development_mode) {
            // Development settings - more permissive
            tls_config.verify_certificates = false;     // Allow dev servers
            tls_config.verify_hostname = false;         // Allow localhost
            tls_config.allow_self_signed = true;        // Allow self-signed certs
            tls_config.require_tls_v12_or_higher = false; // Allow older TLS for testing
            
            FL_WARN("DEVELOPMENT MODE: TLS verification disabled!");
            FL_WARN("DO NOT USE IN PRODUCTION!");
            
        } else {
            // Production settings - maximum security
            tls_config.verify_certificates = true;
            tls_config.verify_hostname = true;
            tls_config.allow_self_signed = false;
            tls_config.require_tls_v12_or_higher = true;
            
            // Load production certificate bundle
            mClient.load_ca_bundle(HttpClient::CA_BUNDLE_MOZILLA);
            
            // Pin critical certificates
            pin_production_certificates();
            
            FL_WARN("PRODUCTION MODE: Full TLS verification enabled");
        }
        
        mClient.set_tls_config(tls_config);
    }
    
    void test_api_connection() {
        const char* api_url = mDevelopmentMode ? 
            "https://localhost:8443/api/test" :      // Dev server
            "https://api.production.com/test";       // Production server
            
        mClient.get_async(api_url, {
            {"Authorization", "Bearer " + get_api_token()},
            {"User-Agent", "FastLED-Device/1.0"}
        }, [this](const Response& response) {
            if (response.ok()) {
                auto tls_info = mClient.get_last_tls_info();
                FL_WARN("API test successful");
                FL_WARN("Security level: " << (mDevelopmentMode ? "DEV" : "PRODUCTION"));
                FL_WARN("TLS: " << tls_info.tls_version << " with " << tls_info.cipher_suite);
            } else {
                FL_WARN("API test failed: " << response.status_code());
                if (!mDevelopmentMode) {
                    FL_WARN("Check certificate configuration");
                }
            }
        });
    }
    
private:
    void pin_production_certificates() {
        // Pin certificates for critical production services
        mClient.pin_certificate("api.production.com", 
                                "a1b2c3d4e5f6...production_cert_sha256");
        mClient.pin_certificate("cdn.production.com", 
                                "f6e5d4c3b2a1...cdn_cert_sha256");
    }
    
    fl::string get_api_token() {
        return mDevelopmentMode ? "dev_token_123" : "prod_token_xyz";
    }
};
```

### 11. **Certificate Management & Updates**

```cpp
class CertificateManager {
private:
    HttpClient* mClient;
    fl::string mCertStoragePath;
    
public:
    CertificateManager(HttpClient* client, const char* storage_path = "/spiffs/certs.bin") 
        : mClient(client), mCertStoragePath(storage_path) {}
    
    void initialize() {
        // Try loading saved certificates first
        int loaded = mClient->load_certificates(mCertStoragePath.c_str());
        
        if (loaded == 0) {
            FL_WARN("No saved certificates found, loading defaults");
            load_default_certificates();
        } else {
            FL_WARN("Loaded " << loaded << " certificates from storage");
            
            // Check if certificates need updating (once per day)
            EVERY_N_HOURS(24) {
                check_for_certificate_updates();
            }
        }
        
        // Display certificate statistics
        print_certificate_stats();
    }
    
    void load_default_certificates() {
        // Load minimal set for common IoT services
        int count = mClient->load_ca_bundle(HttpClient::CA_BUNDLE_IOT_COMMON);
        FL_WARN("Loaded " << count << " default certificates");
        
        // Add custom root CA if needed
        add_custom_root_ca();
        
        // Save for next boot
        mClient->save_certificates(mCertStoragePath.c_str());
    }
    
    void add_custom_root_ca() {
        // Add your organization's root CA
        const char* custom_ca = R"(
-----BEGIN CERTIFICATE-----
MIIDXTCCAkWgAwIBAgIJAKX1+... (your custom root CA)
...certificate content...
-----END CERTIFICATE-----
)";
        
        if (mClient->add_root_certificate(custom_ca, "Company Root CA")) {
            FL_WARN("Added custom root CA");
        } else {
            FL_WARN("Failed to add custom root CA");
        }
    }
    
    void check_for_certificate_updates() {
        FL_WARN("Checking for certificate updates...");
        
        // Use HTTPS to download certificate updates (secured by existing certs)
        mClient->get_async("https://ca-updates.yourservice.com/latest", {
            {"User-Agent", "FastLED-CertUpdater/1.0"},
            {"Accept", "application/json"}
        }, [this](const Response& response) {
            if (response.ok()) {
                // Parse JSON response to check for updates
                if (parse_update_info(response.text())) {
                    download_certificate_updates();
                }
            } else {
                FL_WARN("Certificate update check failed: " << response.status_code());
            }
        });
    }
    
    void download_certificate_updates() {
        FL_WARN("Downloading certificate updates...");
        
        fl::vector<uint8_t> cert_data;
        
        mClient->get_stream_async("https://ca-updates.yourservice.com/bundle.pem", {
            {"Authorization", "Bearer " + get_update_token()}
        },
        [&cert_data](fl::span<const uint8_t> chunk, bool is_final) -> bool {
            // Accumulate certificate data
            for (const uint8_t& byte : chunk) {
                cert_data.push_back(byte);
            }
            
            if (is_final) {
                FL_WARN("Downloaded " << cert_data.size() << " bytes of certificate data");
            }
            return true;
        },
        [this, &cert_data](int status, const char* msg) {
            if (status == 200) {
                process_certificate_update(cert_data);
            } else {
                FL_WARN("Certificate download failed: " << status << " - " << msg);
            }
        });
    }
    
    void print_certificate_stats() {
        auto stats = mClient->get_certificate_stats();
        FL_WARN("Certificate Statistics:");
        FL_WARN("  Certificates: " << stats.certificate_count);
        FL_WARN("  Total size: " << stats.total_certificate_size << " bytes");
        FL_WARN("  Pinned certs: " << stats.pinned_certificate_count);
        FL_WARN("  Capacity: " << stats.max_certificate_capacity);
    }
    
private:
    bool parse_update_info(const char* json_response) {
        // Parse JSON to check version, availability, etc.
        // Return true if update is needed
        return false; // Placeholder
    }
    
    fl::string get_update_token() {
        // Return authentication token for certificate updates
        return "cert_update_token_123";
    }
    
    void process_certificate_update(const fl::vector<uint8_t>& cert_data) {
        // Validate signature, parse certificates, update store
        FL_WARN("Processing certificate update...");
        
        // For demo, just save the new certificates
        // In practice, you'd validate signatures, check dates, etc.
        if (validate_certificate_signature(cert_data)) {
            backup_current_certificates();
            install_new_certificates(cert_data);
            mClient->save_certificates(mCertStoragePath.c_str());
            FL_WARN("Certificates updated successfully");
        } else {
            FL_WARN("Certificate validation failed - update rejected");
        }
    }
    
    bool validate_certificate_signature(const fl::vector<uint8_t>& cert_data) {
        // Verify certificate bundle signature
        return true; // Placeholder
    }
    
    void backup_current_certificates() {
        // Backup current certificates before updating
        fl::string backup_path = mCertStoragePath + ".backup";
        mClient->save_certificates(backup_path.c_str());
    }
    
    void install_new_certificates(const fl::vector<uint8_t>& cert_data) {
        // Clear existing and install new certificates
        mClient->clear_certificates();
        
        // Parse and add each certificate from the bundle
        // (In practice, you'd parse the PEM format)
        
        FL_WARN("New certificates installed");
    }
};
```

## TLS Implementation Details

### **Platform-Specific TLS Support**

#### **ESP32: mbedTLS Integration**

```cpp
#ifdef ESP32
// Internal implementation uses ESP-IDF mbedTLS
class ESP32TlsTransport : public TlsTransport {
    mbedtls_ssl_context ssl_ctx;
    mbedtls_net_context net_ctx;
    mbedtls_ssl_config ssl_config;
    mbedtls_x509_crt ca_chain;
    
public:
    bool connect(const char* hostname, int port) override {
        // Use ESP32 mbedTLS for TLS connections
        mbedtls_ssl_init(&ssl_ctx);
        mbedtls_ssl_config_init(&ssl_config);
        mbedtls_x509_crt_init(&ca_chain);
        
        // Configure for TLS client
        mbedtls_ssl_config_defaults(&ssl_config, MBEDTLS_SSL_IS_CLIENT,
                                   MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
        
        // Set CA certificates
        mbedtls_ssl_conf_ca_chain(&ssl_config, &ca_chain, nullptr);
        
        // Enable hostname verification
        mbedtls_ssl_conf_authmode(&ssl_config, MBEDTLS_SSL_VERIFY_REQUIRED);
        
        return perform_handshake(hostname, port);
    }
};
#endif
```

#### **WebAssembly: Browser TLS**

```cpp
#ifdef __EMSCRIPTEN__
// Use browser's native TLS implementation
class EmscriptenTlsTransport : public TlsTransport {
public:
    bool connect(const char* hostname, int port) override {
        // Browser handles TLS automatically for HTTPS URLs
        // Certificate validation done by browser's certificate store
        
        EM_ASM({
            // JavaScript code to handle TLS connection
            const url = UTF8ToString($0) + ":" + $1;
            console.log("Connecting to: " + url);
            
            // Browser's fetch() API handles TLS automatically
        }, hostname, port);
        
        return true; // Browser handles all TLS details
    }
    
    // Certificate pinning via browser APIs
    void pin_certificate(const char* hostname, const char* sha256) override {
        EM_ASM({
            const host = UTF8ToString($0);
            const fingerprint = UTF8ToString($1);
            
            // Use browser's SubResource Integrity or other mechanisms
            console.log("Pinning certificate for " + host + ": " + fingerprint);
        }, hostname, sha256);
    }
};
#endif
```

#### **Native Platforms: OpenSSL/mbedTLS**

```cpp
#if !defined(ESP32) && !defined(__EMSCRIPTEN__)
// Use system OpenSSL or bundled mbedTLS
class NativeTlsTransport : public TlsTransport {
    SSL_CTX* ssl_ctx = nullptr;
    SSL* ssl = nullptr;
    X509_STORE* cert_store = nullptr;
    
public:
    bool connect(const char* hostname, int port) override {
        // Initialize OpenSSL
        SSL_library_init();
        ssl_ctx = SSL_CTX_new(TLS_client_method());
        
        // Load CA certificates
        SSL_CTX_set_cert_store(ssl_ctx, cert_store);
        
        // Enable hostname verification
        SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, nullptr);
        
        return perform_ssl_handshake(hostname, port);
    }
};
#endif
```

### **Memory Management for Certificates**

#### **Compressed Certificate Storage**

```cpp
class CompressedCertificateStore {
private:
    struct CertificateEntry {
        fl::string name;
        fl::vector<uint8_t> compressed_cert;
        fl::string sha256_fingerprint;
        uint32_t expiry_timestamp;
    };
    
    fl::vector<CertificateEntry> mCertificates;
    fl::size mMaxStorageSize = 32768; // 32KB default
    
public:
    bool add_certificate(const char* pem_cert, const char* name) {
        // Parse PEM certificate
        auto cert_der = parse_pem_to_der(pem_cert);
        
        // Compress using simple RLE or zlib if available
        auto compressed = compress_certificate(cert_der);
        
        // Check if we have space
        if (get_total_size() + compressed.size() > mMaxStorageSize) {
            FL_WARN("Certificate store full, removing oldest");
            remove_oldest_certificate();
        }
        
        CertificateEntry entry;
        entry.name = name ? name : "Unknown";
        entry.compressed_cert = fl::move(compressed);
        entry.sha256_fingerprint = calculate_sha256(cert_der);
        entry.expiry_timestamp = extract_expiry_date(cert_der);
        
        mCertificates.push_back(fl::move(entry));
        return true;
    }
    
    fl::vector<uint8_t> get_certificate(const fl::string& fingerprint) {
        for (const auto& entry : mCertificates) {
            if (entry.sha256_fingerprint == fingerprint) {
                return decompress_certificate(entry.compressed_cert);
            }
        }
        return fl::vector<uint8_t>();
    }
    
private:
    fl::vector<uint8_t> compress_certificate(const fl::vector<uint8_t>& cert_der) {
        // Simple compression for certificates (they compress well)
        return cert_der; // Placeholder - use RLE or zlib
    }
    
    fl::vector<uint8_t> decompress_certificate(const fl::vector<uint8_t>& compressed) {
        return compressed; // Placeholder
    }
    
    fl::size get_total_size() const {
        fl::size total = 0;
        for (const auto& cert : mCertificates) {
            total += cert.compressed_cert.size();
        }
        return total;
    }
    
    void remove_oldest_certificate() {
        if (!mCertificates.empty()) {
            mCertificates.erase(mCertificates.begin());
        }
    }
};
```

### **TLS Error Handling**

```cpp
enum class TlsError {
    SUCCESS = 0,
    CERTIFICATE_VERIFICATION_FAILED,
    HOSTNAME_VERIFICATION_FAILED,
    CIPHER_NEGOTIATION_FAILED,
    HANDSHAKE_TIMEOUT,
    CERTIFICATE_EXPIRED,
    CERTIFICATE_NOT_TRUSTED,
    TLS_VERSION_NOT_SUPPORTED,
    MEMORY_ALLOCATION_FAILED,
    NETWORK_CONNECTION_FAILED
};

const char* tls_error_to_string(TlsError error) {
    switch (error) {
        case TlsError::SUCCESS: return "Success";
        case TlsError::CERTIFICATE_VERIFICATION_FAILED: return "Certificate verification failed";
        case TlsError::HOSTNAME_VERIFICATION_FAILED: return "Hostname verification failed";
        case TlsError::CIPHER_NEGOTIATION_FAILED: return "Cipher negotiation failed";
        case TlsError::HANDSHAKE_TIMEOUT: return "TLS handshake timeout";
        case TlsError::CERTIFICATE_EXPIRED: return "Certificate expired";
        case TlsError::CERTIFICATE_NOT_TRUSTED: return "Certificate not trusted";
        case TlsError::TLS_VERSION_NOT_SUPPORTED: return "TLS version not supported";
        case TlsError::MEMORY_ALLOCATION_FAILED: return "Memory allocation failed";
        case TlsError::NETWORK_CONNECTION_FAILED: return "Network connection failed";
        default: return "Unknown TLS error";
    }
}

// Enhanced error reporting in callbacks
client.get_async("https://api.example.com/data", {},
    [](const Response& response) {
        if (!response.ok()) {
            auto tls_info = client.get_last_tls_info();
            if (tls_info.is_secure && !tls_info.certificate_verified) {
                FL_WARN("TLS Error: Certificate verification failed");
                FL_WARN("Server cert: " << tls_info.server_certificate_sha256);
                FL_WARN("Consider adding root CA or pinning certificate");
            }
        }
    }
);
```

This comprehensive TLS design provides **enterprise-grade security** while maintaining FastLED's **simple, embedded-friendly approach**! 🔒

### **API Design Philosophy: Progressive Complexity**

**🎯 Start Simple, Add Complexity Only When Needed:**

The FastLED HTTP client follows a **progressive complexity** design where the simplest use cases require the least code:

**Level 1 - Minimal (Just URL):**
```cpp
client.get("http://localhost:8899/health");                    // Uses 5s timeout, no headers
client.post("http://localhost:8899/upload", sensor_data);     // Uses 5s timeout, no headers
```

**Level 2 - Custom Timeout:**
```cpp
client.get("http://slow-api.com/data", 15000);                // 15s timeout, no headers
client.post("http://slow-api.com/upload", data, 20000);       // 20s timeout, no headers
```

**Level 3 - Add Headers (Common Case):**
```cpp
client.get("http://api.com/data", 5000, {                     // 5s timeout, with headers
    {"Authorization", "Bearer token"}
});
client.post("http://api.com/upload", data, 10000, {           // 10s timeout, with headers
    {"Content-Type", "application/json"}
});
```

**Benefits of This Approach:**
- **✅ Zero barrier to entry** - health checks and simple APIs work with minimal code
- **✅ Sensible defaults** - 5 second timeout works for most local/fast APIs
- **✅ Incremental complexity** - add timeouts/headers only when actually needed
- **✅ Familiar C++ pattern** - default parameters are a well-understood C++ idiom

### **Why fl::string Shared Buffer Semantics Are Perfect for Networking**

**fl::string Benefits from Codebase Analysis:**
- **Copy-on-write semantics**: Copying `fl::string` only copies a shared_ptr (8-16 bytes)
- **Small string optimization**: Strings ≤64 bytes stored inline (no heap allocation)  
- **Shared ownership**: Multiple strings can share the same buffer efficiently
- **Automatic memory management**: RAII cleanup via shared_ptr
- **Thread-safe sharing**: Safe to share between threads with copy-on-write

**Strategic API Design Decisions:**

**✅ Use `const fl::string&` for:**
- **Return values** that might be stored/shared (response text, error messages)
- **Error messages** in callbacks (efficient sharing across multiple futures)
- **HTTP headers** that might be cached or logged
- **Large content** that benefits from shared ownership
- **Any data the receiver might store**

**✅ Keep `const char*` for:**
- **URLs** (usually string literals, consumed immediately)
- **HTTP methods** ("GET", "POST" - compile-time constants)
- **Immediate consumption** parameters (no storage needed)
- **Legacy C-style access** when raw char* is required

**Memory Efficiency Examples:**

```cpp
// Shared error messages across multiple futures - only ONE copy in memory!
fl::string network_timeout_error("Connection timeout after 30 seconds");

auto future1 = fl::make_error_future<Response>(network_timeout_error);
auto future2 = fl::make_error_future<Response>(network_timeout_error); 
auto future3 = fl::make_error_future<Response>(network_timeout_error);
// All three futures share the same error string buffer via copy-on-write!

// Response text sharing
void handle_response(const Response& response) {
    // Efficient sharing - no string copy!
    fl::string cached_response = response.text(); // Cheap shared_ptr copy
    
    // Can store, log, display, etc. without memory waste
    log_response(cached_response);
    display_on_screen(cached_response);
    cache_for_later(cached_response);
    // All share the same underlying buffer!
}

// Error message propagation in callbacks
client.get_stream_async("http://api.example.com/data", {},
    [](fl::span<const fl::u8> chunk, bool is_final) { return true; },
    [](int status, const fl::string& error_msg) {
        // Can efficiently store/share error message
        last_error = error_msg;        // Cheap copy
        log_error(error_msg);          // Can share same buffer
        display_error(error_msg);      // No extra memory used
        // Single string buffer shared across all uses!
    }
);
```

**Embedded System Benefits:**

1. **Memory Conservation**: Shared buffers reduce memory fragmentation
2. **Cache Efficiency**: Multiple references to same data improve cache locality
3. **Copy Performance**: Copy-on-write makes string passing nearly free
4. **RAII Safety**: Automatic cleanup prevents memory leaks
5. **Thread Safety**: Safe sharing between threads without manual synchronization

### **API Design Pattern Summary**

The FastLED networking API uses consistent naming patterns to make the right choice obvious:

**📊 Complete API Overview:**

| **Method Pattern** | **Returns** | **Use Case** | **Example** |
|-------------------|-------------|--------------|-------------|
| `get(url, timeout=5000, headers={})` | `fl::future<Response>` | Single HTTP request, check result later | Weather data, API calls |
| `get_async(url, callback, timeout=5000, headers={})` | `void` | Single HTTP request, immediate callback | Fire-and-forget requests |
| `get_stream_async(url, chunk_cb, done_cb)` | `void` | Large data, real-time streaming | File downloads, sensor feeds |
| `post(url, data, timeout=5000, headers={})` | `fl::future<Response>` | Single upload, check result later | Sensor data upload |
| `post_async(url, data, callback, timeout=5000, headers={})` | `void` | Single upload, immediate callback | Quick data submission |
| `post_stream_async(url, provider, done_cb)` | `void` | Large uploads, chunked sending | Log files, large datasets |

**🎯 When to Use Each Pattern:**

**✅ Use `get()` / `post()` (Futures) When:**
- You need to check the result later in `loop()`
- You want to store/cache the response
- You need error handling flexibility
- You're doing multiple concurrent requests

**✅ Use `get_async()` / `post_async()` (Callbacks) When:**
- You want immediate response processing
- The response is simple (success/failure)
- You don't need to store the response
- Fire-and-forget pattern is acceptable

**✅ Use `get_stream_async()` / `post_stream_async()` (Streaming) When:**
- Data is too large for memory (>1KB on embedded)
- You need real-time processing (sensor feeds)
- Progress monitoring is important
- Memory efficiency is critical

**🔧 Practical Decision Guide:**

```cpp
// ✅ Perfect for futures - check result when convenient
auto health_future =
