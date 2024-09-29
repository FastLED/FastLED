#pragma once

#include "led_strip/enabled.h"

#if FASTLED_RMT5

#include "pixel_iterator.h"
#include <stdint.h>
#include "led_strip/rmt_strip.h"


class RmtController5
{
public:
    // When built_in_driver is true then the entire RMT datablock is
    // generated ahead of time. This eliminates the flicker at the
    // cost of using WAY more memory (24x the size of the pixel data
    // as opposed to 2x - the size of an additional pixel buffer for
    // stream encoding).
    RmtController5() = delete;
    RmtController5(RmtController5 &&) = delete;
    RmtController5 &operator=(const RmtController5 &) = delete;
    RmtController5(const RmtController5 &) = delete;
    RmtController5(
        int DATA_PIN,
        int T1, int T2, int T3);  // Bit timings.

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

