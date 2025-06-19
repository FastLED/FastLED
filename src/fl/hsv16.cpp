#include "fl/hsv16.h"
#include "fl/math.h"

namespace fl {

static HSV16 RGBtoHSV16(const CRGB &rgb) {
    // Work with 8-bit values directly
    uint8_t r = rgb.r;
    uint8_t g = rgb.g;
    uint8_t b = rgb.b;

    // Find min and max
    uint8_t mx = fl_max(r, fl_max(g, b));
    uint8_t mn = fl_min(r, fl_min(g, b));
    uint8_t delta = mx - mn;

    uint16_t h = 0;
    uint16_t s = 0;
    uint16_t v = ((uint32_t)mx * 65535) / 255; // Scale to 16-bit

    // Calculate saturation
    if (mx > 0) {
        s = ((uint32_t)delta * 65535) / mx;
    }

    // Calculate hue
    if (delta > 0) {
        uint32_t hue_calc = 0;
        
        if (mx == r) {
            // Hue in red sector (0-60 degrees)
            if (g >= b) {
                hue_calc = ((uint32_t)(g - b) * 65535) / (6 * delta);
            } else {
                hue_calc = 65535 - ((uint32_t)(b - g) * 65535) / (6 * delta);
            }
        } else if (mx == g) {
            // Hue in green sector (60-180 degrees)
            hue_calc = (65535 / 3) + ((uint32_t)(b - r) * 65535) / (6 * delta);
        } else { // mx == b
            // Hue in blue sector (180-300 degrees)
            hue_calc = (2 * 65535 / 3) + ((uint32_t)(r - g) * 65535) / (6 * delta);
        }
        
        h = (uint16_t)(hue_calc & 0xFFFF);
    }

    return HSV16{h, s, v};
}

static CRGB HSV16toRGB(const HSV16& hsv) {
    // Convert 16-bit values to working range
    uint32_t h = hsv.h;
    uint32_t s = hsv.s; 
    uint32_t v = hsv.v;

    if (s == 0) {
        // Grayscale case
        uint8_t gray = (v * 255) / 65535;
        return CRGB{gray, gray, gray};
    }

    // Determine which sector of the color wheel (0-5)
    uint32_t sector = (h * 6) / 65536;
    uint32_t sector_pos = (h * 6) % 65536; // Position within sector (0-65535)

    // Calculate intermediate values using fixed-point arithmetic
    uint32_t c = (v * s) / 65535;           // Chroma
    
    // Calculate x = c * (1 - |((sector_pos / 32768) % 2) - 1|)
    uint32_t normalized_pos = sector_pos / 1024; // Scale to 0-63 range
    uint32_t triangle_wave = (normalized_pos <= 32) ? normalized_pos : (64 - normalized_pos);
    uint32_t x = (c * triangle_wave * 2048) / 65535; // Scale back up
    
    uint32_t m = v - c;

    uint32_t r1, g1, b1;
    switch (sector) {
        case 0: r1 = c; g1 = x; b1 = 0; break;
        case 1: r1 = x; g1 = c; b1 = 0; break;
        case 2: r1 = 0; g1 = c; b1 = x; break;
        case 3: r1 = 0; g1 = x; b1 = c; break;
        case 4: r1 = x; g1 = 0; b1 = c; break;
        default: r1 = c; g1 = 0; b1 = x; break;
    }

    // Add baseline and scale to 8-bit
    uint8_t R = ((r1 + m) * 255) / 65535;
    uint8_t G = ((g1 + m) * 255) / 65535;
    uint8_t B = ((b1 + m) * 255) / 65535;

    return CRGB{R, G, B};
}

HSV16::HSV16(const CRGB& rgb) {
    *this = RGBtoHSV16(rgb);
}

CRGB HSV16::ToRGB() const {
    return HSV16toRGB(*this);
}

} // namespace fl
