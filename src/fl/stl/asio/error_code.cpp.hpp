#pragma once

#include "fl/stl/asio/error_code.h"

#ifdef FASTLED_HAS_NETWORKING

// Platform-specific socket includes for errno constants
#ifdef FL_IS_WIN
#include "platforms/win/socket_win.h" // ok platform headers  // IWYU pragma: keep
#else
#include "platforms/posix/socket_posix.h" // ok platform headers  // IWYU pragma: keep
#endif

namespace fl {
namespace asio {

error_code error_code::from_errno(int e) {
    switch (e) {
    case 0:
        return error_code();
#ifdef ECONNREFUSED
    case ECONNREFUSED:
        return error_code(errc::connection_refused, "connection refused");
#endif
#ifdef ECONNRESET
    case ECONNRESET:
        return error_code(errc::connection_reset, "connection reset");
#endif
#ifdef ETIMEDOUT
    case ETIMEDOUT:
        return error_code(errc::timed_out, "timed out");
#endif
#ifdef EADDRINUSE
    case EADDRINUSE:
        return error_code(errc::address_in_use, "address in use");
#endif
#ifdef EWOULDBLOCK
    case EWOULDBLOCK:
        return error_code(errc::would_block, "would block");
#endif
// EAGAIN may equal EWOULDBLOCK on some platforms
#if defined(EAGAIN) && (!defined(EWOULDBLOCK) || EAGAIN != EWOULDBLOCK)
    case EAGAIN:
        return error_code(errc::would_block, "would block");
#endif
    default:
        return error_code(errc::unknown, "unknown error");
    }
}

} // namespace asio
} // namespace fl

#else // !FASTLED_HAS_NETWORKING

// Stub for non-networking builds — only from_errno is needed
namespace fl {
namespace asio {

error_code error_code::from_errno(int) {
    return error_code(errc::unknown, "networking not available");
}

} // namespace asio
} // namespace fl

#endif // FASTLED_HAS_NETWORKING
