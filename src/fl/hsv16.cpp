#include "fl/hsv16.h"
#include "fl/math.h"

#include "lib8tion/intmap.h"
#include "fl/ease.h"

namespace fl {

// Improved 8-bit to 16-bit scaling using the same technique as map8_to_16
// but with proper rounding for the 0-255 to 0-65535 conversion
static inline uint16_t scale8_to_16_accurate(uint8_t x) {
    if (x == 0) return 0;
    if (x == 255) return 65535;
    // Use 32-bit arithmetic with rounding: (x * 65535 + 127) / 255
    // This is equivalent to: (x * 65535 + 255/2) / 255
    return (uint16_t)(((uint32_t)x * 65535 + 127) / 255);
}

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
    uint16_t v = scale8_to_16_accurate(mx);

    // Calculate saturation using improved scaling
    if (mx > 0) {
        // s = (delta * 65535) / mx, but with better accuracy
        // Use the same technique as scale8_to_16_accurate but for arbitrary denominator
        if (delta == mx) {
            s = 65535;  // Saturation is 100%
        } else {
            s = (uint16_t)(((uint32_t)delta * 65535 + (mx >> 1)) / mx);
        }
    }

    // Calculate hue using improved algorithms
    if (delta > 0) {
        uint32_t hue_calc = 0;
        
        if (mx == r) {
            // Hue in red sector (0-60 degrees)
            if (g >= b) {
                // Use improved division: hue_calc = (g - b) * 65535 / (6 * delta)
                uint32_t numerator = (uint32_t)(g - b) * 65535;
                if (delta <= 42) {  // 6 * 42 = 252, safe for small delta
                    hue_calc = numerator / (6 * delta);
                } else {
                    hue_calc = numerator / delta / 6;  // Avoid overflow
                }
            } else {
                uint32_t numerator = (uint32_t)(b - g) * 65535;
                if (delta <= 42) {
                    hue_calc = 65535 - numerator / (6 * delta);
                } else {
                    hue_calc = 65535 - numerator / delta / 6;
                }
            }
        } else if (mx == g) {
            // Hue in green sector (60-180 degrees)
            // Handle signed arithmetic properly to avoid integer underflow
            int32_t signed_diff = (int32_t)b - (int32_t)r;
            uint32_t sector_offset = 65535 / 3;  // 60 degrees (120 degrees in 16-bit space)
            
            if (signed_diff >= 0) {
                // Positive case: b >= r
                uint32_t numerator = (uint32_t)signed_diff * 65535;
                if (delta <= 42) {
                    hue_calc = sector_offset + numerator / (6 * delta);
                } else {
                    hue_calc = sector_offset + numerator / delta / 6;
                }
            } else {
                // Negative case: b < r  
                uint32_t numerator = (uint32_t)(-signed_diff) * 65535;
                if (delta <= 42) {
                    hue_calc = sector_offset - numerator / (6 * delta);
                } else {
                    hue_calc = sector_offset - numerator / delta / 6;
                }
            }
        } else { // mx == b
            // Hue in blue sector (180-300 degrees)
            // Handle signed arithmetic properly to avoid integer underflow
            int32_t signed_diff = (int32_t)r - (int32_t)g;
            uint32_t sector_offset = (2 * 65535) / 3;  // 240 degrees (240 degrees in 16-bit space)
            
            if (signed_diff >= 0) {
                // Positive case: r >= g
                uint32_t numerator = (uint32_t)signed_diff * 65535;
                if (delta <= 42) {
                    hue_calc = sector_offset + numerator / (6 * delta);
                } else {
                    hue_calc = sector_offset + numerator / delta / 6;
                }
            } else {
                // Negative case: r < g
                uint32_t numerator = (uint32_t)(-signed_diff) * 65535;
                if (delta <= 42) {
                    hue_calc = sector_offset - numerator / (6 * delta);
                } else {
                    hue_calc = sector_offset - numerator / delta / 6;
                }
            }
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
        // Grayscale case - use precise mapping
        uint8_t gray = map16_to_8(v);
        return CRGB{gray, gray, gray};
    }

    // Determine which sector of the color wheel (0-5)
    uint32_t sector = (h * 6) / 65536;
    uint32_t sector_pos = (h * 6) % 65536; // Position within sector (0-65535)

    // Calculate intermediate values using precise mapping
    // c = v * s / 65536, with proper rounding
    uint32_t c = map32_to_16(v * s);
    
    // Calculate x = c * (1 - |2*(sector_pos/65536) - 1|)
    uint32_t x;
    if (sector & 1) {
        // For odd sectors (1, 3, 5), we want decreasing values
        // x = c * (65535 - sector_pos) / 65535
        x = map32_to_16(c * (65535 - sector_pos));
    } else {
        // For even sectors (0, 2, 4), we want increasing values  
        // x = c * sector_pos / 65535
        x = map32_to_16(c * sector_pos);
    }
    
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

    // Add baseline and scale to 8-bit using accurate mapping
    uint8_t R = map16_to_8(uint16_t(r1 + m));
    uint8_t G = map16_to_8(uint16_t(g1 + m));
    uint8_t B = map16_to_8(uint16_t(b1 + m));

    return CRGB{R, G, B};
}

HSV16::HSV16(const CRGB& rgb) {
    *this = RGBtoHSV16(rgb);
}

CRGB HSV16::ToRGB() const {
    return HSV16toRGB(*this);
}

CRGB HSV16::colorBoost(EaseType saturation_function, EaseType luminance_function) const {
    HSV16 hsv = *this;
    
    if (saturation_function != EASE_NONE) {
        uint16_t inv_sat = 65535 - hsv.s;
        inv_sat = ease16(saturation_function, inv_sat);
        hsv.s = (65535 - inv_sat);
    }
    
    if (luminance_function != EASE_NONE) {
        hsv.v = ease16(luminance_function, hsv.v);
    }
    
    return hsv.ToRGB();
}

} // namespace fl
