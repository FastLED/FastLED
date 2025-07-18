#include "fl/hsv16.h"
#include "fl/math.h"

#include "lib8tion/intmap.h"
#include "fl/ease.h"

namespace fl {

// Improved 8-bit to 16-bit scaling using the same technique as map8_to_16
// but with proper rounding for the 0-255 to 0-65535 conversion
static inline u16 scale8_to_16_accurate(u8 x) {
    if (x == 0) return 0;
    if (x == 255) return 65535;
    // Use 32-bit arithmetic with rounding: (x * 65535 + 127) / 255
    // This is equivalent to: (x * 65535 + 255/2) / 255
    return (u16)(((u32)x * 65535 + 127) / 255);
}

static HSV16 RGBtoHSV16(const CRGB &rgb) {
    // Work with 8-bit values directly
    u8 r = rgb.r;
    u8 g = rgb.g;
    u8 b = rgb.b;

    // Find min and max
    u8 mx = fl_max(r, fl_max(g, b));
    u8 mn = fl_min(r, fl_min(g, b));
    u8 delta = mx - mn;

    u16 h = 0;
    u16 s = 0;
    u16 v = scale8_to_16_accurate(mx);

    // Calculate saturation using improved scaling
    if (mx > 0) {
        // s = (delta * 65535) / mx, but with better accuracy
        // Use the same technique as scale8_to_16_accurate but for arbitrary denominator
        if (delta == mx) {
            s = 65535;  // Saturation is 100%
        } else {
            s = (u16)(((u32)delta * 65535 + (mx >> 1)) / mx);
        }
    }

    // Calculate hue using improved algorithms
    if (delta > 0) {
        u32 hue_calc = 0;
        
        if (mx == r) {
            // Hue in red sector (0-60 degrees)
            if (g >= b) {
                // Use improved division: hue_calc = (g - b) * 65535 / (6 * delta)
                u32 numerator = (u32)(g - b) * 65535;
                if (delta <= 42) {  // 6 * 42 = 252, safe for small delta
                    hue_calc = numerator / (6 * delta);
                } else {
                    hue_calc = numerator / delta / 6;  // Avoid overflow
                }
            } else {
                u32 numerator = (u32)(b - g) * 65535;
                if (delta <= 42) {
                    hue_calc = 65535 - numerator / (6 * delta);
                } else {
                    hue_calc = 65535 - numerator / delta / 6;
                }
            }
        } else if (mx == g) {
            // Hue in green sector (60-180 degrees)
            // Handle signed arithmetic properly to avoid integer underflow
            i32 signed_diff = (i32)b - (i32)r;
            u32 sector_offset = 65535 / 3;  // 60 degrees (120 degrees in 16-bit space)
            
            if (signed_diff >= 0) {
                // Positive case: b >= r
                u32 numerator = (u32)signed_diff * 65535;
                if (delta <= 42) {
                    hue_calc = sector_offset + numerator / (6 * delta);
                } else {
                    hue_calc = sector_offset + numerator / delta / 6;
                }
            } else {
                // Negative case: b < r  
                u32 numerator = (u32)(-signed_diff) * 65535;
                if (delta <= 42) {
                    hue_calc = sector_offset - numerator / (6 * delta);
                } else {
                    hue_calc = sector_offset - numerator / delta / 6;
                }
            }
        } else { // mx == b
            // Hue in blue sector (180-300 degrees)
            // Handle signed arithmetic properly to avoid integer underflow
            i32 signed_diff = (i32)r - (i32)g;
            u32 sector_offset = (2 * 65535) / 3;  // 240 degrees (240 degrees in 16-bit space)
            
            if (signed_diff >= 0) {
                // Positive case: r >= g
                u32 numerator = (u32)signed_diff * 65535;
                if (delta <= 42) {
                    hue_calc = sector_offset + numerator / (6 * delta);
                } else {
                    hue_calc = sector_offset + numerator / delta / 6;
                }
            } else {
                // Negative case: r < g
                u32 numerator = (u32)(-signed_diff) * 65535;
                if (delta <= 42) {
                    hue_calc = sector_offset - numerator / (6 * delta);
                } else {
                    hue_calc = sector_offset - numerator / delta / 6;
                }
            }
        }
        
        h = (u16)(hue_calc & 0xFFFF);
    }

    return HSV16{h, s, v};
}

static CRGB HSV16toRGB(const HSV16& hsv) {
    // Convert 16-bit values to working range
    u32 h = hsv.h;
    u32 s = hsv.s; 
    u32 v = hsv.v;

    if (s == 0) {
        // Grayscale case - use precise mapping
        u8 gray = map16_to_8(v);
        return CRGB{gray, gray, gray};
    }

    // Determine which sector of the color wheel (0-5)
    u32 sector = (h * 6) / 65536;
    u32 sector_pos = (h * 6) % 65536; // Position within sector (0-65535)

    // Calculate intermediate values using precise mapping
    // c = v * s / 65536, with proper rounding
    u32 c = map32_to_16(v * s);
    
    // Calculate x = c * (1 - |2*(sector_pos/65536) - 1|)
    u32 x;
    if (sector & 1) {
        // For odd sectors (1, 3, 5), we want decreasing values
        // x = c * (65535 - sector_pos) / 65535
        x = map32_to_16(c * (65535 - sector_pos));
    } else {
        // For even sectors (0, 2, 4), we want increasing values  
        // x = c * sector_pos / 65535
        x = map32_to_16(c * sector_pos);
    }
    
    u32 m = v - c;

    u32 r1, g1, b1;
    switch (sector) {
        case 0: r1 = c; g1 = x; b1 = 0; break;
        case 1: r1 = x; g1 = c; b1 = 0; break;
        case 2: r1 = 0; g1 = c; b1 = x; break;
        case 3: r1 = 0; g1 = x; b1 = c; break;
        case 4: r1 = x; g1 = 0; b1 = c; break;
        default: r1 = c; g1 = 0; b1 = x; break;
    }

    // Add baseline and scale to 8-bit using accurate mapping
    u8 R = map16_to_8(u16(r1 + m));
    u8 G = map16_to_8(u16(g1 + m));
    u8 B = map16_to_8(u16(b1 + m));

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
        u16 inv_sat = 65535 - hsv.s;
        inv_sat = ease16(saturation_function, inv_sat);
        hsv.s = (65535 - inv_sat);
    }
    
    if (luminance_function != EASE_NONE) {
        hsv.v = ease16(luminance_function, hsv.v);
    }
    
    return hsv.ToRGB();
}

} // namespace fl
