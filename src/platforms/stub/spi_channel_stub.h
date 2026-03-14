#pragma once

// IWYU pragma: private

// Channel-based SPI controller for stub platform
// Models ESP32's channel-based SPI clockless architecture for testing

#define FL_CLOCKLESS_SPI_CHANNEL_ENGINE_DEFINED 1

#include "eorder.h"
#include "fl/stl/compiler_control.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
#include "pixel_iterator.h"
#include "fl/system/log.h"

namespace fl {

/// @brief Channel-based SPI controller for stub platform
///
/// This controller integrates with the channel driver infrastructure,
/// allowing the legacy FastLED.addLeds<SPI_CHIPSET>() API to route through
/// channel drivers for testing. It mirrors the architecture of ESP32's ClocklessSPI.
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ClocklessSPI : public CPixelLEDController<RGB_ORDER>
{
private:
    // Channel data for transmission
    ChannelDataPtr mChannelData;

    // Channel driver reference (selected dynamically from bus manager)
    fl::shared_ptr<IChannelDriver> mDriver;

public:
    ClocklessSPI()
        : mDriver(getStubSpiEngine())
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
        if (!mDriver) {
            FL_WARN_EVERY(100, "No Engine");
            return;
        }
        // Wait for previous transmission to complete and release buffer
        // This prevents race conditions when show() is called faster than hardware can transmit
        u32 startTime = fl::millis();
        u32 lastWarnTime = startTime;
        if (mChannelData->isInUse()) {

                FL_WARN_EVERY(100, "ClocklessSPI(stub): driver should have finished transmitting by now - waiting");
                bool finished = mDriver->waitForReady();
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

        // Enqueue for transmission (will be sent when driver->show() is called)
        mDriver->enqueue(mChannelData);
    }

    static fl::shared_ptr<IChannelDriver> getStubSpiEngine() {
        return ChannelManager::instance().getDriverByName("SPI");
    }
};

}  // namespace fl
