// ok standalone
/// @file tests/platforms/shared/bitbang/bitbang_channel_driver.cpp
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
#include "fl/system/pin.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"
#include "platforms/stub/stub_gpio.h"
#include "platforms/stub/time_stub.h"

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

// Records each data pin's level at every rising edge of a clock pin, through the
// driver's real enqueue/show path. Also counts falling edges.
struct SpiTrace {
    int clockPin;
    fl::vector<int> dataPins;
    fl::vector<fl::vector<u8>> sampled;  // Per data pin, one level per rising edge.
    int falling = 0;

    SpiTrace(int clock, fl::vector<int> pins) : clockPin(clock), dataPins(pins) {
        sampled.resize(dataPins.size());
        fl::stub::setPinEdgeCallback(clockPin, [this](bool high, u32) FL_NO_EXCEPT {
            if (!high) {
                ++falling;
                return;
            }
            for (fl::size i = 0; i < dataPins.size(); ++i) {
                const bool level = fl::digitalRead(dataPins[i]) == fl::PinValue::High;
                sampled[i].push_back(level ? 1 : 0);
            }
        });
    }
    ~SpiTrace() { fl::stub::clearPinEdgeCallback(clockPin); }
};

struct NanosecondDelayTrace {
    fl::shared_ptr<fl::vector<u32>> mDelays = fl::make_shared<fl::vector<u32>>();

    NanosecondDelayTrace() {
        const auto delays = mDelays;
        fl::stub::setNanosecondDelayCallback([delays](u32 ns) FL_NO_EXCEPT {
            delays->push_back(ns);
        });
    }
    ~NanosecondDelayTrace() { fl::stub::clearNanosecondDelayCallback(); }
    NanosecondDelayTrace(const NanosecondDelayTrace&) = delete;
    NanosecondDelayTrace& operator=(const NanosecondDelayTrace&) = delete;
};

// Checks MSB-first output with zero padding past the lane's end.
void checkLane(const SpiTrace& trace, fl::size lane, const fl::vector<u8>& bytes, fl::size totalBytes) {
    FL_REQUIRE_EQ(trace.sampled[lane].size(), totalBytes * 8);
    for (fl::size i = 0; i < totalBytes * 8; ++i) {
        const u8 byte = (i / 8) < bytes.size() ? bytes[i / 8] : 0;
        FL_CHECK_EQ(trace.sampled[lane][i], static_cast<u8>((byte >> (7 - (i % 8))) & 1));
    }
}

ChannelDataPtr makeSpiVec(int dataPin, int clockPin, const fl::vector<u8>& bytes) {
    return makeSpi(dataPin, clockPin, bytes.data(), bytes.size());
}

} // namespace

FL_TEST_SUITE("BitBangChannelDriver") {

FL_TEST_CASE("poll always READY") {
    BitBangChannelDriver driver;
    FL_CHECK(driver.poll() == IChannelDriver::DriverState::READY);
}

FL_TEST_CASE("getName returns BIT_BANG") {
    BitBangChannelDriver driver;
    FL_CHECK(driver.getName() == fl::string::from_literal("BIT_BANG"));
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

FL_TEST_CASE("SPI trace: all byte values, MSB first, clock falls after each rise") {
    fl::vector<u8> bytes;
    for (int v = 0; v < 256; ++v) {
        bytes.push_back(static_cast<u8>(v));
    }
    BitBangChannelDriver driver;
    SpiTrace trace(21, {20});
    driver.enqueue(makeSpiVec(20, 21, bytes));
    driver.show();
    checkLane(trace, 0, bytes, bytes.size());
    FL_CHECK_EQ(trace.falling, 256 * 8 + 1);  // Plus parking the clock low in setup.
}

FL_TEST_CASE("SPI trace: 8 unequal lanes on one clock pad with zeros") {
    BitBangChannelDriver driver;
    fl::vector<int> pins = {10, 11, 12, 13, 14, 15, 16, 17};
    SpiTrace trace(18, pins);
    fl::vector<fl::vector<u8>> lanes;
    for (fl::size i = 0; i < pins.size(); ++i) {
        fl::vector<u8> bytes;
        for (fl::size b = 0; b <= i; ++b) {
            bytes.push_back(static_cast<u8>(0xA5 ^ (i * 17 + b * 3)));
        }
        lanes.push_back(bytes);
        driver.enqueue(makeSpiVec(pins[i], 18, bytes));
    }
    driver.show();
    for (fl::size i = 0; i < pins.size(); ++i) {
        checkLane(trace, i, lanes[i], pins.size());
    }
}

FL_TEST_CASE("SPI trace: separate clock groups transmit independently") {
    BitBangChannelDriver driver;
    const fl::vector<u8> first = {0xC3};
    const fl::vector<u8> second = {0x3C, 0x81};
    SpiTrace traceA(41, {40});
    SpiTrace traceB(43, {42});
    driver.enqueue(makeSpiVec(40, 41, first));
    driver.enqueue(makeSpiVec(42, 43, second));
    driver.show();
    checkLane(traceA, 0, first, first.size());
    checkLane(traceB, 0, second, second.size());
}

FL_TEST_CASE("SPI trace: duplicate data pin ORs lanes") {
    BitBangChannelDriver driver;
    SpiTrace trace(46, {45});
    driver.enqueue(makeSpiVec(45, 46, {0xF0}));
    driver.enqueue(makeSpiVec(45, 46, {0x0C}));
    driver.show();
    checkLane(trace, 0, {0xFC}, 1);
}

FL_TEST_CASE("SPI trace: empty transfer only parks the clock") {
    BitBangChannelDriver driver;
    SpiTrace trace(48, {47});
    driver.enqueue(makeSpiVec(47, 48, {}));
    driver.show();
    FL_CHECK_EQ(trace.sampled[0].size(), 0U);
    FL_CHECK_EQ(trace.falling, 1);
}

FL_TEST_CASE("SPI trace: repeated show of one channel uses its current data") {
    BitBangChannelDriver driver;
    SpiTrace trace(50, {49});
    auto ch = makeSpiVec(49, 50, {0x81});
    driver.enqueue(ch);
    driver.show();
    ch->getData()[0] = 0x7E;
    driver.enqueue(ch);
    driver.show();
    checkLane(trace, 0, {0x81, 0x7E}, 2);
}

FL_TEST_CASE("SPI trace: a channel with no clock pin writes no data") {
    BitBangChannelDriver driver;
    int edges = 0;
    fl::stub::setPinEdgeCallback(51, [&edges](bool, u32) FL_NO_EXCEPT { ++edges; });
    driver.enqueue(makeSpiVec(51, -1, {0xFF}));
    driver.show();
    fl::stub::clearPinEdgeCallback(51);
    FL_CHECK_EQ(edges, 0);
}

// #5127: channels sharing a clock pin used the first channel's rate, so a fast
// device enqueued first overclocked a slow one. Observe the actual driver's
// requested half-periods: host NOP loops do not guarantee wall-clock timing.
FL_TEST_CASE("SPI: a shared clock runs at the slowest requested rate in either order") {
    for (int fastFirst = 0; fastFirst < 2; ++fastFirst) {
        BitBangChannelDriver driver;
        const fl::vector<u8> bytes(32, 0x55);
        const auto make = [&bytes](int dataPin, u32 hz) {
            fl::vector_psram<u8> data(bytes.begin(), bytes.end());
            ChipsetVariant chipset = SpiChipsetConfig(dataPin, 54, SpiEncoder::apa102(hz));
            return ChannelData::create(chipset, fl::move(data));
        };
        auto fast = make(52, 4000000);
        auto slow = make(53, 250000);
        driver.enqueue(fastFirst ? fast : slow);
        driver.enqueue(fastFirst ? slow : fast);
        SpiTrace trace(54, {52, 53});
        NanosecondDelayTrace delays;
        driver.show();
        FL_CAPTURE(fastFirst);
        FL_REQUIRE_EQ(delays.mDelays->size(), 512u);
        for (u32 ns : *delays.mDelays) {
            FL_CHECK_EQ(ns, 2000u);
        }
        checkLane(trace, 0, bytes, bytes.size());
        checkLane(trace, 1, bytes, bytes.size());
        // Each channel parks the shared clock low before the 256 data bits.
        FL_CHECK_EQ(trace.falling, 258);
    }
}

} // FL_TEST_SUITE
