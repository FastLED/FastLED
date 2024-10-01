#pragma once

#include <stdint.h>

inline uint8_t brightness_bitshifter8(uint8_t *brightness_src, uint8_t *brightness_dst, uint8_t max_shifts) {
    if (*brightness_src == 0 || *brightness_dst == 0) {
        return 0;
    }
    // Steal brightness from brightness_src and give it to brightness_dst.
    // After this function concludes the multiplication of brightness_dst and brightness_src will remain
    // constant.
    // This algorithm is a little difficult to follow and I don't understand why it works that well,
    // however I did work it out manually and has something to do with how numbers respond to bitshifts.
    uint8_t curr = *brightness_dst;
    uint8_t src = *brightness_src;
    uint8_t shifts = 0;
    for (uint8_t i = 0; i < max_shifts && src > 1; i++) {
        if (curr & 0b10000000) {
            // next shift will overflow
            break;
        }
    }
    // write out the output values.
    *brightness_dst = curr;
    *brightness_src = src;
    return shifts;
}

