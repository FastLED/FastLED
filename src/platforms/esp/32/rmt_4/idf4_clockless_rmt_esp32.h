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
 *
 * The default FastLED driver takes over control of the RMT interrupt
 * handler, making it hard to use the RMT device for other
 * (non-FastLED) purposes. You can change it's behavior to use the ESP
 * core driver instead, allowing other RMT applications to
 * co-exist. To switch to this mode, add the following directive
 * before you include FastLED.h:
 *
 *      #define FASTLED_RMT_BUILTIN_DRIVER 1
 */

#pragma once



#include "platforms/esp/32/fastpin_esp32.h"

#include "platforms/esp/esp_version.h"
#include "pixel_iterator.h"
#include "idf4_rmt.h"



// -- Core or custom driver
#ifndef FASTLED_RMT_BUILTIN_DRIVER
#define FASTLED_RMT_BUILTIN_DRIVER false
#endif

FASTLED_NAMESPACE_BEGIN

#define FASTLED_HAS_CLOCKLESS 1
// Not used.
//#define NUM_COLOR_CHANNELS 3

// -- Max RMT TX channel
#ifndef FASTLED_RMT_MAX_CHANNELS
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
// 8 for (ESP32)  4 for (ESP32S2, ESP32S3)  2 for (ESP32C3, ESP32H2)
#include "soc/soc_caps.h"
#define FASTLED_RMT_MAX_CHANNELS SOC_RMT_TX_CANDIDATES_PER_GROUP
#else
#ifdef CONFIG_IDF_TARGET_ESP32S2
#define FASTLED_RMT_MAX_CHANNELS 4
#else
#define FASTLED_RMT_MAX_CHANNELS 8
#endif
#endif
#endif

template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ClocklessController : public CPixelLEDController<RGB_ORDER>
{
private:
    // -- The actual controller object for ESP32
    RmtController mRMTController;

    // -- Verify that the pin is valid
    static_assert(FastPin<DATA_PIN>::validpin(), "This pin has been marked as an invalid pin, common reasons includes it being a ground pin, read only, or too noisy (e.g. hooked up to the uart).");

public:
    ClocklessController()
        : mRMTController(
            DATA_PIN, T1, T2, T3,
            FASTLED_RMT_MAX_CHANNELS,
            FASTLED_RMT_BUILTIN_DRIVER
        )
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
        mRMTController.showPixels(iterator);
    }
};

FASTLED_NAMESPACE_END
