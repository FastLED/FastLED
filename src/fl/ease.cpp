#define FASTLED_INTERNAL
#include "FastLED.h"

#include "fl/ease.h"
#include "lib8tion.h"  // This is the problematic header that's hard to include

namespace fl {

// 8-bit easing functions
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
uint16_t easeInOutQuad16(uint16_t i) {
    return ease16InOutQuad(i);
}

uint16_t easeInOutCubic16(uint16_t i) {
    return ease16InOutCubic(i);
}


}