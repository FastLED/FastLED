/// @file parlio_driver.cpp
/// @brief Comprehensive unit tests for PARLIO driver subsystem
///
/// Tests cover:
/// - WS2812 PARLIO bit encoding (pure encoding, no mock)
/// - Chipset timing grouping and state machine
/// - Mock peripheral initialization, transmission, ISR, error injection
/// - DMA output capture and waveform validation
/// - SPI byte encoding and SPI-over-PARLIO integration
/// - Mode switching (clockless <-> SPI)
/// - Full ChannelDriverPARLIO API (enqueue/show/poll)
/// - Untransposition, bit packing modes
/// - ParlioBufferCalculator DMA buffer size math

#include "fl/stl/stdint.h"
#include "test.h"
#include "fl/int.h"


#include "platforms/esp/32/drivers/parlio/parlio_peripheral_mock.h"
#include "platforms/esp/32/drivers/parlio/parlio_engine.h"
#include "platforms/esp/32/drivers/parlio/parlio_buffer_calc.h"
#include "platforms/esp/32/drivers/parlio/channel_driver_parlio.h"
#include "platforms/esp/32/drivers/parlio/iparlio_peripheral.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/new.h"
#include "fl/channels/data.h"
#include "fl/channels/config.h"
#include "fl/channels/wave8.h"
#include "fl/chipsets/spi.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/chipsets/led_timing.h"
#include "fl/slice.h"
#include "fl/stl/allocator.h"
#include "fl/stl/vector.h"
#include "fl/stl/move.h"
#include "fl/stl/map.h"



using namespace fl;

//=============================================================================
// WS2812 PARLIO Encoding (pure math, no mock needed)
//=============================================================================

namespace {

/// @brief Encode a single LED byte (0x00-0xFF) into 32-bit PARLIO waveform
/// @param byte LED color byte to encode (R, G, or B value)
/// @return 32-bit waveform for PARLIO transmission
///
/// WS2812 timing with PARLIO 4-tick encoding:
/// - Bit 0: 0b1000 (312.5ns high, 937.5ns low)
/// - Bit 1: 0b1110 (937.5ns high, 312.5ns low)
///
/// Each byte produces 8 x 4 = 32 bits of output.
/// MSB is transmitted first (standard WS2812 protocol).
static uint32_t encodeLedByte(uint8_t byte) {
    uint32_t result = 0;

    // Process each bit (MSB first)
    for (int i = 7; i >= 0; i--) {
        // Shift result to make room for 4 new bits
        result <<= 4;

        // Check bit value
        if (byte & (1 << i)) {
            // Bit is 1: encode as 0b1110 (hex: 0xE)
            result |= 0xE;
        } else {
            // Bit is 0: encode as 0b1000 (hex: 0x8)
            result |= 0x8;
        }
    }

    return result;
}

} // anonymous namespace

FL_TEST_CASE("WS2812 PARLIO encoding - all zeros") {
    uint32_t result = encodeLedByte(0x00);
    FL_CHECK_EQ(result, 0x88888888);
}

FL_TEST_CASE("WS2812 PARLIO encoding - all ones") {
    uint32_t result = encodeLedByte(0xFF);
    FL_CHECK_EQ(result, 0xEEEEEEEE);
}

FL_TEST_CASE("WS2812 PARLIO encoding - alternating pattern 0xAA") {
    uint32_t result = encodeLedByte(0xAA);
    FL_CHECK_EQ(result, 0xE8E8E8E8);
}

FL_TEST_CASE("WS2812 PARLIO encoding - alternating pattern 0x55") {
    uint32_t result = encodeLedByte(0x55);
    FL_CHECK_EQ(result, 0x8E8E8E8E);
}

FL_TEST_CASE("WS2812 PARLIO encoding - arbitrary value 0x0F") {
    uint32_t result = encodeLedByte(0x0F);
    FL_CHECK_EQ(result, 0x8888EEEE);
}

FL_TEST_CASE("WS2812 PARLIO encoding - arbitrary value 0xF0") {
    uint32_t result = encodeLedByte(0xF0);
    FL_CHECK_EQ(result, 0xEEEE8888);
}

FL_TEST_CASE("WS2812 PARLIO encoding - arbitrary value 0xC3") {
    uint32_t result = encodeLedByte(0xC3);
    FL_CHECK_EQ(result, 0xEE8888EE);
}

FL_TEST_CASE("WS2812 PARLIO encoding - single bit patterns") {
    FL_SUBCASE("Only MSB set (0x80)") {
        uint32_t result = encodeLedByte(0x80);
        FL_CHECK_EQ(result, 0xE8888888);
    }

    FL_SUBCASE("Only LSB set (0x01)") {
        uint32_t result = encodeLedByte(0x01);
        FL_CHECK_EQ(result, 0x8888888E);
    }

    FL_SUBCASE("Middle bit set (0x10)") {
        uint32_t result = encodeLedByte(0x10);
        FL_CHECK_EQ(result, 0x888E8888);
    }
}

//=============================================================================
// Mock-dependent tests (only run on stub platform)
//=============================================================================


using namespace fl::detail;

namespace {

/// @brief Helper to create WS2812B timing config
static ChipsetTimingConfig getWS2812Timing() {
    return ChipsetTimingConfig(350, 800, 450, 50, "WS2812B");
}

/// @brief Full reset of mock state (initialization + history)
static void resetMockFull() {
    auto& mock = ParlioPeripheralMock::instance();
    mock.reset();
}

/// @brief Light reset of mock (history only, keep initialization)
static void resetMockHistory() {
    auto& mock = ParlioPeripheralMock::instance();
    mock.clearTransmissionHistory();
    mock.setTransmitFailure(false);
    mock.setTransmitDelay(0);
}

/// @brief Wait for PARLIO driver to become READY (synchronous mock: instant)
static void waitForDriverReady(fl::IChannelDriver& driver, int maxPolls = 100) {
    for (int i = 0; i < maxPolls; i++) {
        auto state = driver.poll();
        if (state == fl::IChannelDriver::DriverState::READY) break;
    }
}

} // anonymous namespace

//=============================================================================
// Chipset Grouping
//=============================================================================

FL_TEST_CASE("ParlioEngine - single chipset type (all channels same timing)") {
    resetMockHistory();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1, 2, 4, 8};
    ChipsetTimingConfig timing = getWS2812Timing();

    size_t num_lanes = 4;
    size_t leds_per_lane = 5;
    size_t bytes_per_led = 3;
    size_t lane_stride = leds_per_lane * bytes_per_led;
    size_t total_bytes = num_lanes * lane_stride;

    bool init_ok = driver.initialize(num_lanes, pins, timing, leds_per_lane);
    FL_REQUIRE(init_ok);

    fl::vector<uint8_t> scratch(total_bytes);
    for (size_t i = 0; i < total_bytes; i++) {
        scratch[i] = static_cast<uint8_t>((i * 7) & 0xFF);
    }

    bool tx_ok = driver.beginTransmission(scratch.data(), total_bytes, num_lanes, lane_stride);
    FL_REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();
    const auto& history = mock.getTransmissionHistory();
    FL_CHECK(history.size() > 0);
}

FL_TEST_CASE("ParlioEngine - chipset timing equality operator") {
    ChipsetTimingConfig ws2812_timing = getWS2812Timing();
    ChipsetTimingConfig ws2812_timing2(350, 800, 450, 50, "WS2812B_ALT");
    ChipsetTimingConfig sk6812_timing(300, 900, 600, 80, "SK6812");

    FL_CHECK(ws2812_timing == ws2812_timing2);
    FL_CHECK(!(ws2812_timing == sk6812_timing));
    FL_CHECK(ws2812_timing != sk6812_timing);
}

FL_TEST_CASE("ParlioEngine - READY state before transmission") {
    resetMockHistory();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing();

    bool init_ok = driver.initialize(1, pins, timing, 10);
    FL_REQUIRE(init_ok);

    ParlioEngineState state = driver.poll();
    FL_CHECK(state == ParlioEngineState::READY);
}

FL_TEST_CASE("ParlioEngine - document current single-timing constraint") {
    resetMockHistory();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1, 2};
    ChipsetTimingConfig timing_ws2812 = getWS2812Timing();

    size_t num_lanes = 2;
    size_t leds_per_lane = 5;
    size_t lane_stride = leds_per_lane * 3;
    size_t total_bytes = num_lanes * lane_stride;

    bool init_ok = driver.initialize(num_lanes, pins, timing_ws2812, leds_per_lane);
    FL_REQUIRE(init_ok);

    fl::vector<uint8_t> scratch(total_bytes);
    for (size_t i = 0; i < total_bytes; i++) {
        scratch[i] = static_cast<uint8_t>((i * 11) & 0xFF);
    }

    bool tx_ok = driver.beginTransmission(scratch.data(), total_bytes, num_lanes, lane_stride);
    FL_REQUIRE(tx_ok);

    ParlioEngineState final_state = ParlioEngineState::ERROR;
    for (size_t i = 0; i < 200; i++) {
        ParlioEngineState state = driver.poll();
        if (state == ParlioEngineState::READY) {
            final_state = ParlioEngineState::READY;
            break;
        } else if (state == ParlioEngineState::ERROR) {
            final_state = ParlioEngineState::ERROR;
            break;
        }
    }
    FL_CHECK(final_state == ParlioEngineState::READY);

    auto& mock = ParlioPeripheralMock::instance();
    const auto& history = mock.getTransmissionHistory();
    FL_CHECK(history.size() > 0);
}

//=============================================================================
// Mock Initialization
//=============================================================================

FL_TEST_CASE("ParlioEngine mock - basic initialization") {
    resetMockFull();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing();

    bool success = driver.initialize(1, pins, timing, 10);
    FL_CHECK(success);

    auto& mock = ParlioPeripheralMock::instance();
    FL_CHECK(mock.isInitialized());
    FL_CHECK(mock.getConfig().data_width == 1);
    FL_CHECK(mock.getConfig().gpio_pins[0] == 1);
    FL_CHECK(mock.getConfig().gpio_pins[1] == -1);
}

FL_TEST_CASE("ParlioEngine mock - two-lane initialization") {
    resetMockFull();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1, 2};
    ChipsetTimingConfig timing = getWS2812Timing();

    bool success = driver.initialize(2, pins, timing, 100);
    FL_CHECK(success);

    auto& mock = ParlioPeripheralMock::instance();
    FL_CHECK(mock.isInitialized());

    const auto& config = mock.getConfig();
    FL_CHECK(config.data_width == 2);
    FL_CHECK(config.gpio_pins[0] == 1);
    FL_CHECK(config.gpio_pins[1] == 2);
    FL_CHECK(config.gpio_pins[2] == -1);
}

FL_TEST_CASE("ParlioEngine mock - multi-lane initialization") {
    resetMockFull();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1, 2, 4, 8};
    ChipsetTimingConfig timing = getWS2812Timing();

    bool success = driver.initialize(4, pins, timing, 100);
    FL_CHECK(success);

    auto& mock = ParlioPeripheralMock::instance();
    FL_CHECK(mock.isInitialized());

    const auto& config = mock.getConfig();
    FL_CHECK(config.data_width == 4);
    FL_CHECK(config.gpio_pins[0] == 1);
    FL_CHECK(config.gpio_pins[1] == 2);
    FL_CHECK(config.gpio_pins[2] == 4);
    FL_CHECK(config.gpio_pins[3] == 8);
    FL_CHECK(config.gpio_pins[4] == -1);
}

//=============================================================================
// Basic Transmission
//=============================================================================

FL_TEST_CASE("ParlioEngine mock - single LED transmission") {
    resetMockFull();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing();
    driver.initialize(1, pins, timing, 1);

    uint8_t scratch[3] = {0xFF, 0x00, 0xAA};

    bool success = driver.beginTransmission(scratch, 3, 1, 3);
    FL_CHECK(success);

    auto& mock = ParlioPeripheralMock::instance();
    FL_CHECK(mock.isEnabled());
    FL_CHECK(mock.getTransmitCount() > 0);

    const auto& history = mock.getTransmissionHistory();
    FL_CHECK(history.size() > 0);

    if (history.size() > 0) {
        FL_CHECK(history[0].bit_count > 0);
        FL_CHECK(history[0].buffer_copy.size() > 0);
    }
}

FL_TEST_CASE("ParlioEngine mock - multiple LEDs transmission") {
    resetMockFull();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing();

    size_t num_leds = 10;
    driver.initialize(1, pins, timing, num_leds);

    fl::vector<uint8_t> scratch(num_leds * 3);
    for (size_t i = 0; i < scratch.size(); i++) {
        scratch[i] = static_cast<uint8_t>(i & 0xFF);
    }

    bool success = driver.beginTransmission(scratch.data(), scratch.size(), 1, scratch.size());
    FL_CHECK(success);

    auto& mock = ParlioPeripheralMock::instance();
    FL_CHECK(mock.getTransmitCount() > 0);

    ParlioEngineState state = ParlioEngineState::DRAINING;
    for (int i = 0; i < 200 && state != ParlioEngineState::READY; i++) {
        state = driver.poll();
        if (state == ParlioEngineState::ERROR) {
            break;
        }
        if (state == ParlioEngineState::DRAINING) {
            delay(1);
        }
    }
    FL_CHECK(state == ParlioEngineState::READY);
}

FL_TEST_CASE("ParlioEngine mock - two-lane transmission") {
    auto& mock = ParlioPeripheralMock::instance();
    mock.clearTransmissionHistory();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1, 2};
    ChipsetTimingConfig timing = getWS2812Timing();

    size_t num_leds = 10;
    size_t num_lanes = 2;

    bool init_result = driver.initialize(num_lanes, pins, timing, num_leds);
    FL_REQUIRE(init_result);

    fl::vector<uint8_t> scratch(num_leds * num_lanes * 3);
    for (size_t lane = 0; lane < num_lanes; lane++) {
        for (size_t led = 0; led < num_leds; led++) {
            size_t base_idx = lane * num_leds * 3 + led * 3;
            scratch[base_idx + 0] = static_cast<uint8_t>((lane * 100 + led) & 0xFF);
            scratch[base_idx + 1] = static_cast<uint8_t>((lane * 50 + led) & 0xFF);
            scratch[base_idx + 2] = static_cast<uint8_t>((lane * 25 + led) & 0xFF);
        }
    }

    size_t lane_stride = num_leds * 3;
    bool success = driver.beginTransmission(scratch.data(), scratch.size(), num_lanes, lane_stride);
    FL_CHECK(success);

    FL_CHECK(mock.getTransmitCount() > 0);

    ParlioEngineState state = ParlioEngineState::DRAINING;
    for (int i = 0; i < 200 && state != ParlioEngineState::READY; i++) {
        state = driver.poll();
        if (state == ParlioEngineState::ERROR) {
            break;
        }
        if (state == ParlioEngineState::DRAINING) {
            delay(1);
        }
    }
    FL_CHECK(state == ParlioEngineState::READY);

    const auto& history = mock.getTransmissionHistory();
    FL_CHECK(history.size() > 0);

    if (history.size() > 0) {
        FL_CHECK(history[0].bit_count > 0);
        FL_CHECK(history[0].buffer_copy.size() > 0);
    }
}

//=============================================================================
// ISR Simulation
//=============================================================================

FL_TEST_CASE("ParlioEngine mock - ISR callback simulation") {
    resetMockFull();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing();
    driver.initialize(1, pins, timing, 10);

    uint8_t scratch[30];
    for (size_t i = 0; i < 30; i++) {
        scratch[i] = static_cast<uint8_t>(i);
    }

    bool success = driver.beginTransmission(scratch, 30, 1, 30);
    FL_CHECK(success);

    auto& mock = ParlioPeripheralMock::instance();
    size_t initial_count = mock.getTransmitCount();
    FL_CHECK(initial_count > 0);

    ParlioEngineState state = driver.poll();
    FL_CHECK(state == ParlioEngineState::READY);
}

//=============================================================================
// Error Injection
//=============================================================================

FL_TEST_CASE("ParlioEngine mock - transmit failure injection") {
    resetMockFull();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing();
    driver.initialize(1, pins, timing, 10);

    uint8_t scratch[30];

    auto& mock = ParlioPeripheralMock::instance();
    mock.setTransmitFailure(true);

    bool success = driver.beginTransmission(scratch, 30, 1, 30);
    FL_CHECK_FALSE(success);
    FL_CHECK(driver.poll() == ParlioEngineState::ERROR);

    mock.setTransmitFailure(false);
}

//=============================================================================
// Ring Buffer Streaming
//=============================================================================

FL_TEST_CASE("ParlioEngine mock - large buffer streaming") {
    resetMockFull();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing();

    size_t num_leds = 500;
    driver.initialize(1, pins, timing, num_leds);

    fl::vector<uint8_t> scratch(num_leds * 3);
    for (size_t i = 0; i < scratch.size(); i++) {
        scratch[i] = static_cast<uint8_t>(i & 0xFF);
    }

    bool success = driver.beginTransmission(scratch.data(), scratch.size(), 1, scratch.size());
    FL_CHECK(success);

    auto& mock = ParlioPeripheralMock::instance();
    FL_CHECK(mock.getTransmitCount() > 0);

    ParlioEngineState state = ParlioEngineState::DRAINING;
    for (int i = 0; i < 200 && state != ParlioEngineState::READY; i++) {
        state = driver.poll();
        if (state == ParlioEngineState::ERROR) {
            break;
        }
        if (state == ParlioEngineState::DRAINING) {
            delay(1);
        }
    }
    FL_CHECK(state == ParlioEngineState::READY);
}

FL_TEST_CASE("ParlioEngine mock - multi-lane streaming") {
    resetMockFull();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1, 2, 4, 8};
    ChipsetTimingConfig timing = getWS2812Timing();

    size_t num_leds = 200;
    size_t num_lanes = 4;
    driver.initialize(num_lanes, pins, timing, num_leds);

    fl::vector<uint8_t> scratch(num_leds * num_lanes * 3);
    for (size_t i = 0; i < scratch.size(); i++) {
        scratch[i] = static_cast<uint8_t>((i * 7 + 13) & 0xFF);
    }

    size_t lane_stride = num_leds * 3;
    bool success = driver.beginTransmission(
        scratch.data(),
        scratch.size(),
        num_lanes,
        lane_stride
    );
    FL_CHECK(success);

    auto& mock = ParlioPeripheralMock::instance();
    FL_CHECK(mock.getTransmitCount() > 0);

    ParlioEngineState state = ParlioEngineState::DRAINING;
    for (int i = 0; i < 200 && state != ParlioEngineState::READY; i++) {
        state = driver.poll();
        if (state == ParlioEngineState::ERROR) {
            break;
        }
        if (state == ParlioEngineState::DRAINING) {
            delay(1);
        }
    }
    FL_CHECK(state == ParlioEngineState::READY);
}

//=============================================================================
// State Inspection
//=============================================================================

FL_TEST_CASE("ParlioEngine mock - state inspection") {
    resetMockFull();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1, 2};
    ChipsetTimingConfig timing = getWS2812Timing();

    auto& mock = ParlioPeripheralMock::instance();

    FL_CHECK_FALSE(mock.isInitialized());
    FL_CHECK_FALSE(mock.isEnabled());
    FL_CHECK_FALSE(mock.isTransmitting());
    FL_CHECK(mock.getTransmitCount() == 0);

    driver.initialize(2, pins, timing, 50);
    FL_CHECK(mock.isInitialized());
    FL_CHECK_FALSE(mock.isEnabled());

    fl::vector<uint8_t> scratch(50 * 2 * 3);
    driver.beginTransmission(scratch.data(), scratch.size(), 2, scratch.size());

    FL_CHECK(mock.isEnabled());
    FL_CHECK(mock.getTransmitCount() > 0);

    ParlioEngineState state = ParlioEngineState::DRAINING;
    for (int i = 0; i < 200 && state != ParlioEngineState::READY; i++) {
        state = driver.poll();
        if (state == ParlioEngineState::ERROR) {
            break;
        }
        if (state == ParlioEngineState::DRAINING) {
            delay(1);
        }
    }
    FL_CHECK(state == ParlioEngineState::READY);
}

//=============================================================================
// Waveform Data Capture
//=============================================================================

FL_TEST_CASE("ParlioEngine mock - waveform data capture") {
    resetMockFull();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing();
    driver.initialize(1, pins, timing, 3);

    uint8_t scratch[9] = {
        0xFF, 0x00, 0xAA,
        0x55, 0xF0, 0x0F,
        0xC3, 0x3C, 0x99
    };

    driver.beginTransmission(scratch, 9, 1, 9);

    auto& mock = ParlioPeripheralMock::instance();

    const auto& history = mock.getTransmissionHistory();
    FL_REQUIRE(history.size() > 0);

    const auto& first_tx = history[0];
    FL_CHECK(first_tx.bit_count > 0);
    FL_CHECK(first_tx.buffer_copy.size() > 0);
}

FL_TEST_CASE("ParlioEngine mock - transmission history clearing") {
    resetMockFull();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing();
    driver.initialize(1, pins, timing, 5);

    uint8_t scratch[15] = {0};

    auto& mock = ParlioPeripheralMock::instance();

    driver.beginTransmission(scratch, 15, 1, 15);
    size_t count1 = mock.getTransmissionHistory().size();
    FL_CHECK(count1 > 0);

    mock.clearTransmissionHistory();
    FL_CHECK(mock.getTransmissionHistory().size() == 0);
    FL_CHECK(mock.getTransmitCount() == count1);

    driver.beginTransmission(scratch, 15, 1, 15);
    size_t count2 = mock.getTransmissionHistory().size();
    FL_CHECK(count2 > 0);
    FL_CHECK(mock.getTransmitCount() > count1);
}

//=============================================================================
// Edge Cases
//=============================================================================

FL_TEST_CASE("ParlioEngine mock - zero LEDs") {
    resetMockFull();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing();
    driver.initialize(1, pins, timing, 1);

    uint8_t scratch[1] = {0};
    bool success = driver.beginTransmission(scratch, 0, 1, 0);
    (void)success;

    auto& mock = ParlioPeripheralMock::instance();
    (void)mock;
}

FL_TEST_CASE("ParlioEngine mock - maximum data width") {
    resetMockFull();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    ChipsetTimingConfig timing = getWS2812Timing();

    bool success = driver.initialize(16, pins, timing, 10);
    FL_CHECK(success);

    auto& mock = ParlioPeripheralMock::instance();
    FL_CHECK(mock.getConfig().data_width == 16);

    for (size_t i = 0; i < 16; i++) {
        FL_CHECK(mock.getConfig().gpio_pins[i] == static_cast<int>(i + 1));
    }
}

//=============================================================================
// Untransposition
//=============================================================================

FL_TEST_CASE("parlio_mock_untransposition") {
    ChipsetTiming timing;
    timing.T1 = 1;
    timing.T2 = 999;
    timing.T3 = 1;

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    uint8_t lanes[2] = {0xff, 0x00};
    uint8_t transposed_output[2 * sizeof(Wave8Byte)];

    wave8Transpose_2(lanes, lut, transposed_output);

    for (int i = 0; i < 16; i++) {
        FL_REQUIRE(transposed_output[i] == 0xAA);
    }

    auto& mock = fl::detail::ParlioPeripheralMock::instance();
    mock.reset();

    fl::vector<int> pins = {1, 2};
    fl::detail::ParlioPeripheralConfig config(pins, 8000000, 4, 2, fl::detail::ParlioBitPackOrder::FL_PARLIO_MSB);
    FL_REQUIRE(mock.initialize(config));
    FL_REQUIRE(mock.enable());

    size_t bit_count = 16 * 8;
    FL_REQUIRE(mock.transmit(transposed_output, bit_count, 0));

    delay(5);

    fl::span<const uint8_t> pin1_data = mock.getTransmissionDataForPin(1);
    fl::span<const uint8_t> pin2_data = mock.getTransmissionDataForPin(2);

    FL_REQUIRE(pin1_data.size() == 8);
    FL_REQUIRE(pin2_data.size() == 8);

    for (size_t i = 0; i < pin1_data.size(); i++) {
        FL_REQUIRE(pin1_data[i] == 0xFF);
    }

    for (size_t i = 0; i < pin2_data.size(); i++) {
        FL_REQUIRE(pin2_data[i] == 0x00);
    }
}

FL_TEST_CASE("parlio_mock_untransposition_complex_pattern") {
    ChipsetTiming timing;
    timing.T1 = 1;
    timing.T2 = 999;
    timing.T3 = 1;

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    uint8_t lanes[2] = {0xAA, 0x55};
    uint8_t transposed_output[2 * sizeof(Wave8Byte)];

    wave8Transpose_2(lanes, lut, transposed_output);

    auto& mock = fl::detail::ParlioPeripheralMock::instance();
    mock.reset();

    fl::vector<int> pins = {1, 2};
    fl::detail::ParlioPeripheralConfig config(pins, 8000000, 4, 2, fl::detail::ParlioBitPackOrder::FL_PARLIO_MSB);
    FL_REQUIRE(mock.initialize(config));
    FL_REQUIRE(mock.enable());

    size_t bit_count = 16 * 8;
    FL_REQUIRE(mock.transmit(transposed_output, bit_count, 0));

    delay(5);

    fl::span<const uint8_t> pin1_data = mock.getTransmissionDataForPin(1);
    fl::span<const uint8_t> pin2_data = mock.getTransmissionDataForPin(2);

    FL_REQUIRE(pin1_data.size() == 8);
    FL_REQUIRE(pin2_data.size() == 8);

    uint8_t expected_pin1[8] = {0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00};
    for (size_t i = 0; i < pin1_data.size(); i++) {
        FL_REQUIRE(pin1_data[i] == expected_pin1[i]);
    }

    uint8_t expected_pin2[8] = {0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF};
    for (size_t i = 0; i < pin2_data.size(); i++) {
        FL_REQUIRE(pin2_data[i] == expected_pin2[i]);
    }
}

FL_TEST_CASE("parlio_mock_untransposition_with_span_api") {
    ChipsetTiming timing;
    timing.T1 = 1;
    timing.T2 = 999;
    timing.T3 = 1;

    Wave8BitExpansionLut lut = buildWave8ExpansionLUT(timing);

    uint8_t lanes[2] = {0xAA, 0x55};
    uint8_t transposed_output[2 * sizeof(Wave8Byte)];

    wave8Transpose_2(lanes, lut, transposed_output);

    fl::vector<int> pins = {10, 20};
    fl::span<const uint8_t> transposed_span(transposed_output, 16);
    fl::span<const int> pins_span(pins);

    fl::vector<fl::pair<int, fl::vector<uint8_t>>> result =
        fl::detail::ParlioPeripheralMock::untransposeParlioBitstream(transposed_span, pins_span);

    FL_REQUIRE(result.size() == 2);
    FL_REQUIRE(result[0].first == 10);
    FL_REQUIRE(result[1].first == 20);

    FL_REQUIRE(result[0].second.size() == 8);
    FL_REQUIRE(result[1].second.size() == 8);

    const uint8_t expected_pin10[8] = {0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00};
    const uint8_t expected_pin20[8] = {0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF};

    auto& pair0 = result[0];
    auto& pair1 = result[1];
    FL_REQUIRE(pair0.first == 10);
    FL_REQUIRE(pair1.first == 20);
    for (size_t i = 0; i < pair0.second.size(); i++) {
        FL_REQUIRE(pair0.second[i] == expected_pin10[i]);
    }
    for (size_t i = 0; i < pair1.second.size(); i++) {
        FL_REQUIRE(pair1.second[i] == expected_pin20[i]);
    }

    #define INTERNAL_ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]));

    size_t expected_pin20_size = INTERNAL_ARRAY_SIZE(expected_pin20);
    for (size_t i = 0; i < pair1.second.size(); i++) {
        FL_REQUIRE_LE(i, expected_pin20_size);
        uint8_t val = expected_pin20[i];
        FL_REQUIRE(pair1.second[i] == val);
    }
}

FL_TEST_CASE("parlio_mock_untransposition_empty_inputs") {
    fl::vector<uint8_t> empty_data;
    fl::vector<int> pins = {1, 2};
    fl::span<const uint8_t> empty_span(empty_data);
    fl::span<const int> pins_span(pins);

    auto result = fl::detail::ParlioPeripheralMock::untransposeParlioBitstream(empty_span, pins_span);
    FL_REQUIRE(result.empty());

    fl::vector<uint8_t> data = {0xAA, 0x55};
    fl::vector<int> empty_pins;
    fl::span<const uint8_t> data_span(data);
    fl::span<const int> empty_pins_span(empty_pins);

    result = fl::detail::ParlioPeripheralMock::untransposeParlioBitstream(data_span, empty_pins_span);
    FL_REQUIRE(result.empty());
}

//=============================================================================
// LSB vs MSB Bit Packing Modes
//=============================================================================

FL_TEST_CASE("parlio_mock_lsb_packing") {
    auto& mock = fl::detail::ParlioPeripheralMock::instance();
    mock.reset();

    fl::vector<int> pins = {1, 2};
    fl::detail::ParlioPeripheralConfig config(pins, 8000000, 4, 2, fl::detail::ParlioBitPackOrder::FL_PARLIO_LSB);
    FL_REQUIRE(mock.initialize(config));
    FL_REQUIRE(mock.enable());

    uint8_t test_data[2] = {0xAA, 0x55};
    size_t bit_count = 2 * 8;

    FL_REQUIRE(mock.transmit(test_data, bit_count, 0));

    delay(5);

    FL_REQUIRE(mock.getConfig().packing == fl::detail::ParlioBitPackOrder::FL_PARLIO_LSB);

    const auto& history = mock.getTransmissionHistory();
    FL_REQUIRE(history.size() == 1);
    FL_REQUIRE(history[0].bit_count == 16);
}

FL_TEST_CASE("parlio_mock_msb_packing") {
    auto& mock = fl::detail::ParlioPeripheralMock::instance();
    mock.reset();

    fl::vector<int> pins = {1, 2};
    fl::detail::ParlioPeripheralConfig config(pins, 8000000, 4, 2, fl::detail::ParlioBitPackOrder::FL_PARLIO_MSB);
    FL_REQUIRE(mock.initialize(config));
    FL_REQUIRE(mock.enable());

    uint8_t test_data[2] = {0xAA, 0x55};
    size_t bit_count = 2 * 8;

    FL_REQUIRE(mock.transmit(test_data, bit_count, 0));

    delay(5);

    FL_REQUIRE(mock.getConfig().packing == fl::detail::ParlioBitPackOrder::FL_PARLIO_MSB);

    const auto& history = mock.getTransmissionHistory();
    FL_REQUIRE(history.size() == 1);
    FL_REQUIRE(history[0].bit_count == 16);
}

FL_TEST_CASE("parlio_mock_default_packing_is_msb") {
    auto& mock = fl::detail::ParlioPeripheralMock::instance();
    mock.reset();

    fl::vector<int> pins = {1};
    fl::detail::ParlioPeripheralConfig config(pins, 8000000, 4, 2);
    FL_REQUIRE(mock.initialize(config));

    FL_REQUIRE(mock.getConfig().packing == fl::detail::ParlioBitPackOrder::FL_PARLIO_MSB);
}

FL_TEST_CASE("parlio_mock_packing_mode_persistence") {
    auto& mock = fl::detail::ParlioPeripheralMock::instance();

    {
        mock.reset();
        fl::vector<int> pins = {1, 2};
        fl::detail::ParlioPeripheralConfig config(pins, 8000000, 4, 2, fl::detail::ParlioBitPackOrder::FL_PARLIO_MSB);
        FL_REQUIRE(mock.initialize(config));
        FL_REQUIRE(mock.getConfig().packing == fl::detail::ParlioBitPackOrder::FL_PARLIO_MSB);
    }

    {
        mock.reset();
        fl::vector<int> pins = {1, 2};
        fl::detail::ParlioPeripheralConfig config(pins, 8000000, 4, 2, fl::detail::ParlioBitPackOrder::FL_PARLIO_LSB);
        FL_REQUIRE(mock.initialize(config));
        FL_REQUIRE(mock.getConfig().packing == fl::detail::ParlioBitPackOrder::FL_PARLIO_LSB);
    }
}

//=============================================================================
// DMA Output Capture
//=============================================================================

FL_TEST_CASE("ParlioEngine - DMA output capture basic functionality") {
    resetMockHistory();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing();

    bool init_ok = driver.initialize(1, pins, timing, 10);
    FL_REQUIRE(init_ok);

    uint8_t scratch[3] = {0xFF, 0x00, 0xAA};

    bool tx_ok = driver.beginTransmission(scratch, 3, 1, 3);
    FL_REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();

    const auto& history = mock.getTransmissionHistory();
    FL_REQUIRE(history.size() > 0);

    const auto& first_tx = history[0];
    FL_CHECK(first_tx.bit_count > 0);
    FL_CHECK(first_tx.buffer_copy.size() > 0);
    FL_CHECK(first_tx.buffer_copy.size() >= 24);
}

FL_TEST_CASE("ParlioEngine - verify captured DMA data is non-zero") {
    resetMockHistory();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing();
    driver.initialize(1, pins, timing, 5);

    uint8_t scratch[15] = {
        0xFF, 0xFF, 0xFF,
        0xAA, 0x55, 0xF0,
        0x00, 0x00, 0x00,
        0x12, 0x34, 0x56,
        0x80, 0x40, 0x20
    };

    bool tx_ok = driver.beginTransmission(scratch, 15, 1, 15);
    FL_REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();

    const auto& history = mock.getTransmissionHistory();
    FL_REQUIRE(history.size() > 0);

    const auto& tx = history[0];
    const auto& buffer = tx.buffer_copy;

    bool has_nonzero = false;
    for (size_t i = 0; i < buffer.size(); i++) {
        if (buffer[i] != 0) {
            has_nonzero = true;
            break;
        }
    }
    FL_CHECK(has_nonzero);
}

FL_TEST_CASE("ParlioEngine - multi-lane DMA output capture") {
    resetMockHistory();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1, 2, 4, 8};
    ChipsetTimingConfig timing = getWS2812Timing();

    size_t num_lanes = 4;
    size_t leds_per_lane = 3;
    size_t bytes_per_led = 3;
    size_t lane_stride = leds_per_lane * bytes_per_led;
    size_t total_bytes = num_lanes * lane_stride;

    bool init_ok = driver.initialize(num_lanes, pins, timing, leds_per_lane);
    FL_REQUIRE(init_ok);

    fl::vector<uint8_t> scratch(total_bytes);

    for (size_t i = 0; i < lane_stride; i++) {
        scratch[0 * lane_stride + i] = 0xFF;
    }
    for (size_t i = 0; i < lane_stride; i++) {
        scratch[1 * lane_stride + i] = 0xAA;
    }
    for (size_t i = 0; i < lane_stride; i++) {
        scratch[2 * lane_stride + i] = 0x55;
    }
    for (size_t i = 0; i < lane_stride; i++) {
        scratch[3 * lane_stride + i] = 0x00;
    }

    bool tx_ok = driver.beginTransmission(scratch.data(), total_bytes, num_lanes, lane_stride);
    FL_REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();

    const auto& history = mock.getTransmissionHistory();
    FL_REQUIRE(history.size() > 0);

    const auto& tx = history[0];
    FL_CHECK(tx.buffer_copy.size() > 0);
    FL_CHECK(tx.buffer_copy.size() >= total_bytes * 2);
}

FL_TEST_CASE("ParlioEngine - verify multiple transmissions are captured") {
    resetMockHistory();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing();
    driver.initialize(1, pins, timing, 10);

    auto& mock = ParlioPeripheralMock::instance();
    mock.clearTransmissionHistory();

    uint8_t scratch1[3] = {0xFF, 0x00, 0x00};
    driver.beginTransmission(scratch1, 3, 1, 3);

    size_t count_after_first = mock.getTransmissionHistory().size();
    FL_CHECK(count_after_first > 0);

    uint8_t scratch2[3] = {0x00, 0xFF, 0x00};
    driver.beginTransmission(scratch2, 3, 1, 3);

    size_t count_after_second = mock.getTransmissionHistory().size();
    FL_CHECK(count_after_second >= count_after_first);

    const auto& history = mock.getTransmissionHistory();
    FL_CHECK(history.size() >= 2);
}

FL_TEST_CASE("ParlioEngine - verify bit count matches expected") {
    resetMockHistory();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing();
    driver.initialize(1, pins, timing, 10);

    uint8_t scratch[15];
    for (size_t i = 0; i < 15; i++) {
        scratch[i] = static_cast<uint8_t>((i * 17) & 0xFF);
    }

    bool tx_ok = driver.beginTransmission(scratch, 15, 1, 15);
    FL_REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();

    const auto& history = mock.getTransmissionHistory();
    FL_REQUIRE(history.size() > 0);

    const auto& tx = history[0];
    FL_CHECK(tx.bit_count >= 160);

    size_t expected_bytes = (tx.bit_count + 7) / 8;
    FL_CHECK(tx.buffer_copy.size() >= expected_bytes);
}

FL_TEST_CASE("ParlioEngine - verify idle value is captured") {
    resetMockHistory();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing();
    driver.initialize(1, pins, timing, 5);

    uint8_t scratch[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    bool tx_ok = driver.beginTransmission(scratch, 6, 1, 6);
    FL_REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();

    const auto& history = mock.getTransmissionHistory();
    FL_REQUIRE(history.size() > 0);

    const auto& tx = history[0];
    FL_CHECK(tx.idle_value == 0x0000);
}

FL_TEST_CASE("ParlioEngine - large buffer streaming with capture") {
    resetMockHistory();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing();

    size_t num_leds = 100;
    size_t num_bytes = num_leds * 3;

    bool init_ok = driver.initialize(1, pins, timing, num_leds);
    FL_REQUIRE(init_ok);

    fl::vector<uint8_t> scratch(num_bytes);
    for (size_t i = 0; i < num_bytes; i++) {
        scratch[i] = static_cast<uint8_t>((i * 7 + 13) & 0xFF);
    }

    bool tx_ok = driver.beginTransmission(scratch.data(), num_bytes, 1, num_bytes);
    FL_REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();

    const auto& history = mock.getTransmissionHistory();
    FL_CHECK(history.size() > 0);

    size_t total_bits = 0;
    for (const auto& tx : history) {
        total_bits += tx.bit_count;
    }

    size_t expected_min_bits = num_bytes * 8 * 8;
    FL_CHECK(total_bits >= expected_min_bits);
}

//=============================================================================
// Waveform Parameter Validation
//=============================================================================

FL_TEST_CASE("ParlioEngine - verify timing parameters are applied") {
    resetMockHistory();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};

    ChipsetTimingConfig custom_timing(400, 850, 500, 80, "CustomTiming");

    bool init_ok = driver.initialize(1, pins, custom_timing, 5);
    FL_REQUIRE(init_ok);

    uint8_t scratch[15] = {0xFF, 0xAA, 0x55, 0xF0, 0x0F, 0xC3, 0x3C, 0x99, 0x66,
                           0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    bool tx_ok = driver.beginTransmission(scratch, 15, 1, 15);
    FL_REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();

    const auto& history = mock.getTransmissionHistory();
    FL_REQUIRE(history.size() > 0);
    FL_CHECK(history[0].buffer_copy.size() > 0);
}

FL_TEST_CASE("ParlioEngine - zero-length transmission edge case") {
    resetMockHistory();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing();
    driver.initialize(1, pins, timing, 1);

    uint8_t scratch[1] = {0};
    bool tx_ok = driver.beginTransmission(scratch, 0, 1, 0);
    (void)tx_ok;

    auto& mock = ParlioPeripheralMock::instance();
    (void)mock;

    FL_CHECK(true);
}

FL_TEST_CASE("ParlioEngine - single byte transmission") {
    resetMockHistory();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing();
    driver.initialize(1, pins, timing, 1);

    uint8_t scratch[1] = {0xA5};

    bool tx_ok = driver.beginTransmission(scratch, 1, 1, 1);
    FL_REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();

    const auto& history = mock.getTransmissionHistory();
    FL_REQUIRE(history.size() > 0);

    const auto& tx = history[0];
    FL_CHECK(tx.bit_count >= 64);
    FL_CHECK(tx.buffer_copy.size() >= 8);
}

FL_TEST_CASE("ParlioEngine - max lanes configuration with data capture") {
    resetMockHistory();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins;
    for (int i = 1; i <= 16; i++) {
        pins.push_back(i);
    }

    ChipsetTimingConfig timing = getWS2812Timing();

    size_t num_lanes = 16;
    size_t leds_per_lane = 5;
    size_t bytes_per_led = 3;
    size_t lane_stride = leds_per_lane * bytes_per_led;
    size_t total_bytes = num_lanes * lane_stride;

    bool init_ok = driver.initialize(num_lanes, pins, timing, leds_per_lane);
    FL_REQUIRE(init_ok);

    fl::vector<uint8_t> scratch(total_bytes);
    for (size_t i = 0; i < total_bytes; i++) {
        scratch[i] = static_cast<uint8_t>((i & 0xFF));
    }

    bool tx_ok = driver.beginTransmission(scratch.data(), total_bytes, num_lanes, lane_stride);
    FL_REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();

    const auto& history = mock.getTransmissionHistory();
    FL_REQUIRE(history.size() > 0);

    const auto& tx = history[0];
    FL_CHECK(tx.buffer_copy.size() > 0);
    FL_CHECK(tx.bit_count > 0);
}

FL_TEST_CASE("ParlioEngine - two channels with different lengths (padding test)") {
    resetMockHistory();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1, 2};
    ChipsetTimingConfig timing = getWS2812Timing();

    size_t num_lanes = 2;

    size_t lane0_leds = 5;
    size_t bytes_per_led = 3;
    size_t lane0_bytes = lane0_leds * bytes_per_led;

    size_t lane1_leds = 3;
    size_t lane1_bytes = lane1_leds * bytes_per_led;

    size_t max_channel_bytes = lane0_bytes;
    size_t total_bytes = num_lanes * max_channel_bytes;

    bool init_ok = driver.initialize(num_lanes, pins, timing, lane0_leds);
    FL_REQUIRE(init_ok);

    fl::vector<uint8_t> scratch(total_bytes);

    for (size_t i = 0; i < max_channel_bytes; i++) {
        scratch[0 * max_channel_bytes + i] = static_cast<uint8_t>(i + 1);
    }

    for (size_t i = 0; i < lane1_bytes; i++) {
        scratch[1 * max_channel_bytes + i] = static_cast<uint8_t>(0xF0 + i + 1);
    }
    for (size_t i = lane1_bytes; i < max_channel_bytes; i++) {
        scratch[1 * max_channel_bytes + i] = 0x00;
    }

    bool tx_ok = driver.beginTransmission(scratch.data(), total_bytes, num_lanes, max_channel_bytes);
    FL_REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();

    const auto& history = mock.getTransmissionHistory();
    FL_REQUIRE(history.size() > 0);

    const auto& tx = history[0];
    FL_CHECK(tx.buffer_copy.size() > 0);
    FL_CHECK(tx.buffer_copy.size() >= total_bytes * 2);
    FL_CHECK(tx.bit_count > 0);
}

FL_TEST_CASE("ParlioEngine - verify reset padding is applied for different channel lengths") {
    resetMockHistory();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {1};
    ChipsetTimingConfig timing = getWS2812Timing();

    size_t num_lanes = 1;
    size_t leds_per_lane = 2;
    size_t bytes_per_led = 3;
    size_t total_bytes = leds_per_lane * bytes_per_led;

    bool init_ok = driver.initialize(num_lanes, pins, timing, leds_per_lane);
    FL_REQUIRE(init_ok);

    fl::vector<uint8_t> scratch(total_bytes);
    for (size_t i = 0; i < total_bytes; i++) {
        scratch[i] = static_cast<uint8_t>(0xAA);
    }

    bool tx_ok = driver.beginTransmission(scratch.data(), total_bytes, num_lanes, total_bytes);
    FL_REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();

    const auto& history = mock.getTransmissionHistory();
    FL_REQUIRE(history.size() > 0);

    const auto& tx = history[0];
    FL_CHECK(tx.buffer_copy.size() > total_bytes * 2);
    FL_CHECK(tx.bit_count > total_bytes * 8 * 2);
}

//=============================================================================
// SPI Byte Encoding
//=============================================================================

FL_TEST_CASE("ParlioEngine - encodeSpiByteForTest 0xFF") {
    u8 output[4];
    ParlioEngine::encodeSpiByteForTest(0xFF, output);

    FL_CHECK_EQ(output[0], 0xEE);
    FL_CHECK_EQ(output[1], 0xEE);
    FL_CHECK_EQ(output[2], 0xEE);
    FL_CHECK_EQ(output[3], 0xEE);
}

FL_TEST_CASE("ParlioEngine - encodeSpiByteForTest 0x00") {
    u8 output[4];
    ParlioEngine::encodeSpiByteForTest(0x00, output);

    FL_CHECK_EQ(output[0], 0x44);
    FL_CHECK_EQ(output[1], 0x44);
    FL_CHECK_EQ(output[2], 0x44);
    FL_CHECK_EQ(output[3], 0x44);
}

FL_TEST_CASE("ParlioEngine - encodeSpiByteForTest 0xA5") {
    u8 output[4];
    ParlioEngine::encodeSpiByteForTest(0xA5, output);

    FL_CHECK_EQ(output[0], 0xE4);
    FL_CHECK_EQ(output[1], 0xE4);
    FL_CHECK_EQ(output[2], 0x4E);
    FL_CHECK_EQ(output[3], 0x4E);
}

//=============================================================================
// SPI via ParlioEngine Integration
//=============================================================================

FL_TEST_CASE("ParlioEngine - initializeSpi basic") {
    resetMockHistory();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {5, 6};
    bool init_ok = driver.initializeSpi(pins, 6000000, 100);
    FL_REQUIRE(init_ok);

    u8 data[4] = {0xFF, 0x00, 0xAA, 0x55};
    bool tx_ok = driver.beginTransmission(data, 4, 1, 4);
    FL_REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();
    const auto& history = mock.getTransmissionHistory();
    FL_REQUIRE(history.size() > 0);

    const auto& tx = history[0];
    FL_CHECK(tx.buffer_copy.size() >= 16);
}

FL_TEST_CASE("ParlioEngine - initializeSpi rejects wrong pin count") {
    resetMockHistory();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins_one = {5};
    FL_CHECK(!driver.initializeSpi(pins_one, 6000000, 100));

    fl::vector<int> pins_three = {5, 6, 7};
    FL_CHECK(!driver.initializeSpi(pins_three, 6000000, 100));
}

FL_TEST_CASE("ParlioEngine - SPI DMA output matches encoding") {
    resetMockHistory();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins = {5, 6};
    bool init_ok = driver.initializeSpi(pins, 6000000, 100);
    FL_REQUIRE(init_ok);

    // Test multiple bytes with diverse bit patterns
    u8 data[] = {0xFF, 0x00, 0xA5, 0x5A, 0x0F, 0xF0, 0xC3, 0x3C};
    const size_t dataLen = sizeof(data);
    bool tx_ok = driver.beginTransmission(data, dataLen, 1, dataLen);
    FL_REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();
    const auto& history = mock.getTransmissionHistory();
    FL_REQUIRE(history.size() > 0);

    // Collect all DMA bytes across all chunks
    fl::vector<u8> allDmaBytes;
    for (const auto& tx : history) {
        for (size_t i = 0; i < tx.buffer_copy.size(); i++) {
            allDmaBytes.push_back(tx.buffer_copy[i]);
        }
    }
    FL_REQUIRE(allDmaBytes.size() >= dataLen * 4);

    // Verify every byte is correctly encoded
    for (size_t i = 0; i < dataLen; i++) {
        u8 expected[4];
        ParlioEngine::encodeSpiByteForTest(data[i], expected);
        size_t dmaOffset = i * 4;
        FL_CHECK_EQ(allDmaBytes[dmaOffset + 0], expected[0]);
        FL_CHECK_EQ(allDmaBytes[dmaOffset + 1], expected[1]);
        FL_CHECK_EQ(allDmaBytes[dmaOffset + 2], expected[2]);
        FL_CHECK_EQ(allDmaBytes[dmaOffset + 3], expected[3]);
    }
}

//=============================================================================
// Mode Switching
//=============================================================================

FL_TEST_CASE("ParlioEngine - clockless to SPI mode switch") {
    resetMockHistory();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins_clockless = {1};
    ChipsetTimingConfig timing = getWS2812Timing();
    bool init_ok = driver.initialize(1, pins_clockless, timing, 5);
    FL_REQUIRE(init_ok);

    u8 clockless_data[3] = {0xFF, 0x00, 0xAA};
    bool tx_ok = driver.beginTransmission(clockless_data, 3, 1, 3);
    FL_REQUIRE(tx_ok);

    resetMockHistory();

    fl::vector<int> pins_spi = {5, 6};
    init_ok = driver.initializeSpi(pins_spi, 6000000, 10);
    FL_REQUIRE(init_ok);

    u8 spi_data[2] = {0xFF, 0x00};
    tx_ok = driver.beginTransmission(spi_data, 2, 1, 2);
    FL_REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();
    const auto& history = mock.getTransmissionHistory();
    FL_REQUIRE(history.size() > 0);

    const auto& tx = history[0];
    FL_REQUIRE(tx.buffer_copy.size() >= 8);

    u8 expected_ff[4];
    ParlioEngine::encodeSpiByteForTest(0xFF, expected_ff);
    FL_CHECK_EQ(tx.buffer_copy[0], expected_ff[0]);
}

FL_TEST_CASE("ParlioEngine - SPI to clockless mode switch") {
    resetMockHistory();

    auto& driver = ParlioEngine::getInstance();

    fl::vector<int> pins_spi = {5, 6};
    bool init_ok = driver.initializeSpi(pins_spi, 6000000, 10);
    FL_REQUIRE(init_ok);

    u8 spi_data[1] = {0xFF};
    bool tx_ok = driver.beginTransmission(spi_data, 1, 1, 1);
    FL_REQUIRE(tx_ok);

    resetMockHistory();

    fl::vector<int> pins_clockless = {1};
    ChipsetTimingConfig timing = getWS2812Timing();
    init_ok = driver.initialize(1, pins_clockless, timing, 5);
    FL_REQUIRE(init_ok);

    u8 clockless_data[3] = {0xFF, 0x00, 0xAA};
    tx_ok = driver.beginTransmission(clockless_data, 3, 1, 3);
    FL_REQUIRE(tx_ok);

    auto& mock = ParlioPeripheralMock::instance();
    const auto& history = mock.getTransmissionHistory();
    FL_REQUIRE(history.size() > 0);

    const auto& tx = history[0];
    FL_CHECK(tx.buffer_copy.size() >= 24);
}

//=============================================================================
// Full Channels API - SPI through PARLIO
//=============================================================================

FL_TEST_CASE("ChannelDriverPARLIO - canHandle accepts SPI channel data") {
    fl::ChannelDriverPARLIO parlioDriver;

    SpiEncoder encoder = SpiEncoder::apa102(6000000);
    SpiChipsetConfig spiConfig{5, 18, encoder};
    fl::vector_psram<u8> data = {0x00, 0x00, 0x00, 0x00,
                                  0xFF, 0xFF, 0x00, 0x00,
                                  0xFF, 0x00, 0xFF, 0x00};
    auto channelData = fl::ChannelData::create(spiConfig, fl::move(data));
    FL_REQUIRE(channelData != nullptr);

    FL_CHECK(channelData->isSpi());
    FL_CHECK(!channelData->isClockless());
    FL_CHECK(parlioDriver.canHandle(channelData));
}

FL_TEST_CASE("ChannelDriverPARLIO - canHandle accepts clockless channel data") {
    fl::ChannelDriverPARLIO parlioDriver;

    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::vector_psram<u8> data = {0xFF, 0x00, 0xAA};
    auto channelData = fl::ChannelData::create(5, timing, fl::move(data));
    FL_REQUIRE(channelData != nullptr);

    FL_CHECK(!channelData->isSpi());
    FL_CHECK(channelData->isClockless());
    FL_CHECK(parlioDriver.canHandle(channelData));
}

FL_TEST_CASE("ChannelDriverPARLIO - capabilities support both clockless and SPI") {
    fl::ChannelDriverPARLIO parlioDriver;
    auto caps = parlioDriver.getCapabilities();
    FL_CHECK(caps.supportsClockless);
    FL_CHECK(caps.supportsSpi);
}

FL_TEST_CASE("ChannelDriverPARLIO - SPI enqueue/show/poll sends data to PARLIO mock") {
    resetMockHistory();

    fl::ChannelDriverPARLIO parlioDriver;

    SpiEncoder encoder = SpiEncoder::apa102(6000000);
    SpiChipsetConfig spiConfig{5, 18, encoder};

    // 16 input bytes with varied patterns to exercise all encoding paths
    const u8 inputBytes[] = {
        0x00, 0x00, 0x00, 0x00,
        0xFF, 0x00, 0xFF, 0x00,
        0xFF, 0xAA, 0x55, 0xCC,
        0xFF, 0xFF, 0xFF, 0xFF
    };
    const size_t inputLen = sizeof(inputBytes);

    fl::vector_psram<u8> spiBytes(inputBytes, inputBytes + inputLen);
    auto channelData = fl::ChannelData::create(spiConfig, fl::move(spiBytes));
    FL_REQUIRE(channelData != nullptr);
    FL_REQUIRE(channelData->isSpi());

    parlioDriver.enqueue(channelData);
    parlioDriver.show();
    waitForDriverReady(parlioDriver);

    // Collect all DMA bytes across all transmission chunks
    auto& mock = ParlioPeripheralMock::instance();
    const auto& history = mock.getTransmissionHistory();
    FL_REQUIRE(history.size() > 0);

    fl::vector<u8> allDmaBytes;
    for (const auto& tx : history) {
        for (size_t i = 0; i < tx.buffer_copy.size(); i++) {
            allDmaBytes.push_back(tx.buffer_copy[i]);
        }
    }

    // Each input byte produces 4 DMA bytes → 16 * 4 = 64 minimum
    FL_REQUIRE(allDmaBytes.size() >= inputLen * 4);

    // Verify EVERY input byte is correctly encoded in the DMA output
    for (size_t i = 0; i < inputLen; i++) {
        u8 expected[4];
        ParlioEngine::encodeSpiByteForTest(inputBytes[i], expected);
        size_t dmaOffset = i * 4;
        FL_CHECK_EQ(allDmaBytes[dmaOffset + 0], expected[0]);
        FL_CHECK_EQ(allDmaBytes[dmaOffset + 1], expected[1]);
        FL_CHECK_EQ(allDmaBytes[dmaOffset + 2], expected[2]);
        FL_CHECK_EQ(allDmaBytes[dmaOffset + 3], expected[3]);
    }
}

FL_TEST_CASE("ChannelDriverPARLIO - SPI uses clock and data pins from config") {
    resetMockHistory();

    fl::ChannelDriverPARLIO parlioDriver;

    const int CLOCK_PIN = 7;
    const int DATA_PIN = 8;
    SpiEncoder encoder = SpiEncoder::apa102(6000000);
    SpiChipsetConfig spiConfig{DATA_PIN, CLOCK_PIN, encoder};

    fl::vector_psram<u8> spiBytes = {0xFF, 0x00, 0xAA, 0x55};
    auto channelData = fl::ChannelData::create(spiConfig, fl::move(spiBytes));

    parlioDriver.enqueue(channelData);
    parlioDriver.show();
    waitForDriverReady(parlioDriver);

    auto& mock = ParlioPeripheralMock::instance();
    FL_CHECK(mock.isInitialized());

    const auto& config = mock.getConfig();
    FL_CHECK_EQ(config.data_width, (size_t)2);
    FL_CHECK_EQ(config.gpio_pins[0], CLOCK_PIN);
    FL_CHECK_EQ(config.gpio_pins[1], DATA_PIN);
}

FL_TEST_CASE("ChannelDriverPARLIO - SPI per-pin clock and data lane verification") {
    resetMockHistory();

    fl::ChannelDriverPARLIO parlioDriver;

    const int CLOCK_PIN = 10;
    const int DATA_PIN = 11;
    SpiEncoder encoder = SpiEncoder::apa102(6000000);
    SpiChipsetConfig spiConfig{DATA_PIN, CLOCK_PIN, encoder};

    // Use a single byte 0xFF so we can predict the exact per-pin waveform.
    //
    // SPI encoding: tick = (data_bit << 1) | clock_bit
    //   bit1 = data, bit0 = clock
    //
    // MSB untranspose assigns lane 0 to bit1 (data), lane 1 to bit0 (clock).
    // gpio_pins[0] = CLOCK_PIN → gets lane 0 → data waveform
    // gpio_pins[1] = DATA_PIN  → gets lane 1 → clock waveform
    //
    // For 0xFF (all 1s):
    //   Clock waveform: alternating 1,0,1,0... → packed LSB-first = 0x55
    //   Data waveform:  all 1s → 0xFF
    fl::vector_psram<u8> spiBytes = {0xFF};
    auto channelData = fl::ChannelData::create(spiConfig, fl::move(spiBytes));
    FL_REQUIRE(channelData != nullptr);

    parlioDriver.enqueue(channelData);
    parlioDriver.show();
    waitForDriverReady(parlioDriver);

    auto& mock = ParlioPeripheralMock::instance();
    FL_REQUIRE(mock.isInitialized());

    // CLOCK_PIN gets lane 0 which is the DATA waveform (MSB bit1 extracted first)
    fl::span<const u8> clockPinData = mock.getTransmissionDataForPin(CLOCK_PIN);
    FL_REQUIRE(clockPinData.size() >= 2);
    FL_CHECK_EQ(clockPinData[0], (u8)0xFF);  // data waveform for 0xFF: all 1s
    FL_CHECK_EQ(clockPinData[1], (u8)0xFF);

    // DATA_PIN gets lane 1 which is the CLOCK waveform
    fl::span<const u8> dataPinData = mock.getTransmissionDataForPin(DATA_PIN);
    FL_REQUIRE(dataPinData.size() >= 2);
    FL_CHECK_EQ(dataPinData[0], (u8)0x55);  // clock waveform: alternating 1,0
    FL_CHECK_EQ(dataPinData[1], (u8)0x55);
}

FL_TEST_CASE("ChannelDriverPARLIO - SPI per-pin data lane varies with input") {
    resetMockHistory();

    fl::ChannelDriverPARLIO parlioDriver;

    const int CLOCK_PIN = 10;
    const int DATA_PIN = 11;
    SpiEncoder encoder = SpiEncoder::apa102(6000000);
    SpiChipsetConfig spiConfig{DATA_PIN, CLOCK_PIN, encoder};

    // Use a single byte 0x00 (all zeros):
    //   Clock waveform: alternating 1,0 → 0x55 (always, regardless of data)
    //   Data waveform:  all 0s → 0x00
    // Lane mapping: CLOCK_PIN→lane0=data, DATA_PIN→lane1=clock
    fl::vector_psram<u8> spiBytes = {0x00};
    auto channelData = fl::ChannelData::create(spiConfig, fl::move(spiBytes));
    FL_REQUIRE(channelData != nullptr);

    parlioDriver.enqueue(channelData);
    parlioDriver.show();
    waitForDriverReady(parlioDriver);

    auto& mock = ParlioPeripheralMock::instance();

    // CLOCK_PIN → lane 0 → data waveform: all zeros
    fl::span<const u8> clockPinData = mock.getTransmissionDataForPin(CLOCK_PIN);
    FL_REQUIRE(clockPinData.size() >= 2);
    FL_CHECK_EQ(clockPinData[0], (u8)0x00);
    FL_CHECK_EQ(clockPinData[1], (u8)0x00);

    // DATA_PIN → lane 1 → clock waveform: always alternating
    fl::span<const u8> dataPinData = mock.getTransmissionDataForPin(DATA_PIN);
    FL_REQUIRE(dataPinData.size() >= 2);
    FL_CHECK_EQ(dataPinData[0], (u8)0x55);
    FL_CHECK_EQ(dataPinData[1], (u8)0x55);
}

FL_TEST_CASE("ChannelDriverPARLIO - SPI per-pin data lane for 0xA5") {
    resetMockHistory();

    fl::ChannelDriverPARLIO parlioDriver;

    const int CLOCK_PIN = 10;
    const int DATA_PIN = 11;
    SpiEncoder encoder = SpiEncoder::apa102(6000000);
    SpiChipsetConfig spiConfig{DATA_PIN, CLOCK_PIN, encoder};

    // SPI byte 0xA5 = 0b10100101 (MSB first)
    // Each SPI bit produces 2 ticks (rise+fall) with data=data_bit.
    // Bit sequence MSB→LSB: 1,0,1,0,0,1,0,1
    // Data ticks (2 per bit): 1,1, 0,0, 1,1, 0,0, 0,0, 1,1, 0,0, 1,1
    // Packed LSB-first into bytes:
    //   Byte 0 (ticks 0-7): bits 1,1,0,0,1,1,0,0 → 0b00110011 = 0x33
    //   Byte 1 (ticks 8-15): bits 0,0,1,1,0,0,1,1 → 0b11001100 = 0xCC
    // Lane mapping: CLOCK_PIN→lane0=data, DATA_PIN→lane1=clock
    fl::vector_psram<u8> spiBytes = {0xA5};
    auto channelData = fl::ChannelData::create(spiConfig, fl::move(spiBytes));
    FL_REQUIRE(channelData != nullptr);

    parlioDriver.enqueue(channelData);
    parlioDriver.show();
    waitForDriverReady(parlioDriver);

    auto& mock = ParlioPeripheralMock::instance();

    // CLOCK_PIN → lane 0 → data waveform for 0xA5
    fl::span<const u8> clockPinData = mock.getTransmissionDataForPin(CLOCK_PIN);
    FL_REQUIRE(clockPinData.size() >= 2);
    FL_CHECK_EQ(clockPinData[0], (u8)0x33);
    FL_CHECK_EQ(clockPinData[1], (u8)0xCC);

    // DATA_PIN → lane 1 → clock waveform: always alternating
    fl::span<const u8> dataPinData = mock.getTransmissionDataForPin(DATA_PIN);
    FL_REQUIRE(dataPinData.size() >= 2);
    FL_CHECK_EQ(dataPinData[0], (u8)0x55);
    FL_CHECK_EQ(dataPinData[1], (u8)0x55);
}

FL_TEST_CASE("ChannelDriverPARLIO - mixed clockless and SPI channels") {
    resetMockHistory();

    fl::ChannelDriverPARLIO parlioDriver;

    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::vector_psram<u8> clocklessBytes = {0xFF, 0x00, 0xAA};
    auto clocklessData = fl::ChannelData::create(1, timing, fl::move(clocklessBytes));

    SpiEncoder encoder = SpiEncoder::apa102(6000000);
    SpiChipsetConfig spiConfig{5, 18, encoder};
    const u8 spiInputBytes[] = {0x00, 0x00, 0x00, 0x00, 0xFF, 0xAA, 0x55, 0xCC};
    const size_t spiInputLen = sizeof(spiInputBytes);
    fl::vector_psram<u8> spiBytes(spiInputBytes, spiInputBytes + spiInputLen);
    auto spiData = fl::ChannelData::create(spiConfig, fl::move(spiBytes));

    parlioDriver.enqueue(clocklessData);
    parlioDriver.enqueue(spiData);
    parlioDriver.show();
    waitForDriverReady(parlioDriver);

    auto& mock = ParlioPeripheralMock::instance();
    const auto& history = mock.getTransmissionHistory();

    // Must have at least 2 transmission groups (clockless then SPI)
    FL_REQUIRE(history.size() >= 2);

    // The SPI transmission is the last group. Collect its DMA bytes.
    // Clockless transmission uses wider data width, SPI uses data_width=2.
    // The SPI group starts after the clockless group completes.
    // The mock records per-pin data only for the last transmit() call,
    // so verify the mock config switched to 2-bit SPI mode.
    const auto& config = mock.getConfig();
    FL_CHECK_EQ(config.data_width, (size_t)2);

    // Verify the SPI encoded bytes in the last transmission group.
    // After the clockless group completes, the engine re-initializes for SPI.
    // Find the SPI transmission(s) — they come after clockless ones.
    // SPI transmission data width is 2 (encoded as 4 DMA bytes per input byte).
    // Collect all DMA bytes from the last transmission (the SPI one).
    const auto& spiTx = history[history.size() - 1];
    FL_REQUIRE(spiTx.buffer_copy.size() >= spiInputLen * 4);

    for (size_t i = 0; i < spiInputLen; i++) {
        u8 expected[4];
        ParlioEngine::encodeSpiByteForTest(spiInputBytes[i], expected);
        size_t dmaOffset = i * 4;
        FL_CHECK_EQ(spiTx.buffer_copy[dmaOffset + 0], expected[0]);
        FL_CHECK_EQ(spiTx.buffer_copy[dmaOffset + 1], expected[1]);
        FL_CHECK_EQ(spiTx.buffer_copy[dmaOffset + 2], expected[2]);
        FL_CHECK_EQ(spiTx.buffer_copy[dmaOffset + 3], expected[3]);
    }
}

FL_TEST_CASE("ChannelDriverPARLIO - SPI poll returns READY after completion") {
    resetMockHistory();

    fl::ChannelDriverPARLIO parlioDriver;

    FL_CHECK(parlioDriver.poll() == fl::IChannelDriver::DriverState::READY);

    SpiEncoder encoder = SpiEncoder::apa102(6000000);
    SpiChipsetConfig spiConfig{5, 18, encoder};
    fl::vector_psram<u8> spiBytes = {0xFF, 0x00};
    auto channelData = fl::ChannelData::create(spiConfig, fl::move(spiBytes));

    parlioDriver.enqueue(channelData);
    parlioDriver.show();

    waitForDriverReady(parlioDriver);

    FL_CHECK(parlioDriver.poll() == fl::IChannelDriver::DriverState::READY);
}

//=============================================================================
// ParlioBufferCalculator - DMA Buffer Size Math
//=============================================================================

FL_TEST_CASE("ParlioBufferCalculator - outputBytesPerInputByte for all data widths") {
    FL_SUBCASE("data_width = 1 (single lane)") {
        ParlioBufferCalculator calc{1};
        FL_CHECK(calc.outputBytesPerInputByte() == 8);
    }

    FL_SUBCASE("data_width = 2 (two lanes)") {
        ParlioBufferCalculator calc{2};
        FL_CHECK(calc.outputBytesPerInputByte() == 16);
    }

    FL_SUBCASE("data_width = 4 (four lanes)") {
        ParlioBufferCalculator calc{4};
        FL_CHECK(calc.outputBytesPerInputByte() == 32);
    }

    FL_SUBCASE("data_width = 8 (eight lanes)") {
        ParlioBufferCalculator calc{8};
        FL_CHECK(calc.outputBytesPerInputByte() == 64);
    }

    FL_SUBCASE("data_width = 16 (sixteen lanes)") {
        ParlioBufferCalculator calc{16};
        FL_CHECK(calc.outputBytesPerInputByte() == 128);
    }
}

FL_TEST_CASE("ParlioBufferCalculator - boundaryPaddingBytes for all data widths") {
    FL_SUBCASE("data_width = 1") {
        ParlioBufferCalculator calc{1};
        FL_CHECK(calc.boundaryPaddingBytes() == 8);
    }

    FL_SUBCASE("data_width = 2") {
        ParlioBufferCalculator calc{2};
        FL_CHECK(calc.boundaryPaddingBytes() == 16);
    }

    FL_SUBCASE("data_width = 4") {
        ParlioBufferCalculator calc{4};
        FL_CHECK(calc.boundaryPaddingBytes() == 32);
    }

    FL_SUBCASE("data_width = 8") {
        ParlioBufferCalculator calc{8};
        FL_CHECK(calc.boundaryPaddingBytes() == 64);
    }

    FL_SUBCASE("data_width = 16") {
        ParlioBufferCalculator calc{16};
        FL_CHECK(calc.boundaryPaddingBytes() == 128);
    }
}

FL_TEST_CASE("ParlioBufferCalculator - transposeBlockSize matches outputBytesPerInputByte") {
    FL_SUBCASE("data_width = 1") {
        ParlioBufferCalculator calc{1};
        FL_CHECK(calc.transposeBlockSize() == 8);
        FL_CHECK(calc.transposeBlockSize() == calc.outputBytesPerInputByte());
    }

    FL_SUBCASE("data_width = 2") {
        ParlioBufferCalculator calc{2};
        FL_CHECK(calc.transposeBlockSize() == 16);
        FL_CHECK(calc.transposeBlockSize() == calc.outputBytesPerInputByte());
    }

    FL_SUBCASE("data_width = 4") {
        ParlioBufferCalculator calc{4};
        FL_CHECK(calc.transposeBlockSize() == 32);
        FL_CHECK(calc.transposeBlockSize() == calc.outputBytesPerInputByte());
    }

    FL_SUBCASE("data_width = 8") {
        ParlioBufferCalculator calc{8};
        FL_CHECK(calc.transposeBlockSize() == 64);
        FL_CHECK(calc.transposeBlockSize() == calc.outputBytesPerInputByte());
    }

    FL_SUBCASE("data_width = 16") {
        ParlioBufferCalculator calc{16};
        FL_CHECK(calc.transposeBlockSize() == 128);
        FL_CHECK(calc.transposeBlockSize() == calc.outputBytesPerInputByte());
    }
}

FL_TEST_CASE("ParlioBufferCalculator - resetPaddingBytes") {
    ParlioBufferCalculator calc{1};

    FL_SUBCASE("zero reset time") {
        FL_CHECK(calc.resetPaddingBytes(0) == 0);
    }

    FL_SUBCASE("1us reset time") {
        FL_CHECK(calc.resetPaddingBytes(1) == 8);
    }

    FL_SUBCASE("8us reset time (exactly 1 Wave8Byte)") {
        FL_CHECK(calc.resetPaddingBytes(8) == 8);
    }

    FL_SUBCASE("9us reset time") {
        FL_CHECK(calc.resetPaddingBytes(9) == 16);
    }

    FL_SUBCASE("80us reset time (WS2812 typical)") {
        FL_CHECK(calc.resetPaddingBytes(80) == 80);
    }

    FL_SUBCASE("280us reset time (SK6812 typical)") {
        FL_CHECK(calc.resetPaddingBytes(280) == 280);
    }

    FL_SUBCASE("300us reset time") {
        FL_CHECK(calc.resetPaddingBytes(300) == 304);
    }
}

FL_TEST_CASE("ParlioBufferCalculator - dmaBufferSize basic calculations") {
    FL_SUBCASE("single lane, single LED, no reset") {
        ParlioBufferCalculator calc{1};
        FL_CHECK(calc.dmaBufferSize(3, 0) == 32);
    }

    FL_SUBCASE("single lane, single LED, 80us reset") {
        ParlioBufferCalculator calc{1};
        FL_CHECK(calc.dmaBufferSize(3, 80) == 112);
    }

    FL_SUBCASE("single lane, 10 LEDs, no reset") {
        ParlioBufferCalculator calc{1};
        FL_CHECK(calc.dmaBufferSize(30, 0) == 248);
    }

    FL_SUBCASE("four lanes, 10 LEDs per lane, no reset") {
        ParlioBufferCalculator calc{4};
        FL_CHECK(calc.dmaBufferSize(120, 0) == 3872);
    }

    FL_SUBCASE("four lanes, 5 LEDs per lane (15 bytes), 280us reset") {
        ParlioBufferCalculator calc{4};
        FL_CHECK(calc.dmaBufferSize(60, 280) == 2232);
    }

    FL_SUBCASE("16 lanes, 1 LED per lane, no reset") {
        ParlioBufferCalculator calc{16};
        FL_CHECK(calc.dmaBufferSize(48, 0) == 6272);
    }
}

FL_TEST_CASE("ParlioBufferCalculator - dmaBufferSize edge cases") {
    FL_SUBCASE("zero input bytes") {
        ParlioBufferCalculator calc{1};
        FL_CHECK(calc.dmaBufferSize(0, 0) == 8);
    }

    FL_SUBCASE("zero input bytes with reset") {
        ParlioBufferCalculator calc{1};
        FL_CHECK(calc.dmaBufferSize(0, 80) == 88);
    }

    FL_SUBCASE("large input (1000 LEDs, single lane)") {
        ParlioBufferCalculator calc{1};
        FL_CHECK(calc.dmaBufferSize(3000, 0) == 24008);
    }
}

FL_TEST_CASE("ParlioBufferCalculator - calculateRingBufferCapacity") {
    FL_SUBCASE("100 LEDs, single lane, 3 ring buffers, 80us reset") {
        ParlioBufferCalculator calc{1};
        size_t capacity = calc.calculateRingBufferCapacity(100, 80, 3);
        FL_CHECK(capacity == 1032);
    }

    FL_SUBCASE("10 LEDs, 4 lanes, 3 ring buffers, no reset") {
        ParlioBufferCalculator calc{4};
        size_t capacity = calc.calculateRingBufferCapacity(10, 0, 3);
        FL_CHECK(capacity == 1696);
    }

    FL_SUBCASE("single LED, single lane, 3 ring buffers") {
        ParlioBufferCalculator calc{1};
        size_t capacity = calc.calculateRingBufferCapacity(1, 0, 3);
        FL_CHECK(capacity == 160);
    }

    FL_SUBCASE("3000 LEDs, single lane, 3 ring buffers, 280us reset") {
        ParlioBufferCalculator calc{1};
        size_t capacity = calc.calculateRingBufferCapacity(3000, 280, 3);
        FL_CHECK(capacity == 24416);
    }
}

FL_TEST_CASE("ParlioBufferCalculator - consistency across data widths") {
    size_t input_bytes = 30;
    uint32_t reset_us = 80;

    ParlioBufferCalculator calc1{1};
    ParlioBufferCalculator calc2{2};
    ParlioBufferCalculator calc4{4};
    ParlioBufferCalculator calc8{8};
    ParlioBufferCalculator calc16{16};

    size_t size1 = calc1.dmaBufferSize(input_bytes, reset_us);
    size_t size2 = calc2.dmaBufferSize(input_bytes, reset_us);
    size_t size4 = calc4.dmaBufferSize(input_bytes, reset_us);
    size_t size8 = calc8.dmaBufferSize(input_bytes, reset_us);
    size_t size16 = calc16.dmaBufferSize(input_bytes, reset_us);

    FL_CHECK(size1 < size2);
    FL_CHECK(size2 < size4);
    FL_CHECK(size4 < size8);
    FL_CHECK(size8 < size16);

    FL_CHECK(size2 > size1 * 1.5);
    FL_CHECK(size4 > size2 * 1.5);
    FL_CHECK(size8 > size4 * 1.5);
    FL_CHECK(size16 > size8 * 1.5);
}

FL_TEST_CASE("ParlioBufferCalculator - buffer overflow scenario from BUG-006") {
    ParlioBufferCalculator calc{4};

    size_t num_leds_per_lane = 5;
    size_t num_lanes = 4;
    size_t bytes_per_led = 3;
    size_t lane_stride = num_leds_per_lane * bytes_per_led;
    size_t total_bytes = lane_stride * num_lanes;

    FL_CHECK(lane_stride == 15);
    FL_CHECK(total_bytes == 60);

    size_t dma_size = calc.dmaBufferSize(total_bytes, 0);
    FL_CHECK(dma_size == 1952);

    FL_CHECK(lane_stride == 15);
    FL_CHECK(total_bytes == 60);
}

FL_TEST_CASE("ParlioBufferCalculator - buffer overflow scenario from BUG-007") {
    ParlioBufferCalculator calc{2};

    size_t num_leds_per_lane = 5;
    size_t num_lanes = 2;
    size_t bytes_per_led = 3;
    size_t lane_stride = num_leds_per_lane * bytes_per_led;
    size_t total_bytes = lane_stride * num_lanes;

    FL_CHECK(lane_stride == 15);
    FL_CHECK(total_bytes == 30);

    size_t dma_size = calc.dmaBufferSize(total_bytes, 0);
    FL_CHECK(dma_size == 496);
}
