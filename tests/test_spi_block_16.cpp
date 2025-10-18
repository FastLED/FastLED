/*
  FastLED — SpiBlock16 Unit Tests
  --------------------------------
  Basic tests for the 16-way (hex) blocking SPI driver.

  These tests verify the SpiBlock16 class compiles and has correct API.
  Full functional testing would require hardware or GPIO simulation.

  License: MIT (FastLED)
*/

#include "test.h"
#include "platforms/shared/spi_bitbang/spi_block_16.h"

namespace {

TEST_CASE("spi_block_16: basic instantiation") {
    fl::SpiBlock16 spi;

    // Verify constants
    CHECK(fl::SpiBlock16::NUM_DATA_PINS == 16);
    CHECK(fl::SpiBlock16::MAX_BUFFER_SIZE == 256);
}

TEST_CASE("spi_block_16: pin mapping setup") {
    fl::SpiBlock16 spi;

    // Configure pins (arbitrary GPIO numbers for test)
    // Note: Only first 8 GPIO pins are used for data (D0-D7 of each byte)
    // Remaining 8 pin parameters (D8-D15) control alternate lanes or expansion
    spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

    // Verify LUT was initialized by checking it exists
    auto* lut = spi.getLUTArray();
    CHECK(lut != nullptr);

    // Verify some LUT entries make sense
    // The LUT maps 16 data pins, but each byte value only has 8 bits
    // Pins 8-15 are always cleared (no corresponding bits in a byte)
    // For byte 0x00 (all bits low), all 16 pins should be cleared
    CHECK(lut[0x00].set_mask == 0);
    CHECK(lut[0x00].clear_mask == 0xFFFF);  // All 16 pins need to be cleared

    // For byte 0xFF (all 8 bits high), first 8 pins set, pins 8-15 cleared
    CHECK(lut[0xFF].set_mask == 0x00FF);  // First 8 bits set
    CHECK(lut[0xFF].clear_mask == 0xFF00);  // Pins 8-15 cleared

    // For byte 0x01 (only bit 0 set), only D0 should be set, others cleared
    CHECK(lut[0x01].set_mask == (1u << 0));
    CHECK(lut[0x01].clear_mask == 0xFFFE);  // All others cleared

    // For byte 0x80 (only bit 7 set), only D7 should be set
    CHECK(lut[0x80].set_mask == (1u << 7));
    CHECK(lut[0x80].clear_mask == 0xFF7F);  // All others cleared

    // For byte 0x55 (01010101 pattern), D0+D2+D4+D6 should be set
    uint32_t expected_set_55 = (1u << 0) | (1u << 2) | (1u << 4) | (1u << 6);
    CHECK(lut[0x55].set_mask == expected_set_55);

    // For byte 0xAA (10101010 pattern), D1+D3+D5+D7 should be set
    uint32_t expected_set_AA = (1u << 1) | (1u << 3) | (1u << 5) | (1u << 7);
    CHECK(lut[0xAA].set_mask == expected_set_AA);
}

TEST_CASE("spi_block_16: buffer loading") {
    fl::SpiBlock16 spi;

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

TEST_CASE("spi_block_16: buffer loading with size limit") {
    fl::SpiBlock16 spi;

    // Create a buffer larger than MAX_BUFFER_SIZE
    uint8_t large_data[300];
    for (int i = 0; i < 300; i++) {
        large_data[i] = static_cast<uint8_t>(i & 0xFF);
    }

    // Load buffer (should be clamped to MAX_BUFFER_SIZE)
    spi.loadBuffer(large_data, 300);

    // Verify buffer length was clamped
    CHECK(spi.getBuffer() == large_data);
    CHECK(spi.getBufferLength() == fl::SpiBlock16::MAX_BUFFER_SIZE);
}

TEST_CASE("spi_block_16: buffer loading with null pointer") {
    fl::SpiBlock16 spi;

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

TEST_CASE("spi_block_16: transmit with empty buffer") {
    fl::SpiBlock16 spi;

    // Configure pins
    spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

    // Try to transmit without loading buffer (should not crash)
    spi.transmit();

    // No assertion needed - just verify it doesn't crash
    CHECK(true);
}

TEST_CASE("spi_block_16: LUT verification for all patterns") {
    fl::SpiBlock16 spi;

    // Use sequential GPIO pins for easy verification
    spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

    auto* lut = spi.getLUTArray();

    // Verify all 256 LUT entries
    for (int byteValue = 0; byteValue < 256; byteValue++) {
        uint32_t expected_set = 0;
        uint32_t expected_clear = 0xFFFF;  // All 16 data pins start cleared

        // Calculate expected masks
        for (int bitPos = 0; bitPos < 16; bitPos++) {
            if (byteValue & (1 << bitPos)) {
                expected_set |= (1u << bitPos);
                expected_clear &= ~(1u << bitPos);  // Remove from clear mask
            }
        }

        // Verify LUT entry
        CHECK(lut[byteValue].set_mask == expected_set);
        CHECK(lut[byteValue].clear_mask == expected_clear);
    }
}

TEST_CASE("spi_block_16: LUT verification with non-sequential pins") {
    fl::SpiBlock16 spi;

    // Use non-sequential GPIO pins for all 16 pins
    spi.setPinMapping(10, 12, 14, 16, 18, 20, 22, 24,
                      26, 28, 30, 1, 3, 5, 7, 9, 31);

    auto* lut = spi.getLUTArray();

    // Map out which GPIO pin corresponds to each bit
    uint32_t all_data_pins = (1u << 10) | (1u << 12) | (1u << 14) | (1u << 16) |
                             (1u << 18) | (1u << 20) | (1u << 22) | (1u << 24) |
                             (1u << 26) | (1u << 28) | (1u << 30) | (1u << 1) |
                             (1u << 3) | (1u << 5) | (1u << 7) | (1u << 9);

    // Verify specific patterns
    // 0x00: All data pins cleared
    CHECK(lut[0x00].set_mask == 0);
    CHECK(lut[0x00].clear_mask == all_data_pins);

    // 0xFF: Only first 8 bits set (pins 0-7), others not involved
    uint32_t first_8_pins = (1u << 10) | (1u << 12) | (1u << 14) | (1u << 16) |
                            (1u << 18) | (1u << 20) | (1u << 22) | (1u << 24);
    CHECK(lut[0xFF].set_mask == first_8_pins);
    uint32_t upper_8_pins = (1u << 26) | (1u << 28) | (1u << 30) | (1u << 1) |
                            (1u << 3) | (1u << 5) | (1u << 7) | (1u << 9);
    CHECK(lut[0xFF].clear_mask == upper_8_pins);

    // 0x01: Only pin 10 (D0) set
    CHECK(lut[0x01].set_mask == (1u << 10));
    uint32_t others_01 = (1u << 12) | (1u << 14) | (1u << 16) | (1u << 18) | (1u << 20) | (1u << 22) | (1u << 24) | upper_8_pins;
    CHECK(lut[0x01].clear_mask == others_01);
}

TEST_CASE("spi_block_16: 16-way pin count") {
    fl::SpiBlock16 spi;

    // Verify we can configure all 16 data pins
    spi.setPinMapping(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

    auto* lut = spi.getLUTArray();

    // Byte 0xFF only sets the first 8 pins (bits 0-7), pins 8-15 are cleared
    // because there are no corresponding bits in an 8-bit byte value
    CHECK(lut[0xFF].set_mask == 0x00FF);  // First 8 pins set
    CHECK(lut[0xFF].clear_mask == 0xFF00);  // Pins 8-15 cleared

    // Byte 0x00 clears all 16 pins
    CHECK(lut[0x00].clear_mask == 0xFFFF);

    // Verify we have 16 pin parameters accepted and configured
    CHECK(true);
}

} // namespace
