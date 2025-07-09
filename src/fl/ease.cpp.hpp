#ifndef FASTLED_INTERNAL
#define FASTLED_INTERNAL
#endif

#include "FastLED.h"

#include "fl/ease.h"
#include "lib8tion.h" // This is the problematic header that's hard to include

#include "fl/map_range.h"
#include "lib8tion/intmap.h"
#include "fl/sin32.h"
#include "fl/int.h"

namespace fl {

// Gamma 2.8 lookup table for 8-bit to 16-bit gamma correction
// This table converts linear 8-bit values to gamma-corrected 16-bit values
// using a gamma curve of 2.8 (commonly used for LED brightness correction)
const u16 gamma_2_8[256] FL_PROGMEM = {
    0,     0,     0,     1,     1,     2,     4,     6,     8,     11,
    14,    18,    23,    29,    35,    41,    49,    57,    67,    77,
    88,    99,    112,   126,   141,   156,   173,   191,   210,   230,
    251,   274,   297,   322,   348,   375,   404,   433,   464,   497,
    531,   566,   602,   640,   680,   721,   763,   807,   853,   899,
    948,   998,   1050,  1103,  1158,  1215,  1273,  1333,  1394,  1458,
    1523,  1590,  1658,  1729,  1801,  1875,  1951,  2029,  2109,  2190,
    2274,  2359,  2446,  2536,  2627,  2720,  2816,  2913,  3012,  3114,
    3217,  3323,  3431,  3541,  3653,  3767,  3883,  4001,  4122,  4245,
    4370,  4498,  4627,  4759,  4893,  5030,  5169,  5310,  5453,  5599,
    5747,  5898,  6051,  6206,  6364,  6525,  6688,  6853,  7021,  7191,
    7364,  7539,  7717,  7897,  8080,  8266,  8454,  8645,  8838,  9034,
    9233,  9434,  9638,  9845,  10055, 10267, 10482, 10699, 10920, 11143,
    11369, 11598, 11829, 12064, 12301, 12541, 12784, 13030, 13279, 13530,
    13785, 14042, 14303, 14566, 14832, 15102, 15374, 15649, 15928, 16209,
    16493, 16781, 17071, 17365, 17661, 17961, 18264, 18570, 18879, 19191,
    19507, 19825, 20147, 20472, 20800, 21131, 21466, 21804, 22145, 22489,
    22837, 23188, 23542, 23899, 24260, 24625, 24992, 25363, 25737, 26115,
    26496, 26880, 27268, 27659, 28054, 28452, 28854, 29259, 29667, 30079,
    30495, 30914, 31337, 31763, 32192, 32626, 33062, 33503, 33947, 34394,
    34846, 35300, 35759, 36221, 36687, 37156, 37629, 38106, 38586, 39071,
    39558, 40050, 40545, 41045, 41547, 42054, 42565, 43079, 43597, 44119,
    44644, 45174, 45707, 46245, 46786, 47331, 47880, 48432, 48989, 49550,
    50114, 50683, 51255, 51832, 52412, 52996, 53585, 54177, 54773, 55374,
    55978, 56587, 57199, 57816, 58436, 59061, 59690, 60323, 60960, 61601,
    62246, 62896, 63549, 64207, 64869, 65535};

// 8-bit easing functions
u8 easeInQuad8(u8 i) {
    // Simple quadratic ease-in: i^2 scaled to 8-bit range
    // Using scale8(i, i) which computes (i * i) / 255
    return scale8(i, i);
}

u8 easeInOutQuad8(u8 i) {
    constexpr u16 MAX = 0xFF;            // 255
    constexpr u16 HALF = (MAX + 1) >> 1; // 128
    constexpr u16 DENOM = MAX;           // divisor for scaling
    constexpr u16 ROUND = DENOM >> 1;    // for rounding

    if (i < HALF) {
        // first half: y = 2·(i/MAX)² → y_i = 2·i² / MAX
        u32 t = i;
        u32 num = 2 * t * t + ROUND; // 2*i², +half for rounding
        return u8(num / DENOM);
    } else {
        // second half: y = 1 − 2·(1−i/MAX)²
        // → y_i = MAX − (2·(MAX−i)² / MAX)
        u32 d = MAX - i;
        u32 num = 2 * d * d + ROUND; // 2*(MAX−i)², +half for rounding
        return u8(MAX - (num / DENOM));
    }
}

u8 easeInOutCubic8(u8 i) {
    constexpr u16 MAX = 0xFF;                  // 255
    constexpr u16 HALF = (MAX + 1) >> 1;       // 128
    constexpr u32 DENOM = (u32)MAX * MAX; // 255*255 = 65025
    constexpr u32 ROUND = DENOM >> 1;          // for rounding

    if (i < HALF) {
        // first half: y = 4·(i/MAX)³ → y_i = 4·i³ / MAX²
        u32 ii = i;
        u32 cube = ii * ii * ii;    // i³
        u32 num = 4 * cube + ROUND; // 4·i³, +half denom for rounding
        return u8(num / DENOM);
    } else {
        // second half: y = 1 − ((−2·t+2)³)/2
        // where t = i/MAX; equivalently:
        // y_i = MAX − (4·(MAX−i)³ / MAX²)
        u32 d = MAX - i;
        u32 cube = d * d * d; // (MAX−i)³
        u32 num = 4 * cube + ROUND;
        return u8(MAX - (num / DENOM));
    }
}

u8 easeOutQuad8(u8 i) {
    // ease-out is the inverse of ease-in: 1 - (1-t)²
    // For 8-bit: y = MAX - (MAX-i)² / MAX
    constexpr u16 MAX = 0xFF;
    u32 d = MAX - i;              // (MAX - i)
    u32 num = d * d + (MAX >> 1); // (MAX-i)² + rounding
    return u8(MAX - (num / MAX));
}

u8 easeInCubic8(u8 i) {
    // Simple cubic ease-in: i³ scaled to 8-bit range
    // y = i³ / MAX²
    constexpr u16 MAX = 0xFF;
    constexpr u32 DENOM = (u32)MAX * MAX;
    constexpr u32 ROUND = DENOM >> 1;

    u32 ii = i;
    u32 cube = ii * ii * ii; // i³
    u32 num = cube + ROUND;
    return u8(num / DENOM);
}

u8 easeOutCubic8(u8 i) {
    // ease-out cubic: 1 - (1-t)³
    // For 8-bit: y = MAX - (MAX-i)³ / MAX²
    constexpr u16 MAX = 0xFF;
    constexpr u32 DENOM = (u32)MAX * MAX;
    constexpr u32 ROUND = DENOM >> 1;

    u32 d = MAX - i;      // (MAX - i)
    u32 cube = d * d * d; // (MAX-i)³
    u32 num = cube + ROUND;
    return u8(MAX - (num / DENOM));
}

u8 easeInSine8(u8 i) {

    static const u8 easeInSineTable[256] = {
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

u8 easeOutSine8(u8 i) {
    // ease-out sine: sin(t * π/2)
    // Delegate to 16-bit version for consistency and accuracy
    // Scale 8-bit input to 16-bit range, call 16-bit function, scale result back
    u16 input16 = map8_to_16(i);
    u16 result16 = easeOutSine16(input16);
    return map16_to_8(result16);
}

u8 easeInOutSine8(u8 i) {
    // ease-in-out sine: -(cos(π*t) - 1) / 2
    // Delegate to 16-bit version for consistency and accuracy
    // Scale 8-bit input to 16-bit range, call 16-bit function, scale result back
    u16 input16 = map8_to_16(i);
    u16 result16 = easeInOutSine16(input16);
    return map16_to_8(result16);
}

// 16-bit easing functions
u16 easeInQuad16(u16 i) {
    // Simple quadratic ease-in: i^2 scaled to 16-bit range
    // Using scale16(i, i) which computes (i * i) / 65535
    return scale16(i, i);
}

u16 easeInOutQuad16(u16 x) {
    // 16-bit quadratic ease-in / ease-out function
    constexpr u32 MAX = 0xFFFF;          // 65535
    constexpr u32 HALF = (MAX + 1) >> 1; // 32768
    constexpr u32 DENOM = MAX;           // divisor
    constexpr u32 ROUND = DENOM >> 1;    // for rounding

    if (x < HALF) {
        // first half: y = 2·(x/MAX)² → y_i = 2·x² / MAX
        fl::u64 xi = x;
        fl::u64 num = 2 * xi * xi + ROUND; // 2*x², +half for rounding
        return u16(num / DENOM);
    } else {
        // second half: y = 1 − 2·(1−x/MAX)² → y_i = MAX − (2·(MAX−x)² / MAX)
        fl::u64 d = MAX - x;
        fl::u64 num = 2 * d * d + ROUND; // 2*(MAX−x)², +half for rounding
        return u16(MAX - (num / DENOM));
    }
}

u16 easeInOutCubic16(u16 x) {
    const u32 MAX = 0xFFFF;             // 65535
    const u32 HALF = (MAX + 1) >> 1;    // 32768
    const fl::u64 M2 = (fl::u64)MAX * MAX; // 65535² = 4 294 836 225

    if (x < HALF) {
        // first half:  y = 4·(x/MAX)³  →  y_i = 4·x³ / MAX²
        fl::u64 xi = x;
        fl::u64 cube = xi * xi * xi; // x³
        // add M2/2 for rounding
        fl::u64 num = 4 * cube + (M2 >> 1);
        return (u16)(num / M2);
    } else {
        // second half: y = 1 − ((2·(1−x/MAX))³)/2
        // → y_i = MAX − (4·(MAX−x)³ / MAX²)
        fl::u64 d = MAX - x;
        fl::u64 cube = d * d * d; // (MAX−x)³
        fl::u64 num = 4 * cube + (M2 >> 1);
        return (u16)(MAX - (num / M2));
    }
}

u16 easeOutQuad16(u16 i) {
    // ease-out quadratic: 1 - (1-t)²
    // For 16-bit: y = MAX - (MAX-i)² / MAX
    constexpr u32 MAX = 0xFFFF;     // 65535
    constexpr u32 ROUND = MAX >> 1; // for rounding

    fl::u64 d = MAX - i;         // (MAX - i)
    fl::u64 num = d * d + ROUND; // (MAX-i)² + rounding
    return u16(MAX - (num / MAX));
}

u16 easeInCubic16(u16 i) {
    // Simple cubic ease-in: i³ scaled to 16-bit range
    // y = i³ / MAX²
    constexpr u32 MAX = 0xFFFF;                // 65535
    constexpr fl::u64 DENOM = (fl::u64)MAX * MAX; // 65535²
    constexpr fl::u64 ROUND = DENOM >> 1;          // for rounding

    fl::u64 ii = i;
    fl::u64 cube = ii * ii * ii; // i³
    fl::u64 num = cube + ROUND;
    return u16(num / DENOM);
}

u16 easeOutCubic16(u16 i) {
    // ease-out cubic: 1 - (1-t)³
    // For 16-bit: y = MAX - (MAX-i)³ / MAX²
    constexpr u32 MAX = 0xFFFF;                // 65535
    constexpr fl::u64 DENOM = (fl::u64)MAX * MAX; // 65535²
    constexpr fl::u64 ROUND = DENOM >> 1;          // for rounding

    fl::u64 d = MAX - i;      // (MAX - i)
    fl::u64 cube = d * d * d; // (MAX-i)³
    fl::u64 num = cube + ROUND;
    return u16(MAX - (num / DENOM));
}

u16 easeInSine16(u16 i) {
    // ease-in sine: 1 - cos(t * π/2)
    // Handle boundary conditions explicitly
    if (i == 0)
        return 0;
    // Remove the hard-coded boundary for 65535 and let math handle it
        
    // For 16-bit: use cos32 for efficiency and accuracy
    // Map i from [0,65535] to [0,4194304] in cos32 space (zero to quarter wave)
    // Formula: 1 - cos(t * π/2) where t goes from 0 to 1
    // sin32/cos32 quarter cycle is 16777216/4 = 4194304
    u32 angle = ((fl::u64)i * 4194304ULL) / 65535ULL;
    i32 cos_result = fl::cos32(angle);

    // Convert cos32 output and apply easing formula: 1 - cos(t * π/2)
    // cos32 output range is [-2147418112, 2147418112]
    // At t=0: cos(0) = 2147418112, result should be 0
    // At t=1: cos(π/2) = 0, result should be 65535
    
    const fl::i64 MAX_COS32 = 2147418112LL;
    
    // Calculate: (MAX_COS32 - cos_result) and scale to [0, 65535]
    fl::i64 adjusted = MAX_COS32 - (fl::i64)cos_result;
    
    // Scale from [0, 2147418112] to [0, 65535]
    fl::u64 result = (fl::u64)adjusted * 65535ULL + (MAX_COS32 >> 1); // Add half for rounding
    u16 final_result = (u16)(result / (fl::u64)MAX_COS32);
    
    return final_result;
}

u16 easeOutSine16(u16 i) {
    // ease-out sine: sin(t * π/2)
    // Handle boundary conditions explicitly
    if (i == 0)
        return 0;
    if (i == 65535)
        return 65535;

    // For 16-bit: use sin32 for efficiency and accuracy
    // Map i from [0,65535] to [0,4194304] in sin32 space (zero to quarter wave)
    // Formula: sin(t * π/2) where t goes from 0 to 1
    // sin32 quarter cycle is 16777216/4 = 4194304
    u32 angle = ((fl::u64)i * 4194304ULL) / 65535ULL;
    i32 sin_result = fl::sin32(angle);
    
    // Convert sin32 output range [-2147418112, 2147418112] to [0, 65535]
    // sin32 output is in range -32767*65536 to +32767*65536
    // For ease-out sine, we only use positive portion [0, 2147418112] -> [0, 65535]
    return (u16)((fl::u64)sin_result * 65535ULL / 2147418112ULL);
}

u16 easeInOutSine16(u16 i) {
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
    u32 angle = ((fl::u64)i * 8388608ULL) / 65535ULL;
    i32 cos_result = fl::cos32(angle);

    // Convert cos32 output and apply easing formula: (1 - cos(π*t)) / 2
    // cos32 output range is [-2147418112, 2147418112]
    // We want: (2147418112 - cos_result) / 2, then scale to [0, 65535]
    fl::i64 adjusted = (2147418112LL - (fl::i64)cos_result) / 2;
    return (u16)((fl::u64)adjusted * 65535ULL / 2147418112ULL);
}

} // namespace fl
