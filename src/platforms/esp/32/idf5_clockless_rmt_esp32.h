#pragma once

// signal to the world that we have a ClocklessController to allow WS2812 and others.
#define FASTLED_HAS_CLOCKLESS 1

#include "crgb.h"
#include "eorder.h"
#include "pixel_iterator.h"
#include "idf5_rmt.h"

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
        : mRMTController(DATA_PIN, T1, T2, T3)
    {
    }

    void init() override { }
    virtual uint16_t getMaxRefreshRate() const { return 400; }

protected:

    // Wait until the last draw is complete, if necessary.
    virtual void* beginShowLeds() override {
        void* data = CPixelLEDController<RGB_ORDER>::beginShowLeds();
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