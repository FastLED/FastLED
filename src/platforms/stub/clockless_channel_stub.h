#pragma once

// IWYU pragma: private

// Channel-based clockless controller for stub platform
// Models ESP32's channel-based clockless architecture for testing

#define FL_CLOCKLESS_CONTROLLER_DEFINED 1
#define FL_CLOCKLESS_STUB_CHANNEL_ENGINE_DEFINED 1
#define FASTLED_CLOCKLESS_STUB_DEFINED 1

#include "eorder.h"
#include "fl/stl/compiler_control.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
#include "fl/stl/weak_ptr.h"
#include "pixel_iterator.h"
#include "fl/warn.h"
#include "fl/stl/vector.h"
#include "platforms/shared/active_strip_tracker/active_strip_tracker.h"
#include "platforms/stub/stub_gpio.h"

namespace fl {

/// @brief Channel-based clockless controller for stub platform
///
/// This controller integrates with the channel driver infrastructure,
/// allowing the legacy FastLED.addLeds<>() API to route through channel drivers
/// for testing. It mirrors the architecture of ESP32's ClocklessIdf5.
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
private:
    // Channel data for transmission
    ChannelDataPtr mChannelData;

    // Channel driver reference (weak pointer for lifetime safety)
    fl::weak_ptr<IChannelDriver> mDriver;

    // LED capture tracker for simulation/testing
    ActiveStripTracker mTracker;
    fl::vector<u8> mCaptureData;

public:
    ClocklessController()
    {
        // Create channel data with pin and timing configuration
        ChipsetTimingConfig timing = makeTimingConfig<TIMING>();
        mChannelData = ChannelData::create(DATA_PIN, timing);
    }

    virtual void init() override { }

protected:
    virtual void showPixels(PixelController<RGB_ORDER>& pixels) override
    {
        // Get driver (lock weak_ptr to shared_ptr)
        fl::shared_ptr<IChannelDriver> driver = mDriver.lock();

        // If driver is null/expired, select one from ChannelManager
        if (!driver) {
            driver = ChannelManager::instance().selectDriverForChannel(mChannelData, fl::string());  // Empty affinity
            if (driver) {
                // Cache the selected driver as weak_ptr
                mDriver = driver;
            } else {
                FL_ERROR("ClocklessController(stub): No compatible driver found - cannot transmit");
                return;
            }
        }

        // Wait for previous transmission to complete and release buffer
        // This prevents race conditions when show() is called faster than hardware can transmit
        u32 startTime = fl::millis();
        u32 lastWarnTime = startTime;
        while (mChannelData->isInUse()) {
            driver->poll();  // Keep polling until buffer is released

            // Warn every second if still waiting (possible deadlock or hardware issue)
            u32 elapsed = fl::millis() - startTime;
            if (elapsed > 1000 && (fl::millis() - lastWarnTime) >= 1000) {
                FL_WARN("ClocklessController(stub): Buffer still busy after " << elapsed << "ms total - possible deadlock or slow hardware");
                lastWarnTime = fl::millis();
            }
        }

        // Capture LED data for simulation/testing BEFORE encoding
        // Use separate pixel controller with RGB order and no color adjustment
        mCaptureData.clear();
        PixelController<RGB> pixels_rgb = pixels; // Converts to RGB pixels
        // NOTE: disableColorAdjustment() only sets color=white but keeps brightness
        // We need to manually ensure full brightness for accurate capture
        #if FASTLED_HD_COLOR_MIXING
        pixels_rgb.mColorAdjustment.brightness = 255;
        #endif
        pixels_rgb.disableColorAdjustment();
        auto capture_iterator = pixels_rgb.as_iterator(RgbwInvalid());
        capture_iterator.writeWS2812(&mCaptureData);
        mTracker.update(mCaptureData);

        // Convert pixels to encoded byte data for transmission
        fl::PixelIterator iterator = pixels.as_iterator(this->getRgbw());
        auto& data = mChannelData->getData();
        data.clear();
        iterator.writeWS2812(&data);

        // Enqueue for transmission (will be sent when driver->show() is called)
        driver->enqueue(mChannelData);
    }
};

// Adapter for timing-like objects via duck typing
template <int DATA_PIN, typename TIMING_LIKE, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
struct ClocklessControllerAdapter : public ClocklessController<DATA_PIN, TIMING_LIKE, RGB_ORDER, XTRA0, FLIP, WAIT_TIME> {
    // Inherits all functionality from ClocklessController
};

// ClocklessBlockController for type-based timing
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
class ClocklessBlockController : public ClocklessController<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME> {
    // Inherits all functionality from ClocklessController
};

}  // namespace fl
