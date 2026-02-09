#pragma once

/// @file init_channel_engine.h
/// @brief Stub platform channel engine initialization
///
/// This registers the stub channel engine so the legacy FastLED.addLeds<>() API
/// can route through the channel engine infrastructure for testing.

#include "fl/channels/bus_manager.h"
#include "fl/channels/channel.h"

namespace fl {
namespace platforms {

/// @brief Initialize channel engines for stub platform
///
/// Registers the stub channel engine with the ChannelBusManager.
/// This allows the legacy API to work with channel engines in tests.
inline void initChannelEngines() {
    // Register the stub channel engine (low priority)
    // This allows tests to override with higher-priority mock engines
    fl::ChannelBusManager& manager = fl::ChannelBusManager::instance();

    // Get the stub engine singleton
    auto* stubEngine = fl::getStubChannelEngine();

    // Wrap it in a shared_ptr without taking ownership (non-deleting)
    auto sharedStub = fl::shared_ptr<IChannelEngine>(stubEngine, [](IChannelEngine*){});

    // Register with low priority so test mocks can override
    manager.addEngine(0, sharedStub);
}

}  // namespace platforms
}  // namespace fl
