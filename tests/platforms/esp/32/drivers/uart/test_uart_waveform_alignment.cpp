/// @file test_uart_waveform_alignment.cpp
/// @brief Tests UART waveform alignment with start/stop bits
///
/// This test verifies that the wave8 encoder patterns align correctly
/// with UART LSB-first transmission and start/stop bit framing.

#include "doctest.h"

#include "platforms/esp/32/drivers/uart/wave8_encoder_uart.h"
#include "platforms/shared/mock/esp/32/drivers/uart_peripheral_mock.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "fl/log.h"
#include "fl/stl/strstream.h"
#include "platforms/esp/32/drivers/uart/iuart_peripheral.h"
#include "fl/stl/vector.h"

using namespace fl;

namespace {

/// @brief Create a default test configuration
UartConfig createDefaultConfig() {
    return UartConfig(
        3200000,  // 3.2 Mbps baud rate
        17,       // TX pin (GPIO 17)
        -1,       // RX pin (not used)
        4096,     // TX buffer size (4 KB)
        0,        // RX buffer size (not used)
        1,        // Stop bits (8N1)
        1         // UART peripheral 1
    );
}

} // anonymous namespace

TEST_CASE("UART Waveform Alignment - Pattern 0b00") {
    // Create and configure mock UART
    UartPeripheralMock mock;
    UartConfig config = createDefaultConfig();
    REQUIRE(mock.initialize(config));

    // Encode pattern 0b00 using the LUT
    uint8_t pattern_00 = detail::encodeUart2Bits(0x00);
    REQUIRE(pattern_00 == 0x11);  // Expected rotated pattern

    // Write to UART
    mock.writeBytes(&pattern_00, 1);

    // Get waveform with UART framing
    fl::vector<bool> waveform = mock.getWaveformWithFraming();
    REQUIRE(waveform.size() == 10);  // 1 byte = 10 bits (start + 8 data + stop)

    // Print waveform for debugging
    FL_DBG("Pattern 0x11 waveform:");
    for (size_t i = 0; i < waveform.size(); ++i) {
        FL_DBG("  Bit " << i << ": " << (waveform[i] ? "HIGH" : "LOW"));
    }

    // Verify UART framing
    REQUIRE(waveform[0] == false);  // START bit must be LOW
    REQUIRE(waveform[9] == true);   // STOP bit must be HIGH

    // Verify data bits (0x11 = 0b00010001 transmitted LSB-first)
    // Expected: 1-0-0-0-1-0-0-0 (reading from bit 0 to bit 7)
    REQUIRE(waveform[1] == true);   // Bit 0 (LSB) = 1
    REQUIRE(waveform[2] == false);  // Bit 1 = 0
    REQUIRE(waveform[3] == false);  // Bit 2 = 0
    REQUIRE(waveform[4] == false);  // Bit 3 = 0
    REQUIRE(waveform[5] == true);   // Bit 4 = 1
    REQUIRE(waveform[6] == false);  // Bit 5 = 0
    REQUIRE(waveform[7] == false);  // Bit 6 = 0
    REQUIRE(waveform[8] == false);  // Bit 7 (MSB) = 0

    // Full waveform should be: [START(0)][1][0][0][0][1][0][0][0][STOP(1)]
}

TEST_CASE("UART Waveform Alignment - All patterns") {
    UartPeripheralMock mock;
    UartConfig config = createDefaultConfig();
    REQUIRE(mock.initialize(config));

    SUBCASE("Pattern 0b00 → 0x11") {
        uint8_t pattern = detail::encodeUart2Bits(0x00);
        REQUIRE(pattern == 0x11);

        mock.writeBytes(&pattern, 1);
        fl::vector<bool> waveform = mock.getWaveformWithFraming();

        // 0x11 = 0b00010001 → LSB-first: 1-0-0-0-1-0-0-0
        FL_DBG("Pattern 0b00 (0x11) waveform:");
        for (size_t i = 0; i < waveform.size(); ++i) {
            FL_DBG("  [" << i << "] = " << (waveform[i] ? "1" : "0"));
        }

        REQUIRE(waveform[0] == false);  // START
        REQUIRE(waveform[1] == true);   // Bit 0 = 1
        REQUIRE(waveform[2] == false);  // Bit 1 = 0
        REQUIRE(waveform[3] == false);  // Bit 2 = 0
        REQUIRE(waveform[4] == false);  // Bit 3 = 0
        REQUIRE(waveform[5] == true);   // Bit 4 = 1
        REQUIRE(waveform[6] == false);  // Bit 5 = 0
        REQUIRE(waveform[7] == false);  // Bit 6 = 0
        REQUIRE(waveform[8] == false);  // Bit 7 = 0
        REQUIRE(waveform[9] == true);   // STOP
    }

    SUBCASE("Pattern 0b01 → 0x19") {
        mock.resetCapturedData();
        uint8_t pattern = detail::encodeUart2Bits(0x01);
        REQUIRE(pattern == 0x19);

        mock.writeBytes(&pattern, 1);
        fl::vector<bool> waveform = mock.getWaveformWithFraming();

        // 0x19 = 0b00011001 → LSB-first: 1-0-0-1-1-0-0-0
        FL_DBG("Pattern 0b01 (0x19) waveform:");
        for (size_t i = 0; i < waveform.size(); ++i) {
            FL_DBG("  [" << i << "] = " << (waveform[i] ? "1" : "0"));
        }

        REQUIRE(waveform[0] == false);  // START
        REQUIRE(waveform[1] == true);   // Bit 0 = 1
        REQUIRE(waveform[2] == false);  // Bit 1 = 0
        REQUIRE(waveform[3] == false);  // Bit 2 = 0
        REQUIRE(waveform[4] == true);   // Bit 3 = 1
        REQUIRE(waveform[5] == true);   // Bit 4 = 1
        REQUIRE(waveform[6] == false);  // Bit 5 = 0
        REQUIRE(waveform[7] == false);  // Bit 6 = 0
        REQUIRE(waveform[8] == false);  // Bit 7 = 0
        REQUIRE(waveform[9] == true);   // STOP
    }

    SUBCASE("Pattern 0b10 → 0x91") {
        mock.resetCapturedData();
        uint8_t pattern = detail::encodeUart2Bits(0x02);
        REQUIRE(pattern == 0x91);

        mock.writeBytes(&pattern, 1);
        fl::vector<bool> waveform = mock.getWaveformWithFraming();

        // 0x91 = 0b10010001 → LSB-first: 1-0-0-0-1-0-0-1
        FL_DBG("Pattern 0b10 (0x91) waveform:");
        for (size_t i = 0; i < waveform.size(); ++i) {
            FL_DBG("  [" << i << "] = " << (waveform[i] ? "1" : "0"));
        }

        REQUIRE(waveform[0] == false);  // START
        REQUIRE(waveform[1] == true);   // Bit 0 = 1
        REQUIRE(waveform[2] == false);  // Bit 1 = 0
        REQUIRE(waveform[3] == false);  // Bit 2 = 0
        REQUIRE(waveform[4] == false);  // Bit 3 = 0
        REQUIRE(waveform[5] == true);   // Bit 4 = 1
        REQUIRE(waveform[6] == false);  // Bit 5 = 0
        REQUIRE(waveform[7] == false);  // Bit 6 = 0
        REQUIRE(waveform[8] == true);   // Bit 7 = 1
        REQUIRE(waveform[9] == true);   // STOP
    }

    SUBCASE("Pattern 0b11 → 0x99") {
        mock.resetCapturedData();
        uint8_t pattern = detail::encodeUart2Bits(0x03);
        REQUIRE(pattern == 0x99);

        mock.writeBytes(&pattern, 1);
        fl::vector<bool> waveform = mock.getWaveformWithFraming();

        // 0x99 = 0b10011001 → LSB-first: 1-0-0-1-1-0-0-1
        FL_DBG("Pattern 0b11 (0x99) waveform:");
        for (size_t i = 0; i < waveform.size(); ++i) {
            FL_DBG("  [" << i << "] = " << (waveform[i] ? "1" : "0"));
        }

        REQUIRE(waveform[0] == false);  // START
        REQUIRE(waveform[1] == true);   // Bit 0 = 1
        REQUIRE(waveform[2] == false);  // Bit 1 = 0
        REQUIRE(waveform[3] == false);  // Bit 2 = 0
        REQUIRE(waveform[4] == true);   // Bit 3 = 1
        REQUIRE(waveform[5] == true);   // Bit 4 = 1
        REQUIRE(waveform[6] == false);  // Bit 5 = 0
        REQUIRE(waveform[7] == false);  // Bit 6 = 0
        REQUIRE(waveform[8] == true);   // Bit 7 = 1
        REQUIRE(waveform[9] == true);   // STOP
    }
}

TEST_CASE("UART Waveform - Original patterns (pre-rotation)") {
    // This test documents what the ORIGINAL patterns (0x88, 0x8C, 0xC8, 0xCC)
    // would look like when transmitted over UART LSB-first.
    // This helps us understand WHY the rotation was needed.

    UartPeripheralMock mock;
    UartConfig config = createDefaultConfig();
    REQUIRE(mock.initialize(config));

    SUBCASE("Original 0x88 (before rotation)") {
        // 0x88 = 0b10001000 → LSB-first: 0-0-0-1-0-0-0-1
        uint8_t pattern = 0x88;
        mock.writeBytes(&pattern, 1);
        fl::vector<bool> waveform = mock.getWaveformWithFraming();

        FL_DBG("Original 0x88 waveform (BEFORE rotation):");
        for (size_t i = 0; i < waveform.size(); ++i) {
            FL_DBG("  [" << i << "] = " << (waveform[i] ? "1" : "0"));
        }

        // The waveform is: [START(0)][0][0][0][1][0][0][0][1][STOP(1)]
        // Notice how the START bit blends with the first three data bits (all 0s),
        // creating a long LOW pulse. This is the alignment problem!
        REQUIRE(waveform[0] == false);  // START
        REQUIRE(waveform[1] == false);  // Bit 0 = 0  ← Problem: START + B0 + B1 + B2 = 4 consecutive LOWs
        REQUIRE(waveform[2] == false);  // Bit 1 = 0
        REQUIRE(waveform[3] == false);  // Bit 2 = 0
        REQUIRE(waveform[4] == true);   // Bit 3 = 1
        REQUIRE(waveform[5] == false);  // Bit 4 = 0
        REQUIRE(waveform[6] == false);  // Bit 5 = 0
        REQUIRE(waveform[7] == false);  // Bit 6 = 0
        REQUIRE(waveform[8] == true);   // Bit 7 = 1
        REQUIRE(waveform[9] == true);   // STOP
    }

    SUBCASE("Rotated 0x11 (after rotation)") {
        // 0x11 = 0b00010001 → LSB-first: 1-0-0-0-1-0-0-0
        uint8_t pattern = 0x11;
        mock.writeBytes(&pattern, 1);
        fl::vector<bool> waveform = mock.getWaveformWithFraming();

        FL_DBG("Rotated 0x11 waveform (AFTER rotation):");
        for (size_t i = 0; i < waveform.size(); ++i) {
            FL_DBG("  [" << i << "] = " << (waveform[i] ? "1" : "0"));
        }

        // The waveform is: [START(0)][1][0][0][0][1][0][0][0][STOP(1)]
        // The START bit is now followed by a HIGH bit (B0=1), providing better separation!
        REQUIRE(waveform[0] == false);  // START
        REQUIRE(waveform[1] == true);   // Bit 0 = 1  ← Fixed: START(0) followed by HIGH(1) creates clear edge
        REQUIRE(waveform[2] == false);  // Bit 1 = 0
        REQUIRE(waveform[3] == false);  // Bit 2 = 0
        REQUIRE(waveform[4] == false);  // Bit 3 = 0
        REQUIRE(waveform[5] == true);   // Bit 4 = 1
        REQUIRE(waveform[6] == false);  // Bit 5 = 0
        REQUIRE(waveform[7] == false);  // Bit 6 = 0
        REQUIRE(waveform[8] == false);  // Bit 7 = 0
        REQUIRE(waveform[9] == true);   // STOP
    }
}
