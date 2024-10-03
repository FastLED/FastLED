#pragma once

#include <stdint.h>

uint16_t xy_serpentine(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    if (y & 1) // Even or odd row?
        // reverse every second line for a serpentine lled layout
        return (y + 1) * width - 1 - x;
    else
        return y * width + x;
}

uint16_t xy_line_by_line(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    return y * width + x;
}