/// @file flexio_channel_engine.cpp
/// @brief ChannelEngineFlexIO integration tests with mock peripheral
///
/// Tests the FlexIO channel engine business logic using the mock peripheral:
/// - canHandle() pin and timing filtering
/// - Single channel transmission
/// - State machine progression (READY → BUSY → READY)
/// - Pin change re-initialization
///
/// These tests run ONLY on stub platforms (host-based testing).

#ifdef FASTLED_STUB_IMPL  // Mock tests only run on stub platform

#include "platforms/shared/mock/arm/teensy4/drivers/flexio/flexio_peripheral_mock.h"
#include "platforms/arm/teensy/teensy4_common/drivers/flexio/channel_engine_flexio.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "test.h"

using namespace fl;

using DriverState = IChannelDriver::DriverState;

namespace {

/// @brief Create WS2812B timing configuration
ChipsetTimingConfig createWS2812Timing() {
    return ChipsetTimingConfig(
        350,   // t1_ns: T0H
        450,   // t2_ns: T1H - T0H
        450,   // t3_ns: T0L
        50,    // reset_us
        "WS2812B"
    );
}

/// @brief Create channel data for testing
ChannelDataPtr createChannelData(int pin, size_t num_leds,
                                  const uint8_t* rgb_data = nullptr) {
    ChipsetTimingConfig timing = createWS2812Timing();
    fl::vector_psram<uint8_t> encoded(num_leds * 3);
    if (rgb_data) {
        for (size_t i = 0; i < num_leds * 3; i++) {
            encoded[i] = rgb_data[i];
        }
    }
    return ChannelData::create(pin, timing, fl::move(encoded));
}

} // anonymous namespace

//=============================================================================
// Test Suite: canHandle() Filtering
//=============================================================================

FL_TEST_CASE("FlexIO engine - canHandle accepts FlexIO2 pins") {
    auto mock = fl::make_shared<FlexIOPeripheralMock>();
    ChannelEngineFlexIO engine(mock);

    // FlexIO2-capable pins: 6, 7, 8, 9, 10, 11, 12, 13, 32
    auto ch6 = createChannelData(6, 1);
    auto ch10 = createChannelData(10, 1);
    auto ch32 = createChannelData(32, 1);

    FL_CHECK(engine.canHandle(ch6) == true);
    FL_CHECK(engine.canHandle(ch10) == true);
    FL_CHECK(engine.canHandle(ch32) == true);
}

FL_TEST_CASE("FlexIO engine - canHandle rejects non-FlexIO pins") {
    auto mock = fl::make_shared<FlexIOPeripheralMock>();
    ChannelEngineFlexIO engine(mock);

    // Non-FlexIO pins should be rejected (fall through to ObjectFLED)
    auto ch0 = createChannelData(0, 1);
    auto ch1 = createChannelData(1, 1);
    auto ch14 = createChannelData(14, 1);

    FL_CHECK(engine.canHandle(ch0) == false);
    FL_CHECK(engine.canHandle(ch1) == false);
    FL_CHECK(engine.canHandle(ch14) == false);
}

FL_TEST_CASE("FlexIO engine - canHandle rejects null data") {
    auto mock = fl::make_shared<FlexIOPeripheralMock>();
    ChannelEngineFlexIO engine(mock);

    FL_CHECK(engine.canHandle(nullptr) == false);
}

//=============================================================================
// Test Suite: Basic Transmission
//=============================================================================

FL_TEST_CASE("FlexIO engine - initial state is READY") {
    auto mock = fl::make_shared<FlexIOPeripheralMock>();
    ChannelEngineFlexIO engine(mock);

    FL_CHECK(engine.poll() == DriverState::READY);
}

FL_TEST_CASE("FlexIO engine - single channel transmission") {
    auto mock = fl::make_shared<FlexIOPeripheralMock>();
    ChannelEngineFlexIO engine(mock);

    uint8_t red[] = {0xFF, 0x00, 0x00};
    auto ch = createChannelData(10, 1, red);

    engine.enqueue(ch);
    engine.show();

    // Mock completes instantly, so should be READY after show()
    FL_CHECK(engine.poll() == DriverState::READY);

    // Verify transmission occurred
    FL_CHECK(mock->getTransmissionCount() == 1);

    // Verify transmitted data
    auto* record = mock->getLastTransmission();
    FL_REQUIRE(record != nullptr);
    FL_CHECK(record->pin == 10);
    FL_CHECK(record->data.size() == 3);
    FL_CHECK(record->data[0] == 0xFF);
    FL_CHECK(record->data[1] == 0x00);
    FL_CHECK(record->data[2] == 0x00);

    // Channel should no longer be in use
    FL_CHECK(ch->isInUse() == false);
}

FL_TEST_CASE("FlexIO engine - multiple LED transmission") {
    auto mock = fl::make_shared<FlexIOPeripheralMock>();
    ChannelEngineFlexIO engine(mock);

    uint8_t pixels[] = {
        0xFF, 0x00, 0x00,  // Red
        0x00, 0xFF, 0x00,  // Green
        0x00, 0x00, 0xFF   // Blue
    };
    auto ch = createChannelData(6, 3, pixels);

    engine.enqueue(ch);
    engine.show();

    FL_CHECK(mock->getTransmissionCount() == 1);
    auto* record = mock->getLastTransmission();
    FL_REQUIRE(record != nullptr);
    FL_CHECK(record->data.size() == 9);
    FL_CHECK(record->pin == 6);
}

FL_TEST_CASE("FlexIO engine - empty enqueue does nothing") {
    auto mock = fl::make_shared<FlexIOPeripheralMock>();
    ChannelEngineFlexIO engine(mock);

    // show() with no enqueued channels should be a no-op
    engine.show();

    FL_CHECK(mock->getTransmissionCount() == 0);
    FL_CHECK(engine.poll() == DriverState::READY);
}

//=============================================================================
// Test Suite: Engine Properties
//=============================================================================

FL_TEST_CASE("FlexIO engine - name is FLEXIO") {
    auto mock = fl::make_shared<FlexIOPeripheralMock>();
    ChannelEngineFlexIO engine(mock);

    FL_CHECK(engine.getName() == "FLEXIO");
}

FL_TEST_CASE("FlexIO engine - capabilities are clockless only") {
    auto mock = fl::make_shared<FlexIOPeripheralMock>();
    ChannelEngineFlexIO engine(mock);

    auto caps = engine.getCapabilities();
    FL_CHECK(caps.supportsClockless == true);
    FL_CHECK(caps.supportsSpi == false);
}

//=============================================================================
// Test Suite: Pin Re-initialization
//=============================================================================

FL_TEST_CASE("FlexIO engine - reinitializes on pin change") {
    auto mock = fl::make_shared<FlexIOPeripheralMock>();
    ChannelEngineFlexIO engine(mock);

    // First transmission on pin 10
    auto ch1 = createChannelData(10, 1);
    engine.enqueue(ch1);
    engine.show();
    FL_CHECK(mock->getCurrentPin() == 10);

    // Second transmission on pin 6 (different pin → should reinit)
    auto ch2 = createChannelData(6, 1);
    engine.enqueue(ch2);
    engine.show();
    FL_CHECK(mock->getCurrentPin() == 6);

    FL_CHECK(mock->getTransmissionCount() == 2);
}

//=============================================================================
// Test Suite: Error Handling
//=============================================================================

FL_TEST_CASE("FlexIO engine - handles init failure gracefully") {
    auto mock = fl::make_shared<FlexIOPeripheralMock>();
    ChannelEngineFlexIO engine(mock);

    mock->setInitFailure(true);

    auto ch = createChannelData(10, 1);
    engine.enqueue(ch);
    engine.show();

    // Should not crash, but no transmission should occur
    FL_CHECK(mock->getTransmissionCount() == 0);
    FL_CHECK(engine.poll() == DriverState::READY);
}

#endif // FASTLED_STUB_IMPL
