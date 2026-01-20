/// @file test_wave8_encoder_uart.cpp
/// @brief Unit tests for UART wave8 encoder
///
/// Tests the 2-bit LUT encoding strategy for UART LED transmission.
/// Validates encoding correctness, buffer sizing, and edge cases.

#include "doctest.h"

#include "platforms/esp/32/drivers/uart/wave8_encoder_uart.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "fl/stl/new.h"
#include "fl/stl/vector.h"

using namespace fl;

//=============================================================================
// Test Suite: 2-Bit LUT Encoding
//=============================================================================

TEST_CASE("Wave8 UART Encoder - 2-bit LUT correctness") {
    SUBCASE("LUT pattern 0x00 (0b00) → 0x11") {
        uint8_t result = detail::encodeUart2Bits(0x00);
        REQUIRE(result == 0x11);
    }

    SUBCASE("LUT pattern 0x01 (0b01) → 0x19") {
        uint8_t result = detail::encodeUart2Bits(0x01);
        REQUIRE(result == 0x19);
    }

    SUBCASE("LUT pattern 0x02 (0b10) → 0x91") {
        uint8_t result = detail::encodeUart2Bits(0x02);
        REQUIRE(result == 0x91);
    }

    SUBCASE("LUT pattern 0x03 (0b11) → 0x99") {
        uint8_t result = detail::encodeUart2Bits(0x03);
        REQUIRE(result == 0x99);
    }

    SUBCASE("LUT masking (input > 3 masked to 2 bits)") {
        // encodeUart2Bits should mask input to 2 bits
        REQUIRE(detail::encodeUart2Bits(0x00) == detail::encodeUart2Bits(0x00));
        REQUIRE(detail::encodeUart2Bits(0x01) == detail::encodeUart2Bits(0xFD));
        REQUIRE(detail::encodeUart2Bits(0x02) == detail::encodeUart2Bits(0xFE));
        REQUIRE(detail::encodeUart2Bits(0x03) == detail::encodeUart2Bits(0xFF));
    }
}

//=============================================================================
// Test Suite: Byte-Level Encoding
//=============================================================================

TEST_CASE("Wave8 UART Encoder - Byte encoding") {
    SUBCASE("Encode byte 0x00 (all bits 0)") {
        uint8_t output[4];
        detail::encodeUartByte(0x00, output);

        // 0x00 = 0b00 00 00 00 → all pairs are 0b00 → 0x11
        REQUIRE(output[0] == 0x11);  // Bits 7-6: 0b00
        REQUIRE(output[1] == 0x11);  // Bits 5-4: 0b00
        REQUIRE(output[2] == 0x11);  // Bits 3-2: 0b00
        REQUIRE(output[3] == 0x11);  // Bits 1-0: 0b00
    }

    SUBCASE("Encode byte 0xFF (all bits 1)") {
        uint8_t output[4];
        detail::encodeUartByte(0xFF, output);

        // 0xFF = 0b11 11 11 11 → all pairs are 0b11 → 0x99
        REQUIRE(output[0] == 0x99);  // Bits 7-6: 0b11
        REQUIRE(output[1] == 0x99);  // Bits 5-4: 0b11
        REQUIRE(output[2] == 0x99);  // Bits 3-2: 0b11
        REQUIRE(output[3] == 0x99);  // Bits 1-0: 0b11
    }

    SUBCASE("Encode byte 0xAA (alternating 1010 1010)") {
        uint8_t output[4];
        detail::encodeUartByte(0xAA, output);

        // 0xAA = 0b10 10 10 10 → all pairs are 0b10 → 0x91
        REQUIRE(output[0] == 0x91);  // Bits 7-6: 0b10
        REQUIRE(output[1] == 0x91);  // Bits 5-4: 0b10
        REQUIRE(output[2] == 0x91);  // Bits 3-2: 0b10
        REQUIRE(output[3] == 0x91);  // Bits 1-0: 0b10
    }

    SUBCASE("Encode byte 0x55 (alternating 0101 0101)") {
        uint8_t output[4];
        detail::encodeUartByte(0x55, output);

        // 0x55 = 0b01 01 01 01 → all pairs are 0b01 → 0x19
        REQUIRE(output[0] == 0x19);  // Bits 7-6: 0b01
        REQUIRE(output[1] == 0x19);  // Bits 5-4: 0b01
        REQUIRE(output[2] == 0x19);  // Bits 3-2: 0b01
        REQUIRE(output[3] == 0x19);  // Bits 1-0: 0b01
    }

    SUBCASE("Encode byte 0xE4 (mixed pattern)") {
        uint8_t output[4];
        detail::encodeUartByte(0xE4, output);

        // 0xE4 = 0b11 10 01 00
        REQUIRE(output[0] == 0x99);  // Bits 7-6: 0b11
        REQUIRE(output[1] == 0x91);  // Bits 5-4: 0b10
        REQUIRE(output[2] == 0x19);  // Bits 3-2: 0b01
        REQUIRE(output[3] == 0x11);  // Bits 1-0: 0b00
    }
}

//=============================================================================
// Test Suite: Buffer-Level Encoding
//=============================================================================

TEST_CASE("Wave8 UART Encoder - Buffer encoding") {
    SUBCASE("Encode single byte") {
        uint8_t input[] = { 0x42 };
        uint8_t output[4];

        size_t encoded = encodeLedsToUart(input, 1, output, sizeof(output));

        REQUIRE(encoded == 4);
        // 0x42 = 0b01 00 00 10
        REQUIRE(output[0] == 0x19);  // 0b01 → 0x19
        REQUIRE(output[1] == 0x11);  // 0b00 → 0x11
        REQUIRE(output[2] == 0x11);  // 0b00 → 0x11
        REQUIRE(output[3] == 0x91);  // 0b10 → 0x91
    }

    SUBCASE("Encode multiple bytes") {
        uint8_t input[] = { 0x00, 0xFF, 0xAA };
        uint8_t output[12];

        size_t encoded = encodeLedsToUart(input, 3, output, sizeof(output));

        REQUIRE(encoded == 12);

        // Byte 0: 0x00 → 0x11 0x11 0x11 0x11
        REQUIRE(output[0] == 0x11);
        REQUIRE(output[1] == 0x11);
        REQUIRE(output[2] == 0x11);
        REQUIRE(output[3] == 0x11);

        // Byte 1: 0xFF → 0x99 0x99 0x99 0x99
        REQUIRE(output[4] == 0x99);
        REQUIRE(output[5] == 0x99);
        REQUIRE(output[6] == 0x99);
        REQUIRE(output[7] == 0x99);

        // Byte 2: 0xAA → 0x91 0x91 0x91 0x91
        REQUIRE(output[8] == 0x91);
        REQUIRE(output[9] == 0x91);
        REQUIRE(output[10] == 0x91);
        REQUIRE(output[11] == 0x91);
    }

    SUBCASE("Encode RGB LED (3 bytes)") {
        // Red LED: R=255, G=0, B=0
        uint8_t input[] = { 0xFF, 0x00, 0x00 };
        uint8_t output[12];  // 3 bytes × 4 = 12 bytes

        size_t encoded = encodeLedsToUart(input, 3, output, sizeof(output));

        REQUIRE(encoded == 12);

        // R=0xFF → 0x99 0x99 0x99 0x99
        REQUIRE(output[0] == 0x99);
        REQUIRE(output[1] == 0x99);
        REQUIRE(output[2] == 0x99);
        REQUIRE(output[3] == 0x99);

        // G=0x00 → 0x11 0x11 0x11 0x11
        REQUIRE(output[4] == 0x11);
        REQUIRE(output[5] == 0x11);
        REQUIRE(output[6] == 0x11);
        REQUIRE(output[7] == 0x11);

        // B=0x00 → 0x11 0x11 0x11 0x11
        REQUIRE(output[8] == 0x11);
        REQUIRE(output[9] == 0x11);
        REQUIRE(output[10] == 0x11);
        REQUIRE(output[11] == 0x11);
    }

    SUBCASE("Encode 100 RGB LEDs") {
        const size_t num_leds = 100;
        fl::vector<uint8_t> input(num_leds * 3);  // 300 bytes
        fl::vector<uint8_t> output(num_leds * 3 * 4);  // 1200 bytes

        // Fill with test pattern (alternating 0x00 and 0xFF)
        for (size_t i = 0; i < input.size(); ++i) {
            input[i] = (i % 2 == 0) ? 0x00 : 0xFF;
        }

        size_t encoded = encodeLedsToUart(input.data(), input.size(),
                                          output.data(), output.size());

        REQUIRE(encoded == 1200);

        // Verify first LED (R=0x00, G=0xFF, B=0x00)
        REQUIRE(output[0] == 0x11);   // R byte 0 (index 0: even → 0x00)
        REQUIRE(output[4] == 0x99);   // G byte 0 (index 1: odd → 0xFF)
        REQUIRE(output[8] == 0x11);   // B byte 0 (index 2: even → 0x00)

        // Verify last LED encoding (LED 99)
        // Input byte indices: 297, 298, 299
        // 297 % 2 == 1 → 0xFF, 298 % 2 == 0 → 0x00, 299 % 2 == 1 → 0xFF
        size_t last_led_offset = (num_leds - 1) * 12;
        REQUIRE(output[last_led_offset + 0] == 0x99);  // R=0xFF → 0x99
        REQUIRE(output[last_led_offset + 4] == 0x11);  // G=0x00 → 0x11
        REQUIRE(output[last_led_offset + 8] == 0x99);  // B=0xFF → 0x99
    }
}

//=============================================================================
// Test Suite: Buffer Sizing
//=============================================================================

TEST_CASE("Wave8 UART Encoder - Buffer sizing") {
    SUBCASE("Calculate buffer size for raw bytes") {
        REQUIRE(calculateUartBufferSize(1) == 4);
        REQUIRE(calculateUartBufferSize(3) == 12);
        REQUIRE(calculateUartBufferSize(300) == 1200);  // 100 RGB LEDs
    }

    SUBCASE("Calculate buffer size for RGB LEDs") {
        REQUIRE(calculateUartBufferSizeForLeds(1) == 12);    // 1 LED = 12 bytes
        REQUIRE(calculateUartBufferSizeForLeds(10) == 120);  // 10 LEDs = 120 bytes
        REQUIRE(calculateUartBufferSizeForLeds(100) == 1200);  // 100 LEDs = 1200 bytes
        REQUIRE(calculateUartBufferSizeForLeds(1000) == 12000);  // 1000 LEDs = 12000 bytes
    }

    SUBCASE("Insufficient output buffer (returns 0)") {
        uint8_t input[] = { 0xFF };
        uint8_t output[3];  // Need 4 bytes, only 3 available

        size_t encoded = encodeLedsToUart(input, 1, output, sizeof(output));

        REQUIRE(encoded == 0);  // Encoding failed due to insufficient capacity
    }

    SUBCASE("Exact output buffer capacity (success)") {
        uint8_t input[] = { 0xFF };
        uint8_t output[4];  // Exactly 4 bytes

        size_t encoded = encodeLedsToUart(input, 1, output, sizeof(output));

        REQUIRE(encoded == 4);  // Encoding succeeded
        REQUIRE(output[0] == 0x99);
    }
}

//=============================================================================
// Test Suite: Edge Cases
//=============================================================================

TEST_CASE("Wave8 UART Encoder - Edge cases") {
    SUBCASE("Empty input (0 bytes)") {
        uint8_t output[16];

        size_t encoded = encodeLedsToUart(nullptr, 0, output, sizeof(output));

        REQUIRE(encoded == 0);  // 0 input bytes → 0 output bytes
    }

    SUBCASE("Large buffer encoding (stress test)") {
        const size_t large_size = 10000;  // 10,000 bytes input
        fl::vector<uint8_t> input(large_size, 0xAA);
        fl::vector<uint8_t> output(large_size * 4);

        size_t encoded = encodeLedsToUart(input.data(), input.size(),
                                          output.data(), output.size());

        REQUIRE(encoded == large_size * 4);

        // Verify first 10 bytes are correctly encoded
        for (size_t i = 0; i < 10; ++i) {
            REQUIRE(output[i] == 0x91);  // 0xAA = 0b10 10 10 10 → all 0x91
        }

        // Verify last 10 bytes are correctly encoded
        for (size_t i = output.size() - 10; i < output.size(); ++i) {
            REQUIRE(output[i] == 0x91);
        }
    }
}

//=============================================================================
// Test Suite: Waveform Validation
//=============================================================================

TEST_CASE("Wave8 UART Encoder - Waveform structure validation") {
    SUBCASE("Encoded byte has valid UART patterns") {
        // All LUT values must have valid bit patterns for UART transmission
        // Valid patterns: 0x11, 0x19, 0x91, 0x99 (rotated from original 0x88, 0x8C, 0xC8, 0xCC)
        for (uint8_t two_bits = 0; two_bits < 4; ++two_bits) {
            uint8_t encoded = detail::encodeUart2Bits(two_bits);

            // All LUT values should be one of the 4 valid patterns
            bool valid = (encoded == 0x11 || encoded == 0x19 ||
                         encoded == 0x91 || encoded == 0x99);
            REQUIRE(valid);
        }
    }

    SUBCASE("Verify bit distribution for LED protocols") {
        // LED protocols require specific pulse width ratios
        // The LUT patterns (0x11, 0x19, 0x91, 0x99) provide these ratios
        // when transmitted at 3.2 Mbps with UART start/stop bits
        // These are left-rotated by 1 bit from original patterns to align with UART framing

        // Pattern 0x11 (0b00010001): 2 HIGH bits, 6 LOW bits
        uint8_t pattern_00 = detail::encodeUart2Bits(0x00);
        int high_bits_00 = __builtin_popcount(pattern_00);
        REQUIRE(high_bits_00 == 2);

        // Pattern 0x19 (0b00011001): 3 HIGH bits, 5 LOW bits
        uint8_t pattern_01 = detail::encodeUart2Bits(0x01);
        int high_bits_01 = __builtin_popcount(pattern_01);
        REQUIRE(high_bits_01 == 3);

        // Pattern 0x91 (0b10010001): 3 HIGH bits, 5 LOW bits
        uint8_t pattern_10 = detail::encodeUart2Bits(0x02);
        int high_bits_10 = __builtin_popcount(pattern_10);
        REQUIRE(high_bits_10 == 3);

        // Pattern 0x99 (0b10011001): 4 HIGH bits, 4 LOW bits
        uint8_t pattern_11 = detail::encodeUart2Bits(0x03);
        int high_bits_11 = __builtin_popcount(pattern_11);
        REQUIRE(high_bits_11 == 4);
    }
}

//=============================================================================
// Test Suite: Performance Characteristics
//=============================================================================

TEST_CASE("Wave8 UART Encoder - Performance characteristics") {
    SUBCASE("Encoding determinism (repeated calls produce same output)") {
        uint8_t input[] = { 0x42, 0xAA, 0xFF };
        uint8_t output1[12];
        uint8_t output2[12];

        size_t encoded1 = encodeLedsToUart(input, 3, output1, sizeof(output1));
        size_t encoded2 = encodeLedsToUart(input, 3, output2, sizeof(output2));

        REQUIRE(encoded1 == encoded2);
        REQUIRE(encoded1 == 12);

        for (size_t i = 0; i < 12; ++i) {
            REQUIRE(output1[i] == output2[i]);
        }
    }

    SUBCASE("No data dependencies (parallel encoding feasible)") {
        // Each input byte encodes independently
        // Verify that encoding order doesn't affect output
        uint8_t input[] = { 0x11, 0x22, 0x33 };
        uint8_t output_sequential[12];
        uint8_t output_manual[12];

        // Sequential encoding
        encodeLedsToUart(input, 3, output_sequential, sizeof(output_sequential));

        // Manual encoding (simulates parallel)
        detail::encodeUartByte(input[0], &output_manual[0]);
        detail::encodeUartByte(input[1], &output_manual[4]);
        detail::encodeUartByte(input[2], &output_manual[8]);

        for (size_t i = 0; i < 12; ++i) {
            REQUIRE(output_sequential[i] == output_manual[i]);
        }
    }
}

//=============================================================================
// Test Suite: Bit Rotation Verification
//=============================================================================

TEST_CASE("Wave8 UART Encoder - Bit rotation correctness") {
    SUBCASE("LUT values are left-rotated by 1 bit from original patterns") {
        // Original patterns (before rotation): 0x88, 0x8C, 0xC8, 0xCC
        // Rotated patterns (current): 0x11, 0x19, 0x91, 0x99
        //
        // Left rotation by 1 bit: (value << 1) | (value >> 7)
        //
        // This rotation compensates for the UART transmission preamble that
        // shifts all bits by 1 position relative to the expected timing.

        // Helper function to left-rotate a byte by 1 bit
        auto rotate_left_1 = [](uint8_t value) -> uint8_t {
            return (value << 1) | (value >> 7);
        };

        // Verify rotation for 0b00 pattern
        uint8_t original_00 = 0x88;
        uint8_t rotated_00 = rotate_left_1(original_00);
        REQUIRE(rotated_00 == 0x11);
        REQUIRE(detail::encodeUart2Bits(0x00) == rotated_00);

        // Verify rotation for 0b01 pattern
        uint8_t original_01 = 0x8C;
        uint8_t rotated_01 = rotate_left_1(original_01);
        REQUIRE(rotated_01 == 0x19);
        REQUIRE(detail::encodeUart2Bits(0x01) == rotated_01);

        // Verify rotation for 0b10 pattern
        uint8_t original_10 = 0xC8;
        uint8_t rotated_10 = rotate_left_1(original_10);
        REQUIRE(rotated_10 == 0x91);
        REQUIRE(detail::encodeUart2Bits(0x02) == rotated_10);

        // Verify rotation for 0b11 pattern
        uint8_t original_11 = 0xCC;
        uint8_t rotated_11 = rotate_left_1(original_11);
        REQUIRE(rotated_11 == 0x99);
        REQUIRE(detail::encodeUart2Bits(0x03) == rotated_11);
    }

    SUBCASE("Rotation preserves bit count (HIGH bits remain same)") {
        // Rotation doesn't change the number of HIGH bits, only their position
        // This is critical for maintaining pulse width characteristics

        // Helper to count HIGH bits
        auto count_high_bits = [](uint8_t value) -> int {
            return __builtin_popcount(value);
        };

        // Original patterns and their bit counts
        uint8_t original_patterns[4] = {0x88, 0x8C, 0xC8, 0xCC};
        uint8_t rotated_patterns[4] = {0x11, 0x19, 0x91, 0x99};

        for (int i = 0; i < 4; ++i) {
            int original_count = count_high_bits(original_patterns[i]);
            int rotated_count = count_high_bits(rotated_patterns[i]);

            // Rotation must preserve bit count
            REQUIRE(original_count == rotated_count);

            // Verify current LUT uses rotated pattern
            REQUIRE(detail::encodeUart2Bits(i) == rotated_patterns[i]);
        }
    }

    SUBCASE("Rotated patterns align with UART framing sequence") {
        // The rotation compensates for the UART transmission preamble:
        // 1. Preamble (transmission setup)
        // 2. Start bit (LOW)
        // 3. Data bits (0-7)
        // 4. Stop bit (HIGH)
        // 5. Begin transmission to LED strip
        //
        // Without rotation, the start bit would misalign the data bits.
        // With rotation, the data bits align correctly with LED timing expectations.

        // Example: 0x11 (rotated from 0x88)
        // Binary: 0b00010001
        // UART frame: START(0) - 1-0-0-0-1-0-0-0 - STOP(1)
        //
        // Transmitted waveform (LSB first):
        // [S=L][B0=H][B1=L][B2=L][B3=L][B4=H][B5=L][B6=L][B7=L][P=H]
        //
        // The rotation ensures this waveform produces the correct pulse widths
        // for WS2812B-style LED protocols.

        uint8_t pattern = detail::encodeUart2Bits(0x00);  // 0x11
        REQUIRE(pattern == 0x11);

        // Verify it's the rotated version of the original 0x88
        uint8_t original = 0x88;
        uint8_t rotated = (original << 1) | (original >> 7);
        REQUIRE(pattern == rotated);
    }
}
