#pragma once

#ifdef FASTLED_HAS_NETWORKING

#include "fl/networking/socket.h"
#include "fl/string.h"
#include "fl/shared_ptr.h"

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
        h ^= static_cast<fl::size>(ip_version);
        h ^= static_cast<fl::size>(enable_keepalive);
        h ^= static_cast<fl::size>(enable_nodelay);
        h ^= static_cast<fl::size>(connect_timeout_ms);
        h ^= static_cast<fl::size>(read_timeout_ms);
        h ^= static_cast<fl::size>(write_timeout_ms);
        h ^= buffer_size;
        return h;
    }
};

/// Socket factory for creating platform-specific socket implementations
/// Each platform provides implementations of these functions directly
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
};

// Platform-specific implementations - each platform provides these
// These are declared here but implemented in platform-specific files
// Only the platform being compiled will provide these symbols

/// Platform-specific socket creation functions
/// Each platform implements these directly - no registration needed
fl::shared_ptr<Socket> create_platform_socket(const SocketOptions& options);
fl::shared_ptr<ServerSocket> create_platform_server_socket(const SocketOptions& options);

/// Platform capability queries
bool platform_supports_ipv6();
bool platform_supports_tls();
bool platform_supports_non_blocking_connect();
bool platform_supports_socket_reuse();

} // namespace fl 

#endif // FASTLED_HAS_NETWORKING 
