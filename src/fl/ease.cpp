#define FASTLED_INTERNAL
#include "FastLED.h"

#include "fl/ease.h"
#include "lib8tion.h" // This is the problematic header that's hard to include

#include "lib8tion/intmap.h"
#include "fl/map_range.h"

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

uint8_t easeOutQuad8(uint8_t i) {
    // ease-out is the inverse of ease-in: 1 - (1-t)²
    // For 8-bit: y = MAX - (MAX-i)² / MAX
    constexpr uint16_t MAX = 0xFF;
    uint32_t d = MAX - i;                           // (MAX - i) 
    uint32_t num = d * d + (MAX >> 1);              // (MAX-i)² + rounding
    return uint8_t(MAX - (num / MAX));
}

uint8_t easeInCubic8(uint8_t i) {
    // Simple cubic ease-in: i³ scaled to 8-bit range
    // y = i³ / MAX²
    constexpr uint16_t MAX = 0xFF;
    constexpr uint32_t DENOM = (uint32_t)MAX * MAX;
    constexpr uint32_t ROUND = DENOM >> 1;
    
    uint32_t ii = i;
    uint32_t cube = ii * ii * ii;                   // i³
    uint32_t num = cube + ROUND;
    return uint8_t(num / DENOM);
}

uint8_t easeOutCubic8(uint8_t i) {
    // ease-out cubic: 1 - (1-t)³
    // For 8-bit: y = MAX - (MAX-i)³ / MAX²
    constexpr uint16_t MAX = 0xFF;
    constexpr uint32_t DENOM = (uint32_t)MAX * MAX;
    constexpr uint32_t ROUND = DENOM >> 1;
    
    uint32_t d = MAX - i;                           // (MAX - i)
    uint32_t cube = d * d * d;                      // (MAX-i)³
    uint32_t num = cube + ROUND;
    return uint8_t(MAX - (num / DENOM));
}

uint8_t easeInSine8(uint8_t i) {
    // ease-in sine: 1 - cos(t * π/2)
    // For 8-bit: use sin8 lookup table for efficiency
    // Map i from [0,255] to [64,255] in sin8 space (quarter to full wave)
    // Then invert: 255 - result to get ease-in behavior
    uint8_t angle = map8(i, 64, 255);               // Map to quarter-wave range
    return 255 - cos8(angle);
}

uint8_t easeOutSine8(uint8_t i) {
    // ease-out sine: sin(t * π/2)
    // For 8-bit: use sin8 lookup table for efficiency
    // Map i from [0,255] to [0,64] in sin8 space (zero to quarter wave)
    uint8_t angle = map8(i, 0, 64);                 // Map to quarter-wave range
    return sin8(angle);
}

uint8_t easeInOutSine8(uint8_t i) {
    // ease-in-out sine: -(cos(π*t) - 1) / 2
    // For 8-bit: use cos8 lookup table
    // Map i from [0,255] to [0,128] in cos8 space (0 to half wave)
    uint8_t angle = map8(i, 0, 128);                // Map to half-wave range
    return (255 - cos8(angle)) >> 1;                // (255 - cos) / 2
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
    const uint64_t M2    = (uint64_t)MAX * MAX;     // 65535² = 4 294 836 225

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

uint16_t easeOutQuad16(uint16_t i) {
    // ease-out quadratic: 1 - (1-t)²
    // For 16-bit: y = MAX - (MAX-i)² / MAX
    constexpr uint32_t MAX = 0xFFFF;                // 65535
    constexpr uint32_t ROUND = MAX >> 1;            // for rounding
    
    uint64_t d = MAX - i;                           // (MAX - i)
    uint64_t num = d * d + ROUND;                   // (MAX-i)² + rounding
    return uint16_t(MAX - (num / MAX));
}

uint16_t easeInCubic16(uint16_t i) {
    // Simple cubic ease-in: i³ scaled to 16-bit range
    // y = i³ / MAX²
    constexpr uint32_t MAX = 0xFFFF;                // 65535
    constexpr uint64_t DENOM = (uint64_t)MAX * MAX; // 65535²
    constexpr uint64_t ROUND = DENOM >> 1;          // for rounding
    
    uint64_t ii = i;
    uint64_t cube = ii * ii * ii;                   // i³
    uint64_t num = cube + ROUND;
    return uint16_t(num / DENOM);
}

uint16_t easeOutCubic16(uint16_t i) {
    // ease-out cubic: 1 - (1-t)³
    // For 16-bit: y = MAX - (MAX-i)³ / MAX²
    constexpr uint32_t MAX = 0xFFFF;                // 65535
    constexpr uint64_t DENOM = (uint64_t)MAX * MAX; // 65535²
    constexpr uint64_t ROUND = DENOM >> 1;          // for rounding
    
    uint64_t d = MAX - i;                           // (MAX - i)
    uint64_t cube = d * d * d;                      // (MAX-i)³
    uint64_t num = cube + ROUND;
    return uint16_t(MAX - (num / DENOM));
}

uint16_t easeInSine16(uint16_t i) {
    // ease-in sine: 1 - cos(t * π/2)
    // For 16-bit: use sin16 lookup table for efficiency
    // Map i from [0,65535] to [16384,65535] in sin16 space (quarter to full wave)
    // uint16_t angle = map16(i, 16384, 65535);        // Map to quarter-wave range
    uint16_t angle = map_range<uint16_t, uint16_t>(i, 0, 65535, 16384, 65535);
    return 65535 - cos16(angle);
}

uint16_t easeOutSine16(uint16_t i) {
    // ease-out sine: sin(t * π/2)
    // For 16-bit: use sin16 lookup table for efficiency
    // Map i from [0,65535] to [0,16384] in sin16 space (zero to quarter wave)
    //uint16_t angle = map16(i, 0, 16384);            // Map to quarter-wave range
    uint16_t angle = map_range<uint16_t, uint16_t>(i, 0, 65535, 0, 16384);
    return sin16(angle);
}

uint16_t easeInOutSine16(uint16_t i) {
    // ease-in-out sine: -(cos(π*t) - 1) / 2
    // For 16-bit: use cos16 lookup table
    // Map i from [0,65535] to [0,32768] in cos16 space (0 to half wave)
    //uint16_t angle = map16(i, 0, 32768);            // Map to half-wave range
    uint16_t angle = map_range<uint16_t, uint16_t>(i, 0, 65535, 0, 32768);
    return (65535 - cos16(angle)) >> 1;             // (65535 - cos) / 2
}

} // namespace fl