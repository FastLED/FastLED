#if defined(FASTLED_HAS_NETWORKING) && 0
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)

#include "socket_posix.h"

// Additional POSIX socket includes for implementation
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <cstdarg>  // For va_list in fcntl and ioctl

namespace fl {

//=============================================================================
// Normalized POSIX-Style Socket API Functions (Direct Passthrough)
//=============================================================================

// Core Socket Operations - direct passthrough to POSIX API
int socket(int domain, int type, int protocol) {
    return ::socket(domain, type, protocol);
}

int socketpair(int domain, int type, int protocol, int sv[2]) {
    return ::socketpair(domain, type, protocol, sv);
}

// Addressing - direct passthrough to POSIX API
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return ::bind(sockfd, addr, addrlen);
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return ::connect(sockfd, addr, addrlen);
}

int listen(int sockfd, int backlog) {
    return ::listen(sockfd, backlog);
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return ::accept(sockfd, addr, addrlen);
}

// Data Transfer - direct passthrough to POSIX API
ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    return ::send(sockfd, buf, len, flags);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    return ::recv(sockfd, buf, len, flags);
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen) {
    return ::sendto(sockfd, buf, len, flags, dest_addr, addrlen);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen) {
    return ::recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
}

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags) {
    return ::sendmsg(sockfd, msg, flags);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
    return ::recvmsg(sockfd, msg, flags);
}

// Connection Teardown - direct passthrough to POSIX API
int shutdown(int sockfd, int how) {
    return ::shutdown(sockfd, how);
}

int close(int fd) {
    return ::close(fd);
}

// Socket Options - direct passthrough to POSIX API
int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    return ::setsockopt(sockfd, level, optname, optval, optlen);
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    return ::getsockopt(sockfd, level, optname, optval, optlen);
}

// Peer & Local Address Retrieval - direct passthrough to POSIX API
int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return ::getpeername(sockfd, addr, addrlen);
}

int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return ::getsockname(sockfd, addr, addrlen);
}

// Name and Service Translation - direct passthrough to POSIX API
int getaddrinfo(const char *node, const char *service, 
                const struct addrinfo *hints, struct addrinfo **res) {
    return ::getaddrinfo(node, service, hints, res);
}

void freeaddrinfo(struct addrinfo *res) {
    ::freeaddrinfo(res);
}

int getnameinfo(const struct sockaddr *sa, socklen_t salen,
                char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags) {
    return ::getnameinfo(sa, salen, host, hostlen, serv, servlen, flags);
}

// Address Conversion - direct passthrough to POSIX API
int inet_pton(int af, const char *src, void *dst) {
    return ::inet_pton(af, src, dst);
}

const char* inet_ntop(int af, const void *src, char *dst, socklen_t size) {
    return ::inet_ntop(af, src, dst, static_cast<size_t>(size));
}

// File Control - direct passthrough to POSIX API
int fcntl(int fd, int cmd, ...) {
    va_list args;
    va_start(args, cmd);
    int result = ::fcntl(fd, cmd, va_arg(args, int));
    va_end(args);
    return result;
}

// I/O Control - direct passthrough to POSIX API
int ioctl(int fd, unsigned long request, ...) {
    va_list args;
    va_start(args, request);
    int result = ::ioctl(fd, request, va_arg(args, void*));
    va_end(args);
    return result;
}

// Error handling - direct access to errno
int get_errno() {
    return errno;
}

} // namespace fl

#endif // !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#endif // FASTLED_HAS_NETWORKING 
