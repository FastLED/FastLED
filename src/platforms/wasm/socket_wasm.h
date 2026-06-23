#pragma once

// IWYU pragma: private

#ifdef FASTLED_HAS_NETWORKING

#include "fl/stl/string.h"
#include "fl/stl/stdint.h"

// Minimal socket includes for function declarations
// WASM provides some basic socket types but limited functionality
// IWYU pragma: begin_keep
#include <sys/types.h>    // For ssize_t, socklen_t if available
// IWYU pragma: end_keep
#include "platforms/wasm/is_wasm.h"
#include "platforms/win/is_win.h"

#ifdef FL_IS_WASM
    // IWYU pragma: begin_keep
    #include <sys/socket.h>   // Basic socket definitions if available
    #include <netinet/in.h>   // For sockaddr_in 
    #include <arpa/inet.h>    // For inet_pton/ntop
    #include <netdb.h>        // For getaddrinfo
    // IWYU pragma: end_keep
    
    // Define constants that may be missing in Emscripten
    #ifndef TCP_NODELAY
    #define TCP_NODELAY 1
    #endif
#elif defined(FL_IS_WIN)
    // When testing on Windows, use Windows headers - don't define our own types
    // IWYU pragma: begin_keep
    #include <winsock2.h>
    #include <ws2tcpip.h>
#include "fl/stl/noexcept.h"
    // IWYU pragma: end_keep
#else
    // Pure WASM environment - define our own socket types
    typedef int socklen_t;
    typedef long ssize_t;
    typedef unsigned short sa_family_t;
    typedef unsigned short in_port_t;
    
    struct sockaddr {
        sa_family_t sa_family;
        char sa_data[14];
    };
    
    struct sockaddr_in {
        sa_family_t sin_family;
        in_port_t sin_port;
        struct in_addr {
            unsigned long s_addr;
        } sin_addr;
        char sin_zero[8];
    };
    
    struct sockaddr_in6 {
        sa_family_t sin6_family;
        in_port_t sin6_port;
        unsigned long sin6_flowinfo;
        struct in6_addr {
            unsigned char s6_addr[16];
        } sin6_addr;
        unsigned long sin6_scope_id;
    };
    
    struct addrinfo {
        int ai_flags;
        int ai_family;
        int ai_socktype;
        int ai_protocol;
        socklen_t ai_addrlen;
        struct sockaddr *ai_addr;
        char *ai_canonname;
        struct addrinfo *ai_next;
    };
    
    struct msghdr {
        void *msg_name;
        socklen_t msg_namelen;
        struct iovec *msg_iov;
        size_t msg_iovlen;
        void *msg_control;
        size_t msg_controllen;
        int msg_flags;
    };
    
    struct iovec {
        void *iov_base;
        size_t iov_len;
    };
    
    // Socket constants for pure WASM
    #define AF_INET     2
    #define AF_INET6    10
    #define SOCK_STREAM 1
    #define SOCK_DGRAM  2
    #define IPPROTO_TCP 6
    #define IPPROTO_UDP 17
    #define SOL_SOCKET  1
    #define SO_REUSEADDR 2
    #define SO_REUSEPORT 15
    #define TCP_NODELAY 1
    #define SHUT_RD     0
    #define SHUT_WR     1
    #define SHUT_RDWR   2
    
    // Error codes for pure WASM
    #define EWOULDBLOCK     11
    #define ECONNREFUSED    111
    #define ETIMEDOUT       110
    #define ENETUNREACH     101
    #define EACCES          13
    #define EADDRINUSE      98
    #define EINVAL          22
    #define ENOTCONN        107
    #define ECONNRESET      104
    #define ECONNABORTED    103
    #define EAFNOSUPPORT    97
    
    // fcntl constants for pure WASM
    #define F_GETFL         3
    #define F_SETFL         4
    #define O_NONBLOCK      2048
#endif

namespace fl {

//=============================================================================
// Normalized POSIX-Style Socket Types for WASM (Real Implementation)
//=============================================================================

// Use standard socket types where available, provide fallbacks otherwise
using ::socklen_t;
using ::ssize_t;
using ::sa_family_t;
using ::in_port_t;

// Use standard socket address structures
using ::sockaddr;
using ::sockaddr_in;
using ::sockaddr_in6;

// WASM socket constants are compatible with POSIX values

//=============================================================================
// Normalized POSIX-Style Socket API Functions (Real WASM POSIX Implementation)
//=============================================================================

// Core Socket Operations - real POSIX implementations for WASM
int socket(int domain, int type, int protocol) FL_NOEXCEPT;
int socketpair(int domain, int type, int protocol, int sv[2]) FL_NOEXCEPT;

// Addressing - real POSIX implementations for WASM
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) FL_NOEXCEPT;
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) FL_NOEXCEPT;
int listen(int sockfd, int backlog) FL_NOEXCEPT;
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) FL_NOEXCEPT;

// Data Transfer - real POSIX implementations for WASM
ssize_t send(int sockfd, const void *buf, size_t len, int flags) FL_NOEXCEPT;
ssize_t recv(int sockfd, void *buf, size_t len, int flags) FL_NOEXCEPT;
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen) FL_NOEXCEPT;
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen) FL_NOEXCEPT;
ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags) FL_NOEXCEPT;
ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) FL_NOEXCEPT;

// Connection Teardown - real POSIX implementations for WASM
// NOTE: Use shutdown() instead of close() for sockets in WASM
int shutdown(int sockfd, int how) FL_NOEXCEPT;
int close(int fd) FL_NOEXCEPT;  // Uses shutdown() for sockets, real close() for other FDs

// Socket Options - real POSIX implementations for WASM
int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) FL_NOEXCEPT;
int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) FL_NOEXCEPT;

// Peer & Local Address Retrieval - real POSIX implementations for WASM
int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen) FL_NOEXCEPT;
int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen) FL_NOEXCEPT;

// Name and Service Translation - real POSIX implementations for WASM
int getaddrinfo(const char *node, const char *service, 
                const struct addrinfo *hints, struct addrinfo **res) FL_NOEXCEPT;
void freeaddrinfo(struct addrinfo *res) FL_NOEXCEPT;
int getnameinfo(const struct sockaddr *sa, socklen_t salen,
                char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags) FL_NOEXCEPT;

// Address Conversion - real POSIX implementations for WASM
int inet_pton(int af, const char *src, void *dst) FL_NOEXCEPT;
const char* inet_ntop(int af, const void *src, char *dst, socklen_t size) FL_NOEXCEPT;

// File Control - limited POSIX implementation for WASM (no select/poll support)
int fcntl(int fd, int cmd, ...) FL_NOEXCEPT;

// I/O Control - limited POSIX implementation for WASM  
int ioctl(int fd, unsigned long request, ...) FL_NOEXCEPT;

// Error handling - real implementation for WASM
int get_errno() FL_NOEXCEPT;

// WASM CONSTRAINTS: The following functions are blocking calls and are 
// DISALLOWED and NOT AVAILABLE on WASM due to proxying limitations:
// - select()  
// - poll()
// These functions are not declared in this API and MUST NOT be used.
// Use per-call non-blocking flags like MSG_DONTWAIT instead.

//=============================================================================
// WASM-specific Helper Functions
//=============================================================================

// Initialize WASM socket subsystem (if needed)
bool initialize_wasm_sockets() FL_NOEXCEPT;

// Cleanup WASM socket subsystem (if needed) 
void cleanup_wasm_sockets() FL_NOEXCEPT;

// Set mock behavior for testing (no-op in real implementation)
void set_wasm_socket_mock_behavior(bool should_fail, int error_code = ECONNREFUSED) FL_NOEXCEPT;

// Get statistics for debugging
struct WasmSocketStats {
    int total_sockets_created;
    int total_connections_attempted;
    int total_connections_successful;
    int total_bytes_sent;
    int total_bytes_received;
    bool mock_mode_enabled;
    int mock_error_code;
};

WasmSocketStats get_wasm_socket_stats() FL_NOEXCEPT;
void reset_wasm_socket_stats() FL_NOEXCEPT;

//=============================================================================
// WASM-specific Socket Class Implementation
//=============================================================================

/// WASM socket implementation using real POSIX sockets
class WasmSocket : public Socket {
public:
    explicit WasmSocket(const SocketOptions& options = {}) FL_NOEXCEPT;
    ~WasmSocket() override = default;
    
    // Socket interface implementation
    fl::future<SocketError> connect(const fl::string& host, int port) FL_NOEXCEPT override;
    fl::future<SocketError> connect_async(const fl::string& host, int port) FL_NOEXCEPT override;
    void disconnect() FL_NOEXCEPT override;
    bool is_connected() const FL_NOEXCEPT override;
    SocketState get_state() const FL_NOEXCEPT override;
    
    fl::size read(fl::span<u8> buffer) FL_NOEXCEPT override;
    fl::size write(fl::span<const u8> data) FL_NOEXCEPT override;
    fl::size available() const FL_NOEXCEPT override;
    void flush() FL_NOEXCEPT override;
    
    bool has_data_available() const FL_NOEXCEPT override;
    bool can_write() const FL_NOEXCEPT override;
    void set_non_blocking(bool non_blocking) FL_NOEXCEPT override;
    bool is_non_blocking() const FL_NOEXCEPT override;
    
    void set_timeout(u32 timeout_ms) FL_NOEXCEPT override;
    u32 get_timeout() const FL_NOEXCEPT override;
    void set_keep_alive(bool enable) FL_NOEXCEPT override;
    void set_nodelay(bool enable) FL_NOEXCEPT override;
    
    fl::string remote_address() const FL_NOEXCEPT override;
    int remote_port() const FL_NOEXCEPT override;
    fl::string local_address() const FL_NOEXCEPT override;
    int local_port() const FL_NOEXCEPT override;
    
    SocketError get_last_error() const FL_NOEXCEPT override;
    fl::string get_error_message() const FL_NOEXCEPT override;
    
    bool set_socket_option(int level, int option, const void* value, fl::size value_size) FL_NOEXCEPT override;
    bool get_socket_option(int level, int option, void* value, fl::size* value_size) FL_NOEXCEPT override;
    
    int get_socket_handle() const FL_NOEXCEPT override;
    
protected:
    void set_state(SocketState state) FL_NOEXCEPT override;
    void set_error(SocketError error, const fl::string& message = "") FL_NOEXCEPT override;
    
private:
    const SocketOptions mOptions;
    SocketState mState = SocketState::CLOSED;
    SocketError mLastError = SocketError::SUCCESS;
    fl::string mErrorMessage;
    fl::string mRemoteHost;
    int mRemotePort = 0;
    fl::string mLocalAddress;
    int mLocalPort = 0;
    int mSocketHandle = -1;
    bool mIsNonBlocking = false;
    u32 mTimeout = 5000;
    
    // Internal helper methods
    SocketError translate_errno_to_socket_error(int error_code) FL_NOEXCEPT;
    bool setup_socket_options() FL_NOEXCEPT;
};

// Platform-specific socket creation functions (required by socket_factory.cpp)
fl::shared_ptr<Socket> create_platform_socket(const SocketOptions& options) FL_NOEXCEPT;

// Platform capability queries  
bool platform_supports_ipv6() FL_NOEXCEPT;
bool platform_supports_tls() FL_NOEXCEPT;
bool platform_supports_non_blocking_connect() FL_NOEXCEPT;
bool platform_supports_socket_reuse() FL_NOEXCEPT;

} // namespace fl

#endif // FASTLED_HAS_NETWORKING 
