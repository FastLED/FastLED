/// @brief Optional GPIO operations for the existing power indicator.

#include "fl/system/gpio/power_indicator.h"
#include "fl/stl/noexcept.h"
#include "fastpin.h"
#include "power_mgt.h"

namespace {
void writePowerIndicator(fl::u8 pin, bool overLimit) FL_NO_EXCEPT {
    if (overLimit) {
        Pin(pin).hi();
    } else {
        Pin(pin).lo();
    }
}
} // namespace

void set_max_power_indicator_LED( fl::u8 pinNumber)
{
    fl::detail::powerIndicatorPin = pinNumber;
    fl::detail::powerIndicatorWrite = pinNumber ? &writePowerIndicator : nullptr;
}
