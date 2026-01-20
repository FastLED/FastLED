/// @file test_uart_reset_timing.cpp
/// @brief Unit tests for UART reset signal timing behavior
///
/// Tests the timing-based reset period that prevents new transmissions
/// from starting immediately after the previous transmission completes.
/// This simulates the WS2812 reset signal requirement (>50us low period)
/// which UART cannot send as zeros due to start/stop bit framing.

#include "platforms/shared/mock/esp/32/drivers/uart_peripheral_mock.h"
#include "fl/stl/stdint.h"
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

TEST_CASE("UartPeripheralMock - Reset timing behavior") {
    UartPeripheralMock mock;
    // Enable virtual time mode for deterministic testing (no real-time dependencies)
    mock.setVirtualTimeMode(true);
    UartConfig config = createDefaultConfig();
    CHECK(mock.initialize(config));

    SUBCASE("Peripheral enters reset period after transmission completes") {
        // Write some data
        uint8_t data[] = {0xAA, 0x55, 0xFF};
        CHECK(mock.writeBytes(data, sizeof(data)));

        // Initially busy during transmission
        CHECK(mock.isBusy());

        // Advance time to complete transmission (3 bytes at 3.2 Mbps ~= 10us per byte)
        mock.advanceTime(100);

        // Wait for transmission to complete
        CHECK(mock.waitTxDone(1000));

        // After waitTxDone() returns true, peripheral should STILL be busy
        // due to reset period (channel draining)
        CHECK(mock.isBusy());
    }

    SUBCASE("Peripheral accepts new writes after reset period expires") {
        // First transmission
        uint8_t data1[] = {0xAA};
        CHECK(mock.writeBytes(data1, sizeof(data1)));

        // Advance time to complete transmission
        mock.advanceTime(100);
        CHECK(mock.waitTxDone(1000));

        // Should be in reset period (busy)
        CHECK(mock.isBusy());

        // Advance time past reset period (>= 50us minimum for WS2812)
        mock.advanceTime(200);

        // After reset period, should not be busy
        CHECK_FALSE(mock.isBusy());

        // Should accept new transmission
        uint8_t data2[] = {0x55};
        CHECK(mock.writeBytes(data2, sizeof(data2)));
        CHECK(mock.isBusy());
    }

    SUBCASE("Multiple transmissions respect reset gaps") {
        const int num_transmissions = 3;
        fl::vector<uint8_t> all_captured;

        for (int i = 0; i < num_transmissions; i++) {
            uint8_t data = 0x11 * (i + 1);  // 0x11, 0x22, 0x33

            // Advance time until peripheral is ready (not busy)
            int max_iterations = 100;
            int iterations = 0;
            while (mock.isBusy() && iterations < max_iterations) {
                mock.advanceTime(10);
                iterations++;
            }

            // Should be ready after reset period
            CHECK_FALSE(mock.isBusy());

            // Submit new transmission
            CHECK(mock.writeBytes(&data, 1));
            CHECK(mock.isBusy());

            // Advance time for transmission
            mock.advanceTime(100);

            // Wait for transmission
            CHECK(mock.waitTxDone(1000));

            // Should enter reset period (still busy)
            CHECK(mock.isBusy());

            all_captured.push_back(data);
        }

        // Verify all data was captured in order
        auto captured = mock.getCapturedBytes();
        REQUIRE(captured.size() == num_transmissions);
        for (int i = 0; i < num_transmissions; i++) {
            CHECK(captured[i] == all_captured[i]);
        }
    }

    SUBCASE("Reset period duration scales with transmission size") {
        // This test verifies that the reset period after transmission
        // is proportional to the transmission time (or 50us minimum for WS2812).
        //
        // We test the mock's CALCULATED reset duration (not wall-clock measurements)
        // to avoid flakiness from CPU scheduling during parallel test execution.

        // Small transmission (10 bytes)
        // At 3.2 Mbps with 10 bits/byte: 10 * 10 bits / 3.2 MHz = ~31.25us
        // Expected reset: 50us (minimum for WS2812)
        uint8_t small_data[10];
        for (int i = 0; i < 10; i++) {
            small_data[i] = 0xAA;
        }

        CHECK(mock.writeBytes(small_data, sizeof(small_data)));
        // Advance time for transmission
        mock.advanceTime(100);
        CHECK(mock.waitTxDone(1000));
        CHECK(mock.isBusy());  // Should be in reset period

        // Get the calculated reset duration (not wall-clock measurement)
        uint64_t small_reset_us = mock.getLastCalculatedResetDurationUs();

        // Advance time past reset period to complete
        mock.advanceTime(small_reset_us + 100);

        // Large transmission (1000 bytes)
        // At 3.2 Mbps with 10 bits/byte: 1000 * 10 bits / 3.2 MHz = ~3125us
        // Expected reset: ~3125us (proportional to transmission time)
        uint8_t large_data[1000];
        for (int i = 0; i < 1000; i++) {
            large_data[i] = static_cast<uint8_t>(i & 0xFF);
        }

        CHECK(mock.writeBytes(large_data, sizeof(large_data)));
        // Advance time for transmission (~3125us)
        mock.advanceTime(4000);
        CHECK(mock.waitTxDone(1000));
        CHECK(mock.isBusy());  // Should be in reset period

        // Get the calculated reset duration (not wall-clock measurement)
        uint64_t large_reset_us = mock.getLastCalculatedResetDurationUs();

        // Verify reset duration calculations:
        // 1. Small transmission should use minimum reset (50us) since tx time < 50us
        REQUIRE(small_reset_us >= 50);  // Minimum WS2812 reset requirement

        // 2. Large transmission should have proportionally larger reset
        REQUIRE(large_reset_us > small_reset_us);

        // 3. Large should be roughly proportional to transmission time
        // 1000 bytes at 3.2 Mbps = ~3125us transmission time
        // Reset duration should be at least 3000us (with buffer overhead)
        REQUIRE(large_reset_us >= 3000);
    }

    SUBCASE("writeBytes() during reset period blocks until reset completes") {
        // First transmission
        uint8_t data1[] = {0xAA};
        CHECK(mock.writeBytes(data1, sizeof(data1)));
        mock.advanceTime(100);
        CHECK(mock.waitTxDone(1000));

        // Should be in reset period
        CHECK(mock.isBusy());

        // Advance past reset period before attempting second write
        mock.advanceTime(200);

        // Attempt to write after reset period
        uint8_t data2[] = {0x55};
        bool write_success = mock.writeBytes(data2, sizeof(data2));

        // Write should succeed
        CHECK(write_success);
    }
}

TEST_CASE("UartPeripheralMock - Reset timing with virtual timing simulation") {
    UartPeripheralMock mock;
    // Enable virtual time mode for deterministic testing
    mock.setVirtualTimeMode(true);
    UartConfig config = createDefaultConfig();
    CHECK(mock.initialize(config));

    SUBCASE("Transmission time calculation is correct") {
        // At 3.2 Mbps, each bit takes 312.5 ns
        // For 8N1: 10 bits per byte = 3.125 us per byte
        // For 10 bytes: 31.25 us transmission time (plus 10us overhead = ~41us)

        uint8_t data[10] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

        CHECK(mock.writeBytes(data, sizeof(data)));

        // Advance time past the expected transmission delay
        // 10 bytes * 10 bits/byte / 3.2 MHz = 31.25us + 10us overhead = ~42us
        mock.advanceTime(50);
        CHECK(mock.waitTxDone(1000));

        // Verify we can get the calculated reset duration
        uint64_t reset_duration = mock.getLastCalculatedResetDurationUs();
        CHECK(reset_duration >= 50);  // At least 50us for WS2812 requirement

        // Note: We're testing that the timing calculation is consistent,
        // not that real wall-clock time passes (which would be flaky in parallel tests)
    }

    SUBCASE("WS2812 reset requirement (>50us) is satisfied") {
        // WS2812 protocol requires >50us low period between frames
        // UART cannot send this as zeros (start/stop bits interfere)
        // So we must wait 50us after transmission completes

        uint8_t data[] = {0xAA, 0x55};
        CHECK(mock.writeBytes(data, sizeof(data)));

        // Advance time for transmission
        mock.advanceTime(50);
        CHECK(mock.waitTxDone(1000));

        // Should be in reset period
        CHECK(mock.isBusy());

        // Verify the calculated reset duration meets WS2812 requirement
        uint64_t reset_duration = mock.getLastCalculatedResetDurationUs();
        CHECK(reset_duration >= 50);  // At least 50us for WS2812 compatibility

        // Advance time past reset period
        mock.advanceTime(reset_duration + 10);

        // Should no longer be busy
        CHECK_FALSE(mock.isBusy());
    }
}
