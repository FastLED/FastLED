/// @file test_wave8_encoder_uart.cpp
/// @brief Unit tests for UART wave10 encoder
///
/// Tests the wave10 LUT generation, timing predicate, and encoding
/// correctness for multiple chipset timings.

#include "test.h"

#include "platforms/esp/32/drivers/uart/wave8_encoder_uart.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "fl/stl/new.h"
#include "fl/stl/vector.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

//=============================================================================
// Helper: Simulate UART wire waveform (with TX inversion)
//=============================================================================

namespace {

/// Generate 10-bit inverted UART wire pattern for a data byte
void uartFrameToBits(uint8_t byte_val, int out_bits[10]) {
    // Normal UART: START=0 (LOW), data LSB first, STOP=1 (HIGH)
    int raw[10];
    raw[0] = 0; // start bit
    for (int i = 0; i < 8; i++) {
        raw[1 + i] = (byte_val >> i) & 1;
    }
    raw[9] = 1; // stop bit

    // Apply TX inversion
    for (int i = 0; i < 10; i++) {
        out_bits[i] = raw[i] ? 0 : 1;
    }
}

/// Measure HIGH pulse durations in first and second 5-bit halves
struct HalfFramePulses {
    int high_count_a; // HIGH pulse count in positions 0-4
    int high_count_b; // HIGH pulse count in positions 5-9
};

HalfFramePulses measureHalfFrames(uint8_t byte_val) {
    int bits[10];
    uartFrameToBits(byte_val, bits);

    HalfFramePulses result = {0, 0};
    // Count leading HIGH pulses in each half
    for (int i = 0; i < 5 && bits[i] == 1; i++) result.high_count_a++;
    for (int i = 5; i < 10 && bits[i] == 1; i++) result.high_count_b++;
    return result;
}

} // anonymous namespace

//=============================================================================
// Test Suite: Legacy 2-Bit LUT Encoding (backward compatibility)
//=============================================================================

FL_TEST_CASE("Wave8 UART Encoder - 2-bit LUT correctness (legacy)") {
    FL_SUBCASE("LUT pattern 0x00 (0b00) -> 0xEF") {
        uint8_t result = detail::encodeUart2Bits(0x00);
        FL_REQUIRE_EQ(result, 0xEF);
    }

    FL_SUBCASE("LUT pattern 0x01 (0b01) -> 0x8F") {
        uint8_t result = detail::encodeUart2Bits(0x01);
        FL_REQUIRE_EQ(result, 0x8F);
    }

    FL_SUBCASE("LUT pattern 0x02 (0b10) -> 0xEC") {
        uint8_t result = detail::encodeUart2Bits(0x02);
        FL_REQUIRE_EQ(result, 0xEC);
    }

    FL_SUBCASE("LUT pattern 0x03 (0b11) -> 0x8C") {
        uint8_t result = detail::encodeUart2Bits(0x03);
        FL_REQUIRE_EQ(result, 0x8C);
    }

    FL_SUBCASE("LUT masking (input > 3 masked to 2 bits)") {
        FL_REQUIRE(detail::encodeUart2Bits(0x00) == detail::encodeUart2Bits(0x00));
        FL_REQUIRE(detail::encodeUart2Bits(0x01) == detail::encodeUart2Bits(0xFD));
        FL_REQUIRE(detail::encodeUart2Bits(0x02) == detail::encodeUart2Bits(0xFE));
        FL_REQUIRE(detail::encodeUart2Bits(0x03) == detail::encodeUart2Bits(0xFF));
    }
}

//=============================================================================
// Test Suite: Legacy Byte-Level Encoding
//=============================================================================

FL_TEST_CASE("Wave8 UART Encoder - Byte encoding (legacy)") {
    FL_SUBCASE("Encode byte 0x00 (all bits 0)") {
        uint8_t output[4];
        detail::encodeUartByte(0x00, output);
        FL_REQUIRE_EQ(output[0], 0xEF);
        FL_REQUIRE_EQ(output[1], 0xEF);
        FL_REQUIRE_EQ(output[2], 0xEF);
        FL_REQUIRE_EQ(output[3], 0xEF);
    }

    FL_SUBCASE("Encode byte 0xFF (all bits 1)") {
        uint8_t output[4];
        detail::encodeUartByte(0xFF, output);
        FL_REQUIRE_EQ(output[0], 0x8C);
        FL_REQUIRE_EQ(output[1], 0x8C);
        FL_REQUIRE_EQ(output[2], 0x8C);
        FL_REQUIRE_EQ(output[3], 0x8C);
    }

    FL_SUBCASE("Encode byte 0xE4 (mixed pattern)") {
        uint8_t output[4];
        detail::encodeUartByte(0xE4, output);
        FL_REQUIRE_EQ(output[0], 0x8C);  // Bits 7-6: 0b11
        FL_REQUIRE_EQ(output[1], 0xEC);  // Bits 5-4: 0b10
        FL_REQUIRE_EQ(output[2], 0x8F);  // Bits 3-2: 0b01
        FL_REQUIRE_EQ(output[3], 0xEF);  // Bits 1-0: 0b00
    }
}

//=============================================================================
// Test Suite: Legacy Buffer-Level Encoding
//=============================================================================

FL_TEST_CASE("Wave8 UART Encoder - Buffer encoding (legacy)") {
    FL_SUBCASE("Encode single byte") {
        uint8_t input[] = { 0x42 };
        uint8_t output[4];
        size_t encoded = encodeLedsToUart(input, 1, output, sizeof(output));
        FL_REQUIRE(encoded == 4);
        FL_REQUIRE_EQ(output[0], 0x8F);  // 0b01
        FL_REQUIRE_EQ(output[1], 0xEF);  // 0b00
        FL_REQUIRE_EQ(output[2], 0xEF);  // 0b00
        FL_REQUIRE_EQ(output[3], 0xEC);  // 0b10
    }

    FL_SUBCASE("Insufficient output buffer (returns 0)") {
        uint8_t input[] = { 0xFF };
        uint8_t output[3];
        size_t encoded = encodeLedsToUart(input, 1, output, sizeof(output));
        FL_REQUIRE(encoded == 0);
    }
}

//=============================================================================
// Test Suite: Buffer Sizing
//=============================================================================

FL_TEST_CASE("Wave8 UART Encoder - Buffer sizing") {
    FL_SUBCASE("Calculate buffer size for raw bytes") {
        FL_REQUIRE(calculateUartBufferSize(1) == 4);
        FL_REQUIRE(calculateUartBufferSize(3) == 12);
        FL_REQUIRE(calculateUartBufferSize(300) == 1200);
    }

    FL_SUBCASE("Calculate buffer size for RGB LEDs") {
        FL_REQUIRE(calculateUartBufferSizeForLeds(1) == 12);
        FL_REQUIRE(calculateUartBufferSizeForLeds(10) == 120);
        FL_REQUIRE(calculateUartBufferSizeForLeds(100) == 1200);
        FL_REQUIRE(calculateUartBufferSizeForLeds(1000) == 12000);
    }
}

//=============================================================================
// Test Suite: Wave10 LUT Generation
//=============================================================================

FL_TEST_CASE("Wave10 - buildWave10Lut for WS2812") {
    // WS2812: T1=250, T2=625, T3=375, period=1250
    ChipsetTimingConfig timing(250, 625, 375, 280);
    Wave10Lut lut = buildWave10Lut(timing);

    FL_SUBCASE("LUT matches hardcoded WS2812 values") {
        FL_CHECK_EQ(lut.lut[0], 0xEF);  // "00"
        FL_CHECK_EQ(lut.lut[1], 0x8F);  // "01"
        FL_CHECK_EQ(lut.lut[2], 0xEC);  // "10"
        FL_CHECK_EQ(lut.lut[3], 0x8C);  // "11"
    }

    FL_SUBCASE("Wire waveform has correct pulse structure") {
        // "00" pattern: both LED bits should have 1 HIGH pulse
        auto p00 = measureHalfFrames(lut.lut[0]);
        FL_CHECK_EQ(p00.high_count_a, 1);
        FL_CHECK_EQ(p00.high_count_b, 1);

        // "11" pattern: both LED bits should have 3 HIGH pulses
        auto p11 = measureHalfFrames(lut.lut[3]);
        FL_CHECK_EQ(p11.high_count_a, 3);
        FL_CHECK_EQ(p11.high_count_b, 3);

        // "01" pattern: bit A=0 (1 pulse), bit B=1 (3 pulses)
        auto p01 = measureHalfFrames(lut.lut[1]);
        FL_CHECK_EQ(p01.high_count_a, 1);
        FL_CHECK_EQ(p01.high_count_b, 3);

        // "10" pattern: bit A=1 (3 pulses), bit B=0 (1 pulse)
        auto p10 = measureHalfFrames(lut.lut[2]);
        FL_CHECK_EQ(p10.high_count_a, 3);
        FL_CHECK_EQ(p10.high_count_b, 1);
    }

    FL_SUBCASE("Baud rate is 4.0 Mbps") {
        u32 baud = Wave10Lut::computeBaudRate(timing);
        FL_CHECK_EQ(baud, 4000000u);
    }
}

FL_TEST_CASE("Wave10 - buildWave10Lut for SK6812") {
    // SK6812: T1=300, T2=600, T3=300, period=1200
    ChipsetTimingConfig timing(300, 600, 300, 80);
    Wave10Lut lut = buildWave10Lut(timing);

    FL_SUBCASE("Pulse counts are correct") {
        // pulse_width = 1200/5 = 240ns
        // T0H = 300ns → 300/240 = 1.25 → best-fit: floor=1 (err=60) vs ceil=2 (err=180) → 1
        // T1H = 900ns → 900/240 = 3.75 → best-fit: floor=3 (err=180) vs ceil=4 (err=60) → 4
        auto p00 = measureHalfFrames(lut.lut[0]);
        FL_CHECK_EQ(p00.high_count_a, 1);

        auto p11 = measureHalfFrames(lut.lut[3]);
        FL_CHECK_EQ(p11.high_count_a, 4);
    }

    FL_SUBCASE("All 4 LUT entries are distinct") {
        FL_CHECK_NE(lut.lut[0], lut.lut[1]);
        FL_CHECK_NE(lut.lut[0], lut.lut[2]);
        FL_CHECK_NE(lut.lut[0], lut.lut[3]);
        FL_CHECK_NE(lut.lut[1], lut.lut[2]);
        FL_CHECK_NE(lut.lut[1], lut.lut[3]);
        FL_CHECK_NE(lut.lut[2], lut.lut[3]);
    }

    FL_SUBCASE("Baud rate is correct") {
        u32 baud = Wave10Lut::computeBaudRate(timing);
        // 5e9 / 1200 = 4166666
        FL_CHECK_EQ(baud, 4166666u);
    }
}

FL_TEST_CASE("Wave10 - buildWave10Lut for WS2811-400") {
    // WS2811-400: T1=500, T2=700, T3=1300, period=2500
    ChipsetTimingConfig timing(500, 700, 1300, 280);
    Wave10Lut lut = buildWave10Lut(timing);

    FL_SUBCASE("Pulse counts are correct") {
        // pulse_width = 2500/5 = 500ns
        // T0H = 500ns → 500/500 = 1.0 → 1 pulse
        // T1H = 1200ns → 1200/500 = 2.4 → rounds to 2
        auto p00 = measureHalfFrames(lut.lut[0]);
        FL_CHECK_EQ(p00.high_count_a, 1);

        auto p11 = measureHalfFrames(lut.lut[3]);
        FL_CHECK_EQ(p11.high_count_a, 2);
    }

    FL_SUBCASE("Baud rate is 2.0 Mbps") {
        u32 baud = Wave10Lut::computeBaudRate(timing);
        FL_CHECK_EQ(baud, 2000000u);
    }
}

//=============================================================================
// Test Suite: Wave10 Encoding Round-Trip
//=============================================================================

FL_TEST_CASE("Wave10 - Encoding with dynamic LUT matches legacy for WS2812") {
    ChipsetTimingConfig timing(250, 625, 375, 280);
    Wave10Lut lut = buildWave10Lut(timing);

    FL_SUBCASE("Single byte encoding matches legacy") {
        for (int val = 0; val < 256; val++) {
            uint8_t input = static_cast<uint8_t>(val);
            uint8_t output_legacy[4];
            uint8_t output_wave10[4];

            encodeLedsToUart(&input, 1, output_legacy, 4);
            encodeLedsToUart(&input, 1, output_wave10, 4, lut);

            for (int i = 0; i < 4; i++) {
                FL_CHECK_EQ(output_legacy[i], output_wave10[i]);
            }
        }
    }
}

FL_TEST_CASE("Wave10 - Encoding with SK6812 LUT") {
    ChipsetTimingConfig timing(300, 600, 300, 80);
    Wave10Lut lut = buildWave10Lut(timing);

    FL_SUBCASE("Encode 0x00 and 0xFF") {
        uint8_t input_zero = 0x00;
        uint8_t input_ones = 0xFF;
        uint8_t output_zero[4];
        uint8_t output_ones[4];

        encodeLedsToUart(&input_zero, 1, output_zero, 4, lut);
        encodeLedsToUart(&input_ones, 1, output_ones, 4, lut);

        // All bytes in output_zero should be the same (lut[0])
        for (int i = 0; i < 4; i++) {
            FL_CHECK_EQ(output_zero[i], lut.lut[0]);
        }
        // All bytes in output_ones should be the same (lut[3])
        for (int i = 0; i < 4; i++) {
            FL_CHECK_EQ(output_ones[i], lut.lut[3]);
        }
    }
}

//=============================================================================
// Test Suite: canRepresentTiming Predicate
//=============================================================================

FL_TEST_CASE("Wave10 - canRepresentTiming accepts valid chipsets") {
    FL_SUBCASE("WS2812 (800kHz)") {
        ChipsetTimingConfig timing(250, 625, 375, 280);
        FL_CHECK(canRepresentTiming(timing));
    }

    FL_SUBCASE("SK6812") {
        ChipsetTimingConfig timing(300, 600, 300, 80);
        FL_CHECK(canRepresentTiming(timing));
    }

    FL_SUBCASE("WS2811-400 (400kHz)") {
        ChipsetTimingConfig timing(500, 700, 1300, 280);
        FL_CHECK(canRepresentTiming(timing));
    }

    FL_SUBCASE("WS2815") {
        ChipsetTimingConfig timing(250, 1090, 550, 0);
        FL_CHECK(canRepresentTiming(timing));
    }

    FL_SUBCASE("TM1809-800") {
        ChipsetTimingConfig timing(350, 350, 450, 0);
        FL_CHECK(canRepresentTiming(timing));
    }

    FL_SUBCASE("GE8822") {
        ChipsetTimingConfig timing(350, 660, 350, 0);
        FL_CHECK(canRepresentTiming(timing));
    }
}

FL_TEST_CASE("Wave10 - canRepresentTiming rejects infeasible chipsets") {
    FL_SUBCASE("TM1829-1600kHz (baud too high: 8.3 Mbps)") {
        ChipsetTimingConfig timing(100, 300, 200, 500);
        FL_CHECK_FALSE(canRepresentTiming(timing));
    }

    FL_SUBCASE("LPD1886-1250kHz (baud too high: 6.25 Mbps)") {
        ChipsetTimingConfig timing(200, 400, 200, 0);
        FL_CHECK_FALSE(canRepresentTiming(timing));
    }

    FL_SUBCASE("Zero period") {
        ChipsetTimingConfig timing(0, 0, 0, 0);
        FL_CHECK_FALSE(canRepresentTiming(timing));
    }
}

FL_TEST_CASE("Wave10 - canRepresentTiming baud rate boundary") {
    FL_SUBCASE("Exactly at 5.0 Mbps limit") {
        // period = 5e9 / 5e6 = 1000ns → baud = 5,000,000
        ChipsetTimingConfig timing(200, 400, 400, 0);
        u32 baud = Wave10Lut::computeBaudRate(timing);
        FL_CHECK_EQ(baud, 5000000u);
        FL_CHECK(canRepresentTiming(timing));
    }
}

//=============================================================================
// Test Suite: Wave10Lut::computeBaudRate
//=============================================================================

FL_TEST_CASE("Wave10 - computeBaudRate") {
    FL_SUBCASE("WS2812: 4.0 Mbps") {
        ChipsetTimingConfig timing(250, 625, 375, 280);
        FL_CHECK_EQ(Wave10Lut::computeBaudRate(timing), 4000000u);
    }

    FL_SUBCASE("WS2811-400: 2.0 Mbps") {
        ChipsetTimingConfig timing(500, 700, 1300, 280);
        FL_CHECK_EQ(Wave10Lut::computeBaudRate(timing), 2000000u);
    }

    FL_SUBCASE("Zero period returns 0") {
        ChipsetTimingConfig timing(0, 0, 0, 0);
        FL_CHECK_EQ(Wave10Lut::computeBaudRate(timing), 0u);
    }
}

} // FL_TEST_FILE
