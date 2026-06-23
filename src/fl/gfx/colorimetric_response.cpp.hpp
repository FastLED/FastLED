/// @file colorimetric_response.cpp.hpp
/// Reserved for the heavy implementations of the shared colorimetric
/// response layer (issue #3255).
///
/// PR 1 of #3255 only extracts the inline header helpers into
/// `colorimetric_response.h`; no out-of-line definitions live here yet.
/// PR 3 will land `solve_rgb_colorimetric(...)` and the per-pixel
/// `apply_rgb_colorimetric_impl(...)` adapter behind the same gate that
/// `rgbw_colorimetric.cpp.hpp` uses today, so the unity build picks the
/// file up consistently.

#include "fl/gfx/colorimetric_response.h"

#if FASTLED_RGBW_COLORIMETRIC

namespace fl {
namespace colorimetric_response {

// (Empty — implementations land in PR 3 of #3255.)

} // namespace colorimetric_response
} // namespace fl

#endif  // FASTLED_RGBW_COLORIMETRIC
