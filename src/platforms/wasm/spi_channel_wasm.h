#pragma once

// IWYU pragma: private

// Channel-based SPI controller for WASM platform
// Models stub platform's SPI channel architecture for web builds

#define FL_CLOCKLESS_SPI_CHANNEL_ENGINE_DEFINED 1

#include "eorder.h"
#include "fl/stl/compiler_control.h"
#include "fl/channels/slim_bridge_controller.h"
#include "platforms/stub/bus_traits.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @brief Channel-based SPI controller for WASM platform
///
/// This controller integrates with the channel driver infrastructure for
/// SPI-based WS2812 driving in web builds. Uses stub driver (no real hardware).
///
/// Registered with `ChannelManager` via the `SlimBridgeController` base
/// constructor and flushed by `ChannelManager::onEndFrame()`.
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ClocklessSPI : public SlimBridgeController<DATA_PIN, TIMING, RGB_ORDER, WAIT_TIME, BusTraits<Bus::BIT_BANG>>
{
public:
    u16 getMaxRefreshRate() const FL_NO_EXCEPT override { return 800; }
};

}  // namespace fl
