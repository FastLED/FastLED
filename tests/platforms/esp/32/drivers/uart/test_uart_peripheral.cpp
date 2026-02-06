/// @file uart_peripheral.cpp
/// @brief Unit tests for UartPeripheralMock

#include "platforms/shared/mock/esp/32/drivers/uart_peripheral_mock.h"
#include "platforms/esp/32/drivers/uart_esp32.h"
#include "fl/stl/cstddef.h" // ok include
#include "fl/stl/cstdint.h"
#include "fl/stl/thread.h"
#include "fl/stl/new.h"
#include "doctest.h"
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

TEST_CASE("UartPeripheralMock - Lifecycle") {
    UartPeripheralMock mock;

    SUBCASE("Initial state") {
        FL_CHECK_FALSE(mock.isInitialized());
        FL_CHECK_FALSE(mock.isBusy());
        FL_CHECK(mock.getCapturedByteCount() == 0);
    }

    SUBCASE("Initialize and deinitialize") {
        UartConfig config = createDefaultConfig();

        FL_CHECK(mock.initialize(config));
        FL_CHECK(mock.isInitialized());
        FL_CHECK(mock.getConfig().mBaudRate == 3200000);
        FL_CHECK(mock.getConfig().mTxPin == 17);
        FL_CHECK(mock.getConfig().mStopBits == 1);

        mock.deinitialize();
        FL_CHECK_FALSE(mock.isInitialized());
    }

    SUBCASE("Double initialization") {
        UartConfig config = createDefaultConfig();

        FL_CHECK(mock.initialize(config));
        FL_CHECK(mock.isInitialized());

        // Second initialization should succeed (reinitialize)
        FL_CHECK(mock.initialize(config));
        FL_CHECK(mock.isInitialized());
    }

    SUBCASE("Invalid configuration - zero baud rate") {
        UartConfig config = createDefaultConfig();
        config.mBaudRate = 0;

        FL_CHECK_FALSE(mock.initialize(config));
        FL_CHECK_FALSE(mock.isInitialized());
    }

    SUBCASE("Invalid configuration - invalid TX pin") {
        UartConfig config = createDefaultConfig();
        config.mTxPin = -1;

        FL_CHECK_FALSE(mock.initialize(config));
        FL_CHECK_FALSE(mock.isInitialized());
    }

    SUBCASE("Invalid configuration - invalid stop bits") {
        UartConfig config = createDefaultConfig();
        config.mStopBits = 0;

        FL_CHECK_FALSE(mock.initialize(config));
        FL_CHECK_FALSE(mock.isInitialized());
    }
}

TEST_CASE("UartPeripheralMock - Single byte transmission") {
    UartPeripheralMock mock;
    UartConfig config = createDefaultConfig();
    FL_CHECK(mock.initialize(config));

    SUBCASE("Write and verify single byte") {
        uint8_t data = 0xA5;
        FL_CHECK(mock.writeBytes(&data, 1));
        FL_CHECK(mock.waitTxDone(1000));

        auto captured = mock.getCapturedBytes();
        FL_REQUIRE(captured.size() == 1);
        FL_CHECK(captured[0] == 0xA5);
    }

    SUBCASE("Write multiple single bytes") {
        uint8_t data1 = 0xAA;
        uint8_t data2 = 0x55;
        uint8_t data3 = 0xFF;

        FL_CHECK(mock.writeBytes(&data1, 1));
        FL_CHECK(mock.writeBytes(&data2, 1));
        FL_CHECK(mock.writeBytes(&data3, 1));
        FL_CHECK(mock.waitTxDone(1000));

        auto captured = mock.getCapturedBytes();
        FL_REQUIRE(captured.size() == 3);
        FL_CHECK(captured[0] == 0xAA);
        FL_CHECK(captured[1] == 0x55);
        FL_CHECK(captured[2] == 0xFF);
    }

    SUBCASE("Write without initialization") {
        mock.deinitialize();
        uint8_t data = 0xA5;
        FL_CHECK_FALSE(mock.writeBytes(&data, 1));
    }

    SUBCASE("Write with nullptr") {
        FL_CHECK_FALSE(mock.writeBytes(nullptr, 1));
    }

    SUBCASE("Write with zero length") {
        uint8_t data = 0xA5;
        FL_CHECK_FALSE(mock.writeBytes(&data, 0));
    }
}

TEST_CASE("UartPeripheralMock - Multi-byte transmission") {
    UartPeripheralMock mock;
    UartConfig config = createDefaultConfig();
    FL_CHECK(mock.initialize(config));

    SUBCASE("Write byte array") {
        uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
        FL_CHECK(mock.writeBytes(data, 5));
        FL_CHECK(mock.waitTxDone(1000));

        auto captured = mock.getCapturedBytes();
        FL_REQUIRE(captured.size() == 5);
        for (size_t i = 0; i < 5; ++i) {
            FL_CHECK(captured[i] == data[i]);
        }
    }

    SUBCASE("Write RGB LED data (3 bytes)") {
        uint8_t rgb[] = {0xFF, 0x80, 0x00};  // Orange
        FL_CHECK(mock.writeBytes(rgb, 3));
        FL_CHECK(mock.waitTxDone(1000));

        auto captured = mock.getCapturedBytes();
        FL_REQUIRE(captured.size() == 3);
        FL_CHECK(captured[0] == 0xFF);
        FL_CHECK(captured[1] == 0x80);
        FL_CHECK(captured[2] == 0x00);
    }

    SUBCASE("Large buffer streaming (50 RGB LEDs)") {
        // Reduced from 100 to 50 LEDs for performance (still tests large buffer streaming)
        const size_t num_leds = 50;
        fl::vector<uint8_t> data(num_leds * 3);

        // Fill with test pattern
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] = static_cast<uint8_t>(i & 0xFF);
        }

        FL_CHECK(mock.writeBytes(data.data(), data.size()));
        FL_CHECK(mock.waitTxDone(5000));

        auto captured = mock.getCapturedBytes();
        FL_REQUIRE(captured.size() == data.size());
        for (size_t i = 0; i < data.size(); ++i) {
            FL_CHECK(captured[i] == data[i]);
        }
    }
}

TEST_CASE("UartPeripheralMock - Waveform extraction (8N1)") {
    UartPeripheralMock mock;
    UartConfig config = createDefaultConfig();
    config.mStopBits = 1;  // 8N1
    FL_CHECK(mock.initialize(config));

    SUBCASE("Single byte waveform") {
        uint8_t data = 0xA5;  // 0b10100101
        FL_CHECK(mock.writeBytes(&data, 1));
        FL_CHECK(mock.waitTxDone(1000));

        auto waveform = mock.getWaveformWithFraming();
        FL_REQUIRE(waveform.size() == 10);  // 1 byte × 10 bits (8N1)

        // Verify frame structure:
        // [START] [B0] [B1] [B2] [B3] [B4] [B5] [B6] [B7] [STOP]
        //   0      1    0    1    0    0    1    0    1      1
        FL_CHECK(waveform[0] == false);  // Start bit (LOW)
        FL_CHECK(waveform[1] == true);   // B0 (LSB) = 1
        FL_CHECK(waveform[2] == false);  // B1 = 0
        FL_CHECK(waveform[3] == true);   // B2 = 1
        FL_CHECK(waveform[4] == false);  // B3 = 0
        FL_CHECK(waveform[5] == false);  // B4 = 0
        FL_CHECK(waveform[6] == true);   // B5 = 1
        FL_CHECK(waveform[7] == false);  // B6 = 0
        FL_CHECK(waveform[8] == true);   // B7 (MSB) = 1
        FL_CHECK(waveform[9] == true);   // Stop bit (HIGH)
    }

    SUBCASE("Multiple byte waveform") {
        uint8_t data[] = {0xFF, 0x00, 0xAA};
        FL_CHECK(mock.writeBytes(data, 3));
        FL_CHECK(mock.waitTxDone(1000));

        auto waveform = mock.getWaveformWithFraming();
        FL_REQUIRE(waveform.size() == 30);  // 3 bytes × 10 bits

        // Verify first frame (0xFF)
        FL_CHECK(waveform[0] == false);   // Start bit
        for (int i = 1; i <= 8; ++i) {
            FL_CHECK(waveform[i] == true);  // All data bits HIGH
        }
        FL_CHECK(waveform[9] == true);    // Stop bit

        // Verify second frame (0x00)
        FL_CHECK(waveform[10] == false);  // Start bit
        for (int i = 11; i <= 18; ++i) {
            FL_CHECK(waveform[i] == false);  // All data bits LOW
        }
        FL_CHECK(waveform[19] == true);   // Stop bit
    }
}

TEST_CASE("UartPeripheralMock - Waveform extraction (8N2)") {
    UartPeripheralMock mock;
    UartConfig config = createDefaultConfig();
    config.mStopBits = 2;  // 8N2
    FL_CHECK(mock.initialize(config));

    SUBCASE("Single byte waveform with 2 stop bits") {
        uint8_t data = 0x55;  // 0b01010101
        FL_CHECK(mock.writeBytes(&data, 1));
        FL_CHECK(mock.waitTxDone(1000));

        auto waveform = mock.getWaveformWithFraming();
        FL_REQUIRE(waveform.size() == 11);  // 1 byte × 11 bits (8N2)

        // Verify frame structure:
        // [START] [B0-B7] [STOP1] [STOP2]
        FL_CHECK(waveform[0] == false);   // Start bit (LOW)
        FL_CHECK(waveform[9] == true);    // Stop bit 1 (HIGH)
        FL_CHECK(waveform[10] == true);   // Stop bit 2 (HIGH)
    }

    SUBCASE("Multiple byte waveform with 2 stop bits") {
        uint8_t data[] = {0xAA, 0x55};
        FL_CHECK(mock.writeBytes(data, 2));
        FL_CHECK(mock.waitTxDone(1000));

        auto waveform = mock.getWaveformWithFraming();
        FL_REQUIRE(waveform.size() == 22);  // 2 bytes × 11 bits
    }
}

TEST_CASE("UartPeripheralMock - Start/stop bit validation") {
    UartPeripheralMock mock;
    UartConfig config = createDefaultConfig();

    SUBCASE("Valid 8N1 frames") {
        config.mStopBits = 1;
        FL_CHECK(mock.initialize(config));

        uint8_t data[] = {0x00, 0xFF, 0xAA, 0x55, 0x12, 0x34};
        FL_CHECK(mock.writeBytes(data, 6));
        FL_CHECK(mock.waitTxDone(1000));

        FL_CHECK(mock.verifyStartStopBits());
    }

    SUBCASE("Valid 8N2 frames") {
        config.mStopBits = 2;
        FL_CHECK(mock.initialize(config));

        uint8_t data[] = {0x00, 0xFF, 0xAA, 0x55};
        FL_CHECK(mock.writeBytes(data, 4));
        FL_CHECK(mock.waitTxDone(1000));

        FL_CHECK(mock.verifyStartStopBits());
    }

    SUBCASE("Verification fails with no data") {
        FL_CHECK(mock.initialize(config));
        FL_CHECK_FALSE(mock.verifyStartStopBits());
    }
}

TEST_CASE("UartPeripheralMock - Transmission timing") {
    UartPeripheralMock mock;
    UartConfig config = createDefaultConfig();
    FL_CHECK(mock.initialize(config));

    SUBCASE("Automatic transmission timing (deterministic)") {
        // Enable virtual time mode for deterministic testing
        mock.setVirtualTimeMode(true);

        uint8_t data = 0xA5;
        FL_CHECK(mock.writeBytes(&data, 1));
        // With automatic timing calculation, transmission delay is calculated from baud rate
        // 1 byte × 10 bits (8N1) = 10 bits at 3200000 baud = ~3.125 microseconds + 10us overhead
        FL_CHECK(mock.isBusy());  // Should be busy immediately after write

        // Pump time through transmission delay
        uint64_t tx_duration = mock.getTransmissionDuration();
        mock.pumpTime(tx_duration);
        FL_CHECK(mock.isBusy());  // Still busy (in reset period)

        // Pump time through reset period
        uint64_t reset_duration = mock.getResetDuration();
        mock.pumpTime(reset_duration);
        FL_CHECK_FALSE(mock.isBusy());  // Now idle
    }

    SUBCASE("Delayed transmission (deterministic)") {
        // Enable virtual time mode for deterministic testing
        mock.setVirtualTimeMode(true);
        mock.setTransmissionDelay(1000);  // 1ms delay

        uint8_t data = 0xA5;
        FL_CHECK(mock.writeBytes(&data, 1));
        FL_CHECK(mock.isBusy());

        // Pump time through transmission delay
        mock.pumpTime(1000);
        FL_CHECK(mock.isBusy());  // Still busy (in reset period)

        // Pump time through reset period
        uint64_t reset_duration = mock.getResetDuration();
        mock.pumpTime(reset_duration);
        FL_CHECK_FALSE(mock.isBusy());
    }

    SUBCASE("Force transmission complete") {
        mock.setTransmissionDelay(10000000);  // 10 second delay

        uint8_t data = 0xA5;
        FL_CHECK(mock.writeBytes(&data, 1));
        FL_CHECK(mock.isBusy());

        // Force completion
        mock.forceTransmissionComplete();
        FL_CHECK_FALSE(mock.isBusy());
    }

    SUBCASE("Wait on idle peripheral") {
        FL_CHECK(mock.waitTxDone(1000));  // Should return immediately
    }
}

TEST_CASE("UartPeripheralMock - State management") {
    UartPeripheralMock mock;
    UartConfig config = createDefaultConfig();
    FL_CHECK(mock.initialize(config));

    SUBCASE("Reset captured data") {
        uint8_t data[] = {0x01, 0x02, 0x03};
        FL_CHECK(mock.writeBytes(data, 3));
        FL_CHECK(mock.waitTxDone(1000));

        FL_CHECK(mock.getCapturedByteCount() == 3);
        mock.resetCapturedData();
        FL_CHECK(mock.getCapturedByteCount() == 0);
    }

    SUBCASE("Full reset") {
        uint8_t data[] = {0x01, 0x02, 0x03};
        FL_CHECK(mock.writeBytes(data, 3));
        FL_CHECK(mock.waitTxDone(1000));

        mock.reset();
        FL_CHECK_FALSE(mock.isInitialized());
        FL_CHECK_FALSE(mock.isBusy());
        FL_CHECK(mock.getCapturedByteCount() == 0);
    }

    SUBCASE("Reset between tests") {
        // First test
        uint8_t data1 = 0xAA;
        FL_CHECK(mock.writeBytes(&data1, 1));
        FL_CHECK(mock.waitTxDone(1000));
        FL_CHECK(mock.getCapturedByteCount() == 1);

        // Reset
        mock.reset();
        FL_CHECK(mock.initialize(config));

        // Second test
        uint8_t data2 = 0x55;
        FL_CHECK(mock.writeBytes(&data2, 1));
        FL_CHECK(mock.waitTxDone(1000));
        auto captured = mock.getCapturedBytes();
        FL_REQUIRE(captured.size() == 1);
        FL_CHECK(captured[0] == 0x55);  // Should NOT contain 0xAA from first test
    }
}

TEST_CASE("UartPeripheralMock - Edge cases") {
    UartPeripheralMock mock;
    UartConfig config = createDefaultConfig();
    FL_CHECK(mock.initialize(config));

    SUBCASE("All zeros byte") {
        uint8_t data = 0x00;
        FL_CHECK(mock.writeBytes(&data, 1));
        FL_CHECK(mock.waitTxDone(1000));

        auto waveform = mock.getWaveformWithFraming();
        FL_CHECK(waveform[0] == false);   // Start bit
        for (int i = 1; i <= 8; ++i) {
            FL_CHECK(waveform[i] == false);  // All data bits LOW
        }
        FL_CHECK(waveform[9] == true);    // Stop bit
    }

    SUBCASE("All ones byte") {
        uint8_t data = 0xFF;
        FL_CHECK(mock.writeBytes(&data, 1));
        FL_CHECK(mock.waitTxDone(1000));

        auto waveform = mock.getWaveformWithFraming();
        FL_CHECK(waveform[0] == false);   // Start bit
        for (int i = 1; i <= 8; ++i) {
            FL_CHECK(waveform[i] == true);  // All data bits HIGH
        }
        FL_CHECK(waveform[9] == true);    // Stop bit
    }

    SUBCASE("Alternating pattern") {
        uint8_t data = 0xAA;  // 0b10101010
        FL_CHECK(mock.writeBytes(&data, 1));
        FL_CHECK(mock.waitTxDone(1000));

        auto waveform = mock.getWaveformWithFraming();
        FL_CHECK(waveform[0] == false);   // Start bit
        FL_CHECK(waveform[1] == false);   // B0 = 0
        FL_CHECK(waveform[2] == true);    // B1 = 1
        FL_CHECK(waveform[3] == false);   // B2 = 0
        FL_CHECK(waveform[4] == true);    // B3 = 1
        FL_CHECK(waveform[5] == false);   // B4 = 0
        FL_CHECK(waveform[6] == true);    // B5 = 1
        FL_CHECK(waveform[7] == false);   // B6 = 0
        FL_CHECK(waveform[8] == true);    // B7 = 1
        FL_CHECK(waveform[9] == true);    // Stop bit
    }
}

TEST_CASE("UartPeripheralMock - Virtual time control") {
    UartPeripheralMock mock;
    UartConfig config = createDefaultConfig();
    FL_CHECK(mock.initialize(config));
    mock.setVirtualTimeMode(true);

    SUBCASE("Manual time pumping - transmission lifecycle") {
        uint8_t data = 0xA5;
        FL_CHECK(mock.writeBytes(&data, 1));

        // Immediately after write: busy
        FL_CHECK(mock.isBusy());
        FL_CHECK(mock.getRemainingTransmissionTime() > 0);
        FL_CHECK(mock.getRemainingResetTime() == 0);  // Not in reset yet

        // Query calculated delays
        uint64_t tx_duration = mock.getTransmissionDuration();
        uint64_t reset_duration = mock.getResetDuration();
        FL_CHECK(tx_duration > 0);
        FL_CHECK(reset_duration >= 50);  // Minimum WS2812 reset

        // Pump time forward to transmission complete (but not past reset)
        mock.pumpTime(tx_duration);
        FL_CHECK(mock.isBusy());  // Still busy (in reset period)
        FL_CHECK(mock.getRemainingTransmissionTime() == 0);  // Transmission done
        FL_CHECK(mock.getRemainingResetTime() > 0);  // In reset period

        // Pump time forward through reset period
        mock.pumpTime(reset_duration);
        FL_CHECK_FALSE(mock.isBusy());  // Now idle
        FL_CHECK(mock.getRemainingTransmissionTime() == 0);
        FL_CHECK(mock.getRemainingResetTime() == 0);

        // Verify captured data
        auto captured = mock.getCapturedBytes();
        FL_REQUIRE(captured.size() == 1);
        FL_CHECK(captured[0] == 0xA5);
    }

    SUBCASE("Partial time advancement") {
        uint8_t data = 0xA5;
        FL_CHECK(mock.writeBytes(&data, 1));

        uint64_t tx_duration = mock.getTransmissionDuration();
        uint64_t remaining = mock.getRemainingTransmissionTime();
        FL_CHECK(remaining == tx_duration);

        // Pump halfway through transmission
        mock.pumpTime(tx_duration / 2);
        uint64_t new_remaining = mock.getRemainingTransmissionTime();
        FL_CHECK(new_remaining < remaining);
        FL_CHECK(new_remaining > 0);
        FL_CHECK(mock.isBusy());  // Still transmitting

        // Pump to completion
        mock.pumpTime(new_remaining);
        FL_CHECK(mock.getRemainingTransmissionTime() == 0);
        FL_CHECK(mock.getRemainingResetTime() > 0);  // Now in reset period
        FL_CHECK(mock.isBusy());  // Still busy (reset)

        // Complete reset period
        uint64_t reset_remaining = mock.getRemainingResetTime();
        mock.pumpTime(reset_remaining);
        FL_CHECK_FALSE(mock.isBusy());
    }

    SUBCASE("Virtual time query methods") {
        FL_CHECK(mock.getVirtualTime() > 0);  // Should be initialized to non-zero
        uint64_t start_time = mock.getVirtualTime();

        mock.pumpTime(1000);
        FL_CHECK(mock.getVirtualTime() == start_time + 1000);

        mock.pumpTime(500);
        FL_CHECK(mock.getVirtualTime() == start_time + 1500);
    }

    SUBCASE("Multiple transmissions with virtual time") {
        // First transmission
        uint8_t data1 = 0xAA;
        FL_CHECK(mock.writeBytes(&data1, 1));
        uint64_t tx1 = mock.getTransmissionDuration();
        uint64_t reset1 = mock.getResetDuration();
        mock.pumpTime(tx1 + reset1);
        FL_CHECK_FALSE(mock.isBusy());

        // Second transmission
        uint8_t data2 = 0x55;
        FL_CHECK(mock.writeBytes(&data2, 1));
        uint64_t tx2 = mock.getTransmissionDuration();
        uint64_t reset2 = mock.getResetDuration();
        mock.pumpTime(tx2 + reset2);
        FL_CHECK_FALSE(mock.isBusy());

        // Verify both captures
        auto captured = mock.getCapturedBytes();
        FL_REQUIRE(captured.size() == 2);
        FL_CHECK(captured[0] == 0xAA);
        FL_CHECK(captured[1] == 0x55);
    }

    SUBCASE("waitTxDone() in virtual time mode") {
        uint8_t data = 0xA5;
        FL_CHECK(mock.writeBytes(&data, 1));

        // In virtual time mode, waitTxDone() is non-blocking
        // It only checks/updates current state based on virtual time
        FL_CHECK(mock.isBusy());  // Still busy (time not advanced)

        // Must manually pump time to advance
        uint64_t tx_duration = mock.getTransmissionDuration();
        uint64_t reset_duration = mock.getResetDuration();

        // Pump through both transmission and reset
        mock.pumpTime(tx_duration + reset_duration);

        // Now fully complete
        FL_CHECK_FALSE(mock.isBusy());
        FL_CHECK(mock.waitTxDone(1000));  // Returns true (complete)
    }
}
