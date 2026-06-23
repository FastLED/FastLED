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

namespace fl {
namespace platforms {

/// @brief Initialize channel drivers for stub platform (no-op).
///
/// Post-#2428 the stub platform does not auto-register any driver with
/// `ChannelManager`. Tests that need the stub driver call
/// `fl::enableDrivers<Bus::STUB>()` or `FastLED.enableAllDrivers()` directly.
inline void initChannelDrivers() FL_NOEXCEPT {
    // No auto-registration. See `fl::enableDrivers<Bus::STUB>()` for opt-in.
}

}  // namespace platforms
}  // namespace fl
