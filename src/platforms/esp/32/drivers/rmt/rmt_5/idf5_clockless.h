#pragma once

// IWYU pragma: private

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

// signal to the world that we have a ClocklessController to allow WS2812 and others.
#define FL_CLOCKLESS_CONTROLLER_DEFINED 1

#include "eorder.h"
#include "fl/channels/bus.h"
#include "fl/channels/slim_bridge_controller.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"
#include "platforms/esp/32/drivers/rmt/rmt_5/bus_traits.h"

namespace fl {

// Legacy `addLeds<>()` entry point for the ESP32 RMT5 driver. This no
// longer goes through `fl::Channel`: ClocklessIdf5 -> SlimBridgeController
// -> RMT5 IChannelDriver (via `BusTraits<Bus::RMT>`). SlimBridgeController
// owns the ChannelData buffer, re-encodes it every frame, and hands it to
// the RMT5 driver singleton through the shared IChannelDriver contract.
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class ClocklessIdf5 : public SlimBridgeController<DATA_PIN, TIMING, RGB_ORDER, WAIT_TIME, BusTraits<Bus::RMT>>
{
    // -- Verify that the pin is valid
    FL_STATIC_ASSERT(FastPin<DATA_PIN>::validpin(), "This pin has been marked as an invalid pin, common reasons includes it being a ground pin, read only, or too noisy (e.g. hooked up to the uart).");

public:
    ClocklessIdf5() FL_NO_EXCEPT = default;

    void init() FL_NO_EXCEPT override { }
    virtual u16 getMaxRefreshRate() const FL_NO_EXCEPT { return 800; }
};

// Backward compatibility alias
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
using ClocklessRMT = ClocklessIdf5<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;

}  // namespace fl

#endif // FASTLED_RMT5
