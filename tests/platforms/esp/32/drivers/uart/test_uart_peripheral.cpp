/// @file uart_peripheral.cpp
/// @brief Unit tests for UartPeripheralMock

#include "platforms/shared/mock/esp/32/drivers/uart_peripheral_mock.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
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
        CHECK_FALSE(mock.isInitialized());
        CHECK_FALSE(mock.isBusy());
        CHECK(mock.getCapturedByteCount() == 0);
    }

    SUBCASE("Initialize and deinitialize") {
        UartConfig config = createDefaultConfig();

        CHECK(mock.initialize(config));
        CHECK(mock.isInitialized());
        CHECK(mock.getConfig().mBaudRate == 3200000);
        CHECK(mock.getConfig().mTxPin == 17);
        CHECK(mock.getConfig().mStopBits == 1);

        mock.deinitialize();
        CHECK_FALSE(mock.isInitialized());
    }

    SUBCASE("Double initialization") {
        UartConfig config = createDefaultConfig();

        CHECK(mock.initialize(config));
        CHECK(mock.isInitialized());

        // Second initialization should succeed (reinitialize)
        CHECK(mock.initialize(config));
        CHECK(mock.isInitialized());
    }

    SUBCASE("Invalid configuration - zero baud rate") {
        UartConfig config = createDefaultConfig();
        config.mBaudRate = 0;

        CHECK_FALSE(mock.initialize(config));
        CHECK_FALSE(mock.isInitialized());
    }

    SUBCASE("Invalid configuration - invalid TX pin") {
        UartConfig config = createDefaultConfig();
        config.mTxPin = -1;

        CHECK_FALSE(mock.initialize(config));
        CHECK_FALSE(mock.isInitialized());
    }

    SUBCASE("Invalid configuration - invalid stop bits") {
        UartConfig config = createDefaultConfig();
        config.mStopBits = 0;

        CHECK_FALSE(mock.initialize(config));
        CHECK_FALSE(mock.isInitialized());
    }
}

TEST_CASE("UartPeripheralMock - Single byte transmission") {
    UartPeripheralMock mock;
    UartConfig config = createDefaultConfig();
    CHECK(mock.initialize(config));

    SUBCASE("Write and verify single byte") {
        uint8_t data = 0xA5;
        CHECK(mock.writeBytes(&data, 1));
        CHECK(mock.waitTxDone(1000));

        auto captured = mock.getCapturedBytes();
        REQUIRE(captured.size() == 1);
        CHECK(captured[0] == 0xA5);
    }

    SUBCASE("Write multiple single bytes") {
        uint8_t data1 = 0xAA;
        uint8_t data2 = 0x55;
        uint8_t data3 = 0xFF;

        CHECK(mock.writeBytes(&data1, 1));
        CHECK(mock.writeBytes(&data2, 1));
        CHECK(mock.writeBytes(&data3, 1));
        CHECK(mock.waitTxDone(1000));

        auto captured = mock.getCapturedBytes();
        REQUIRE(captured.size() == 3);
        CHECK(captured[0] == 0xAA);
        CHECK(captured[1] == 0x55);
        CHECK(captured[2] == 0xFF);
    }

    SUBCASE("Write without initialization") {
        mock.deinitialize();
        uint8_t data = 0xA5;
        CHECK_FALSE(mock.writeBytes(&data, 1));
    }

    SUBCASE("Write with nullptr") {
        CHECK_FALSE(mock.writeBytes(nullptr, 1));
    }

    SUBCASE("Write with zero length") {
        uint8_t data = 0xA5;
        CHECK_FALSE(mock.writeBytes(&data, 0));
    }
}

TEST_CASE("UartPeripheralMock - Multi-byte transmission") {
    UartPeripheralMock mock;
    UartConfig config = createDefaultConfig();
    CHECK(mock.initialize(config));

    SUBCASE("Write byte array") {
        uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
        CHECK(mock.writeBytes(data, 5));
        CHECK(mock.waitTxDone(1000));

        auto captured = mock.getCapturedBytes();
        REQUIRE(captured.size() == 5);
        for (size_t i = 0; i < 5; ++i) {
            CHECK(captured[i] == data[i]);
        }
    }

    SUBCASE("Write RGB LED data (3 bytes)") {
        uint8_t rgb[] = {0xFF, 0x80, 0x00};  // Orange
        CHECK(mock.writeBytes(rgb, 3));
        CHECK(mock.waitTxDone(1000));

        auto captured = mock.getCapturedBytes();
        REQUIRE(captured.size() == 3);
        CHECK(captured[0] == 0xFF);
        CHECK(captured[1] == 0x80);
        CHECK(captured[2] == 0x00);
    }

    SUBCASE("Large buffer streaming (50 RGB LEDs)") {
        // Reduced from 100 to 50 LEDs for performance (still tests large buffer streaming)
        const size_t num_leds = 50;
        fl::vector<uint8_t> data(num_leds * 3);

        // Fill with test pattern
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] = static_cast<uint8_t>(i & 0xFF);
        }

        CHECK(mock.writeBytes(data.data(), data.size()));
        CHECK(mock.waitTxDone(5000));

        auto captured = mock.getCapturedBytes();
        REQUIRE(captured.size() == data.size());
        for (size_t i = 0; i < data.size(); ++i) {
            CHECK(captured[i] == data[i]);
        }
    }
}

TEST_CASE("UartPeripheralMock - Waveform extraction (8N1)") {
    UartPeripheralMock mock;
    UartConfig config = createDefaultConfig();
    config.mStopBits = 1;  // 8N1
    CHECK(mock.initialize(config));

    SUBCASE("Single byte waveform") {
        uint8_t data = 0xA5;  // 0b10100101
        CHECK(mock.writeBytes(&data, 1));
        CHECK(mock.waitTxDone(1000));

        auto waveform = mock.getWaveformWithFraming();
        REQUIRE(waveform.size() == 10);  // 1 byte × 10 bits (8N1)

        // Verify frame structure:
        // [START] [B0] [B1] [B2] [B3] [B4] [B5] [B6] [B7] [STOP]
        //   0      1    0    1    0    0    1    0    1      1
        CHECK(waveform[0] == false);  // Start bit (LOW)
        CHECK(waveform[1] == true);   // B0 (LSB) = 1
        CHECK(waveform[2] == false);  // B1 = 0
        CHECK(waveform[3] == true);   // B2 = 1
        CHECK(waveform[4] == false);  // B3 = 0
        CHECK(waveform[5] == false);  // B4 = 0
        CHECK(waveform[6] == true);   // B5 = 1
        CHECK(waveform[7] == false);  // B6 = 0
        CHECK(waveform[8] == true);   // B7 (MSB) = 1
        CHECK(waveform[9] == true);   // Stop bit (HIGH)
    }

    SUBCASE("Multiple byte waveform") {
        uint8_t data[] = {0xFF, 0x00, 0xAA};
        CHECK(mock.writeBytes(data, 3));
        CHECK(mock.waitTxDone(1000));

        auto waveform = mock.getWaveformWithFraming();
        REQUIRE(waveform.size() == 30);  // 3 bytes × 10 bits

        // Verify first frame (0xFF)
        CHECK(waveform[0] == false);   // Start bit
        for (int i = 1; i <= 8; ++i) {
            CHECK(waveform[i] == true);  // All data bits HIGH
        }
        CHECK(waveform[9] == true);    // Stop bit

        // Verify second frame (0x00)
        CHECK(waveform[10] == false);  // Start bit
        for (int i = 11; i <= 18; ++i) {
            CHECK(waveform[i] == false);  // All data bits LOW
        }
        CHECK(waveform[19] == true);   // Stop bit
    }
}

TEST_CASE("UartPeripheralMock - Waveform extraction (8N2)") {
    UartPeripheralMock mock;
    UartConfig config = createDefaultConfig();
    config.mStopBits = 2;  // 8N2
    CHECK(mock.initialize(config));

    SUBCASE("Single byte waveform with 2 stop bits") {
        uint8_t data = 0x55;  // 0b01010101
        CHECK(mock.writeBytes(&data, 1));
        CHECK(mock.waitTxDone(1000));

        auto waveform = mock.getWaveformWithFraming();
        REQUIRE(waveform.size() == 11);  // 1 byte × 11 bits (8N2)

        // Verify frame structure:
        // [START] [B0-B7] [STOP1] [STOP2]
        CHECK(waveform[0] == false);   // Start bit (LOW)
        CHECK(waveform[9] == true);    // Stop bit 1 (HIGH)
        CHECK(waveform[10] == true);   // Stop bit 2 (HIGH)
    }

    SUBCASE("Multiple byte waveform with 2 stop bits") {
        uint8_t data[] = {0xAA, 0x55};
        CHECK(mock.writeBytes(data, 2));
        CHECK(mock.waitTxDone(1000));

        auto waveform = mock.getWaveformWithFraming();
        REQUIRE(waveform.size() == 22);  // 2 bytes × 11 bits
    }
}

TEST_CASE("UartPeripheralMock - Start/stop bit validation") {
    UartPeripheralMock mock;
    UartConfig config = createDefaultConfig();

    SUBCASE("Valid 8N1 frames") {
        config.mStopBits = 1;
        CHECK(mock.initialize(config));

        uint8_t data[] = {0x00, 0xFF, 0xAA, 0x55, 0x12, 0x34};
        CHECK(mock.writeBytes(data, 6));
        CHECK(mock.waitTxDone(1000));

        CHECK(mock.verifyStartStopBits());
    }

    SUBCASE("Valid 8N2 frames") {
        config.mStopBits = 2;
        CHECK(mock.initialize(config));

        uint8_t data[] = {0x00, 0xFF, 0xAA, 0x55};
        CHECK(mock.writeBytes(data, 4));
        CHECK(mock.waitTxDone(1000));

        CHECK(mock.verifyStartStopBits());
    }

    SUBCASE("Verification fails with no data") {
        CHECK(mock.initialize(config));
        CHECK_FALSE(mock.verifyStartStopBits());
    }
}

TEST_CASE("UartPeripheralMock - Transmission timing") {
    UartPeripheralMock mock;
    UartConfig config = createDefaultConfig();
    CHECK(mock.initialize(config));

    SUBCASE("Automatic transmission timing (default)") {
        uint8_t data = 0xA5;
        CHECK(mock.writeBytes(&data, 1));
        // With automatic timing calculation, transmission delay is calculated from baud rate
        // 1 byte × 10 bits (8N1) = 10 bits at 3200000 baud = ~3.125 microseconds + 10us overhead
        CHECK(mock.isBusy());  // Should be busy (transmission not instant)
        CHECK(mock.waitTxDone(1000));  // Wait should succeed within timeout
        // Wait for reset period (50us WS2812 reset requirement) - reduced from 100us to 60us for performance
        std::this_thread::sleep_for(std::chrono::microseconds(60));  // okay std namespace - threading
        CHECK_FALSE(mock.isBusy());
    }

    SUBCASE("Delayed transmission") {
        mock.setTransmissionDelay(1000);  // 1ms delay

        uint8_t data = 0xA5;
        CHECK(mock.writeBytes(&data, 1));
        CHECK(mock.isBusy());

        // Wait should succeed within timeout
        CHECK(mock.waitTxDone(5000));
        // Reset period is proportional to transmission time (max of 50us or tx_delay)
        // With 1000us transmission delay, reset period is also 1000us
        // Add 100us buffer for timing margin
        std::this_thread::sleep_for(std::chrono::microseconds(1100));  // okay std namespace - threading
        CHECK_FALSE(mock.isBusy());
    }

    SUBCASE("Force transmission complete") {
        mock.setTransmissionDelay(10000000);  // 10 second delay

        uint8_t data = 0xA5;
        CHECK(mock.writeBytes(&data, 1));
        CHECK(mock.isBusy());

        // Force completion
        mock.forceTransmissionComplete();
        CHECK_FALSE(mock.isBusy());
    }

    SUBCASE("Wait on idle peripheral") {
        CHECK(mock.waitTxDone(1000));  // Should return immediately
    }
}

TEST_CASE("UartPeripheralMock - State management") {
    UartPeripheralMock mock;
    UartConfig config = createDefaultConfig();
    CHECK(mock.initialize(config));

    SUBCASE("Reset captured data") {
        uint8_t data[] = {0x01, 0x02, 0x03};
        CHECK(mock.writeBytes(data, 3));
        CHECK(mock.waitTxDone(1000));

        CHECK(mock.getCapturedByteCount() == 3);
        mock.resetCapturedData();
        CHECK(mock.getCapturedByteCount() == 0);
    }

    SUBCASE("Full reset") {
        uint8_t data[] = {0x01, 0x02, 0x03};
        CHECK(mock.writeBytes(data, 3));
        CHECK(mock.waitTxDone(1000));

        mock.reset();
        CHECK_FALSE(mock.isInitialized());
        CHECK_FALSE(mock.isBusy());
        CHECK(mock.getCapturedByteCount() == 0);
    }

    SUBCASE("Reset between tests") {
        // First test
        uint8_t data1 = 0xAA;
        CHECK(mock.writeBytes(&data1, 1));
        CHECK(mock.waitTxDone(1000));
        CHECK(mock.getCapturedByteCount() == 1);

        // Reset
        mock.reset();
        CHECK(mock.initialize(config));

        // Second test
        uint8_t data2 = 0x55;
        CHECK(mock.writeBytes(&data2, 1));
        CHECK(mock.waitTxDone(1000));
        auto captured = mock.getCapturedBytes();
        REQUIRE(captured.size() == 1);
        CHECK(captured[0] == 0x55);  // Should NOT contain 0xAA from first test
    }
}

TEST_CASE("UartPeripheralMock - Edge cases") {
    UartPeripheralMock mock;
    UartConfig config = createDefaultConfig();
    CHECK(mock.initialize(config));

    SUBCASE("All zeros byte") {
        uint8_t data = 0x00;
        CHECK(mock.writeBytes(&data, 1));
        CHECK(mock.waitTxDone(1000));

        auto waveform = mock.getWaveformWithFraming();
        CHECK(waveform[0] == false);   // Start bit
        for (int i = 1; i <= 8; ++i) {
            CHECK(waveform[i] == false);  // All data bits LOW
        }
        CHECK(waveform[9] == true);    // Stop bit
    }

    SUBCASE("All ones byte") {
        uint8_t data = 0xFF;
        CHECK(mock.writeBytes(&data, 1));
        CHECK(mock.waitTxDone(1000));

        auto waveform = mock.getWaveformWithFraming();
        CHECK(waveform[0] == false);   // Start bit
        for (int i = 1; i <= 8; ++i) {
            CHECK(waveform[i] == true);  // All data bits HIGH
        }
        CHECK(waveform[9] == true);    // Stop bit
    }

    SUBCASE("Alternating pattern") {
        uint8_t data = 0xAA;  // 0b10101010
        CHECK(mock.writeBytes(&data, 1));
        CHECK(mock.waitTxDone(1000));

        auto waveform = mock.getWaveformWithFraming();
        CHECK(waveform[0] == false);   // Start bit
        CHECK(waveform[1] == false);   // B0 = 0
        CHECK(waveform[2] == true);    // B1 = 1
        CHECK(waveform[3] == false);   // B2 = 0
        CHECK(waveform[4] == true);    // B3 = 1
        CHECK(waveform[5] == false);   // B4 = 0
        CHECK(waveform[6] == true);    // B5 = 1
        CHECK(waveform[7] == false);   // B6 = 0
        CHECK(waveform[8] == true);    // B7 = 1
        CHECK(waveform[9] == true);    // Stop bit
    }
}
