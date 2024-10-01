#pragma once

#include <stdint.h>

// Steals brightness from brightness_src and gives it to brightness_dst. After
// this function concludes the multiplication of brightness_dst and brightness_src will remain
// constant. The function will return true if the brightness_dst was changed.
// Note: This functionw was only tested with the brightness_src having fully saturated low order bits.
// I have no idea what will happen if the brightness_src is not fully saturated.
template<int N_BITS, uint8_t MAX_ITERATIONS = N_BITS-1>
bool bitshift_brightness(uint8_t *brightness_src, uint8_t *brightness_dst) {
    const uint8_t brightness = *brightness_dst;
    static_assert(
        N_BITS <= 5,
        "Not tested on more than 5 bits, denomintor or numerator may overflow.");
    uint8_t v5 = *brightness_src;
    uint32_t numerator = 1;
    uint16_t denominator = 1; // can hold all possible denominators for v5.
    // Loop while there is room to adjust brightness
    uint8_t iteration = 0;
    while (v5 > 1 && iteration++ < MAX_ITERATIONS) {
        // Calculate the next reduced value of v5
        uint8_t next_v5 = v5 >> 1;
        // Update the numerator and denominator to scale brightness
        uint32_t next_numerator = numerator * v5;
        uint32_t next_denominator = denominator * next_v5;
        // Check for overflow
        if (brightness * next_numerator > 0xff * next_denominator) {
            break;
        }

        numerator = next_numerator;
        denominator = next_denominator;
        v5 = next_v5;
    }
    if (denominator != 1) {
        uint32_t b32 = brightness;
        b32 *= numerator;
        *brightness_dst = static_cast<uint8_t>(b32 / denominator);
        *brightness_src = v5;
        return true;
    }
    return false;
}