#pragma once

// Channel-based SPI controller for WASM platform
// Models stub platform's SPI channel architecture for web builds

#define FL_CLOCKLESS_SPI_CHANNEL_ENGINE_DEFINED 1

#include "eorder.h"
#include "fl/unused.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/channels/data.h"
#include "fl/channels/engine.h"
#include "fl/channels/bus_manager.h"
#include "pixel_iterator.h"
#include "fl/warn.h"

namespace fl {

/// @brief Channel-based SPI controller for WASM platform
///
/// This controller integrates with the channel engine infrastructure for
/// SPI-based WS2812 driving in web builds. Uses stub engine (no real hardware).
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ClocklessSPI : public CPixelLEDController<RGB_ORDER>
{
private:
    // Channel data for transmission
    ChannelDataPtr mChannelData;

    // Channel engine reference (manager provides best available engine)
    IChannelEngine* mEngine;

public:
    ClocklessSPI()
        : mEngine(&channelBusManager())
    {
        // Create channel data with pin and timing configuration
        ChipsetTimingConfig timing = makeTimingConfig<TIMING>();
        mChannelData = ChannelData::create(DATA_PIN, timing);
    }

    void init() override { }
    virtual u16 getMaxRefreshRate() const { return 800; }

protected:
    // Prepares data for the draw.
    virtual void showPixels(PixelController<RGB_ORDER>& pixels) override
    {
        // Wait for previous transmission to complete and release buffer
        // This prevents race conditions when show() is called faster than hardware can transmit
        u32 startTime = fl::millis();
        u32 lastWarnTime = startTime;
        while (mChannelData->isInUse()) {
            mEngine->poll();  // Keep polling until buffer is released

            // Warn every second if still waiting (possible deadlock or hardware issue)
            u32 elapsed = fl::millis() - startTime;
            if (elapsed > 1000 && (fl::millis() - lastWarnTime) >= 1000) {
                FL_WARN("ClocklessSPI(wasm): Buffer still busy after " << elapsed << "ms total");
                lastWarnTime = fl::millis();
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
