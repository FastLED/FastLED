/// @file rgbw_colorimetric.h
/// Chromaticity-aware RGBW solvers — strict sub-gamut + wx_lp_legacy white
/// overdrive + LUT + RGBCCT (issue #2545). This header carries the type
/// declarations + tiny pure-math helpers; the heavy solver / LUT / RGBCCT
/// implementations live in rgbw_colorimetric.cpp.hpp behind the
/// FASTLED_RGBW_COLORIMETRIC compile gate.

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

    // Source-space → device-XYZ matrix (#2705). M_src · source_rgb = target
    // XYZ in the device's absolute XYZ frame. Built from profile.input_xy_*
    // via the standard CIE primary-matrix construction (column-normalized so
    // M_src·[1,1,1] equals the source white XYZ at Y=1). When the source
    // chromaticities are degenerate (input_xy_w[1] == 0, the default for
    // value-initialized `DiodeProfile{}`), `has_source_space` is false and
    // solvers fall back to the legacy device-emitter projection.
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

// Strict sub-gamut solver (gist sec 5). Routes target chromaticity to one of
// {RGW, RBW, BGW} and solves the 3x3 system. Returns false if numerically
// degenerate (caller should fall back to a simpler path).
bool solve_strict_subgamut(const ProfileCache& cache, float s_r,
                           float s_g, float s_b,
                           float out_rgbw[4]) FL_NOEXCEPT;

// Per-chromaticity variant: starts from explicit (xy, Y) instead of input RGB.
// Used by the LUT builder.
bool solve_strict_subgamut_xy(const ProfileCache& cache,
                              const float xy_t[2], float Y_t,
                              float out_rgbw[4]) FL_NOEXCEPT;

// White-overdrive solver (#2706). Computes the strict-vertex W (same as
// `solve_strict_subgamut`) and then pushes W past it by `overdrive_ratio`,
// accepting chromaticity drift toward the W diode in exchange for higher
// luminance. At overdrive_ratio = 0 this matches the strict-vertex output;
// at overdrive_ratio = 1 W is driven to 1.0. Default boosted mode uses
// `kDefaultOverdriveRatio` (see below). Replaces the previous `solve_wx_lp`
// which was mathematically equivalent to the strict sub-gamut solver.
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
