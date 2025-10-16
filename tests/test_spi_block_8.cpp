/*
  FastLED — SpiBlock8 Unit Tests
  -------------------------------
  Basic tests for the 8-way (octal) blocking SPI driver.

  These tests verify the SpiBlock8 class compiles and has correct API.
  Full functional testing would require hardware or GPIO simulation.

  License: MIT (FastLED)
*/

#include "test.h"
#include "platforms/shared/spi_bitbang/spi_block_8.h"

namespace {

TEST_CASE("spi_block_8: basic instantiation") {
    fl::SpiBlock8 spi;

    // Verify constants
    CHECK(fl::SpiBlock8::NUM_DATA_PINS == 8);
    CHECK(fl::SpiBlock8::MAX_BUFFER_SIZE == 256);
}

TEST_CASE("spi_block_8: pin mapping setup") {
    fl::SpiBlock8 spi;

    // Configure pins (arbitrary GPIO numbers for test)
    spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8);

    // Verify LUT was initialized by checking it exists
    auto* lut = spi.getLUTArray();
    CHECK(lut != nullptr);

    // Verify some LUT entries make sense
    // For byte 0x00 (all bits low), all data pins should be cleared
    CHECK(lut[0x00].set_mask == 0);
    CHECK(lut[0x00].clear_mask != 0);

    // For byte 0xFF (all bits high), all data pins should be set
    CHECK(lut[0xFF].set_mask != 0);
    CHECK(lut[0xFF].clear_mask == 0);

    // For byte 0x01 (only bit 0 set), only D0 should be set
    CHECK(lut[0x01].set_mask == (1u << 0));
    CHECK((lut[0x01].clear_mask & ((1u << 1) | (1u << 2) | (1u << 3) |
                                     (1u << 4) | (1u << 5) | (1u << 6) | (1u << 7))) != 0);

    // For byte 0x80 (only bit 7 set), only D7 should be set
    CHECK(lut[0x80].set_mask == (1u << 7));
    CHECK((lut[0x80].clear_mask & ((1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) |
                                     (1u << 4) | (1u << 5) | (1u << 6))) != 0);

    // For byte 0x55 (01010101 pattern), D0+D2+D4+D6 should be set
    uint32_t expected_set_55 = (1u << 0) | (1u << 2) | (1u << 4) | (1u << 6);
    CHECK(lut[0x55].set_mask == expected_set_55);

    // For byte 0xAA (10101010 pattern), D1+D3+D5+D7 should be set
    uint32_t expected_set_AA = (1u << 1) | (1u << 3) | (1u << 5) | (1u << 7);
    CHECK(lut[0xAA].set_mask == expected_set_AA);
}

TEST_CASE("spi_block_8: buffer loading") {
    fl::SpiBlock8 spi;

    // Initially, buffer should be empty
    CHECK(spi.getBuffer() == nullptr);
    CHECK(spi.getBufferLength() == 0);

    // Load a buffer
    uint8_t data[] = {0x00, 0xFF, 0xAA, 0x55};
    spi.loadBuffer(data, 4);

    // Verify buffer was loaded
    CHECK(spi.getBuffer() == data);
    CHECK(spi.getBufferLength() == 4);
}

TEST_CASE("spi_block_8: buffer loading with size limit") {
    fl::SpiBlock8 spi;

    // Create a buffer larger than MAX_BUFFER_SIZE
    uint8_t large_data[300];
    for (int i = 0; i < 300; i++) {
        large_data[i] = static_cast<uint8_t>(i & 0xFF);
    }

    // Load buffer (should be clamped to MAX_BUFFER_SIZE)
    spi.loadBuffer(large_data, 300);

    // Verify buffer length was clamped
    CHECK(spi.getBuffer() == large_data);
    CHECK(spi.getBufferLength() == fl::SpiBlock8::MAX_BUFFER_SIZE);
}

TEST_CASE("spi_block_8: buffer loading with null pointer") {
    fl::SpiBlock8 spi;

    // Load a valid buffer first
    uint8_t data[] = {0x00, 0xFF};
    spi.loadBuffer(data, 2);
    CHECK(spi.getBuffer() == data);
    CHECK(spi.getBufferLength() == 2);

    // Try to load null pointer (should be ignored)
    spi.loadBuffer(nullptr, 10);

    // Verify buffer unchanged
    CHECK(spi.getBuffer() == data);
    CHECK(spi.getBufferLength() == 2);
}

TEST_CASE("spi_block_8: transmit with empty buffer") {
    fl::SpiBlock8 spi;

    // Configure pins
    spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8);

    // Try to transmit without loading buffer (should not crash)
    spi.transmit();

    // No assertion needed - just verify it doesn't crash
    CHECK(true);
}

TEST_CASE("spi_block_8: LUT verification for all patterns") {
    fl::SpiBlock8 spi;

    // Use sequential GPIO pins for easy verification
    spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8);

    auto* lut = spi.getLUTArray();

    // Verify all 256 LUT entries
    for (int byteValue = 0; byteValue < 256; byteValue++) {
        uint32_t expected_set = 0;
        uint32_t expected_clear = 0;

        // Calculate expected masks
        for (int bitPos = 0; bitPos < 8; bitPos++) {
            if (byteValue & (1 << bitPos)) {
                expected_set |= (1u << bitPos);
            } else {
                expected_clear |= (1u << bitPos);
            }
        }

        // Verify LUT entry
        CHECK(lut[byteValue].set_mask == expected_set);
        CHECK(lut[byteValue].clear_mask == expected_clear);
    }
}

TEST_CASE("spi_block_8: LUT verification with non-sequential pins") {
    fl::SpiBlock8 spi;

    // Use non-sequential GPIO pins
    uint8_t pins[8] = {10, 12, 14, 16, 18, 20, 22, 24};
    spi.setPinMapping(pins[0], pins[1], pins[2], pins[3],
                      pins[4], pins[5], pins[6], pins[7], 9);

    auto* lut = spi.getLUTArray();

    // Verify specific patterns
    // 0x00: All clear
    CHECK(lut[0x00].set_mask == 0);
    uint32_t all_pins = (1u << 10) | (1u << 12) | (1u << 14) | (1u << 16) |
                        (1u << 18) | (1u << 20) | (1u << 22) | (1u << 24);
    CHECK(lut[0x00].clear_mask == all_pins);

    // 0xFF: All set
    CHECK(lut[0xFF].set_mask == all_pins);
    CHECK(lut[0xFF].clear_mask == 0);

    // 0x01: Only pin 10 (D0) set
    CHECK(lut[0x01].set_mask == (1u << 10));

    // 0x80: Only pin 24 (D7) set
    CHECK(lut[0x80].set_mask == (1u << 24));
}

} // namespace
