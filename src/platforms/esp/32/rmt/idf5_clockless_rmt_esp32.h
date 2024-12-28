#pragma once

// signal to the world that we have a ClocklessController to allow WS2812 and others.
#define FASTLED_HAS_CLOCKLESS 1

#include "crgb.h"
#include "eorder.h"
#include "pixel_iterator.h"
#include "idf5_rmt.h"

// It turns out that RMT5 recycling causes a lot of problems with
// the first led. A bug has been filed with Espressif about this.
// Therefore we use the alternative implementation that does not
// reycle. To get the old behavior, set FASTLED_RMT5_RECYCLE to 1.
// If you enable this then it will allow more strips to be processed
// than RMT channels, however, you will get a staggered affect as some
// strips will start drawing only after others have finished.
#ifndef FASTLED_RMT5_RECYCLE
#define FASTLED_RMT5_RECYCLE 0
#endif

template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ClocklessController : public CPixelLEDController<RGB_ORDER>
{
private:
    // -- Verify that the pin is valid
    static_assert(FastPin<DATA_PIN>::validpin(), "Invalid pin specified");

    // -- The actual controller object for ESP32
    RmtController5 mRMTController;

public:
    ClocklessController()
        : mRMTController(DATA_PIN, T1, T2, T3, FASTLED_RMT5_RECYCLE)
    {
    }

    void init() override { }
    virtual uint16_t getMaxRefreshRate() const { return 800; }

protected:

    // Wait until the last draw is complete, if necessary.
    virtual void* beginShowLeds(int nleds) override {
        void* data = CPixelLEDController<RGB_ORDER>::beginShowLeds(nleds);
        mRMTController.waitForDrawComplete();
        return data;
    }

    // Prepares data for the draw.
    virtual void showPixels(PixelController<RGB_ORDER> &pixels) override
    {
        PixelIterator iterator = pixels.as_iterator(this->getRgbw());
        mRMTController.loadPixelData(iterator);
    }

    // Send the data to the strip
    virtual void endShowLeds(void* data) override {
        CPixelLEDController<RGB_ORDER>::endShowLeds(data);
        mRMTController.showPixels();
    }
};