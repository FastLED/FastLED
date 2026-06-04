/// @file rgbw_colorimetric.h
/// Chromaticity-aware RGBW solvers — strict sub-gamut + wx_lp_legacy white
/// extraction + boosted overdrive + LUT + RGBCCT (issue #2545).
///
/// This header carries the public/internal declarations and the small inline
/// math helpers used by the implementation.  The heavier solver, LUT, and
/// RGBCCT code lives in rgbw_colorimetric.cpp.hpp behind the
/// FASTLED_RGBW_COLORIMETRIC compile gate.
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

#include "fl/gfx/rgbw.h"
#include "fl/math/math.h"
#include "fl/stl/stdint.h"
#include "fl/stl/unique_ptr.h"

namespace fl {
namespace colorimetric_detail {

// ===== Pure-math inline helpers ==============================================
// Tight, leaf-level operations kept inline because:
//   1. They're trivial bodies (≤ 25 lines).
//   2. They appear in hot inner loops; inlining lets the optimizer fold them.
//   3. Tests can validate them directly without a full library rebuild.

// Krystek's approximation for blackbody chromaticity (good 1000K - 15000K).
// Krystek's outputs are (u, v) in the CIE 1960 UCS; convert to xy via the
// standard 1960->xy formulas. Reference: Krystek, "An algorithm to calculate
// correlated color temperature", Color Research & Application, 1985.
inline void cct_to_xy(int cct, float out[2]) FL_NOEXCEPT {
    const float T = static_cast<float>(
        (cct < 1500) ? 1500 : ((cct > 15000) ? 15000 : cct));
    const float T2 = T * T;
    const float u_num = 0.860117757f + 1.54118254e-4f * T + 1.28641212e-7f * T2;
    const float u_den = 1.0f + 8.42420235e-4f * T + 7.08145163e-7f * T2;
    const float v_num = 0.317398726f + 4.22806245e-5f * T + 4.20481691e-8f * T2;
    const float v_den = 1.0f - 2.89741816e-5f * T + 1.61456053e-7f * T2;
    const float u = u_num / u_den;
    const float v = v_num / v_den;
    const float den = 2.0f * u - 8.0f * v + 4.0f;
    out[0] = 3.0f * u / den;
    out[1] = 2.0f * v / den;
}

inline void xyY_to_XYZ(float x, float y, float Y, float out[3]) FL_NOEXCEPT {
    if (y < 1e-12f) {
        out[0] = out[1] = out[2] = 0.0f;
        return;
    }
    const float inv_y = 1.0f / y;
    out[0] = x * Y * inv_y;
    out[1] = Y;
    out[2] = (1.0f - x - y) * Y * inv_y;
}

inline bool invert3x3(const float in[3][3], float out[3][3]) FL_NOEXCEPT {
    const float a = in[0][0], b = in[0][1], c = in[0][2];
    const float d = in[1][0], e = in[1][1], f = in[1][2];
    const float g = in[2][0], h = in[2][1], i = in[2][2];
    const float det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    if (fl::fabs(det) < 1e-20f) {
        return false;
    }
    const float inv_det = 1.0f / det;
    out[0][0] = (e * i - f * h) * inv_det;
    out[0][1] = (c * h - b * i) * inv_det;
    out[0][2] = (b * f - c * e) * inv_det;
    out[1][0] = (f * g - d * i) * inv_det;
    out[1][1] = (a * i - c * g) * inv_det;
    out[1][2] = (c * d - a * f) * inv_det;
    out[2][0] = (d * h - e * g) * inv_det;
    out[2][1] = (b * g - a * h) * inv_det;
    out[2][2] = (a * e - b * d) * inv_det;
    return true;
}

inline void matvec3(const float M[3][3], const float v[3], float out[3]) FL_NOEXCEPT {
    out[0] = M[0][0] * v[0] + M[0][1] * v[1] + M[0][2] * v[2];
    out[1] = M[1][0] * v[0] + M[1][1] * v[1] + M[1][2] * v[2];
    out[2] = M[2][0] * v[0] + M[2][1] * v[1] + M[2][2] * v[2];
}

inline bool barycentric_xy(const float t[2], const float A[2], const float B[2],
                           const float C[2], float bary[3]) FL_NOEXCEPT {
    const float v0x = B[0] - A[0], v0y = B[1] - A[1];
    const float v1x = C[0] - A[0], v1y = C[1] - A[1];
    const float v2x = t[0] - A[0], v2y = t[1] - A[1];
    const float d00 = v0x * v0x + v0y * v0y;
    const float d01 = v0x * v1x + v0y * v1y;
    const float d11 = v1x * v1x + v1y * v1y;
    const float d20 = v2x * v0x + v2y * v0y;
    const float d21 = v2x * v1x + v2y * v1y;
    const float den = d00 * d11 - d01 * d01;
    if (fl::fabs(den) < 1e-20f) {
        return false;
    }
    const float inv_den = 1.0f / den;
    const float u = (d11 * d20 - d01 * d21) * inv_den;
    const float v = (d00 * d21 - d01 * d20) * inv_den;
    bary[0] = 1.0f - u - v;
    bary[1] = u;
    bary[2] = v;
    return true;
}

inline u8 quantize_u8(float v) FL_NOEXCEPT {
    const float scaled = v * 255.0f + 0.5f;
    if (scaled <= 0.0f) return 0;
    if (scaled >= 255.0f) return 255;
    return static_cast<u8>(scaled);
}

// Standard CIE primary-matrix construction (#2705). Given source primary
// chromaticities xy_r/g/b and a source white chromaticity xy_w, build the
// 3x3 matrix M such that M·[1,1,1]^T = xyY_to_XYZ(xy_w, 1.0). Columns are
// per-channel scaled XYZ vectors of the primaries at Y=1, with scaling
// chosen so the (1,1,1) input lands at source white in XYZ. This is the
// classic linear-sRGB -> XYZ derivation generalized to arbitrary primaries.
// Returns false if the primary matrix is singular (collinear chromaticities).
inline bool build_source_matrix(const float xy_r[2], const float xy_g[2],
                                const float xy_b[2], const float xy_w[2],
                                float M_out[3][3]) FL_NOEXCEPT {
    float xyz_R[3], xyz_G[3], xyz_B[3], xyz_W[3];
    xyY_to_XYZ(xy_r[0], xy_r[1], 1.0f, xyz_R);
    xyY_to_XYZ(xy_g[0], xy_g[1], 1.0f, xyz_G);
    xyY_to_XYZ(xy_b[0], xy_b[1], 1.0f, xyz_B);
    xyY_to_XYZ(xy_w[0], xy_w[1], 1.0f, xyz_W);

    float P[3][3];
    P[0][0] = xyz_R[0]; P[0][1] = xyz_G[0]; P[0][2] = xyz_B[0];
    P[1][0] = xyz_R[1]; P[1][1] = xyz_G[1]; P[1][2] = xyz_B[1];
    P[2][0] = xyz_R[2]; P[2][1] = xyz_G[2]; P[2][2] = xyz_B[2];

    float P_inv[3][3];
    if (!invert3x3(P, P_inv)) {
        return false;
    }
    float k[3];
    matvec3(P_inv, xyz_W, k);

    M_out[0][0] = k[0] * xyz_R[0]; M_out[0][1] = k[1] * xyz_G[0]; M_out[0][2] = k[2] * xyz_B[0];
    M_out[1][0] = k[0] * xyz_R[1]; M_out[1][1] = k[1] * xyz_G[1]; M_out[1][2] = k[2] * xyz_B[1];
    M_out[2][0] = k[0] * xyz_R[2]; M_out[2][1] = k[1] * xyz_G[2]; M_out[2][2] = k[2] * xyz_B[2];
    return true;
}

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
inline bool is_native_input_gamut(const DiodeProfile& p) FL_NOEXCEPT {
    constexpr float kPrimaryEps = 1e-6f;
    auto close = [](const float a[2], const float b[2]) FL_NOEXCEPT {
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
inline int count_active_channels(float s_r, float s_g, float s_b) FL_NOEXCEPT {
    // 1 / 65535 — matches the 16-bit verifier precision used in the
    // reference math model; anything below this is below noise floor.
    constexpr float kTopoEps = 1.0f / 65535.0f;
    int n = 0;
    if (s_r > kTopoEps) ++n;
    if (s_g > kTopoEps) ++n;
    if (s_b > kTopoEps) ++n;
    return n;
}

// Non-negative least squares for the 3×3 sub-system M·t = b with t ≥ 0
// (#2708, gist §3). Projected-gradient form matching the reference
// `_nnls_solve` fallback used when scipy is unavailable: 500 iterations at
// step 0.01. Cheap (~50 µs scalar on a typical MCU at -O2; only invoked for
// out-of-hull source targets, never on the in-hull fast path) and free of
// dynamic allocation, which makes it safe to inline behind the colorimetric
// solvers' rare-branch.
inline void nnls3(const float M[3][3], const float b[3],
                  float t_out[3], float* residual_out) FL_NOEXCEPT {
    float t[3] = {0.0f, 0.0f, 0.0f};
    constexpr float kStep = 0.01f;
    constexpr int kIters = 500;
    for (int it = 0; it < kIters; ++it) {
        float r[3];
        r[0] = M[0][0]*t[0] + M[0][1]*t[1] + M[0][2]*t[2] - b[0];
        r[1] = M[1][0]*t[0] + M[1][1]*t[1] + M[1][2]*t[2] - b[1];
        r[2] = M[2][0]*t[0] + M[2][1]*t[1] + M[2][2]*t[2] - b[2];
        // grad = Mᵀ · r
        float g[3];
        g[0] = M[0][0]*r[0] + M[1][0]*r[1] + M[2][0]*r[2];
        g[1] = M[0][1]*r[0] + M[1][1]*r[1] + M[2][1]*r[2];
        g[2] = M[0][2]*r[0] + M[1][2]*r[1] + M[2][2]*r[2];
        for (int j = 0; j < 3; ++j) {
            float v = t[j] - kStep * g[j];
            t[j] = v > 0.0f ? v : 0.0f;
        }
    }
    if (residual_out != nullptr) {
        float r[3];
        r[0] = M[0][0]*t[0] + M[0][1]*t[1] + M[0][2]*t[2] - b[0];
        r[1] = M[1][0]*t[0] + M[1][1]*t[1] + M[1][2]*t[2] - b[1];
        r[2] = M[2][0]*t[0] + M[2][1]*t[1] + M[2][2]*t[2] - b[2];
        *residual_out = fl::sqrt(r[0]*r[0] + r[1]*r[1] + r[2]*r[2]);
    }
    t_out[0] = t[0]; t_out[1] = t[1]; t_out[2] = t[2];
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

// LUT cell quantization scale (value in [-8, +8) maps to i16 via *kLutQ).
constexpr i16 kLutQ = 4096;

// Storage stride per grid point. Bilinear LUTs store 4 (rgbw) values; Hermite
// LUTs additionally store ∂/∂t_x and ∂/∂t_y per channel (in cell-parameter
// units), enabling bicubic Hermite interpolation that reaches comparable
// accuracy at ~half the grid edge length — ~25 % of the memory at ~ the
// same error vs. bilinear. The grid step is uniform so derivatives are
// naturally expressed in cell-parameter units (t ∈ [0, 1] per cell).
constexpr int kLutStrideBilinear = 4;
constexpr int kLutStrideHermite = 12;

enum class LutInterp : u8 {
    Bilinear = 0,
    Hermite = 1,
};

// Cubic Hermite basis on [0, 1]. Output layout: { h00, h01, h10, h11 } where
//   h00(t) = 2t³ - 3t² + 1   value at t=0
//   h01(t) = -2t³ + 3t²      value at t=1
//   h10(t) = t³ - 2t² + t    derivative at t=0
//   h11(t) = t³ - t²         derivative at t=1
// Lifted into a header inline so `lookup_lut` and the test suite consume
// exactly the same evaluator (CodeRabbit #2707: tests that redefine basis
// locally cannot catch regressions in the production code).
inline void hermite_basis(float t, float out[4]) FL_NOEXCEPT {
    const float t2 = t * t;
    const float t3 = t2 * t;
    out[0] = 2.0f * t3 - 3.0f * t2 + 1.0f;
    out[1] = -2.0f * t3 + 3.0f * t2;
    out[2] = t3 - 2.0f * t2 + t;
    out[3] = t3 - t2;
}

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

inline i16 quantize_lut_cell(float v) FL_NOEXCEPT {
    const float scaled = v * static_cast<float>(kLutQ) + 0.5f;
    if (scaled <= -32768.0f) return -32768;
    if (scaled >= 32767.0f) return 32767;
    return static_cast<i16>(scaled);
}

// Convenience: derive eta from a target CCT relative to the warm/cool
// reference CCTs. Linear interpolation in CCT space, clamped to [0, 1].
inline float rgbcct_eta_for_cct(int target_cct, int warm_cct,
                                int cool_cct) FL_NOEXCEPT {
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
                         ProfileCache* cache) FL_NOEXCEPT;

// Convenience overload: no CCT override.
inline void build_profile_cache(const DiodeProfile* p,
                                ProfileCache* cache) FL_NOEXCEPT {
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
                           float out_rgbw[4]) FL_NOEXCEPT;

// Per-chromaticity variant: starts from explicit (xy, Y) instead of input RGB.
// Used by the LUT builder where xy/Y have already been chosen by the table
// sampling pass. This path intentionally skips native input topology rules
// because there is no source RGB active-channel mask attached to an arbitrary
// xy sample.
bool solve_strict_subgamut_xy(const ProfileCache& cache,
                              const float xy_t[2], float Y_t,
                              float out_rgbw[4]) FL_NOEXCEPT;

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
                        float s_b, float out_rgbw[4]) FL_NOEXCEPT;

// White-overdrive / boosted solver (#2706). Uses a separate visual policy
// that pushes W past the non-overdriven residual boundary by
// `overdrive_ratio`, accepting chromaticity drift toward the W diode in
// exchange for higher luminance. Keep this separate from wx_lp_legacy; the LP
// legacy reference is the bounded chromaticity-preserving solver above.
void solve_wx_overdrive(const ProfileCache& cache, float s_r, float s_g,
                        float s_b, float overdrive_ratio,
                        float out_rgbw[4]) FL_NOEXCEPT;

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
                   LutInterp interp) FL_NOEXCEPT;

inline LutTable build_lut(const ProfileCache& cache, int grid_n) FL_NOEXCEPT {
    return build_lut(cache, grid_n, LutInterp::Hermite);
}

// Bilinear lookup + Y multiply + normalize.
void lookup_lut(const LutTable& lut, const float xy_t[2], float Y_t,
                float out_rgbw[4]) FL_NOEXCEPT;

// RGBCCT layered solver: solve target against each W diode separately, then
// line-blend the resulting RGBW tuples by an eta in [0, 1].
// Output layout: out[0..2] = RGB, out[3] = warm-W, out[4] = cool-W.
void solve_rgbcct(const RgbcctProfile& profile, float s_r, float s_g,
                  float s_b, float eta, float out[5]) FL_NOEXCEPT;

} // namespace colorimetric_detail
} // namespace fl
