#pragma once

#include "fl/stdint.h"
#include "fl/string.h"
#include "fl/span.h"
#include "fl/shared_ptr.h"
#include "fl/vector.h"
#include "fl/function.h"
#include "fl/future.h"

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
    /// Static factory method for async connection
    /// @param host The hostname or IP address to connect to
    /// @param port The port number to connect to
    /// @param ip_version IP version preference (defaults to AUTO)
    /// @return Future that completes with connected Socket or error
    static fl::future<fl::shared_ptr<Socket>> create_connected(
        const fl::string& host, 
        int port, 
        IpVersion ip_version = IpVersion::AUTO
    );
    
    /// Static factory method for creating disconnected socket
    /// @param ip_version IP version preference (defaults to AUTO)
    /// @return Shared pointer to disconnected Socket instance
    static fl::shared_ptr<Socket> create(IpVersion ip_version = IpVersion::AUTO);
    
    /// Connection management
    virtual fl::future<SocketError> connect(const fl::string& host, int port) = 0;
    virtual fl::future<SocketError> connect_async(const fl::string& host, int port) = 0;
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
    /// Protected constructor - use static factory methods instead
    Socket() = default;
    
    /// Virtual destructor
    virtual ~Socket() = default;
    
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
