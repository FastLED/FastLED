#pragma once

// Per-chip 5-bit field semantics for the colour-managed HD path (P8, B1,
// #4042).
//
// APA102-class parts drive their 5-bit field as a secondary slow PWM stage,
// so a joint code/field solve may use it for low-light resolution, bounded
// below by a flicker floor. SK9822's field is a current gain whose effect on
// chromaticity is not characterised, so it is held fixed. A chip whose
// semantics are not known gets the SK9822-conservative treatment. A bound
// profile's `five_bit_semantics`, when it says anything, overrides the chip's
// default.

#include "fl/chipsets/spi_chipsets.h"
#include "fl/gfx/color_profile.h"  // FiveBitSemantics
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// The 5-bit semantics for `chip`, with `profile` overriding the chip's
/// default when it is not `NotApplicable`. Non-HD chips have no field solve
/// and are `NotApplicable` whatever the profile says.
FiveBitSemantics fiveBitSemanticsFor(SpiChipset chip,
                                     FiveBitSemantics profile) FL_NO_EXCEPT;

/// The lowest field the managed HD path may choose: the configured flicker
/// floor for a secondary slow-PWM field, 31 (held fixed) for everything else.
u8 hdMinimumField(FiveBitSemantics semantics, u8 floor_setting) FL_NO_EXCEPT;

namespace detail {
/// Backing store for `CFastLED::setHdFieldFloor()`. Default 31: the field is
/// held fixed, which cannot add flicker, until a sketch lowers it.
u8& hdFieldFloor() FL_NO_EXCEPT;
}  // namespace detail

}  // namespace fl
