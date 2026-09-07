#pragma once

// Brightness x power as one linear amplitude stage (P6 C4, #4040).
//
// The pipeline has exactly one place where amplitude changes. Brightness and
// the power limiter both land here and compose multiplicatively; nothing
// downstream rescales channels.

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/span.h"

namespace fl {

/// A linear flux scalar in s16.16.
///
/// Deliberately a single scalar for every channel rather than a per-channel
/// array: a chromaticity-changing rescale cannot be expressed through this
/// type at all, which is how C4's "chromaticity preserved by construction"
/// is enforced rather than merely tested.
class FluxScalar {
  public:
    /// Identity. Drives pass through unchanged.
    static FluxScalar unity() FL_NO_EXCEPT { return FluxScalar(65536); }

    /// `setBrightness(b)` as the linear flux scalar b/255. 255 is exactly
    /// unity, so full brightness cannot dim the output by a rounding step.
    static FluxScalar fromBrightness(u8 brightness) FL_NO_EXCEPT;

    /// A limiter fraction already in s16.16, clamped to [0, 1].
    static FluxScalar fromRawQ16(i32 raw) FL_NO_EXCEPT;

    /// Multiplicative composition. Associative and commutative, so the order
    /// brightness and the limiter are combined cannot change the result.
    FluxScalar composedWith(FluxScalar other) const FL_NO_EXCEPT;

    i32 rawQ16() const FL_NO_EXCEPT { return mRawQ16; }

  private:
    explicit FluxScalar(i32 raw) FL_NO_EXCEPT : mRawQ16(raw) {}
    i32 mRawQ16;
};

/// Scale every drive by one scalar, in place.
void applyFluxScalar(FluxScalar scalar, span<i32> drives) FL_NO_EXCEPT;

}  // namespace fl
