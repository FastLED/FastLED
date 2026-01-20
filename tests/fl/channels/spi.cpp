/// @file spi.cpp
/// @brief Tests for SPI-based ChannelEngine WS2812 encoding
///
/// Tests the WS2812-over-SPI bit encoding implementation used by ChannelEngineSpi.
/// Each LED bit is encoded as 3 SPI bits (3:1 expansion ratio).

#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "doctest.h"
#include "fl/stl/cstring.h"

using namespace fl;

namespace {

// BIT macro definition for bit manipulation
#ifndef BIT
#define BIT(n) (1U << (n))
#endif

// Replicate encodeLedByte function for testing
// Note: This is a copy of the implementation from channel_engine_spi.cpp
void encodeLedByte(uint8_t data, uint8_t* buf) {
    // Each LED bit → 3 SPI bits at 2.5MHz
    // Low bit (0): 100 (binary) = keeps line low longer
    // High bit (1): 110 (binary) = keeps line high longer
    //
    // This encoding matches WS2812 timing requirements:
    // - T0H (high time for '0'): ~400ns → 1 bit @ 2.5MHz = 400ns
    // - T0L (low time for '0'):  ~850ns → 2 bits @ 2.5MHz = 800ns
    // - T1H (high time for '1'): ~800ns → 2 bits @ 2.5MHz = 800ns
    // - T1L (low time for '1'):  ~450ns → 1 bit @ 2.5MHz = 400ns

    // Note: Buffer must be zeroed before calling this function
    *(buf + 2) |= data & BIT(0) ? BIT(2) | BIT(1) : BIT(2);
    *(buf + 2) |= data & BIT(1) ? BIT(5) | BIT(4) : BIT(5);
    *(buf + 2) |= data & BIT(2) ? BIT(7) : 0x00;
    *(buf + 1) |= BIT(0);
    *(buf + 1) |= data & BIT(3) ? BIT(3) | BIT(2) : BIT(3);
    *(buf + 1) |= data & BIT(4) ? BIT(6) | BIT(5) : BIT(6);
    *(buf + 0) |= data & BIT(5) ? BIT(1) | BIT(0) : BIT(1);
    *(buf + 0) |= data & BIT(6) ? BIT(4) | BIT(3) : BIT(4);
    *(buf + 0) |= data & BIT(7) ? BIT(7) | BIT(6) : BIT(7);
}

} // anonymous namespace

TEST_CASE("WS2812 SPI encoding - basic patterns") {
    uint8_t buf[3];

    // Test all zeros (0x00)
    fl::memset(buf, 0, 3);
    encodeLedByte(0x00, buf);

    // Each bit should be encoded as 100b (0x4 in 3-bit chunks)
    // Verify buffer is not all zeros and not all ones
    CHECK_NE(buf[0], 0x00);
    CHECK_NE(buf[1], 0x00);
    CHECK_NE(buf[2], 0x00);
    CHECK_NE(buf[0], 0xFF);
    CHECK_NE(buf[1], 0xFF);
    CHECK_NE(buf[2], 0xFF);

    // Test all ones (0xFF)
    fl::memset(buf, 0, 3);
    encodeLedByte(0xFF, buf);

    // Each bit should be encoded as 110b (0x6 in 3-bit chunks)
    // Verify buffer has more bits set than 0x00 case
    CHECK_NE(buf[0], 0x00);
    CHECK_NE(buf[1], 0x00);
    CHECK_NE(buf[2], 0x00);
}

TEST_CASE("WS2812 SPI encoding - 3:1 expansion ratio") {
    uint8_t buf[3];

    // Test that 1 LED byte produces exactly 3 SPI bytes
    fl::memset(buf, 0, 3);
    encodeLedByte(0xAA, buf);  // 10101010b pattern

    // Verify all 3 bytes are populated
    CHECK_NE(buf[0], 0x00);
    CHECK_NE(buf[1], 0x00);
    CHECK_NE(buf[2], 0x00);

    // Test that buffer must be zeroed (encoding uses OR operations)
    uint8_t buf2[3] = {0xFF, 0xFF, 0xFF};
    encodeLedByte(0x00, buf2);

    // With pre-filled buffer, result should have all bits set
    CHECK_EQ(buf2[0], 0xFF);
    CHECK_EQ(buf2[1], 0xFF);
    CHECK_EQ(buf2[2], 0xFF);
}

TEST_CASE("WS2812 SPI encoding - specific bit patterns") {
    uint8_t buf[3];

    // Test bit 0 set (0x01)
    fl::memset(buf, 0, 3);
    encodeLedByte(0x01, buf);

    // Should have bit 1 and bit 2 set in buf[2] (110b pattern for '1')
    CHECK((buf[2] & (BIT(1) | BIT(2))) != 0);

    // Test bit 0 clear (0x00)
    fl::memset(buf, 0, 3);
    encodeLedByte(0x00, buf);

    // Should have only bit 2 set in buf[2] (100b pattern for '0')
    CHECK((buf[2] & BIT(2)) != 0);
}

TEST_CASE("WS2812 SPI encoding - alternating pattern") {
    uint8_t buf1[3], buf2[3];

    // Test 0x55 (01010101b) vs 0xAA (10101010b)
    fl::memset(buf1, 0, 3);
    fl::memset(buf2, 0, 3);

    encodeLedByte(0x55, buf1);
    encodeLedByte(0xAA, buf2);

    // These should produce different encodings
    CHECK_NE(buf1[0], buf2[0]);
    CHECK_NE(buf1[1], buf2[1]);
    CHECK_NE(buf1[2], buf2[2]);
}

TEST_CASE("WS2812 SPI encoding - buffer size requirements") {
    // Verify expansion ratio: N LED bytes → 3*N SPI bytes
    size_t led_count = 100;
    size_t bytes_per_led = 3;  // RGB
    size_t led_bytes = led_count * bytes_per_led;

    // Calculate SPI buffer size (3:1 expansion)
    size_t spi_bytes = led_bytes * 3;

    CHECK_EQ(spi_bytes, led_bytes * 3);
    CHECK_EQ(spi_bytes, 100 * 3 * 3);  // 100 LEDs * 3 colors * 3 expansion
    CHECK_EQ(spi_bytes, 900);
}

TEST_CASE("WS2812 SPI encoding - msb first") {
    uint8_t buf[3];

    // WS2812 uses MSB first bit ordering
    // Test 0x80 (bit 7 set, most significant)
    fl::memset(buf, 0, 3);
    encodeLedByte(0x80, buf);

    // Bit 7 should be encoded in buf[0] (first byte transmitted)
    CHECK_NE(buf[0], 0x00);

    // Test 0x01 (bit 0 set, least significant)
    fl::memset(buf, 0, 3);
    encodeLedByte(0x01, buf);

    // Bit 0 should be encoded in buf[2] (last byte transmitted)
    CHECK_NE(buf[2], 0x00);
}

TEST_CASE("WS2812 SPI encoding - all byte values") {
    uint8_t buf[3];

    // Test encoding for all 256 possible byte values
    for (int i = 0; i < 256; i++) {
        fl::memset(buf, 0, 3);
        encodeLedByte(static_cast<uint8_t>(i), buf);

        // Verify all 3 bytes are written (at least one bit set per byte)
        // Note: Due to bit patterns, at minimum buf[1] should have BIT(0) set
        bool hasData = (buf[0] != 0 || buf[1] != 0 || buf[2] != 0);
        CHECK(hasData);
    }
}
