#pragma once

#ifdef FASTLED_HAS_NETWORKING

#include "fl/net/socket.h"
#include "fl/net/socket_factory.h"
#include "fl/vector.h"
#include "fl/string.h"
#include "fl/mutex.h"
#include "fl/future.h"

namespace fl {

// Forward declarations and platform-neutral types
// Platform-specific details are handled in the implementation
using socket_handle_t = int;  // Platform-neutral socket handle type
static constexpr socket_handle_t INVALID_SOCKET_HANDLE = -1;

/// POSIX socket implementation using system socket APIs
class PosixSocket : public Socket {
public:
    explicit PosixSocket(const SocketOptions& options = {});
    ~PosixSocket() override;
    
    // Socket interface implementation
    fl::future<SocketError> connect(const fl::string& host, int port) override;
    fl::future<SocketError> connect_async(const fl::string& host, int port) override;
    void disconnect() override;
    bool is_connected() const override;
    SocketState get_state() const override;
    
    fl::size read(fl::span<fl::u8> buffer) override;
    fl::size write(fl::span<const fl::u8> data) override;
    fl::size available() const override;
    void flush() override;
    
    bool has_data_available() const override;
    bool can_write() const override;
    void set_non_blocking(bool non_blocking) override;
    bool is_non_blocking() const override;
    
    void set_timeout(fl::u32 timeout_ms) override;
    fl::u32 get_timeout() const override;
    void set_keep_alive(bool enable) override;
    void set_nodelay(bool enable) override;
    
    fl::string remote_address() const override;
    int remote_port() const override;
    fl::string local_address() const override;
    int local_port() const override;
    
    SocketError get_last_error() const override;
    fl::string get_error_message() const override;
    
    bool set_socket_option(int level, int option, const void* value, fl::size value_size) override;
    bool get_socket_option(int level, int option, void* value, fl::size* value_size) override;
    
    int get_socket_handle() const override;
    
protected:
    void set_state(SocketState state) override;
    void set_error(SocketError error, const fl::string& message = "") override;
    
private:
    const SocketOptions mOptions;
    socket_handle_t mSocket = INVALID_SOCKET_HANDLE;
    SocketState mState = SocketState::CLOSED;
    SocketError mLastError = SocketError::SUCCESS;
    fl::string mErrorMessage;
    fl::string mRemoteHost;
    int mRemotePort = 0;
    fl::string mLocalAddress;
    int mLocalPort = 0;
    bool mIsNonBlocking = false;
    fl::u32 mTimeout = 5000;
    
    // Platform-specific helpers
    static bool initialize_networking();
    static void cleanup_networking();
    static fl::string get_socket_error_string(int error_code);
    static SocketError translate_socket_error(int error_code);
    
    SocketError connect_internal(const fl::string& host, int port);
    bool setup_socket_options();
    fl::string resolve_hostname(const fl::string& hostname);
    
    // Static initialization tracking
    static bool s_networking_initialized;
    static int s_instance_count;
    static fl::mutex s_init_mutex;
};

/// POSIX server socket implementation
class PosixServerSocket : public ServerSocket {
public:
    explicit PosixServerSocket(const SocketOptions& options = {});
    ~PosixServerSocket() override;
    
    // ServerSocket interface implementation
    SocketError bind(const fl::string& address, int port) override;
    SocketError listen(int backlog = 5) override;
    void close() override;
    bool is_listening() const override;
    
    fl::shared_ptr<Socket> accept() override;
    fl::vector<fl::shared_ptr<Socket>> accept_multiple(fl::size max_connections = 10) override;
    bool has_pending_connections() const override;
    
    void set_reuse_address(bool enable) override;
    void set_reuse_port(bool enable) override;
    void set_non_blocking(bool non_blocking) override;
    
    fl::string bound_address() const override;
    int bound_port() const override;
    fl::size max_connections() const override;
    fl::size current_connections() const override;
    
    SocketError get_last_error() const override;
    fl::string get_error_message() const override;
    
    int get_socket_handle() const override;
    
protected:
    void set_error(SocketError error, const fl::string& message = "") override;
    
private:
    const SocketOptions mOptions;
    socket_handle_t mSocket = INVALID_SOCKET_HANDLE;
    bool mIsListening = false;
    fl::string mBoundAddress;
    int mBoundPort = 0;
    int mBacklog = 5;
    SocketError mLastError = SocketError::SUCCESS;
    fl::string mErrorMessage;
    bool mIsNonBlocking = false;
    fl::size mCurrentConnections = 0;
};

// Platform-specific socket creation functions (required by socket_factory.cpp)
fl::shared_ptr<Socket> create_platform_socket(const SocketOptions& options);
fl::shared_ptr<ServerSocket> create_platform_server_socket(const SocketOptions& options);

// Platform capability queries
bool platform_supports_ipv6();
bool platform_supports_tls();
bool platform_supports_non_blocking_connect();
bool platform_supports_socket_reuse();

} // namespace fl

#endif // FASTLED_HAS_NETWORKING 
