#pragma once

#include "fl/networking/socket.h"
#include "fl/hash.h"
#include "fl/function.h"

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
