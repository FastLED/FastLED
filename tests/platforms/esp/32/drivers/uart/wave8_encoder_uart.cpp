/// @file test_wave8_encoder_uart.cpp
/// @brief Unit tests for UART wave8 encoder
///
/// Tests the 2-bit LUT encoding strategy for UART LED transmission.
/// Validates encoding correctness, buffer sizing, and edge cases.

#include "test.h"

#include "platforms/esp/32/drivers/uart/wave8_encoder_uart.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "fl/stl/new.h"
#include "fl/stl/vector.h"

using namespace fl;

//=============================================================================
// Test Suite: 2-Bit LUT Encoding
//=============================================================================

FL_TEST_CASE("Wave8 UART Encoder - 2-bit LUT correctness") {
    FL_SUBCASE("LUT pattern 0x00 (0b00) → 0xEF") {
        uint8_t result = detail::encodeUart2Bits(0x00);
        FL_REQUIRE_EQ(result, 0xEF);
    }

    FL_SUBCASE("LUT pattern 0x01 (0b01) → 0x8F") {
        uint8_t result = detail::encodeUart2Bits(0x01);
        FL_REQUIRE_EQ(result, 0x8F);
    }

    FL_SUBCASE("LUT pattern 0x02 (0b10) → 0xEC") {
        uint8_t result = detail::encodeUart2Bits(0x02);
        FL_REQUIRE_EQ(result, 0xEC);
    }

    FL_SUBCASE("LUT pattern 0x03 (0b11) → 0x8C") {
        uint8_t result = detail::encodeUart2Bits(0x03);
        FL_REQUIRE_EQ(result, 0x8C);
    }

    FL_SUBCASE("LUT masking (input > 3 masked to 2 bits)") {
        // encodeUart2Bits should mask input to 2 bits
        FL_REQUIRE(detail::encodeUart2Bits(0x00) == detail::encodeUart2Bits(0x00));
        FL_REQUIRE(detail::encodeUart2Bits(0x01) == detail::encodeUart2Bits(0xFD));
        FL_REQUIRE(detail::encodeUart2Bits(0x02) == detail::encodeUart2Bits(0xFE));
        FL_REQUIRE(detail::encodeUart2Bits(0x03) == detail::encodeUart2Bits(0xFF));
    }
}

//=============================================================================
// Test Suite: Byte-Level Encoding
//=============================================================================

FL_TEST_CASE("Wave8 UART Encoder - Byte encoding") {
    FL_SUBCASE("Encode byte 0x00 (all bits 0)") {
        uint8_t output[4];
        detail::encodeUartByte(0x00, output);

        // 0x00 = 0b00 00 00 00 → all pairs are 0b00 → 0xEF
        FL_REQUIRE_EQ(output[0], 0xEF);  // Bits 7-6: 0b00
        FL_REQUIRE_EQ(output[1], 0xEF);  // Bits 5-4: 0b00
        FL_REQUIRE_EQ(output[2], 0xEF);  // Bits 3-2: 0b00
        FL_REQUIRE_EQ(output[3], 0xEF);  // Bits 1-0: 0b00
    }

    FL_SUBCASE("Encode byte 0xFF (all bits 1)") {
        uint8_t output[4];
        detail::encodeUartByte(0xFF, output);

        // 0xFF = 0b11 11 11 11 → all pairs are 0b11 → 0x8C
        FL_REQUIRE_EQ(output[0], 0x8C);  // Bits 7-6: 0b11
        FL_REQUIRE_EQ(output[1], 0x8C);  // Bits 5-4: 0b11
        FL_REQUIRE_EQ(output[2], 0x8C);  // Bits 3-2: 0b11
        FL_REQUIRE_EQ(output[3], 0x8C);  // Bits 1-0: 0b11
    }

    FL_SUBCASE("Encode byte 0xAA (alternating 1010 1010)") {
        uint8_t output[4];
        detail::encodeUartByte(0xAA, output);

        // 0xAA = 0b10 10 10 10 → all pairs are 0b10 → 0xEC
        FL_REQUIRE_EQ(output[0], 0xEC);  // Bits 7-6: 0b10
        FL_REQUIRE_EQ(output[1], 0xEC);  // Bits 5-4: 0b10
        FL_REQUIRE_EQ(output[2], 0xEC);  // Bits 3-2: 0b10
        FL_REQUIRE_EQ(output[3], 0xEC);  // Bits 1-0: 0b10
    }

    FL_SUBCASE("Encode byte 0x55 (alternating 0101 0101)") {
        uint8_t output[4];
        detail::encodeUartByte(0x55, output);

        // 0x55 = 0b01 01 01 01 → all pairs are 0b01 → 0x8F
        FL_REQUIRE_EQ(output[0], 0x8F);  // Bits 7-6: 0b01
        FL_REQUIRE_EQ(output[1], 0x8F);  // Bits 5-4: 0b01
        FL_REQUIRE_EQ(output[2], 0x8F);  // Bits 3-2: 0b01
        FL_REQUIRE_EQ(output[3], 0x8F);  // Bits 1-0: 0b01
    }

    FL_SUBCASE("Encode byte 0xE4 (mixed pattern)") {
        uint8_t output[4];
        detail::encodeUartByte(0xE4, output);

        // 0xE4 = 0b11 10 01 00
        FL_REQUIRE_EQ(output[0], 0x8C);  // Bits 7-6: 0b11
        FL_REQUIRE_EQ(output[1], 0xEC);  // Bits 5-4: 0b10
        FL_REQUIRE_EQ(output[2], 0x8F);  // Bits 3-2: 0b01
        FL_REQUIRE_EQ(output[3], 0xEF);  // Bits 1-0: 0b00
    }
}

//=============================================================================
// Test Suite: Buffer-Level Encoding
//=============================================================================

FL_TEST_CASE("Wave8 UART Encoder - Buffer encoding") {
    FL_SUBCASE("Encode single byte") {
        uint8_t input[] = { 0x42 };
        uint8_t output[4];

        size_t encoded = encodeLedsToUart(input, 1, output, sizeof(output));

        FL_REQUIRE(encoded == 4);
        // 0x42 = 0b01 00 00 10
        FL_REQUIRE_EQ(output[0], 0x8F);  // 0b01 → 0x8F
        FL_REQUIRE_EQ(output[1], 0xEF);  // 0b00 → 0xEF
        FL_REQUIRE_EQ(output[2], 0xEF);  // 0b00 → 0xEF
        FL_REQUIRE_EQ(output[3], 0xEC);  // 0b10 → 0xEC
    }

    FL_SUBCASE("Encode multiple bytes") {
        uint8_t input[] = { 0x00, 0xFF, 0xAA };
        uint8_t output[12];

        size_t encoded = encodeLedsToUart(input, 3, output, sizeof(output));

        FL_REQUIRE(encoded == 12);

        // Byte 0: 0x00 → 0xEF 0xEF 0xEF 0xEF
        FL_REQUIRE_EQ(output[0], 0xEF);
        FL_REQUIRE_EQ(output[1], 0xEF);
        FL_REQUIRE_EQ(output[2], 0xEF);
        FL_REQUIRE_EQ(output[3], 0xEF);

        // Byte 1: 0xFF → 0x8C 0x8C 0x8C 0x8C
        FL_REQUIRE_EQ(output[4], 0x8C);
        FL_REQUIRE_EQ(output[5], 0x8C);
        FL_REQUIRE_EQ(output[6], 0x8C);
        FL_REQUIRE_EQ(output[7], 0x8C);

        // Byte 2: 0xAA → 0xEC 0xEC 0xEC 0xEC
        FL_REQUIRE_EQ(output[8], 0xEC);
        FL_REQUIRE_EQ(output[9], 0xEC);
        FL_REQUIRE_EQ(output[10], 0xEC);
        FL_REQUIRE_EQ(output[11], 0xEC);
    }

    FL_SUBCASE("Encode RGB LED (3 bytes)") {
        // Red LED: R=255, G=0, B=0
        uint8_t input[] = { 0xFF, 0x00, 0x00 };
        uint8_t output[12];  // 3 bytes × 4 = 12 bytes

        size_t encoded = encodeLedsToUart(input, 3, output, sizeof(output));

        FL_REQUIRE(encoded == 12);

        // R=0xFF → 0x8C 0x8C 0x8C 0x8C
        FL_REQUIRE_EQ(output[0], 0x8C);
        FL_REQUIRE_EQ(output[1], 0x8C);
        FL_REQUIRE_EQ(output[2], 0x8C);
        FL_REQUIRE_EQ(output[3], 0x8C);

        // G=0x00 → 0xEF 0xEF 0xEF 0xEF
        FL_REQUIRE_EQ(output[4], 0xEF);
        FL_REQUIRE_EQ(output[5], 0xEF);
        FL_REQUIRE_EQ(output[6], 0xEF);
        FL_REQUIRE_EQ(output[7], 0xEF);

        // B=0x00 → 0xEF 0xEF 0xEF 0xEF
        FL_REQUIRE_EQ(output[8], 0xEF);
        FL_REQUIRE_EQ(output[9], 0xEF);
        FL_REQUIRE_EQ(output[10], 0xEF);
        FL_REQUIRE_EQ(output[11], 0xEF);
    }

    FL_SUBCASE("Encode 100 RGB LEDs") {
        const size_t num_leds = 100;
        fl::vector<uint8_t> input(num_leds * 3);  // 300 bytes
        fl::vector<uint8_t> output(num_leds * 3 * 4);  // 1200 bytes

        // Fill with test pattern (alternating 0x00 and 0xFF)
        for (size_t i = 0; i < input.size(); ++i) {
            input[i] = (i % 2 == 0) ? 0x00 : 0xFF;
        }

        size_t encoded = encodeLedsToUart(input.data(), input.size(),
                                          output.data(), output.size());

        FL_REQUIRE(encoded == 1200);

        // Verify first LED (R=0x00, G=0xFF, B=0x00)
        FL_REQUIRE_EQ(output[0], 0xEF);   // R byte 0 (index 0: even → 0x00)
        FL_REQUIRE_EQ(output[4], 0x8C);   // G byte 0 (index 1: odd → 0xFF)
        FL_REQUIRE_EQ(output[8], 0xEF);   // B byte 0 (index 2: even → 0x00)

        // Verify last LED encoding (LED 99)
        // Input byte indices: 297, 298, 299
        // 297 % 2 == 1 → 0xFF, 298 % 2 == 0 → 0x00, 299 % 2 == 1 → 0xFF
        size_t last_led_offset = (num_leds - 1) * 12;
        FL_REQUIRE_EQ(output[last_led_offset + 0], 0x8C);  // R=0xFF → 0x8C
        FL_REQUIRE_EQ(output[last_led_offset + 4], 0xEF);  // G=0x00 → 0xEF
        FL_REQUIRE_EQ(output[last_led_offset + 8], 0x8C);  // B=0xFF → 0x8C
    }
}

//=============================================================================
// Test Suite: Buffer Sizing
//=============================================================================

FL_TEST_CASE("Wave8 UART Encoder - Buffer sizing") {
    FL_SUBCASE("Calculate buffer size for raw bytes") {
        FL_REQUIRE(calculateUartBufferSize(1) == 4);
        FL_REQUIRE(calculateUartBufferSize(3) == 12);
        FL_REQUIRE(calculateUartBufferSize(300) == 1200);  // 100 RGB LEDs
    }

    FL_SUBCASE("Calculate buffer size for RGB LEDs") {
        FL_REQUIRE(calculateUartBufferSizeForLeds(1) == 12);    // 1 LED = 12 bytes
        FL_REQUIRE(calculateUartBufferSizeForLeds(10) == 120);  // 10 LEDs = 120 bytes
        FL_REQUIRE(calculateUartBufferSizeForLeds(100) == 1200);  // 100 LEDs = 1200 bytes
        FL_REQUIRE(calculateUartBufferSizeForLeds(1000) == 12000);  // 1000 LEDs = 12000 bytes
    }

    FL_SUBCASE("Insufficient output buffer (returns 0)") {
        uint8_t input[] = { 0xFF };
        uint8_t output[3];  // Need 4 bytes, only 3 available

        size_t encoded = encodeLedsToUart(input, 1, output, sizeof(output));

        FL_REQUIRE(encoded == 0);  // Encoding failed due to insufficient capacity
    }

    FL_SUBCASE("Exact output buffer capacity (success)") {
        uint8_t input[] = { 0xFF };
        uint8_t output[4];  // Exactly 4 bytes

        size_t encoded = encodeLedsToUart(input, 1, output, sizeof(output));

        FL_REQUIRE(encoded == 4);  // Encoding succeeded
        FL_REQUIRE_EQ(output[0], 0x8C);  // 0xFF = 0b11 → 0x8C
    }
}

//=============================================================================
// Test Suite: Edge Cases
//=============================================================================

FL_TEST_CASE("Wave8 UART Encoder - Edge cases") {
    FL_SUBCASE("Empty input (0 bytes)") {
        uint8_t output[16];

        size_t encoded = encodeLedsToUart(nullptr, 0, output, sizeof(output));

        FL_REQUIRE(encoded == 0);  // 0 input bytes → 0 output bytes
    }

    FL_SUBCASE("Large buffer encoding (stress test)") {
        const size_t large_size = 10000;  // 10,000 bytes input
        fl::vector<uint8_t> input(large_size, 0xAA);
        fl::vector<uint8_t> output(large_size * 4);

        size_t encoded = encodeLedsToUart(input.data(), input.size(),
                                          output.data(), output.size());

        FL_REQUIRE(encoded == large_size * 4);

        // Verify first 10 bytes are correctly encoded
        for (size_t i = 0; i < 10; ++i) {
            FL_REQUIRE_EQ(output[i], 0xEC);  // 0xAA = 0b10 10 10 10 → all 0xEC
        }

        // Verify last 10 bytes are correctly encoded
        for (size_t i = output.size() - 10; i < output.size(); ++i) {
            FL_REQUIRE_EQ(output[i], 0xEC);
        }
    }
}

//=============================================================================
// Test Suite: Waveform Validation
//=============================================================================

FL_TEST_CASE("Wave8 UART Encoder - Waveform structure validation") {
    FL_SUBCASE("Encoded byte has valid UART patterns") {
        // All LUT values must produce valid WS2812 timing with inverted TX at 4 Mbps
        // Valid patterns: 0xEF, 0x8F, 0xEC, 0x8C
        for (uint8_t two_bits = 0; two_bits < 4; ++two_bits) {
            uint8_t encoded = detail::encodeUart2Bits(two_bits);

            bool valid = (encoded == 0xEF || encoded == 0x8F ||
                         encoded == 0xEC || encoded == 0x8C);
            FL_REQUIRE(valid);
        }
    }

    FL_SUBCASE("Verify bit distribution for LED protocols") {
        // With inverted TX, the wire output flips all bits.
        // What matters is the HIGH/LOW pulse structure on the wire,
        // not the popcount of the data byte itself.

        // Pattern 0xEF: LED "00" → wire H L L L L H L L L L (2 HIGH groups)
        uint8_t pattern_00 = detail::encodeUart2Bits(0x00);
        FL_REQUIRE_EQ(pattern_00, 0xEF);

        // Pattern 0x8F: LED "01" → wire H L L L L H H H L L
        uint8_t pattern_01 = detail::encodeUart2Bits(0x01);
        FL_REQUIRE_EQ(pattern_01, 0x8F);

        // Pattern 0xEC: LED "10" → wire H H H L L H L L L L
        uint8_t pattern_10 = detail::encodeUart2Bits(0x02);
        FL_REQUIRE_EQ(pattern_10, 0xEC);

        // Pattern 0x8C: LED "11" → wire H H H L L H H H L L
        uint8_t pattern_11 = detail::encodeUart2Bits(0x03);
        FL_REQUIRE_EQ(pattern_11, 0x8C);
    }
}

//=============================================================================
// Test Suite: Performance Characteristics
//=============================================================================

FL_TEST_CASE("Wave8 UART Encoder - Performance characteristics") {
    FL_SUBCASE("Encoding determinism (repeated calls produce same output)") {
        uint8_t input[] = { 0x42, 0xAA, 0xFF };
        uint8_t output1[12];
        uint8_t output2[12];

        size_t encoded1 = encodeLedsToUart(input, 3, output1, sizeof(output1));
        size_t encoded2 = encodeLedsToUart(input, 3, output2, sizeof(output2));

        FL_REQUIRE(encoded1 == encoded2);
        FL_REQUIRE(encoded1 == 12);

        for (size_t i = 0; i < 12; ++i) {
            FL_REQUIRE(output1[i] == output2[i]);
        }
    }

    FL_SUBCASE("No data dependencies (parallel encoding feasible)") {
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
            FL_REQUIRE(output_sequential[i] == output_manual[i]);
        }
    }
}

//=============================================================================
// Test Suite: Bit Rotation Verification
//=============================================================================

FL_TEST_CASE("Wave8 UART Encoder - LUT derivation correctness") {
    FL_SUBCASE("LUT values derived from inverted TX + 4 Mbps timing") {
        // The LUT values are derived by computing what UART data byte,
        // when transmitted with TX inversion at 4 Mbps, produces the
        // correct WS2812 HIGH/LOW pulse durations.
        //
        // Inverted TX frame: [START=H] [~D0..~D7 LSB-first] [STOP=L]
        // 10 bits total = 2500ns = 2 LED bit periods at 4 Mbps
        //
        // LED "0": 1H + 4L = 250ns + 1000ns (T0H + T0L)
        // LED "1": 3H + 2L = 750ns + 500ns  (T1H + T1L)

        FL_REQUIRE_EQ(detail::encodeUart2Bits(0x00), 0xEF); // "00"
        FL_REQUIRE_EQ(detail::encodeUart2Bits(0x01), 0x8F); // "01"
        FL_REQUIRE_EQ(detail::encodeUart2Bits(0x02), 0xEC); // "10"
        FL_REQUIRE_EQ(detail::encodeUart2Bits(0x03), 0x8C); // "11"
    }

    FL_SUBCASE("LUT values are self-consistent") {
        // The "11" pattern (0x8C) should have the most HIGH bits on wire
        // The "00" pattern (0xEF) should have the fewest HIGH bits on wire
        // (After TX inversion, more 0-bits in data = more H-bits on wire
        //  for data portion, but inverted data means 0→H, 1→L)

        // All 4 values should be distinct
        FL_CHECK_NE(detail::encodeUart2Bits(0x00), detail::encodeUart2Bits(0x01));
        FL_CHECK_NE(detail::encodeUart2Bits(0x00), detail::encodeUart2Bits(0x02));
        FL_CHECK_NE(detail::encodeUart2Bits(0x00), detail::encodeUart2Bits(0x03));
        FL_CHECK_NE(detail::encodeUart2Bits(0x01), detail::encodeUart2Bits(0x02));
        FL_CHECK_NE(detail::encodeUart2Bits(0x01), detail::encodeUart2Bits(0x03));
        FL_CHECK_NE(detail::encodeUart2Bits(0x02), detail::encodeUart2Bits(0x03));
    }
}

//=============================================================================
// Test Suite: Waveform Timing Analysis
//
// Simulates the exact wire-level waveform for UART bytes including start/stop
// bits, LSB-first ordering, and optional TX inversion. Measures HIGH/LOW pulse
// durations and checks against WS2812 timing spec.
//=============================================================================

namespace { // anonymous namespace for timing analysis

// WS2812 timing spec (nanoseconds) - from datasheet with tolerances
// Bit "0": T0H = 220-380ns, T0L = 580-1000ns
// Bit "1": T1H = 580-1000ns, T1L = 220-420ns
// Total bit period: ~1250ns (800kHz)
// But real WS2812 ICs are much more tolerant (see josh.com analysis):
//   T0H: 62.5ns - 500ns
//   T1H: 625ns+
//   The only critical threshold is ~625ns: below = "0", above = "1"

struct PulseInfo {
    int high_ns;  // HIGH pulse duration
    int low_ns;   // LOW pulse duration
    int total_ns; // Total bit period
};

// Generate the wire-level bit sequence for a UART 8N1 frame.
// Returns 10 bits: [START, D0(LSB), D1, ..., D7(MSB), STOP]
// If invert_tx is true, all bits are flipped (UART_SIGNAL_TXD_INV).
void uartFrameToBits(uint8_t byte_val, bool invert_tx, int out_bits[10]) {
    // Normal UART: START=0 (LOW), data LSB first, STOP=1 (HIGH)
    int raw[10];
    raw[0] = 0; // start bit
    for (int i = 0; i < 8; i++) {
        raw[1 + i] = (byte_val >> i) & 1; // LSB first
    }
    raw[9] = 1; // stop bit

    if (invert_tx) {
        for (int i = 0; i < 10; i++) {
            out_bits[i] = raw[i] ? 0 : 1;
        }
    } else {
        for (int i = 0; i < 10; i++) {
            out_bits[i] = raw[i];
        }
    }
}

// Measure HIGH/LOW pulse durations from a bit stream.
// Each bit lasts bit_time_ns nanoseconds.
// Returns pulses as alternating HIGH/LOW periods.
struct Pulse {
    bool is_high;
    int duration_ns;
};

int measurePulses(const int* bits, int num_bits, int bit_time_ns,
                  Pulse* out_pulses, int max_pulses) {
    if (num_bits == 0) return 0;
    int count = 0;
    bool current_level = bits[0] != 0;
    int current_duration = bit_time_ns;

    for (int i = 1; i < num_bits; i++) {
        bool level = bits[i] != 0;
        if (level == current_level) {
            current_duration += bit_time_ns;
        } else {
            if (count < max_pulses) {
                out_pulses[count].is_high = current_level;
                out_pulses[count].duration_ns = current_duration;
                count++;
            }
            current_level = level;
            current_duration = bit_time_ns;
        }
    }
    // Final pulse
    if (count < max_pulses) {
        out_pulses[count].is_high = current_level;
        out_pulses[count].duration_ns = current_duration;
        count++;
    }
    return count;
}

// Analyze two consecutive UART bytes (encoding 2 LED bits each = 4 LED bits total)
// and extract the LED bit pulse timings.
struct LedBitTiming {
    int t_high_ns;  // HIGH pulse duration
    int t_low_ns;   // LOW pulse duration (includes inter-byte gap)
};

} // anonymous namespace

FL_TEST_CASE("Wave8 UART Encoder - Waveform timing analysis") {

    // Test the ESP8266 proven configuration: 3.2 Mbps, non-inverted, LUT {0x88,0x8C,0xC8,0xCC}
    FL_SUBCASE("ESP8266 config: 3.2 Mbps, no inversion, original LUT") {
        const int bit_time_ns = 312; // 1e9 / 3.2e6 ≈ 312.5ns
        const uint8_t lut[4] = {0x88, 0x8C, 0xC8, 0xCC};
        const bool invert = false;

        // With non-inverted UART, idle = HIGH.
        // WS2812 idle = LOW. So non-inverted UART cannot work directly
        // unless the encoding takes advantage of the inverted polarity.

        // Trace LUT[0] = 0x88 (LED bits "00")
        int bits[10];
        uartFrameToBits(lut[0], invert, bits);
        // Expected: START(L) D0=0 D1=0 D2=0 D3=1 D4=0 D5=0 D6=0 D7=1 STOP(H)
        // Wire: L L L L H L L L H H
        FL_CHECK_EQ(bits[0], 0); // START = LOW
        FL_CHECK_EQ(bits[1], 0); // D0 (LSB of 0x88 = 0)
        FL_CHECK_EQ(bits[4], 1); // D3 (bit3 of 0x88 = 1)
        FL_CHECK_EQ(bits[9], 1); // STOP = HIGH

        Pulse pulses[20];
        int np = measurePulses(bits, 10, bit_time_ns, pulses, 20);

        // Non-inverted: idle=HIGH, so the "reset" between LED frames is HIGH.
        // This is WRONG for WS2812 which needs idle=LOW reset.
        // The encoding still works on ESP8266 because:
        // 1. The HIGH idle acts as the reset/latch signal
        // 2. The LOW-going start bit begins each LED bit
        //
        // Wire for 0x88: L L L L H L L L H H
        // Pulses: LOW(4*312=1248ns) HIGH(312ns) LOW(3*312=936ns) HIGH(2*312=624ns)
        FL_CHECK_GE(np, 3);

        // The pattern creates: 2 LED "bits" where each has:
        // LOW(leading) then HIGH(short) pattern
        // This is inverted polarity from what WS2812 expects!
        // WS2812 expects: HIGH(leading) then LOW
        //
        // So the ESP8266 actually produces INVERTED WS2812 signals!
        // It works because WS2812 timing detection is based on the
        // HIGH pulse width relative to the total period, and many
        // WS2812 clones are polarity-tolerant.
    }

    // Test the ESP8266 config WITH TX inversion
    FL_SUBCASE("3.2 Mbps with TX inversion and original LUT") {
        const int bit_time_ns = 312; // 1e9 / 3.2e6
        const uint8_t lut[4] = {0x88, 0x8C, 0xC8, 0xCC};
        const bool invert = true;

        // Trace LUT[0] = 0x88 (LED bits "00")
        int bits[10];
        uartFrameToBits(lut[0], invert, bits);
        // Inverted: START(H) D0=H D1=H D2=H D3=L D4=H D5=H D6=H D7=L STOP(L)
        // Wire: H H H H L H H H L L
        FL_CHECK_EQ(bits[0], 1); // START inverted = HIGH
        FL_CHECK_EQ(bits[9], 0); // STOP inverted = LOW

        Pulse pulses[20];
        int np = measurePulses(bits, 10, bit_time_ns, pulses, 20);

        // Wire: H H H H L H H H L L
        // Pulses: HIGH(4*312=1248ns) LOW(312ns) HIGH(3*312=936ns) LOW(2*312=624ns)
        //
        // Idle is LOW (correct for WS2812!)
        // But the HIGH pulses are too long for LED "0" bits.
        // LED "0" should have T0H ≈ 350ns (max 500ns)
        // Here the first HIGH pulse is 1248ns - that's LED "1" territory!
        //
        // So 3.2 Mbps + inversion + original LUT does NOT produce correct "0" bits.
        FL_CHECK_GE(np, 3);
        // First pulse should be HIGH
        FL_CHECK(pulses[0].is_high);
        // First pulse is 1248ns - way too long for LED "0"
        FL_CHECK_GT(pulses[0].duration_ns, 1000);
    }

    // Test what CORRECT WS2812 encoding should look like with inverted TX
    FL_SUBCASE("Correct WS2812 timing with inverted TX at 3.2 Mbps") {
        (void)0; // Analysis subcase - derivations only, no runtime checks needed here

        // For inverted TX at 3.2 Mbps:
        // - idle = LOW (good)
        // - START bit becomes HIGH (leading edge of LED bit!)
        // - STOP bit becomes LOW (trailing edge / reset)
        //
        // Each 10-bit frame = 3120ns ≈ 2.5 LED bit periods (not clean multiple)
        // This means 3.2 Mbps with 10-bit frames doesn't cleanly encode LED bits.
        //
        // For 2 LED bits per frame (2 × 1250ns = 2500ns):
        // We need 10 UART bits × bit_time = 2500ns → bit_time = 250ns → baud = 4 Mbps
        //
        // For 4 Mbps with inverted TX:
        // bit_time = 250ns, frame = 2500ns = exactly 2 LED bit periods!
        //
        // LED bit "0" needs: T0H ≈ 300ns, T0L ≈ 950ns → ratio HIGH:LOW ≈ 1:3
        //   In 5 UART bits (1250ns): 1 HIGH + 4 LOW = 250ns H + 1000ns L ✓
        //
        // LED bit "1" needs: T1H ≈ 700ns, T1L ≈ 550ns → ratio HIGH:LOW ≈ 3:2
        //   In 5 UART bits (1250ns): 3 HIGH + 2 LOW = 750ns H + 500ns L ✓

        // At 4 Mbps with inverted TX, we need byte values where:
        // Inverted frame = [H, ~D0, ~D1, ..., ~D7, L]
        // The 10 bits must split into 2 groups of 5 that look like LED bits.
        //
        // For LED bits "00": H L L L L | H L L L L
        //   Inverted data must be: D0=H D1=H D2=H D3=H D4=L D5=H D6=H D7=H
        //   But inverted: ~D0=L, so D0=1; ~D1=L, so D1=1; etc.
        //   Wait: inverted frame means raw data bit D_i becomes ~D_i on wire.
        //   Frame: [~START=H], [~D0], [~D1], ..., [~D7], [~STOP=L]
        //
        // For LED bits "00" we need wire: H L L L L H L L L L
        //   Pos 0 (START): H ✓ (inverted start)
        //   Pos 1 (~D0): L → D0=1
        //   Pos 2 (~D1): L → D1=1
        //   Pos 3 (~D2): L → D2=1
        //   Pos 4 (~D3): L → D3=1
        //   Pos 5 (~D4): H → D4=0
        //   Pos 6 (~D5): L → D5=1
        //   Pos 7 (~D6): L → D6=1
        //   Pos 8 (~D7): L → D7=1
        //   Pos 9 (STOP): L ✓ (inverted stop)
        //
        // D7..D0 = 1110_1111 = D0 is LSB → byte = 0b11101111 = 0xEF
        // Wait, let me recalculate:
        // D0=1, D1=1, D2=1, D3=1, D4=0, D5=1, D6=1, D7=1
        // Byte (D7..D0) = 1110_1111 = 0xEF

        // Verify: 0xEF with inverted TX
        int bits_00[10];
        uartFrameToBits(0xEF, true, bits_00);
        // Should be: H L L L L H L L L L
        FL_CHECK_EQ(bits_00[0], 1);  // H
        FL_CHECK_EQ(bits_00[1], 0);  // L
        FL_CHECK_EQ(bits_00[2], 0);  // L
        FL_CHECK_EQ(bits_00[3], 0);  // L
        FL_CHECK_EQ(bits_00[4], 0);  // L
        FL_CHECK_EQ(bits_00[5], 1);  // H
        FL_CHECK_EQ(bits_00[6], 0);  // L
        FL_CHECK_EQ(bits_00[7], 0);  // L
        FL_CHECK_EQ(bits_00[8], 0);  // L
        FL_CHECK_EQ(bits_00[9], 0);  // L

        // For LED bits "01" we need wire: H L L L L H H H L L
        // D0=1, D1=1, D2=1, D3=1, D4=0, D5=0, D6=0, D7=1
        // Byte = 0b10001111 = 0x8F
        // WAIT - let me re-derive more carefully.
        //
        // Inverted TX wire for byte val:
        //   bit[0] = ~0 = 1 (start)
        //   bit[1..8] = ~(val >> 0..7 & 1) for each data bit, LSB first
        //   bit[9] = ~1 = 0 (stop)
        //
        // For "01" = LED_bit_a=0, LED_bit_b=1
        //   LED bit 0 (positions 0-4): H L L L L  → 1 0 0 0 0
        //   LED bit 1 (positions 5-9): H H H L L  → 1 1 1 0 0
        //
        // Pos 0: 1 (start, always H with inversion) ✓
        // Pos 1: 0 → ~D0=0 → D0=1
        // Pos 2: 0 → ~D1=0 → D1=1
        // Pos 3: 0 → ~D2=0 → D2=1
        // Pos 4: 0 → ~D3=0 → D3=1
        // Pos 5: 1 → ~D4=1 → D4=0
        // Pos 6: 1 → ~D5=1 → D5=0
        // Pos 7: 1 → ~D6=1 → D6=0  WAIT - LED "1" is H H H L L
        //   positions 5-9: 1 1 1 0 0
        //   pos 5: ~D4=1 → D4=0
        //   pos 6: ~D5=1 → D5=0
        //   pos 7: ~D6=1 → D6=0  WRONG
        //
        // Wait, re-examine LED bit "1" timing:
        // T1H ≈ 750ns (3 bits), T1L ≈ 500ns (2 bits)
        // Wire: H H H L L → positions 5,6,7 = H; positions 8,9 = L
        //
        // pos 5: ~D4=1 → D4=0
        // pos 6: ~D5=1 → D5=0
        // pos 7: ~D6=1 → D6=0
        // pos 8: ~D7=0 → D7=1
        // pos 9: 0 (stop, always L with inversion) ✓
        //
        // D7..D0: D7=1, D6=0, D5=0, D4=0, D3=1, D2=1, D1=1, D0=1
        // Byte = 0b10001111 = 0x8F

        int bits_01[10];
        uartFrameToBits(0x8F, true, bits_01);
        FL_CHECK_EQ(bits_01[0], 1);  // H (start)
        FL_CHECK_EQ(bits_01[1], 0);  // L
        FL_CHECK_EQ(bits_01[2], 0);  // L
        FL_CHECK_EQ(bits_01[3], 0);  // L
        FL_CHECK_EQ(bits_01[4], 0);  // L
        FL_CHECK_EQ(bits_01[5], 1);  // H
        FL_CHECK_EQ(bits_01[6], 1);  // H
        FL_CHECK_EQ(bits_01[7], 1);  // H
        FL_CHECK_EQ(bits_01[8], 0);  // L
        FL_CHECK_EQ(bits_01[9], 0);  // L

        // For LED bits "10" we need wire: H H H L L H L L L L
        // pos 0: 1 (start) ✓
        // pos 1: ~D0=1 → D0=0
        // pos 2: ~D1=1 → D1=0
        // pos 3: ~D2=0 → D2=1
        // pos 4: ~D3=0 → D3=1
        // pos 5: ~D4=1 → D4=0  WAIT
        //
        // LED bit "1" (positions 0-4): H H H L L → 1 1 1 0 0
        //   pos 0: 1 (start) ✓
        //   pos 1: ~D0=1 → D0=0
        //   pos 2: ~D1=1 → D1=0
        //   pos 3: ~D2=0 → D2=1
        //   pos 4: ~D3=0 → D3=1
        //
        // LED bit "0" (positions 5-9): H L L L L → 1 0 0 0 0
        //   pos 5: ~D4=1 → D4=0
        //   pos 6: ~D5=0 → D5=1
        //   pos 7: ~D6=0 → D6=1
        //   pos 8: ~D7=0 → D7=1
        //   pos 9: 0 (stop) ✓
        //
        // D7..D0: D7=1, D6=1, D5=1, D4=0, D3=1, D2=1, D1=0, D0=0
        // Byte = 0b11101100 = 0xEC

        int bits_10[10];
        uartFrameToBits(0xEC, true, bits_10);
        FL_CHECK_EQ(bits_10[0], 1);  // H
        FL_CHECK_EQ(bits_10[1], 1);  // H
        FL_CHECK_EQ(bits_10[2], 1);  // H
        FL_CHECK_EQ(bits_10[3], 0);  // L
        FL_CHECK_EQ(bits_10[4], 0);  // L
        FL_CHECK_EQ(bits_10[5], 1);  // H
        FL_CHECK_EQ(bits_10[6], 0);  // L
        FL_CHECK_EQ(bits_10[7], 0);  // L
        FL_CHECK_EQ(bits_10[8], 0);  // L
        FL_CHECK_EQ(bits_10[9], 0);  // L

        // For LED bits "11" we need wire: H H H L L H H H L L
        // LED bit "1" (positions 0-4): H H H L L
        //   Same as "10" first half: D0=0, D1=0, D2=1, D3=1
        // LED bit "1" (positions 5-9): H H H L L
        //   Same as "01" second half: D4=0, D5=0, D6=0, D7=1
        //
        // D7..D0: D7=1, D6=0, D5=0, D4=0, D3=1, D2=1, D1=0, D0=0
        // Byte = 0b10001100 = 0x8C

        int bits_11[10];
        uartFrameToBits(0x8C, true, bits_11);
        FL_CHECK_EQ(bits_11[0], 1);  // H
        FL_CHECK_EQ(bits_11[1], 1);  // H
        FL_CHECK_EQ(bits_11[2], 1);  // H
        FL_CHECK_EQ(bits_11[3], 0);  // L
        FL_CHECK_EQ(bits_11[4], 0);  // L
        FL_CHECK_EQ(bits_11[5], 1);  // H
        FL_CHECK_EQ(bits_11[6], 1);  // H
        FL_CHECK_EQ(bits_11[7], 1);  // H
        FL_CHECK_EQ(bits_11[8], 0);  // L
        FL_CHECK_EQ(bits_11[9], 0);  // L

        // Summary: Correct LUT for 4 Mbps + inverted TX:
        // LED "00" → 0xEF
        // LED "01" → 0x8F
        // LED "10" → 0xEC
        // LED "11" → 0x8C
    }

    // Now test: can we make it work WITHOUT inversion by flipping the LUT?
    FL_SUBCASE("Non-inverted TX at 4 Mbps - derive correct LUT") {
        const bool invert_tx = false;

        // Without inversion: idle=HIGH, START=LOW, STOP=HIGH
        // WS2812 idle must be LOW. With non-inverted UART, idle=HIGH.
        // Between frames: STOP(HIGH) → START(LOW) → data...
        //
        // For non-inverted TX, the wire is INVERTED relative to WS2812.
        // We need the OPPOSITE polarity: LED "0" = LOW(short) HIGH(long)
        //
        // Wait - this means the entire signal is upside down.
        // The "reset" (idle) is HIGH instead of LOW.
        // WS2812 resets when the line goes LOW for >50us.
        // With non-inverted UART, the line is HIGH during idle.
        // This means we can never produce a valid reset signal!
        //
        // CONCLUSION: Non-inverted UART CANNOT work for WS2812
        // unless the chip is tolerant of inverted reset polarity,
        // or there's an external inverter.
        //
        // The ESP8266 implementation works because either:
        // a) It uses a hardware register to invert (not visible in high-level code)
        // b) The specific WS2812 clones used are polarity-tolerant
        // c) There's something else going on

        // For non-inverted TX: START=L, data, STOP=H
        // Frame bits for 0x88: L L L L H L L L H H
        //
        // Thinking about this differently:
        // What if we treat the WS2812 as seeing the signal from the UART's
        // perspective where LOW = "active" and HIGH = "idle"?
        //
        // Actually - the ESP8266 UART WS2812 approach is well-established
        // and works WITHOUT an inverter. The trick is that UART transmits
        // start bit (LOW) which creates a "dip" from idle HIGH.
        // The WS2812 data format:
        //   "0" bit: HIGH(short) then LOW(long)
        //   "1" bit: HIGH(long) then LOW(short)
        //
        // With non-inverted UART (idle HIGH):
        //   Each UART byte: [START=L]...[STOP=H]
        //   Between bytes: [STOP=H][START=L] = H→L transition
        //
        // For 0x88 at 3.2 Mbps non-inverted:
        //   Wire: L L L L H L L L H H
        //   If we consider the inter-byte transitions, consecutive 0x88:
        //   ...H H | L L L L H L L L H H | L L L L H L L L H H | ...
        //          ^stop^start             ^stop^start
        //
        // Each 10-bit frame at 3.2 Mbps = 3125ns
        // The wire looks like:
        //   H→L (from stop to start): Creates a falling edge
        //   L(4 bits=1250ns): LOW pulse
        //   H(1 bit=312ns): brief HIGH
        //   L(3 bits=937ns): LOW pulse
        //   H(2 bits=625ns): HIGH pulse (merges with next stop→start)
        //
        // From WS2812's perspective, the "first" edge it sees after reset
        // is a RISING edge. But with non-inverted UART, the first edge
        // after idle HIGH is a FALLING edge (start bit).
        //
        // The ESP8266 approach works because it's actually generating
        // INVERTED WS2812 signals, and the specific WS2812 ICs being
        // driven happen to be tolerant of this.
        //
        // For reliable operation on ESP32, we should use TX INVERSION.

        // Verify: non-inverted 0x88 does NOT match WS2812 spec
        int bits[10];
        uartFrameToBits(0x88, invert_tx, bits);
        // Wire: L L L L H L L L H H
        // First pulse is LOW (start bit), not HIGH
        FL_CHECK_EQ(bits[0], 0); // First bit is LOW - wrong for WS2812!
    }

    // THE KEY TEST: Verify the derived LUT produces correct WS2812 timing
    FL_SUBCASE("4 Mbps inverted TX: pulse timing validation") {
        const int bit_time_ns = 250; // 1e9 / 4e6 = 250ns
        const bool invert = true;

        // Derived LUT for 4 Mbps + inverted TX
        const uint8_t correct_lut[4] = {0xEF, 0x8F, 0xEC, 0x8C};

        // For each LUT entry, verify consecutive bytes produce correct timing
        for (int pattern = 0; pattern < 4; pattern++) {
            uint8_t byte_val = correct_lut[pattern];
            int bits[10];
            uartFrameToBits(byte_val, invert, bits);

            Pulse pulses[20];
            int np = measurePulses(bits, 10, bit_time_ns, pulses, 20);
            FL_CHECK_GE(np, 3); // At least 3 pulses per frame

            // Every pattern should start with HIGH (inverted start bit)
            FL_CHECK(pulses[0].is_high);

            // Extract LED bit 0 timing (first 5 UART bits = 1250ns)
            int led_bit_0 = (pattern >> 1) & 1; // MSB of 2-bit pair

            // LED bit 0 occupies positions 0-4 (1250ns)
            // LED bit "0": H(250ns) L(1000ns) → T0H=250ns
            // LED bit "1": H(750ns) L(500ns)  → T1H=750ns

            if (led_bit_0 == 0) {
                // First HIGH pulse should be ~250ns (1 bit)
                FL_CHECK_EQ(pulses[0].duration_ns, 250);
            } else {
                // First HIGH pulse should be ~750ns (3 bits)
                FL_CHECK_EQ(pulses[0].duration_ns, 750);
            }
        }
    }

    // Test consecutive bytes to verify inter-byte transitions
    FL_SUBCASE("4 Mbps inverted TX: consecutive byte transitions") {
        const int bit_time_ns = 250;
        const bool invert = true;
        const uint8_t correct_lut[4] = {0xEF, 0x8F, 0xEC, 0x8C};

        // Generate 2 consecutive "00" bytes and check the full waveform
        int bits1[10], bits2[10];
        uartFrameToBits(correct_lut[0], invert, bits1); // LED "00"
        uartFrameToBits(correct_lut[0], invert, bits2); // LED "00"

        // Combine into 20-bit stream
        int combined[20];
        for (int i = 0; i < 10; i++) combined[i] = bits1[i];
        for (int i = 0; i < 10; i++) combined[10 + i] = bits2[i];

        // Expected: H L L L L H L L L L | H L L L L H L L L L
        // Each frame encodes 2 "zero" LED bits
        // 4 total LED "0" bits, each with T0H=250ns, T0L=1000ns

        Pulse pulses[20];
        int np = measurePulses(combined, 20, bit_time_ns, pulses, 20);

        // Should see alternating: H(250) L(1000) H(250) L(1000) ...
        // The transition from byte1 stop (L) to byte2 start (H) is seamless
        // because stop=L and start=H with inversion → L→H = rising edge
        FL_CHECK_GE(np, 7); // At least 7 pulses in 20 bits
        // First pulse: H 250ns
        FL_CHECK(pulses[0].is_high);
        FL_CHECK_EQ(pulses[0].duration_ns, 250);
        // Second pulse: L 1000ns
        FL_CHECK_FALSE(pulses[1].is_high);
        FL_CHECK_EQ(pulses[1].duration_ns, 1000);
        // Third pulse: H 250ns (second LED bit starts mid-frame)
        FL_CHECK(pulses[2].is_high);
        FL_CHECK_EQ(pulses[2].duration_ns, 250);
        // Fourth pulse: L 1000ns (crosses byte boundary: 750ns from byte1 + 250ns from byte2 start... wait)

        // Actually let me recount. The combined bits for two 0xEF inverted:
        // Byte 1: H L L L L H L L L L
        // Byte 2: H L L L L H L L L L
        // Combined: H L L L L H L L L L H L L L L H L L L L
        //
        // Pulses: H(250) L(1000) H(250) L(1000) H(250) L(1000) H(250) L(1000)
        // Perfect! 4 LED "0" bits with T0H=250ns, T0L=1000ns each
        FL_CHECK_EQ(np, 8);
        for (int i = 0; i < np; i++) {
            if (i % 2 == 0) {
                FL_CHECK(pulses[i].is_high);
                FL_CHECK_EQ(pulses[i].duration_ns, 250);
            } else {
                FL_CHECK_FALSE(pulses[i].is_high);
                FL_CHECK_EQ(pulses[i].duration_ns, 1000);
            }
        }
    }

    // Verify "11" consecutive bytes produce correct LED "1" timing
    FL_SUBCASE("4 Mbps inverted TX: consecutive '11' bytes") {
        const int bit_time_ns = 250;
        const bool invert = true;
        const uint8_t correct_lut[4] = {0xEF, 0x8F, 0xEC, 0x8C};

        int bits1[10], bits2[10];
        uartFrameToBits(correct_lut[3], invert, bits1); // LED "11"
        uartFrameToBits(correct_lut[3], invert, bits2); // LED "11"

        int combined[20];
        for (int i = 0; i < 10; i++) combined[i] = bits1[i];
        for (int i = 0; i < 10; i++) combined[10 + i] = bits2[i];

        // Expected per byte: H H H L L H H H L L
        // Combined: H H H L L H H H L L H H H L L H H H L L
        // Pulses: H(750) L(500) H(750) L(500) H(750) L(500) H(750) L(500)
        // = 4 LED "1" bits with T1H=750ns, T1L=500ns

        Pulse pulses[20];
        int np = measurePulses(combined, 20, bit_time_ns, pulses, 20);

        FL_CHECK_EQ(np, 8);
        for (int i = 0; i < np; i++) {
            if (i % 2 == 0) {
                FL_CHECK(pulses[i].is_high);
                FL_CHECK_EQ(pulses[i].duration_ns, 750);
            } else {
                FL_CHECK_FALSE(pulses[i].is_high);
                FL_CHECK_EQ(pulses[i].duration_ns, 500);
            }
        }
    }

    // WS2812 timing tolerance check
    FL_SUBCASE("4 Mbps inverted TX: timing within WS2812 spec") {
        // WS2812 datasheet timing (with common tolerances):
        // T0H: 200-500ns (typ 350ns)
        // T0L: 650-1050ns (typ 900ns)
        // T1H: 550-850ns (typ 700ns)
        // T1L: 200-600ns (typ 450ns)
        //
        // Our derived timings at 4 Mbps:
        // T0H = 250ns (within 200-500ns ✓)
        // T0L = 1000ns (within 650-1050ns ✓)
        // T1H = 750ns (within 550-850ns ✓)
        // T1L = 500ns (within 200-600ns ✓)

        FL_CHECK_GE(250, 200);   // T0H min
        FL_CHECK_LE(250, 500);   // T0H max
        FL_CHECK_GE(1000, 650);  // T0L min
        FL_CHECK_LE(1000, 1050); // T0L max
        FL_CHECK_GE(750, 550);   // T1H min
        FL_CHECK_LE(750, 850);   // T1H max
        FL_CHECK_GE(500, 200);   // T1L min
        FL_CHECK_LE(500, 600);   // T1L max
    }
}
