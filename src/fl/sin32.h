#pragma once

#include "fl/stdint.h"

#include "fl/int.h"
#include "force_inline.h"
#include "namespace.h"

namespace fl {

// fast and accurate sin and cos approximations
// they differ only 0.01 % from actual sinf and cosf funtions
// sin32 and cos32 use 24 bit unsigned integer arguments
// 0 to 16777216 is one cycle
// they output an integer between -2147418112 and 2147418112
// that's 32767 * 65536
// this is because I use i16 look up table to interpolate the results
//
// sin16 and cos16 are faster and more accurate funtions that take u16 as
// arguments and return i16 as output they can replace the older
// implementation and are 62 times more accurate and twice as fast the downside
// with these funtions is that they use 640 bytes for the look up table thats a
// lot for old microcontrollers but nothing for modern ones
//
// sin32 and cos32 take about 13 cyces to execute on an esp32
// they're 32 times faster than sinf and cosf
// you can use choose to use these new by writing
// #define USE_SIN_32 before #include "FastLED.h"

extern const i16 *sinArray;

extern const i16 *cosArray;

// 0 to 16777216 is a full circle
// output is between -2147418112 and 2147418112
FASTLED_FORCE_INLINE static i32 sin32(u32 angle) {
    u8 angle256 = angle / 65536;
    i32 subAngle = angle % 65536;
    return sinArray[angle256] * (65536 - subAngle) +
           sinArray[angle256 + 1] * subAngle;
}

// 0 to 16777216 is a full circle
// output is between -2147418112 and 2147418112
FASTLED_FORCE_INLINE static i32 cos32(u32 angle) {
    u8 angle256 = angle / 65536;
    i32 subAngle = angle % 65536;
    return cosArray[angle256] * (65536 - subAngle) +
           cosArray[angle256 + 1] * subAngle;
}

// 0 to 65536 is a full circle
// output is between -32767 and 32767
FASTLED_FORCE_INLINE static i16 sin16lut(u16 angle) {
    u8 angle256 = angle / 256;
    i32 subAngle = angle % 256;
    return (sinArray[angle256] * (256 - subAngle) +
            sinArray[angle256 + 1] * subAngle) /
           256;
}

// 0 to 65536 is a full circle
// output is between -32767 and 32767
FASTLED_FORCE_INLINE static i16 cos16lut(u16 angle) {
    u8 angle256 = angle / 256;
    i32 subAngle = angle % 256;
    return (cosArray[angle256] * (256 - subAngle) +
            cosArray[angle256 + 1] * subAngle) /
           256;
}


} // namespace fl
