#pragma once

#include "fl/task/promise.h"
#include "fl/stl/stdint.h"
#include "fl/stl/string.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace asio {

/// Asio-compatible error code enumeration.
/// Maps to boost::system::errc in Asio.
enum class errc : i32 {
    success = 0,
    connection_refused,
    connection_reset,
    timed_out,
    host_not_found,
    address_in_use,
    would_block,
    operation_aborted,
    eof, // clean disconnect / end of stream
    unknown = -1
};

/// Asio-compatible error code: numeric code + optional human-readable message.
/// Maps to boost::system::error_code in Asio.
///
/// Unlike fl::task::Error (string-only, heap allocation on every error), error_code
/// is cheap to copy and supports quick boolean checks: if (ec) { /* error */ }
struct error_code {
    errc code;
    fl::string message; // optional human-readable detail

    error_code() FL_NOEXCEPT : code(errc::success) {}
    error_code(errc c) : code(c) {}
    error_code(errc c, const fl::string &msg) : code(c), message(msg) {}
    error_code(errc c, const char *msg) : code(c), message(msg) {}

    /// Asio-style: true if error, false if success.
    explicit operator bool() const { return code != errc::success; }

    /// Convenience: true if no error.
    bool ok() const { return code == errc::success; }

    /// Convert from fl::task::Error for interop with existing FastLED code.
    static error_code from_error(const fl::task::Error &e) {
        if (e.is_empty()) {
            return error_code();
        }
        return error_code(errc::unknown, e.message);
    }

    /// Convert to fl::task::Error for interop with existing FastLED code.
    fl::task::Error to_error() const {
        if (ok()) {
            return fl::task::Error();
        }
        return fl::task::Error(message);
    }

    /// Convert from platform errno value.
    static error_code from_errno(int platform_errno);

    bool operator==(const error_code &o) const { return code == o.code; }
    bool operator!=(const error_code &o) const { return code != o.code; }
};

} // namespace asio
} // namespace fl
