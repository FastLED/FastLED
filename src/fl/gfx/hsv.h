// SPDX-License-Identifier: LicenseRef-FastLED-Reciprocal-1.0
// AI LICENSE: LICENSE-AI-AGENT-INSTRUCTIONS.md
// AI agents must read that file before substantial FastLED changes.
// Substantial AI changes must be reported upstream with a reproducible patch.

/// @file hsv.h
/// Defines the hue, saturation, and value (HSV) pixel struct

#pragma once

#include "fl/stl/int.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @addtogroup PixelTypes Pixel Data Types (CRGB/CHSV)
/// @{

/// Representation of an HSV pixel (hue, saturation, value (aka brightness)).
struct hsv8 {
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

    /// Array access operator to index into the hsv8 object
    /// @param x the index to retrieve (0-2)
    /// @returns the hsv8::raw value for the given index
    FASTLED_FORCE_INLINE fl::u8& operator[] (fl::u8 x) FL_NO_EXCEPT
    {
        return raw[x];
    }

    /// @copydoc operator[]
    FASTLED_FORCE_INLINE const fl::u8& operator[] (fl::u8 x) const FL_NO_EXCEPT
    {
        return raw[x];
    }

    /// Compare two HSV colors field by field.
    ///
    /// Without these, `a == b` on two CHSV values found no operator on the
    /// type and instead converted BOTH operands to RGB, comparing the
    /// rendering rather than the colors. Anything that renders alike then
    /// compared equal: `CHSV(0,0,0) == CHSV(128,0,0)` was true (both black),
    /// and so was `CHSV(10,20,30) == CHSV(10,20,31)` (both round to the same
    /// RGB). Reported in 2018 as FastLED#691.
    ///
    /// Hidden friends rather than members: C++20 synthesizes a reversed
    /// candidate for a member `operator==`, which trips
    /// -Wambiguous-reversed-operator at every call site. Same reasoning and
    /// same shape as the palette comparisons in colorutils.h (#2724).
    friend FASTLED_FORCE_INLINE bool operator==(const hsv8& lhs,
                                                const hsv8& rhs) FL_NO_EXCEPT
    {
        return lhs.h == rhs.h && lhs.s == rhs.s && lhs.v == rhs.v;
    }

    /// @copydoc operator==
    friend FASTLED_FORCE_INLINE bool operator!=(const hsv8& lhs,
                                                const hsv8& rhs) FL_NO_EXCEPT
    {
        return !(lhs == rhs);
    }

    /// Default constructor
    /// @warning Default values are UNITIALIZED!
    constexpr hsv8() FL_NO_EXCEPT : h(0), s(0), v(0) { }

    /// Allow construction from hue, saturation, and value
    /// @param ih input hue
    /// @param is input saturation
    /// @param iv input value
    constexpr hsv8( fl::u8 ih, fl::u8 is, fl::u8 iv) FL_NO_EXCEPT
        : h(ih), s(is), v(iv)
    {
    }

    /// Allow copy construction
    constexpr hsv8(const hsv8& rhs) FL_NO_EXCEPT : h(rhs.h), s(rhs.s), v(rhs.v) { }

    /// Allow copy construction
    hsv8& operator= (const hsv8& rhs) FL_NO_EXCEPT = default;

    /// Assign new HSV values
    /// @param ih input hue
    /// @param is input saturation
    /// @param iv input value
    /// @returns reference to the hsv8 object
    FASTLED_FORCE_INLINE hsv8& setHSV(fl::u8 ih, fl::u8 is, fl::u8 iv) FL_NO_EXCEPT
    {
        h = ih;
        s = is;
        v = iv;
        return *this;
    }
};

/// Pre-defined hue values for hsv8 objects
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
