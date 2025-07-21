#ifdef FASTLED_HAS_NETWORKING

#include "fl/net/server_socket.h"

// Include platform-specific normalized API ONLY
// These headers provide the fl:: namespace functions we need
#if defined(__EMSCRIPTEN__)
    #include "platforms/wasm/socket_wasm.h"
#elif defined(_WIN32)
    #include "platforms/win/socket_win.h"
#else
    #include "platforms/posix/socket_posix.h"
#endif

// NO DIRECT POSIX OR SOCKET HEADERS!
// All socket functions MUST go through the fl:: namespace API
// provided by the platform-specific implementations above
// NO EXCETIONS!

namespace fl {

//=============================================================================
// Helper functions for error translation
//=============================================================================

SocketError translate_errno_to_socket_error(int error_code) {
    switch (error_code) {
        case ECONNREFUSED: return SocketError::CONNECTION_REFUSED;
        case ETIMEDOUT: return SocketError::CONNECTION_TIMEOUT;
        case ENETUNREACH: return SocketError::NETWORK_UNREACHABLE;
        case EACCES: return SocketError::PERMISSION_DENIED;
        case EADDRINUSE: return SocketError::ADDRESS_IN_USE;
        case EINVAL: return SocketError::INVALID_ADDRESS;
        default: return SocketError::UNKNOWN_ERROR;
    }
}


//=============================================================================
// ServerSocket Implementation
//=============================================================================

ServerSocket::ServerSocket(const SocketOptions& options) : mOptions(options) {
    // Use normalized fl:: socket API - same code on all platforms
    mSocket = fl::socket(AF_INET, SOCK_STREAM, 0);
    if (mSocket == -1) {
        set_error(SocketError::UNKNOWN_ERROR, "Failed to create server socket");
        return;
    }
    
    setup_socket_options();
}

ServerSocket::~ServerSocket() {
    close();
}

SocketError ServerSocket::bind(const fl::string& address, int port) {
    if (mSocket == -1) {
        return SocketError::UNKNOWN_ERROR;
    }
    
    // Set up address structure
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    
    // Convert address string to binary form
    if (fl::inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1) {
        set_error(SocketError::INVALID_ADDRESS, "Invalid address format");
        return SocketError::INVALID_ADDRESS;
    }
    
    // Bind socket using normalized fl:: API
    if (fl::bind(mSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        int error = fl::get_errno();
        SocketError socket_error = translate_errno_to_socket_error(error);
        set_error(socket_error, "Failed to bind server socket");
        return socket_error;
    }
    
    mBoundAddress = address;
    mBoundPort = port;
    return SocketError::SUCCESS;
}

SocketError ServerSocket::listen(int backlog) {
    if (mSocket == -1) {
        return SocketError::UNKNOWN_ERROR;
    }
    
    // Listen using normalized fl:: API
    if (fl::listen(mSocket, backlog) == -1) {
        int error = fl::get_errno();
        SocketError socket_error = translate_errno_to_socket_error(error);
        set_error(socket_error, "Failed to listen on server socket");
        return socket_error;
    }
    
    mIsListening = true;
    mBacklog = backlog;
    return SocketError::SUCCESS;
}

fl::shared_ptr<Socket> ServerSocket::accept() {
    if (!mIsListening || mSocket == -1) {
        return nullptr;
    }
    
    // Accept connection using normalized fl:: API
    sockaddr_in client_addr = {};
    socklen_t addr_len = sizeof(client_addr);
    
    int client_socket = fl::accept(mSocket, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
    if (client_socket == -1) {
        return nullptr;
    }
    
    // Create client socket from the accepted connection
    // This uses the existing Socket factory pattern
    auto client = SocketFactory::create_client_socket(mOptions);
    // TODO: Initialize client socket with the accepted socket handle
    // For now we need to implement a way to create Socket from existing handle
    
    // Close the accepted socket for now since we can't use it yet
    fl::close(client_socket);
    
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
    if (mSocket != -1) {
        // Close using normalized fl:: API
        fl::close(mSocket);
        mSocket = -1;
    }
    mIsListening = false;
    mCurrentConnections = 0;
}

bool ServerSocket::is_listening() const {
    return mIsListening && mSocket != -1;
}

bool ServerSocket::has_pending_connections() const {
    if (!mIsListening || mSocket == -1) {
        return false;
    }
    
    // WASM COMPATIBLE: Use per-call non-blocking recv() with MSG_DONTWAIT
    // fcntl() is NOT proxied in WASM, so we must use per-call flags
    
    // Define MSG_DONTWAIT for platforms where it's missing (like Windows)
    #ifndef MSG_DONTWAIT
    #define MSG_DONTWAIT 0x40  // Standard Linux value
    #endif
    
    char dummy;
    ssize_t result = fl::recv(mSocket, &dummy, 0, MSG_PEEK | MSG_DONTWAIT);
    
    if (result >= 0) {
        // Pending connection available
        return true;
    } else {
        // Check errno - EAGAIN/EWOULDBLOCK means no pending connections
        int error = fl::get_errno();
        return (error != EWOULDBLOCK && error != EAGAIN);
    }
}

void ServerSocket::set_reuse_address(bool enable) {
    if (mSocket != -1) {
        int optval = enable ? 1 : 0;
        fl::setsockopt(mSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    }
}

void ServerSocket::set_reuse_port(bool enable) {
    if (mSocket != -1) {
        int optval = enable ? 1 : 0;
        // SO_REUSEPORT may not be supported on all platforms (like Windows)
        fl::setsockopt(mSocket, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    }
}

void ServerSocket::set_non_blocking(bool non_blocking) {
    if (mSocket != -1) {
        // WASM COMPATIBLE: Cannot use fcntl() to set socket modes in WASM
        // fcntl() is not proxied and doesn't work properly
        // Instead, rely on per-call non-blocking flags like MSG_DONTWAIT
        
        // Just track the intended state - actual non-blocking behavior
        // is achieved through per-call flags in WASM-compatible methods
        mIsNonBlocking = non_blocking;
        
        // Note: On WASM, socket blocking mode is handled through:
        // - MSG_DONTWAIT flag for recv/send operations  
        // - SO_RCVTIMEO/SO_SNDTIMEO timeouts via setsockopt()
    }
}

fl::string ServerSocket::bound_address() const {
    if (mSocket != -1) {
        // Get bound address using normalized fl:: API
        sockaddr_in addr = {};
        socklen_t addr_len = sizeof(addr);
        
        if (fl::getsockname(mSocket, reinterpret_cast<sockaddr*>(&addr), &addr_len) == 0) {
            char addr_str[INET_ADDRSTRLEN];
            if (fl::inet_ntop(AF_INET, &addr.sin_addr, addr_str, sizeof(addr_str))) {
                return fl::string(addr_str);
            }
        }
    }
    return mBoundAddress;
}

int ServerSocket::bound_port() const {
    if (mSocket != -1) {
        // Get bound port using normalized fl:: API
        sockaddr_in addr = {};
        socklen_t addr_len = sizeof(addr);
        
        if (fl::getsockname(mSocket, reinterpret_cast<sockaddr*>(&addr), &addr_len) == 0) {
            return ntohs(addr.sin_port);
        }
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
    return mSocket;
}

void ServerSocket::set_error(SocketError error, const fl::string& message) {
    mLastError = error;
    mErrorMessage = message;
}

bool ServerSocket::setup_socket_options() {
    if (mSocket == -1) {
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
