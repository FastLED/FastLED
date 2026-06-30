#pragma once

// IWYU pragma: private

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_PARLIO

#define FL_CLOCKLESS_CONTROLLER_DEFINED 1

#include "eorder.h"
#include "fl/channels/bus.h"
#include "fl/channels/channel.h"
#include "fl/channels/config.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"
#include "platforms/esp/32/drivers/parlio/bus_traits.h"

namespace fl {

template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class ClocklessPARLIO : public Channel
{
    FL_STATIC_ASSERT(FastPin<DATA_PIN>::validpin(), "This pin has been marked as an invalid pin, common reasons includes it being a ground pin, read only, or too noisy (e.g. hooked up to the uart).");

    static ChipsetVariant makeChipset() FL_NO_EXCEPT {
        return ClocklessChipset(DATA_PIN, makeTimingConfig<TIMING>());
    }

public:
    ClocklessPARLIO() FL_NO_EXCEPT
        : Channel(makeChipset(), RGB_ORDER, RegistrationMode::DeferRegister)
    {
        BusTraits<Bus::FLEX_IO, 0>::registerWithManager();
        setDriver(BusTraits<Bus::FLEX_IO, 0>::instancePtr());
        addToList();
    }

    void init() FL_NO_EXCEPT override { }
    u16 getMaxRefreshRate() const FL_NO_EXCEPT override { return 800; }
};

}  // namespace fl

#endif  // FASTLED_ESP32_HAS_PARLIO
