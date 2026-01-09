// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/win/init_win.h
/// @brief Windows platform initialization
///
/// Windows platform initialization is currently a no-op. Winsock initialization
/// is handled lazily on first socket use. This provides a hook for future
/// initialization needs.

namespace fl {
namespace platforms {

/// @brief Initialize Windows platform
///
/// Performs one-time initialization of Windows-specific subsystems.
///
/// Currently a no-op. Winsock (Windows Sockets API) is initialized lazily
/// on first socket use in socket_win.cpp. Moving this to early init would
/// require extracting the WSAStartup call into a reusable function.
///
/// This function is called once during FastLED::init() and is safe to call
/// multiple times (subsequent calls are no-ops).
///
/// Future considerations:
/// - Early Winsock initialization (WSAStartup)
/// - High-resolution timer setup
/// - COM library initialization for advanced features
inline void init() {
    // No-op: Winsock is currently initialized on-demand
    // Future: Consider early WSAStartup() for networking predictability
}

} // namespace platforms
} // namespace fl
