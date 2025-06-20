#define FASTLED_INTERNAL
#include "FastLED.h"

#include "fl/ease.h"
#include "lib8tion.h" // This is the problematic header that's hard to include

#include "fl/map_range.h"
#include "fl/sin32.h"
#include "lib8tion/intmap.h"

namespace fl {

// 8-bit easing functions
uint8_t easeInQuad8(uint8_t i) {
    // Simple quadratic ease-in: i^2 scaled to 8-bit range
    // Using scale8(i, i) which computes (i * i) / 255
    return scale8(i, i);
}

uint8_t easeInOutQuad8(uint8_t i) {
    constexpr uint16_t MAX = 0xFF;            // 255
    constexpr uint16_t HALF = (MAX + 1) >> 1; // 128
    constexpr uint16_t DENOM = MAX;           // divisor for scaling
    constexpr uint16_t ROUND = DENOM >> 1;    // for rounding

    if (i < HALF) {
        // first half: y = 2·(i/MAX)² → y_i = 2·i² / MAX
        uint32_t t = i;
        uint32_t num = 2 * t * t + ROUND; // 2*i², +half for rounding
        return uint8_t(num / DENOM);
    } else {
        // second half: y = 1 − 2·(1−i/MAX)²
        // → y_i = MAX − (2·(MAX−i)² / MAX)
        uint32_t d = MAX - i;
        uint32_t num = 2 * d * d + ROUND; // 2*(MAX−i)², +half for rounding
        return uint8_t(MAX - (num / DENOM));
    }
}

uint8_t easeInOutCubic8(uint8_t i) {
    constexpr uint16_t MAX = 0xFF;                  // 255
    constexpr uint16_t HALF = (MAX + 1) >> 1;       // 128
    constexpr uint32_t DENOM = (uint32_t)MAX * MAX; // 255*255 = 65025
    constexpr uint32_t ROUND = DENOM >> 1;          // for rounding

    if (i < HALF) {
        // first half: y = 4·(i/MAX)³ → y_i = 4·i³ / MAX²
        uint32_t ii = i;
        uint32_t cube = ii * ii * ii;    // i³
        uint32_t num = 4 * cube + ROUND; // 4·i³, +half denom for rounding
        return uint8_t(num / DENOM);
    } else {
        // second half: y = 1 − ((−2·t+2)³)/2
        // where t = i/MAX; equivalently:
        // y_i = MAX − (4·(MAX−i)³ / MAX²)
        uint32_t d = MAX - i;
        uint32_t cube = d * d * d; // (MAX−i)³
        uint32_t num = 4 * cube + ROUND;
        return uint8_t(MAX - (num / DENOM));
    }
}

uint8_t easeOutQuad8(uint8_t i) {
    // ease-out is the inverse of ease-in: 1 - (1-t)²
    // For 8-bit: y = MAX - (MAX-i)² / MAX
    constexpr uint16_t MAX = 0xFF;
    uint32_t d = MAX - i;              // (MAX - i)
    uint32_t num = d * d + (MAX >> 1); // (MAX-i)² + rounding
    return uint8_t(MAX - (num / MAX));
}

uint8_t easeInCubic8(uint8_t i) {
    // Simple cubic ease-in: i³ scaled to 8-bit range
    // y = i³ / MAX²
    constexpr uint16_t MAX = 0xFF;
    constexpr uint32_t DENOM = (uint32_t)MAX * MAX;
    constexpr uint32_t ROUND = DENOM >> 1;

    uint32_t ii = i;
    uint32_t cube = ii * ii * ii; // i³
    uint32_t num = cube + ROUND;
    return uint8_t(num / DENOM);
}

uint8_t easeOutCubic8(uint8_t i) {
    // ease-out cubic: 1 - (1-t)³
    // For 8-bit: y = MAX - (MAX-i)³ / MAX²
    constexpr uint16_t MAX = 0xFF;
    constexpr uint32_t DENOM = (uint32_t)MAX * MAX;
    constexpr uint32_t ROUND = DENOM >> 1;

    uint32_t d = MAX - i;      // (MAX - i)
    uint32_t cube = d * d * d; // (MAX-i)³
    uint32_t num = cube + ROUND;
    return uint8_t(MAX - (num / DENOM));
}

uint8_t easeInSine8(uint8_t i) {

    static const uint8_t easeInSineTable[256] = {
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,
        1,   1,   1,   1,   2,   2,   2,   2,   2,   3,   3,   3,   3,   4,
        4,   4,   4,   5,   5,   5,   6,   6,   6,   7,   7,   7,   8,   8,
        8,   9,   9,   10,  10,  11,  11,  12,  12,  12,  13,  13,  14,  14,
        15,  16,  16,  17,  17,  18,  18,  19,  20,  20,  21,  21,  22,  23,
        23,  24,  25,  25,  26,  27,  27,  28,  29,  30,  30,  31,  32,  33,
        33,  34,  35,  36,  37,  37,  38,  39,  40,  41,  42,  42,  43,  44,
        45,  46,  47,  48,  49,  50,  51,  52,  52,  53,  54,  55,  56,  57,
        58,  59,  60,  61,  62,  63,  64,  65,  67,  68,  69,  70,  71,  72,
        73,  74,  75,  76,  77,  79,  80,  81,  82,  83,  84,  86,  87,  88,
        89,  90,  91,  93,  94,  95,  96,  98,  99,  100, 101, 103, 104, 105,
        106, 108, 109, 110, 112, 113, 114, 115, 117, 118, 119, 121, 122, 123,
        125, 126, 127, 129, 130, 132, 133, 134, 136, 137, 139, 140, 141, 143,
        144, 146, 147, 148, 150, 151, 153, 154, 156, 157, 159, 160, 161, 163,
        164, 166, 167, 169, 170, 172, 173, 175, 176, 178, 179, 181, 182, 184,
        185, 187, 188, 190, 191, 193, 194, 196, 197, 199, 200, 202, 204, 205,
        207, 208, 210, 211, 213, 214, 216, 217, 219, 221, 222, 224, 225, 227,
        228, 230, 231, 233, 235, 236, 238, 239, 241, 242, 244, 246, 247, 249,
        250, 252, 253, 255};

    // ease-in sine: 1 - cos(t * π/2)
    // Handle boundary conditions explicitly
    return easeInSineTable[i];
}

uint8_t easeOutSine8(uint8_t i) {
    // ease-out sine: sin(t * π/2)
    // Handle boundary conditions explicitly
    if (i == 0)
        return 0;
    if (i == 255)
        return 255;

    // For 8-bit: use sin8 lookup table for efficiency
    // Map i from [0,255] to [0,64] in sin8 space (zero to quarter wave)
    // Formula: sin(t * π/2) where t goes from 0 to 1
    uint8_t angle = map8(i, 0, 64); // Map to quarter-wave range
    return sin8(angle);
}

uint8_t easeInOutSine8(uint8_t i) {
    // ease-in-out sine: -(cos(π*t) - 1) / 2
    // Handle boundary conditions explicitly
    if (i == 0)
        return 0;
    if (i == 255)
        return 255;

    // For 8-bit: use cos8 lookup table
    // Map i from [0,255] to [0,128] in cos8 space (0 to half wave)
    // Formula: (1 - cos(π*t)) / 2 where t goes from 0 to 1
    uint8_t angle = map8(i, 0, 128); // Map to half-wave range
    return (255 - cos8(angle)) >> 1; // (255 - cos) / 2
}

// 16-bit easing functions
uint16_t easeInQuad16(uint16_t i) {
    // Simple quadratic ease-in: i^2 scaled to 16-bit range
    // Using scale16(i, i) which computes (i * i) / 65535
    return scale16(i, i);
}

uint16_t easeInOutQuad16(uint16_t x) {
    // 16-bit quadratic ease-in / ease-out function
    constexpr uint32_t MAX = 0xFFFF;          // 65535
    constexpr uint32_t HALF = (MAX + 1) >> 1; // 32768
    constexpr uint32_t DENOM = MAX;           // divisor
    constexpr uint32_t ROUND = DENOM >> 1;    // for rounding

    if (x < HALF) {
        // first half: y = 2·(x/MAX)² → y_i = 2·x² / MAX
        uint64_t xi = x;
        uint64_t num = 2 * xi * xi + ROUND; // 2*x², +half for rounding
        return uint16_t(num / DENOM);
    } else {
        // second half: y = 1 − 2·(1−x/MAX)² → y_i = MAX − (2·(MAX−x)² / MAX)
        uint64_t d = MAX - x;
        uint64_t num = 2 * d * d + ROUND; // 2*(MAX−x)², +half for rounding
        return uint16_t(MAX - (num / DENOM));
    }
}

uint16_t easeInOutCubic16(uint16_t x) {
    const uint32_t MAX = 0xFFFF;             // 65535
    const uint32_t HALF = (MAX + 1) >> 1;    // 32768
    const uint64_t M2 = (uint64_t)MAX * MAX; // 65535² = 4 294 836 225

    if (x < HALF) {
        // first half:  y = 4·(x/MAX)³  →  y_i = 4·x³ / MAX²
        uint64_t xi = x;
        uint64_t cube = xi * xi * xi; // x³
        // add M2/2 for rounding
        uint64_t num = 4 * cube + (M2 >> 1);
        return (uint16_t)(num / M2);
    } else {
        // second half: y = 1 − ((2·(1−x/MAX))³)/2
        // → y_i = MAX − (4·(MAX−x)³ / MAX²)
        uint64_t d = MAX - x;
        uint64_t cube = d * d * d; // (MAX−x)³
        uint64_t num = 4 * cube + (M2 >> 1);
        return (uint16_t)(MAX - (num / M2));
    }
}

uint16_t easeOutQuad16(uint16_t i) {
    // ease-out quadratic: 1 - (1-t)²
    // For 16-bit: y = MAX - (MAX-i)² / MAX
    constexpr uint32_t MAX = 0xFFFF;     // 65535
    constexpr uint32_t ROUND = MAX >> 1; // for rounding

    uint64_t d = MAX - i;         // (MAX - i)
    uint64_t num = d * d + ROUND; // (MAX-i)² + rounding
    return uint16_t(MAX - (num / MAX));
}

uint16_t easeInCubic16(uint16_t i) {
    // Simple cubic ease-in: i³ scaled to 16-bit range
    // y = i³ / MAX²
    constexpr uint32_t MAX = 0xFFFF;                // 65535
    constexpr uint64_t DENOM = (uint64_t)MAX * MAX; // 65535²
    constexpr uint64_t ROUND = DENOM >> 1;          // for rounding

    uint64_t ii = i;
    uint64_t cube = ii * ii * ii; // i³
    uint64_t num = cube + ROUND;
    return uint16_t(num / DENOM);
}

uint16_t easeOutCubic16(uint16_t i) {
    // ease-out cubic: 1 - (1-t)³
    // For 16-bit: y = MAX - (MAX-i)³ / MAX²
    constexpr uint32_t MAX = 0xFFFF;                // 65535
    constexpr uint64_t DENOM = (uint64_t)MAX * MAX; // 65535²
    constexpr uint64_t ROUND = DENOM >> 1;          // for rounding

    uint64_t d = MAX - i;      // (MAX - i)
    uint64_t cube = d * d * d; // (MAX-i)³
    uint64_t num = cube + ROUND;
    return uint16_t(MAX - (num / DENOM));
}

uint16_t easeInSine16(uint16_t i) {
    // ease-in sine: 1 - cos(t * π/2)
    // Handle boundary conditions explicitly
    if (i == 0)
        return 0;
    if (i == 65535)
        return 65535;

    // For 16-bit: use cos32 for efficiency and accuracy
    // Map i from [0,65535] to [0,4194304] in cos32 space (zero to quarter wave)
    // Formula: 1 - cos(t * π/2) where t goes from 0 to 1
    // sin32/cos32 quarter cycle is 16777216/4 = 4194304
    uint32_t angle = ((uint64_t)i * 4194304ULL) / 65535ULL;
    int32_t cos_result = fl::cos32(angle);

    // Convert cos32 output and apply easing formula: 1 - cos(t * π/2)
    // cos32 output range is [-2147418112, 2147418112]
    // We want: (2147418112 - cos_result), then scale to [0, 65535]
    int64_t adjusted = 2147418112LL - (int64_t)cos_result;
    return (uint16_t)((uint64_t)adjusted * 65535ULL /
                      4294836224ULL); // 4294836224 = 2 * 2147418112
}

uint16_t easeOutSine16(uint16_t i) {

    static const uint16_t easeOutSine16Table[257] = {
        0,     402,   804,   1206,  1608,  2010,  2412,  2814,  3216,  3617,
        4019,  4420,  4821,  5222,  5623,  6023,  6424,  6824,  7223,  7623,
        8022,  8421,  8820,  9218,  9616,  10014, 10411, 10808, 11204, 11600,
        11996, 12391, 12785, 13179, 13573, 13966, 14359, 14751, 15142, 15533,
        15924, 16313, 16703, 17091, 17479, 17866, 18253, 18639, 19024, 19408,
        19792, 20175, 20557, 20939, 21319, 21699, 22078, 22456, 22834, 23210,
        23586, 23960, 24334, 24707, 25079, 25450, 25820, 26189, 26557, 26925,
        27291, 27656, 28020, 28383, 28745, 29106, 29465, 29824, 30181, 30538,
        30893, 31247, 31600, 31952, 32302, 32651, 32999, 33346, 33692, 34036,
        34379, 34721, 35061, 35400, 35738, 36074, 36409, 36743, 37075, 37406,
        37736, 38064, 38390, 38715, 39039, 39361, 39682, 40001, 40319, 40635,
        40950, 41263, 41575, 41885, 42194, 42500, 42806, 43109, 43411, 43712,
        44011, 44308, 44603, 44897, 45189, 45479, 45768, 46055, 46340, 46624,
        46905, 47185, 47464, 47740, 48014, 48287, 48558, 48827, 49095, 49360,
        49624, 49885, 50145, 50403, 50659, 50913, 51166, 51416, 51664, 51911,
        52155, 52398, 52638, 52877, 53113, 53348, 53580, 53811, 54039, 54266,
        54490, 54713, 54933, 55151, 55367, 55582, 55794, 56003, 56211, 56417,
        56620, 56822, 57021, 57218, 57413, 57606, 57797, 57985, 58171, 58356,
        58537, 58717, 58895, 59070, 59243, 59414, 59582, 59749, 59913, 60075,
        60234, 60391, 60546, 60699, 60850, 60998, 61144, 61287, 61429, 61567,
        61704, 61838, 61970, 62100, 62227, 62352, 62475, 62595, 62713, 62829,
        62942, 63053, 63161, 63267, 63371, 63472, 63571, 63668, 63762, 63853,
        63943, 64030, 64114, 64196, 64276, 64353, 64428, 64500, 64570, 64638,
        64703, 64765, 64826, 64883, 64939, 64992, 65042, 65090, 65136, 65179,
        65219, 65258, 65293, 65327, 65357, 65386, 65412, 65435, 65456, 65475,
        65491, 65504, 65515, 65524, 65530, 65534, 65535};

    // Integer-only ease-out sine with linear interpolation
    uint16_t idx = i >> 8;    // high 8 bits for index (0..255)
    uint16_t frac = i & 0xFF; // low 8 bits for fractional part
    uint16_t y0 = easeOutSine16Table[idx];
    uint16_t y1 = easeOutSine16Table[idx + 1];
    uint32_t diff = (uint32_t)y1 - y0;
    // interpolate: y0 + diff * frac / 256, with rounding
    return y0 + uint16_t((diff * frac + 0x80) >> 8);
}

uint16_t easeInOutSine16(uint16_t i) {
    // ease-in-out sine: -(cos(π*t) - 1) / 2
    // Handle boundary conditions explicitly
    if (i == 0)
        return 0;
    if (i == 65535)
        return 65535;

    // For 16-bit: use cos32 for efficiency and accuracy
    // Map i from [0,65535] to [0,8388608] in cos32 space (0 to half wave)
    // Formula: (1 - cos(π*t)) / 2 where t goes from 0 to 1
    // sin32/cos32 half cycle is 16777216/2 = 8388608
    uint32_t angle = ((uint64_t)i * 8388608ULL) / 65535ULL;
    int32_t cos_result = fl::cos32(angle);

    // Convert cos32 output and apply easing formula: (1 - cos(π*t)) / 2
    // cos32 output range is [-2147418112, 2147418112]
    // We want: (2147418112 - cos_result) / 2, then scale to [0, 65535]
    int64_t adjusted = (2147418112LL - (int64_t)cos_result) / 2;
    return (uint16_t)((uint64_t)adjusted * 65535ULL / 2147418112ULL);
}

} // namespace fl