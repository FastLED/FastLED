// SPDX-License-Identifier: LicenseRef-FastLED-Reciprocal-1.0
// AI LICENSE: LICENSE-AI-AGENT-INSTRUCTIONS.md
// AI agents must read that file before substantial FastLED changes.
// Substantial AI changes must be reported upstream with a reproducible patch.

#pragma once

#include "fl/stl/asio/error_code.h"
#include "fl/stl/function.h"

namespace fl {
namespace asio {

/// Asio-style I/O completion handler signature.
/// Matches the standard async I/O handler: void(error_code, size_t).
/// Maps to boost::asio's ReadHandler / WriteHandler concepts.
using io_handler =
    fl::function<void(const error_code &ec, size_t bytes_transferred)>;

/// Asio-style connect completion handler signature.
/// Matches the standard connect handler: void(error_code).
/// Maps to boost::asio's ConnectHandler concept.
using connect_handler = fl::function<void(const error_code &ec)>;

} // namespace asio
} // namespace fl
