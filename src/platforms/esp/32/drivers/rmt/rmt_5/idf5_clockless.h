#pragma once

// IWYU pragma: private

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

// signal to the world that we have a ClocklessController to allow WS2812 and others.
#define FL_CLOCKLESS_CONTROLLER_DEFINED 1

#include "eorder.h"
#include "fl/channels/bus.h"
#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
#include "fl/chipsets/encoders/pixel_iterator.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"
#include "platforms/esp/32/drivers/rmt/rmt_5/bus_traits.h"

namespace fl {

/// Legacy FastLED.addLeds<> clockless controller on the RMT5 engine.
///
/// A plain CPixelLEDController (not an fl::Channel) that still bridges through
/// ChannelManager: the RMT5 driver is registered with the manager, which owns
/// the frame (begin-frame wait, end-frame show() + async drain) for legacy and
/// Channel API strips alike. The controller owns one ChannelData whose byte
/// buffer is cleared and refilled each frame, so after the first frame no
/// allocation happens on the show path.
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class ClocklessIdf5 : public CPixelLEDController<RGB_ORDER>
{
    // -- Verify that the pin is valid
    FL_STATIC_ASSERT(FastPin<DATA_PIN>::validpin(), "This pin has been marked as an invalid pin, common reasons includes it being a ground pin, read only, or too noisy (e.g. hooked up to the uart).");

    ChannelDataPtr mChannelData;

public:
    ClocklessIdf5() FL_NO_EXCEPT
        : mChannelData(ChannelData::create(DATA_PIN, makeTimingConfig<TIMING>()))
    {
        // ChannelManager::addDriver() is idempotent for duplicates.
        BusTraits<Bus::RMT>::registerWithManager();
    }

    void init() FL_NO_EXCEPT override { }
    virtual u16 getMaxRefreshRate() const FL_NO_EXCEPT { return 800; }

protected:
    void showPixels(PixelController<RGB_ORDER> &pixels) FL_NO_EXCEPT override {
        auto &driver = BusTraits<Bus::RMT>::instance();
        if (mChannelData->isInUse() && !driver.waitForReady()) {
            return;
        }
        fl::PixelIterator iterator = pixels.as_iterator(this->getRgbw());
        auto &data = mChannelData->getData();
        data.clear();  // keeps capacity: the buffer is reused frame to frame
        iterator.writeWS2812(&data);
        driver.enqueue(mChannelData);
    }
};

// Backward compatibility alias
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
using ClocklessRMT = ClocklessIdf5<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;

}  // namespace fl

#endif // FASTLED_RMT5
