#define FASTLED_INTERNAL
#include "FastLED.h"

#include "fl/ease.h"
#include "lib8tion.h" // This is the problematic header that's hard to include

#include "lib8tion/intmap.h"

namespace fl {

// 8-bit easing functions
uint8_t easeInQuad8(uint8_t i) {
    // Simple quadratic ease-in: i^2 scaled to 8-bit range
    // Using scale8(i, i) which computes (i * i) / 255
    return scale8(i, i);
}

uint8_t easeInOutQuad8(uint8_t i) {
    constexpr uint16_t MAX   = 0xFF;             // 255
    constexpr uint16_t HALF  = (MAX + 1) >> 1;   // 128
    constexpr uint16_t DENOM = MAX;              // divisor for scaling
    constexpr uint16_t ROUND = DENOM >> 1;       // for rounding

    if (i < HALF) {
        // first half: y = 2·(i/MAX)² → y_i = 2·i² / MAX
        uint32_t t   = i;
        uint32_t num = 2 * t * t + ROUND;        // 2*i², +half for rounding
        return uint8_t(num / DENOM);
    } else {
        // second half: y = 1 − 2·(1−i/MAX)²
        // → y_i = MAX − (2·(MAX−i)² / MAX)
        uint32_t d   = MAX - i;
        uint32_t num = 2 * d * d + ROUND;        // 2*(MAX−i)², +half for rounding
        return uint8_t(MAX - (num / DENOM));
    }
}

uint8_t easeInOutCubic8(uint8_t i) {
    constexpr uint16_t MAX   = 0xFF;               // 255
    constexpr uint16_t HALF  = (MAX + 1) >> 1;     // 128
    constexpr uint32_t DENOM = (uint32_t)MAX * MAX; // 255*255 = 65025
    constexpr uint32_t ROUND = DENOM >> 1;         // for rounding

    if (i < HALF) {
        // first half: y = 4·(i/MAX)³ → y_i = 4·i³ / MAX²
        uint32_t ii   = i;
        uint32_t cube = ii * ii * ii;              // i³
        uint32_t num  = 4 * cube + ROUND;          // 4·i³, +half denom for rounding
        return uint8_t(num / DENOM);
    } else {
        // second half: y = 1 − ((−2·t+2)³)/2
        // where t = i/MAX; equivalently:
        // y_i = MAX − (4·(MAX−i)³ / MAX²)
        uint32_t d    = MAX - i;
        uint32_t cube = d * d * d;                  // (MAX−i)³
        uint32_t num  = 4 * cube + ROUND;
        return uint8_t(MAX - (num / DENOM));
    }
}

// 16-bit easing functions
uint16_t easeInQuad16(uint16_t i) {
    // Simple quadratic ease-in: i^2 scaled to 16-bit range
    // Using scale16(i, i) which computes (i * i) / 65535
    return scale16(i, i);
}

uint16_t easeInOutQuad16(uint16_t x) {
    // 16-bit quadratic ease-in / ease-out function
    constexpr uint32_t MAX    = 0xFFFF;             // 65535
    constexpr uint32_t HALF   = (MAX + 1) >> 1;     // 32768
    constexpr uint32_t DENOM  = MAX;                // divisor
    constexpr uint32_t ROUND  = DENOM >> 1;         // for rounding

    if (x < HALF) {
        // first half: y = 2·(x/MAX)² → y_i = 2·x² / MAX
        uint64_t xi  = x;
        uint64_t num = 2 * xi * xi + ROUND;         // 2*x², +half for rounding
        return uint16_t(num / DENOM);
    } else {
        // second half: y = 1 − 2·(1−x/MAX)² → y_i = MAX − (2·(MAX−x)² / MAX)
        uint64_t d   = MAX - x;
        uint64_t num = 2 * d * d + ROUND;           // 2*(MAX−x)², +half for rounding
        return uint16_t(MAX - (num / DENOM));
    }
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