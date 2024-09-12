
#pragma once

#include "namespace.h"
#include "rgb_2_rgbw.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

FASTLED_NAMESPACE_BEGIN

// Abstract class
class PixelIterator {
  public:
    virtual ~PixelIterator() {}
    explicit PixelIterator(Rgbw rgbw_arg = RgbwInvalid::value()) : mRgbw(rgbw_arg) {}
    virtual bool has(int n) = 0;
    // loadAndScaleRGBW
    virtual void loadAndScaleRGBW(uint8_t *b0_out, uint8_t *b1_out,
                                  uint8_t *b2_out, uint8_t *w_out) = 0;

    // loadAndScaleRGB
    virtual void loadAndScaleRGB(uint8_t *r_out, uint8_t *g_out,
                                 uint8_t *b_out) = 0;

    virtual void loadAndScale_APA102_HD(
        uint8_t *b0_out, uint8_t *b1_out,
        uint8_t *b2_out, // Output RGB values in order of RGB_ORDER
        uint8_t *brightness_out) = 0;

    // stepDithering
    virtual void stepDithering() = 0;

    // advanceData
    virtual void advanceData() = 0;

    // size
    virtual int size() = 0;

    void set_rgbw(Rgbw rgbw) { mRgbw = rgbw; }
    Rgbw get_rgbw() const { return mRgbw; }

  protected:
    Rgbw mRgbw;
};

FASTLED_NAMESPACE_END
