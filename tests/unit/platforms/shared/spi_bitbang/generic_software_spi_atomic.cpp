/// @file generic_software_spi_atomic.cpp
/// @brief Runs the production generic software SPI writer through an atomic
/// per-pin policy, the shape ESP32 uses, on a simulated GPIO output register.
///
/// fastpin_esp32.h needs vendor headers, so its FastPin and BitbangPin are not
/// compiled here. The fixture pins and policy stand in for them; the writer and
/// writeBitSetClearNoBranch are production code. Target code generation is checked from
/// ESP32 disassembly instead.

#include "test.h"

#include "fastspi_types.h"
#include "fl/stl/int.h"
#include "fl/stl/vector.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"

namespace fl {
namespace atomic_spi_test {

constexpr u32 kData = 1U << 3;
constexpr u32 kClock = 1U << 17;
constexpr u32 kOther = 0x80000001U;
constexpr u32 kInjected = 1U << 9;
constexpr u8 kDataPin = 240;
constexpr u8 kClockPin = 241;

u32 gOutput = 0;
volatile u32 gSet = 0;
volatile u32 gClear = 0;
fl::vector<u8> gRisingBits;
int gClockLowStores = 0;
int gDataStores = 0;
int gDataStoresSinceEdge = 0;
bool gMultipleDataStoresForEdge = false;
bool gInjectConcurrentUpdates = false;

// Applies what writeBitSetClear stored to the simulated output register.
void applyDataStore() {
    const u32 set = gSet;
    const u32 clear = gClear;
    gSet = 0;
    gClear = 0;
    gOutput = (gOutput | set) & ~clear;
    ++gDataStores;
    if (++gDataStoresSinceEdge > 1) {
        gMultipleDataStoresForEdge = true;
    }
}

} // namespace atomic_spi_test

template <> class FastPin<atomic_spi_test::kDataPin> {
  public:
    typedef volatile u32* port_ptr_t;
    typedef u32 port_t;
    static constexpr bool validpin() { return true; }
    static void setOutput() {}
    static u32 mask() { return atomic_spi_test::kData; }
    static port_ptr_t sport() { return &atomic_spi_test::gSet; }
    static port_ptr_t cport() { return &atomic_spi_test::gClear; }
    static port_ptr_t port() { return &atomic_spi_test::gSet; }
    static void hi() {}
    static void lo() {}
    static port_t hival() { return 0; }
    static port_t loval() { return 0; }
    static void fastset(port_ptr_t, port_t) { FL_FAIL("whole-port store on an atomic bus"); }
    static void hi(port_ptr_t) {}
    static void lo(port_ptr_t) {}
};

template <> class FastPin<atomic_spi_test::kClockPin> {
  public:
    typedef volatile u32* port_ptr_t;
    typedef u32 port_t;
    static constexpr bool validpin() { return true; }
    static void setOutput() {}
    static u32 mask() { return atomic_spi_test::kClock; }
    static port_ptr_t sport() { return &atomic_spi_test::gSet; }
    static port_ptr_t cport() { return &atomic_spi_test::gClear; }
    static port_ptr_t port() { return &atomic_spi_test::gSet; }
    static void hi() {
        using namespace atomic_spi_test;
        gOutput |= kClock;
        gRisingBits.push_back((gOutput & kData) ? 1 : 0);
        FL_CHECK_EQ(gDataStoresSinceEdge, 1); // Data settled before this edge.
        gDataStoresSinceEdge = 0;
    }
    static void lo() {
        using namespace atomic_spi_test;
        gOutput &= ~kClock;
        ++gClockLowStores;
        if (gInjectConcurrentUpdates) {
            gOutput ^= kInjected; // Another context toggles an unrelated pin.
        }
    }
    static port_t hival() { return 0; }
    static port_t loval() { return 0; }
    static void fastset(port_ptr_t, port_t) { FL_FAIL("whole-port store on an atomic bus"); }
    static void hi(port_ptr_t) { hi(); }
    static void lo(port_ptr_t) { lo(); }
};

template <> struct BitbangPin<FastPin<atomic_spi_test::kDataPin>>
    : BitbangPinDefaults<FastPin<atomic_spi_test::kDataPin>> {
    static constexpr bool kAtomicWrite = true;
    template <u8 BIT> static void write(u8 value) {
        writeBitSetClearNoBranch<BIT>(value, atomic_spi_test::kData, &atomic_spi_test::gSet,
                              &atomic_spi_test::gClear);
        atomic_spi_test::applyDataStore();
    }
};

FL_TEST_FILE(FL_FILEPATH) {
using namespace atomic_spi_test;
using Spi = GenericSoftwareSPIOutput<kDataPin, kClockPin, 4>;

namespace {

void reset() {
    gOutput = kOther;
    gSet = 0;
    gClear = 0;
    gRisingBits.clear();
    gClockLowStores = 0;
    gDataStores = 0;
    gDataStoresSinceEdge = 0;
    gMultipleDataStoresForEdge = false;
    gInjectConcurrentUpdates = false;
}

void check(const fl::vector<u8>& expected) {
    const size_t bits = expected.size() * 8;
    FL_REQUIRE_EQ(gRisingBits.size(), bits);
    FL_CHECK_EQ(gDataStores, static_cast<int>(bits));
    FL_CHECK_EQ(gClockLowStores, static_cast<int>(bits));
    FL_CHECK_FALSE(gMultipleDataStoresForEdge);
    for (size_t i = 0; i < bits; ++i) {
        const u8 bit = (expected[i / 8] >> (7 - (i % 8))) & 1;
        FL_CHECK_EQ(gRisingBits[i], bit);
    }
    FL_CHECK_EQ(gOutput & kClock, 0U); // First and last edges leave the clock low.
    FL_CHECK_EQ(gOutput & kOther, kOther);
}

} // namespace

FL_TEST_CASE("Atomic writeByte sends all 256 values with one data store per clock") {
    for (int value = 0; value < 256; ++value) {
        reset();
        Spi::writeByte(static_cast<u8>(value));
        check({static_cast<u8>(value)});
    }
}

FL_TEST_CASE("Atomic back-to-back writeByte keeps every byte") {
    reset();
    fl::vector<u8> expected;
    for (int value = 0; value < 256; ++value) {
        Spi::writeByte(static_cast<u8>(value));
        expected.push_back(static_cast<u8>(value));
    }
    check(expected);
}

FL_TEST_CASE("Atomic writeBytes handles empty and multi-byte input without port stores") {
    reset();
    Spi spi;
    u8 unused = 0xFF;
    spi.writeBytes<DATA_NOP>(&unused, 0);
    FL_CHECK(gRisingBits.empty());
    FL_CHECK_EQ(gClockLowStores, 0);

    reset();
    fl::vector<u8> input = {0x00, 0xFF, 0xAA, 0x55, 0x81, 0x7E};
    spi.writeBytes<DATA_NOP>(input.data(), static_cast<int>(input.size()));
    check({0x00, 0xFF, 0xAA, 0x55, 0x81, 0x7E});
}

FL_TEST_CASE("Atomic writeBytesValueRaw handles empty and repeated values") {
    reset();
    Spi::writeBytesValueRaw(0xA5, 0);
    FL_CHECK(gRisingBits.empty());

    reset();
    Spi::writeBytesValueRaw(0xA5, 3);
    check({0xA5, 0xA5, 0xA5});
}

FL_TEST_CASE("Atomic writePixels sends pixels with and without a start bit") {
    CRGB leds[1] = {CRGB(0x12, 0x34, 0x56)};
    reset();
    Spi spi;
    PixelController<RGB> plain(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    spi.writePixels<0, DATA_NOP, RGB>(plain);
    check({0x12, 0x34, 0x56});

    reset();
    PixelController<RGB> flagged(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    spi.writePixels<FLAG_START_BIT, DATA_NOP, RGB>(flagged);
    FL_REQUIRE_EQ(gRisingBits.size(), 25U); // Start bit plus 24 data bits.
    FL_CHECK_EQ(gRisingBits[0], 1);
}

FL_TEST_CASE("Atomic writes never store the whole port and leave unrelated pins alone") {
    reset();
    gInjectConcurrentUpdates = true;
    Spi::writeBytesValueRaw(0x5A, 1);
    // A whole-port store fails through fastset; this model cannot lose a toggle.
    FL_CHECK_EQ(gOutput & kInjected, 0U);
    FL_CHECK_EQ(gOutput & kOther, kOther);

    reset();
    gInjectConcurrentUpdates = true;
    Spi::writeBytesValueRaw(0x5A, 1);
    Spi::writeBit<7>(0x80);
    FL_CHECK_EQ(gOutput & kInjected, kInjected); // A ninth toggle is visible.
}

} // FL_TEST_FILE

} // namespace fl
