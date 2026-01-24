/// @file ucs7604.cpp
/// @brief UCS7604 chipset global state

#include "ucs7604.h"

// Define FL_UCS7604_BRIGHTNESS if not already defined
#ifndef FL_UCS7604_BRIGHTNESS
#define FL_UCS7604_BRIGHTNESS 0x0F
#endif

namespace fl {
namespace ucs7604 {

namespace detail {
    // Global current control value for UCS7604
    // Defaults to FL_UCS7604_BRIGHTNESS (0x0F if not defined)
    // Static to hide from external linkage
    static CurrentControl g_current(FL_UCS7604_BRIGHTNESS);
}  // namespace detail

void set_brightness(CurrentControl current) {
    detail::g_current = current;
}

CurrentControl brightness() {
    return detail::g_current;
}

}  // namespace ucs7604
}  // namespace fl
