#pragma once

// IWYU pragma: private

/// @file init_channel_engine.h
/// @brief Stub platform channel engine initialization
///
/// Registers ClocklessChannelEngineStub with the ChannelBusManager so that
/// the FastLED.add() API (Channel objects) drives stub GPIO simulation and
/// fires SimEdgeObserver callbacks that NativeRxDevice uses for validation.

#include "fl/channels/bus_manager.h"
#include "fl/channels/channel.h"
#include "fl/stl/shared_ptr.h"
#include "platforms/stub/clockless_channel_engine_stub.h"

namespace fl {
namespace platforms {

/// @brief Initialize channel engines for stub platform
inline void initChannelEngines() {
    fl::ChannelBusManager& manager = fl::ChannelBusManager::instance();

    // Register the stub clockless engine with priority 1.
    // Priority 1 > 0 (the no-op StubChannelEngine registered elsewhere),
    // so this engine is preferred for clockless channels on the stub platform.
    static fl::stub::ClocklessChannelEngineStub stubEngine;  // ok static in header
    // Cast to base so make_shared_no_tracking creates shared_ptr<IChannelEngine>
    fl::IChannelEngine& engineRef = stubEngine;
    auto sharedEngine = fl::make_shared_no_tracking(engineRef);
    manager.addEngine(1, sharedEngine);
}

}  // namespace platforms
}  // namespace fl
