/// @file fastspi_arduino_core.cpp
/// @brief Runs the real ArdunioCoreSPIOutput on a recording SPIClass. Bytes still go
/// out, and single-bit writes are rejected at compile time instead of dropped.

#include "test.h"

#include "FastLED.h"
#include "fastspi_types.h"
#include "fl/stl/int.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/vector.h"

fl::vector<fl::u8> gBytes;
int gOpenTransactions = 0;
int gTransactions = 0;
int gBytesOutsideTransaction = 0;

struct SPISettings {
    SPISettings(fl::u32, int, int) {}
};

struct SPIClass {
    void begin() {}
    void beginTransaction(SPISettings) {
        ++gOpenTransactions;
        ++gTransactions;
    }
    void endTransaction() { --gOpenTransactions; }
    fl::u8 transfer(fl::u8 b) {
        gBytesOutsideTransaction += gOpenTransactions == 1 ? 0 : 1;
        gBytes.push_back(b);
        return 0;
    }
    fl::u16 transfer16(fl::u16 w) {
        gBytes.push_back(static_cast<fl::u8>(w >> 8));
        gBytes.push_back(static_cast<fl::u8>(w));
        return 0;
    }
};

SPIClass gSpi;

#ifndef MSBFIRST
#define MSBFIRST 1
#endif
#ifndef SPI_MODE0
#define SPI_MODE0 0
#endif
#define ARDUNIO_CORE_SPI
#include "platforms/fastspi_arduino_core.h"
#include "platforms/shared/spi_pixel_writer.h"
#undef ARDUNIO_CORE_SPI

namespace fl {

using Output = ArdunioCoreSPIOutput<11, 13, 1000000, gSpi>;

template <typename T>
auto canWriteBit(int) -> decltype(fl::declval<T&>().template writeBit<0>(fl::u8(1)), fl::true_type{});
template <typename T> fl::false_type canWriteBit(...);

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("a single-bit write is a compile error, not a silent no-op") {
    FL_CHECK_FALSE(decltype(canWriteBit<Output>(0))::value);
}

namespace {

void expectSixBytesInOneTransaction() {
    FL_REQUIRE_EQ(gBytes.size(), 6U);
    for (fl::size i = 0; i < 6; ++i) {
        FL_CHECK_EQ(gBytes[i], static_cast<u8>(i + 1));
    }
    FL_CHECK_EQ(gOpenTransactions, 0);
    FL_CHECK_EQ(gTransactions, 1);
    FL_CHECK_EQ(gBytesOutsideTransaction, 0);
}

void resetSpi() {
    gBytes.clear();
    gTransactions = 0;
    gBytesOutsideTransaction = 0;
}

} // namespace

FL_TEST_CASE("byte-only pixels still go out through the class's own writer") {
    resetSpi();
    Output output;
    CRGB leds[2] = {CRGB(1, 2, 3), CRGB(4, 5, 6)};
    PixelController<RGB> pixels(leds, 2, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    output.writePixels<0, DATA_NOP, RGB>(pixels);
    expectSixBytesInOneTransaction();
}

FL_TEST_CASE("byte-only pixels still go out through the shared writer chipsets use") {
    resetSpi();
    Output output;
    CRGB leds[2] = {CRGB(1, 2, 3), CRGB(4, 5, 6)};
    PixelController<RGB> pixels(leds, 2, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    fl::writePixelsToSPI<0, DATA_NOP, RGB>(pixels, output, nullptr);
    expectSixBytesInOneTransaction();
}

FL_TEST_CASE("byte and word writes are unchanged") {
    gBytes.clear();
    Output output;
    output.select();
    output.writeByte(0xA5);
    output.writeWord(0x1234);
    output.release();
    FL_REQUIRE_EQ(gBytes.size(), 3U);
    FL_CHECK_EQ(gBytes[0], 0xA5);
    FL_CHECK_EQ(gBytes[1], 0x12);
    FL_CHECK_EQ(gBytes[2], 0x34);
}

} // FL_TEST_FILE

} // namespace fl
