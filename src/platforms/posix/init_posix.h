// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/posix/init_posix.h
/// @brief POSIX platform initialization (no-op)
///
/// POSIX platforms (Linux, macOS, BSD) are primarily used for host-based testing.
/// No additional platform-specific initialization is required.

namespace fl {
namespace platforms {

/// @brief POSIX platform initialization (no-op)
///
/// POSIX platforms are host-based testing environments that don't require
/// platform-specific initialization. This function is a no-op and exists
/// for API consistency.
inline void init() {
    // No-op: POSIX platforms don't need special initialization
}

} // namespace platforms
} // namespace fl
