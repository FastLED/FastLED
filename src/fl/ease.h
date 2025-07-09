#pragma once

/*
This are accurate and tested easing functions.

Note that the easing functions in lib8tion.h are tuned are implemented wrong, such as easeInOutCubic8 and easeInOutCubic16.
Modern platforms are so fast that the extra performance is not needed, but accuracy is important.

*/

#include "fl/stdint.h"
#include "fl/int.h"
#include "fastled_progmem.h"

namespace fl {

// Gamma 2.8 lookup table for 8-bit to 16-bit gamma correction 
// Used for converting linear 8-bit values to gamma-corrected 16-bit values
extern const u16 gamma_2_8[256] FL_PROGMEM;

enum EaseType {
    EASE_NONE,
    EASE_IN_QUAD,
    EASE_OUT_QUAD,
    EASE_IN_OUT_QUAD,
    EASE_IN_CUBIC,
    EASE_OUT_CUBIC,
    EASE_IN_OUT_CUBIC,
    EASE_IN_SINE,
    EASE_OUT_SINE,
    EASE_IN_OUT_SINE,
};

// 8-bit easing functions
/// 8-bit quadratic ease-in function
/// Takes an input value 0-255 and returns an eased value 0-255
/// The curve starts slow and accelerates (ease-in only)
u8 easeInQuad8(u8 i);

/// 8-bit quadratic ease-out function
/// Takes an input value 0-255 and returns an eased value 0-255
/// The curve starts fast and decelerates (ease-out only)
u8 easeOutQuad8(u8 i);

/// 8-bit quadratic ease-in/ease-out function
/// Takes an input value 0-255 and returns an eased value 0-255
/// The curve starts slow, accelerates in the middle, then slows down again
u8 easeInOutQuad8(u8 i);

/// 8-bit cubic ease-in function
/// Takes an input value 0-255 and returns an eased value 0-255
/// More pronounced acceleration than quadratic
u8 easeInCubic8(u8 i);

/// 8-bit cubic ease-out function
/// Takes an input value 0-255 and returns an eased value 0-255
/// More pronounced deceleration than quadratic
u8 easeOutCubic8(u8 i);

/// 8-bit cubic ease-in/ease-out function
/// Takes an input value 0-255 and returns an eased value 0-255
/// More pronounced easing curve than quadratic
u8 easeInOutCubic8(u8 i);

/// 8-bit sine ease-in function
/// Takes an input value 0-255 and returns an eased value 0-255
/// Smooth sinusoidal acceleration
u8 easeInSine8(u8 i);

/// 8-bit sine ease-out function
/// Takes an input value 0-255 and returns an eased value 0-255
/// Smooth sinusoidal deceleration
u8 easeOutSine8(u8 i);

/// 8-bit sine ease-in/ease-out function
/// Takes an input value 0-255 and returns an eased value 0-255
/// Smooth sinusoidal acceleration and deceleration
u8 easeInOutSine8(u8 i);


// 16-bit easing functions
/// 16-bit quadratic ease-in function
/// Takes an input value 0-65535 and returns an eased value 0-65535
u16 easeInQuad16(u16 i);

/// 16-bit quadratic ease-out function
/// Takes an input value 0-65535 and returns an eased value 0-65535
u16 easeOutQuad16(u16 i);

/// 16-bit quadratic ease-in/ease-out function
/// Takes an input value 0-65535 and returns an eased value 0-65535
u16 easeInOutQuad16(u16 i);

/// 16-bit cubic ease-in function
/// Takes an input value 0-65535 and returns an eased value 0-65535
u16 easeInCubic16(u16 i);

/// 16-bit cubic ease-out function
/// Takes an input value 0-65535 and returns an eased value 0-65535
u16 easeOutCubic16(u16 i);

/// 16-bit cubic ease-in/ease-out function
/// Takes an input value 0-65535 and returns an eased value 0-65535
u16 easeInOutCubic16(u16 i);

/// 16-bit sine ease-in function
/// Takes an input value 0-65535 and returns an eased value 0-65535
u16 easeInSine16(u16 i);

/// 16-bit sine ease-out function
/// Takes an input value 0-65535 and returns an eased value 0-65535
u16 easeOutSine16(u16 i);

/// 16-bit sine ease-in/ease-out function
/// Takes an input value 0-65535 and returns an eased value 0-65535
u16 easeInOutSine16(u16 i);

u16 ease16(EaseType type, u16 i);
void ease16(EaseType type, u16* src, u16* dst, u16 count);
u8 ease8(EaseType type, u8 i);
void ease8(EaseType type, u8* src, u8* dst, u8 count);


//////// INLINE FUNCTIONS ////////

inline u16 ease16(EaseType type, u16 i) {
    switch (type) {
        case EASE_NONE: return i;
        case EASE_IN_QUAD: return easeInQuad16(i);
        case EASE_OUT_QUAD: return easeOutQuad16(i);
        case EASE_IN_OUT_QUAD: return easeInOutQuad16(i);
        case EASE_IN_CUBIC: return easeInCubic16(i);
        case EASE_OUT_CUBIC: return easeOutCubic16(i);
        case EASE_IN_OUT_CUBIC: return easeInOutCubic16(i);
        case EASE_IN_SINE: return easeInSine16(i);
        case EASE_OUT_SINE: return easeOutSine16(i);
        case EASE_IN_OUT_SINE: return easeInOutSine16(i);
        default: return i;
    }
}

inline void ease16(EaseType type, u16* src, u16* dst, u16 count) {
    switch (type) {
        case EASE_NONE: return;
        case EASE_IN_QUAD: {
            for (u16 i = 0; i < count; i++) {
                dst[i] = easeInQuad16(src[i]);
            }
            break;
        }
        case EASE_OUT_QUAD: {
            for (u16 i = 0; i < count; i++) {
                dst[i] = easeOutQuad16(src[i]);
            }
            break;
        }
        case EASE_IN_OUT_QUAD: {
            for (u16 i = 0; i < count; i++) {  
                dst[i] = easeInOutQuad16(src[i]);
            }
            break;
        }
        case EASE_IN_CUBIC: {
            for (u16 i = 0; i < count; i++) {
                dst[i] = easeInCubic16(src[i]);
            }
            break;
        }
        case EASE_OUT_CUBIC: {
            for (u16 i = 0; i < count; i++) {
                dst[i] = easeOutCubic16(src[i]);
            }
            break;
        }
        case EASE_IN_OUT_CUBIC: {
            for (u16 i = 0; i < count; i++) {
                dst[i] = easeInOutCubic16(src[i]);
            }
            break;
        }
        case EASE_IN_SINE: {
            for (u16 i = 0; i < count; i++) {
                dst[i] = easeInSine16(src[i]);
            }
            break;
        }
        case EASE_OUT_SINE: {
            for (u16 i = 0; i < count; i++) {
                dst[i] = easeOutSine16(src[i]);
            }
            break;
        }
        case EASE_IN_OUT_SINE: {
            for (u16 i = 0; i < count; i++) {
                dst[i] = easeInOutSine16(src[i]);
            }
            break;
        }
    }
}

inline u8 ease8(EaseType type, u8 i) {
    switch (type) {
        case EASE_NONE: return i;
        case EASE_IN_QUAD: return easeInQuad8(i);
        case EASE_OUT_QUAD: return easeOutQuad8(i);
        case EASE_IN_OUT_QUAD: return easeInOutQuad8(i);
        case EASE_IN_CUBIC: return easeInCubic8(i);
        case EASE_OUT_CUBIC: return easeOutCubic8(i);
        case EASE_IN_OUT_CUBIC: return easeInOutCubic8(i);
        case EASE_IN_SINE: return easeInSine8(i);
        case EASE_OUT_SINE: return easeOutSine8(i);
        case EASE_IN_OUT_SINE: return easeInOutSine8(i);
        default: return i;
    }
}

inline void ease8(EaseType type, u8* src, u8* dst, u8 count) {
    switch (type) {
        case EASE_NONE: return;
        case EASE_IN_QUAD: {
            for (u8 i = 0; i < count; i++) {
                dst[i] = easeInQuad8(src[i]);
            }
            break;
        }
        case EASE_OUT_QUAD: {
            for (u8 i = 0; i < count; i++) {
                dst[i] = easeOutQuad8(src[i]);
            }
            break;
        }
        case EASE_IN_OUT_QUAD: {
            for (u8 i = 0; i < count; i++) {
                dst[i] = easeInOutQuad8(src[i]);
            }
            break;
        }
        case EASE_IN_CUBIC: {
            for (u8 i = 0; i < count; i++) {
                dst[i] = easeInCubic8(src[i]);
            }
            break;
        }
        case EASE_OUT_CUBIC: {
            for (u8 i = 0; i < count; i++) {
                dst[i] = easeOutCubic8(src[i]);
            }
            break;
        }
        case EASE_IN_OUT_CUBIC: {
            for (u8 i = 0; i < count; i++) {
                dst[i] = easeInOutCubic8(src[i]);
            }
            break;
        }
        case EASE_IN_SINE: {
            for (u8 i = 0; i < count; i++) {
                dst[i] = easeInSine8(src[i]);
            }
            break;
        }
        case EASE_OUT_SINE: {
            for (u8 i = 0; i < count; i++) {
                dst[i] = easeOutSine8(src[i]);
            }
            break;
        }
        case EASE_IN_OUT_SINE: {
            for (u8 i = 0; i < count; i++) {
                dst[i] = easeInOutSine8(src[i]);
            }
            break;
        }
    }
}

} // namespace fl
