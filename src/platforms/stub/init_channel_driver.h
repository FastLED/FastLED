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
/// `fl::enableDrivers<Bus::BIT_BANG>()` or `FastLED.enableAllDrivers()` directly.
inline void initChannelDrivers() FL_NO_EXCEPT {
    // No auto-registration. See `fl::enableDrivers<Bus::BIT_BANG>()` for opt-in.
}

}  // namespace platforms
}  // namespace fl
