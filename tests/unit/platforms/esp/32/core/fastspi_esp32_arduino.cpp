/// @file fastspi_esp32_arduino.cpp
/// @brief Runs the real opt-in Arduino ESP32SPIOutput (FL_ESP32_SPI_ARDUINO=1) on a
/// recording SPIClass. SPIClass can only send whole bytes, so a single-bit write is
/// a compile error instead of eight clocks (#5060). Byte traffic is unchanged.

#include "test.h"

#include "FastLED.h"
#include "fastspi_types.h"
#include "fl/stl/int.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/vector.h"
#include "platforms/arduino/spi/fastspi_esp32_arduino.h"
#include "platforms/shared/spi_pixel_writer.h"

namespace {

fl::vector<fl::u8> gBytes;
int gOpenTransactions = 0;
int gTransactions = 0;
int gBytesOutsideTransaction = 0;

void record(fl::u8 b) {
    gBytesOutsideTransaction += gOpenTransactions == 1 ? 0 : 1;
    gBytes.push_back(b);
}

void resetSpi() {
    gBytes.clear();
    gOpenTransactions = 0;
    gTransactions = 0;
    gBytesOutsideTransaction = 0;
}

} // namespace

void SPIClass::begin(int8_t, int8_t, int8_t, int8_t) {}
void SPIClass::beginTransaction(SPISettings) {
    ++gOpenTransactions;
    ++gTransactions;
}
void SPIClass::endTransaction() { --gOpenTransactions; }
uint8_t SPIClass::transfer(uint8_t data) {
    record(data);
    return 0;
}
void SPIClass::writePixels(const void* data, uint32_t size) {
    const fl::u8* bytes = static_cast<const fl::u8*>(data);
    for (uint32_t i = 0; i < size; ++i) {
        record(bytes[i]);
    }
}

namespace fl {

constexpr u8 kData = 11;
constexpr u8 kClock = 13;
template <> class FastPin<kData> {
  public:
    static constexpr bool validpin() { return true; }
};
template <> class FastPin<kClock> {
  public:
    static constexpr bool validpin() { return true; }
};

using Output = ESP32SPIOutput<kData, kClock, 1000000>;

template <typename T>
auto canWriteBit(int) -> decltype(fl::declval<T&>().template writeBit<0>(fl::u8(1)), fl::true_type{});
template <typename T> fl::false_type canWriteBit(...);

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("a single-bit write is a compile error, not a whole byte") {
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

} // namespace

FL_TEST_CASE("byte-only pixels still go out through the class's own writer") {
    resetSpi();
    Output output;
    CRGB leds[2] = {CRGB(1, 2, 3), CRGB(4, 5, 6)};
    PixelController<RGB> pixels(leds, 2, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    output.writePixels<0, DATA_NOP, RGB>(pixels, nullptr);
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
    resetSpi();
    Output output;
    output.select();
    output.writeByte(0xA5);
    output.writeWord(0x1234);
    output.release();
    FL_REQUIRE_EQ(gBytes.size(), 3U);
    FL_CHECK_EQ(gBytes[0], 0xA5);
    FL_CHECK_EQ(gBytes[1], 0x12);
    FL_CHECK_EQ(gBytes[2], 0x34);
    FL_CHECK_EQ(gOpenTransactions, 0);
}

} // FL_TEST_FILE

} // namespace fl
