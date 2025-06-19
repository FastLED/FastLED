#define FASTLED_INTERNAL
#include "FastLED.h"

#include "fl/ease.h"
#include "lib8tion.h" // This is the problematic header that's hard to include

namespace fl {

// 8-bit easing functions
uint8_t easeInQuad8(uint8_t i) {
    // Simple quadratic ease-in: i^2 scaled to 8-bit range
    // Using scale8(i, i) which computes (i * i) / 255
    return scale8(i, i);
}

uint8_t easeInOutQuad8(uint8_t i) { return ease8InOutQuad(i); }

uint8_t easeInOutCubic8(uint8_t i) { return ease8InOutCubic(i); }

uint8_t easeInOutApprox8(uint8_t i) { return ease8InOutApprox(i); }

// 16-bit easing functions
uint16_t easeInQuad16(uint16_t i) {
    // Simple quadratic ease-in: i^2 scaled to 16-bit range
    // Using scale16(i, i) which computes (i * i) / 65535
    return scale16(i, i);
}

uint16_t easeInOutQuad16(uint16_t i) {
    // 16-bit quadratic ease-in / ease-out function
    uint16_t j = i;
    if (j & 0x8000) {
        j = 65535 - j;
    }
    uint16_t jj = scale16(j, j);
    uint16_t jj2 = jj << 1;
    if (i & 0x8000) {
        jj2 = 65535 - jj2;
    }
    return jj2;
}

uint16_t easeInOutCubic16(uint16_t i) {
    // 16-bit cubic ease-in / ease-out function
    // Equivalent to ease8InOutCubic() but for 16-bit values
    // Formula: 3(x^2) - 2(x^3) applied with proper ease-in-out curve

    // Apply the cubic formula directly, similar to the 8-bit version
    // scale16(a, b) computes (a * b) / 65536
    uint32_t ii = scale16(i, i);   // i^2 scaled to 16-bit
    uint32_t iii = scale16(ii, i); // i^3 scaled to 16-bit

    // Apply cubic formula: 3x^2 - 2x^3
    uint32_t r1 = (3 * ii) - (2 * iii);

    // Clamp result to 16-bit range
    if (r1 > 65535) {
        return 65535;
    }
    return (uint16_t)r1;
}

} // namespace fl