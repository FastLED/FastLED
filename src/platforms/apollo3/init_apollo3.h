// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/apollo3/init_apollo3.h
/// @brief Apollo3 platform initialization (no-op)
///
/// Apollo3 platforms (Ambiq Apollo3 - SparkFun Artemis family) rely on
/// core initialization. No additional platform-specific initialization is required.

namespace fl {
namespace platforms {

/// @brief Apollo3 platform initialization (no-op)
///
/// Apollo3 platforms (SparkFun Artemis) rely on core initialization.
/// This function is a no-op and exists for API consistency.
inline void init() {
    // No-op: Apollo3 platforms rely on core initialization
}

} // namespace platforms
} // namespace fl
