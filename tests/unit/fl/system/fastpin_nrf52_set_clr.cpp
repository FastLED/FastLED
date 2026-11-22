// Unit tests for the nRF52 forced-software pin: hi(), lo() and toggle() must
// drive one pin through OUTSET/OUTCLR and never store the whole OUT latch (#5396).
#include "led_sysdefs.h"
#include "fl/system/pin.h"
#include "fl/stl/int.h"

// Stand-in for the Nordic CMSIS GPIO block, in vendor register order. The fake
// does not fold OUTSET/OUTCLR into OUT, so any store to OUT stays visible.
struct NRF_GPIO_Type {
    volatile fl::u32 OUT, OUTSET, OUTCLR, IN;
};
static NRF_GPIO_Type gGpio;

#undef FASTLED_NO_PINMAP
#define FASTLED_FORCE_SOFTWARE_PINS
#define FL_IS_NRF52
#define digitalPinToBitMask(pin) (1U << 3)
#define digitalPinToPort(pin) (&gGpio)
#define portOutputRegister(port) (&(port)->OUT)
#define portInputRegister(port) (&(port)->IN)
#define FastPin Nrf52MappedTestFastPin
#include "fl/system/fastpin_base.h"
#undef FL_IS_NRF52
#include "test.h"
#include "FastLED.h"
#undef FastPin
#undef portInputRegister
#undef portOutputRegister
#undef digitalPinToPort
#undef digitalPinToBitMask
#undef FASTLED_FORCE_SOFTWARE_PINS

using namespace fl;

namespace {
using Pin = fl::Nrf52MappedTestFastPin<3>;
constexpr fl::u32 kMask = 0x08U;
// Another pin on the same port that an interrupt raised; a read-modify-write
// of OUT taken before that interrupt would write it back low.
constexpr fl::u32 kOther = 0x81U;

void resetGpio(fl::u32 out) {
    gGpio.OUT = out;
    gGpio.OUTSET = 0;
    gGpio.OUTCLR = 0;
    gGpio.IN = 0;
}

FL_TEST_CASE("nRF52 forced pin hi()/lo() use OUTSET/OUTCLR, not an OUT read-modify-write") {
    resetGpio(kOther);
    Pin::setOutput();
    Pin::hi();
    FL_CHECK_EQ(gGpio.OUTSET, kMask);
    FL_CHECK_EQ(gGpio.OUTCLR, 0U);
    FL_CHECK_EQ(gGpio.OUT, kOther);

    resetGpio(kOther | kMask);
    Pin::lo();
    FL_CHECK_EQ(gGpio.OUTCLR, kMask);
    FL_CHECK_EQ(gGpio.OUTSET, 0U);
    FL_CHECK_EQ(gGpio.OUT, kOther | kMask);
}

FL_TEST_CASE("nRF52 forced pin hi(port)/lo(port) use OUTSET/OUTCLR") {
    resetGpio(kOther);
    Pin::setOutput();
    Pin::hi(Pin::port());
    FL_CHECK_EQ(gGpio.OUTSET, kMask);
    FL_CHECK_EQ(gGpio.OUT, kOther);

    resetGpio(kOther | kMask);
    Pin::lo(Pin::port());
    FL_CHECK_EQ(gGpio.OUTCLR, kMask);
    FL_CHECK_EQ(gGpio.OUT, kOther | kMask);
}

FL_TEST_CASE("nRF52 forced pin toggle() reads OUT and writes only OUTSET/OUTCLR") {
    resetGpio(kOther);
    Pin::setOutput();
    Pin::toggle();
    FL_CHECK_EQ(gGpio.OUTSET, kMask);
    FL_CHECK_EQ(gGpio.OUTCLR, 0U);
    FL_CHECK_EQ(gGpio.OUT, kOther);

    resetGpio(kOther | kMask);
    Pin::toggle();
    FL_CHECK_EQ(gGpio.OUTCLR, kMask);
    FL_CHECK_EQ(gGpio.OUTSET, 0U);
    FL_CHECK_EQ(gGpio.OUT, kOther | kMask);
}
} // namespace
