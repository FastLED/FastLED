#pragma once

// TCP socket, acceptor, and endpoint types — Asio-compatible shapes.
// Requires native socket APIs (Windows or POSIX).
// On embedded platforms (STM32, AVR, etc.) this file compiles to nothing.

#include "fl/stl/asio/completion_handler.h"
#include "fl/stl/asio/error_code.h"
#include "fl/stl/span.h"
#include "fl/stl/stdint.h"
#include "fl/stl/string.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace asio {
namespace ip {
namespace tcp {

/// Asio-compatible endpoint type: bundles host address + port.
/// Maps to boost::asio::ip::tcp::endpoint in Asio.
struct endpoint {
    fl::string host; // hostname or IP address
    u16 port;

    endpoint() FL_NO_EXCEPT : port(0) {}
    endpoint(const fl::string &h, u16 p) FL_NO_EXCEPT : host(h), port(p) {}
    endpoint(const char *h, u16 p) FL_NO_EXCEPT : host(h), port(p) {}

    bool operator==(const endpoint &o) const FL_NO_EXCEPT {
        return port == o.port && host == o.host;
    }
    bool operator!=(const endpoint &o) const FL_NO_EXCEPT { return !(*this == o); }
};

#ifdef FASTLED_HAS_NETWORKING

/// RAII TCP socket — Asio-compatible shape.
/// Maps to boost::asio::ip::tcp::socket.
///
/// Owns a file descriptor; closes on destruction.
/// Movable, not copyable.
class socket {
  public:
    socket() FL_NO_EXCEPT;
    ~socket() FL_NO_EXCEPT;

    // Movable
    socket(socket &&other) FL_NO_EXCEPT;
    socket &operator=(socket &&other) FL_NO_EXCEPT;

    // Not copyable
    socket(const socket &) FL_NO_EXCEPT = delete;
    socket &operator=(const socket &) FL_NO_EXCEPT = delete;

    /// True if the socket has a valid file descriptor.
    bool is_open() const FL_NO_EXCEPT;

    /// Synchronous connect to endpoint. Sets socket non-blocking first.
    error_code connect(const endpoint &ep) FL_NO_EXCEPT;

    /// Close the socket.
    void close() FL_NO_EXCEPT;

    /// Graceful shutdown (both directions).
    void shutdown() FL_NO_EXCEPT;

    /// Synchronous read — returns bytes transferred, sets ec on error.
    /// Returns 0 with errc::would_block if no data available.
    size_t read_some(fl::span<u8> buffer, error_code &ec) FL_NO_EXCEPT;

    /// Synchronous write — returns bytes transferred, sets ec on error.
    size_t write_some(fl::span<const u8> buffer, error_code &ec) FL_NO_EXCEPT;

    /// Async read with Asio-style completion handler.
    void async_read_some(fl::span<u8> buffer, io_handler handler) FL_NO_EXCEPT;

    /// Async write with Asio-style completion handler.
    void async_write_some(fl::span<const u8> buffer, io_handler handler) FL_NO_EXCEPT;

    /// Async connect with Asio-style completion handler.
    void async_connect(const endpoint &ep, connect_handler handler) FL_NO_EXCEPT;

    /// Set non-blocking mode.
    void set_non_blocking(bool mode) FL_NO_EXCEPT;

    /// Query non-blocking mode.
    bool is_non_blocking() const FL_NO_EXCEPT;

    /// Access underlying file descriptor (escape hatch).
    int native_handle() const FL_NO_EXCEPT;

    /// Adopt an existing file descriptor (takes ownership).
    void assign(int fd) FL_NO_EXCEPT;

  private:
    int mFd;
    bool mNonBlocking;

    void close_fd() FL_NO_EXCEPT;
};

/// RAII TCP acceptor — Asio-compatible shape.
/// Maps to boost::asio::ip::tcp::acceptor.
///
/// Binds, listens, and accepts incoming connections.
class acceptor {
  public:
    acceptor() FL_NO_EXCEPT;
    ~acceptor() FL_NO_EXCEPT;

    // Not copyable or movable (owns listening socket)
    acceptor(const acceptor &) FL_NO_EXCEPT = delete;
    acceptor &operator=(const acceptor &) FL_NO_EXCEPT = delete;

    /// Bind and prepare to listen on port.
    error_code open(u16 port) FL_NO_EXCEPT;

    /// Start listening for connections.
    error_code listen(int backlog = 5) FL_NO_EXCEPT;

    /// Accept a connection into peer socket (non-blocking).
    /// Returns errc::would_block if no connection pending.
    error_code accept(socket &peer) FL_NO_EXCEPT;

    /// Async accept with Asio-style completion handler.
    void async_accept(socket &peer, connect_handler handler) FL_NO_EXCEPT;

    /// Close the acceptor.
    void close() FL_NO_EXCEPT;

    /// True if acceptor is open.
    bool is_open() const FL_NO_EXCEPT;

    /// Access underlying file descriptor.
    int native_handle() const FL_NO_EXCEPT;

    /// Get the port the acceptor is bound to.
    u16 port() const FL_NO_EXCEPT;

  private:
    int mFd;
    u16 mPort;
};

#endif // FASTLED_HAS_NETWORKING

} // namespace tcp
} // namespace ip
} // namespace asio
} // namespace fl
