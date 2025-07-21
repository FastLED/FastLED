#pragma once

#ifdef FASTLED_HAS_NETWORKING

#include "fl/net/socket.h"
#include "fl/net/socket_factory.h"
#include "fl/string.h"
#include "fl/shared_ptr.h"
#include "fl/vector.h"

namespace fl {

/// Unified non-polymorphic ServerSocket class
/// Uses normalized fl:: socket API for all operations - same code on all platforms
class ServerSocket {
public:
    explicit ServerSocket(const SocketOptions& options = {});
    ~ServerSocket();

    // No virtual methods - direct implementation using fl:: socket API
    SocketError bind(const fl::string& address, int port);
    SocketError listen(int backlog = 5);
    void close();
    bool is_listening() const;
    
    fl::shared_ptr<Socket> accept();
    fl::vector<fl::shared_ptr<Socket>> accept_multiple(fl::size max_connections = 10);
    bool has_pending_connections() const;
    
    void set_reuse_address(bool enable);
    void set_reuse_port(bool enable);
    void set_non_blocking(bool non_blocking);
    
    fl::string bound_address() const;
    int bound_port() const;
    fl::size max_connections() const;
    fl::size current_connections() const;
    
    SocketError get_last_error() const;
    fl::string get_error_message() const;
    int get_socket_handle() const;

private:
    // Platform-neutral member variables using standard POSIX file descriptors
    SocketOptions mOptions;
    int mSocket = -1;  // Standard POSIX file descriptor (-1 = invalid)
    bool mIsListening = false;
    fl::string mBoundAddress;
    int mBoundPort = 0;
    int mBacklog = 5;
    SocketError mLastError = SocketError::SUCCESS;
    fl::string mErrorMessage;
    bool mIsNonBlocking = false;
    fl::size mCurrentConnections = 0;
    
    // Internal helpers
    void set_error(SocketError error, const fl::string& message = "");
    bool setup_socket_options();
};

} // namespace fl

#endif // FASTLED_HAS_NETWORKING 
