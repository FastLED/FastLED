/// @file rgbw_colorimetric.cpp.hpp
/// Heavy implementations for the colorimetric RGBW solvers (issue #2545).
/// Gated by FASTLED_RGBW_COLORIMETRIC so the float math + simplex solver
/// + LUT machinery only land in the binary when the user opts in.

#include "fl/gfx/rgbw_colorimetric.h"

#if FASTLED_RGBW_COLORIMETRIC

#include "fl/log/log.h"
#include "fl/math/math.h"
#include "fl/stl/unique_ptr.h"

namespace fl {
namespace colorimetric_detail {

// =============================================================================
// Implementation map
// =============================================================================
//
// The colorimetric path is deliberately small-matrix analytical math rather
// than a baked 3D LUT.  The main implementation pieces are:
//
//   build_profile_cache()
//     Converts measured xy/Y emitters into absolute XYZ columns, caches the
//     three strict sub-gamut inverses, caches the RGB inverse used by W/LP
//     solvers, and scales the source RGB->XYZ matrix into the same absolute
//     device-Y domain.
//
//   solve_strict_subgamut()
//     Reference strict RGBW solve. Native singles are exact identity; native
//     dual edges are fixed-topology measured two-emitter solves; interiors
//     solve a full-chroma endpoint in RGW/RBW/BGW and then apply value.
//
//   solve_wx_lp_legacy()
//     Reference white-extraction solve. It maximizes W while preserving target
//     chromaticity, then applies value. This is separate from boosted mode.
//
//   solve_wx_overdrive()
//     Visual boosted policy that intentionally pushes W beyond the LP/strict
//     residual boundary and therefore may drift toward the W diode.
//
// The most important invariant is value/chroma separation.  If we solve a
// value-scaled RGB node directly and then normalize, several value levels can
// collapse to the same saturated tuple.  Solving the full-chroma endpoint
// first, normalizing/projection there, and applying the original value last
// preserves fixed-hue HSV ramps and matches the Python reference model.

// Build all per-profile quantities once so the per-pixel runtime path is just
// a few matrix-vector operations and tiny fixed-size solves.
//
// Important scale convention:
//   * P_R/P_G/P_B/P_W are measured emitter columns in absolute device XYZ/Y.
//   * build_source_matrix() returns a normalized source matrix where white
//     has Y=1.
//   * We scale M_src by the same white-fit factor used by the reference math
//     model so source targets and measured emitter columns share units.
//
// Without the M_src scale, native/D65 dual edges and LP legacy would compare
// a Y=1 source target against hundreds/thousands of device-Y emitter units.
// The result is underpowered solves and the wrong active-channel ratios.
void build_profile_cache(const DiodeProfile* p, int cct_override,
                         ProfileCache* cache) FL_NOEXCEPT {
    cache->profile = p;
    xyY_to_XYZ(p->xy_r[0], p->xy_r[1], p->lum_r, cache->P_R);
    xyY_to_XYZ(p->xy_g[0], p->xy_g[1], p->lum_g, cache->P_G);
    xyY_to_XYZ(p->xy_b[0], p->xy_b[1], p->lum_b, cache->P_B);
    cache->xy_w[0] = p->xy_w[0];
    cache->xy_w[1] = p->xy_w[1];
    if (cct_override >= 1500 && cct_override <= 15000) {
        cct_to_xy(cct_override, cache->xy_w);
    }
    xyY_to_XYZ(cache->xy_w[0], cache->xy_w[1], p->lum_w, cache->P_W);

    auto pack = [](const float* a, const float* b, const float* c,
                   float out[3][3]) FL_NOEXCEPT {
        out[0][0] = a[0]; out[0][1] = b[0]; out[0][2] = c[0];
        out[1][0] = a[1]; out[1][1] = b[1]; out[1][2] = c[1];
        out[2][0] = a[2]; out[2][1] = b[2]; out[2][2] = c[2];
    };

    float P_RGB[3][3], P_RGW[3][3], P_RBW[3][3], P_BGW[3][3];
    pack(cache->P_R, cache->P_G, cache->P_B, P_RGB);
    pack(cache->P_R, cache->P_G, cache->P_W, P_RGW);
    pack(cache->P_R, cache->P_B, cache->P_W, P_RBW);
    pack(cache->P_B, cache->P_G, cache->P_W, P_BGW);

    // Inverting a singular matrix leaves the destination with whatever
    // invert3x3 wrote — solvers reading it would silently produce garbage.
    // Warn once if any inversion fails so the user knows their profile has
    // degenerate primaries (colinear chromaticities, near-zero luminance, etc).
    const bool ok_rgb = invert3x3(P_RGB, cache->P_RGB_inv);
    const bool ok_rgw = invert3x3(P_RGW, cache->P_RGW_inv);
    const bool ok_rbw = invert3x3(P_RBW, cache->P_RBW_inv);
    const bool ok_bgw = invert3x3(P_BGW, cache->P_BGW_inv);
    if (!(ok_rgb && ok_rgw && ok_rbw && ok_bgw)) {
        FL_WARN_ONCE("RGBW colorimetric: profile has degenerate primaries — "
                     "one or more sub-gamut matrix inversions failed. Output "
                     "colors will be incorrect. Check DiodeProfile xy/lum values.");
        // Zero-init any failed inverse so downstream matvec3() output is
        // deterministic (zero) rather than UB-valued garbage.
        auto zero3x3 = [](float m[3][3]) FL_NOEXCEPT {
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) m[i][j] = 0.0f;
        };
        if (!ok_rgb) zero3x3(cache->P_RGB_inv);
        if (!ok_rgw) zero3x3(cache->P_RGW_inv);
        if (!ok_rbw) zero3x3(cache->P_RBW_inv);
        if (!ok_bgw) zero3x3(cache->P_BGW_inv);
    }

    matvec3(cache->P_RGB_inv, cache->P_W, cache->d_W);

    // Source-space matrix (#2705). When input_xy_w is degenerate (the
    // legacy value-initialized case `DiodeProfile{}`), leave M_src empty
    // and signal the solvers to fall back to the device-emitter projection.
    cache->has_source_space = (p->input_xy_w[1] > 1e-6f)
                            && build_source_matrix(p->input_xy_r, p->input_xy_g,
                                                   p->input_xy_b, p->input_xy_w,
                                                   cache->M_src);
    if (cache->has_source_space) {
        // build_source_matrix() produces the standard normalized CIE source
        // matrix with source white at Y=1. The solver matrices below are in
        // measured-device absolute XYZ units, so scale M_src by the same
        // white-fit factor used by the reference math model:
        //   K = 1 / max(solve_subgamut(source_white_Y1))
        // This makes M_src·[1,1,1] land at the brightest achievable device
        // white instead of an underpowered Y=1 target. Without this scale,
        // native/D65 dual-edge and LP solves cannot match the Python model.
        float X_w[3];
        xyY_to_XYZ(p->input_xy_w[0], p->input_xy_w[1], 1.0f, X_w);

        const float (*invs[3])[3] = {
            cache->P_RGW_inv, cache->P_RBW_inv, cache->P_BGW_inv,
        };
        float best_max_t = 0.0f;
        bool found_scale = false;
        constexpr float kScaleEps = 1e-6f;
        for (int k = 0; k < 3; ++k) {
            float t[3];
            matvec3(invs[k], X_w, t);
            if (t[0] < -kScaleEps || t[1] < -kScaleEps || t[2] < -kScaleEps) {
                continue;
            }
            const float mt = fl::max(fl::max(t[0], t[1]), t[2]);
            if (mt > kScaleEps && (!found_scale || mt < best_max_t)) {
                best_max_t = mt;
                found_scale = true;
            }
        }
        if (!found_scale) {
            float t[3];
            matvec3(cache->P_RGB_inv, X_w, t);
            const float mt = fl::max(fl::max(fl::fabs(t[0]), fl::fabs(t[1])),
                                     fl::fabs(t[2]));
            if (mt > kScaleEps) {
                best_max_t = mt;
                found_scale = true;
            }
        }
        const float scale_k = found_scale ? (1.0f / best_max_t) : 1.0f;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                cache->M_src[i][j] *= scale_k;
            }
        }
    } else {
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) cache->M_src[i][j] = 0.0f;
    }
}

// Project an out-of-hull target XYZ onto the achievable LED gamut (#2708,
// math-model gist §3). Returns true if a projection was applied, in which
// case `X_t` and `xy_t` are updated to the projected point at unit Y. When
// the target is already inside the full RGB triangle, returns false and
// leaves `X_t` / `xy_t` untouched.
//
// Algorithm matches the reference's `_strict_project_target_xyz_to_led_hull`:
// run NNLS in each of the three sub-gamuts (RGW / RBW / BGW), cap drive at
// 1.0 per sub-gamut, then pick the sub-gamut with smallest residual. Use
// the achieved chromaticity as the new target xy.
static bool project_to_hull(const ProfileCache& cache,
                            float X_t[3], float xy_t[2]) FL_NOEXCEPT {
    const float sum = X_t[0] + X_t[1] + X_t[2];
    if (sum < 1e-12f) return false;
    xy_t[0] = X_t[0] / sum;
    xy_t[1] = X_t[1] / sum;

    const DiodeProfile& p = *cache.profile;
    float bary[3];
    // Test the full RGB triangle (not the sub-gamut triangles) — a target
    // can be outside a single sub-gamut yet still inside the full hull.
    if (barycentric_xy(xy_t, p.xy_r, p.xy_g, p.xy_b, bary)
        && bary[0] >= -1e-9f && bary[1] >= -1e-9f && bary[2] >= -1e-9f) {
        return false;
    }

    struct Tri { const float* a; const float* b; const float* c; };
    const Tri tris[3] = {
        { cache.P_R, cache.P_G, cache.P_W },
        { cache.P_R, cache.P_B, cache.P_W },
        { cache.P_B, cache.P_G, cache.P_W },
    };
    float best_xyz[3] = {0, 0, 0};
    float best_residual = 1e30f;
    for (int k = 0; k < 3; ++k) {
        const Tri& tri = tris[k];
        const float M[3][3] = {
            { tri.a[0], tri.b[0], tri.c[0] },
            { tri.a[1], tri.b[1], tri.c[1] },
            { tri.a[2], tri.b[2], tri.c[2] },
        };
        float t[3], residual;
        nnls3(M, X_t, t, &residual);
        // Cap drive at full-scale per sub-gamut, matching the reference.
        float mt = t[0];
        if (t[1] > mt) mt = t[1];
        if (t[2] > mt) mt = t[2];
        if (mt > 1.0f) { const float inv = 1.0f / mt; t[0] *= inv; t[1] *= inv; t[2] *= inv; }
        float xyz[3];
        xyz[0] = M[0][0]*t[0] + M[0][1]*t[1] + M[0][2]*t[2];
        xyz[1] = M[1][0]*t[0] + M[1][1]*t[1] + M[1][2]*t[2];
        xyz[2] = M[2][0]*t[0] + M[2][1]*t[1] + M[2][2]*t[2];
        if (xyz[1] <= 1e-12f) continue;
        if (residual < best_residual) {
            best_residual = residual;
            best_xyz[0] = xyz[0]; best_xyz[1] = xyz[1]; best_xyz[2] = xyz[2];
        }
    }
    if (best_xyz[1] <= 1e-12f) {
        // All sub-gamut projections collapsed to zero — pathological
        // profile or extreme target. Leave the original X_t alone; the
        // strict solver will return false and the dispatch falls back.
        return false;
    }
    const float s2 = best_xyz[0] + best_xyz[1] + best_xyz[2];
    xy_t[0] = best_xyz[0] / s2;
    xy_t[1] = best_xyz[1] / s2;
    // Per reference, normalize Y to 1.0 for downstream routing. The full-chroma
    // topology is re-solved from this projected XYZ in the caller.
    xyY_to_XYZ(xy_t[0], xy_t[1], 1.0f, X_t);
    return true;
}


// Small leaf helpers used by the solvers below.  They are kept local to this
// translation unit so the public header stays focused on reusable math helpers
// and type declarations.
static float max3f(float a, float b, float c) FL_NOEXCEPT {
    return fl::max(fl::max(a, b), c);
}

static void zero4(float out[4]) FL_NOEXCEPT {
    out[0] = out[1] = out[2] = out[3] = 0.0f;
}

static void normalize4_if_needed(float out[4]) FL_NOEXCEPT {
    const float m = fl::max(fl::max(out[0], out[1]), fl::max(out[2], out[3]));
    if (m > 1.0f) {
        const float inv_m = 1.0f / m;
        out[0] *= inv_m; out[1] *= inv_m; out[2] *= inv_m; out[3] *= inv_m;
    }
}

static const float* column_for_idx(const ProfileCache& cache, int idx) FL_NOEXCEPT {
    switch (idx) {
    case 0: return cache.P_R;
    case 1: return cache.P_G;
    case 2: return cache.P_B;
    default: return cache.P_W;
    }
}

// Convert linear source RGB into measured-device absolute XYZ.
//
// In normal operation this uses cache.M_src, which has already been scaled into
// the measured emitter domain by build_profile_cache().  The fallback is for
// legacy / partially initialized profiles where input_xy_* is not populated:
// then the caller's RGB values are interpreted as direct measured RGB emitter
// drive fractions.
static void source_rgb_to_XYZ(const ProfileCache& cache, float s_r,
                              float s_g, float s_b,
                              float X_t[3]) FL_NOEXCEPT {
    if (cache.has_source_space) {
        const float s[3] = { s_r, s_g, s_b };
        matvec3(cache.M_src, s, X_t);
    } else {
        X_t[0] = cache.P_R[0] * s_r + cache.P_G[0] * s_g + cache.P_B[0] * s_b;
        X_t[1] = cache.P_R[1] * s_r + cache.P_G[1] * s_g + cache.P_B[1] * s_b;
        X_t[2] = cache.P_R[2] * s_r + cache.P_G[2] * s_g + cache.P_B[2] * s_b;
    }
}

// Native single-axis authority.  For true native input primaries, a pure
// R/G/B source coordinate is already a request for that exact LED channel.
// Do not route it through RGW/RBW/BGW or W/LP paths, since that can introduce
// W or another primary due to white-point/source-matrix differences.
static bool native_single_identity(float s_r, float s_g, float s_b,
                                   float out[4]) FL_NOEXCEPT {
    out[0] = fl::clamp(s_r, 0.0f, 1.0f);
    out[1] = fl::clamp(s_g, 0.0f, 1.0f);
    out[2] = fl::clamp(s_b, 0.0f, 1.0f);
    out[3] = 0.0f;
    return true;
}

// Solve a target XYZ using only a requested physical channel set.
//
// This is used for native outer edges.  It is intentionally not a generic
// four-channel optimizer: RG/RB/GB authority means the inactive primary and W
// must remain exactly zero.  For n==2 we solve the 3x2 least-squares normal
// equations with a tiny non-negative active-set fallback.  That preserves the
// edge topology while still respecting measured diode XYZ/Y ratios.
static bool solve_fixed_topology_least_squares(const ProfileCache& cache,
                                               const float X[3],
                                               const int* idx,
                                               int n,
                                               float out[4]) FL_NOEXCEPT {
    zero4(out);
    constexpr float kEps = 1e-9f;

    if (n == 1) {
        const float* A = column_for_idx(cache, idx[0]);
        const float aa = A[0]*A[0] + A[1]*A[1] + A[2]*A[2];
        const float ax = A[0]*X[0] + A[1]*X[1] + A[2]*X[2];
        out[idx[0]] = (aa > kEps) ? fl::max(ax / aa, 0.0f) : 0.0f;
        return true;
    }

    if (n == 2) {
        const float* A = column_for_idx(cache, idx[0]);
        const float* B = column_for_idx(cache, idx[1]);
        const float aa = A[0]*A[0] + A[1]*A[1] + A[2]*A[2];
        const float ab = A[0]*B[0] + A[1]*B[1] + A[2]*B[2];
        const float bb = B[0]*B[0] + B[1]*B[1] + B[2]*B[2];
        const float ax = A[0]*X[0] + A[1]*X[1] + A[2]*X[2];
        const float bx = B[0]*X[0] + B[1]*X[1] + B[2]*X[2];
        const float det = aa * bb - ab * ab;
        if (fl::fabs(det) < kEps) {
            return false;
        }

        float t0 = ( ax * bb - bx * ab) / det;
        float t1 = (-ax * ab + bx * aa) / det;

        // Non-negative active-set fallback for the 2-column case. If one
        // coefficient goes negative, project onto the remaining column.
        if (t0 < 0.0f && t1 >= 0.0f) {
            t0 = 0.0f;
            t1 = (bb > kEps) ? fl::max(bx / bb, 0.0f) : 0.0f;
        } else if (t1 < 0.0f && t0 >= 0.0f) {
            t1 = 0.0f;
            t0 = (aa > kEps) ? fl::max(ax / aa, 0.0f) : 0.0f;
        } else if (t0 < 0.0f && t1 < 0.0f) {
            t0 = 0.0f;
            t1 = 0.0f;
        }

        out[idx[0]] = fl::max(t0, 0.0f);
        out[idx[1]] = fl::max(t1, 0.0f);
        return true;
    }

    return false;
}

// Native dual-edge authority.  A native RG/RB/GB input must not introduce W or
// the inactive RGB primary, but it also must not be raw passthrough.  The
// target is first reduced to full-chroma edge coordinates, solved using only
// the active measured emitters, normalized at the endpoint, and finally scaled
// by the original source value.  This is the path that fixes yellow_half /
// cyan_half / magenta_half without reintroducing illegal topology.
static bool solve_native_dual_edge_fixed_topology(const ProfileCache& cache,
                                                  float s_r, float s_g,
                                                  float s_b,
                                                  float out[4]) FL_NOEXCEPT {
    zero4(out);
    constexpr float kEps = 1.0f / 65535.0f;
    const float value = max3f(s_r, s_g, s_b);
    if (value <= kEps) {
        return true;
    }

    // Native dual authority means edge-lock the active channel set, not raw
    // input passthrough. Solve the full-chroma edge target against the active
    // two measured emitter columns, normalize at the endpoint, then apply the
    // original value scale so yellow_half/cyan_half/etc remain granular.
    const float c_r = s_r / value;
    const float c_g = s_g / value;
    const float c_b = s_b / value;

    float X_full[3];
    source_rgb_to_XYZ(cache, c_r, c_g, c_b, X_full);

    int active[2];
    int n = 0;
    if (s_r > kEps) active[n++] = 0;
    if (s_g > kEps) active[n++] = 1;
    if (s_b > kEps) active[n++] = 2;
    if (n != 2) {
        return false;
    }

    float full[4];
    if (!solve_fixed_topology_least_squares(cache, X_full, active, n, full)) {
        return false;
    }
    normalize4_if_needed(full);

    out[0] = full[0] * value;
    out[1] = full[1] * value;
    out[2] = full[2] * value;
    out[3] = 0.0f;
    normalize4_if_needed(out);
    return true;
}

// Strict full-chroma endpoint solve from an absolute XYZ target.
//
// Callers pass the full-chroma target here, not the original value-scaled RGB
// node.  This function owns hull projection, xy routing into RGW/RBW/BGW, the
// cached 3x3 inverse solve, and endpoint normalization.  Value scaling happens
// in the public solve_strict_subgamut() wrapper after this endpoint is found.
static bool solve_strict_subgamut_from_XYZ(const ProfileCache& cache,
                                           float X_t[3],
                                           float out_rgbw[4]) FL_NOEXCEPT {
    zero4(out_rgbw);
    const float sum_xyz = X_t[0] + X_t[1] + X_t[2];
    if (sum_xyz < 1e-9f) {
        return true;
    }
    float xy_t[2] = { X_t[0] / sum_xyz, X_t[1] / sum_xyz };

    // Project full-chroma endpoints before topology routing. This keeps
    // value-scaled nodes from repeatedly normalizing to the same saturated
    // tuple and mirrors the reference model's endpoint-first structure.
    project_to_hull(cache, X_t, xy_t);

    struct SubGamut {
        const float* xy_a;
        const float* xy_b;
        const float* xy_c;
        const float (*Pinv)[3];
        int idx_a, idx_b, idx_c;
    };
    const SubGamut sgs[3] = {
        { cache.profile->xy_r, cache.profile->xy_g, cache.xy_w,
          cache.P_RGW_inv, 0, 1, 3 },
        { cache.profile->xy_r, cache.profile->xy_b, cache.xy_w,
          cache.P_RBW_inv, 0, 2, 3 },
        { cache.profile->xy_b, cache.profile->xy_g, cache.xy_w,
          cache.P_BGW_inv, 2, 1, 3 },
    };

    constexpr float kEps = 1e-4f;
    for (int k = 0; k < 3; ++k) {
        const SubGamut& sg = sgs[k];
        float bary[3];
        if (!barycentric_xy(xy_t, sg.xy_a, sg.xy_b, sg.xy_c, bary)) continue;
        if (bary[0] < -kEps || bary[1] < -kEps || bary[2] < -kEps) continue;
        float t[3];
        matvec3(sg.Pinv, X_t, t);
        if (t[0] < -kEps || t[1] < -kEps || t[2] < -kEps) continue;
        t[0] = fl::max(t[0], 0.0f);
        t[1] = fl::max(t[1], 0.0f);
        t[2] = fl::max(t[2], 0.0f);
        const float m = max3f(t[0], t[1], t[2]);
        if (m > 1.0f) {
            const float inv_m = 1.0f / m;
            t[0] *= inv_m; t[1] *= inv_m; t[2] *= inv_m;
        }
        out_rgbw[sg.idx_a] = t[0];
        out_rgbw[sg.idx_b] = t[1];
        out_rgbw[sg.idx_c] = t[2];
        return true;
    }
    return false;
}

bool solve_strict_subgamut(const ProfileCache& cache, float s_r,
                           float s_g, float s_b,
                           float out_rgbw[4]) FL_NOEXCEPT {
    zero4(out_rgbw);

    const float value = max3f(s_r, s_g, s_b);
    if (value <= 1e-9f) {
        return true;
    }

    // Native topology authority is split by case:
    //   n == 1: exact single-axis identity is correct.
    //   n == 2: keep the active RG/RB/GB edge locked, but still solve that
    //           two-emitter topology colorimetrically from measured XYZ/Y.
    // The previous n<=2 passthrough fixed W/third-channel bleed, but regressed
    // dual edges into raw 1:1 input->output tuples.
    if (is_native_input_gamut(*cache.profile)) {
        const int n_active = count_active_channels(s_r, s_g, s_b);
        if (n_active == 1) {
            return native_single_identity(s_r, s_g, s_b, out_rgbw);
        }
        if (n_active == 2) {
            return solve_native_dual_edge_fixed_topology(cache, s_r, s_g,
                                                         s_b, out_rgbw);
        }
    }

    // Interior colors must preserve value separately from chroma. Solving the
    // already value-scaled node and then normalizing is what collapsed several
    // HSV v055/v075/v100 rows to identical saturated tuples. Instead, solve
    // the full-chroma endpoint once, normalize/project there, then apply the
    // original value scale.
    const float c_r = s_r / value;
    const float c_g = s_g / value;
    const float c_b = s_b / value;

    float X_full[3];
    source_rgb_to_XYZ(cache, c_r, c_g, c_b, X_full);

    float full[4];
    if (!solve_strict_subgamut_from_XYZ(cache, X_full, full)) {
        return false;
    }

    out_rgbw[0] = full[0] * value;
    out_rgbw[1] = full[1] * value;
    out_rgbw[2] = full[2] * value;
    out_rgbw[3] = full[3] * value;
    normalize4_if_needed(out_rgbw);
    return true;
}

bool solve_strict_subgamut_xy(const ProfileCache& cache,
                              const float xy_t[2], float Y_t,
                              float out_rgbw[4]) FL_NOEXCEPT {
    out_rgbw[0] = out_rgbw[1] = out_rgbw[2] = out_rgbw[3] = 0.0f;
    if (Y_t < 1e-9f) {
        return true;
    }
    float X_t[3];
    xyY_to_XYZ(xy_t[0], xy_t[1], Y_t, X_t);

    // Out-of-hull projection (#2708). xy_t arrives directly (this variant is
    // called by the LUT builder), so we must project here before routing.
    // `project_to_hull` updates `X_t` and `local_xy` in place when projection
    // fires; the local copy is needed because the parameter is const.
    float local_xy[2] = { xy_t[0], xy_t[1] };
    project_to_hull(cache, X_t, local_xy);

    struct SubGamut {
        const float* xy_a;
        const float* xy_b;
        const float* xy_c;
        const float (*Pinv)[3];
        int idx_a, idx_b, idx_c;
    };
    // See note in solve_strict_subgamut: use cache.xy_w, not profile->xy_w.
    const SubGamut sgs[3] = {
        { cache.profile->xy_r, cache.profile->xy_g, cache.xy_w,
          cache.P_RGW_inv, 0, 1, 3 },
        { cache.profile->xy_r, cache.profile->xy_b, cache.xy_w,
          cache.P_RBW_inv, 0, 2, 3 },
        { cache.profile->xy_b, cache.profile->xy_g, cache.xy_w,
          cache.P_BGW_inv, 2, 1, 3 },
    };

    constexpr float kEps = 1e-4f;
    for (int k = 0; k < 3; ++k) {
        const SubGamut& sg = sgs[k];
        float bary[3];
        if (!barycentric_xy(local_xy, sg.xy_a, sg.xy_b, sg.xy_c, bary)) continue;
        if (bary[0] < -kEps || bary[1] < -kEps || bary[2] < -kEps) continue;
        float t[3];
        matvec3(sg.Pinv, X_t, t);
        if (t[0] < -kEps || t[1] < -kEps || t[2] < -kEps) continue;
        t[0] = fl::max(t[0], 0.0f);
        t[1] = fl::max(t[1], 0.0f);
        t[2] = fl::max(t[2], 0.0f);
        out_rgbw[sg.idx_a] = t[0];
        out_rgbw[sg.idx_b] = t[1];
        out_rgbw[sg.idx_c] = t[2];
        return true;
    }
    return false;
}


// Reference wx_lp_legacy endpoint helper.
//
// Given a target xy, search the bounded four-channel manifold for a tuple that
// preserves chromaticity while maximizing W.  The constraints are linear in
// drive fractions when written as:
//
//   X_i - x_target * (X_i + Y_i + Z_i)
//   Y_i - y_target * (X_i + Y_i + Z_i)
//
// so each candidate fixes two channels at low/high bounds and solves the other
// two channels from the 2x2 xy residual system.  A small RGB floor is tried
// before allowing channels to collapse, matching the reference LP legacy model
// better for white-like / neutral values in wall-reflected profiles.
static bool solve_wx_balanced_fraction_for_xy(const ProfileCache& cache,
                                              const float target_xy[2],
                                              float out[4]) FL_NOEXCEPT {
    zero4(out);
    if (!(target_xy[0] == target_xy[0]) || !(target_xy[1] == target_xy[1])) {
        return false;
    }

    const float* cols[4] = { cache.P_R, cache.P_G, cache.P_B, cache.P_W };
    float A[2][4];
    const float x = target_xy[0];
    const float y = target_xy[1];
    for (int i = 0; i < 4; ++i) {
        const float* P = cols[i];
        const float S = P[0] + P[1] + P[2];
        A[0][i] = P[0] - x * S;
        A[1][i] = P[1] - y * S;
    }

    const float y_max = fl::max(fl::max(cache.P_R[1], cache.P_G[1]),
                                fl::max(cache.P_B[1], cache.P_W[1]));
    const float y_weight[4] = {
        (y_max > 1e-12f) ? cache.P_R[1] / y_max : 0.0f,
        (y_max > 1e-12f) ? cache.P_G[1] / y_max : 0.0f,
        (y_max > 1e-12f) ? cache.P_B[1] / y_max : 0.0f,
        (y_max > 1e-12f) ? cache.P_W[1] / y_max : 0.0f,
    };

    bool found = false;
    float best_obj = -1e30f;
    float best[4] = {0, 0, 0, 0};
    const float floors[4] = { 1.0f / 1024.0f, 1.0f / 2048.0f,
                              1.0f / 4096.0f, 0.0f };
    constexpr float kEps = 1e-5f;

    for (int fidx = 0; fidx < 4 && !found; ++fidx) {
        const float lo = floors[fidx];
        const float hi = 1.0f;
        for (int free0 = 0; free0 < 4; ++free0) {
            for (int free1 = free0 + 1; free1 < 4; ++free1) {
                int fixed[2];
                int nf = 0;
                for (int i = 0; i < 4; ++i) {
                    if (i != free0 && i != free1) fixed[nf++] = i;
                }
                for (int b0 = 0; b0 < 2; ++b0) {
                    for (int b1 = 0; b1 < 2; ++b1) {
                        float cand[4] = {0, 0, 0, 0};
                        cand[fixed[0]] = b0 ? hi : lo;
                        cand[fixed[1]] = b1 ? hi : lo;

                        const float rhs0 = -(A[0][fixed[0]] * cand[fixed[0]]
                                           + A[0][fixed[1]] * cand[fixed[1]]);
                        const float rhs1 = -(A[1][fixed[0]] * cand[fixed[0]]
                                           + A[1][fixed[1]] * cand[fixed[1]]);
                        const float a00 = A[0][free0];
                        const float a01 = A[0][free1];
                        const float a10 = A[1][free0];
                        const float a11 = A[1][free1];
                        const float det = a00 * a11 - a01 * a10;
                        if (fl::fabs(det) < 1e-12f) {
                            continue;
                        }
                        cand[free0] = (rhs0 * a11 - a01 * rhs1) / det;
                        cand[free1] = (a00 * rhs1 - rhs0 * a10) / det;

                        bool ok = true;
                        for (int i = 0; i < 4; ++i) {
                            if (cand[i] < lo - kEps || cand[i] > hi + kEps) {
                                ok = false;
                                break;
                            }
                            cand[i] = fl::clamp(cand[i], lo, hi);
                        }
                        if (!ok) continue;

                        const float residual0 = A[0][0]*cand[0] + A[0][1]*cand[1]
                                              + A[0][2]*cand[2] + A[0][3]*cand[3];
                        const float residual1 = A[1][0]*cand[0] + A[1][1]*cand[1]
                                              + A[1][2]*cand[2] + A[1][3]*cand[3];
                        if (fl::fabs(residual0) > 5e-4f || fl::fabs(residual1) > 5e-4f) {
                            continue;
                        }

                        const float obj = cand[3]
                                        + 1e-6f * (y_weight[0]*cand[0]
                                                 + y_weight[1]*cand[1]
                                                 + y_weight[2]*cand[2]
                                                 + y_weight[3]*cand[3]);
                        if (!found || obj > best_obj) {
                            found = true;
                            best_obj = obj;
                            best[0] = cand[0]; best[1] = cand[1];
                            best[2] = cand[2]; best[3] = cand[3];
                        }
                    }
                }
            }
        }
    }

    if (!found) {
        return false;
    }

    const float m = fl::max(fl::max(best[0], best[1]), fl::max(best[2], best[3]));
    if (m <= 1e-12f) {
        return false;
    }
    const float inv_m = 1.0f / m;
    out[0] = fl::clamp(best[0] * inv_m, 0.0f, 1.0f);
    out[1] = fl::clamp(best[1] * inv_m, 0.0f, 1.0f);
    out[2] = fl::clamp(best[2] * inv_m, 0.0f, 1.0f);
    out[3] = fl::clamp(best[3] * inv_m, 0.0f, 1.0f);
    return true;
}

// Chromaticity-preserving max-W solver.
//
// This is the FastLED analytical implementation of the math-model
// wx_lp_legacy path.  It should not be merged with solve_wx_overdrive():
// LP legacy's objective is "as much W as possible without moving xy", whereas
// overdrive's objective is "more W/luminance, accepting controlled xy drift".
bool solve_wx_lp_legacy(const ProfileCache& cache, float s_r,
                        float s_g, float s_b,
                        float out_rgbw[4]) FL_NOEXCEPT {
    zero4(out_rgbw);

    const float value = max3f(s_r, s_g, s_b);
    if (value <= 1e-9f) {
        return true;
    }

    // LP legacy shares the native topology contract. Single-axis inputs are
    // exact identity. Dual edges are locked to RG/RB/GB but still solved from
    // measured/source XYZ; they are not W-overdrive candidates.
    if (is_native_input_gamut(*cache.profile)) {
        const int n_active = count_active_channels(s_r, s_g, s_b);
        if (n_active == 1) {
            return native_single_identity(s_r, s_g, s_b, out_rgbw);
        }
        if (n_active == 2) {
            return solve_native_dual_edge_fixed_topology(cache, s_r, s_g,
                                                         s_b, out_rgbw);
        }
    }

    // Reference wx_lp_legacy uses the current math model's bounded LP endpoint:
    // project to the strict reachable hull, solve xy constraints while
    // maximizing W, use a small four-channel floor when feasible, normalize the
    // endpoint, then apply source value. This matches the LP legacy cube much
    // better than the old direct residual formula, which collapses D65-like
    // values toward W plus a tiny RGB residual.
    const float c_r = s_r / value;
    const float c_g = s_g / value;
    const float c_b = s_b / value;

    float X_t[3];
    source_rgb_to_XYZ(cache, c_r, c_g, c_b, X_t);

    float xy_t[2];
    project_to_hull(cache, X_t, xy_t);

    float full[4];
    if (!solve_wx_balanced_fraction_for_xy(cache, xy_t, full)) {
        // Degenerate edge fallback: strict projected endpoint. This mirrors the
        // Python reference's fallback when the balanced four-channel manifold
        // cannot represent the requested xy.
        if (!solve_strict_subgamut_from_XYZ(cache, X_t, full)) {
            return false;
        }
        normalize4_if_needed(full);
    }

    out_rgbw[0] = full[0] * value;
    out_rgbw[1] = full[1] * value;
    out_rgbw[2] = full[2] * value;
    out_rgbw[3] = full[3] * value;
    normalize4_if_needed(out_rgbw);
    return true;
}

// Boosted / overdrive solver.
//
// This remains useful as an optional visual policy, but it is deliberately
// distinct from wx_lp_legacy.  The non-overdriven residual boundary gives the
// chromaticity-preserving W limit; overdrive_ratio then moves W toward 1.0 and
// accepts the corresponding drift toward the W diode.
void solve_wx_overdrive(const ProfileCache& cache, float s_r, float s_g,
                        float s_b, float overdrive_ratio,
                        float out_rgbw[4]) FL_NOEXCEPT {
    zero4(out_rgbw);

    const float value = max3f(s_r, s_g, s_b);
    if (value <= 1e-9f) {
        return;
    }

    // Boosted/overdrive is a distinct visual policy, but it still must keep
    // native single/dual topology authority. Single axes stay exact; dual
    // edges stay locked to RG/RB/GB through the measured two-emitter solve.
    if (is_native_input_gamut(*cache.profile)) {
        const int n_active = count_active_channels(s_r, s_g, s_b);
        if (n_active == 1) {
            native_single_identity(s_r, s_g, s_b, out_rgbw);
            return;
        }
        if (n_active == 2) {
            solve_native_dual_edge_fixed_topology(cache, s_r, s_g,
                                                  s_b, out_rgbw);
            return;
        }
    }

    // Solve the full-chroma endpoint first and scale value later. This keeps
    // boosted mode from flattening fixed-hue HSV value ramps into repeated
    // saturated endpoint tuples.
    const float c_r = s_r / value;
    const float c_g = s_g / value;
    const float c_b = s_b / value;

    float X_t[3];
    source_rgb_to_XYZ(cache, c_r, c_g, c_b, X_t);

    float xy_t[2];
    project_to_hull(cache, X_t, xy_t);

    float a[3];
    matvec3(cache.P_RGB_inv, X_t, a);
    const float* d = cache.d_W;

    float w_strict = 1.0f;
    for (int i = 0; i < 3; ++i) {
        if (d[i] > 1e-9f && a[i] >= 0.0f) {
            const float lim = a[i] / d[i];
            if (lim < w_strict) {
                w_strict = lim;
            }
        }
    }
    w_strict = fl::clamp(w_strict, 0.0f, 1.0f);

    const float rho = fl::clamp(overdrive_ratio, 0.0f, 1.0f);
    float w = w_strict + rho * (1.0f - w_strict);
    w = fl::clamp(w, 0.0f, 1.0f);

    float full[4];
    full[0] = fl::max(a[0] - w * d[0], 0.0f);
    full[1] = fl::max(a[1] - w * d[1], 0.0f);
    full[2] = fl::max(a[2] - w * d[2], 0.0f);
    full[3] = w;
    normalize4_if_needed(full);

    out_rgbw[0] = full[0] * value;
    out_rgbw[1] = full[1] * value;
    out_rgbw[2] = full[2] * value;
    out_rgbw[3] = full[3] * value;
    normalize4_if_needed(out_rgbw);
}

LutTable build_lut(const ProfileCache& cache, int grid_n,
                   LutInterp interp) FL_NOEXCEPT {
    LutTable lut;
    // Guard against degenerate grids: N < 2 would divide-by-zero in the
    // 1/(N-1) step below, and negative N would overflow the allocation size.
    // Return an empty LutTable so callers can detect failure via .N == 0.
    if (grid_n < 2) {
        return lut;
    }
    const fl::size_t n = static_cast<fl::size_t>(grid_n);
    lut.N = grid_n;
    lut.interp = interp;
    const fl::size_t stride =
        (interp == LutInterp::Hermite) ? kLutStrideHermite : kLutStrideBilinear;
    lut.cells = fl::make_unique<i16[]>(n * n * stride);

    const float* R = cache.profile->xy_r;
    const float* G = cache.profile->xy_g;
    const float* B = cache.profile->xy_b;
    const float xmin = fl::min(fl::min(R[0], G[0]), B[0]);
    const float xmax = fl::max(fl::max(R[0], G[0]), B[0]);
    const float ymin = fl::min(fl::min(R[1], G[1]), B[1]);
    const float ymax = fl::max(fl::max(R[1], G[1]), B[1]);
    const float xpad = (xmax - xmin) * 0.02f;
    const float ypad = (ymax - ymin) * 0.02f;
    lut.xy_min[0] = xmin - xpad;
    lut.xy_min[1] = ymin - ypad;
    lut.xy_max[0] = xmax + xpad;
    lut.xy_max[1] = ymax + ypad;

    i16* cells = lut.cells.get();
    const int N = grid_n;
    const float inv_Nm1 = 1.0f / static_cast<float>(N - 1);
    const float cell_dx = (lut.xy_max[0] - lut.xy_min[0]) * inv_Nm1;
    const float cell_dy = (lut.xy_max[1] - lut.xy_min[1]) * inv_Nm1;

    auto sample = [&](float x, float y, float out[4]) FL_NOEXCEPT {
        const float xy[2] = {x, y};
        solve_strict_subgamut_xy(cache, xy, 1.0f, out);
    };

    for (int j = 0; j < N; ++j) {
        const float y = lut.xy_min[1] + cell_dy * j;
        for (int i = 0; i < N; ++i) {
            const float x = lut.xy_min[0] + cell_dx * i;
            float v[4];
            sample(x, y, v);
            i16* cell = &cells[(j * N + i) * stride];
            cell[0] = quantize_lut_cell(v[0]);
            cell[1] = quantize_lut_cell(v[1]);
            cell[2] = quantize_lut_cell(v[2]);
            cell[3] = quantize_lut_cell(v[3]);

            if (interp != LutInterp::Hermite) continue;

            // Numerical partials in cell-parameter (t) units. Sample
            // half-a-cell ahead / behind in world coords, clamped to the LUT
            // domain so edges fall back to one-sided differences. With
            // eps_world = cell_dx / 2 and step_world = (clamped_+ - clamped_-),
            // df/dt = (v_xp - v_xn) / step_world * cell_dx.
            const float eps_x = cell_dx * 0.5f;
            const float eps_y = cell_dy * 0.5f;
            const float x_hi = fl::clamp(x + eps_x, lut.xy_min[0], lut.xy_max[0]);
            const float x_lo = fl::clamp(x - eps_x, lut.xy_min[0], lut.xy_max[0]);
            const float y_hi = fl::clamp(y + eps_y, lut.xy_min[1], lut.xy_max[1]);
            const float y_lo = fl::clamp(y - eps_y, lut.xy_min[1], lut.xy_max[1]);

            float v_xp[4] = {0}, v_xn[4] = {0}, v_yp[4] = {0}, v_yn[4] = {0};
            sample(x_hi, y, v_xp);
            sample(x_lo, y, v_xn);
            sample(x, y_hi, v_yp);
            sample(x, y_lo, v_yn);

            const float step_x = x_hi - x_lo;
            const float step_y = y_hi - y_lo;
            const float scale_x = (step_x > 1e-9f) ? (cell_dx / step_x) : 0.0f;
            const float scale_y = (step_y > 1e-9f) ? (cell_dy / step_y) : 0.0f;
            for (int k = 0; k < 4; ++k) {
                cell[4 + k] = quantize_lut_cell((v_xp[k] - v_xn[k]) * scale_x);
                cell[8 + k] = quantize_lut_cell((v_yp[k] - v_yn[k]) * scale_y);
            }
        }
    }
    return lut;
}

void lookup_lut(const LutTable& lut, const float xy_t[2], float Y_t,
                float out_rgbw[4]) FL_NOEXCEPT {
    // Defend against degenerate LUTs (empty table, unbuilt cells, zero span).
    // build_lut() returns an empty LutTable on bad input — without these
    // guards the divide-by-zero and OOB reads would corrupt random memory.
    out_rgbw[0] = out_rgbw[1] = out_rgbw[2] = out_rgbw[3] = 0.0f;
    if (lut.N < 2 || lut.cells.get() == nullptr) {
        return;
    }
    const int N = lut.N;
    const float dx = lut.xy_max[0] - lut.xy_min[0];
    const float dy = lut.xy_max[1] - lut.xy_min[1];
    if (dx <= 0.0f || dy <= 0.0f) {
        return;
    }
    float x_norm = (xy_t[0] - lut.xy_min[0]) / dx * (N - 1);
    float y_norm = (xy_t[1] - lut.xy_min[1]) / dy * (N - 1);
    x_norm = fl::clamp(x_norm, 0.0f, static_cast<float>(N - 1) - 1e-4f);
    y_norm = fl::clamp(y_norm, 0.0f, static_cast<float>(N - 1) - 1e-4f);
    const int i = static_cast<int>(x_norm);
    const int j = static_cast<int>(y_norm);
    const float fx = x_norm - i;
    const float fy = y_norm - j;

    const i16* base = lut.cells.get();
    const int stride =
        (lut.interp == LutInterp::Hermite) ? kLutStrideHermite : kLutStrideBilinear;
    const i16* c00 = &base[(j * N + i) * stride];
    const i16* c10 = &base[(j * N + i + 1) * stride];
    const i16* c01 = &base[((j + 1) * N + i) * stride];
    const i16* c11 = &base[((j + 1) * N + i + 1) * stride];

    const float inv_Q = 1.0f / static_cast<float>(kLutQ);

    if (lut.interp == LutInterp::Hermite) {
        // Bicubic Hermite with no fxy cross term. Basis is computed via the
        // shared header helper `hermite_basis` so the test suite exercises the
        // same evaluator (CodeRabbit #2707).
        float bx[4], by[4];
        hermite_basis(fx, bx);
        hermite_basis(fy, by);
        const float h00x = bx[0], h01x = bx[1], h10x = bx[2], h11x = bx[3];
        const float h00y = by[0], h01y = by[1], h10y = by[2], h11y = by[3];

        for (int k = 0; k < 4; ++k) {
            const float v00 = c00[k] * inv_Q;
            const float v10 = c10[k] * inv_Q;
            const float v01 = c01[k] * inv_Q;
            const float v11 = c11[k] * inv_Q;
            const float dx00 = c00[4 + k] * inv_Q;
            const float dx10 = c10[4 + k] * inv_Q;
            const float dx01 = c01[4 + k] * inv_Q;
            const float dx11 = c11[4 + k] * inv_Q;
            const float dy00 = c00[8 + k] * inv_Q;
            const float dy10 = c10[8 + k] * inv_Q;
            const float dy01 = c01[8 + k] * inv_Q;
            const float dy11 = c11[8 + k] * inv_Q;

            const float per_Y =
                  v00 * h00x * h00y + v10 * h01x * h00y
                + v01 * h00x * h01y + v11 * h01x * h01y
                + dx00 * h10x * h00y + dx10 * h11x * h00y
                + dx01 * h10x * h01y + dx11 * h11x * h01y
                + dy00 * h00x * h10y + dy10 * h01x * h10y
                + dy01 * h00x * h11y + dy11 * h01x * h11y;

            float t = per_Y * Y_t;
            if (t < 0.0f) t = 0.0f;
            out_rgbw[k] = t;
        }
    } else {
        const float w00 = (1 - fx) * (1 - fy);
        const float w10 = fx * (1 - fy);
        const float w01 = (1 - fx) * fy;
        const float w11 = fx * fy;
        for (int k = 0; k < 4; ++k) {
            const float per_Y = (w00 * c00[k] + w10 * c10[k]
                               + w01 * c01[k] + w11 * c11[k]) * inv_Q;
            float t = per_Y * Y_t;
            if (t < 0.0f) t = 0.0f;
            out_rgbw[k] = t;
        }
    }

    const float m = fl::max(fl::max(out_rgbw[0], out_rgbw[1]),
                            fl::max(out_rgbw[2], out_rgbw[3]));
    if (m > 1.0f) {
        const float inv_m = 1.0f / m;
        out_rgbw[0] *= inv_m;
        out_rgbw[1] *= inv_m;
        out_rgbw[2] *= inv_m;
        out_rgbw[3] *= inv_m;
    }
}

void solve_rgbcct(const RgbcctProfile& profile, float s_r, float s_g,
                  float s_b, float eta, float out[5]) FL_NOEXCEPT {
    ProfileCache warm_cache;
    ProfileCache cool_cache;
    build_profile_cache(&profile.warm_path, 0, &warm_cache);
    build_profile_cache(&profile.cool_path, 0, &cool_cache);

    float warm_rgbw[4] = {0};
    float cool_rgbw[4] = {0};
    solve_strict_subgamut(warm_cache, s_r, s_g, s_b, warm_rgbw);
    solve_strict_subgamut(cool_cache, s_r, s_g, s_b, cool_rgbw);

    eta = fl::clamp(eta, 0.0f, 1.0f);
    const float w_warm = 1.0f - eta;
    const float w_cool = eta;

    out[0] = w_warm * warm_rgbw[0] + w_cool * cool_rgbw[0];
    out[1] = w_warm * warm_rgbw[1] + w_cool * cool_rgbw[1];
    out[2] = w_warm * warm_rgbw[2] + w_cool * cool_rgbw[2];
    out[3] = w_warm * warm_rgbw[3];
    out[4] = w_cool * cool_rgbw[3];

    const float m = fl::max(
        fl::max(fl::max(out[0], out[1]), fl::max(out[2], out[3])), out[4]);
    if (m > 1.0f) {
        const float inv_m = 1.0f / m;
        for (int k = 0; k < 5; ++k) out[k] *= inv_m;
    }
}

} // namespace colorimetric_detail
} // namespace fl

#endif  // FASTLED_RGBW_COLORIMETRIC
