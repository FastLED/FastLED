/// @file lpuart_encoder.cpp
/// @brief Host-side tests for the WS2812-over-LPUART wave8 encoder.

#ifdef FASTLED_STUB_IMPL

#include "platforms/arm/teensy/teensy4_common/drivers/lpuart/lpuart_encoder.h"
#include "fl/stl/stdint.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_TEST_CASE("encoder all-zero WS2812 byte -> 4x 0xEF") {
    u8 out[4] = {0};
    lpuart_encode_byte(0x00, out);
    FL_REQUIRE_EQ((int)out[0], 0xEF);
    FL_REQUIRE_EQ((int)out[1], 0xEF);
    FL_REQUIRE_EQ((int)out[2], 0xEF);
    FL_REQUIRE_EQ((int)out[3], 0xEF);
}

FL_TEST_CASE("encoder all-ones WS2812 byte -> 4x 0x8C") {
    u8 out[4] = {0};
    lpuart_encode_byte(0xFF, out);
    for (int i = 0; i < 4; ++i) {
        FL_REQUIRE_EQ((int)out[i], 0x8C);
    }
}

FL_TEST_CASE("encoder MSB-first ordering: 0xF0 high half ones, low half zeros") {
    // 0xF0 = bits 7..0 = 1,1,1,1,0,0,0,0. MSB-first pairs:
    //   (b7,b6)=(1,1) -> 0x8C
    //   (b5,b4)=(1,1) -> 0x8C
    //   (b3,b2)=(0,0) -> 0xEF
    //   (b1,b0)=(0,0) -> 0xEF
    u8 out[4] = {0};
    lpuart_encode_byte(0xF0, out);
    FL_REQUIRE_EQ((int)out[0], 0x8C);
    FL_REQUIRE_EQ((int)out[1], 0x8C);
    FL_REQUIRE_EQ((int)out[2], 0xEF);
    FL_REQUIRE_EQ((int)out[3], 0xEF);
}

FL_TEST_CASE("encoder 0x0F is the byte-reverse: low half ones, high half zeros") {
    u8 out[4] = {0};
    lpuart_encode_byte(0x0F, out);
    FL_REQUIRE_EQ((int)out[0], 0xEF);
    FL_REQUIRE_EQ((int)out[1], 0xEF);
    FL_REQUIRE_EQ((int)out[2], 0x8C);
    FL_REQUIRE_EQ((int)out[3], 0x8C);
}

FL_TEST_CASE("encoder 0xAA alternating (1,0) pairs -> 4x 0xEC") {
    // 0xAA = 10101010. MSB-first pairs: (1,0)(1,0)(1,0)(1,0).
    u8 out[4] = {0};
    lpuart_encode_byte(0xAA, out);
    for (int i = 0; i < 4; ++i) {
        FL_REQUIRE_EQ((int)out[i], 0xEC);
    }
}

FL_TEST_CASE("encoder 0x55 alternating (0,1) pairs -> 4x 0x8F") {
    // 0x55 = 01010101. MSB-first pairs: (0,1)(0,1)(0,1)(0,1).
    u8 out[4] = {0};
    lpuart_encode_byte(0x55, out);
    for (int i = 0; i < 4; ++i) {
        FL_REQUIRE_EQ((int)out[i], 0x8F);
    }
}

FL_TEST_CASE("pair-to-byte table covers all 4 pair values") {
    FL_REQUIRE_EQ((int)lpuart_pair_to_byte(0), 0xEF);
    FL_REQUIRE_EQ((int)lpuart_pair_to_byte(1), 0x8F);
    FL_REQUIRE_EQ((int)lpuart_pair_to_byte(2), 0xEC);
    FL_REQUIRE_EQ((int)lpuart_pair_to_byte(3), 0x8C);
}

FL_TEST_CASE("encoder wire-format invariant: every UART byte produces correct H/L pattern") {
    // For each (WS0, WS1) pair, verify the bits the UART would put on
    // the wire (start_H + ~b0..~b7 + stop_L) match the expected
    // WS2812 5+5 pattern.
    struct PairCase {
        u8 ws0, ws1;
        const char* wire_pattern;  // 10 wire bits as H/L characters
    };
    const PairCase cases[] = {
        {0, 0, "HLLLLHLLLL"},
        {0, 1, "HLLLLHHHLL"},
        {1, 0, "HHHLLHLLLL"},
        {1, 1, "HHHLLHHHLL"},
    };

    for (const auto& c : cases) {
        const u8 pair = (u8)((c.ws0 << 1) | c.ws1);
        const u8 byte = lpuart_pair_to_byte(pair);

        char wire[11];
        wire[0] = 'H';  // start, inverted
        for (int i = 0; i < 8; ++i) {
            const int data_bit = (byte >> i) & 1;
            wire[1 + i] = data_bit ? 'L' : 'H';  // TXINV: wire = ~data
        }
        wire[9] = 'L';  // stop, inverted
        wire[10] = '\0';

        FL_INFO("pair WS0=" << (int)c.ws0 << " WS1=" << (int)c.ws1
                            << " byte=0x" << fl::hex << (int)byte << fl::dec
                            << " wire=" << wire);

        for (int i = 0; i < 10; ++i) {
            FL_REQUIRE_EQ((int)wire[i], (int)c.wire_pattern[i]);
        }
    }
}

}  // FL_TEST_FILE

#endif  // FASTLED_STUB_IMPL
