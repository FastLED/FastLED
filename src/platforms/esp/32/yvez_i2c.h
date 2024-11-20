#pragma once

#include <stdint.h>

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

// #include "I2SClocklessVirtualLedDriver.h
// #include "pixel_iterator.h" // fastled: src/pixel_iterator.h

class PixelIterator;

class YvezI2C_WS2812 {
  public:
    // Bridge between FastLED and the ESP RMT5 driver.

    YvezI2C_WS2812(int DATA_PIN, int LATCH_PIN);

    ~YvezI2C_WS2812();

    void loadPixelData(PixelIterator &pixels);
    void showPixels();
    void waitForDrawComplete();

    //
    // YvezI2C_WS2812() = delete;
    // YvezI2C_WS2812(YvezI2C_WS2812 &&) = delete;
    // YvezI2C_WS2812 &operator=(const YvezI2C_WS2812 &) = delete;
    // YvezI2C_WS2812(const YvezI2C_WS2812 &) = delete;

  private:
    int mDataPin;
    int mLatchPin;
};

FASTLED_NAMESPACE_END
