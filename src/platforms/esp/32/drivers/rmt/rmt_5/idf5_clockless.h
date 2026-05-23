#pragma once

// IWYU pragma: private

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

// signal to the world that we have a ClocklessController to allow WS2812 and others.
#define FL_CLOCKLESS_CONTROLLER_DEFINED 1

#include "eorder.h"
#include "fl/channels/bus.h"
#include "fl/channels/channel.h"
#include "fl/channels/config.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"
#include "platforms/esp/32/drivers/rmt/rmt_5/bus_traits.h"

namespace fl {
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class ClocklessIdf5 : public Channel
{
    // -- Verify that the pin is valid
    FL_STATIC_ASSERT(FastPin<DATA_PIN>::validpin(), "This pin has been marked as an invalid pin, common reasons includes it being a ground pin, read only, or too noisy (e.g. hooked up to the uart).");

    static ChipsetVariant makeChipset() FL_NOEXCEPT {
        return ClocklessChipset(DATA_PIN, makeTimingConfig<TIMING>());
    }

public:
    ClocklessIdf5() FL_NOEXCEPT
        : Channel(makeChipset(), RGB_ORDER, RegistrationMode::DeferRegister)
    {
        // Register the RMT5 driver with ChannelManager so onEndFrame() will
        // iterate mDrivers and call driver->show() to actually transmit. The
        // pre-bind via setDriver() below only short-circuits per-frame driver
        // *selection* in showPixels() -- it does NOT trigger transmission.
        // Post-#2469 drivers no longer auto-register at static-init time, so
        // legacy addLeds<>-style controllers must do this explicitly here or
        // the encoded pixel data sits enqueued and never reaches the hardware.
        // ChannelManager::addDriver() is idempotent for true duplicates, so
        // this is safe to call from every ClocklessIdf5 instantiation.
        BusTraits<Bus::RMT>::registerWithManager();
        // Phase 5b of #2428: pre-bind to the RMT5 driver singleton so
        // showPixels() bypasses ChannelManager::selectDriverForChannel()
        // on every frame -- avoids the per-frame priority lookup overhead.
        setDriver(BusTraits<Bus::RMT>::instancePtr());
        // Auto-register in the controller draw list (template API expects this)
        addToList();
    }

    void init() FL_NOEXCEPT override { }
    virtual u16 getMaxRefreshRate() const FL_NOEXCEPT { return 800; }
};

// Backward compatibility alias
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
using ClocklessRMT = ClocklessIdf5<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;

}  // namespace fl

#endif // FASTLED_RMT5
