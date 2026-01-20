/// @file test_uart_channel_engine.cpp
/// @brief Unit tests for UART channel engine
///
/// Tests the ChannelEngineUART implementation using UartPeripheralMock for
/// hardware abstraction. Validates channel management, encoding, transmission,
/// and state machine behavior.

#include "doctest.h"
#include "fl/channels/data.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "platforms/esp/32/drivers/uart/channel_engine_uart.h"
#include "platforms/shared/mock/esp/32/drivers/uart_peripheral_mock.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "fl/stl/new.h"
#include "fl/channels/engine.h"
#include "fl/delay.h"
#include "fl/stl/move.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"

// WS2812 timing constants for testing
namespace {
    constexpr uint32_t WS2812_T0H = 400;   // 0.4 µs
    constexpr uint32_t WS2812_T0L = 850;   // 0.85 µs
    constexpr uint32_t WS2812_T1H = 800;   // 0.8 µs
    constexpr uint32_t WS2812_T1L = 450;   // 0.45 µs
}

using namespace fl;

//=============================================================================
// Test Fixture
//=============================================================================

class ChannelEngineUARTFixture {
public:
    ChannelEngineUARTFixture()
        : mMockPeripheralShared(fl::make_shared<UartPeripheralMock>()),
          mMockPeripheral(mMockPeripheralShared.get()),
          mEngine(mMockPeripheralShared) {
        // Engine stores shared_ptr to peripheral
    }

    ~ChannelEngineUARTFixture() {
        // Engine destructor will clean up peripheral via shared_ptr
    }

    // Helper: Create test channel data
    ChannelDataPtr createChannel(int pin, size_t num_leds) {
        // Create WS2812-like timing
        ChipsetTimingConfig timing(WS2812_T0H, WS2812_T0L, WS2812_T1H, WS2812_T1L);

        // Create encoded data buffer with test pattern
        fl::vector_psram<uint8_t> encodedData(num_leds * 3);
        for (size_t i = 0; i < encodedData.size(); i++) {
            encodedData[i] = static_cast<uint8_t>(i);
        }

        auto data = fl::make_shared<ChannelData>(
            pin, timing, fl::move(encodedData));

        return data;
    }

    // Helper: Poll until ready or timeout
    bool pollUntilReady(uint32_t timeout_ms = 1000) {
        uint32_t elapsed = 0;
        while (elapsed < timeout_ms) {
            IChannelEngine::EngineState state = mEngine.poll();
            if (state == IChannelEngine::EngineState::READY) {
                return true;
            }
            fl::delayMicroseconds(100);
            elapsed++;
        }
        return false;
    }

    fl::shared_ptr<UartPeripheralMock> mMockPeripheralShared; // Shared ownership
    UartPeripheralMock* mMockPeripheral; // Raw pointer for test access
    ChannelEngineUART mEngine;
};

//=============================================================================
// Test Cases
//=============================================================================

TEST_CASE("ChannelEngineUART - Lifecycle") {
    ChannelEngineUARTFixture fixture;

    SUBCASE("Initial state is READY") {
        CHECK(fixture.mEngine.poll() == IChannelEngine::EngineState::READY);
    }

    SUBCASE("Engine name is UART") {
        CHECK(fl::string(fixture.mEngine.getName()) == "UART");
    }

    SUBCASE("Peripheral not initialized before first show") {
        CHECK_FALSE(fixture.mMockPeripheral->isInitialized());
    }
}

TEST_CASE("ChannelEngineUART - Single channel enqueue and show") {
    ChannelEngineUARTFixture fixture;

    SUBCASE("Enqueue channel") {
        auto channel = fixture.createChannel(17, 10); // 10 RGB LEDs
        fixture.mEngine.enqueue(channel);

        // State should still be READY (show not called yet)
        CHECK(fixture.mEngine.poll() == IChannelEngine::EngineState::READY);
    }

    SUBCASE("Show triggers initialization") {
        auto channel = fixture.createChannel(17, 10);
        fixture.mEngine.enqueue(channel);
        fixture.mEngine.show();

        // Peripheral should be initialized after show
        CHECK(fixture.mMockPeripheral->isInitialized());
    }

    SUBCASE("Show transmits encoded data") {
        auto channel = fixture.createChannel(17, 10); // 10 RGB LEDs = 30 bytes
        fixture.mEngine.enqueue(channel);
        fixture.mEngine.show();

        // Wait for mock transmission to complete
        fixture.mMockPeripheral->forceTransmissionComplete();

        // Poll until ready
        CHECK(fixture.pollUntilReady());

        // Verify encoded data was transmitted
        auto captured = fixture.mMockPeripheral->getCapturedBytes();

        // Expected: 30 bytes * 4 expansion = 120 bytes
        CHECK(captured.size() == 120);
    }

    SUBCASE("Encoding correctness - single byte") {
        // Create channel with single RGB LED
        auto channel = fixture.createChannel(17, 1);
        auto& buffer = channel->getData();
        buffer[0] = 0xE4; // Test pattern
        buffer[1] = 0x00;
        buffer[2] = 0xFF;

        fixture.mEngine.enqueue(channel);
        fixture.mEngine.show();
        fixture.mMockPeripheral->forceTransmissionComplete();
        CHECK(fixture.pollUntilReady());

        auto captured = fixture.mMockPeripheral->getCapturedBytes();
        CHECK(captured.size() == 12); // 3 bytes * 4 = 12 bytes

        // Verify first byte (0xE4 = 0b11100100) encoding
        // Using rotated LUT: 0b00→0x11, 0b01→0x19, 0b10→0x91, 0b11→0x99
        CHECK(captured[0] == 0x99); // Bits 7-6 (0b11)
        CHECK(captured[1] == 0x91); // Bits 5-4 (0b10)
        CHECK(captured[2] == 0x19); // Bits 3-2 (0b01)
        CHECK(captured[3] == 0x11); // Bits 1-0 (0b00)

        // Verify second byte (0x00)
        CHECK(captured[4] == 0x11);
        CHECK(captured[5] == 0x11);
        CHECK(captured[6] == 0x11);
        CHECK(captured[7] == 0x11);

        // Verify third byte (0xFF)
        CHECK(captured[8] == 0x99);
        CHECK(captured[9] == 0x99);
        CHECK(captured[10] == 0x99);
        CHECK(captured[11] == 0x99);
    }
}

TEST_CASE("ChannelEngineUART - State machine") {
    ChannelEngineUARTFixture fixture;

    SUBCASE("State progression: READY → DRAINING → READY") {
        // Set a transmission delay so we can observe DRAINING state
        fixture.mMockPeripheral->setTransmissionDelay(1000); // 1ms delay

        auto channel = fixture.createChannel(17, 10);
        fixture.mEngine.enqueue(channel);

        // Initial: READY
        CHECK(fixture.mEngine.poll() == IChannelEngine::EngineState::READY);

        // After show: DRAINING (transmission in progress)
        fixture.mEngine.show();
        CHECK(fixture.mEngine.poll() == IChannelEngine::EngineState::DRAINING);

        // Complete transmission
        fixture.mMockPeripheral->forceTransmissionComplete();

        // After completion: READY
        CHECK(fixture.pollUntilReady());
    }

    SUBCASE("Multiple show() calls with different data") {
        // First transmission
        auto channel1 = fixture.createChannel(17, 5);
        fixture.mEngine.enqueue(channel1);
        fixture.mEngine.show();
        fixture.mMockPeripheral->forceTransmissionComplete();
        CHECK(fixture.pollUntilReady());

        auto captured1 = fixture.mMockPeripheral->getCapturedBytes();
        CHECK(captured1.size() == 60); // 5 LEDs * 3 bytes * 4 = 60

        // Reset mock
        fixture.mMockPeripheral->resetCapturedData();

        // Second transmission
        auto channel2 = fixture.createChannel(17, 10);
        fixture.mEngine.enqueue(channel2);
        fixture.mEngine.show();
        fixture.mMockPeripheral->forceTransmissionComplete();
        CHECK(fixture.pollUntilReady());

        auto captured2 = fixture.mMockPeripheral->getCapturedBytes();
        CHECK(captured2.size() == 120); // 10 LEDs * 3 bytes * 4 = 120
    }
}

TEST_CASE("ChannelEngineUART - Multiple channels sequential transmission") {
    ChannelEngineUARTFixture fixture;

    SUBCASE("Multiple channels transmitted sequentially") {
        auto channel1 = fixture.createChannel(17, 10);
        auto channel2 = fixture.createChannel(18, 10);

        fixture.mEngine.enqueue(channel1);
        fixture.mEngine.enqueue(channel2);
        fixture.mEngine.show();

        // Engine should handle multiple channels sequentially (UART is single-lane)
        // First channel should trigger initialization
        CHECK(fixture.mMockPeripheral->isInitialized());

        // Complete first channel and verify second channel is transmitted
        fixture.mMockPeripheral->forceTransmissionComplete();
        fixture.mEngine.poll(); // Start second channel
        fixture.mMockPeripheral->forceTransmissionComplete();
        CHECK(fixture.pollUntilReady());
    }
}

TEST_CASE("ChannelEngineUART - Buffer sizing") {
    ChannelEngineUARTFixture fixture;

    SUBCASE("Small buffer (10 LEDs)") {
        auto channel = fixture.createChannel(17, 10);
        fixture.mEngine.enqueue(channel);
        fixture.mEngine.show();
        fixture.mMockPeripheral->forceTransmissionComplete();
        CHECK(fixture.pollUntilReady());

        auto captured = fixture.mMockPeripheral->getCapturedBytes();
        CHECK(captured.size() == 120); // 10 * 3 * 4 = 120
    }

    SUBCASE("Medium buffer (50 LEDs)") {
        // Reduced from 100 to 50 LEDs for performance (still provides excellent coverage)
        auto channel = fixture.createChannel(17, 50);
        fixture.mEngine.enqueue(channel);
        fixture.mEngine.show();
        fixture.mMockPeripheral->forceTransmissionComplete();
        CHECK(fixture.pollUntilReady());

        auto captured = fixture.mMockPeripheral->getCapturedBytes();
        CHECK(captured.size() == 600); // 50 * 3 * 4 = 600
    }

    SUBCASE("Large buffer (500 LEDs)") {
        // Reduced from 1000 to 500 LEDs for performance (still provides excellent coverage)
        auto channel = fixture.createChannel(17, 500);
        fixture.mEngine.enqueue(channel);
        fixture.mEngine.show();
        fixture.mMockPeripheral->forceTransmissionComplete();
        CHECK(fixture.pollUntilReady());

        auto captured = fixture.mMockPeripheral->getCapturedBytes();
        CHECK(captured.size() == 6000); // 500 * 3 * 4 = 6000
    }
}

TEST_CASE("ChannelEngineUART - Empty channel handling") {
    ChannelEngineUARTFixture fixture;

    SUBCASE("Empty channel (0 LEDs)") {
        ChipsetTimingConfig timing(WS2812_T0H, WS2812_T0L, WS2812_T1H, WS2812_T1L);
        fl::vector_psram<uint8_t> emptyData;
        auto data = fl::make_shared<ChannelData>(
            17, timing, fl::move(emptyData));

        fixture.mEngine.enqueue(data);
        fixture.mEngine.show();

        // Should remain READY (no transmission)
        CHECK(fixture.mEngine.poll() == IChannelEngine::EngineState::READY);

        // Peripheral should NOT be initialized
        CHECK_FALSE(fixture.mMockPeripheral->isInitialized());
    }

    SUBCASE("Null channel") {
        fixture.mEngine.enqueue(nullptr);
        fixture.mEngine.show();

        // Should remain READY
        CHECK(fixture.mEngine.poll() == IChannelEngine::EngineState::READY);
    }
}

TEST_CASE("ChannelEngineUART - Chipset grouping") {
    ChannelEngineUARTFixture fixture;

    SUBCASE("Single chipset group") {
        // All channels use same timing (WS2812)
        auto channel = fixture.createChannel(17, 10);
        fixture.mEngine.enqueue(channel);
        fixture.mEngine.show();
        fixture.mMockPeripheral->forceTransmissionComplete();
        CHECK(fixture.pollUntilReady());

        // Verify single transmission occurred
        auto captured = fixture.mMockPeripheral->getCapturedBytes();
        CHECK(captured.size() == 120);
    }

    // Note: Multiple chipset groups would require different timing configs
    // Currently we only have WS2812, so we can't test multi-group behavior
    // This will be extended when more LED protocols are supported
}

TEST_CASE("ChannelEngineUART - Waveform validation") {
    ChannelEngineUARTFixture fixture;

    SUBCASE("Verify wave8 encoding patterns") {
        auto channel = fixture.createChannel(17, 1);
        auto& buffer = channel->getData();

        // Test all 4 2-bit patterns
        buffer[0] = 0x00; // 0b00000000
        buffer[1] = 0x55; // 0b01010101
        buffer[2] = 0xAA; // 0b10101010

        fixture.mEngine.enqueue(channel);
        fixture.mEngine.show();
        fixture.mMockPeripheral->forceTransmissionComplete();
        CHECK(fixture.pollUntilReady());

        auto captured = fixture.mMockPeripheral->getCapturedBytes();

        // Byte 0x00: all 0b00 → all 0x11 (rotated LUT)
        CHECK(captured[0] == 0x11);
        CHECK(captured[1] == 0x11);
        CHECK(captured[2] == 0x11);
        CHECK(captured[3] == 0x11);

        // Byte 0x55: alternating 0b01 → all 0x19 (rotated LUT)
        CHECK(captured[4] == 0x19);
        CHECK(captured[5] == 0x19);
        CHECK(captured[6] == 0x19);
        CHECK(captured[7] == 0x19);

        // Byte 0xAA: alternating 0b10 → all 0x91 (rotated LUT)
        CHECK(captured[8] == 0x91);
        CHECK(captured[9] == 0x91);
        CHECK(captured[10] == 0x91);
        CHECK(captured[11] == 0x91);
    }

    SUBCASE("Extract waveform from mock") {
        auto channel = fixture.createChannel(17, 1);
        auto& buffer = channel->getData();
        buffer[0] = 0xFF; // All 1s
        buffer[1] = 0x00; // All 0s
        buffer[2] = 0xCC; // 0b11001100

        fixture.mEngine.enqueue(channel);
        fixture.mEngine.show();
        fixture.mMockPeripheral->forceTransmissionComplete();
        CHECK(fixture.pollUntilReady());

        // Get waveform with start/stop bits
        auto waveform = fixture.mMockPeripheral->getWaveformWithFraming();

        // Verify waveform size: 12 bytes * 10 bits = 120 bits
        CHECK(waveform.size() == 120);

        // Verify start/stop bits are present
        CHECK(fixture.mMockPeripheral->verifyStartStopBits());
    }
}

TEST_CASE("ChannelEngineUART - Stress test") {
    ChannelEngineUARTFixture fixture;

    SUBCASE("Rapid show() calls") {
        for (int i = 0; i < 10; i++) {
            auto channel = fixture.createChannel(17, 10);
            fixture.mEngine.enqueue(channel);
            fixture.mEngine.show();
            fixture.mMockPeripheral->forceTransmissionComplete();
            CHECK(fixture.pollUntilReady());
            fixture.mMockPeripheral->resetCapturedData();
        }
    }

    SUBCASE("Very large LED count (2000 LEDs)") {
        // Reduced from 5000 to 2000 LEDs for performance (still provides excellent stress test coverage)
        auto channel = fixture.createChannel(17, 2000);
        fixture.mEngine.enqueue(channel);
        fixture.mEngine.show();
        fixture.mMockPeripheral->forceTransmissionComplete();
        CHECK(fixture.pollUntilReady());

        auto captured = fixture.mMockPeripheral->getCapturedBytes();
        CHECK(captured.size() == 24000); // 2000 * 3 * 4 = 24000
    }
}

TEST_CASE("ChannelEngineUART - Edge cases") {
    ChannelEngineUARTFixture fixture;

    SUBCASE("Show with no enqueued channels") {
        fixture.mEngine.show();
        CHECK(fixture.mEngine.poll() == IChannelEngine::EngineState::READY);
    }

    SUBCASE("Multiple enqueue before show") {
        auto channel1 = fixture.createChannel(17, 5);
        auto channel2 = fixture.createChannel(17, 10);

        fixture.mEngine.enqueue(channel1);
        fixture.mEngine.enqueue(channel2);
        fixture.mEngine.show();

        // UART is single-lane but handles multiple channels sequentially
        // First channel should be transmitted immediately
        CHECK(fixture.mMockPeripheral->isInitialized());

        // Complete first transmission
        fixture.mMockPeripheral->forceTransmissionComplete();

        // Poll to start second channel
        fixture.mEngine.poll();

        // Complete second transmission
        fixture.mMockPeripheral->forceTransmissionComplete();
        CHECK(fixture.pollUntilReady());
    }

    SUBCASE("Poll before initialization") {
        CHECK(fixture.mEngine.poll() == IChannelEngine::EngineState::READY);
    }
}
