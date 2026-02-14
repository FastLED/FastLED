#pragma once

// IWYU pragma: private

/// @file init_channel_engine.h
/// @brief Default channel engine initialization (stub engine fallback)
///
/// This is the fallback implementation for platforms that don't have
/// platform-specific channel engines. Registers the stub engine so that
/// the legacy FastLED.addLeds<>() API works in tests and on unsupported platforms.

#include "fl/channels/bus_manager.h"
#include "fl/channels/channel.h"

namespace fl {
namespace platforms {

/// @brief Initialize channel engines (registers stub engine for non-hardware platforms)
///
/// For platforms without hardware-specific channel engines (stub, posix, tests),
/// this registers the stub engine as a fallback. This allows the legacy API to work
/// and enables LED capture for testing.
inline void initChannelEngines() {
    // Register the stub channel engine (low priority fallback)
    // This allows tests and unsupported platforms to use the channel API
    fl::ChannelBusManager& manager = fl::ChannelBusManager::instance();

    // Get the stub engine singleton
    auto* stubEngine = fl::getStubChannelEngine();

    // Wrap it in a shared_ptr without taking ownership using make_shared_no_tracking
    // This is a zero-overhead wrapper that doesn't create a control block
    auto sharedStub = fl::make_shared_no_tracking(*stubEngine);

    // Register with low priority so platform-specific engines can override
    manager.addEngine(0, sharedStub);
}

}  // namespace platforms
}  // namespace fl
