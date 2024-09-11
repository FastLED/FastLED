
#pragma once

#include "rgb_2_rgbw.h"
#include <stddef.h>
#include <stdint.h>

struct RgbwArg {
    uint16_t white_color_temp;
    RGBW_MODE rgbw_mode;
};

struct RgbwDefault: public RgbwArg {
    RgbwDefault() {
        white_color_temp = kRGBWDefaultColorTemp;
        rgbw_mode = kRGBWInvalid;
    }
};

// Abstract class
class PixelIterator {
  public:
    explicit PixelIterator(RgbwArg rgbw_arg = RgbwDefault()) : mRgbw(rgbw_arg) {}
    virtual bool has(int n) = 0;
    // loadAndScaleRGBW
    virtual void loadAndScaleRGBW(uint8_t *b0_out,
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

    bool uses_rgbw() { return mRgbw.rgbw_mode != kRGBWInvalid; }

    void set_rgbw(RgbwArg rgbw) { mRgbw = rgbw; }
    RgbwArg get_rgbw() const { return mRgbw; }

  protected:
    RgbwArg mRgbw;
};