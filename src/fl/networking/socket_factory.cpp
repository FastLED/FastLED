#include "fl/networking/socket_factory.h"

namespace fl {

fl::shared_ptr<Socket> SocketFactory::create_client_socket(const SocketOptions& options) {
    return create_platform_socket(options);
}

fl::shared_ptr<ServerSocket> SocketFactory::create_server_socket(const SocketOptions& options) {
    return create_platform_server_socket(options);
}

bool SocketFactory::supports_ipv6() {
    return platform_supports_ipv6();
}

bool SocketFactory::supports_tls() {
    return platform_supports_tls();
}

bool SocketFactory::supports_non_blocking_connect() {
    return platform_supports_non_blocking_connect();
}

bool SocketFactory::supports_socket_reuse() {
    return platform_supports_socket_reuse();
}

} // namespace fl 
