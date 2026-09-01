#pragma once

/// @file fl/channels/dither_frame.h
/// Shared temporal-dither phase tracking.

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

struct DitherFrameState {
    u8 frame = 0;
};

/// Return the temporal-dither phase shared by all controllers in this frame.
u8 ditherFrame() FL_NO_EXCEPT;

/// Advance temporal dithering once per logical frame, not once per controller.
void advanceDitherFrame() FL_NO_EXCEPT;

} // namespace detail
} // namespace fl
