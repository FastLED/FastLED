/// @file fastspi.cpp
/// @brief Runs the public SoftwareSPIOutput wrapper from fastspi.h, not only the
/// GenericSoftwareSPIOutput it derives from, on a fake GPIO port. The port records
/// the data level on every low-to-high clock transition.

#include "test.h"

#include "FastLED.h"
#include "fastspi.h"
#include "fl/stl/int.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/vector.h"

namespace fl {
namespace fastspi_test {

constexpr u32 kDataMask = 1U << 5;
constexpr u32 kClockMask = 1U << 21;

volatile u32 gPort = 0;
fl::vector<u8> gSamples;

bool clockHigh() { return (gPort & kClockMask) != 0; }

void store(volatile u32* port, u32 next) {
    const bool before = clockHigh();
    *port = next;
    if (!before && clockHigh()) {
        gSamples.push_back((gPort & kDataMask) ? 1 : 0);
    }
}

template <u32 MASK> struct FakePin {
    typedef volatile u32* port_ptr_t;
    typedef u32 port_t;
    static constexpr bool validpin() { return true; }
    static void setOutput() {} // Keeps the latch, so init() must park the clock.
    static void hi() { store(&gPort, gPort | MASK); }
    static void lo() { store(&gPort, gPort & ~MASK); }
    static void hi(port_ptr_t) { hi(); }
    static void lo(port_ptr_t) { lo(); }
    static void fastset(port_ptr_t port, port_t value) { store(port, value); }
    static port_ptr_t port() { return &gPort; }
    static port_t hival() { return gPort | MASK; }
    static port_t loval() { return gPort & ~MASK; }
    static port_t mask() { return MASK; }
};

struct CountingSelect : public Selectable {
    int selects = 0;
    int releases = 0;
    void select() FL_NO_EXCEPT override { ++selects; }
    void release() FL_NO_EXCEPT override { ++releases; }
    bool isSelected() FL_NO_EXCEPT override { return selects > releases; }
};

} // namespace fastspi_test

template <> class FastPin<246> : public fastspi_test::FakePin<fastspi_test::kDataMask> {};
template <> class FastPin<247> : public fastspi_test::FakePin<fastspi_test::kClockMask> {};

} // namespace fl

// The wrapper the public API names, at a divider small enough to skip padding.
using WrapperOutput = SoftwareSPIOutput<246, 247, 2>;
FL_STATIC_ASSERT((fl::is_base_of<fl::GenericSoftwareSPIOutput<246, 247, 2>, WrapperOutput>::value),
                 "on a non-ESP32 host, SoftwareSPIOutput forwards its divider unchanged");

FL_TEST_FILE(FL_FILEPATH) {
using namespace fl;
using namespace fl::fastspi_test;

namespace {

// Leaves the clock latched high, as a previous owner might, then runs init().
void primeAndInit(WrapperOutput& output) {
    gPort = kClockMask;
    gSamples.clear();
    output.init();
    FL_REQUIRE_FALSE(clockHigh());
    FL_REQUIRE_EQ(gSamples.size(), 0U);
}

void requireByte(u8 value, fl::size offset) {
    for (int bit = 0; bit < 8; ++bit) {
        const u8 expected = static_cast<u8>((value >> (7 - bit)) & 1);
        if (gSamples[offset + bit] != expected) {
            FL_FAIL("byte 0x" << static_cast<int>(value) << " bit " << bit << " (MSB first) sampled "
                              << static_cast<int>(gSamples[offset + bit]));
        }
    }
}

} // namespace

FL_TEST_CASE("SoftwareSPIOutput writes every byte MSB first, one rising edge per bit") {
    WrapperOutput output;
    primeAndInit(output);
    for (int v = 0; v < 256; ++v) {
        gSamples.clear();
        WrapperOutput::writeByte(static_cast<u8>(v));
        FL_REQUIRE_EQ(gSamples.size(), 8U);
        requireByte(static_cast<u8>(v), 0);
        FL_REQUIRE_FALSE(clockHigh());
    }
}

FL_TEST_CASE("SoftwareSPIOutput writeBytes streams all 256 values inside one select") {
    WrapperOutput output;
    CountingSelect cs;
    output.setSelect(&cs);
    primeAndInit(output);
    FL_CHECK_EQ(cs.releases, 1); // init() releases the select.
    u8 data[256];
    for (int v = 0; v < 256; ++v) {
        data[v] = static_cast<u8>(v);
    }
    output.writeBytes(data, 256);
    FL_REQUIRE_EQ(gSamples.size(), 256U * 8U);
    for (int v = 0; v < 256; ++v) {
        requireByte(static_cast<u8>(v), static_cast<fl::size>(v) * 8U);
    }
    FL_CHECK_EQ(cs.selects, 1);
    FL_CHECK_EQ(cs.releases, 2);
    FL_CHECK_FALSE(cs.isSelected());
    FL_CHECK_FALSE(clockHigh()); // Clock idles low after release().
}

FL_TEST_CASE("SoftwareSPIOutput writeBit and the start bit are one edge each") {
    WrapperOutput output;
    primeAndInit(output);
    WrapperOutput::writeBit<0>(1);
    WrapperOutput::writeBit<0>(0);
    FL_REQUIRE_EQ(gSamples.size(), 2U);
    FL_CHECK_EQ(gSamples[0], 1);
    FL_CHECK_EQ(gSamples[1], 0);
    FL_CHECK_FALSE(clockHigh());

    primeAndInit(output);
    CRGB led(0x81, 0x00, 0xFF);
    PixelController<RGB> pixels(&led, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    output.template writePixels<FLAG_START_BIT, DATA_NOP, RGB>(pixels);
    FL_REQUIRE_EQ(gSamples.size(), 25U);
    FL_CHECK_EQ(gSamples[0], 1); // Start bit precedes the first byte.
    requireByte(0x81, 1);
    requireByte(0x00, 9);
    requireByte(0xFF, 17);
    FL_CHECK_FALSE(clockHigh()); // Clock idles low after the release.
}

} // FL_TEST_FILE
