// The PCH imports the normal FastPin branch. This fresh internal component is
// also the implementation inherited by production FASTLED_FORCE_SOFTWARE_PINS.
#include "FastLED.h"
#include "fl/system/forced_software_pin.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"
#include "platforms/stub/stub_gpio.h"
#include "fl/stl/span.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/vector.h"
#include "test.h"

namespace { // anonymous namespace for forced pin lookup controls

volatile RwReg& mappedOutput() {
    static RwReg output;
    return output;
}
volatile RoReg& mappedInput() {
    static RoReg input;
    return input;
}

// This supplies addresses and masks only. All pin operations and every SPI
// loop remain the actual production implementation, including mapped controls.
template <fl::u8 PIN> struct MappedLookup {
    static constexpr bool kHasPinMap = true;
    static RwReg mask() { return PIN == 208 ? 1u << 16 : 1u << 17; }
    static volatile RwReg* output() { return &mappedOutput(); }
    static volatile RoReg* input() { return &mappedInput(); }
};

} // anonymous namespace for forced pin lookup controls

namespace fl {
template <> class FastPin<206> : public detail::ForcedSoftwarePin<206> {};
template <> class FastPin<207> : public detail::ForcedSoftwarePin<207> {};
template <> class FastPin<208> : public detail::ForcedSoftwarePin<208, ::MappedLookup<208>> {};
template <> class FastPin<209> : public detail::ForcedSoftwarePin<209, ::MappedLookup<209>> {};
} // namespace fl

FL_STATIC_ASSERT(!fl::detail::ForcedSoftwarePinMap<206>::kHasPinMap,
                 "native forced component must use the production NoPinMap lookup");

FL_TEST_FILE(FL_FILEPATH) {
using namespace fl;

namespace { // anonymous namespace for forced software SPI tests

// Divider8 uses the existing <=10 delay branch to isolate GPIO correctness.
// Divider16 currently links an unavailable delaycycles<0> specialization; that
// independent default-delay defect remains tracked by #5032, not mocked here.
using NoMapOutput = GenericSoftwareSPIOutput<206, 207, 8>;
using MappedOutput = GenericSoftwareSPIOutput<208, 209, 8>;

class Capture {
  public:
    Capture() {
        stub::setPinState(206, false);
        stub::setPinState(207, false);
        stub::setPinState(205, true);
        stub::setPinEdgeCallback(206, [this](bool high, u32) {
            ++data_writes;
            if (stub::getPinState(207) && high != data) stable = false;
            data = high;
        });
        stub::setPinEdgeCallback(207, [this](bool high, u32) {
            ++clock_writes;
            if (high && !clock) bits.push_back(stub::getPinState(206) ? 1 : 0);
            clock = high;
        });
    }
    ~Capture() {
        stub::clearPinEdgeCallback(206);
        stub::clearPinEdgeCallback(207);
    }
    void clear() {
        bits.clear();
        data_writes = 0;
        clock_writes = 0;
        stable = true;
    }
    fl::vector<u8> bits;
    int data_writes = 0;
    int clock_writes = 0;
    bool data = false;
    bool clock = false;
    bool stable = true;
};

void checkBytes(const Capture& capture, fl::span<const u8> expected) {
    FL_REQUIRE_EQ(capture.bits.size(), expected.size() * 8);
    for (size_t index = 0; index < expected.size(); ++index) {
        for (u8 bit = 0; bit < 8; ++bit) {
            FL_CHECK_EQ(capture.bits[index * 8 + bit],
                        static_cast<u8>((expected[index] >> (7 - bit)) & 1));
        }
    }
    FL_CHECK_EQ(capture.clock_writes, static_cast<int>(expected.size() * 16));
    FL_CHECK(capture.stable);
    FL_CHECK_FALSE(stub::getPinState(207));
    FL_CHECK(stub::getPinState(205));
}

} // anonymous namespace for forced software SPI tests

FL_TEST_CASE("Forced NoPinMap production SPI bulk must use real logical pin writes (#5056)") {
    Capture capture;
    NoMapOutput output;
    output.init();
    FL_CHECK_FALSE(stub::getPinState(207));
    capture.clear(); // Exclude the initialization LOW from payload write counts.
    FL_CHECK(fl::FastPin<206>::port() == nullptr);
    FL_CHECK(fl::FastPin<207>::port() == nullptr);
    u8 input[] = {0x00, 0xff, 0x55, 0xaa, 0x81};
    output.writeBytes(input, 5);
    checkBytes(capture, input);
}

FL_TEST_CASE("Forced NoPinMap production SPI scalar and repeats preserve every byte") {
    Capture capture;
    NoMapOutput output;
    output.init();
    FL_CHECK_FALSE(stub::getPinState(207));
    capture.clear();
    for (int value = 0; value < 256; ++value) {
        capture.clear();
        NoMapOutput::writeByte(static_cast<u8>(value));
        const u8 byte[] = {static_cast<u8>(value)};
        checkBytes(capture, byte);
        capture.clear();
        NoMapOutput::writeBytesValueRaw(static_cast<u8>(value), 2);
        const u8 repeated[] = {static_cast<u8>(value), static_cast<u8>(value)};
        checkBytes(capture, repeated);
    }
    capture.clear();
    output.writeBytesValue(0xa5, 2);
    const u8 repeated[] = {0xa5, 0xa5};
    checkBytes(capture, repeated);
    capture.clear();
    NoMapOutput::writeWord(0x80a5);
    const u8 word[] = {0x80, 0xa5};
    checkBytes(capture, word);
}

FL_TEST_CASE("Forced NoPinMap production SPI pixels preserve start bits and empty output") {
    Capture capture;
    NoMapOutput output;
    output.init();
    FL_CHECK_FALSE(stub::getPinState(207));
    capture.clear();
    CRGB colours[] = {CRGB(0x00, 0xff, 0x55), CRGB(0xaa, 0x80, 0x01)};
    PixelController<RGB> pixels(colours, 2, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    output.template writePixels<FLAG_START_BIT, DATA_NOP, RGB>(pixels);
    FL_REQUIRE_EQ(capture.bits.size(), 50);
    FL_CHECK_EQ(capture.bits[0], 1);
    FL_CHECK_EQ(capture.bits[25], 1);
    fl::vector<u8> payload;
    for (size_t index = 0; index < capture.bits.size(); ++index) {
        if (index != 0 && index != 25) payload.push_back(capture.bits[index]);
    }
    const u8 expected[] = {0x00, 0xff, 0x55, 0xaa, 0x80, 0x01};
    FL_REQUIRE_EQ(payload.size(), 48);
    for (size_t index = 0; index < 6; ++index) {
        for (u8 bit = 0; bit < 8; ++bit) {
            FL_CHECK_EQ(payload[index * 8 + bit], static_cast<u8>((expected[index] >> (7 - bit)) & 1));
        }
    }
    FL_CHECK_EQ(capture.clock_writes, 100);
    FL_CHECK(capture.stable);
    FL_CHECK_FALSE(stub::getPinState(207));
    capture.clear();
    u8 dummy = 0;
    output.writeBytes(&dummy, 0);
    NoMapOutput::writeBytesValueRaw(0xff, 0);
    PixelController<RGB> empty(colours, 0, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    output.template writePixels<FLAG_START_BIT, DATA_NOP, RGB>(empty);
    FL_CHECK(capture.bits.empty());
    FL_CHECK_EQ(capture.data_writes, 0);
    FL_CHECK_EQ(capture.clock_writes, 0);
}

FL_TEST_CASE("Forced NoPinMap direct pin operations use native logical GPIO") {
    Capture capture;
    using Pin = fl::FastPin<206>;
    Pin::setOutput();
    Pin::hi();
    FL_CHECK(stub::getPinState(206));
    Pin::lo();
    FL_CHECK_FALSE(stub::getPinState(206));
    Pin::hi(Pin::port());
    FL_CHECK(stub::getPinState(206));
    Pin::lo(Pin::port());
    FL_CHECK_FALSE(stub::getPinState(206));
    Pin::set(2);
    FL_CHECK(stub::getPinState(206));
    Pin::set(0);
    FL_CHECK_FALSE(stub::getPinState(206));
    Pin::fastset(Pin::port(), Pin::hival());
    FL_CHECK(stub::getPinState(206));
    Pin::fastset(Pin::port(), Pin::loval());
    FL_CHECK_FALSE(stub::getPinState(206));
    Pin::toggle();
    FL_CHECK(stub::getPinState(206));
    Pin::strobe();
    FL_CHECK(stub::getPinState(206));
    Pin::toggle();
    FL_CHECK_FALSE(stub::getPinState(206));
    Pin::setInput();
    FL_CHECK(Pin::port() == nullptr);
    FL_CHECK(stub::getPinState(205));
}

FL_TEST_CASE("Forced production pin component retains mapped register operations") {
    using Data = fl::FastPin<208>;
    using Clock = fl::FastPin<209>;
    mappedOutput() = 0x80000080u;
    mappedInput() = 0;
    Data::setOutput();
    Clock::setOutput();
    FL_CHECK(Data::port() == &mappedOutput());
    FL_CHECK(Clock::port() == &mappedOutput());
    FL_CHECK_EQ(Data::mask(), 1u << 16);
    Data::hi();
    FL_CHECK_EQ(mappedOutput(), 0x80010080u);
    Data::lo(Data::port());
    FL_CHECK_EQ(mappedOutput(), 0x80000080u);
    Data::set(0x91234567u);
    FL_CHECK_EQ(mappedOutput(), 0x91234567u);
    Data::fastset(Data::port(), 0x80000080u);
    FL_CHECK_EQ(mappedOutput(), 0x80000080u);
    Data::toggle();
    FL_CHECK_EQ(mappedInput(), 1u << 16);
    MappedOutput output;
    output.init();
    u8 input[] = {0x00, 0xff, 0x81};
    output.writeBytes(input, 3);
    FL_CHECK_EQ(mappedOutput(), 0x80010080u);
    FL_CHECK_EQ(Clock::mask(), 1u << 17);
}

} // FL_TEST_FILE
