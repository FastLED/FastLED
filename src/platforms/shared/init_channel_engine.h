#pragma once

/// @file init_channel_engine.h
/// @brief Default (no-op) channel engine initialization
///
/// This is the fallback implementation for platforms that don't have
/// platform-specific channel engines. Currently, only ESP32 has channel engines.

namespace fl {
namespace platform {

/// @brief Initialize channel engines (no-op for non-ESP32 platforms)
///
/// This is a no-op function for platforms that don't use the channel bus manager.
/// On ESP32, this is replaced by the platform-specific implementation.
inline void initChannelEngines() {
    // No-op: Non-ESP32 platforms don't have channel engines
}

}  // namespace platform
}  // namespace fl
