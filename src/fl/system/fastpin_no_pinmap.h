#pragma once

// IWYU pragma: private

/// @file fl/system/fastpin_no_pinmap.h
/// Forced software pin for Arduino cores with no register pin map. It keeps
/// FastPin's interface but has no port: a "port value" is just this pin's level.

#include "fl/stl/compiler_control.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/system/pin.h"

namespace fl {

template <u8 PIN> class NoPinMapPin {
  public:
    typedef volatile u32* port_ptr_t;
    typedef u32 port_t;

    // Whether the pin exists is the core's call; there is no table to check here.
    static constexpr bool validpin() FL_NO_EXCEPT { return true; }

    static void setOutput() FL_NO_EXCEPT { (fl::pinMode)(PIN, PinMode::Output); }
    static void setInput() FL_NO_EXCEPT { (fl::pinMode)(PIN, PinMode::Input); }

    static void hi() FL_NO_EXCEPT {
        sHigh = true;
        (fl::digitalWrite)(PIN, PinValue::High);
    }
    static void lo() FL_NO_EXCEPT {
        sHigh = false;
        (fl::digitalWrite)(PIN, PinValue::Low);
    }
    // Uses the last level written: not every core reads back an output pin.
    static void toggle() FL_NO_EXCEPT { sHigh ? lo() : hi(); }
    static void strobe() FL_NO_EXCEPT {
        toggle();
        toggle();
    }

    static void hi(port_ptr_t) FL_NO_EXCEPT { hi(); }
    static void lo(port_ptr_t) FL_NO_EXCEPT { lo(); }
    static void set(port_t val) FL_NO_EXCEPT { val ? hi() : lo(); }
    static void fastset(port_ptr_t, port_t val) FL_NO_EXCEPT { set(val); }

    static port_t hival() FL_NO_EXCEPT { return 1; }
    static port_t loval() FL_NO_EXCEPT { return 0; }
    static port_ptr_t port() FL_NO_EXCEPT { return nullptr; }
    static port_t mask() FL_NO_EXCEPT { return 1; }

  private:
    static bool sHigh;
};

template <u8 PIN> bool NoPinMapPin<PIN>::sHigh = false;

template <u8 PIN> constexpr bool isNoPinMapPin(const NoPinMapPin<PIN>*) FL_NO_EXCEPT { return true; }
constexpr bool isNoPinMapPin(const void*) FL_NO_EXCEPT { return false; }

} // namespace fl
