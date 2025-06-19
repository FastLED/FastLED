#define FASTLED_INTERNAL
#include "FastLED.h"

#include "fl/ease.h"
#include "lib8tion.h"  // This is the problematic header that's hard to include

namespace fl {

// 8-bit easing functions
uint8_t easeInQuad8(uint8_t i) {
    // Simple quadratic ease-in: i^2 scaled to 8-bit range
    // Using scale8(i, i) which computes (i * i) / 255
    return scale8(i, i);
}

uint8_t easeInOutQuad8(uint8_t i) {
    return ease8InOutQuad(i);
}

uint8_t easeInOutCubic8(uint8_t i) {
    return ease8InOutCubic(i);
}

uint8_t easeInOutApprox8(uint8_t i) {
    return ease8InOutApprox(i);
}

// 16-bit easing functions
uint16_t easeInQuad16(uint16_t i) {
    // Simple quadratic ease-in: i^2 scaled to 16-bit range
    // Using scale16(i, i) which computes (i * i) / 65535
    return scale16(i, i);
}

uint16_t easeInOutQuad16(uint16_t i) {
    return ease16InOutQuad(i);
}

uint16_t easeInOutCubic16(uint16_t i) {
    return ease16InOutCubic(i);
}

}