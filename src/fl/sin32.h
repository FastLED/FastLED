#pragma once

#include "fl/stdint.h"

#include "force_inline.h"
#include "namespace.h"

namespace fl {

// fast and accurate sin and cos approximations
// they differ only 0.01 % from actual sinf and cosf funtions
// sin32 and cos32 use 24 bit unsigned integer arguments
// 0 to 16777216 is one cycle
// they output an integer between -2147418112 and 2147418112
// that's 32767 * 65536
// this is because I use int16_t look up table to interpolate the results
//
// sin16 and cos16 are faster and more accurate funtions that take uint16_t as
// arguments and return int16_t as output they can replace the older
// implementation and are 62 times more accurate and twice as fast the downside
// with these funtions is that they use 640 bytes for the look up table thats a
// lot for old microcontrollers but nothing for modern ones
//
// sin32 and cos32 take about 13 cyces to execute on an esp32
// they're 32 times faster than sinf and cosf
// you can use choose to use these new by writing
// #define USE_SIN_32 before #include "FastLED.h"

extern const int16_t *sinArray;

extern const int16_t *cosArray;

// 0 to 16777216 is a full circle
// output is between -2147418112 and 2147418112
FASTLED_FORCE_INLINE static int32_t sin32(uint32_t angle) {
    uint8_t angle256 = angle / 65536;
    int32_t subAngle = angle % 65536;
    return sinArray[angle256] * (65536 - subAngle) +
           sinArray[angle256 + 1] * subAngle;
}

// 0 to 16777216 is a full circle
// output is between -2147418112 and 2147418112
FASTLED_FORCE_INLINE static int32_t cos32(uint32_t angle) {
    uint8_t angle256 = angle / 65536;
    int32_t subAngle = angle % 65536;
    return cosArray[angle256] * (65536 - subAngle) +
           cosArray[angle256 + 1] * subAngle;
}

// 0 to 65536 is a full circle
// output is between -32767 and 32767
FASTLED_FORCE_INLINE static int16_t sin16lut(uint16_t angle) {
    uint8_t angle256 = angle / 256;
    int32_t subAngle = angle % 256;
    return (sinArray[angle256] * (256 - subAngle) +
            sinArray[angle256 + 1] * subAngle) /
           256;
}

// 0 to 65536 is a full circle
// output is between -32767 and 32767
FASTLED_FORCE_INLINE static int16_t cos16lut(uint16_t angle) {
    uint8_t angle256 = angle / 256;
    int32_t subAngle = angle % 256;
    return (cosArray[angle256] * (256 - subAngle) +
            cosArray[angle256 + 1] * subAngle) /
           256;
}

// Drop-in replacements for sin() and cos() using sin32 and cos32
// These functions take float arguments in radians and return float values
// They use the optimized sin32/cos32 functions internally for better performance
FASTLED_FORCE_INLINE static float sin_fixed(float angle_radians) {
    // Convert radians to sin32 angle format (0 to 16777216 is one full cycle)
    // 2π radians = 16777216 in sin32 format
    constexpr float RADIANS_TO_SIN32 = 16777216.0f / (2.0f * 3.14159265358979323846f);
    uint32_t angle_sin32 = (uint32_t)(angle_radians * RADIANS_TO_SIN32);
    
    // Call sin32 and convert result back to float
    // sin32 output range is [-2147418112, 2147418112] which maps to [-1.0, 1.0]
    constexpr float SIN32_TO_FLOAT = 1.0f / 2147418112.0f;
    return (float)sin32(angle_sin32) * SIN32_TO_FLOAT;
}

FASTLED_FORCE_INLINE static float cos_fixed(float angle_radians) {
    // Convert radians to cos32 angle format (0 to 16777216 is one full cycle)
    // 2π radians = 16777216 in cos32 format
    constexpr float RADIANS_TO_COS32 = 16777216.0f / (2.0f * 3.14159265358979323846f);
    uint32_t angle_cos32 = (uint32_t)(angle_radians * RADIANS_TO_COS32);
    
    // Call cos32 and convert result back to float
    // cos32 output range is [-2147418112, 2147418112] which maps to [-1.0, 1.0]
    constexpr float COS32_TO_FLOAT = 1.0f / 2147418112.0f;
    return (float)cos32(angle_cos32) * COS32_TO_FLOAT;
}

// Drop-in replacements for sinf() and cosf() using sin32 and cos32
// These functions take float arguments in radians and return float values
// They use the optimized sin32/cos32 functions internally for better performance
FASTLED_FORCE_INLINE static float sinf_fixed(float angle_radians) {
    // Convert radians to sin32 angle format (0 to 16777216 is one full cycle)
    // 2π radians = 16777216 in sin32 format
    constexpr float RADIANS_TO_SIN32 = 16777216.0f / (2.0f * 3.14159265358979323846f);
    uint32_t angle_sin32 = (uint32_t)(angle_radians * RADIANS_TO_SIN32);
    
    // Call sin32 and convert result back to float
    // sin32 output range is [-2147418112, 2147418112] which maps to [-1.0, 1.0]
    constexpr float SIN32_TO_FLOAT = 1.0f / 2147418112.0f;
    return (float)sin32(angle_sin32) * SIN32_TO_FLOAT;
}

FASTLED_FORCE_INLINE static float cosf_fixed(float angle_radians) {
    // Convert radians to cos32 angle format (0 to 16777216 is one full cycle)
    // 2π radians = 16777216 in cos32 format
    constexpr float RADIANS_TO_COS32 = 16777216.0f / (2.0f * 3.14159265358979323846f);
    uint32_t angle_cos32 = (uint32_t)(angle_radians * RADIANS_TO_COS32);
    
    // Call cos32 and convert result back to float
    // cos32 output range is [-2147418112, 2147418112] which maps to [-1.0, 1.0]
    constexpr float COS32_TO_FLOAT = 1.0f / 2147418112.0f;
    return (float)cos32(angle_cos32) * COS32_TO_FLOAT;
}

// Double precision version of sin_fixed
FASTLED_FORCE_INLINE static double sin_fixed(double angle_radians) {
    // Convert radians to sin32 angle format (0 to 16777216 is one full cycle)
    // 2π radians = 16777216 in sin32 format
    constexpr double RADIANS_TO_SIN32 = 16777216.0 / (2.0 * 3.14159265358979323846);
    uint32_t angle_sin32 = (uint32_t)(angle_radians * RADIANS_TO_SIN32);
    
    // Call sin32 and convert result back to double
    // sin32 output range is [-2147418112, 2147418112] which maps to [-1.0, 1.0]
    constexpr double SIN32_TO_DOUBLE = 1.0 / 2147418112.0;
    return (double)sin32(angle_sin32) * SIN32_TO_DOUBLE;
}

// Double precision version of cos_fixed
FASTLED_FORCE_INLINE static double cos_fixed(double angle_radians) {
    // Convert radians to cos32 angle format (0 to 16777216 is one full cycle)
    // 2π radians = 16777216 in cos32 format
    constexpr double RADIANS_TO_COS32 = 16777216.0 / (2.0 * 3.14159265358979323846);
    uint32_t angle_cos32 = (uint32_t)(angle_radians * RADIANS_TO_COS32);
    
    // Call cos32 and convert result back to double
    // cos32 output range is [-2147418112, 2147418112] which maps to [-1.0, 1.0]
    constexpr double COS32_TO_DOUBLE = 1.0 / 2147418112.0;
    return (double)cos32(angle_cos32) * COS32_TO_DOUBLE;
}

} // namespace fl
