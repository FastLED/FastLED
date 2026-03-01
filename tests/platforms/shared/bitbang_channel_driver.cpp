/// @file tests/platforms/shared/bitbang_channel_driver.cpp
/// @brief Unit tests for BitBangChannelDriver
///
/// Tests cover:
/// - Driver interface (poll, getName, capabilities, canHandle)
/// - Clockless transmission (enqueue, show, isInUse lifecycle)
/// - SPI transmission
/// - Mixed clockless + SPI in one frame
/// - Pin limit (>8 unique pins warning)
/// - Exposed DigitalMultiWrite8 via getMultiWriter()

#include "test.h"
#include "platforms/shared/bitbang/bitbang_channel_driver.h"
#include "fl/channels/data.h"
#include "fl/channels/config.h"
#include "fl/pin.h"

using namespace fl;

namespace {

// Helper: create clockless ChannelData with given pin and byte data
ChannelDataPtr makeClockless(int pin, const u8* bytes, size_t len) {
    ChipsetTimingConfig timing(250, 625, 375, 280);  // WS2812-like
    fl::vector_psram<u8> data(bytes, bytes + len);
    return ChannelData::create(pin, timing, fl::move(data));
}

// Helper: create SPI ChannelData with given data/clock pins and byte data
ChannelDataPtr makeSpi(int dataPin, int clockPin, const u8* bytes,
                       size_t len) {
    SpiChipsetConfig spiCfg(dataPin, clockPin, SpiEncoder::apa102());
    fl::vector_psram<u8> data(bytes, bytes + len);
    ChipsetVariant chipset = spiCfg;
    return ChannelData::create(chipset, fl::move(data));
}

} // namespace

FL_TEST_SUITE("BitBangChannelDriver") {

FL_TEST_CASE("poll always READY") {
    BitBangChannelDriver driver;
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

FL_TEST_CASE("getName returns BITBANG") {
    BitBangChannelDriver driver;
    FL_CHECK(driver.getName() == fl::string::from_literal("BITBANG"));
}

FL_TEST_CASE("capabilities: clockless and SPI") {
    BitBangChannelDriver driver;
    auto caps = driver.getCapabilities();
    FL_CHECK(caps.supportsClockless);
    FL_CHECK(caps.supportsSpi);
}

FL_TEST_CASE("canHandle always true") {
    BitBangChannelDriver driver;
    u8 bytes[] = {0xFF, 0x00, 0xAA};
    auto ch = makeClockless(5, bytes, 3);
    FL_CHECK(driver.canHandle(ch));
}

FL_TEST_CASE("clockless enqueue and show") {
    BitBangChannelDriver driver;
    u8 bytes[] = {0xFF, 0x00, 0xAA};
    auto ch = makeClockless(5, bytes, 3);

    driver.enqueue(ch);
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);

    driver.show();

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    FL_CHECK_FALSE(ch->isInUse());
}

FL_TEST_CASE("SPI enqueue and show") {
    BitBangChannelDriver driver;
    u8 bytes[] = {0xE0, 0xFF, 0x00, 0x80};
    auto ch = makeSpi(10, 11, bytes, 4);

    driver.enqueue(ch);
    driver.show();

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    FL_CHECK_FALSE(ch->isInUse());
}

FL_TEST_CASE("mixed clockless and SPI") {
    BitBangChannelDriver driver;

    u8 clocklessBytes[] = {0xAA, 0x55};
    auto ch1 = makeClockless(2, clocklessBytes, 2);

    u8 spiBytes[] = {0xE0, 0xFF, 0x00, 0x80};
    auto ch2 = makeSpi(3, 4, spiBytes, 4);

    driver.enqueue(ch1);
    driver.enqueue(ch2);
    driver.show();

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
    FL_CHECK_FALSE(ch1->isInUse());
    FL_CHECK_FALSE(ch2->isInUse());
}

FL_TEST_CASE("multiple clockless channels in parallel") {
    BitBangChannelDriver driver;

    u8 bytes1[] = {0xFF, 0x00, 0xFF};
    u8 bytes2[] = {0x00, 0xFF};
    auto ch1 = makeClockless(5, bytes1, 3);
    auto ch2 = makeClockless(6, bytes2, 2);

    driver.enqueue(ch1);
    driver.enqueue(ch2);
    driver.show();

    FL_CHECK_FALSE(ch1->isInUse());
    FL_CHECK_FALSE(ch2->isInUse());
}

FL_TEST_CASE("empty show is no-op") {
    BitBangChannelDriver driver;
    driver.show();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

FL_TEST_CASE("more than 8 pins warns and skips") {
    BitBangChannelDriver driver;

    u8 byte = 0xFF;
    for (int i = 0; i < 9; ++i) {
        driver.enqueue(makeClockless(i, &byte, 1));
    }

    driver.show();
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

FL_TEST_CASE("consecutive shows") {
    BitBangChannelDriver driver;

    u8 bytes1[] = {0xAA};
    u8 bytes2[] = {0x55};

    driver.enqueue(makeClockless(5, bytes1, 1));
    driver.show();

    driver.enqueue(makeClockless(5, bytes2, 1));
    driver.show();

    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

FL_TEST_CASE("getMultiWriter exposes initialized writer") {
    BitBangChannelDriver driver;
    u8 bytes[] = {0xFF};
    driver.enqueue(makeClockless(5, bytes, 1));
    driver.show();

    const DigitalMultiWrite8& writer = driver.getMultiWriter();
    writer.writeByte(0xFF);
    FL_CHECK_EQ(fl::digitalRead(5), fl::PinValue::High);
    writer.writeByte(0x00);
    FL_CHECK_EQ(fl::digitalRead(5), fl::PinValue::Low);
}

} // FL_TEST_SUITE
