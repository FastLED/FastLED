#pragma once

/// @file port.h
/// @brief Range-checked conversion of an untrusted integer to a TCP/UDP port.
///
/// JSON-RPC and config surfaces hand port numbers around as a wide signed
/// integer, but the socket APIs take a `u16`. Narrowing with a bare
/// `static_cast<u16>` silently wraps: `70000` becomes `4464`, so a request
/// that should have been rejected instead targets a completely different
/// endpoint and the failure surfaces much later as a confusing connection
/// error (FastLED#3956).
///
/// A low-side-only guard (`port > 0 ? static_cast<u16>(port) : 0`) does not
/// help — it rejects `65536` only by the accident of it truncating to zero,
/// while `65537` still slips through as port 1.

#include "fl/stl/int.h"
#include "fl/stl/optional.h"

namespace fl {
namespace net {

/// Lowest port a caller may request. Port 0 means "any port" to the socket
/// layer, which is never what an explicit endpoint request intends.
constexpr i64 kMinPort = 1;
/// Highest port representable in the 16-bit wire field.
constexpr i64 kMaxPort = 65535;

/// Convert an untrusted integer to a port number.
///
/// @return the port when @p value lies in [1, 65535], otherwise `nullopt`.
///         Callers must surface the empty case as an error rather than
///         substituting a default, since a wrong port is indistinguishable
///         from a working one until the connection fails.
inline fl::optional<u16> tryParsePort(i64 value) {
    if (value < kMinPort || value > kMaxPort) {
        return fl::nullopt;
    }
    return fl::optional<u16>(static_cast<u16>(value));
}

} // namespace net
} // namespace fl
