#ifdef FASTLED_HAS_NETWORKING

#include "fl/net/server_socket.h"

#include "platforms/socket_platform.h"

namespace fl {

ServerSocket::ServerSocket(const SocketOptions& options) : mOptions(options) {
    mSocket = platform_create_server_socket();
    if (mSocket == INVALID_SOCKET_HANDLE) {
        set_error(SocketError::UNKNOWN_ERROR, "Failed to create server socket");
        return;
    }
    
    setup_socket_options();
}

ServerSocket::~ServerSocket() {
    close();
}

SocketError ServerSocket::bind(const fl::string& address, int port) {
    if (mSocket == INVALID_SOCKET_HANDLE) {
        return SocketError::UNKNOWN_ERROR;
    }
    
    SocketError result = platform_bind_server_socket(mSocket, address, port);
    if (result == SocketError::SUCCESS) {
        mBoundAddress = address;
        mBoundPort = port;
    } else {
        set_error(result, "Failed to bind server socket");
    }
    
    return result;
}

SocketError ServerSocket::listen(int backlog) {
    if (mSocket == INVALID_SOCKET_HANDLE) {
        return SocketError::UNKNOWN_ERROR;
    }
    
    SocketError result = platform_listen_server_socket(mSocket, backlog);
    if (result == SocketError::SUCCESS) {
        mIsListening = true;
        mBacklog = backlog;
    } else {
        set_error(result, "Failed to listen on server socket");
    }
    
    return result;
}

fl::shared_ptr<Socket> ServerSocket::accept() {
    if (!mIsListening || mSocket == INVALID_SOCKET_HANDLE) {
        return nullptr;
    }
    
    socket_handle_t client_socket = platform_accept_connection(mSocket);
    if (client_socket == INVALID_SOCKET_HANDLE) {
        return nullptr;
    }
    
    // Create client socket from the accepted connection
    // This uses the existing Socket factory pattern
    auto client = SocketFactory::create_client_socket(mOptions);
    // TODO: Initialize client socket with the accepted socket handle
    // For now we need to implement a way to create Socket from existing handle
    
    mCurrentConnections++;
    return client;
}

fl::vector<fl::shared_ptr<Socket>> ServerSocket::accept_multiple(fl::size max_connections) {
    fl::vector<fl::shared_ptr<Socket>> accepted_connections;
    
    for (fl::size i = 0; i < max_connections && has_pending_connections(); ++i) {
        auto connection = accept();
        if (connection) {
            accepted_connections.push_back(connection);
        } else {
            break;
        }
    }
    
    return accepted_connections;
}

void ServerSocket::close() {
    if (mSocket != INVALID_SOCKET_HANDLE) {
        platform_close_server_socket(mSocket);
        mSocket = INVALID_SOCKET_HANDLE;
    }
    mIsListening = false;
    mCurrentConnections = 0;
}

bool ServerSocket::is_listening() const {
    return mIsListening && mSocket != INVALID_SOCKET_HANDLE;
}

bool ServerSocket::has_pending_connections() const {
    if (!mIsListening || mSocket == INVALID_SOCKET_HANDLE) {
        return false;
    }
    return platform_server_socket_has_pending_connections(mSocket);
}

void ServerSocket::set_reuse_address(bool enable) {
    if (mSocket != INVALID_SOCKET_HANDLE) {
        platform_set_server_socket_reuse_address(mSocket, enable);
    }
}

void ServerSocket::set_reuse_port(bool enable) {
    if (mSocket != INVALID_SOCKET_HANDLE) {
        platform_set_server_socket_reuse_port(mSocket, enable);
    }
}

void ServerSocket::set_non_blocking(bool non_blocking) {
    if (mSocket != INVALID_SOCKET_HANDLE) {
        platform_set_server_socket_non_blocking(mSocket, non_blocking);
        mIsNonBlocking = non_blocking;
    }
}

fl::string ServerSocket::bound_address() const {
    if (mSocket != INVALID_SOCKET_HANDLE) {
        return platform_get_server_socket_bound_address(mSocket);
    }
    return mBoundAddress;
}

int ServerSocket::bound_port() const {
    if (mSocket != INVALID_SOCKET_HANDLE) {
        return platform_get_server_socket_bound_port(mSocket);
    }
    return mBoundPort;
}

fl::size ServerSocket::max_connections() const {
    return static_cast<fl::size>(mBacklog);
}

fl::size ServerSocket::current_connections() const {
    return mCurrentConnections;
}

SocketError ServerSocket::get_last_error() const {
    return mLastError;
}

fl::string ServerSocket::get_error_message() const {
    return mErrorMessage;
}

int ServerSocket::get_socket_handle() const {
    return static_cast<int>(mSocket);
}

void ServerSocket::set_error(SocketError error, const fl::string& message) {
    mLastError = error;
    mErrorMessage = message;
}

bool ServerSocket::setup_socket_options() {
    if (mSocket == INVALID_SOCKET_HANDLE) {
        return false;
    }
    
    // Set default socket options based on SocketOptions
    if (mOptions.enable_reuse_addr) {
        set_reuse_address(true);
    }
    
    if (mOptions.enable_reuse_port) {
        set_reuse_port(true);
    }
    
    return true;
}

} // namespace fl

#endif // FASTLED_HAS_NETWORKING 
