#pragma once

/*
This are accurate and tested easing functions.

Note that the easing functions in lib8tion.h are tuned are implemented wrong, such as easeInOutCubic8 and easeInOutCubic16.
Modern platforms are so fast that the extra performance is not needed, but accuracy is important.

*/

#include "fl/stdint.h"

namespace fl {

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
uint8_t easeInQuad8(uint8_t i);

/// 8-bit quadratic ease-out function
/// Takes an input value 0-255 and returns an eased value 0-255
/// The curve starts fast and decelerates (ease-out only)
uint8_t easeOutQuad8(uint8_t i);

/// 8-bit quadratic ease-in/ease-out function
/// Takes an input value 0-255 and returns an eased value 0-255
/// The curve starts slow, accelerates in the middle, then slows down again
uint8_t easeInOutQuad8(uint8_t i);

/// 8-bit cubic ease-in function
/// Takes an input value 0-255 and returns an eased value 0-255
/// More pronounced acceleration than quadratic
uint8_t easeInCubic8(uint8_t i);

/// 8-bit cubic ease-out function
/// Takes an input value 0-255 and returns an eased value 0-255
/// More pronounced deceleration than quadratic
uint8_t easeOutCubic8(uint8_t i);

/// 8-bit cubic ease-in/ease-out function
/// Takes an input value 0-255 and returns an eased value 0-255
/// More pronounced easing curve than quadratic
uint8_t easeInOutCubic8(uint8_t i);

/// 8-bit sine ease-in function
/// Takes an input value 0-255 and returns an eased value 0-255
/// Smooth sinusoidal acceleration
uint8_t easeInSine8(uint8_t i);

/// 8-bit sine ease-out function
/// Takes an input value 0-255 and returns an eased value 0-255
/// Smooth sinusoidal deceleration
uint8_t easeOutSine8(uint8_t i);

/// 8-bit sine ease-in/ease-out function
/// Takes an input value 0-255 and returns an eased value 0-255
/// Smooth sinusoidal acceleration and deceleration
uint8_t easeInOutSine8(uint8_t i);


// 16-bit easing functions
/// 16-bit quadratic ease-in function
/// Takes an input value 0-65535 and returns an eased value 0-65535
uint16_t easeInQuad16(uint16_t i);

/// 16-bit quadratic ease-out function
/// Takes an input value 0-65535 and returns an eased value 0-65535
uint16_t easeOutQuad16(uint16_t i);

/// 16-bit quadratic ease-in/ease-out function
/// Takes an input value 0-65535 and returns an eased value 0-65535
uint16_t easeInOutQuad16(uint16_t i);

/// 16-bit cubic ease-in function
/// Takes an input value 0-65535 and returns an eased value 0-65535
uint16_t easeInCubic16(uint16_t i);

/// 16-bit cubic ease-out function
/// Takes an input value 0-65535 and returns an eased value 0-65535
uint16_t easeOutCubic16(uint16_t i);

/// 16-bit cubic ease-in/ease-out function
/// Takes an input value 0-65535 and returns an eased value 0-65535
uint16_t easeInOutCubic16(uint16_t i);

/// 16-bit sine ease-in function
/// Takes an input value 0-65535 and returns an eased value 0-65535
uint16_t easeInSine16(uint16_t i);

/// 16-bit sine ease-out function
/// Takes an input value 0-65535 and returns an eased value 0-65535
uint16_t easeOutSine16(uint16_t i);

/// 16-bit sine ease-in/ease-out function
/// Takes an input value 0-65535 and returns an eased value 0-65535
uint16_t easeInOutSine16(uint16_t i);

uint16_t ease16(EaseType type, uint16_t i);
void ease16(EaseType type, uint16_t* src, uint16_t* dst, uint16_t count);
uint8_t ease8(EaseType type, uint8_t i);
void ease8(EaseType type, uint8_t* src, uint8_t* dst, uint8_t count);


//////// INLINE FUNCTIONS ////////

inline uint16_t ease16(EaseType type, uint16_t i) {
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

inline void ease16(EaseType type, uint16_t* src, uint16_t* dst, uint16_t count) {
    switch (type) {
        case EASE_NONE: return;
        case EASE_IN_QUAD: {
            for (uint16_t i = 0; i < count; i++) {
                dst[i] = easeInQuad16(src[i]);
            }
            break;
        }
        case EASE_OUT_QUAD: {
            for (uint16_t i = 0; i < count; i++) {
                dst[i] = easeOutQuad16(src[i]);
            }
            break;
        }
        case EASE_IN_OUT_QUAD: {
            for (uint16_t i = 0; i < count; i++) {  
                dst[i] = easeInOutQuad16(src[i]);
            }
            break;
        }
        case EASE_IN_CUBIC: {
            for (uint16_t i = 0; i < count; i++) {
                dst[i] = easeInCubic16(src[i]);
            }
            break;
        }
        case EASE_OUT_CUBIC: {
            for (uint16_t i = 0; i < count; i++) {
                dst[i] = easeOutCubic16(src[i]);
            }
            break;
        }
        case EASE_IN_OUT_CUBIC: {
            for (uint16_t i = 0; i < count; i++) {
                dst[i] = easeInOutCubic16(src[i]);
            }
            break;
        }
        case EASE_IN_SINE: {
            for (uint16_t i = 0; i < count; i++) {
                dst[i] = easeInSine16(src[i]);
            }
            break;
        }
        case EASE_OUT_SINE: {
            for (uint16_t i = 0; i < count; i++) {
                dst[i] = easeOutSine16(src[i]);
            }
            break;
        }
        case EASE_IN_OUT_SINE: {
            for (uint16_t i = 0; i < count; i++) {
                dst[i] = easeInOutSine16(src[i]);
            }
            break;
        }
    }
}

inline uint8_t ease8(EaseType type, uint8_t i) {
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

inline void ease8(EaseType type, uint8_t* src, uint8_t* dst, uint8_t count) {
    switch (type) {
        case EASE_NONE: return;
        case EASE_IN_QUAD: {
            for (uint8_t i = 0; i < count; i++) {
                dst[i] = easeInQuad8(src[i]);
            }
            break;
        }
        case EASE_OUT_QUAD: {
            for (uint8_t i = 0; i < count; i++) {
                dst[i] = easeOutQuad8(src[i]);
            }
            break;
        }
        case EASE_IN_OUT_QUAD: {
            for (uint8_t i = 0; i < count; i++) {
                dst[i] = easeInOutQuad8(src[i]);
            }
            break;
        }
        case EASE_IN_CUBIC: {
            for (uint8_t i = 0; i < count; i++) {
                dst[i] = easeInCubic8(src[i]);
            }
            break;
        }
        case EASE_OUT_CUBIC: {
            for (uint8_t i = 0; i < count; i++) {
                dst[i] = easeOutCubic8(src[i]);
            }
            break;
        }
        case EASE_IN_OUT_CUBIC: {
            for (uint8_t i = 0; i < count; i++) {
                dst[i] = easeInOutCubic8(src[i]);
            }
            break;
        }
        case EASE_IN_SINE: {
            for (uint8_t i = 0; i < count; i++) {
                dst[i] = easeInSine8(src[i]);
            }
            break;
        }
        case EASE_OUT_SINE: {
            for (uint8_t i = 0; i < count; i++) {
                dst[i] = easeOutSine8(src[i]);
            }
            break;
        }
        case EASE_IN_OUT_SINE: {
            for (uint8_t i = 0; i < count; i++) {
                dst[i] = easeInOutSine8(src[i]);
            }
            break;
        }
    }
}

} // namespace fl
