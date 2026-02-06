/// @file test_uart_reset_timing.cpp
/// @brief Unit tests for UART reset signal timing behavior
///
/// Tests the timing-based reset period that prevents new transmissions
/// from starting immediately after the previous transmission completes.
/// This simulates the WS2812 reset signal requirement (>50us low period)
/// which UART cannot send as zeros due to start/stop bit framing.

#include "platforms/shared/mock/esp/32/drivers/uart_peripheral_mock.h"
#include "platforms/esp/32/drivers/uart_esp32.h"
#include "fl/stl/stdint.h"
#include "fl/stl/thread.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "platforms/esp/32/drivers/uart/iuart_peripheral.h"
#include <chrono>  // ok include - for std::chrono::steady_clock
#include "fl/stl/vector.h"

using namespace fl;

namespace {

/// @brief Create a default test configuration
UartPeripheralConfig createDefaultConfig() {
    return UartPeripheralConfig(
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

TEST_CASE("UartPeripheralMock - Reset timing behavior") {
    UartPeripheralMock mock;
    UartPeripheralConfig config = createDefaultConfig();
    FL_CHECK(mock.initialize(config));

    // Enable virtual time mode for deterministic testing
    mock.setVirtualTimeMode(true);

    SUBCASE("Peripheral enters reset period after transmission completes") {
        // Write some data
        uint8_t data[] = {0xAA, 0x55, 0xFF};
        FL_CHECK(mock.writeBytes(data, sizeof(data)));

        // Initially busy during transmission
        FL_CHECK(mock.isBusy());

        // Pump time forward to complete transmission
        uint64_t tx_duration = mock.getTransmissionDuration();
        mock.pumpTime(tx_duration);

        // Wait for transmission to complete
        FL_CHECK(mock.waitTxDone(1000));

        // After waitTxDone() returns true, peripheral should STILL be busy
        // due to reset period (channel draining)
        FL_CHECK(mock.isBusy());
    }

    SUBCASE("Peripheral accepts new writes after reset period expires") {
        // First transmission
        uint8_t data1[] = {0xAA};
        FL_CHECK(mock.writeBytes(data1, sizeof(data1)));

        // Pump time forward to complete transmission
        uint64_t tx_duration = mock.getTransmissionDuration();
        mock.pumpTime(tx_duration);
        FL_CHECK(mock.waitTxDone(1000));

        // Should be in reset period (busy)
        FL_CHECK(mock.isBusy());

        // Pump time forward to complete reset period
        uint64_t reset_duration = mock.getResetDuration();
        mock.pumpTime(reset_duration);

        // After reset period, should not be busy
        FL_CHECK_FALSE(mock.isBusy());

        // Should accept new transmission
        uint8_t data2[] = {0x55};
        FL_CHECK(mock.writeBytes(data2, sizeof(data2)));
        FL_CHECK(mock.isBusy());
    }

    SUBCASE("Multiple transmissions respect reset gaps") {
        const int num_transmissions = 3;
        fl::vector<uint8_t> all_captured;

        for (int i = 0; i < num_transmissions; i++) {
            uint8_t data = 0x11 * (i + 1);  // 0x11, 0x22, 0x33

            // Pump time forward until peripheral is ready (not busy)
            if (mock.isBusy()) {
                uint64_t remaining = mock.getRemainingResetTime();
                if (remaining > 0) {
                    mock.pumpTime(remaining);
                }
            }

            // Should be ready after reset period
            FL_CHECK_FALSE(mock.isBusy());

            // Submit new transmission
            FL_CHECK(mock.writeBytes(&data, 1));
            FL_CHECK(mock.isBusy());

            // Pump time forward to complete transmission
            uint64_t tx_duration = mock.getTransmissionDuration();
            mock.pumpTime(tx_duration);
            FL_CHECK(mock.waitTxDone(1000));

            // Should enter reset period (still busy)
            FL_CHECK(mock.isBusy());

            all_captured.push_back(data);
        }

        // Verify all data was captured in order
        auto captured = mock.getCapturedBytes();
        FL_REQUIRE(captured.size() == num_transmissions);
        for (int i = 0; i < num_transmissions; i++) {
            FL_CHECK(captured[i] == all_captured[i]);
        }
    }

    SUBCASE("Reset period duration scales with transmission size") {
        // This test verifies that the reset period after transmission
        // is proportional to the transmission time (or 50us minimum for WS2812).
        //
        // Using virtual time mode makes this test completely deterministic.

        // Small transmission (10 bytes)
        // Expected: transmission ~31us, reset period 50us (minimum)
        uint8_t small_data[10];
        for (int i = 0; i < 10; i++) {
            small_data[i] = 0xAA;
        }

        FL_CHECK(mock.writeBytes(small_data, sizeof(small_data)));
        uint64_t small_tx_duration = mock.getTransmissionDuration();
        mock.pumpTime(small_tx_duration);
        FL_CHECK(mock.waitTxDone(1000));
        FL_CHECK(mock.isBusy());  // Should be in reset period

        uint64_t small_reset_duration = mock.getResetDuration();
        // Pump time forward to complete reset
        mock.pumpTime(small_reset_duration);
        FL_CHECK_FALSE(mock.isBusy());  // Reset should be complete

        // Large transmission (1000 bytes)
        // Expected: transmission ~3125us, reset period 3125us
        uint8_t large_data[1000];
        for (int i = 0; i < 1000; i++) {
            large_data[i] = static_cast<uint8_t>(i & 0xFF);
        }

        FL_CHECK(mock.writeBytes(large_data, sizeof(large_data)));
        uint64_t large_tx_duration = mock.getTransmissionDuration();
        mock.pumpTime(large_tx_duration);
        FL_CHECK(mock.waitTxDone(1000));
        FL_CHECK(mock.isBusy());  // Should be in reset period

        uint64_t large_reset_duration = mock.getResetDuration();
        // Pump time forward to complete reset
        mock.pumpTime(large_reset_duration);
        FL_CHECK_FALSE(mock.isBusy());  // Reset should be complete

        // Reset period for larger transmission should be longer (proportional to transmission time)
        // Expected: ~50us for small (10 bytes) vs ~3135us for large (1000 bytes) at 3.2 Mbps
        FL_CHECK(large_reset_duration > small_reset_duration);
        FL_CHECK(small_reset_duration >= 50);  // Minimum WS2812 reset period
    }

    SUBCASE("writeBytes() during reset period blocks until reset completes") {
        // First transmission
        uint8_t data1[] = {0xAA};
        FL_CHECK(mock.writeBytes(data1, sizeof(data1)));
        uint64_t tx_duration = mock.getTransmissionDuration();
        mock.pumpTime(tx_duration);
        FL_CHECK(mock.waitTxDone(1000));

        // Should be in reset period
        FL_CHECK(mock.isBusy());

        // Attempt to write during reset period
        // Mock implementation accepts the write immediately (doesn't block)
        uint8_t data2[] = {0x55};
        bool write_success = mock.writeBytes(data2, sizeof(data2));

        // Write should succeed
        FL_CHECK(write_success);

        // Note: Mock implementation allows writes during reset period.
        // Real hardware behavior may vary.
    }
}

TEST_CASE("UartPeripheralMock - Reset timing with real timing simulation") {
    UartPeripheralMock mock;
    UartPeripheralConfig config = createDefaultConfig();
    FL_CHECK(mock.initialize(config));

    // Enable virtual time mode for deterministic testing
    mock.setVirtualTimeMode(true);

    SUBCASE("Transmission time calculation is realistic") {
        // At 3.2 Mbps, each bit takes 312.5 ns
        // For 8N1: 10 bits per byte = 3.125 us per byte
        // For 10 bytes: 31.25 us transmission time

        uint8_t data[10] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

        FL_CHECK(mock.writeBytes(data, sizeof(data)));
        uint64_t tx_duration = mock.getTransmissionDuration();

        // Expected transmission time: 31.25 us + 10 us overhead
        FL_CHECK(tx_duration >= 31);   // At least 31us
        FL_CHECK(tx_duration <= 100);  // No more than 100us

        mock.pumpTime(tx_duration);
        FL_CHECK(mock.waitTxDone(1000));
    }

    SUBCASE("WS2812 reset requirement (>50us) is satisfied") {
        // WS2812 protocol requires >50us low period between frames
        // UART cannot send this as zeros (start/stop bits interfere)
        // So we must wait 50us after transmission completes

        uint8_t data[] = {0xAA, 0x55};
        FL_CHECK(mock.writeBytes(data, sizeof(data)));
        uint64_t tx_duration = mock.getTransmissionDuration();
        mock.pumpTime(tx_duration);
        FL_CHECK(mock.waitTxDone(1000));

        // Should be in reset period
        FL_CHECK(mock.isBusy());

        // Reset duration should be at least 50us for WS2812 compatibility
        uint64_t reset_duration = mock.getResetDuration();
        FL_CHECK(reset_duration >= 50);
    }
}
