#pragma once

#include "fl/compiler_control.h"
#ifdef ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

#include "fl/stl/stdint.h"
#include "common.h"

namespace fl {

// RMT item structure (compatible with RMT4)
union rmt_item32_t {
    struct {
        uint32_t duration0 : 15;
        uint32_t level0 : 1;
        uint32_t duration1 : 15;
        uint32_t level1 : 1;
    };
    uint32_t val;
};

// Nibble lookup table: 16 nibbles (0x0-0xF), each mapping to 4 RMT items
typedef rmt_item32_t rmt_nibble_lut_t[16][4];

/**
 * Build nibble lookup table for fast byte-to-RMT conversion
 *
 * Each nibble (4 bits) maps to 4 RMT items (MSB first: bit3, bit2, bit1, bit0)
 * Same LUT used for both high nibble (bits 7-4) and low nibble (bits 3-0)
 *
 * @param lut Output array [16][4] to fill with RMT items
 * @param zero_val RMT item value for '0' bit (uint32_t)
 * @param one_val RMT item value for '1' bit (uint32_t)
 */
inline void buildNibbleLut(rmt_nibble_lut_t& lut, uint32_t zero_val, uint32_t one_val) {
    for (int nibble = 0; nibble < 16; nibble++) {
        // Nibble LUT: MSB first (bit 3 → bit 2 → bit 1 → bit 0)
        lut[nibble][0].val = (nibble & 0x8) ? one_val : zero_val;  // bit 3 (MSB)
        lut[nibble][1].val = (nibble & 0x4) ? one_val : zero_val;  // bit 2
        lut[nibble][2].val = (nibble & 0x2) ? one_val : zero_val;  // bit 1
        lut[nibble][3].val = (nibble & 0x1) ? one_val : zero_val;  // bit 0 (LSB)
    }
}

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32
