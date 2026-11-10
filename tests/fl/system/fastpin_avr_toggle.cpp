// Unit test for the AVR mapped forced-software pin toggle.
#include "led_sysdefs.h"
#include "fl/system/pin.h"
// Freeze native alignment before simulating AVR so the shared test harness ABI matches.
#include "fl/stl/compiler_control.h"

static volatile RwReg outputRegister = 0;
static volatile RoReg inputRegister = 0;

#undef FASTLED_NO_PINMAP
#define FASTLED_FORCE_SOFTWARE_PINS
#define FL_IS_AVR
#define digitalPinToBitMask(pin) (1U << 3)
#define digitalPinToPort(pin) (pin)
#define portOutputRegister(port) (&outputRegister)
#define portInputRegister(port) (&inputRegister)
#define FastPin AvrMappedTestFastPin
#include "fl/system/fastpin_base.h"
#undef FL_IS_AVR
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
FL_TEST_CASE("AVR mapped forced pin writes its mask to the input register") {
    outputRegister = 0x81;
    inputRegister = 0x20;
    fl::AvrMappedTestFastPin<3>::setOutput();
    fl::AvrMappedTestFastPin<3>::toggle();
    FL_CHECK_EQ(inputRegister, 0x08U);
    FL_CHECK_EQ(outputRegister, 0x81U);
}
} // namespace
