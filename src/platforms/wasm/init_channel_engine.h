#pragma once

/// @file init_channel_engine.h
/// @brief WASM platform channel engine initialization
///
/// This registers the stub channel engine so the legacy FastLED.addLeds<>() API
/// can route through the channel engine infrastructure for web builds.

#include "fl/channels/bus_manager.h"
#include "fl/channels/channel.h"

namespace fl {
namespace platforms {

/// @brief Initialize channel engines for WASM platform
///
/// Registers the stub channel engine with the ChannelBusManager.
/// WASM uses stub engine because there's no real hardware in browser.
/// This allows the legacy API to work with channel engines in web builds.
inline void initChannelEngines() {
    fl::ChannelBusManager& manager = fl::ChannelBusManager::instance();

    // Get the stub engine singleton
    auto* stubEngine = fl::getStubChannelEngine();

    // Wrap it in a shared_ptr without taking ownership
    auto sharedStub = fl::make_shared_no_tracking(*stubEngine);

    // Register with low priority (stub is a fallback/testing engine)
    manager.addEngine(0, sharedStub, "STUB");
}

}  // namespace platforms
}  // namespace fl
