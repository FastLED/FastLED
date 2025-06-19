#pragma once

#include "fl/stdint.h"

namespace fl {

enum EaseType {
    EASE_IN_QUAD,
    EASE_IN_OUT_QUAD,
    EASE_IN_OUT_CUBIC,
};

// 8-bit easing functions
/// 8-bit quadratic ease-in function
/// Takes an input value 0-255 and returns an eased value 0-255
/// The curve starts slow and accelerates (ease-in only)
uint8_t easeInQuad8(uint8_t i);

/// 8-bit quadratic ease-in/ease-out function
/// Takes an input value 0-255 and returns an eased value 0-255
/// The curve starts slow, accelerates in the middle, then slows down again
uint8_t easeInOutQuad8(uint8_t i);

/// 8-bit cubic ease-in/ease-out function
/// Takes an input value 0-255 and returns an eased value 0-255
/// More pronounced easing curve than quadratic
uint8_t easeInOutCubic8(uint8_t i);

/// Fast 8-bit ease-in/ease-out approximation
/// Faster than cubic but slightly less accurate
/// Good for performance-critical applications
uint8_t easeInOutApprox8(uint8_t i);

// 16-bit easing functions
/// 16-bit quadratic ease-in function
/// Takes an input value 0-65535 and returns an eased value 0-65535
uint16_t easeInQuad16(uint16_t i);

/// 16-bit quadratic ease-in/ease-out function
/// Takes an input value 0-65535 and returns an eased value 0-65535
uint16_t easeInOutQuad16(uint16_t i);

/// 16-bit cubic ease-in/ease-out function
/// Takes an input value 0-65535 and returns an eased value 0-65535
uint16_t easeInOutCubic16(uint16_t i);

inline uint16_t ease16(EaseType type, uint16_t i) {
    switch (type) {
        case EASE_IN_QUAD: return easeInQuad16(i);
        case EASE_IN_OUT_QUAD: return easeInOutQuad16(i);
        case EASE_IN_OUT_CUBIC: return easeInOutCubic16(i);
    }
}

inline uint8_t ease8(EaseType type, uint8_t i) {
    switch (type) {
        case EASE_IN_QUAD: return easeInQuad8(i);
        case EASE_IN_OUT_QUAD: return easeInOutQuad8(i);
        case EASE_IN_OUT_CUBIC: return easeInOutCubic8(i);
    }
}

} // namespace fl