#pragma once

// signal to the world that we have a ClocklessController to allow WS2812 and others.
#define FASTLED_HAS_CLOCKLESS 1

#warning "Work in progress: ESP32 ClocklessController for IDF 5.1, this is non functional right now"

#include "crgb.h"
#include "eorder.h"
#include "pixel_iterator.h"
#include "idf5_rmt.h"

// #include "idf4_clockless_rmt_esp32.h"


template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ClocklessController : public CPixelLEDController<RGB_ORDER>
{
private:
    // -- Verify that the pin is valid
    static_assert(FastPin<DATA_PIN>::validpin(), "Invalid pin specified");

public:
    ClocklessController()
    {
    }

    void init()
    {
    }

    virtual uint16_t getMaxRefreshRate() const { return 400; }

protected:
    // -- Show pixels
    //    This is the main entry point for the controller.
    virtual void showPixels(PixelController<RGB_ORDER> &pixels)
    {
        PixelIterator iterator = pixels.as_iterator(this->getRgbw());
        // mRMTController.showPixels(iterator);
    }
};