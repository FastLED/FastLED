#pragma once

/// @file fl/system/pin_probe.h
/// Platform-neutral "may a pin probe touch this pin?" API.
///
/// A pin probe (e.g. AutoResearch's jumper discovery) drives a pin OUTPUT
/// LOW/HIGH, reads others with INPUT_PULLUP, and leaves them in INPUT. On
/// some pins that resets or locks up the board, or kills the host link:
/// SPI flash/PSRAM pads, flash power, the console UART and native USB.
/// The platform owns that list; callers only ask.
///
/// Platforms without such pins report none: every pin is safe and drivable.

#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"

namespace fl {

/// Bit for `pin` in a 64-bit pin mask; 0 when `pin` is outside [0, 64).
constexpr u64 pinMaskBit(int pin) FL_NO_EXCEPT {
    return (pin >= 0 && pin < 64) ? (u64(1) << pin) : u64(0);
}

/// Why a probe must not touch `pin` at all (no pinMode, read or write), or
/// nullptr when it may. Reasons: "not-a-gpio", "reserved-by-FastLED",
/// "console-link", "flash-or-psram".
const char* pinProbeSkipReason(int pin) FL_NO_EXCEPT;

/// Why a probe may read `pin` but must not drive it, or nullptr when it may
/// drive it. Reasons: "input-only", "not-output-capable".
const char* pinProbeDriveSkipReason(int pin) FL_NO_EXCEPT;

/// True when `pinProbeSkipReason(pin)` is non-null.
bool isPinUnsafeToProbe(int pin) FL_NO_EXCEPT;

/// Mask of pins 0..63 that `isPinUnsafeToProbe` rejects (0 = none).
u64 pinUnsafeForProbeMask() FL_NO_EXCEPT;

}  // namespace fl
