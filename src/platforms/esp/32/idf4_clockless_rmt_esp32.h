/*
 * Integration into FastLED ClocklessController
 * Copyright (c) 2024, Zach Vorhies
 * Copyright (c) 2018,2019,2020 Samuel Z. Guyer
 * Copyright (c) 2017 Thomas Basler
 * Copyright (c) 2017 Martin F. Falatic
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once


#include "idf4_rmt.h"
#include "FastLED.h"


FASTLED_NAMESPACE_BEGIN


#define FASTLED_HAS_CLOCKLESS 1
#define NUM_COLOR_CHANNELS 3


template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ClocklessController : public CPixelLEDController<RGB_ORDER>
{
private:

    // -- The actual controller object for ESP32
    ESP32RMTController mRMTController;

    // -- Verify that the pin is valid
    static_assert(FastPin<DATA_PIN>::validpin(), "Invalid pin specified");

public:

    ClocklessController()
        : mRMTController(DATA_PIN, T1, T2, T3, FASTLED_RMT_MAX_CHANNELS, FASTLED_RMT_BUILTIN_DRIVER)
        {}

    void init()
    {
    }

    virtual uint16_t getMaxRefreshRate() const { return 400; }

protected:
    // -- Show pixels
    //    This is the main entry point for the controller.
    virtual void showPixels(PixelController<RGB_ORDER> & pixels)
    {
        if (FASTLED_RMT_BUILTIN_DRIVER) {
            mRMTController.loadAllPixelsToRmtSymbolData(pixels);
        } else {
            mRMTController.loadPixelDataForStreamEncoding(pixels);
        }
        mRMTController.showPixels();
    }
};


FASTLED_NAMESPACE_END
