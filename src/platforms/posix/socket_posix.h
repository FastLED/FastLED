#pragma once

#ifdef FASTLED_HAS_NETWORKING
#if !defined(_WIN32)

#include "fl/net/socket.h"  // For SocketError enum
#include "fl/string.h"
#include "fl/stdint.h"

// Standard POSIX socket includes
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <cstdarg>  // For va_list in fcntl and ioctl

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
inline int socket(int domain, int type, int protocol) {
    return ::socket(domain, type, protocol);
}

inline int socketpair(int domain, int type, int protocol, int sv[2]) {
    return ::socketpair(domain, type, protocol, sv);
}

// Addressing - direct passthrough to POSIX API
inline int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return ::bind(sockfd, addr, addrlen);
}

inline int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return ::connect(sockfd, addr, addrlen);
}

inline int listen(int sockfd, int backlog) {
    return ::listen(sockfd, backlog);
}

inline int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return ::accept(sockfd, addr, addrlen);
}

// Data Transfer - direct passthrough to POSIX API
inline ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    return ::send(sockfd, buf, len, flags);
}

inline ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    return ::recv(sockfd, buf, len, flags);
}

inline ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
                     const struct sockaddr *dest_addr, socklen_t addrlen) {
    return ::sendto(sockfd, buf, len, flags, dest_addr, addrlen);
}

inline ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                       struct sockaddr *src_addr, socklen_t *addrlen) {
    return ::recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
}

inline ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags) {
    return ::sendmsg(sockfd, msg, flags);
}

inline ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
    return ::recvmsg(sockfd, msg, flags);
}

// Connection Teardown - direct passthrough to POSIX API
inline int shutdown(int sockfd, int how) {
    return ::shutdown(sockfd, how);
}

inline int close(int fd) {
    return ::close(fd);
}

// Socket Options - direct passthrough to POSIX API
inline int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    return ::setsockopt(sockfd, level, optname, optval, optlen);
}

inline int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    return ::getsockopt(sockfd, level, optname, optval, optlen);
}

// Peer & Local Address Retrieval - direct passthrough to POSIX API
inline int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return ::getpeername(sockfd, addr, addrlen);
}

inline int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return ::getsockname(sockfd, addr, addrlen);
}

// Name and Service Translation - direct passthrough to POSIX API
inline int getaddrinfo(const char *node, const char *service, 
                      const struct addrinfo *hints, struct addrinfo **res) {
    return ::getaddrinfo(node, service, hints, res);
}

inline void freeaddrinfo(struct addrinfo *res) {
    ::freeaddrinfo(res);
}

inline int getnameinfo(const struct sockaddr *sa, socklen_t salen,
                      char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags) {
    return ::getnameinfo(sa, salen, host, hostlen, serv, servlen, flags);
}

// Address Conversion - direct passthrough to POSIX API
inline int inet_pton(int af, const char *src, void *dst) {
    return ::inet_pton(af, src, dst);
}

inline const char* inet_ntop(int af, const void *src, char *dst, socklen_t size) {
    return ::inet_ntop(af, src, dst, static_cast<size_t>(size));
}

// File Control - direct passthrough to POSIX API
inline int fcntl(int fd, int cmd, ...) {
    va_list args;
    va_start(args, cmd);
    int result = ::fcntl(fd, cmd, va_arg(args, int));
    va_end(args);
    return result;
}

// I/O Control - direct passthrough to POSIX API
inline int ioctl(int fd, unsigned long request, ...) {
    va_list args;
    va_start(args, request);
    int result = ::ioctl(fd, request, va_arg(args, void*));
    va_end(args);
    return result;
}

// Error handling - direct access to errno
inline int get_errno() {
    return errno;
}

// POSIX socket constants and errno values are already defined correctly
// No need to redefine AF_INET, SOCK_STREAM, ECONNREFUSED, etc.

} // namespace fl

#endif // !defined(_WIN32)
#endif // FASTLED_HAS_NETWORKING 
