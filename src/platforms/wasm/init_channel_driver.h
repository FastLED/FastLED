#pragma once

// IWYU pragma: private

/// @file init_channel_driver.h
/// @brief WASM platform channel driver initialization
///
/// This registers the stub channel driver so the legacy FastLED.addLeds<>() API
/// can route through the channel driver infrastructure for web builds.

#include "fl/channels/manager.h"
#include "fl/channels/channel.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace platforms {

/// @brief Initialize channel drivers for WASM platform
///
/// Registers the stub channel driver with the ChannelManager.
/// WASM uses stub driver because there's no real hardware in browser.
/// This allows the legacy API to work with channel drivers in web builds.
inline void initChannelDrivers() FL_NOEXCEPT {
    fl::ChannelManager& manager = fl::ChannelManager::instance();

    // Get the stub driver singleton
    auto* stubEngine = fl::getStubChannelEngine();

    // Wrap it in a shared_ptr without taking ownership
    auto sharedStub = fl::make_shared_no_tracking(*stubEngine);

    // Register with low priority (stub is a fallback/testing driver)
    manager.addDriver(0, sharedStub);
}

}  // namespace platforms
}  // namespace fl
