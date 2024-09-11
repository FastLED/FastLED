
#pragma once

#include <stdint.h>
#include "rgb_2_rgbw.h"
#include <stddef.h>

// Abstract class
class PixelIterator {
  public:
    virtual bool has(int n) = 0;
    // loadAndScaleRGBW
    virtual void loadAndScaleRGBW(RGBW_MODE rgbw_mode,
                                  uint16_t white_color_temp, uint8_t *b0_out,
                                  uint8_t *b1_out, uint8_t *b2_out,
                                  uint8_t *w_out) = 0;

    // loadAndScaleRGB
    virtual void loadAndScaleRGB(uint8_t *r_out, uint8_t *g_out,
                                 uint8_t *b_out) = 0;

    // stepDithering
    virtual void stepDithering() = 0;

    // advanceData
    virtual void advanceData() = 0;

    // size
    virtual int size() = 0;
};