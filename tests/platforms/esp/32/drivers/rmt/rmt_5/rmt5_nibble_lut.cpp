// Test for RMT5 nibble lookup table builder
//
// Validates that buildNibbleLut() produces correct RMT symbols for all nibble values

#include "test.h"

#ifndef ESP32
#define ESP32
#endif


#include "platforms/esp/32/drivers/rmt/rmt_5/rmt5_worker_lut.h"

using namespace fl;

TEST_CASE("rmt5_nibble_lut_correctness") {
    // WS2812B timings at 10MHz (100ns per tick)
    rmt_item32_t zero;
    zero.duration0 = 4;  // 400ns high
    zero.level0 = 1;
    zero.duration1 = 9;  // 900ns low
    zero.level1 = 0;

    rmt_item32_t one;
    one.duration0 = 8;  // 800ns high
    one.level0 = 1;
    one.duration1 = 4;  // 400ns low
    one.level1 = 0;

    rmt_nibble_lut_t lut;
    buildNibbleLut(lut, zero.val, one.val);

    // Test all 16 nibbles - each nibble maps to 4 RMT items (MSB first)
    for (int nibble = 0; nibble < 16; nibble++) {
        for (int bit_pos = 0; bit_pos < 4; bit_pos++) {
            int bit_mask = (0x8 >> bit_pos);  // 0x8, 0x4, 0x2, 0x1
            bool bit_value = (nibble & bit_mask) != 0;
            uint32_t expected = bit_value ? one.val : zero.val;
            CHECK(lut[nibble][bit_pos].val == expected);
        }
    }

    // Test specific byte conversion: 0b01101001 = 0x69
    // Bit pattern (MSB first): 0-1-1-0-1-0-0-1
    uint8_t test_byte = 0b01101001;
    uint8_t high_nibble = test_byte >> 4;    // 0x6 = 0110
    uint8_t low_nibble = test_byte & 0x0F;   // 0x9 = 1001

    // Verify each bit in unrolled loop with inlined expected values
    CHECK(lut[high_nibble][0].val == zero.val);  // bit 7 = 0
    CHECK(lut[high_nibble][1].val == one.val);   // bit 6 = 1
    CHECK(lut[high_nibble][2].val == one.val);   // bit 5 = 1
    CHECK(lut[high_nibble][3].val == zero.val);  // bit 4 = 0
    CHECK(lut[low_nibble][0].val == one.val);    // bit 3 = 1
    CHECK(lut[low_nibble][1].val == zero.val);   // bit 2 = 0
    CHECK(lut[low_nibble][2].val == zero.val);   // bit 1 = 0
    CHECK(lut[low_nibble][3].val == one.val);    // bit 0 = 1
}
