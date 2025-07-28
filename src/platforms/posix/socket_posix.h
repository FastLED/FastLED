#pragma once

#ifdef FASTLED_HAS_NETWORKING
#if !defined(_WIN32)

#include "fl/networking.h"  // For SocketError enum
#include "fl/string.h"
#include "fl/stdint.h"

// Minimal POSIX includes for function declarations
#include <sys/types.h>    // For ssize_t, socklen_t
#include <sys/socket.h>   // For struct sockaddr, struct msghdr  
#include <netdb.h>        // For struct addrinfo

namespace fl {

//=============================================================================
// Normalized POSIX-Style Socket Types (Direct POSIX Types)
//=============================================================================

// Use standard POSIX types directly - no normalization needed
using ::socklen_t;
using ::ssize_t;
using ::sa_family_t;
using ::in_port_t;

// Use standard POSIX socket address structures directly
using ::sockaddr;
using ::sockaddr_in;
using ::sockaddr_in6;
using ::in_addr;
using ::in6_addr;

// POSIX socket constants are already correct - no redefinition needed
// AF_INET, AF_INET6, SOCK_STREAM, SOCK_DGRAM, etc. are already defined correctly

//=============================================================================
// Normalized POSIX-Style Socket API Functions (Direct Passthrough)
//=============================================================================

// Core Socket Operations - direct passthrough to POSIX API
int socket(int domain, int type, int protocol);
int socketpair(int domain, int type, int protocol, int sv[2]);

// Addressing - direct passthrough to POSIX API
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

// Data Transfer - direct passthrough to POSIX API
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen);
ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);
ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);

// Connection Teardown - direct passthrough to POSIX API
int shutdown(int sockfd, int how);
int close(int fd);  // Keep close() - it's emulated on WASM

// Socket Options - direct passthrough to POSIX API
int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);

// Peer & Local Address Retrieval - direct passthrough to POSIX API
int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

// Name and Service Translation - direct passthrough to POSIX API
int getaddrinfo(const char *node, const char *service, 
                const struct addrinfo *hints, struct addrinfo **res);
void freeaddrinfo(struct addrinfo *res);
int getnameinfo(const struct sockaddr *sa, socklen_t salen,
                char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags);

// Address Conversion - direct passthrough to POSIX API
int inet_pton(int af, const char *src, void *dst);
const char* inet_ntop(int af, const void *src, char *dst, socklen_t size);

// File Control - direct passthrough to POSIX API
int fcntl(int fd, int cmd, ...);

// I/O Control - direct passthrough to POSIX API
int ioctl(int fd, unsigned long request, ...);

// Error handling - direct access to errno
int get_errno();

// WASM CONSTRAINTS: The following functions are blocking calls and are 
// DISALLOWED and NOT AVAILABLE on WASM due to proxying limitations:
// - select()  
// - poll()
// These functions are not declared in this API and MUST NOT be used.
// Use per-call non-blocking flags like MSG_DONTWAIT instead.

// POSIX socket constants and errno values are already defined correctly
// No need to redefine AF_INET, SOCK_STREAM, ECONNREFUSED, etc.

} // namespace fl

#endif // !defined(_WIN32)
#endif // FASTLED_HAS_NETWORKING 
