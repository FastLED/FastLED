#pragma once

// IWYU pragma: private

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_CLOCKLESS_SPI

// signal to the world that we have a ClocklessController to allow WS2812 and others.
#define FL_CLOCKLESS_CONTROLLER_DEFINED 1
// Mark that new ChannelEngine-based ClocklessSPI is defined (prevents old alias collision)
#define FL_CLOCKLESS_SPI_CHANNEL_ENGINE_DEFINED 1

#include "crgb.h"
#include "eorder.h"
#include "fl/channels/slim_bridge_controller.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"
#include "platforms/esp/32/core/fastpin_esp32.h"
#include "platforms/esp/32/drivers/spi/bus_traits.h"

namespace fl {

// Slim bridge (issue #4584): thin subclass of SlimBridgeController that
// registers with ChannelManager via BusTraits<Bus::SPI> so
// ChannelManager's show() actually reaches this driver. Note behavior
// change: WAIT_TIME now folds into reset_us (mirrors ClocklessIdf4's
// reset_us folding) -- previously WAIT_TIME was ignored here.
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ClocklessSPI : public SlimBridgeController<DATA_PIN, TIMING, RGB_ORDER, WAIT_TIME, BusTraits<Bus::SPI>>
{
private:
    // -- Verify that the pin is valid
    FL_STATIC_ASSERT(FastPin<DATA_PIN>::validpin(), "This pin has been marked as an invalid pin, common reasons includes it being a ground pin, read only, or too noisy (e.g. hooked up to the uart).");

public:
    u16 getMaxRefreshRate() const FL_NO_EXCEPT override { return 800; }
};

}  // namespace fl

#endif // FASTLED_ESP32_HAS_CLOCKLESS_SPI
