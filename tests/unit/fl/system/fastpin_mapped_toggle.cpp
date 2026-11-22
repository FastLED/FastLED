// Unit tests for mapped forced-software pin register access.
#include "led_sysdefs.h"
#include "fl/system/pin.h"

static volatile RwReg outputRegister = 0;
static volatile RoReg inputRegister = 0;

#undef FASTLED_NO_PINMAP
#define FASTLED_FORCE_SOFTWARE_PINS
#define digitalPinToBitMask(pin) (1U << 3)
#define digitalPinToPort(pin) (pin)
#define portOutputRegister(port) (&outputRegister)
#define portInputRegister(port) (&inputRegister)
#define FastPin MappedTestFastPin
#include "fl/system/fastpin_base.h"
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
FL_TEST_CASE("Mapped forced pin toggles output latch, not input status") {
    outputRegister = 0x81;
    inputRegister = 0x20;
    fl::MappedTestFastPin<3>::setOutput();
    fl::MappedTestFastPin<3>::toggle();
    FL_CHECK_EQ(outputRegister, 0x89U);
    FL_CHECK_EQ(inputRegister, 0x20U);
    fl::MappedTestFastPin<3>::toggle();
    FL_CHECK_EQ(outputRegister, 0x81U);
    FL_CHECK_EQ(inputRegister, 0x20U);
}
}
