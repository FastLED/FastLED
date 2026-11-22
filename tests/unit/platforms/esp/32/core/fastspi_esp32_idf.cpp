/// @file fastspi_esp32_idf.cpp
/// @brief Runs the real native-IDF ESP32SPIOutput and SM16716Controller against a
/// fake SPI master that records what each transaction asks the peripheral to send:
/// its bit length and only those bits, MSB first.

#include "test.h"

#include "FastLED.h"
#include "fastspi_types.h"
#include "fl/stl/int.h"
#include "fl/stl/vector.h"
#include "platforms/esp/32/core/fastspi_esp32_idf.h"

namespace {

fl::vector<fl::u8> gWire;
fl::vector<size_t> gLengths;
fl::vector<spi_transaction_t*> gQueued;

void record(const spi_transaction_t* t) {
    gLengths.push_back(t->length);
    const fl::u8* data = static_cast<const fl::u8*>(t->tx_buffer);
    for (size_t bit = 0; bit < t->length; ++bit) {
        gWire.push_back((data[bit / 8] >> (7 - (bit % 8))) & 1);
    }
}

void clearWire() {
    gWire.clear();
    gLengths.clear();
}

} // namespace

esp_err_t spi_device_queue_trans(spi_device_handle_t, spi_transaction_t* t, uint32_t) {
    record(t); // Copied at queue time: the driver owns the buffer until the result.
    gQueued.push_back(t);
    return ESP_OK;
}
esp_err_t spi_device_get_trans_result(spi_device_handle_t, spi_transaction_t** t, uint32_t) {
    *t = gQueued.empty() ? nullptr : gQueued[0];
    if (!gQueued.empty()) {
        gQueued.erase(gQueued.begin());
    }
    return ESP_OK;
}
esp_err_t spi_device_polling_transmit(spi_device_handle_t, spi_transaction_t* t) {
    record(t);
    return ESP_OK;
}

namespace fl {

constexpr u32 kSpeed = 1000000;
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

using Output = ESP32SPIOutput<kData, kClock, kSpeed>;

// The ESP32 dispatch selects this backend; the host's SPIOutput name is redirected.
template <> class SPIOutput<kData, kClock, kSpeed> : public Output {};

class Controller : public SM16716Controller<kData, kClock, RGB, kSpeed> {
  public:
    using SM16716Controller<kData, kClock, RGB, kSpeed>::showPixels;
};

FL_TEST_FILE(FL_FILEPATH) {

namespace {

void expectByte(fl::size at, u8 value) {
    FL_REQUIRE(at + 8 <= gWire.size());
    for (int i = 0; i < 8; ++i) {
        FL_CHECK_EQ(gWire[at + i], static_cast<u8>((value >> (7 - i)) & 1));
    }
}

template <u8 BIT> void checkEveryValue(Output& output) {
    for (int value = 0; value < 256; ++value) {
        clearWire();
        output.select();
        output.template writeBit<BIT>(static_cast<u8>(value));
        output.release();
        FL_REQUIRE_EQ(gLengths.size(), 1U);
        FL_REQUIRE_EQ(gLengths[0], 1U);
        FL_REQUIRE_EQ(gWire[0], static_cast<u8>((value >> BIT) & 1));
    }
}

} // namespace

FL_TEST_CASE("writeBit requests one bit: every bit, every value") {
    Output output;
    output.init();
    checkEveryValue<0>(output);
    checkEveryValue<1>(output);
    checkEveryValue<2>(output);
    checkEveryValue<3>(output);
    checkEveryValue<4>(output);
    checkEveryValue<5>(output);
    checkEveryValue<6>(output);
    checkEveryValue<7>(output);
}

FL_TEST_CASE("bit, byte, bit, adjacent bits, then byte stay in order across byte edges") {
    Output output;
    output.init();
    clearWire();
    output.select();
    output.writeBit<0>(1);
    output.writeByte(0xA5);
    output.writeBit<0>(0);
    output.writeBit<7>(0x80);
    output.writeByte(0x3C);
    output.release();
    FL_REQUIRE_EQ(gLengths.size(), 1U);
    FL_REQUIRE_EQ(gLengths[0], 19U);
    FL_CHECK_EQ(gWire[0], 1);
    expectByte(1, 0xA5);
    FL_CHECK_EQ(gWire[9], 0);
    FL_CHECK_EQ(gWire[10], 1);
    expectByte(11, 0x3C);
}

FL_TEST_CASE("a frame after a partial byte starts aligned, and bytes-only frames are unchanged") {
    Output output;
    output.init();
    clearWire();
    output.select();
    output.writeBit<0>(1);
    output.release();
    output.select();
    output.writeByte(0xC3);
    output.writeByte(0x18);
    output.release();
    FL_REQUIRE_EQ(gLengths.size(), 2U);
    FL_CHECK_EQ(gLengths[0], 1U);
    FL_CHECK_EQ(gLengths[1], 16U);
    expectByte(1, 0xC3);
    expectByte(9, 0x18);
}

FL_TEST_CASE("select abandons a partial byte left by a frame that was never released") {
    Output output;
    output.init();
    clearWire();
    output.select();
    output.writeBit<0>(1);
    output.select();
    output.writeByte(0xA5);
    output.release();
    FL_REQUIRE_EQ(gLengths.size(), 1U);
    FL_CHECK_EQ(gLengths[0], 8U);
    expectByte(0, 0xA5);
}

FL_TEST_CASE("a full buffer drops bits rather than writing past it") {
    Output output;
    output.init();
    clearWire();
    output.select();
    for (int i = 0; i < FL_ESP32_SPI_DMA_BUFFER_BYTES; ++i) {
        output.writeByte(0xFF);
    }
    output.writeBit<0>(1); // No room for a new byte.
    output.release();
    FL_REQUIRE_EQ(gLengths.size(), 1U);
    FL_CHECK_EQ(gLengths[0], static_cast<size_t>(FL_ESP32_SPI_DMA_BUFFER_BYTES) * 8);

    clearWire();
    output.select();
    for (int i = 0; i < FL_ESP32_SPI_DMA_BUFFER_BYTES - 1; ++i) {
        output.writeByte(0x00);
    }
    output.writeBit<0>(1);
    output.writeByte(0xFF); // Seven bits fit; the byte's remainder has nowhere to go.
    output.release();
    FL_REQUIRE_EQ(gLengths.size(), 1U);
    FL_CHECK_EQ(gLengths[0], static_cast<size_t>(FL_ESP32_SPI_DMA_BUFFER_BYTES) * 8);
    const fl::size last = gWire.size() - 8;
    for (fl::size i = last; i < gWire.size(); ++i) {
        FL_CHECK_EQ(gWire[i], 1); // The lone bit, then the byte's first seven bits.
    }

    clearWire(); // The dropped remainder must not misalign the next frame.
    output.select();
    output.writeByte(0xA5);
    output.release();
    FL_REQUIRE_EQ(gLengths.size(), 1U);
    FL_CHECK_EQ(gLengths[0], 8U);
    expectByte(0, 0xA5);
}

FL_TEST_CASE("the real SM16716 controller: 25 bits per pixel, 50-bit header, across frames") {
    Controller controller;
    controller.init();
    CRGB leds[3] = {CRGB(0x81, 0x42, 0x24), CRGB(0xFF, 0x00, 0x18), CRGB(0x00, 0xFF, 0x7E)};
    for (int frame = 0; frame < 3; ++frame) {
        clearWire();
        PixelController<RGB> pixels(leds, 3, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
        controller.showPixels(pixels);
        FL_REQUIRE_EQ(gWire.size(), 3U * 25U + 50U);
        for (int p = 0; p < 3; ++p) {
            const fl::size at = static_cast<fl::size>(p) * 25;
            FL_CHECK_EQ(gWire[at], 1);
            expectByte(at + 1, leds[p].r);
            expectByte(at + 9, leds[p].g);
            expectByte(at + 17, leds[p].b);
        }
        for (fl::size i = 75; i < gWire.size(); ++i) {
            FL_CHECK_EQ(gWire[i], 0);
        }
    }
}

} // FL_TEST_FILE

} // namespace fl
