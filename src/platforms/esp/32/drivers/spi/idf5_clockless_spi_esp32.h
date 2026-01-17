#pragma once

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_CLOCKLESS_SPI

// signal to the world that we have a ClocklessController to allow WS2812 and others.
#define FL_CLOCKLESS_CONTROLLER_DEFINED 1
// Mark that new ChannelEngine-based ClocklessSPI is defined (prevents old alias collision)
#define FL_CLOCKLESS_SPI_CHANNEL_ENGINE_DEFINED 1

#include "crgb.h"
#include "eorder.h"
#include "pixel_iterator.h"
#include "fl/channels/data.h"
#include "fl/channels/engine.h"
#include "fl/channels/bus_manager.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/warn.h"

namespace fl {
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ClocklessSPI : public CPixelLEDController<RGB_ORDER>
{
private:
    // Channel data for transmission
    ChannelDataPtr mChannelData;

    // Channel engine reference (manager provides best available engine)
    IChannelEngine* mEngine;

    // -- Verify that the pin is valid
    static_assert(FastPin<DATA_PIN>::validpin(), "This pin has been marked as an invalid pin, common reasons includes it being a ground pin, read only, or too noisy (e.g. hooked up to the uart).");

public:
    ClocklessSPI()
        : mEngine(&channelBusManager())
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
        // Wait for previous transmission to complete and release buffer
        // This prevents race conditions when show() is called faster than hardware can transmit
        uint32_t startTime = millis();
        uint32_t lastWarnTime = startTime;
        while (mChannelData->isInUse()) {
            mEngine->poll();  // Keep polling until buffer is released

            // Warn every second if still waiting (possible deadlock or hardware issue)
            uint32_t elapsed = millis() - startTime;
            if (elapsed > 1000 && (millis() - lastWarnTime) >= 1000) {
                FL_WARN("ClocklessSPI: Buffer still busy after " << elapsed << "ms total - possible deadlock or slow hardware");
                lastWarnTime = millis();
            }
        }

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

#endif // FASTLED_ESP32_HAS_CLOCKLESS_SPI
