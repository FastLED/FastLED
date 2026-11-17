#pragma once

// IWYU pragma: private

/// @file platforms/arm/lpc/pin_lpc.hpp
/// Native runtime pin backend for LPC845 / LPC804 (#5260).
///
/// Runtime pin N is PIO0_N, the same port-0 mapping `fastpin_arm_lpc.h` uses
/// (`_ARMPIN<PIN, 1u << PIN>`), and the digital ops reuse its `lpc_detail`
/// GPIO accessors. This backend must never call `FastPin<N>`: under
/// FASTLED_FORCE_SOFTWARE_PINS that is `NoPinMapPin`, which calls back into
/// `fl::digitalWrite` and would recurse.
///
/// Registers come from the vendor CMSIS PAL (<LPC845.h> / <LPC804.h>) that
/// `led_sysdefs_arm_lpc.h` includes:
///   - direction / level: GPIO->DIRSET[0] / DIRCLR[0] / SET[0] / CLR[0] / PIN[0]
///   - pulls: IOCON->PIO[IOCON_INDEX_PIO0_n], MODE field. The IOCON array is
///     not in pin order (IOCON_INDEX_PIO0_0 == 17), so pin N is looked up in
///     IoconTable, built from the vendor IOCON_INDEX_PIO0_n macros.
///
/// Pin function selection is the switch matrix (SWM). pinMode() does not touch
/// it: clearing PINENABLE0 could cut off SWD/RESET. A pin that SWM has routed
/// to USART/SPI/SCT, or whose fixed function is enabled, is not under GPIO
/// control, same as with FastPin. pinMode(Output) leaves the IOCON pull alone.
///
/// pinMode(Input) selects no pull (high impedance), per PinMode::Input.
///
/// LPC845 PIO0_10 / PIO0_11 are true open-drain I2C pads: no pull resistors,
/// and IOCON bits 5:0 are reserved there (UM11029 Tables 207/208), so pinMode
/// never writes MODE for them. Driving them HIGH needs an external pull-up
/// (LPC84x datasheet, Table 4 note [6]). LPC804's PIO0_10/11 are normal pads.
///
/// Pins outside 0..31, and pads the package lacks (PIO0_6 and PIO0_31 on
/// LPC804), are ignored; digitalRead() returns Low for them. The FastPin grid
/// still declares those two LPC804 pins in native and forced mode alike.
///
/// Analog and PWM are stubs, like the LPC8xx Arduino core's own.

#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/is_lpc.h"

#if defined(FL_IS_ARM_LPC_845) || defined(FL_IS_ARM_LPC_804)

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"
#include "fl/system/pin.h"
#include "platforms/arm/lpc/led_sysdefs_arm_lpc.h"  // vendor CMSIS PAL: GPIO, IOCON, SYSCON
#include "platforms/arm/lpc/fastpin_arm_lpc.h"  // lpc_detail GPIO accessors shared with FastPin

// LPC804 has no PIO0_6 or PIO0_31 pad. Every other IOCON_INDEX_PIO0_n is
// required, so a vendor header missing one fails to compile.
#ifdef IOCON_INDEX_PIO0_6
#define FL_LPC_IOCON_PIO0_6 IOCON_INDEX_PIO0_6
#else
#define FL_LPC_IOCON_PIO0_6 0xFF
#endif
#ifdef IOCON_INDEX_PIO0_31
#define FL_LPC_IOCON_PIO0_31 IOCON_INDEX_PIO0_31
#else
#define FL_LPC_IOCON_PIO0_31 0xFF
#endif

namespace fl {
namespace platforms {
namespace lpc_pin_detail {

constexpr int kPinCount = 32;  // PIO0_0..PIO0_31, the FastPin grid
constexpr u8 kNoIocon = 0xFF;

// A class-template static member has one definition program-wide, unlike a
// namespace-scope constexpr array, which every including TU would copy.
template <typename = void> struct IoconTable {
    /// IOCON->PIO[] slot for PIO0_n, or kNoIocon if the pad does not exist.
    static constexpr u8 kIndex[kPinCount] = {
        IOCON_INDEX_PIO0_0,  IOCON_INDEX_PIO0_1,  IOCON_INDEX_PIO0_2,  IOCON_INDEX_PIO0_3,
        IOCON_INDEX_PIO0_4,  IOCON_INDEX_PIO0_5,  FL_LPC_IOCON_PIO0_6, IOCON_INDEX_PIO0_7,
        IOCON_INDEX_PIO0_8,  IOCON_INDEX_PIO0_9,  IOCON_INDEX_PIO0_10, IOCON_INDEX_PIO0_11,
        IOCON_INDEX_PIO0_12, IOCON_INDEX_PIO0_13, IOCON_INDEX_PIO0_14, IOCON_INDEX_PIO0_15,
        IOCON_INDEX_PIO0_16, IOCON_INDEX_PIO0_17, IOCON_INDEX_PIO0_18, IOCON_INDEX_PIO0_19,
        IOCON_INDEX_PIO0_20, IOCON_INDEX_PIO0_21, IOCON_INDEX_PIO0_22, IOCON_INDEX_PIO0_23,
        IOCON_INDEX_PIO0_24, IOCON_INDEX_PIO0_25, IOCON_INDEX_PIO0_26, IOCON_INDEX_PIO0_27,
        IOCON_INDEX_PIO0_28, IOCON_INDEX_PIO0_29, IOCON_INDEX_PIO0_30, FL_LPC_IOCON_PIO0_31,
    };
};
template <typename T> constexpr u8 IoconTable<T>::kIndex[kPinCount];

using Iocon = IoconTable<>;

constexpr u32 kIoconSlots = sizeof(IOCON_Type::PIO) / sizeof(u32);

// C++11 constexpr: every present entry must index inside IOCON->PIO[].
constexpr bool ioconTableFits(int i) {
    return i >= kPinCount ||
           ((Iocon::kIndex[i] == kNoIocon || Iocon::kIndex[i] < kIoconSlots) &&
            ioconTableFits(i + 1));
}
FL_STATIC_ASSERT(ioconTableFits(0), "IOCON_INDEX_PIO0_n exceeds IOCON->PIO[]");

// IOCON MODE field values (vendor IOCON_PIO_MODE documentation).
constexpr u32 kModeInactive = 0;
constexpr u32 kModePullDown = 1;
constexpr u32 kModePullUp = 2;

inline bool validPin(int pin) FL_NO_EXCEPT {
    return pin >= 0 && pin < kPinCount && Iocon::kIndex[pin] != kNoIocon;
}

/// False for the LPC845 true open-drain pads, whose IOCON has no MODE field.
inline bool hasPullResistors(int pin) FL_NO_EXCEPT {
#if defined(FL_IS_ARM_LPC_845)
    return pin != 10 && pin != 11;
#else
    (void)pin;
    return true;
#endif
}

inline u32 pinMask(int pin) FL_NO_EXCEPT { return 1u << static_cast<u32>(pin); }

inline void setPull(int pin, u32 mode) FL_NO_EXCEPT {
    if (!hasPullResistors(pin)) {
        return;
    }
    volatile u32& reg = IOCON->PIO[Iocon::kIndex[pin]];
    reg = (reg & ~static_cast<u32>(IOCON_PIO_MODE_MASK)) | IOCON_PIO_MODE(mode);
}

}  // namespace lpc_pin_detail

inline void (pinMode)(int pin, PinMode mode) FL_NO_EXCEPT {
    if (!lpc_pin_detail::validPin(pin)) {
        return;
    }
    // The core enables these at boot; repeating the OR is idempotent.
    SYSCON->SYSAHBCLKCTRL0 |=
        SYSCON_SYSAHBCLKCTRL0_GPIO0_MASK | SYSCON_SYSAHBCLKCTRL0_IOCON_MASK;
    const u32 mask = lpc_pin_detail::pinMask(pin);
    switch (mode) {
    case PinMode::Output:
        *fl::lpc_detail::lpc_gpio_dirset() = mask;
        return;
    case PinMode::InputPullup:
        lpc_pin_detail::setPull(pin, lpc_pin_detail::kModePullUp);
        break;
    case PinMode::InputPulldown:
        lpc_pin_detail::setPull(pin, lpc_pin_detail::kModePullDown);
        break;
    case PinMode::Input:
        lpc_pin_detail::setPull(pin, lpc_pin_detail::kModeInactive);
        break;
    }
    *fl::lpc_detail::lpc_gpio_dirclr() = mask;
}

inline void (digitalWrite)(int pin, PinValue val) FL_NO_EXCEPT {
    if (!lpc_pin_detail::validPin(pin)) {
        return;
    }
    const u32 mask = lpc_pin_detail::pinMask(pin);
    if (val == PinValue::High) {
        *fl::lpc_detail::lpc_gpio_set() = mask;
    } else {
        *fl::lpc_detail::lpc_gpio_clr() = mask;
    }
}

inline PinValue (digitalRead)(int pin) FL_NO_EXCEPT {
    if (!lpc_pin_detail::validPin(pin)) {
        return PinValue::Low;
    }
    return (*fl::lpc_detail::lpc_gpio_pin() & lpc_pin_detail::pinMask(pin)) != 0
               ? PinValue::High
               : PinValue::Low;
}

// Analog / PWM: no ADC or PWM backend yet (the LPC8xx core's are stubs too).

inline u16 (analogRead)(int /*pin*/) FL_NO_EXCEPT { return 0; }

inline void (analogWrite)(int /*pin*/, u16 /*val*/) FL_NO_EXCEPT {}

inline void setPwm16(int /*pin*/, u16 /*val*/) FL_NO_EXCEPT {}

inline void setAdcRange(AdcRange /*range*/) FL_NO_EXCEPT {}

inline bool needsPwmIsrFallback(int /*pin*/, u32 /*frequency_hz*/) FL_NO_EXCEPT {
    return true;
}

inline int setPwmFrequencyNative(int /*pin*/, u32 /*frequency_hz*/) FL_NO_EXCEPT {
    return -4;
}

inline u32 getPwmFrequencyNative(int /*pin*/) FL_NO_EXCEPT { return 0; }

}  // namespace platforms
}  // namespace fl

#undef FL_LPC_IOCON_PIO0_6
#undef FL_LPC_IOCON_PIO0_31

#endif  // FL_IS_ARM_LPC_845 || FL_IS_ARM_LPC_804
