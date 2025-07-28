#pragma once

#ifdef FASTLED_HAS_NETWORKING

#include "fl/networking.h"  // For SocketError enum
#include "fl/string.h"
#include "fl/stdint.h"

// Minimal socket includes for function declarations
// WASM provides some basic socket types but limited functionality
#include <sys/types.h>    // For ssize_t, socklen_t if available
#ifdef __EMSCRIPTEN__
    #include <sys/socket.h>   // Basic socket definitions if available
    #include <netinet/in.h>   // For sockaddr_in 
    #include <arpa/inet.h>    // For inet_pton/ntop
    #include <netdb.h>        // For getaddrinfo
    
    // Define constants that may be missing in Emscripten
    #ifndef TCP_NODELAY
    #define TCP_NODELAY 1
    #endif
#elif defined(_WIN32)
    // When testing on Windows, use Windows headers - don't define our own types
    #include <winsock2.h>
    #include <ws2tcpip.h>
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
int socket(int domain, int type, int protocol);
int socketpair(int domain, int type, int protocol, int sv[2]);

// Addressing - real POSIX implementations for WASM
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

// Data Transfer - real POSIX implementations for WASM
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen);
ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);
ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);

// Connection Teardown - real POSIX implementations for WASM
// NOTE: Use shutdown() instead of close() for sockets in WASM
int shutdown(int sockfd, int how);
int close(int fd);  // Uses shutdown() for sockets, real close() for other FDs

// Socket Options - real POSIX implementations for WASM
int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);

// Peer & Local Address Retrieval - real POSIX implementations for WASM
int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

// Name and Service Translation - real POSIX implementations for WASM
int getaddrinfo(const char *node, const char *service, 
                const struct addrinfo *hints, struct addrinfo **res);
void freeaddrinfo(struct addrinfo *res);
int getnameinfo(const struct sockaddr *sa, socklen_t salen,
                char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags);

// Address Conversion - real POSIX implementations for WASM
int inet_pton(int af, const char *src, void *dst);
const char* inet_ntop(int af, const void *src, char *dst, socklen_t size);

// File Control - limited POSIX implementation for WASM (no select/poll support)
int fcntl(int fd, int cmd, ...);

// I/O Control - limited POSIX implementation for WASM  
int ioctl(int fd, unsigned long request, ...);

// Error handling - real implementation for WASM
int get_errno();

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
bool initialize_wasm_sockets();

// Cleanup WASM socket subsystem (if needed) 
void cleanup_wasm_sockets();

// Set mock behavior for testing (no-op in real implementation)
void set_wasm_socket_mock_behavior(bool should_fail, int error_code = ECONNREFUSED);

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

WasmSocketStats get_wasm_socket_stats();
void reset_wasm_socket_stats();

//=============================================================================
// WASM-specific Socket Class Implementation
//=============================================================================

/// WASM socket implementation using real POSIX sockets
class WasmSocket : public Socket {
public:
    explicit WasmSocket(const SocketOptions& options = {});
    ~WasmSocket() override = default;
    
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
    SocketState mState = SocketState::CLOSED;
    SocketError mLastError = SocketError::SUCCESS;
    fl::string mErrorMessage;
    fl::string mRemoteHost;
    int mRemotePort = 0;
    fl::string mLocalAddress;
    int mLocalPort = 0;
    int mSocketHandle = -1;
    bool mIsNonBlocking = false;
    fl::u32 mTimeout = 5000;
    
    // Internal helper methods
    SocketError translate_errno_to_socket_error(int error_code);
    bool setup_socket_options();
};

// Platform-specific socket creation functions (required by socket_factory.cpp)
fl::shared_ptr<Socket> create_platform_socket(const SocketOptions& options);

// Platform capability queries  
bool platform_supports_ipv6();
bool platform_supports_tls();
bool platform_supports_non_blocking_connect();
bool platform_supports_socket_reuse();

} // namespace fl

#endif // FASTLED_HAS_NETWORKING 
