
#pragma once

#include "namespace.h"
#include "rgb_2_rgbw.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

FASTLED_NAMESPACE_BEGIN

struct RgbwArg {
    explicit RgbwArg(uint16_t white_color_temp = kRGBWDefaultColorTemp,
                      RGBW_MODE rgbw_mode = kRGBWExactColors)
        : white_color_temp(white_color_temp), rgbw_mode(rgbw_mode) {}
    uint16_t white_color_temp = kRGBWDefaultColorTemp;
    RGBW_MODE rgbw_mode = kRGBWExactColors;
};

struct RgbwArgInvalid : public RgbwArg {
    RgbwArgInvalid() {
        white_color_temp = kRGBWDefaultColorTemp;
        rgbw_mode = kRGBWInvalid;
    }
    static RgbwArg value() {
        RgbwArgInvalid invalid;
        return invalid;
    }
};

struct RgbwDefault : public RgbwArg {
    RgbwDefault() {
        white_color_temp = kRGBWDefaultColorTemp;
        rgbw_mode = kRGBWExactColors;
    }
    static RgbwArg value() {
        RgbwDefault _default;
        return _default;
    }
};

// Abstract class
class PixelIterator {
  public:
    virtual ~PixelIterator() {}
    explicit PixelIterator(RgbwArg rgbw_arg = RgbwArgInvalid::value()) : mRgbw(rgbw_arg) {}
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

    void set_rgbw(RgbwArg rgbw) { mRgbw = rgbw; }
    RgbwArg get_rgbw() const { return mRgbw; }

  protected:
    RgbwArg mRgbw;
};

FASTLED_NAMESPACE_END
