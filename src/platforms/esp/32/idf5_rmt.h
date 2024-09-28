#pragma once

#include "led_strip/enabled.h"

#if FASTLED_ESP32_COMPONENT_LED_STRIP_BUILT_IN

#include "pixel_iterator.h"
#include <stdint.h>

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

    void showPixels(PixelIterator &pixels);

private:
    void ingest(uint8_t val);
    void showPixels();
    bool built_in_driver();
    uint8_t *getPixelBuffer(int size_in_bytes);
    void loadPixelDataForStreamEncoding(PixelIterator& pixels);
};



#endif  // FASTLED_ESP32_COMPONENT_LED_STRIP_BUILT_IN

