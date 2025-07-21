#pragma once

#ifdef FASTLED_HAS_NETWORKING

#include "fl/net/http/types.h"
#include "fl/net/socket.h"
#include "fl/function.h"
#include "fl/future.h"
#include "fl/shared_ptr.h"
#include "fl/stdint.h"
#include "fl/string.h"
#include "fl/mutex.h"
#include "fl/hash_map.h"

namespace fl {

/// HTTP transport interface for different networking backends
class Transport {
public:
    virtual ~Transport() = default;
    
    /// Send HTTP request and receive response
    virtual fl::future<Response> send_request(const Request& request) = 0;
    
    /// Send request asynchronously (all requests are async now)
    virtual fl::future<Response> send_request_async(const Request& request) = 0;
    
    /// Check if transport supports the given URL scheme
    virtual bool supports_scheme(const fl::string& scheme) const = 0;
    
    /// Transport capabilities
    virtual bool supports_streaming() const = 0;
    virtual bool supports_keepalive() const = 0;
    virtual bool supports_compression() const = 0;
    virtual bool supports_ssl() const = 0;
    
    /// Connection management
    virtual fl::size get_active_connections() const = 0;
    virtual void close_all_connections() = 0;
    
    /// Transport info
    virtual fl::string get_transport_name() const = 0;
    virtual fl::string get_transport_version() const = 0;
    
    /// Stream download with custom processor
    virtual fl::future<bool> stream_download(const Request& request,
                                            fl::function<bool(fl::span<const fl::u8>)> data_processor) = 0;
    
    /// Stream upload with custom provider
    virtual fl::future<Response> stream_upload(const Request& request,
                                               fl::function<fl::optional<fl::span<const fl::u8>>()> data_provider) = 0;
};

/// Transport error enumeration
enum class TransportError {
    SUCCESS,
    NETWORK_ERROR,
    TIMEOUT,
    SSL_ERROR,
    INVALID_URL,
    INVALID_RESPONSE,
    TOO_MANY_REDIRECTS,
    RESPONSE_TOO_LARGE,
    CONNECTION_FAILED,
    UNSUPPORTED_SCHEME,
    PROTOCOL_ERROR,
    UNKNOWN_ERROR
};

/// Convert transport error to string
fl::string to_string(TransportError error);

/// Transport statistics
struct TransportStats {
    fl::size total_requests = 0;
    fl::size successful_requests = 0;
    fl::size failed_requests = 0;
    fl::size redirects_followed = 0;
    fl::size bytes_sent = 0;
    fl::size bytes_received = 0;
    fl::u32 average_response_time_ms = 0;
    fl::u32 last_request_time_ms = 0;
    fl::u32 active_connections = 0;
    fl::u32 total_connections = 0;
};

/// Base transport implementation with common functionality
class BaseTransport : public Transport {
public:
    BaseTransport();
    virtual ~BaseTransport() = default;
    
    // Common functionality
    
    /// Get transport statistics
    const TransportStats& get_stats() const { return mStats; }
    void reset_stats();
    
    /// Error handling
    TransportError get_last_error() const { return mLastError; }
    fl::string get_last_error_message() const { return mLastErrorMessage; }
    
    /// Configuration
    void set_timeout(fl::u32 timeout_ms) { mTimeoutMs = timeout_ms; }
    fl::u32 get_timeout() const { return mTimeoutMs; }
    
    void set_connect_timeout(fl::u32 timeout_ms) { mConnectTimeoutMs = timeout_ms; }
    fl::u32 get_connect_timeout() const { return mConnectTimeoutMs; }
    
    void set_max_response_size(fl::size max_size) { mMaxResponseSize = max_size; }
    fl::size get_max_response_size() const { return mMaxResponseSize; }
    
    void set_follow_redirects(bool follow) { mFollowRedirects = follow; }
    bool get_follow_redirects() const { return mFollowRedirects; }
    
    void set_max_redirects(fl::size max_redirects) { mMaxRedirects = max_redirects; }
    fl::size get_max_redirects() const { return mMaxRedirects; }
    
protected:
    /// Statistics tracking
    mutable TransportStats mStats;
    
    /// Error handling
    TransportError mLastError = TransportError::SUCCESS;
    fl::string mLastErrorMessage;
    
    /// Configuration
    fl::u32 mTimeoutMs = 10000;
    fl::u32 mConnectTimeoutMs = 5000;
    fl::size mMaxResponseSize = 10485760; // 10MB
    bool mFollowRedirects = true;
    fl::size mMaxRedirects = 5;
    
    /// Update statistics
    void update_stats_request_start();
    void update_stats_request_success(fl::size bytes_sent, fl::size bytes_received, fl::u32 duration_ms);
    void update_stats_request_failure();
    void update_stats_redirect();
    
    /// Error handling helpers
    void set_error(TransportError error, const fl::string& message = "");
    void clear_error();
    
    /// Redirect handling
    fl::future<Response> handle_redirects(const Request& original_request, const Response& response);
    bool should_follow_redirect(const Response& response) const;
    fl::optional<Request> build_redirect_request(const Request& original_request, const Response& response);
    
    /// Response validation
    bool validate_response(const Response& response);
    bool check_response_size(fl::size content_length);
    
    /// Timing utilities
    fl::u32 get_current_time_ms();
    
private:
    fl::u32 mRequestStartTime = 0;
    fl::size mCurrentRedirectCount = 0;
};

/// Connection pool interface for transport implementations
class ConnectionPool {
public:
    virtual ~ConnectionPool() = default;
    
    /// Get connection for host:port
    virtual fl::shared_ptr<Socket> get_connection(const fl::string& host, int port) = 0;
    
    /// Return connection to pool (for reuse)
    virtual void return_connection(fl::shared_ptr<Socket> socket, const fl::string& host, int port) = 0;
    
    /// Close all connections
    virtual void close_all_connections() = 0;
    
    /// Get pool statistics
    virtual fl::size get_active_connections() const = 0;
    virtual fl::size get_total_connections() const = 0;
    
    /// Configuration
    virtual void set_max_connections_per_host(fl::size max_connections) = 0;
    virtual void set_max_total_connections(fl::size max_connections) = 0;
    virtual void set_connection_timeout(fl::u32 timeout_ms) = 0;
};

/// Simple connection pool implementation
class SimpleConnectionPool : public ConnectionPool {
public:
    struct Options {
        fl::size max_connections_per_host;
        fl::size max_total_connections;
        fl::u32 connection_timeout_ms;
        bool enable_keepalive;
        
        // Constructor with defaults
        Options() 
            : max_connections_per_host(5)
            , max_total_connections(50)
            , connection_timeout_ms(30000)
            , enable_keepalive(true) {}
    };
    
    explicit SimpleConnectionPool(const Options& options = {});
    ~SimpleConnectionPool() override;
    
    // ConnectionPool interface
    fl::shared_ptr<Socket> get_connection(const fl::string& host, int port) override;
    void return_connection(fl::shared_ptr<Socket> socket, const fl::string& host, int port) override;
    void close_all_connections() override;
    fl::size get_active_connections() const override;
    fl::size get_total_connections() const override;
    
    // Configuration
    void set_max_connections_per_host(fl::size max_connections) override;
    void set_max_total_connections(fl::size max_connections) override;
    void set_connection_timeout(fl::u32 timeout_ms) override;
    
private:
    Options mOptions;
    
    struct ConnectionEntry {
        fl::shared_ptr<Socket> socket;
        fl::string host;
        int port;
        fl::u32 last_used_time;
        bool in_use;
    };
    
    fl::vector<ConnectionEntry> mConnections;
    mutable fl::mutex mMutex;
    
    // Internal helpers
    fl::string make_connection_key(const fl::string& host, int port);
    void cleanup_expired_connections();
    bool is_connection_valid(const ConnectionEntry& entry);
    fl::shared_ptr<Socket> create_new_connection(const fl::string& host, int port);
};

/// Transport factory for creating transport instances
class TransportFactory {
public:
    /// Create transport based on URL scheme
    static fl::shared_ptr<Transport> create_for_scheme(const fl::string& scheme);
    
    /// Create transport with specific options
    static fl::shared_ptr<Transport> create_tcp_transport();
    static fl::shared_ptr<Transport> create_tls_transport();
    
    /// Register custom transport implementation
    using TransportCreator = fl::function<fl::shared_ptr<Transport>()>;
    static void register_transport(const fl::string& scheme, TransportCreator creator);
    
    /// Check if scheme is supported
    static bool is_scheme_supported(const fl::string& scheme);
    
    /// Get list of supported schemes
    static fl::vector<fl::string> get_supported_schemes();
    
private:
    static fl::hash_map<fl::string, TransportCreator>& get_transport_registry();
};

} // namespace fl 

#endif // FASTLED_HAS_NETWORKING 
