// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/arm/sam/init_sam.h
/// @brief SAM platform initialization (no-op)
///
/// SAM platforms (Arduino Due - SAM3X8E) rely primarily on Arduino core initialization.
/// No additional platform-specific initialization is required.

namespace fl {
namespace platforms {

/// @brief SAM platform initialization (no-op)
///
/// SAM platforms (Arduino Due) rely on Arduino core for initialization.
/// This function is a no-op and exists for API consistency.
inline void init() {
    // No-op: SAM platforms rely on Arduino core initialization
}

} // namespace platforms
} // namespace fl
