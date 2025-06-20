#define FASTLED_INTERNAL
#include "FastLED.h"

#include "fl/ease.h"
#include "lib8tion.h" // This is the problematic header that's hard to include

#include "fl/map_range.h"
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
    static const uint16_t easeInSine16Table[257] = {
        0,     1,     5,     11,    20,    31,    44,    60,    79,    100,
        123,   149,   178,   208,   242,   277,   316,   356,   399,   445,
        493,   543,   596,   652,   709,   770,   832,   897,   965,   1035,
        1107,  1182,  1259,  1339,  1421,  1505,  1592,  1682,  1773,  1867,
        1964,  2063,  2164,  2268,  2374,  2482,  2593,  2706,  2822,  2940,
        3060,  3183,  3308,  3435,  3565,  3697,  3831,  3968,  4106,  4248,
        4391,  4537,  4685,  4836,  4989,  5144,  5301,  5460,  5622,  5786,
        5953,  6121,  6292,  6465,  6640,  6818,  6998,  7179,  7364,  7550,
        7738,  7929,  8122,  8317,  8514,  8713,  8915,  9118,  9324,  9532,
        9741,  9953,  10168, 10384, 10602, 10822, 11045, 11269, 11496, 11724,
        11955, 12187, 12422, 12658, 12897, 13137, 13380, 13624, 13871, 14119,
        14369, 14622, 14876, 15132, 15390, 15650, 15911, 16175, 16440, 16708,
        16977, 17248, 17521, 17795, 18071, 18350, 18630, 18911, 19195, 19480,
        19767, 20056, 20346, 20638, 20932, 21227, 21524, 21823, 22124, 22426,
        22729, 23035, 23341, 23650, 23960, 24272, 24585, 24900, 25216, 25534,
        25853, 26174, 26496, 26820, 27145, 27471, 27799, 28129, 28460, 28792,
        29126, 29461, 29797, 30135, 30474, 30814, 31156, 31499, 31843, 32189,
        32536, 32884, 33233, 33583, 33935, 34288, 34642, 34997, 35354, 35711,
        36070, 36429, 36790, 37152, 37515, 37879, 38244, 38610, 38978, 39346,
        39715, 40085, 40456, 40828, 41201, 41575, 41949, 42325, 42701, 43079,
        43457, 43836, 44216, 44596, 44978, 45360, 45743, 46127, 46511, 46896,
        47282, 47669, 48056, 48444, 48832, 49222, 49611, 50002, 50393, 50784,
        51176, 51569, 51962, 52356, 52750, 53144, 53539, 53935, 54331, 54727,
        55124, 55521, 55919, 56317, 56715, 57114, 57513, 57912, 58312, 58711,
        59111, 59512, 59912, 60313, 60714, 61115, 61516, 61918, 62319, 62721,
        63123, 63525, 63927, 64329, 64731, 65133, 65535};
    if (i == 0xFFFF) return 0xFFFF;
    // Integer-only ease-in sine with linear interpolation
    uint16_t idx  = i >> 8;           // high 8 bits for index (0..255)
    uint16_t frac = i & 0xFF;         // low 8 bits for fractional part
    uint16_t y0   = easeInSine16Table[idx];
    uint16_t y1   = easeInSine16Table[idx + 1];
    uint32_t diff = (uint32_t)y1 - y0;
    // interpolate: y0 + diff * frac / 256, with rounding
    return y0 + uint16_t((diff * frac + 0x80) >> 8);
}

uint16_t easeOutSine16(uint16_t i) {
    // ease-out sine: sin(t * π/2)
    // Handle boundary conditions explicitly
    if (i == 0)
        return 0;
    if (i == 65535)
        return 65535;

    // For 16-bit: use sin16 lookup table for efficiency
    // Map i from [0,65535] to [0,16384] in sin16 space (zero to quarter wave)
    // Formula: sin(t * π/2) where t goes from 0 to 1
    uint16_t angle = map_range<uint16_t, uint16_t>(i, 0, 65535, 0, 16384);
    return sin16(angle);
}

uint16_t easeInOutSine16(uint16_t i) {
    // ease-in-out sine: -(cos(π*t) - 1) / 2
    // Handle boundary conditions explicitly
    if (i == 0)
        return 0;
    if (i == 65535)
        return 65535;

    // For 16-bit: use cos16 lookup table
    // Map i from [0,65535] to [0,32768] in cos16 space (0 to half wave)
    // Formula: (1 - cos(π*t)) / 2 where t goes from 0 to 1
    uint16_t angle = map_range<uint16_t, uint16_t>(i, 0, 65535, 0, 32768);
    return (65535 - cos16(angle)) >> 1; // (65535 - cos) / 2
}

} // namespace fl