#include <stdint.h>
#include "rgb_2_rgbw.h"
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

namespace {
    uint8_t min3(uint8_t a, uint8_t b, uint8_t c) {
        if (a < b) {
            if (a < c) {
                return a;
            } else {
                return c;
            }
        } else {
            if (b < c) {
                return b;
            } else {
                return c;
            }
        }
    }
}



void rgb_2_rgbw(uint8_t r, uint8_t g, uint8_t b, uint16_t color_temperature,
                uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_w) {
    uint8_t min_component = min3(r, g, b);
    *out_r = r - min_component;
    *out_g = g - min_component;
    *out_b = b - min_component;
    *out_w = min_component;
}

FASTLED_NAMESPACE_END
