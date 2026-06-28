/// @file rgbw_colorimetric.h
/// Chromaticity-aware RGBW solvers — strict sub-gamut + wx_lp_legacy white
/// extraction + boosted overdrive + LUT + RGBCCT (issue #2545).
///
/// This header carries the public/internal declarations for the RGBW-specific
/// solver topology. The shared math primitives (CCT-to-xy, xy/XYZ conversion,
/// 3x3 invert, NNLS, Hermite basis, LUT quantization constants) now live in
/// `fl/gfx/colorimetric_response.h` and are re-exported into the
/// `fl::colorimetric_detail::` namespace below so existing callers
/// (`rgbw.cpp.hpp`, `rgbww.cpp.hpp`, tests using
/// `using namespace fl::colorimetric_detail`) keep compiling unchanged.
/// See #3255 for the extraction plan.
///
/// The heavier solver, LUT, and RGBCCT code lives in
/// rgbw_colorimetric.cpp.hpp behind the FASTLED_RGBW_COLORIMETRIC compile
/// gate.
///
/// Solver model in one paragraph:
///   * Input RGB is treated as linear-light source-gamut coordinates.
///   * A DiodeProfile supplies measured emitter xy + peak Y values.
///   * The source matrix maps input RGB to target CIE XYZ in the measured
///     device's absolute XYZ frame.
///   * Strict mode routes that target into a legal RGBW topology and solves a
///     small linear system.
///   * wx_lp_legacy solves a chromaticity-preserving maximum-W endpoint.
///   * boosted/overdrive intentionally trades chromaticity for more W/luminance.
///
/// Native-gamut invariants:
///   * Single-channel R/G/B inputs are exact identity.
///   * Native outer edges RG/RB/GB are topology-locked: no W and no inactive
///     third RGB primary may appear.
///   * Dual edges are still colorimetric solves, not raw 1:1 passthrough; the
///     measured emitter XYZ/Y values determine the active-channel ratio.
///   * Three-channel/interior inputs use chroma/value separation: solve the
///     full-chroma endpoint, then apply the original source value.

#pragma once

#include "fl/gfx/colorimetric_response.h"
#include "fl/gfx/rgbw.h"
#include "fl/math/math.h"
#include "fl/stl/stdint.h"
#include "fl/stl/unique_ptr.h"

namespace fl {
namespace colorimetric_detail {

// ===== Shared primitives — re-exported from colorimetric_response ===========
//
// The pure-math leaf helpers, the LUT quantization constants, and the
// `LutInterp` enum now live in `colorimetric_response::` (#3255). They are
// re-exported here so callers that reach into `colorimetric_detail::name`
// (and tests using `using namespace fl::colorimetric_detail`) keep
// compiling without churn. New code should prefer the
// `colorimetric_response::` namespace directly.

using colorimetric_response::cct_to_xy;            // ok bare using
using colorimetric_response::xyY_to_XYZ;           // ok bare using
using colorimetric_response::invert3x3;            // ok bare using
using colorimetric_response::matvec3;              // ok bare using
using colorimetric_response::barycentric_xy;       // ok bare using
using colorimetric_response::build_source_matrix;  // ok bare using
using colorimetric_response::quantize_u8;          // ok bare using
using colorimetric_response::quantize_lut_cell;    // ok bare using
using colorimetric_response::nnls3;                // ok bare using
using colorimetric_response::hermite_basis;        // ok bare using
using colorimetric_response::LutInterp;            // ok bare using

// `constexpr` variables cannot be re-exported via using-declarations in a
// way that preserves constant-expression status across all consumers, so
// shadow them here as initialized constexpr aliases pointing at the
// shared definitions. The values come from one place — the shared header —
// and any future change there flows through automatically.
constexpr i16 kLutQ = colorimetric_response::kLutQ;
constexpr int kLutStrideBilinear = colorimetric_response::kLutStrideBilinear;
constexpr int kLutStrideHermite = colorimetric_response::kLutStrideHermite;

// ===== RGBW-specific topology helpers =======================================

// Native-gamut detection (#2748). Returns true when the source primaries
// of `p` (input_xy_r/g/b) match the LED's measured primaries (xy_r/g/b)
// within float tolerance — i.e. the user is feeding "native LED gamut"
// drive coordinates rather than a foreign gamut like sRGB / Rec.2020.
//
// Native mode has stricter topology authority than named-gamut mode:
//
//   * Single-channel inputs are exact drive identity:
//       (R,0,0)->(R,0,0,0), (0,G,0)->(0,G,0,0), (0,0,B)->(0,0,B,0).
//
//   * Dual-channel outer edges are channel-set identity, not value identity.
//     For RG/RB/GB edges the inactive RGB channel and W must remain zero, but
//     the two active channels are still solved from the source target XYZ and
//     the measured diode XYZ/Y columns.  This is why yellow_half is not simply
//     raw (0.5,0.5,0,0): measured R/G luminance and the D65 source matrix set
//     the correct active-channel ratio.
//
//   * Three-channel inputs fall through to the strict RGBW sub-gamut routing
//     and may use W inside RGW/RBW/BGW.
//
// The W chromaticity (input_xy_w) is intentionally NOT compared here: native
// authority is about RGB topology.  Different target whites affect the source
// matrix and full-3-channel routing, not whether a pure/edge native input is
// allowed to introduce W.
inline bool is_native_input_gamut(const DiodeProfile& p) FL_NO_EXCEPT {
    constexpr float kPrimaryEps = 1e-6f;
    auto close = [](const float a[2], const float b[2]) FL_NO_EXCEPT {
        const float dx = a[0] - b[0];
        const float dy = a[1] - b[1];
        return (dx * dx + dy * dy) < kPrimaryEps;
    };
    return close(p.input_xy_r, p.xy_r)
        && close(p.input_xy_g, p.xy_g)
        && close(p.input_xy_b, p.xy_b);
}

// Topology activity classifier (#2748). Counts how many of `s_r, s_g, s_b`
// are above the LSB-level epsilon.  The solvers use this to select the native
// authority fast path:
//   n == 1 : exact single-axis identity
//   n == 2 : fixed-topology two-emitter solve
//   n == 3 : normal strict / LP / overdrive interior solve
inline int count_active_channels(float s_r, float s_g, float s_b) FL_NO_EXCEPT {
    // 1 / 65535 — matches the 16-bit verifier precision used in the
    // reference math model; anything below this is below noise floor.
    constexpr float kTopoEps = 1.0f / 65535.0f;
    int n = 0;
    if (s_r > kTopoEps) ++n;
    if (s_g > kTopoEps) ++n;
    if (s_b > kTopoEps) ++n;
    return n;
}

// ===== Types =================================================================

// Precomputed per-profile data: emitter XYZ columns + the four matrix inverses
// the solvers need. Built once when the active (profile, cct) pair changes.
// xy_w is the *effective* W chromaticity — equal to profile->xy_w when no CCT
// override is active, otherwise the cct_to_xy() shift. The strict solvers
// must route barycentric containment against this value (not the profile's
// raw xy_w) or they will choose the wrong sub-gamut for CCT-shifted whites.
struct ProfileCache {
    const DiodeProfile* profile;
    float xy_w[2];                           // effective W chromaticity (incl. CCT shift)
    float P_R[3], P_G[3], P_B[3], P_W[3];   // emitter XYZ at full drive
    float P_RGB_inv[3][3];                   // [R G B]^-1, used by wx_lp_legacy
    float P_RGW_inv[3][3];                   // [R G W]^-1
    float P_RBW_inv[3][3];                   // [R B W]^-1
    float P_BGW_inv[3][3];                   // [B G W]^-1
    float d_W[3];                            // P_RGB_inv * P_W (wx_lp_legacy cache)

    // Source-space → measured-device XYZ matrix (#2705).
    //
    // build_source_matrix() first derives the standard normalized CIE source
    // matrix from profile.input_xy_* (source white has Y=1). build_profile_cache()
    // then scales that matrix by the reference white-fit factor used by the
    // math model so M_src·[1,1,1] is in the same absolute XYZ/Y domain as the
    // measured emitter columns P_R/P_G/P_B/P_W.
    //
    // This scaling is essential: all runtime solves compare M_src targets
    // directly against measured diode columns. Leaving M_src at Y=1 would make
    // native D65 dual-edge and LP solves underpowered and would not match the
    // reference model/cube.
    //
    // When the source chromaticities are degenerate (input_xy_w[1] == 0, the
    // default for value-initialized `DiodeProfile{}`), `has_source_space` is
    // false and solvers fall back to direct device-emitter projection.
    float M_src[3][3];
    bool has_source_space;
};

// Owns its cell storage via fl::unique_ptr. Obtain via `build_lut(...)`,
// which atomically allocates + populates. Lookup code can rely on
// .cells.get() being non-null for the table's lifetime.
struct LutTable {
    int N = 0;                          // grid edge length
    LutInterp interp = LutInterp::Bilinear; // storage / interp scheme
    float xy_min[2] = {0.0f, 0.0f};     // grid origin in xy
    float xy_max[2] = {0.0f, 0.0f};     // grid extent in xy
    fl::unique_ptr<i16[]> cells;        // size = N*N*stride for chosen interp
};

// Two-emitter (warm-W, cool-W) profile for RGBCCT layered solver.
struct RgbcctProfile {
    DiodeProfile warm_path;  // R, G, B, W=warm — typical xy_w ≈ 3000K
    DiodeProfile cool_path;  // R, G, B, W=cool — typical xy_w ≈ 6500K
};

// Convenience: derive eta from a target CCT relative to the warm/cool
// reference CCTs. Linear interpolation in CCT space, clamped to [0, 1].
inline float rgbcct_eta_for_cct(int target_cct, int warm_cct,
                                int cool_cct) FL_NO_EXCEPT {
    if (cool_cct == warm_cct) return 0.5f;
    const float t = (static_cast<float>(target_cct) - warm_cct)
                  / (static_cast<float>(cool_cct) - warm_cct);
    return fl::clamp(t, 0.0f, 1.0f);
}

// ===== Function declarations (definitions in rgbw_colorimetric.cpp.hpp) =====
// All gated by FASTLED_RGBW_COLORIMETRIC at the definition site. With the
// macro off, the symbols simply don't exist in the library — the dispatch
// in rgbw.cpp.hpp falls back to the stub path (warn-once + rgb_2_rgbw_exact).

// Build the per-profile cache. If cct_override is in [1500, 15000], the W
// vertex chromaticity is replaced with cct_to_xy(cct_override). Pass
// cct_override = 0 to use the profile's xy_w unchanged.
void build_profile_cache(const DiodeProfile* p, int cct_override,
                         ProfileCache* cache) FL_NO_EXCEPT;

// Convenience overload: no CCT override.
inline void build_profile_cache(const DiodeProfile* p,
                                ProfileCache* cache) FL_NO_EXCEPT {
    build_profile_cache(p, 0, cache);
}

// Strict sub-gamut solver (gist sec 5).
//
// Runtime path:
//   1. Black/near-black returns zero.
//   2. Native single-axis inputs return exact identity.
//   3. Native dual-edge inputs are locked to RG/RB/GB and solved as a measured
//      two-emitter least-squares problem — no W/third-channel bleed, but also
//      not raw passthrough.
//   4. Interior inputs split source value from chroma, solve the full-chroma
//      endpoint in one of {RGW, RBW, BGW}, then scale by source value.
//
// Returns false only for numerically degenerate profiles / matrices where the
// caller should fall back to a simpler path.
bool solve_strict_subgamut(const ProfileCache& cache, float s_r,
                           float s_g, float s_b,
                           float out_rgbw[4]) FL_NO_EXCEPT;

// Per-chromaticity variant: starts from explicit (xy, Y) instead of input RGB.
// Used by the LUT builder where xy/Y have already been chosen by the table
// sampling pass. This path intentionally skips native input topology rules
// because there is no source RGB active-channel mask attached to an arbitrary
// xy sample.
bool solve_strict_subgamut_xy(const ProfileCache& cache,
                              const float xy_t[2], float Y_t,
                              float out_rgbw[4]) FL_NO_EXCEPT;

// Reference wx_lp_legacy solver.
//
// This is the analytical runtime counterpart of the reference math-model
// "wx_lp_legacy" cube.  It is not the boosted/overdrive mode.  It solves a
// chromaticity-preserving bounded endpoint:
//   1. honor native single/dual topology authority;
//   2. split source value from chroma;
//   3. project the full-chroma target into the reachable hull if needed;
//   4. solve the xy constraints while maximizing W;
//   5. keep a small RGB floor when the four-channel manifold can represent the
//      target, normalize the endpoint, then apply the original source value.
//
// Native single-axis inputs remain exact identity. Native dual edges remain
// locked to RG/RB/GB but are solved from measured/source XYZ instead of raw
// passthrough. This is intentionally separate from boosted overdrive below.
bool solve_wx_lp_legacy(const ProfileCache& cache, float s_r, float s_g,
                        float s_b, float out_rgbw[4]) FL_NO_EXCEPT;

// White-overdrive / boosted solver (#2706). Uses a separate visual policy
// that pushes W past the non-overdriven residual boundary by
// `overdrive_ratio`, accepting chromaticity drift toward the W diode in
// exchange for higher luminance. Keep this separate from wx_lp_legacy; the LP
// legacy reference is the bounded chromaticity-preserving solver above.
void solve_wx_overdrive(const ProfileCache& cache, float s_r, float s_g,
                        float s_b, float overdrive_ratio,
                        float out_rgbw[4]) FL_NO_EXCEPT;

// Default overdrive ratio for `kRGBWColorimetricBoosted`. 0.5 = halfway
// between the strict vertex and full W; produces visibly brighter output
// than the strict mode while limiting chromaticity drift.
constexpr float kDefaultOverdriveRatio = 0.5f;

// Allocate + populate a LUT for `cache` at `grid_n` edge length, using
// `interp` (default Hermite). Hermite tables carry per-cell value + slope
// (∂/∂t_x, ∂/∂t_y), tripling per-cell storage but typically achieving lower
// error than bilinear at the same grid size — enabling small tables (N=8..16)
// suitable for memory-constrained targets.
LutTable build_lut(const ProfileCache& cache, int grid_n,
                   LutInterp interp) FL_NO_EXCEPT;

inline LutTable build_lut(const ProfileCache& cache, int grid_n) FL_NO_EXCEPT {
    return build_lut(cache, grid_n, LutInterp::Hermite);
}

// Bilinear lookup + Y multiply + normalize.
void lookup_lut(const LutTable& lut, const float xy_t[2], float Y_t,
                float out_rgbw[4]) FL_NO_EXCEPT;

// RGBCCT layered solver: solve target against each W diode separately, then
// line-blend the resulting RGBW tuples by an eta in [0, 1].
// Output layout: out[0..2] = RGB, out[3] = warm-W, out[4] = cool-W.
void solve_rgbcct(const RgbcctProfile& profile, float s_r, float s_g,
                  float s_b, float eta, float out[5]) FL_NO_EXCEPT;

} // namespace colorimetric_detail
} // namespace fl
