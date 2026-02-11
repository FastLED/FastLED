#pragma once

// Channel-based SPI controller for stub platform
// Models ESP32's channel-based SPI clockless architecture for testing

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

/// @brief Channel-based SPI controller for stub platform
///
/// This controller integrates with the channel engine infrastructure,
/// allowing the legacy FastLED.addLeds<SPI_CHIPSET>() API to route through
/// channel engines for testing. It mirrors the architecture of ESP32's ClocklessSPI.
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ClocklessSPI : public CPixelLEDController<RGB_ORDER>
{
private:
    // Channel data for transmission
    ChannelDataPtr mChannelData;

    // Channel engine reference (selected dynamically from bus manager)
    fl::shared_ptr<IChannelEngine> mEngine;

public:
    ClocklessSPI()
        : mEngine(getStubSpiEngine())
    {
        // Create channel data with pin and timing configuration
        ChipsetTimingConfig timing = makeTimingConfig<TIMING>();
        mChannelData = ChannelData::create(DATA_PIN, timing);
    }

    void init() override { }
    virtual u16 getMaxRefreshRate() const { return 800; }

protected:
    // -- Show pixels
    //    This is the main entry point for the controller.
    virtual void showPixels(PixelController<RGB_ORDER>& pixels) override
    {
        if (!mEngine) {
            FL_WARN_EVERY(100, "No Engine");
            return;
        }
        // Wait for previous transmission to complete and release buffer
        // This prevents race conditions when show() is called faster than hardware can transmit
        u32 startTime = fl::millis();
        u32 lastWarnTime = startTime;
        if (mChannelData->isInUse()) {

                FL_WARN_EVERY(100, "ClocklessSPI(stub): engine should have finished transmitting by now - waiting");
                bool finished = mEngine->waitForReady();
                if (!finished) {
                    FL_ERROR("ClocklessSPI(stub): Engine still busy after " << fl::millis() - startTime << "ms");
                    return;
                }

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

    static fl::shared_ptr<IChannelEngine> getStubSpiEngine() {
        return ChannelBusManager::instance().getEngineByName("SPI");
    }
};

}  // namespace fl
