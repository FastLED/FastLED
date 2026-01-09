// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/stub/init_stub.h
/// @brief Stub platform initialization (no-op)
///
/// This header provides a no-op initialization function for platforms that
/// don't require any special initialization.

namespace fl {
namespace platforms {

/// @brief Stub platform initialization (no-op)
///
/// This function does nothing and is used for platforms that don't require
/// any special initialization during FastLED::init().
inline void init() {
    // No-op: Platforms using this stub don't need initialization
}

} // namespace platforms
} // namespace fl
