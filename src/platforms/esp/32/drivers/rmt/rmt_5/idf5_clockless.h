#pragma once

// IWYU pragma: private

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

// signal to the world that we have a ClocklessController to allow WS2812 and others.
#define FL_CLOCKLESS_CONTROLLER_DEFINED 1

#include "eorder.h"
#include "fl/channels/channel.h"
#include "fl/channels/config.h"
#include "fl/chipsets/timing_traits.h"

namespace fl {
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class ClocklessIdf5 : public Channel
{
    // -- Verify that the pin is valid
    static_assert(FastPin<DATA_PIN>::validpin(), "This pin has been marked as an invalid pin, common reasons includes it being a ground pin, read only, or too noisy (e.g. hooked up to the uart).");

    static ChipsetVariant makeChipset() {
        return ClocklessChipset(DATA_PIN, makeTimingConfig<TIMING>());
    }

public:
    ClocklessIdf5()
        : Channel(makeChipset(), RGB_ORDER, RegistrationMode::DeferRegister)
    {
        // Auto-register in the controller draw list (template API expects this)
        addToList();
    }

    void init() override { }
    virtual u16 getMaxRefreshRate() const { return 800; }
};

// Backward compatibility alias
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
using ClocklessRMT = ClocklessIdf5<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;

}  // namespace fl

#endif // FASTLED_RMT5
