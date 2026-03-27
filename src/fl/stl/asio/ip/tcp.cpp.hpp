#pragma once

// TCP socket and acceptor implementation.
// Requires native socket APIs (Windows or POSIX).
#ifdef FASTLED_HAS_NETWORKING

// Platform-specific socket includes (provides normalized POSIX API)
#ifdef FL_IS_WIN
#include "platforms/win/socket_win.h" // ok platform headers  // IWYU pragma: keep
#else
#include "platforms/posix/socket_posix.h" // ok platform headers  // IWYU pragma: keep
#endif

#include "fl/stl/asio/ip/tcp.h"
#include "fl/stl/noexcept.h"

// Ensure MSG_NOSIGNAL is available (not defined on Windows/macOS)
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

namespace fl {
namespace asio {
namespace ip {
namespace tcp {

namespace {

// Platform socket wrappers — avoids name conflicts.
// On Windows: delegates to fl:: wrappers from socket_win.h
// On POSIX: delegates to :: system calls from socket_posix.h

int plat_socket(int domain, int type, int protocol) {
#ifdef FL_IS_WIN
    return fl::socket(domain, type, protocol);
#else
    return ::socket(domain, type, protocol);
#endif
}

int plat_connect(int fd, const struct sockaddr *addr, socklen_t addrlen) {
#ifdef FL_IS_WIN
    return fl::connect(fd, addr, addrlen);
#else
    return ::connect(fd, addr, addrlen);
#endif
}

ssize_t plat_send(int fd, const void *buf, size_t len, int flags) {
#ifdef FL_IS_WIN
    return fl::send(fd, buf, len, flags);
#else
    return ::send(fd, buf, len, flags);
#endif
}

ssize_t plat_recv(int fd, void *buf, size_t len, int flags) {
#ifdef FL_IS_WIN
    return fl::recv(fd, buf, len, flags);
#else
    return ::recv(fd, buf, len, flags);
#endif
}

int plat_close(int fd) {
#ifdef FL_IS_WIN
    return fl::close(fd);
#else
    return ::close(fd);
#endif
}

int plat_shutdown(int fd, int how) {
#ifdef FL_IS_WIN
    return fl::shutdown(fd, how);
#else
    return ::shutdown(fd, how);
#endif
}

int plat_setsockopt(int fd, int level, int optname, const void *optval,
                    socklen_t optlen) {
#ifdef FL_IS_WIN
    return fl::setsockopt(fd, level, optname, optval, optlen);
#else
    return ::setsockopt(fd, level, optname, optval, optlen);
#endif
}

int plat_bind(int fd, const struct sockaddr *addr, socklen_t addrlen) {
#ifdef FL_IS_WIN
    return fl::bind(fd, addr, addrlen);
#else
    return ::bind(fd, addr, addrlen);
#endif
}

int plat_listen(int fd, int backlog) {
#ifdef FL_IS_WIN
    return fl::listen(fd, backlog);
#else
    return ::listen(fd, backlog);
#endif
}

int plat_accept(int fd, struct sockaddr *addr, socklen_t *addrlen) {
#ifdef FL_IS_WIN
    return fl::accept(fd, addr, addrlen);
#else
    return ::accept(fd, addr, addrlen);
#endif
}

bool set_nonblocking(int fd, bool enabled) {
#ifdef FL_IS_WIN
    u_long mode = enabled ? 1 : 0;
    return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return false;
    if (enabled) {
        return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
    } else {
        return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) != -1;
    }
#endif
}

bool is_would_block() {
#ifdef FL_IS_WIN
    int err = WSAGetLastError();
    return err == WSAEWOULDBLOCK;
#else
    return errno == EWOULDBLOCK || errno == EAGAIN;
#endif
}


int plat_getsockname(int fd, struct sockaddr *addr, socklen_t *addrlen) {
#ifdef FL_IS_WIN
    return fl::getsockname(fd, addr, addrlen);
#else
    return ::getsockname(fd, addr, addrlen);
#endif
}

} // anonymous namespace

// ============================================================================
// socket implementation
// ============================================================================

socket::socket() : mFd(-1), mNonBlocking(false) {}

socket::~socket() FL_NOEXCEPT { close_fd(); }

socket::socket(socket &&other) : mFd(other.mFd), mNonBlocking(other.mNonBlocking) {
    other.mFd = -1;
    other.mNonBlocking = false;
}

socket &socket::operator=(socket &&other) FL_NOEXCEPT {
    if (this != &other) {
        close_fd();
        mFd = other.mFd;
        mNonBlocking = other.mNonBlocking;
        other.mFd = -1;
        other.mNonBlocking = false;
    }
    return *this;
}

bool socket::is_open() const { return mFd != -1; }

error_code socket::connect(const endpoint &ep) {
    // Clean up existing
    close_fd();

#ifdef FL_IS_WIN
    if (!initialize_winsock()) {
        return error_code(errc::unknown, "winsock init failed");
    }
#endif

    // Resolve hostname
    struct addrinfo hints {};
    struct addrinfo *result = nullptr;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    char portStr[16];
    snprintf(portStr, sizeof(portStr), "%u", ep.port);

    int ret = getaddrinfo(ep.host.c_str(), portStr, &hints, &result);
    if (ret != 0 || result == nullptr) {
        return error_code(errc::host_not_found, "getaddrinfo failed");
    }

    error_code ec(errc::connection_refused, "no addresses succeeded");

    for (struct addrinfo *addr = result; addr != nullptr; addr = addr->ai_next) {
        int sock =
            plat_socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (sock < 0)
            continue;

        mFd = sock;
        // Keep socket BLOCKING for connect() — non-blocking connect
        // returns EINPROGRESS/WSAEWOULDBLOCK which is racy: subsequent
        // send() can fail with ENOTCONN if the TCP handshake hasn't
        // completed yet.  A blocking connect on loopback completes in
        // microseconds; on real networks, the timeout is the OS default
        // (typically 75-120 s), which is acceptable for a "synchronous
        // connect" API.

        ret = plat_connect(mFd, addr->ai_addr,
                           static_cast<socklen_t>(addr->ai_addrlen));

        if (ret == 0) {
            // Connected — now switch to non-blocking for all I/O
            set_nonblocking(mFd, true);
            mNonBlocking = true;
            ec = error_code();
            break;
        }

        // Failed — try next address
        plat_close(mFd);
        mFd = -1;
    }

    freeaddrinfo(result);
    return ec;
}

void socket::close() { close_fd(); }

void socket::shutdown() {
    if (mFd != -1) {
        plat_shutdown(mFd, SHUT_RDWR);
    }
}

size_t socket::read_some(fl::span<u8> buffer, error_code &ec) {
    ec = error_code();
    if (mFd == -1) {
        ec = error_code(errc::operation_aborted, "socket not open");
        return 0;
    }

    ssize_t result =
        plat_recv(mFd, (char *)buffer.data(), buffer.size(), 0);

    if (result < 0) {
        if (is_would_block()) {
            ec = error_code(errc::would_block);
            return 0;
        }
#ifdef FL_IS_WIN
        ec = error_code(errc::unknown, "recv failed");
#else
        ec = error_code::from_errno(errno);
#endif
        return 0;
    }

    if (result == 0) {
        ec = error_code(errc::eof, "connection closed by peer");
        return 0;
    }

    return static_cast<size_t>(result);
}

size_t socket::write_some(fl::span<const u8> buffer, error_code &ec) {
    ec = error_code();
    if (mFd == -1) {
        ec = error_code(errc::operation_aborted, "socket not open");
        return 0;
    }

    ssize_t result = plat_send(mFd, (const char *)buffer.data(),
                               buffer.size(), MSG_NOSIGNAL);

    if (result < 0) {
        if (is_would_block()) {
            ec = error_code(errc::would_block);
            return 0;
        }
#ifdef FL_IS_WIN
        ec = error_code(errc::unknown, "send failed");
#else
        ec = error_code::from_errno(errno);
#endif
        return 0;
    }

    return static_cast<size_t>(result);
}

void socket::async_read_some(fl::span<u8> buffer, io_handler handler) {
    error_code ec;
    size_t n = read_some(buffer, ec);
    if (handler) {
        handler(ec, n);
    }
}

void socket::async_write_some(fl::span<const u8> buffer, io_handler handler) {
    error_code ec;
    size_t n = write_some(buffer, ec);
    if (handler) {
        handler(ec, n);
    }
}

void socket::async_connect(const endpoint &ep, connect_handler handler) {
    error_code ec = connect(ep);
    if (handler) {
        handler(ec);
    }
}

void socket::set_non_blocking(bool mode) {
    if (mFd != -1) {
        set_nonblocking(mFd, mode);
    }
    mNonBlocking = mode;
}

bool socket::is_non_blocking() const { return mNonBlocking; }

int socket::native_handle() const { return mFd; }

void socket::assign(int fd) {
    close_fd();
    mFd = fd;
}

void socket::close_fd() {
    if (mFd != -1) {
        plat_close(mFd);
        mFd = -1;
    }
    mNonBlocking = false;
}

// ============================================================================
// acceptor implementation
// ============================================================================

acceptor::acceptor() : mFd(-1), mPort(0) {}

acceptor::~acceptor() FL_NOEXCEPT { close(); }

error_code acceptor::open(u16 port) {
    close();

#ifdef FL_IS_WIN
    if (!initialize_winsock()) {
        return error_code(errc::unknown, "winsock init failed");
    }
#endif

    int sock = plat_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        return error_code(errc::unknown, "socket creation failed");
    }

    // SO_REUSEADDR for quick restarts
    int reuse = 1;
    plat_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse,
                    sizeof(reuse));

    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    // Bind to loopback for consistent behavior across platforms
    // especially for port 0 (ephemeral port assignment)
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    int ret =
        plat_bind(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret != 0) {
        plat_close(sock);
        return error_code(errc::address_in_use, "bind failed");
    }

    // Query actual port if 0 was requested
    if (port == 0) {
        struct sockaddr_in boundAddr {};
        socklen_t addrLen = sizeof(boundAddr);
        if (plat_getsockname(sock, (struct sockaddr *)&boundAddr,
                             &addrLen) != 0) {
            plat_close(sock);
            return error_code(errc::unknown, "getsockname failed - cannot resolve ephemeral port");
        }
        port = ntohs(boundAddr.sin_port);
    }

    mFd = sock;
    mPort = port;

    // Non-blocking — blocking is never appropriate on embedded
    set_nonblocking(mFd, true);

    return error_code();
}

error_code acceptor::listen(int backlog) {
    if (mFd == -1) {
        return error_code(errc::operation_aborted, "acceptor not open");
    }

    int ret = plat_listen(mFd, backlog);
    if (ret != 0) {
        return error_code(errc::unknown, "listen failed");
    }

    return error_code();
}

error_code acceptor::accept(socket &peer) {
    if (mFd == -1) {
        return error_code(errc::operation_aborted, "acceptor not open");
    }

    struct sockaddr_in clientAddr {};
    socklen_t addrLen = sizeof(clientAddr);
    int clientFd =
        plat_accept(mFd, (struct sockaddr *)&clientAddr, &addrLen);

    if (clientFd < 0) {
        if (is_would_block()) {
            return error_code(errc::would_block);
        }
#ifdef FL_IS_WIN
        return error_code(errc::unknown, "accept failed");
#else
        return error_code::from_errno(errno);
#endif
    }

    // Non-blocking on accepted socket
    set_nonblocking(clientFd, true);
    peer.assign(clientFd);

    return error_code();
}

void acceptor::async_accept(socket &peer, connect_handler handler) {
    error_code ec = accept(peer);
    if (handler) {
        handler(ec);
    }
}

void acceptor::close() {
    if (mFd != -1) {
        plat_close(mFd);
        mFd = -1;
    }
    mPort = 0;
}

bool acceptor::is_open() const { return mFd != -1; }

int acceptor::native_handle() const { return mFd; }

u16 acceptor::port() const { return mPort; }

} // namespace tcp
} // namespace ip
} // namespace asio
} // namespace fl

#endif // FASTLED_HAS_NETWORKING
