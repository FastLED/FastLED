// Unit test for forced software pins without a register map.
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
#undef FastPin
#undef FASTLED_FORCE_SOFTWARE_PINS

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
} // namespace
