// Execute the production software SPI template; the fixture observes GPIO only.
#include "FastLED.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "test.h"

// Test-only pin/register instrumentation; it never supplies an SPI writer.
#include "generic_software_spi_fixture.h" // ok test include

FL_TEST_FILE(FL_FILEPATH) {
using namespace fl;

namespace { // anonymous namespace for generic_software_spi

void checkBits(fl::span<const u8> actual, fl::span<const u8> bytes) {
    FL_REQUIRE_EQ(actual.size(), bytes.size() * 8);
    for (size_t index = 0; index < bytes.size(); ++index) {
        for (u8 bit = 0; bit < 8; ++bit) {
            FL_CHECK_EQ(actual[index * 8 + bit],
                        static_cast<u8>((bytes[index] >> (7 - bit)) & 1));
        }
    }
}

struct AdjustBytes {
    static int& adjusted() { static int value = 0; return value; }
    static int& posted() { static int value = -1; return value; }
    static void*& context() { static void* value = nullptr; return value; }
    static u8 adjust(u8 value) {
        ++adjusted();
        return value ^ 0x5a;
    }
    static void postBlock(int length, void* writer) {
        posted() = length;
        context() = writer;
    }
    static void reset() { adjusted() = 0; posted() = -1; context() = nullptr; }
};

template <int Model> void checkProductionScalar() {
    SpiTraceFixture<Model> trace;
    using Output = typename SpiTraceFixture<Model>::Output;
    trace.reset();
    for (int value = 0; value < 256; ++value) {
        trace.clearTrace();
        Output::writeByte(static_cast<u8>(value));
        const u8 expected[] = {static_cast<u8>(value)};
        checkBits(trace.bits(), expected);
        FL_CHECK_EQ(trace.gpioWrites(), 24);
        FL_CHECK(trace.clockLow());
        FL_CHECK(trace.unrelatedPreserved());
        FL_CHECK(trace.dataStable());
        FL_CHECK_GT(trace.highDelayCalls(), 0);
        FL_CHECK_GT(trace.lowDelayCalls(), 0);
    }
    trace.clearTrace();
    Output::writeWord(0x80a5);
    const u8 word[] = {0x80, 0xa5};
    checkBits(trace.bits(), word);
    FL_CHECK_EQ(trace.gpioWrites(), 48);
}

template <int Model> void checkProductionBulk() {
    SpiTraceFixture<Model> trace;
    using Output = typename SpiTraceFixture<Model>::Output;
    Output output;
    trace.reset();
    u8 input[] = {0x00, 0xff, 0x80, 0x01, 0xa5};
    AdjustBytes::reset();
    output.template writeBytes<AdjustBytes>(input, 5);
    const u8 expected[] = {0x5a, 0xa5, 0xda, 0x5b, 0xff};
    checkBits(trace.bits(), expected);
    FL_CHECK_EQ(trace.gpioWrites(), 5 * 24);
    FL_CHECK_EQ(AdjustBytes::adjusted(), 5);
    FL_CHECK_EQ(AdjustBytes::posted(), 5);
    FL_CHECK_EQ(AdjustBytes::context(), static_cast<void*>(&output));
    FL_CHECK(trace.clockLow());
    FL_CHECK(trace.unrelatedPreserved());
    FL_CHECK(trace.dataStable());

    trace.clearTrace();
    Output::writeBytesValueRaw(0x81, 3);
    const u8 repeated[] = {0x81, 0x81, 0x81};
    checkBits(trace.bits(), repeated);
    FL_CHECK_EQ(trace.gpioWrites(), 72);

    trace.clearTrace();
    output.writeBytesValue(0x42, 2);
    const u8 selected_repeat[] = {0x42, 0x42};
    checkBits(trace.bits(), selected_repeat);

    trace.clearTrace();
    output.writeBytes(input, 0);
    Output::writeBytesValueRaw(0xff, 0);
    FL_CHECK_EQ(trace.gpioWrites(), 0);
    FL_CHECK(trace.bits().empty());
}

template <int Model> void checkProductionStreaming() {
    SpiTraceFixture<Model> trace;
    using Output = typename SpiTraceFixture<Model>::Output;
    trace.reset();
    Output output(trace.selectable());
    u8 values[256];
    for (int value = 0; value < 256; ++value) values[value] = static_cast<u8>(value);
    output.writeBytes(values, 256);
    checkBits(trace.bits(), values);
    // This assertion observes production writeByteStreaming/finishByteStream,
    // not a flag: restoring their old three-store body must fail this count.
    FL_CHECK_EQ(trace.gpioWrites(), 2 * 256 * 8 + 1);
    FL_CHECK(trace.clockLow());
    FL_CHECK(trace.dataStable());
    FL_CHECK(trace.unrelatedPreserved());
    FL_CHECK(trace.selectionOrderValid());
    FL_CHECK_GT(trace.highDelayCalls(), 0);
    FL_CHECK_GT(trace.lowDelayCalls(), 0);

    trace.clearTrace();
    Output::writeBytesValueRaw(0x81, 3);
    const u8 repeated[] = {0x81, 0x81, 0x81};
    checkBits(trace.bits(), repeated);
    FL_CHECK_EQ(trace.gpioWrites(), 2 * 24 + 1);
    FL_CHECK(trace.clockLow());

    trace.clearTrace();
    u8 tail[] = {0x80, 0x00, 0x01};
    output.writeBytes(tail, 3);
    checkBits(trace.bits(), tail);
    FL_CHECK_EQ(trace.gpioWrites(), 2 * 24 + 1);
    FL_CHECK(trace.clockLow());
    FL_CHECK(trace.selectionOrderValid());

    trace.clearTrace();
    output.writeBytes(tail, 0);
    Output::writeBytesValueRaw(0xff, 0);
    FL_CHECK_EQ(trace.gpioWrites(), 0);
    FL_CHECK(trace.bits().empty());
    FL_CHECK(trace.clockLow());
    FL_CHECK(trace.selectionOrderValid());

    trace.clearTrace();
    CRGB colours[] = {CRGB(0x00, 0xff, 0x80), CRGB(0x01, 0xa5, 0x42)};
    PixelController<RGB> pixels(colours, 2, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    output.template writePixels<FLAG_START_BIT, DATA_NOP, RGB>(pixels);
    FL_REQUIRE_EQ(trace.bits().size(), 50);
    FL_CHECK_EQ(trace.bits()[0], 1);
    FL_CHECK_EQ(trace.bits()[25], 1);
    fl::vector<u8> payload;
    for (size_t index = 0; index < trace.bits().size(); ++index) {
        if (index != 0 && index != 25) payload.push_back(trace.bits()[index]);
    }
    const u8 expected[] = {0x00, 0xff, 0x80, 0x01, 0xa5, 0x42};
    checkBits(payload, expected);
    FL_CHECK_EQ(trace.gpioWrites(), 2 * 50 + 1);
    FL_CHECK(trace.dataStable());
    FL_CHECK(trace.clockLow());
    FL_CHECK(trace.selectionOrderValid());
}

FL_TEST_CASE("Software SPI production scalar traces preserve MSB edges and widths") {
    // 8/16/32-bit registers, equal and independent port identities.
    checkProductionScalar<0>();
    checkProductionScalar<1>();
    checkProductionScalar<2>();
    checkProductionScalar<3>();
    checkProductionScalar<4>();
    checkProductionScalar<5>();
    // Inverted GPIO model checks logical scalar edges, not stream eligibility.
    checkProductionScalar<6>();
}

FL_TEST_CASE("Software SPI production init establishes low clock before first byte") {
    SpiTraceFixture<4> trace;
    using Output = SpiTraceFixture<4>::Output;
    trace.reset();
    fl::FastPin<229>::setOutput();
    fl::FastPin<229>::hi();
    trace.clearTrace();

    Output output;
    output.init();
    FL_CHECK(trace.clockLow());
    Output::writeByte(0xa5);
    const u8 expected[] = {0xa5};
    checkBits(trace.bits(), expected);
    FL_CHECK(trace.clockLow());
}

FL_TEST_CASE("Software SPI production bulk repeat and callbacks preserve bytes") {
    checkProductionBulk<0>();
    checkProductionBulk<1>();
    checkProductionBulk<2>();
    checkProductionBulk<3>();
    checkProductionBulk<4>();
    checkProductionBulk<5>();
}

FL_TEST_CASE("Software SPI production streaming uses two stores per bit plus final edge") {
    checkProductionStreaming<8>();
    checkProductionStreaming<9>();
    checkProductionStreaming<10>();
}

FL_TEST_CASE("Software SPI production pixels emit start bits and invoke adjustment") {
    SpiTraceFixture<4> trace;
    using Output = SpiTraceFixture<4>::Output;
    Output output;
    CRGB colours[] = {CRGB(0x00, 0xff, 0x80), CRGB(0x01, 0xa5, 0x42)};
    PixelController<RGB> pixels(colours, 2, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    trace.reset();
    AdjustBytes::reset();
    output.template writePixels<FLAG_START_BIT, AdjustBytes, RGB>(pixels);
    const u8 expected[] = {0x5a, 0xa5, 0xda, 0x5b, 0xff, 0x18};
    FL_REQUIRE_EQ(trace.bits().size(), 50);
    FL_CHECK_EQ(trace.bits()[0], 1);
    FL_CHECK_EQ(trace.bits()[25], 1);
    fl::vector<u8> payload;
    for (size_t index = 0; index < trace.bits().size(); ++index) {
        if (index != 0 && index != 25) payload.push_back(trace.bits()[index]);
    }
    checkBits(payload, expected);
    FL_CHECK_EQ(trace.gpioWrites(), 150);
    FL_CHECK_EQ(AdjustBytes::adjusted(), 6);
    FL_CHECK_EQ(AdjustBytes::posted(), 2);
    FL_CHECK_EQ(AdjustBytes::context(), static_cast<void*>(&output));
    FL_CHECK(trace.clockLow());
}

FL_TEST_CASE("Software SPI production atomic path preserves concurrent unrelated GPIO") {
    SpiTraceFixture<7> trace;
    using Output = SpiTraceFixture<7>::Output;
    Output output;
    trace.reset();
    trace.injectInterference(true);
    u8 input[] = {0x00, 0xff, 0x55, 0xaa};
    output.writeBytes(input, 4);
    checkBits(trace.bits(), input);
    FL_CHECK(trace.unrelatedPreserved());
    FL_CHECK_EQ(trace.gpioWrites(), 96);
    FL_CHECK(trace.clockLow());
}

} // anonymous namespace for generic_software_spi
} // FL_TEST_FILE
