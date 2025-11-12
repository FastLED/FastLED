#pragma once

// signal to the world that we have a ClocklessController to allow WS2812 and others.
#define FL_CLOCKLESS_CONTROLLER_DEFINED 1

#define FASTLED_RMT_USE_DMA

#include "crgb.h"
#include "eorder.h"
#include "pixel_iterator.h"
#include "rmt5_controller_lowlevel.h"  // V2: Use new worker pool driver
#include "fl/chipsets/led_timing.h"    // Must be before timing_traits.h
#include "fl/chipsets/timing_traits.h"
namespace fl {
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class ClocklessRMT : public CPixelLEDController<RGB_ORDER>
{
private:
    // Extract timing values from timing type's enum members
    static constexpr uint32_t T1 = TIMING::T1;
    static constexpr uint32_t T2 = TIMING::T2;
    static constexpr uint32_t T3 = TIMING::T3;

    // Convert to runtime ChipsetTiming for RMT controller
    static constexpr ChipsetTiming runtime_timing() {
        return {TIMING::T1, TIMING::T2, TIMING::T3, TIMING::RESET, "timing"};
    }

    // -- The actual controller object for ESP32
    fl::RmtController5LowLevel mRMTController;  // V2: New worker pool driver

        // -- Verify that the pin is valid
    static_assert(FastPin<DATA_PIN>::validpin(), "This pin has been marked as an invalid pin, common reasons includes it being a ground pin, read only, or too noisy (e.g. hooked up to the uart).");

public:
    ClocklessRMT(): mRMTController(DATA_PIN, runtime_timing())
    {
    }

    void init() override { }
    virtual uint16_t getMaxRefreshRate() const { return 800; }

protected:


    // Prepares data for the draw.
    virtual void showPixels(PixelController<RGB_ORDER> &pixels) override
    {
        fl::PixelIterator iterator = pixels.as_iterator(this->getRgbw());
        mRMTController.loadPixelData(iterator);
    }

    virtual void endShowLeds(void *data) override
    {
        CPixelLEDController<RGB_ORDER>::endShowLeds(data);
        mRMTController.showPixels();
    }
};
}  // namespace fl