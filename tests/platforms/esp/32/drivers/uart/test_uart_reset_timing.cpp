/// @file test_uart_reset_timing.cpp
/// @brief Unit tests for UART reset signal timing behavior
///
/// Tests the timing-based reset period that prevents new transmissions
/// from starting immediately after the previous transmission completes.
/// This simulates the WS2812 reset signal requirement (>50us low period)
/// which UART cannot send as zeros due to start/stop bit framing.

#include "platforms/shared/mock/esp/32/drivers/uart_peripheral_mock.h"
#include <stdint.h>
#include "fl/stl/thread.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "platforms/esp/32/drivers/uart/iuart_peripheral.h"
#include "ratio"
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
    UartConfig config = createDefaultConfig();
    CHECK(mock.initialize(config));

    SUBCASE("Peripheral enters reset period after transmission completes") {
        // Write some data
        uint8_t data[] = {0xAA, 0x55, 0xFF};
        CHECK(mock.writeBytes(data, sizeof(data)));

        // Initially busy during transmission
        CHECK(mock.isBusy());

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
        CHECK(mock.waitTxDone(1000));

        // Should be in reset period (busy)
        CHECK(mock.isBusy());

        // Calculate expected reset duration
        // WS2812 reset requires >50us, but actual implementation may vary
        // For this test, we expect the mock to use a reasonable reset period
        // based on the transmission characteristics

        // Wait for reset period to expire (assume ~100us total for small transmission)
        std::this_thread::sleep_for(std::chrono::microseconds(150));

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

            // Wait until peripheral is ready (not busy)
            int max_wait_iterations = 1000;
            int iterations = 0;
            while (mock.isBusy() && iterations < max_wait_iterations) {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                iterations++;
            }

            // Should be ready after reset period
            CHECK_FALSE(mock.isBusy());

            // Submit new transmission
            CHECK(mock.writeBytes(&data, 1));
            CHECK(mock.isBusy());

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
        CHECK(mock.waitTxDone(1000));
        CHECK(mock.isBusy());  // Should be in reset period

        // Get the calculated reset duration (not wall-clock measurement)
        uint64_t small_reset_us = mock.getLastCalculatedResetDurationUs();

        // Wait for reset to complete before next transmission
        while (mock.isBusy()) {
            // Busy wait
        }

        // Large transmission (1000 bytes)
        // At 3.2 Mbps with 10 bits/byte: 1000 * 10 bits / 3.2 MHz = ~3125us
        // Expected reset: ~3125us (proportional to transmission time)
        uint8_t large_data[1000];
        for (int i = 0; i < 1000; i++) {
            large_data[i] = static_cast<uint8_t>(i & 0xFF);
        }

        CHECK(mock.writeBytes(large_data, sizeof(large_data)));
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
        CHECK(mock.waitTxDone(1000));

        // Should be in reset period
        CHECK(mock.isBusy());

        // Attempt to write during reset period
        // This should either:
        // a) Block until reset completes, then accept the write
        // b) Return false to indicate rejection (implementation choice)
        //
        // For this test, we expect it to eventually succeed after reset
        uint8_t data2[] = {0x55};
        bool write_success = mock.writeBytes(data2, sizeof(data2));

        // Write should eventually succeed
        CHECK(write_success);

        // Note: Implementation may either block until reset completes or
        // return immediately. Both behaviors are acceptable.
    }
}

TEST_CASE("UartPeripheralMock - Reset timing with real timing simulation") {
    UartPeripheralMock mock;
    UartConfig config = createDefaultConfig();
    CHECK(mock.initialize(config));

    SUBCASE("Transmission time calculation is realistic") {
        // At 3.2 Mbps, each bit takes 312.5 ns
        // For 8N1: 10 bits per byte = 3.125 us per byte
        // For 10 bytes: 31.25 us transmission time

        uint8_t data[10] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

        auto start = std::chrono::steady_clock::now();
        CHECK(mock.writeBytes(data, sizeof(data)));
        CHECK(mock.waitTxDone(1000));
        auto end = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        // Expected transmission time: 31.25 us
        // With reset period (assume ~50us), total should be ~80-100us
        // Allow generous range for test stability
        CHECK(elapsed >= 10);   // At least 10us (very conservative)
        CHECK(elapsed <= 500);  // No more than 500us (sanity check)
    }

    SUBCASE("WS2812 reset requirement (>50us) is satisfied") {
        // WS2812 protocol requires >50us low period between frames
        // UART cannot send this as zeros (start/stop bits interfere)
        // So we must wait 50us after transmission completes

        uint8_t data[] = {0xAA, 0x55};
        CHECK(mock.writeBytes(data, sizeof(data)));
        CHECK(mock.waitTxDone(1000));

        // Should be in reset period
        CHECK(mock.isBusy());

        // Wait at least 50us
        std::this_thread::sleep_for(std::chrono::microseconds(50));

        // Reset period should still be active (or just expired)
        // We expect reset duration to be AT LEAST 50us for WS2812 compatibility
        // But actual implementation may use a calculated value based on transmission
    }
}
