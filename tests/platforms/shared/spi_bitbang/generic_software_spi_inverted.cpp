// Regression coverage for production logical-clock polarity (#5044).
#include "FastLED.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"
#include "test.h"
#include "fl/stl/span.h"
#include "generic_software_spi_fixture.h" // ok test include

FL_TEST_FILE(FL_FILEPATH) {
using namespace fl;

namespace { // anonymous namespace for inverted software SPI coverage

void checkLogicalBytes(fl::span<const u8> actual, fl::span<const u8> bytes) {
    FL_REQUIRE_EQ(actual.size(), bytes.size() * 8);
    for (size_t index = 0; index < bytes.size(); ++index) {
        for (u8 bit = 0; bit < 8; ++bit) {
            FL_CHECK_EQ(actual[index * 8 + bit],
                        static_cast<u8>((bytes[index] >> (7 - bit)) & 1));
        }
    }
}

template <int Model, u8 Bit> void checkEverySingleBit() {
    SpiTraceFixture<Model> trace;
    using Output = typename SpiTraceFixture<Model>::Output;
    trace.reset();
    for (int value = 0; value < 256; ++value) {
        trace.clearTrace();
        Output::template writeBit<Bit>(static_cast<u8>(value));
        FL_REQUIRE_EQ(trace.bits().size(), 1);
        FL_CHECK_EQ(trace.bits()[0], static_cast<u8>((value >> Bit) & 1));
        FL_CHECK(trace.clockLow());
        FL_CHECK(trace.dataStable());
    }
}

template <int Model> void checkPolarityPaths() {
    SpiTraceFixture<Model> trace;
    using Output = typename SpiTraceFixture<Model>::Output;
    trace.reset();
    Output output(trace.selectable());
    u8 values[256];
    for (int value = 0; value < 256; ++value) values[value] = static_cast<u8>(value);
    output.writeBytes(values, 256);
    checkLogicalBytes(trace.bits(), values);
    FL_CHECK(trace.clockLow());
    FL_CHECK(trace.dataStable());
    FL_CHECK(trace.unrelatedPreserved());
    FL_CHECK(trace.selectionOrderValid());
    const bool stream = SpiTraceState<Model>::kStream;
    FL_CHECK_EQ(trace.gpioWrites(), stream ? 2 * 256 * 8 + 1 : 3 * 256 * 8);

    for (int value = 0; value < 256; ++value) {
        trace.clearTrace();
        // Raw repeat intentionally has no selectable contract.
        SpiTraceState<Model>::get().require_selected = false;
        Output::writeBytesValueRaw(static_cast<u8>(value), 2);
        const u8 repeated[] = {static_cast<u8>(value), static_cast<u8>(value)};
        checkLogicalBytes(trace.bits(), repeated);
        FL_CHECK_EQ(trace.gpioWrites(), stream ? 33 : 48);
        FL_CHECK(trace.clockLow());
        FL_CHECK(trace.dataStable());
        FL_CHECK(trace.unrelatedPreserved());
    }

    trace.clearTrace();
    output.writeBytesValue(0xa5, 2);
    const u8 repeated[] = {0xa5, 0xa5};
    checkLogicalBytes(trace.bits(), repeated);
    FL_CHECK(trace.clockLow());
    FL_CHECK(trace.dataStable());

    trace.clearTrace();
    CRGB colours[] = {CRGB(0x00, 0xff, 0x55), CRGB(0xaa, 0x80, 0x01)};
    PixelController<RGB> pixels(colours, 2, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    output.template writePixels<FLAG_START_BIT, DATA_NOP, RGB>(pixels);
    FL_REQUIRE_EQ(trace.bits().size(), 50);
    FL_CHECK_EQ(trace.bits()[0], 1);
    FL_CHECK_EQ(trace.bits()[25], 1);
    fl::vector<u8> payload;
    for (size_t index = 0; index < trace.bits().size(); ++index) {
        if (index != 0 && index != 25) payload.push_back(trace.bits()[index]);
    }
    const u8 expected[] = {0x00, 0xff, 0x55, 0xaa, 0x80, 0x01};
    checkLogicalBytes(payload, expected);
    FL_CHECK_EQ(trace.gpioWrites(), stream ? 101 : 150);
    FL_CHECK(trace.clockLow());
    FL_CHECK(trace.dataStable());
    FL_CHECK(trace.unrelatedPreserved());

    trace.clearTrace();
    output.writeBytes(values, 0);
    Output::writeBytesValueRaw(0xff, 0);
    PixelController<RGB> empty(colours, 0, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    output.template writePixels<FLAG_START_BIT, DATA_NOP, RGB>(empty);
    FL_CHECK(trace.bits().empty());
    FL_CHECK_EQ(trace.gpioWrites(), 0);
    FL_CHECK(trace.clockLow());

    checkEverySingleBit<Model, 0>();
    checkEverySingleBit<Model, 1>();
    checkEverySingleBit<Model, 2>();
    checkEverySingleBit<Model, 3>();
    checkEverySingleBit<Model, 4>();
    checkEverySingleBit<Model, 5>();
    checkEverySingleBit<Model, 6>();
    checkEverySingleBit<Model, 7>();
}

template <int Model> void checkInitClock() {
    SpiTraceFixture<Model> trace;
    using Output = typename SpiTraceFixture<Model>::Output;
    trace.reset();
    fl::FastPin<221 + Model * 2>::setOutput();
    fl::FastPin<221 + Model * 2>::hi();
    trace.clearTrace();
    Output output;
    output.init();
    FL_CHECK(trace.clockLow());
    trace.clearTrace();
    Output::writeByte(0xa5);
    const u8 expected[] = {0xa5};
    checkLogicalBytes(trace.bits(), expected);
    FL_CHECK(trace.clockLow());
    FL_CHECK(trace.unrelatedPreserved());
}

} // anonymous namespace for inverted software SPI coverage

FL_TEST_CASE("Software SPI inverted same-port bulk must preserve logical GPIO edges (#5044)") {
    // This original RED reproducer must remain sensitive to the legacy
    // unconditional OR-high / AND-low construction of physical clock words.
    SpiTraceFixture<6> trace;
    using Output = SpiTraceFixture<6>::Output;
    trace.reset();
    Output output;
    u8 input[] = {0x00, 0xff, 0x80, 0x01, 0xa5};
    output.writeBytes(input, 5);
    FL_REQUIRE_EQ(trace.bits().size(), 40);
    for (size_t index = 0; index < 5; ++index) {
        for (u8 bit = 0; bit < 8; ++bit) {
            FL_CHECK_EQ(trace.bits()[index * 8 + bit],
                        static_cast<u8>((input[index] >> (7 - bit)) & 1));
        }
    }
    FL_CHECK(trace.dataStable());
    FL_CHECK(trace.clockLow());
    FL_CHECK(trace.unrelatedPreserved());
}

FL_TEST_CASE("Software SPI logical levels cover all bytes and independent data-clock polarities") {
    checkPolarityPaths<4>();  // normal data/clock, legacy same-port
    checkPolarityPaths<6>();  // inverted data/clock, legacy same-port
    checkPolarityPaths<12>(); // inverted data only, legacy same-port
    checkPolarityPaths<13>(); // inverted clock only, legacy same-port
    checkPolarityPaths<10>(); // normal data/clock, streaming same-port
    checkPolarityPaths<11>(); // inverted data/clock, streaming same-port
    checkPolarityPaths<14>(); // inverted data only, streaming same-port
    checkPolarityPaths<15>(); // inverted clock only, streaming same-port
    checkPolarityPaths<16>(); // inverted independent ports, atomic policy
    checkPolarityPaths<17>(); // inverted independent ports, snapshot path
}

FL_TEST_CASE("Software SPI init establishes logical idle across pin policies (#5057)") {
    checkInitClock<4>();
    checkInitClock<6>();
    checkInitClock<10>();
    checkInitClock<11>();
    checkInitClock<16>();
    checkInitClock<17>();
}

FL_TEST_CASE("Software SPI inverted atomic path preserves concurrent unrelated GPIO") {
    SpiTraceFixture<16> trace;
    using Output = SpiTraceFixture<16>::Output;
    trace.reset();
    trace.injectInterference(true);
    Output output;
    u8 input[] = {0x00, 0xff, 0x55, 0xaa};
    output.writeBytes(input, 4);
    checkLogicalBytes(trace.bits(), input);
    FL_CHECK(trace.clockLow());
    FL_CHECK(trace.unrelatedPreserved());
    FL_CHECK(trace.dataStable());
}
} // FL_TEST_FILE
