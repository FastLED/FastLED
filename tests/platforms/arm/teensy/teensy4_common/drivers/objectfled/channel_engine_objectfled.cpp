/// @file objectfled_channel_engine.cpp
/// @brief ChannelEngineObjectFLED integration tests with mock peripheral

#ifdef FASTLED_STUB_IMPL

#include "platforms/shared/mock/arm/teensy4/drivers/objectfled/objectfled_peripheral_mock.h"
#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/channel_engine_objectfled.h"
#include "platforms/arm/teensy/teensy4_common/clockless_objectfled.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/gfx/rectangular_draw_buffer.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;
using DriverState = IChannelDriver::DriverState;

namespace {

ChipsetTimingConfig createWS2812Timing() {
    return ChipsetTimingConfig(350, 450, 450, 50, "WS2812B");
}

ChipsetTimingConfig createSK6812Timing() {
    return ChipsetTimingConfig(300, 600, 600, 80, "SK6812");
}

ChipsetTimingConfig createTooFastTiming() {
    return ChipsetTimingConfig(100, 100, 100, 50, "TooFast");
}

ChipsetTimingConfig createTooSlowTiming() {
    return ChipsetTimingConfig(1000, 1000, 1000, 50, "TooSlow");
}

ChannelDataPtr createRGBChannelData(int pin, size_t num_leds,
                                     const uint8_t* rgb_data = nullptr,
                                     const ChipsetTimingConfig* timing = nullptr) {
    ChipsetTimingConfig t = timing ? *timing : createWS2812Timing();
    fl::vector_psram<uint8_t> encoded(num_leds * 3);
    if (rgb_data) {
        for (size_t i = 0; i < num_leds * 3; i++) {
            encoded[i] = rgb_data[i];
        }
    }
    return ChannelData::create(pin, t, fl::move(encoded));
}

ChannelDataPtr createRGBWChannelData(int pin, size_t num_leds,
                                      const uint8_t* rgbw_data = nullptr) {
    ChipsetTimingConfig t = createWS2812Timing();
    fl::vector_psram<uint8_t> encoded(num_leds * 4);
    if (rgbw_data) {
        for (size_t i = 0; i < num_leds * 4; i++) {
            encoded[i] = rgbw_data[i];
        }
    }
    return ChannelData::create(
        pin, t, fl::move(encoded), ChannelPixelFormat::RGBW);
}

} // anonymous namespace

//=============================================================================
// canHandle() Filtering
//=============================================================================

FL_TEST_CASE("ObjectFLED engine - canHandle accepts standard timing") {
    auto mock = fl::make_shared<ObjectFLEDPeripheralMock>();
    ChannelEngineObjectFLED engine(mock);
    auto ch = createRGBChannelData(2, 1);
    FL_CHECK(engine.canHandle(ch) == true);
}

FL_TEST_CASE("ObjectFLED engine - canHandle rejects too-fast timing") {
    auto mock = fl::make_shared<ObjectFLEDPeripheralMock>();
    ChannelEngineObjectFLED engine(mock);
    ChipsetTimingConfig timing = createTooFastTiming();
    auto ch = createRGBChannelData(2, 1, nullptr, &timing);
    FL_CHECK(engine.canHandle(ch) == false);
}

FL_TEST_CASE("ObjectFLED engine - canHandle rejects too-slow timing") {
    auto mock = fl::make_shared<ObjectFLEDPeripheralMock>();
    ChannelEngineObjectFLED engine(mock);
    ChipsetTimingConfig timing = createTooSlowTiming();
    auto ch = createRGBChannelData(2, 1, nullptr, &timing);
    FL_CHECK(engine.canHandle(ch) == false);
}

FL_TEST_CASE("ObjectFLED engine - canHandle rejects null data") {
    auto mock = fl::make_shared<ObjectFLEDPeripheralMock>();
    ChannelEngineObjectFLED engine(mock);
    FL_CHECK(engine.canHandle(nullptr) == false);
}

//=============================================================================
// Pin Validation
//=============================================================================

FL_TEST_CASE("ObjectFLED engine - valid pin is accepted") {
    auto mock = fl::make_shared<ObjectFLEDPeripheralMock>();
    ChannelEngineObjectFLED engine(mock);
    uint8_t red[] = {0xFF, 0x00, 0x00};
    auto ch = createRGBChannelData(2, 1, red);
    engine.enqueue(ch);
    engine.show();
    FL_CHECK(mock->getCreateCount() == 1);
}

FL_TEST_CASE("ObjectFLED engine - invalid pin is skipped") {
    auto mock = fl::make_shared<ObjectFLEDPeripheralMock>();
    mock->setInvalidPin(5, "Pin 5 is invalid (test)");
    ChannelEngineObjectFLED engine(mock);
    uint8_t red[] = {0xFF, 0x00, 0x00};
    auto ch = createRGBChannelData(5, 1, red);
    engine.enqueue(ch);
    engine.show();
    FL_CHECK(mock->getCreateCount() == 0);
}

//=============================================================================
// Basic Transmission
//=============================================================================

FL_TEST_CASE("ObjectFLED engine - initial state is READY") {
    auto mock = fl::make_shared<ObjectFLEDPeripheralMock>();
    ChannelEngineObjectFLED engine(mock);
    FL_CHECK(engine.poll() == DriverState::READY);
}

FL_TEST_CASE("ObjectFLED engine - single channel RGB transmission") {
    auto mock = fl::make_shared<ObjectFLEDPeripheralMock>();
    ChannelEngineObjectFLED engine(mock);

    uint8_t pixels[] = {0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF};
    auto ch = createRGBChannelData(2, 3, pixels);

    engine.enqueue(ch);
    engine.show();

    FL_CHECK(engine.poll() == DriverState::READY);
    FL_CHECK(mock->getCreateCount() == 1);

    auto* inst = mock->getLastInstance();
    FL_REQUIRE(inst != nullptr);
    FL_CHECK(inst->getShowCount() == 1);
    FL_CHECK(inst->getFrameBufferSize() == 9);

    // Verify pixel data was copied into frame buffer
    const auto& frameData = inst->getRawBuffer();
    FL_REQUIRE(frameData.size() == 9);
    FL_CHECK(frameData[0] == 0xFF);
    FL_CHECK(frameData[1] == 0x00);
    FL_CHECK(frameData[2] == 0x00);
    FL_CHECK(frameData[3] == 0x00);
    FL_CHECK(frameData[4] == 0xFF);
    FL_CHECK(frameData[5] == 0x00);

    FL_CHECK(ch->isInUse() == false);
}

FL_TEST_CASE("ObjectFLED engine - multi-channel same timing = 1 instance") {
    auto mock = fl::make_shared<ObjectFLEDPeripheralMock>();
    ChannelEngineObjectFLED engine(mock);

    uint8_t red[] = {0xFF, 0x00, 0x00};
    uint8_t green[] = {0x00, 0xFF, 0x00};
    auto ch1 = createRGBChannelData(2, 1, red);
    auto ch2 = createRGBChannelData(3, 1, green);

    engine.enqueue(ch1);
    engine.enqueue(ch2);
    engine.show();

    FL_CHECK(mock->getCreateCount() == 1);

    auto* record = mock->getLastCreateRecord();
    FL_REQUIRE(record != nullptr);
    FL_CHECK(record->numPins == 2);
    FL_CHECK(record->pinList[0] == 2);
    FL_CHECK(record->pinList[1] == 3);
}

FL_TEST_CASE("ObjectFLED engine - different timing = 2 instances") {
    auto mock = fl::make_shared<ObjectFLEDPeripheralMock>();
    ChannelEngineObjectFLED engine(mock);

    ChipsetTimingConfig ws2812 = createWS2812Timing();
    ChipsetTimingConfig sk6812 = createSK6812Timing();
    auto ch1 = createRGBChannelData(2, 1, nullptr, &ws2812);
    auto ch2 = createRGBChannelData(3, 1, nullptr, &sk6812);

    engine.enqueue(ch1);
    engine.enqueue(ch2);
    engine.show();
    while (engine.poll() != DriverState::READY) {
    }

    FL_CHECK(mock->getCreateCount() == 2);
}

FL_TEST_CASE("ObjectFLED engine - poll drains active DMA/latch state") {
    auto mock = fl::make_shared<ObjectFLEDPeripheralMock>();
    mock->setBusyAfterShow(true);
    ChannelEngineObjectFLED engine(mock);

    uint8_t red[] = {0xFF, 0x00, 0x00};
    auto ch = createRGBChannelData(2, 1, red);

    engine.enqueue(ch);
    engine.show();

    auto* inst = mock->getLastInstance();
    FL_REQUIRE(inst != nullptr);
    FL_CHECK(ch->isInUse() == true);
    FL_CHECK(engine.poll() == DriverState::DRAINING);

    inst->complete();
    FL_CHECK(engine.poll() == DriverState::READY);
    FL_CHECK(ch->isInUse() == false);
}

FL_TEST_CASE("ObjectFLED engine - timing groups sequence after busy completion") {
    auto mock = fl::make_shared<ObjectFLEDPeripheralMock>();
    mock->setBusyAfterShow(true);
    ChannelEngineObjectFLED engine(mock);

    ChipsetTimingConfig ws2812 = createWS2812Timing();
    ChipsetTimingConfig sk6812 = createSK6812Timing();
    auto ch1 = createRGBChannelData(2, 1, nullptr, &ws2812);
    auto ch2 = createRGBChannelData(3, 1, nullptr, &sk6812);

    engine.enqueue(ch1);
    engine.enqueue(ch2);
    engine.show();

    FL_CHECK(mock->getCreateCount() == 1);
    FL_CHECK(ch1->isInUse() == true);
    FL_CHECK(ch2->isInUse() == true);
    FL_CHECK(engine.poll() == DriverState::DRAINING);

    auto* first = mock->getInstance(0);
    FL_REQUIRE(first != nullptr);
    first->complete();

    FL_CHECK(engine.poll() == DriverState::BUSY);
    FL_CHECK(mock->getCreateCount() == 2);
    FL_CHECK(engine.poll() == DriverState::DRAINING);

    auto* second = mock->getInstance(1);
    FL_REQUIRE(second != nullptr);
    second->complete();

    FL_CHECK(engine.poll() == DriverState::READY);
    FL_CHECK(ch1->isInUse() == false);
    FL_CHECK(ch2->isInUse() == false);
}

FL_TEST_CASE("ObjectFLED engine - empty enqueue does nothing") {
    auto mock = fl::make_shared<ObjectFLEDPeripheralMock>();
    ChannelEngineObjectFLED engine(mock);
    engine.show();
    FL_CHECK(mock->getCreateCount() == 0);
    FL_CHECK(engine.poll() == DriverState::READY);
}

//=============================================================================
// Frame Caching
//=============================================================================

FL_TEST_CASE("ObjectFLED engine - reuses instance on unchanged draw list") {
    auto mock = fl::make_shared<ObjectFLEDPeripheralMock>();
    ChannelEngineObjectFLED engine(mock);

    uint8_t red[] = {0xFF, 0x00, 0x00};
    auto ch1 = createRGBChannelData(2, 1, red);
    engine.enqueue(ch1);
    engine.show();
    FL_CHECK(mock->getCreateCount() == 1);

    auto ch2 = createRGBChannelData(2, 1, red);
    engine.enqueue(ch2);
    engine.show();
    FL_CHECK(mock->getCreateCount() == 1);

    auto* inst = mock->getLastInstance();
    FL_REQUIRE(inst != nullptr);
    FL_CHECK(inst->getShowCount() == 2);
}

//=============================================================================
// RGBW Detection
//=============================================================================

FL_TEST_CASE("ObjectFLED engine - detects RGBW data") {
    auto mock = fl::make_shared<ObjectFLEDPeripheralMock>();
    ChannelEngineObjectFLED engine(mock);

    uint8_t rgbw[] = {0xFF, 0x00, 0x00, 0x80};
    auto ch = createRGBWChannelData(2, 1, rgbw);

    engine.enqueue(ch);
    engine.show();

    FL_CHECK(mock->getCreateCount() == 1);
    auto* record = mock->getLastCreateRecord();
    FL_REQUIRE(record != nullptr);
    FL_CHECK(record->isRgbw == true);
}

FL_TEST_CASE("ObjectFLED engine - explicit RGB metadata wins over 12-byte size") {
    auto mock = fl::make_shared<ObjectFLEDPeripheralMock>();
    ChannelEngineObjectFLED engine(mock);

    uint8_t rgb[] = {
        0x10, 0x11, 0x12,
        0x20, 0x21, 0x22,
        0x30, 0x31, 0x32,
        0x40, 0x41, 0x42,
    };
    auto ch = createRGBChannelData(2, 4, rgb);

    engine.enqueue(ch);
    engine.show();

    auto* record = mock->getLastCreateRecord();
    auto* inst = mock->getLastInstance();
    FL_REQUIRE(record != nullptr);
    FL_REQUIRE(inst != nullptr);
    FL_CHECK(record->isRgbw == false);
    FL_CHECK(record->totalLeds == 4);
    FL_CHECK(inst->getFrameBufferSize() == 12);
}

FL_TEST_CASE("ObjectFLED rectangular sizing rounds RGBW small counts") {
    struct Case {
        u16 leds;
        u32 rectangularBytes;
        u32 frameBytes;
        u32 totalLeds;
    };
    const Case cases[] = {
        {1, 9, 12, 3},
        {2, 9, 12, 3},
        {3, 15, 16, 4},
        {4, 18, 20, 5},
    };

    for (const auto& c : cases) {
        RectangularDrawBuffer drawBuf;
        drawBuf.queue(DrawItem(22, c.leds, true));
        drawBuf.onQueuingDone();

        u32 numStrips = 0;
        u32 bytesPerStrip = 0;
        u32 totalBytes = 0;
        drawBuf.getBlockInfo(&numStrips, &bytesPerStrip, &totalBytes);

        FL_CHECK(numStrips == 1);
        FL_CHECK(bytesPerStrip == c.rectangularBytes);
        FL_CHECK(totalBytes == c.rectangularBytes);
        FL_CHECK(objectFledFrameBytesForRectangularBlock(
                     numStrips, bytesPerStrip, true) == c.frameBytes);
        FL_CHECK(objectFledTotalLedsForRectangularBlock(
                     numStrips, bytesPerStrip, true) == c.totalLeds);
        FL_CHECK(c.frameBytes >= totalBytes);
    }
}

FL_TEST_CASE("ObjectFLED engine - RGBW raw channel counts use exact byte layout") {
    struct Case {
        size_t leds;
        u32 expectedFrameBytes;
    };
    const Case cases[] = {
        {1, 4},
        {2, 8},
        {3, 12},
        {4, 16},
        {6, 24},
        {9, 36},
    };

    for (const auto& c : cases) {
        auto mock = fl::make_shared<ObjectFLEDPeripheralMock>();
        ChannelEngineObjectFLED engine(mock);
        auto ch = createRGBWChannelData(22, c.leds);

        engine.enqueue(ch);
        engine.show();

        auto* inst = mock->getLastInstance();
        auto* record = mock->getLastCreateRecord();
        FL_REQUIRE(inst != nullptr);
        FL_REQUIRE(record != nullptr);
        FL_CHECK(record->isRgbw == true);
        FL_CHECK(record->totalLeds == static_cast<int>(c.leds));
        FL_CHECK(inst->getFrameBufferSize() == c.expectedFrameBytes);
    }
}

FL_TEST_CASE("ObjectFLED engine - RGBW raw channel preserves nonzero white") {
    auto mock = fl::make_shared<ObjectFLEDPeripheralMock>();
    ChannelEngineObjectFLED engine(mock);

    uint8_t rgbw[] = {
        0x01, 0x02, 0x03, 0xA5,
        0x04, 0x05, 0x06, 0xB6,
        0x07, 0x08, 0x09, 0xC7,
    };
    auto ch = createRGBWChannelData(22, 3, rgbw);

    engine.enqueue(ch);
    engine.show();

    auto* record = mock->getLastCreateRecord();
    auto* inst = mock->getLastInstance();
    FL_REQUIRE(record != nullptr);
    FL_REQUIRE(inst != nullptr);
    FL_CHECK(record->isRgbw == true);
    FL_CHECK(record->totalLeds == 3);
    FL_CHECK(inst->getFrameBufferSize() == 12);

    const auto& frameData = inst->getRawBuffer();
    FL_REQUIRE(frameData.size() == 12);
    FL_CHECK(frameData[3] == 0xA5);
    FL_CHECK(frameData[7] == 0xB6);
    FL_CHECK(frameData[11] == 0xC7);
}

//=============================================================================
// Error Handling
//=============================================================================

FL_TEST_CASE("ObjectFLED engine - handles createInstance failure") {
    auto mock = fl::make_shared<ObjectFLEDPeripheralMock>();
    mock->setCreateFailure(true);
    ChannelEngineObjectFLED engine(mock);

    uint8_t red[] = {0xFF, 0x00, 0x00};
    auto ch = createRGBChannelData(2, 1, red);

    engine.enqueue(ch);
    engine.show();

    FL_CHECK(mock->getCreateCount() == 1);
    FL_CHECK(engine.poll() == DriverState::READY);
}

//=============================================================================
// Engine Properties
//=============================================================================

FL_TEST_CASE("ObjectFLED engine - name is OBJECT_FLED") {
    auto mock = fl::make_shared<ObjectFLEDPeripheralMock>();
    ChannelEngineObjectFLED engine(mock);
    FL_CHECK(engine.getName() == "OBJECT_FLED");
}

FL_TEST_CASE("ObjectFLED engine - capabilities are unified clockless + SPI (#3428)") {
    auto mock = fl::make_shared<ObjectFLEDPeripheralMock>();
    ChannelEngineObjectFLED engine(mock);
    auto caps = engine.getCapabilities();
    // Per the unified parallel-IO driver rule: one ChannelEngine, both modes.
    // SPI runtime serving is gated by `objectfled_spi_lookup_pins()` (stubbed
    // until the DMA-bit-banged SPI hardware impl lands), but the capability
    // slot is permanent. See src/fl/channels/README.md -> "Rule: Parallel-IO
    // peripherals -- one engine for both clockless and SPI modes".
    FL_CHECK(caps.supportsClockless == true);
    FL_CHECK(caps.supportsSpi == true);
}

#endif // FASTLED_STUB_IMPL

} // FL_TEST_FILE
