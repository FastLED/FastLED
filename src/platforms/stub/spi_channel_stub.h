#pragma once

// IWYU pragma: private

// Channel-based SPI controller for stub platform
// Models ESP32's channel-based SPI clockless architecture for testing

#define FL_CLOCKLESS_SPI_CHANNEL_ENGINE_DEFINED 1

#include "eorder.h"
#include "fl/stl/compiler_control.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/channels/bus.h"
#include "fl/channels/data.h"
#include "fl/channels/slim_bridge_controller.h"
#include "pixel_iterator.h"
#include "fl/log/log.h"
#include "platforms/stub/bus_traits.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @brief Channel-based SPI controller for stub platform
///
/// This controller integrates with the channel driver infrastructure,
/// allowing the legacy FastLED.addLeds<SPI_CHIPSET>() API to route through
/// channel drivers for testing. It mirrors the architecture of ESP32's ClocklessSPI.
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ClocklessSPI : public SlimBridgeController<DATA_PIN, TIMING, RGB_ORDER, WAIT_TIME, BusTraits<Bus::BIT_BANG>>
{
public:
    virtual u16 getMaxRefreshRate() const FL_NO_EXCEPT override { return 800; }
};

}  // namespace fl
