#pragma once

#include "led_strip/enabled.h"

#if FASTLED_RMT5

#include "pixel_iterator.h"
#include <stdint.h>
#include "led_strip/rmt_strip.h"


class RmtController5
{
public:
    // Bridge between FastLED and the ESP RMT5 driver.
    RmtController5() = delete;
    RmtController5(RmtController5 &&) = delete;
    RmtController5 &operator=(const RmtController5 &) = delete;
    RmtController5(const RmtController5 &) = delete;
    RmtController5(
        int DATA_PIN,
        int T1, int T2, int T3);  // FastLED bit timings. See embedded python script in chipsets.h for how to calculate these.

    ~RmtController5();

    void loadPixelData(PixelIterator &pixels);
    void showPixels();
    void waitForDrawComplete();

private:
    int mPin;
    int mT1, mT2, mT3;
    fastled_rmt51_strip::IRmtLedStrip *mLedStrip = nullptr;
};



#endif  // FASTLED_RMT5

