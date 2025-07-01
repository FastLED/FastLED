/// @file hsv.h
/// Defines the hue, saturation, and value (HSV) pixel struct

#pragma once

#include "fl/stdint.h"
#include "fl/int.h"

namespace fl {

/// @addtogroup PixelTypes Pixel Data Types (CRGB/CHSV)
/// @{

/// Representation of an HSV pixel (hue, saturation, value (aka brightness)).
struct CHSV {
    union {
        struct {
            union {
                /// Color hue.
                /// This is an 8-bit value representing an angle around
                /// the color wheel. Where 0 is 0°, and 255 is 358°.
                fl::u8 hue;
                fl::u8 h;  ///< @copydoc hue
            };
            union {
                /// Color saturation.
                /// This is an 8-bit value representing a percentage.
                fl::u8 saturation;
                fl::u8 sat;  ///< @copydoc saturation
                fl::u8 s;    ///< @copydoc saturation
            };
            union {
                /// Color value (brightness).
                /// This is an 8-bit value representing a percentage.
                fl::u8 value;
                fl::u8 val;  ///< @copydoc value
                fl::u8 v;    ///< @copydoc value
            };
        };
        /// Access the hue, saturation, and value data as an array.
        /// Where:
        /// * `raw[0]` is the hue
        /// * `raw[1]` is the saturation
        /// * `raw[2]` is the value
        fl::u8 raw[3];
    };

    /// Array access operator to index into the CHSV object
    /// @param x the index to retrieve (0-2)
    /// @returns the CHSV::raw value for the given index
    inline fl::u8& operator[] (fl::u8 x) __attribute__((always_inline))
    {
        return raw[x];
    }

    /// @copydoc operator[]
    inline const fl::u8& operator[] (fl::u8 x) const __attribute__((always_inline))
    {
        return raw[x];
    }

    /// Default constructor
    /// @warning Default values are UNITIALIZED!
    constexpr inline CHSV() __attribute__((always_inline)): h(0), s(0), v(0) { }

    /// Allow construction from hue, saturation, and value
    /// @param ih input hue
    /// @param is input saturation
    /// @param iv input value
    constexpr inline CHSV( fl::u8 ih, fl::u8 is, fl::u8 iv) __attribute__((always_inline))
        : h(ih), s(is), v(iv)
    {
    }

    /// Allow copy construction
    constexpr inline CHSV(const CHSV& rhs) noexcept : h(rhs.h), s(rhs.s), v(rhs.v) { }

    /// Allow copy construction
    inline CHSV& operator= (const CHSV& rhs) __attribute__((always_inline)) = default;

    /// Assign new HSV values
    /// @param ih input hue
    /// @param is input saturation
    /// @param iv input value
    /// @returns reference to the CHSV object
    inline CHSV& setHSV(fl::u8 ih, fl::u8 is, fl::u8 iv) __attribute__((always_inline))
    {
        h = ih;
        s = is;
        v = iv;
        return *this;
    }
};

/// Pre-defined hue values for CHSV objects
typedef enum {
    HUE_RED = 0,       ///< Red (0°)
    HUE_ORANGE = 32,   ///< Orange (45°)
    HUE_YELLOW = 64,   ///< Yellow (90°)
    HUE_GREEN = 96,    ///< Green (135°)
    HUE_AQUA = 128,    ///< Aqua (180°)
    HUE_BLUE = 160,    ///< Blue (225°)
    HUE_PURPLE = 192,  ///< Purple (270°)
    HUE_PINK = 224     ///< Pink (315°)
} HSVHue;

/// @} PixelTypes

}  // namespace fl
