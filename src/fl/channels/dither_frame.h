#pragma once

/// @file fl/channels/dither_frame.h
/// Shared temporal-dither phase tracking.

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/compiler_control.h"

namespace fl {
namespace detail {

extern u8 gDitherFrame;

/// Return the temporal-dither phase shared by all controllers in this frame.
u8 ditherFrame() FL_NO_EXCEPT;

/// Advance temporal dithering once per logical frame, not once per controller.
FL_ALWAYS_INLINE void advanceDitherFrame() FL_NO_EXCEPT {
    ++gDitherFrame;
}

} // namespace detail
} // namespace fl
