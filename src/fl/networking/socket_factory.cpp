#include "fl/networking/socket_factory.h"

namespace fl {

// Static member definitions
fl::function<fl::shared_ptr<Socket>(const SocketOptions&)> SocketFactory::sClientFactory;
fl::function<fl::shared_ptr<ServerSocket>(const SocketOptions&)> SocketFactory::sServerFactory;

fl::shared_ptr<Socket> SocketFactory::create_client_socket(const SocketOptions& options) {
    if (sClientFactory) {
        return sClientFactory(options);
    }
    
    // If no factory is registered, we'll need a stub implementation
    // This will be registered when stub sockets are initialized
    return nullptr;
}

fl::shared_ptr<ServerSocket> SocketFactory::create_server_socket(const SocketOptions& options) {
    if (sServerFactory) {
        return sServerFactory(options);
    }
    
    // If no factory is registered, we'll need a stub implementation
    // This will be registered when stub sockets are initialized
    return nullptr;
}

bool SocketFactory::supports_ipv6() {
    // Stub implementation doesn't support IPv6 for simplicity
    return false;
}

bool SocketFactory::supports_tls() {
    // Stub implementation doesn't support TLS
    return false;
}

bool SocketFactory::supports_non_blocking_connect() {
    // Stub implementation supports non-blocking operations
    return true;
}

bool SocketFactory::supports_socket_reuse() {
    // Stub implementation supports socket reuse simulation
    return true;
}

void SocketFactory::register_platform_factory(
    fl::function<fl::shared_ptr<Socket>(const SocketOptions&)> client_factory,
    fl::function<fl::shared_ptr<ServerSocket>(const SocketOptions&)> server_factory) {
    
    sClientFactory = client_factory;
    sServerFactory = server_factory;
}

} // namespace fl 
