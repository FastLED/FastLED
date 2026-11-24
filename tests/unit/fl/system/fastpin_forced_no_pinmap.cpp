// Unit test for forced software pins without a register map.
// FastPin is renamed so this file gets the production forced selection from
// fastpin_base.h instead of the stub's own FastPin specializations.
#include "led_sysdefs.h"
#include "fl/system/pin.h"

#define FASTLED_FORCE_SOFTWARE_PINS
#ifndef FASTLED_NO_PINMAP
#define FASTLED_NO_PINMAP
#endif
#define FastPin NoMapTestFastPin
#include "fl/system/fastpin_base.h"
#include "test.h"
#include "FastLED.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"
#undef FastPin
#undef FASTLED_FORCE_SOFTWARE_PINS

#include "fl/stl/vector.h"
#include "platforms/stub/stub_gpio.h"

using namespace fl;

namespace {
FL_TEST_CASE("Forced no-map pin uses stub GPIO for toggle") {
    constexpr u8 pin = 240;
    FL_CHECK_TRUE(isNoPinMapPin(static_cast<const fl::NoMapTestFastPin<pin>*>(nullptr)));
    FL_CHECK_EQ(fl::NoMapTestFastPin<pin>::port(), nullptr);
    fl::NoMapTestFastPin<pin>::setOutput();
    fl::NoMapTestFastPin<pin>::lo();
    fl::NoMapTestFastPin<pin>::toggle();
    FL_CHECK_EQ(fl::digitalRead(pin), PinValue::High);
    fl::NoMapTestFastPin<pin>::toggle();
    FL_CHECK_EQ(fl::digitalRead(pin), PinValue::Low);
}

// The SPI writer was included while FastPin was renamed, so it runs on the
// production forced class. A forced pin that dereferences its never-set null
// port (#5056) crashes here instead of producing GPIO traffic.
constexpr u8 kData = 242;
constexpr u8 kClock = 243;
using Output = GenericSoftwareSPIOutput<kData, kClock, 2>;

struct Trace {
    fl::vector<u8> bits;
    Trace() {
        fl::stub::setPinEdgeCallback(kClock, [this](bool high, u32) FL_NO_EXCEPT {
            if (high) {
                bits.push_back(fl::digitalRead(kData) == PinValue::High ? 1 : 0);
            }
        });
    }
    ~Trace() { fl::stub::clearPinEdgeCallback(kClock); }
};

void checkBytes(const Trace& trace, const fl::vector<u8>& bytes) {
    FL_REQUIRE_EQ(trace.bits.size(), bytes.size() * 8);
    for (fl::size i = 0; i < trace.bits.size(); ++i) {
        FL_CHECK_EQ(trace.bits[i], static_cast<u8>((bytes[i / 8] >> (7 - (i % 8))) & 1));
    }
    FL_CHECK_EQ(fl::digitalRead(kClock), PinValue::Low);
}

FL_TEST_CASE("Forced no-map pins drive software SPI through digitalWrite") {
    FL_CHECK_TRUE(isNoPinMapPin(static_cast<const fl::NoMapTestFastPin<kClock>*>(nullptr)));
    Output spi;
    spi.init();
    {
        Trace trace;
        spi.writeByte(0xA5);
        checkBytes(trace, {0xA5});
    }
    {
        Trace trace;
        u8 data[] = {0x00, 0xFF, 0x81};
        spi.writeBytes(data, 3);
        checkBytes(trace, {0x00, 0xFF, 0x81});
    }
    {
        Trace trace;
        spi.writeBytesValueRaw(0x3C, 2);
        checkBytes(trace, {0x3C, 0x3C});
    }
    {
        Trace trace;
        CRGB leds[1] = {CRGB(0x12, 0x34, 0x56)};
        PixelController<RGB> pixels(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
        spi.template writePixels<0, DATA_NOP, RGB>(pixels);
        checkBytes(trace, {0x12, 0x34, 0x56});
    }
}
} // namespace
