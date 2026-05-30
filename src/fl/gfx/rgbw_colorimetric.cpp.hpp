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
}

bool solve_strict_subgamut(const ProfileCache& cache, float s_r,
                           float s_g, float s_b,
                           float out_rgbw[4]) FL_NOEXCEPT {
    out_rgbw[0] = out_rgbw[1] = out_rgbw[2] = out_rgbw[3] = 0.0f;

    float X_t[3];
    X_t[0] = cache.P_R[0] * s_r + cache.P_G[0] * s_g + cache.P_B[0] * s_b;
    X_t[1] = cache.P_R[1] * s_r + cache.P_G[1] * s_g + cache.P_B[1] * s_b;
    X_t[2] = cache.P_R[2] * s_r + cache.P_G[2] * s_g + cache.P_B[2] * s_b;
    const float sum_xyz = X_t[0] + X_t[1] + X_t[2];
    if (sum_xyz < 1e-9f) {
        return true;
    }
    const float xy_t[2] = { X_t[0] / sum_xyz, X_t[1] / sum_xyz };

    struct SubGamut {
        const float* xy_a;
        const float* xy_b;
        const float* xy_c;
        const float (*Pinv)[3];
        int idx_a, idx_b, idx_c;
    };
    // Route barycentric containment against the *effective* W chromaticity
    // (cache.xy_w) so CCT-shifted whites land in the same simplex the matrix
    // solve uses. Using profile->xy_w here would mis-route shifted targets.
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
        const float m = fl::max(fl::max(t[0], t[1]), t[2]);
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

bool solve_strict_subgamut_xy(const ProfileCache& cache,
                              const float xy_t[2], float Y_t,
                              float out_rgbw[4]) FL_NOEXCEPT {
    out_rgbw[0] = out_rgbw[1] = out_rgbw[2] = out_rgbw[3] = 0.0f;
    if (Y_t < 1e-9f) {
        return true;
    }
    float X_t[3];
    xyY_to_XYZ(xy_t[0], xy_t[1], Y_t, X_t);

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
        if (!barycentric_xy(xy_t, sg.xy_a, sg.xy_b, sg.xy_c, bary)) continue;
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

void solve_wx_lp(const ProfileCache& cache, float s_r, float s_g,
                 float s_b, float out_rgbw[4]) FL_NOEXCEPT {
    const float a[3] = { s_r, s_g, s_b };
    const float* d = cache.d_W;

    float w = 1.0f;
    for (int i = 0; i < 3; ++i) {
        if (d[i] > 1e-9f && a[i] >= 0.0f) {
            const float lim = a[i] / d[i];
            if (lim < w) {
                w = lim;
            }
        }
    }
    w = fl::clamp(w, 0.0f, 1.0f);

    float t[4];
    t[0] = fl::max(a[0] - w * d[0], 0.0f);
    t[1] = fl::max(a[1] - w * d[1], 0.0f);
    t[2] = fl::max(a[2] - w * d[2], 0.0f);
    t[3] = w;

    const float m = fl::max(fl::max(t[0], t[1]), fl::max(t[2], t[3]));
    if (m > 1.0f) {
        const float inv_m = 1.0f / m;
        t[0] *= inv_m; t[1] *= inv_m; t[2] *= inv_m; t[3] *= inv_m;
    }
    out_rgbw[0] = t[0];
    out_rgbw[1] = t[1];
    out_rgbw[2] = t[2];
    out_rgbw[3] = t[3];
}

LutTable build_lut(const ProfileCache& cache, int grid_n) FL_NOEXCEPT {
    LutTable lut;
    // Guard against degenerate grids: N < 2 would divide-by-zero in the
    // 1/(N-1) step below, and negative N would overflow the allocation size.
    // Return an empty LutTable so callers can detect failure via .N == 0.
    if (grid_n < 2) {
        return lut;
    }
    const fl::size_t n = static_cast<fl::size_t>(grid_n);
    lut.N = grid_n;
    lut.cells = fl::make_unique<i16[]>(n * n * 4u);

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
    for (int j = 0; j < N; ++j) {
        const float y = lut.xy_min[1]
                      + (lut.xy_max[1] - lut.xy_min[1]) * j * inv_Nm1;
        for (int i = 0; i < N; ++i) {
            const float x = lut.xy_min[0]
                          + (lut.xy_max[0] - lut.xy_min[0]) * i * inv_Nm1;
            const float xy[2] = {x, y};
            float rgbw[4];
            solve_strict_subgamut_xy(cache, xy, 1.0f, rgbw);
            i16* cell = &cells[(j * N + i) * 4];
            cell[0] = quantize_lut_cell(rgbw[0]);
            cell[1] = quantize_lut_cell(rgbw[1]);
            cell[2] = quantize_lut_cell(rgbw[2]);
            cell[3] = quantize_lut_cell(rgbw[3]);
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
    const i16* c00 = &base[(j * N + i) * 4];
    const i16* c10 = &base[(j * N + i + 1) * 4];
    const i16* c01 = &base[((j + 1) * N + i) * 4];
    const i16* c11 = &base[((j + 1) * N + i + 1) * 4];

    const float w00 = (1 - fx) * (1 - fy);
    const float w10 = fx * (1 - fy);
    const float w01 = (1 - fx) * fy;
    const float w11 = fx * fy;
    const float inv_Q = 1.0f / static_cast<float>(kLutQ);

    for (int k = 0; k < 4; ++k) {
        const float per_Y = (w00 * c00[k] + w10 * c10[k]
                           + w01 * c01[k] + w11 * c11[k]) * inv_Q;
        float t = per_Y * Y_t;
        if (t < 0.0f) t = 0.0f;
        out_rgbw[k] = t;
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
