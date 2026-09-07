// ok no header - implementation for fl/gfx/flux_scalar.h

#include "fl/gfx/flux_scalar.h"

namespace fl {

namespace {
constexpr i32 kUnityQ16 = 65536;
}  // namespace

FluxScalar::FluxScalar(i32 raw) FL_NO_EXCEPT : mRawQ16(raw) {}

i32 FluxScalar::rawQ16() const FL_NO_EXCEPT { return mRawQ16; }

FluxScalar FluxScalar::unity() FL_NO_EXCEPT { return FluxScalar(kUnityQ16); }

FluxScalar FluxScalar::fromBrightness(u8 brightness) FL_NO_EXCEPT {
    // Round to nearest so mid brightnesses are not biased low. 255 lands on
    // exactly 65536: 255 * 65536 / 255.
    const u32 raw = (static_cast<u32>(brightness) * 65536u + 127u) / 255u;
    return FluxScalar(static_cast<i32>(raw));
}

FluxScalar FluxScalar::fromRawQ16(i32 raw) FL_NO_EXCEPT {
    if (raw < 0) {
        raw = 0;
    } else if (raw > kUnityQ16) {
        raw = kUnityQ16;
    }
    return FluxScalar(raw);
}

FluxScalar FluxScalar::composedWith(FluxScalar other) const FL_NO_EXCEPT {
    // Q16 x Q16 is Q32; the i64 accumulator is required because two
    // near-unity scalars already exceed i32.
    const i64 product = static_cast<i64>(mRawQ16) * static_cast<i64>(other.mRawQ16);
    return FluxScalar(static_cast<i32>((product + 32768) >> 16));
}

void applyFluxScalar(FluxScalar scalar, span<i32> drives) FL_NO_EXCEPT {
    const i64 raw = static_cast<i64>(scalar.rawQ16());
    if (raw == kUnityQ16) {
        // Pure short-circuit: the general path below is already bit-exact at
        // unity, since drive * 65536 + 32768 >> 16 == drive. This only avoids
        // walking the buffer on the default full-brightness path. Exactness is
        // pinned by a test, not by this branch -- verified by removing it.
        return;
    }
    for (size i = 0; i < drives.size(); ++i) {
        const i64 scaled = static_cast<i64>(drives[i]) * raw;
        // Round half away from zero so negative signed-wide intermediates
        // are treated symmetrically.
        drives[i] = static_cast<i32>(scaled >= 0 ? (scaled + 32768) >> 16
                                                 : -((-scaled + 32768) >> 16));
    }
}

}  // namespace fl
