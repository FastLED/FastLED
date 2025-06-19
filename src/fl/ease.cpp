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


uint16_t easeInOutCubic16(uint16_t x) {
    const uint32_t MAX   = 0xFFFF;                  // 65535
    const uint32_t HALF  = (MAX + 1) >> 1;          // 32768
    const uint64_t M2    = (uint64_t)MAX * MAX;     // 65535² = 4 294 836 225

    if (x < HALF) {
        // first half:  y = 4·(x/MAX)³  →  y_i = 4·x³ / MAX²
        uint64_t xi   = x;
        uint64_t cube = xi * xi * xi;               // x³
        // add M2/2 for rounding
        uint64_t num  = 4 * cube + (M2 >> 1);
        return (uint16_t)(num / M2);
    } else {
        // second half: y = 1 − ((2·(1−x/MAX))³)/2
        // → y_i = MAX − (4·(MAX−x)³ / MAX²)
        uint64_t d    = MAX - x;
        uint64_t cube = d * d * d;                   // (MAX−x)³
        uint64_t num  = 4 * cube + (M2 >> 1);
        return (uint16_t)(MAX - (num / M2));
    }
}


} // namespace fl