#pragma once

// IWYU pragma: private

/// @file init_channel_driver.h
/// @brief Default channel driver initialization (stub driver fallback)
///
/// This is the fallback implementation for platforms that don't have
/// platform-specific channel drivers. Registers the stub driver so that
/// the legacy FastLED.addLeds<>() API works in tests and on unsupported platforms.

#include "fl/channels/manager.h"
#include "fl/channels/channel.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace platforms {

/// @brief Initialize channel drivers (registers stub driver for non-hardware platforms)
///
/// For platforms without hardware-specific channel drivers (stub, posix, tests),
/// this registers the stub driver as a fallback. This allows the legacy API to work
/// and enables LED capture for testing.
inline void initChannelDrivers() FL_NOEXCEPT {
    // Register the stub channel driver (low priority fallback)
    // This allows tests and unsupported platforms to use the channel API
    fl::ChannelManager& manager = fl::ChannelManager::instance();

    // Get the stub driver singleton
    auto* stubEngine = fl::getStubChannelEngine();

    // Wrap it in a shared_ptr without taking ownership using make_shared_no_tracking
    // This is a zero-overhead wrapper that doesn't create a control block
    auto sharedStub = fl::make_shared_no_tracking(*stubEngine);

    // Register with low priority so platform-specific drivers can override
    manager.addDriver(0, sharedStub);
}

}  // namespace platforms
}  // namespace fl
