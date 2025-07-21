#pragma once

/// FastLED Networking Header - Conditional networking functionality
/// 
/// This header provides conditional access to FastLED networking functionality.
/// Networking is disabled by default to reduce binary size and dependencies.
/// 
/// To enable networking, compile with: -DFASTLED_HAS_NETWORKING=1
/// Or set the CMake option: -DFASTLED_HAS_NETWORKING=ON

#ifdef FASTLED_HAS_NETWORKING

// Include all networking functionality when enabled
#include "fl/networking/socket.h"
#include "fl/networking/socket_factory.h"

namespace fl {

/// Check if networking is available at compile time
constexpr bool has_networking() {
    return true;
}

} // namespace fl

#else // FASTLED_HAS_NETWORKING not defined

namespace fl {

/// Check if networking is available at compile time
constexpr bool has_networking() {
    return false;
}

// Provide empty namespace stubs when networking is disabled
// This allows code to check for networking availability without compilation errors

/// Placeholder socket error for when networking is disabled
enum class SocketError {
    NETWORKING_DISABLED = 0
};

/// Placeholder socket state for when networking is disabled  
enum class SocketState {
    NETWORKING_DISABLED = 0
};

/// Placeholder IP version for when networking is disabled
enum class IpVersion {
    NETWORKING_DISABLED = 0
};

} // namespace fl

#endif // FASTLED_HAS_NETWORKING 
