#pragma once

#ifdef FASTLED_HAS_NETWORKING

#include "fl/net/socket.h"  // For SocketError enum
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
// Normalized POSIX-Style Socket Types for WASM (Fake Implementation)
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
// Normalized POSIX-Style Socket API Functions (FAKE WASM Implementation)
//=============================================================================

// Core Socket Operations - fake implementations for WASM
int socket(int domain, int type, int protocol);
int socketpair(int domain, int type, int protocol, int sv[2]);

// Addressing - fake implementations for WASM
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

// Data Transfer - fake implementations for WASM
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen);
ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);
ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);

// Connection Teardown - fake implementations for WASM
int shutdown(int sockfd, int how);
int close(int fd);

// Socket Options - fake implementations for WASM
int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);

// Peer & Local Address Retrieval - fake implementations for WASM
int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

// Name and Service Translation - fake implementations for WASM
int getaddrinfo(const char *node, const char *service, 
                const struct addrinfo *hints, struct addrinfo **res);
void freeaddrinfo(struct addrinfo *res);
int getnameinfo(const struct sockaddr *sa, socklen_t salen,
                char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags);

// Address Conversion - fake implementations for WASM
int inet_pton(int af, const char *src, void *dst);
const char* inet_ntop(int af, const void *src, char *dst, socklen_t size);

// File Control - fake implementations for WASM
int fcntl(int fd, int cmd, ...);

// I/O Control - fake implementations for WASM
int ioctl(int fd, unsigned long request, ...);

// Error handling - fake implementation for WASM
int get_errno();

//=============================================================================
// WASM-specific Helper Functions
//=============================================================================

// Initialize WASM socket subsystem (if needed)
bool initialize_wasm_sockets();

// Cleanup WASM socket subsystem (if needed) 
void cleanup_wasm_sockets();

// Set mock behavior for testing
void set_wasm_socket_mock_behavior(bool should_fail, int error_code = ECONNREFUSED);

// Get statistics for debugging
struct WasmSocketStats {
    int total_sockets_created;
    int total_connections_attempted;
    int total_bytes_sent;
    int total_bytes_received;
    bool mock_mode_enabled;
    int mock_error_code;
};

WasmSocketStats get_wasm_socket_stats();
void reset_wasm_socket_stats();

} // namespace fl

#endif // FASTLED_HAS_NETWORKING 
