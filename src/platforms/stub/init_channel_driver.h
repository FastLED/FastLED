#pragma once

// IWYU pragma: private

/// @file init_channel_driver.h
/// @brief Stub platform channel driver initialization
///
/// This registers the stub channel driver so the legacy FastLED.addLeds<>() API
/// can route through the channel driver infrastructure for testing.

#include "fl/channels/manager.h"
#include "fl/channels/channel.h"
#include "fl/stl/shared_ptr.h"
#include "platforms/stub/bus_traits.h"
#include "platforms/stub/clockless_channel_engine_stub.h"
#include "fl/stl/noexcept.h"

// FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY: when defined to 1, this stub auto-init
// is a no-op. Users opting in must call `fl::enableDrivers<fl::Bus::STUB>()`
// explicitly. Default 0 for backward compatibility. See #2428 Phase 5.
#ifndef FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY
#define FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY 0
#endif

namespace fl {
namespace platforms {

/// @brief Initialize channel drivers for stub platform
///
/// Registers ClocklessChannelEngineStub with the ChannelManager so that
/// the FastLED.add() API (Channel objects) drives stub GPIO simulation and
/// fires SimEdgeObserver callbacks that NativeRxDevice uses for validation.
///
/// Delegates to `BusTraits<Bus::STUB>::instancePtr()` so the singleton is
/// shared with the new `fl::enableDrivers<Bus::STUB>()` API. Phase 5b will
/// flip the default of `FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY` to 1.
inline void initChannelDrivers() FL_NOEXCEPT {
#if FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY
    // Opt-in mode: skip auto-registration. Users call enableDrivers<Bus::STUB>().
#else
    fl::ChannelManager& manager = fl::ChannelManager::instance();
    // Priority 1 > 0 (the no-op StubChannelDriver registered elsewhere),
    // so this driver is preferred for clockless channels on the stub platform.
    manager.addDriver(1, BusTraits<Bus::STUB>::instancePtr());
#endif  // !FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY
}

}  // namespace platforms
}  // namespace fl
