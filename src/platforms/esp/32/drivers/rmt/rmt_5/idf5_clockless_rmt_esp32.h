#pragma once

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5



// signal to the world that we have a ClocklessController to allow WS2812 and others.
#define FL_CLOCKLESS_CONTROLLER_DEFINED 1

//#define FASTLED_RMT_USE_DMA

#include "crgb.h"
#include "eorder.h"
#include "pixel_iterator.h"
#include "fl/channels/channel_data.h"
#include "fl/channels/channel_engine.h"
#include "channel_engine_rmt.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/singleton.h"

namespace fl {
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class ClocklessRMT : public CPixelLEDController<RGB_ORDER>
{
private:
    // Channel data for transmission
    ChannelDataPtr mChannelData;

    // Channel engine reference (singleton)
    ChannelEngine* mEngine;

    // -- Verify that the pin is valid
    static_assert(FastPin<DATA_PIN>::validpin(), "This pin has been marked as an invalid pin, common reasons includes it being a ground pin, read only, or too noisy (e.g. hooked up to the uart).");

public:
    ClocklessRMT()
        : mEngine(fl::Singleton<ChannelEngineRMT>::instanceRef())
    {
        // Create channel data with pin and timing configuration
        ChipsetTimingConfig timing = makeTimingConfig<TIMING>();
        mChannelData = ChannelData::create(DATA_PIN, timing);
    }

    void init() override { }
    virtual uint16_t getMaxRefreshRate() const { return 800; }

protected:

    // Prepares data for the draw.
    virtual void showPixels(PixelController<RGB_ORDER> &pixels) override
    {
        // Fail-fast race condition detection: Buffer MUST NOT be in use
        // If this assertion fires, the hardware wait in releaseChannel() is insufficient
        FL_ASSERT(!mChannelData->isInUse(), "ClocklessRMT: Race condition - buffer still in use by engine!");

        // Convert pixels to encoded byte data
        fl::PixelIterator iterator = pixels.as_iterator(this->getRgbw());
        auto& data = mChannelData->getData();
        data.clear();
        iterator.writeWS2812(&data);

        // Enqueue for transmission (will be sent when engine->show() is called)
        mEngine->enqueue(mChannelData);
    }
};
}  // namespace fl

#endif // FASTLED_RMT5
