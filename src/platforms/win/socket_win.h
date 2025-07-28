#pragma once

#ifdef FASTLED_HAS_NETWORKING && 0
#ifdef _WIN32

#include "fl/string.h"
#include "fl/stdint.h"

// Essential Windows header isolation
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOUSER
#define NOUSER
#endif

// Minimal Windows includes for type definitions
#include <winsock2.h>
#include <ws2tcpip.h>

namespace fl {

//=============================================================================
// Normalized POSIX-Style Socket Types
//=============================================================================

// POSIX-style type definitions that normalize Windows socket types
// Use existing Windows types when available to avoid conflicts
using socklen_t = int;
using ssize_t = int;

// Windows already defines sockaddr, sockaddr_in, sockaddr_in6, in_addr, in6_addr
// We use the existing Windows types directly via using declarations
using ::sockaddr;
using ::sockaddr_in;
using ::sockaddr_in6;
using ::in_addr;
using ::in6_addr;

// Type aliases for consistency with POSIX
using sa_family_t = unsigned short;  // Same as ADDRESS_FAMILY
using in_port_t = unsigned short;    // Same as USHORT

//=============================================================================
// POSIX Socket Constants (Windows Normalization)
//=============================================================================

// Address families
#ifndef AF_INET
#define AF_INET     2
#endif
#ifndef AF_INET6  
#define AF_INET6    23
#endif

// Socket types
#ifndef SOCK_STREAM
#define SOCK_STREAM 1
#endif
#ifndef SOCK_DGRAM
#define SOCK_DGRAM  2
#endif

// Protocols
#ifndef IPPROTO_TCP
#define IPPROTO_TCP 6
#endif
#ifndef IPPROTO_UDP
#define IPPROTO_UDP 17
#endif

// Socket options
#ifndef SOL_SOCKET
#define SOL_SOCKET  0xffff
#endif
#ifndef SO_REUSEADDR
#define SO_REUSEADDR 0x0004
#endif
#ifndef SO_REUSEPORT
#define SO_REUSEPORT 0x0200  // Not supported on Windows - will return error
#endif
#ifndef TCP_NODELAY
#define TCP_NODELAY 1
#endif

// fcntl flags for non-blocking
#ifndef F_GETFL
#define F_GETFL     3
#endif
#ifndef F_SETFL
#define F_SETFL     4
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK  0x4000
#endif

// Shutdown options
#ifndef SHUT_RD
#define SHUT_RD     0
#endif
#ifndef SHUT_WR
#define SHUT_WR     1
#endif
#ifndef SHUT_RDWR
#define SHUT_RDWR   2
#endif

//=============================================================================
// Helper Functions for Windows Socket Normalization
//=============================================================================

bool initialize_winsock();
void cleanup_winsock();
int translate_windows_error(int wsa_error);

//=============================================================================
// Normalized POSIX-Style Socket API Functions
//=============================================================================

// Core Socket Operations
int socket(int domain, int type, int protocol);
int socketpair(int domain, int type, int protocol, int sv[2]);

// Addressing
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

// Data Transfer
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen);

// Connection Teardown
int shutdown(int sockfd, int how);
int close(int fd);

// Socket Options
int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);

// Address Information
int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

// Address Resolution
int inet_pton(int af, const char *src, void *dst);
const char* inet_ntop(int af, const void *src, char *dst, socklen_t size);

// fcntl emulation for non-blocking sockets
int fcntl(int fd, int cmd, ...);

// Error handling
int get_errno();

// WASM CONSTRAINTS: The following functions are blocking calls and are 
// DISALLOWED and NOT AVAILABLE on WASM due to proxying limitations:
// - select()  
// - poll()
// These functions are not declared in this API and MUST NOT be used.
// Use per-call non-blocking flags like MSG_DONTWAIT instead.
// (Note: This constraint applies to WASM builds only - Windows builds are not affected)

// POSIX errno constants for Windows
#ifndef EWOULDBLOCK
#define EWOULDBLOCK     WSAEWOULDBLOCK
#endif
#ifndef ECONNREFUSED
#define ECONNREFUSED    WSAECONNREFUSED
#endif
#ifndef ETIMEDOUT
#define ETIMEDOUT       WSAETIMEDOUT
#endif
#ifndef ENETUNREACH
#define ENETUNREACH     WSAENETUNREACH
#endif
#ifndef EACCES
#define EACCES          WSAEACCES
#endif
#ifndef EADDRINUSE
#define EADDRINUSE      WSAEADDRINUSE
#endif
#ifndef EINVAL
#define EINVAL          WSAEINVAL
#endif
#ifndef ENOTCONN
#define ENOTCONN        WSAENOTCONN
#endif
#ifndef ECONNRESET
#define ECONNRESET      WSAECONNRESET
#endif
#ifndef ECONNABORTED
#define ECONNABORTED    WSAECONNABORTED
#endif

} // namespace fl

#endif // _WIN32
#endif // FASTLED_HAS_NETWORKING 
