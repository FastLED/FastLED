// Strategy registry shared by the #3422 colorimetric harness tiers.
//
// Every proposed approximation strategy, its packed-memory model, the
// kStrategies registry and the metadata the readiness checks read. Two
// translation units include it:
//   tests/unit/misc/colorimetric_strategy_checks.cpp: fast harness and
//     production-path checks, run by every `bash test`;
//   tests/integration/misc/colorimetric_strategy_table.cpp: the full
//     research comparison table, run by `bash test --integration`.

#pragma once

#include "test.h"

#include "fl/gfx/rgbw.h"
#include "fl/gfx/rgbw_colorimetric.h"
#include "fl/math/math.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/chrono.h"
#include "fl/stl/cstdlib.h"
#include "fl/stl/unique_ptr.h"
#include "tests/unit/misc/colorimetric_strategy_harness.hpp"
#include "tests/unit/misc/golden/verifier_known_bad.hpp"

namespace test {

using namespace fl;
using namespace fl::colorimetric_detail;
using namespace fl::colorimetric_test;
using namespace fl::colorimetric_test::fixture;

struct Q16RangeStats {
    bool measured = false;
    i32 sample_count = 0;
    i32 fallback_count = 0;
    i32 seeded_fallback_count = 0;
    i64 max_abs_src_q = 0;
    i64 max_abs_coeff_q = 0;
    i64 max_abs_product = 0;
    i64 max_abs_accumulator = 0;
    i64 max_abs_output_q = 0;
    i64 max_abs_det = 0;
    i64 min_abs_det = 0;
    i64 max_abs_numerator = 0;
    i64 max_abs_endpoint_q = 0;
    i64 min_abs_divisor = 0;
    i32 divisor_count = 0;
    i32 zero_det_count = 0;
    i32 zero_divisor_count = 0;
};

namespace strategy_common {

inline void build_default_cache(const DiodeProfile& profile, ProfileCache* cache) {
    build_profile_cache(&profile, 0, cache);
}

inline void quantize_rgbw(const float in[4],
                          u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    *out_r = quantize_u8(in[0]);
    *out_g = quantize_u8(in[1]);
    *out_b = quantize_u8(in[2]);
    *out_w = quantize_u8(in[3]);
}

inline void exact_fallback(u8 r, u8 g, u8 b,
                           u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    rgb_2_rgbw_exact(kRGBWDefaultColorTemp, r, g, b, 255, 255, 255,
                     out_r, out_g, out_b, out_w);
}

inline void solve_strict_u8(const ProfileCache& cache, u8 r, u8 g, u8 b,
                            u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    if ((r | g | b) == 0) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }
    const float s_r = r * (1.0f / 255.0f);
    const float s_g = g * (1.0f / 255.0f);
    const float s_b = b * (1.0f / 255.0f);
    float rgbw[4] = {0, 0, 0, 0};
    if (!solve_strict_subgamut(cache, s_r, s_g, s_b, rgbw)) {
        exact_fallback(r, g, b, out_r, out_g, out_b, out_w);
        return;
    }
    quantize_rgbw(rgbw, out_r, out_g, out_b, out_w);
}

inline bool solve_native_topology_before_lut(const ProfileCache& cache,
                                             u8 r, u8 g, u8 b,
                                             u8* out_r, u8* out_g,
                                             u8* out_b, u8* out_w) {
    const float s_r = r * (1.0f / 255.0f);
    const float s_g = g * (1.0f / 255.0f);
    const float s_b = b * (1.0f / 255.0f);
    if (is_native_input_gamut(*cache.profile)
        && count_active_channels(s_r, s_g, s_b) < 3) {
        solve_strict_u8(cache, r, g, b, out_r, out_g, out_b, out_w);
        return true;
    }
    return false;
}

inline bool source_to_xyY(const ProfileCache& cache, u8 r, u8 g, u8 b,
                          float xy[2], float* Y) {
    if ((r | g | b) == 0) {
        xy[0] = xy[1] = 0.0f;
        *Y = 0.0f;
        return false;
    }
    const float s[3] = {
        r * (1.0f / 255.0f),
        g * (1.0f / 255.0f),
        b * (1.0f / 255.0f),
    };
    float X_t[3];
    if (cache.has_source_space) {
        matvec3(cache.M_src, s, X_t);
    } else {
        X_t[0] = cache.P_R[0] * s[0] + cache.P_G[0] * s[1] + cache.P_B[0] * s[2];
        X_t[1] = cache.P_R[1] * s[0] + cache.P_G[1] * s[1] + cache.P_B[1] * s[2];
        X_t[2] = cache.P_R[2] * s[0] + cache.P_G[2] * s[1] + cache.P_B[2] * s[2];
    }
    const float sum = X_t[0] + X_t[1] + X_t[2];
    if (sum <= 1e-9f) {
        xy[0] = xy[1] = 0.0f;
        *Y = 0.0f;
        return false;
    }
    xy[0] = X_t[0] / sum;
    xy[1] = X_t[1] / sum;
    *Y = X_t[1];
    return true;
}

inline void normalize4(float out[4]) {
    const float m = fl::max(fl::max(out[0], out[1]), fl::max(out[2], out[3]));
    if (m > 1.0f) {
        const float inv = 1.0f / m;
        out[0] *= inv;
        out[1] *= inv;
        out[2] *= inv;
        out[3] *= inv;
    }
}

inline i32 quantize_q(float v, int q) {
    const double scaled = static_cast<double>(v) * static_cast<double>(q);
    const double rounded = scaled >= 0.0 ? scaled + 0.5 : scaled - 0.5;
    if (rounded > 2147483647.0) return 2147483647;
    if (rounded < -2147483647.0) return -2147483647;
    return static_cast<i32>(rounded);
}

inline i64 abs_i64(i64 v) {
    return v < 0 ? -v : v;
}

inline void track_abs(i64 value, i64* dst) {
    const i64 a = abs_i64(value);
    if (a > *dst) {
        *dst = a;
    }
}

inline void track_divisor(i64 divisor, Q16RangeStats& stats) {
    ++stats.divisor_count;
    const i64 a = abs_i64(divisor);
    if (a == 0) {
        ++stats.zero_divisor_count;
        return;
    }
    if (stats.min_abs_divisor == 0 || a < stats.min_abs_divisor) {
        stats.min_abs_divisor = a;
    }
}

inline void track_det(i64 det, Q16RangeStats& stats) {
    track_abs(det, &stats.max_abs_det);
    const i64 a = abs_i64(det);
    if (a == 0) {
        ++stats.zero_det_count;
        return;
    }
    if (stats.min_abs_det == 0 || a < stats.min_abs_det) {
        stats.min_abs_det = a;
    }
}

inline void track_normalize4_divisor(const i32 out[4], Q16RangeStats& stats) {
    i32 m = out[0];
    if (out[1] > m) m = out[1];
    if (out[2] > m) m = out[2];
    if (out[3] > m) m = out[3];
    if (m > 65536) {
        track_divisor(m, stats);
    }
}

inline void track_normalize5_divisor(const i32 out[5], Q16RangeStats& stats) {
    i32 m = out[0];
    for (int i = 1; i < 5; ++i) {
        if (out[i] > m) m = out[i];
    }
    if (m > 65536) {
        track_divisor(m, stats);
    }
}

}  // namespace strategy_common

namespace strategy_closed_form {

static ProfileCache g_cache;

inline void setup(const DiodeProfile& profile) {
    strategy_common::build_default_cache(profile, &g_cache);
}

inline void solve(u8 r, u8 g, u8 b, u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    strategy_common::solve_strict_u8(g_cache, r, g, b, out_r, out_g, out_b, out_w);
}

inline void teardown() {}

}  // namespace strategy_closed_form

namespace strategy_wx_lp_legacy {

static ProfileCache g_cache;

inline void setup(const DiodeProfile& profile) {
    strategy_common::build_default_cache(profile, &g_cache);
}

inline void solve(u8 r, u8 g, u8 b, u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    if ((r | g | b) == 0) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }
    const float s_r = r * (1.0f / 255.0f);
    const float s_g = g * (1.0f / 255.0f);
    const float s_b = b * (1.0f / 255.0f);
    float rgbw[4] = {0, 0, 0, 0};
    if (!solve_wx_lp_legacy(g_cache, s_r, s_g, s_b, rgbw)) {
        strategy_common::exact_fallback(r, g, b, out_r, out_g, out_b, out_w);
        return;
    }
    strategy_common::quantize_rgbw(rgbw, out_r, out_g, out_b, out_w);
}

inline void teardown() {}

}  // namespace strategy_wx_lp_legacy

namespace strategy_wx_overdrive {

static ProfileCache g_cache;

inline void setup(const DiodeProfile& profile) {
    strategy_common::build_default_cache(profile, &g_cache);
}

inline void solve(u8 r, u8 g, u8 b, u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    if ((r | g | b) == 0) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }
    const float s_r = r * (1.0f / 255.0f);
    const float s_g = g * (1.0f / 255.0f);
    const float s_b = b * (1.0f / 255.0f);
    float rgbw[4] = {0, 0, 0, 0};
    solve_wx_overdrive(g_cache, s_r, s_g, s_b, kDefaultOverdriveRatio, rgbw);
    strategy_common::quantize_rgbw(rgbw, out_r, out_g, out_b, out_w);
}

inline void teardown() {}

}  // namespace strategy_wx_overdrive

namespace strategy_wx_overdrive_q16 {

static constexpr i32 kQ = 65536;
static constexpr i32 kRhoQ = kQ / 2;

struct State {
    bool native_input = false;
    bool has_source_space = false;
    i32 P[4][3] = {};
    i32 M_src[3][3] = {};
    i32 P_RGB_inv[3][3] = {};
    i32 d_W[3] = {};
};

static State g_state;

inline i32 q(float v) {
    return strategy_common::quantize_q(v, kQ);
}

inline i64 mul_q(i64 a, i64 b) {
    return (a * b) / kQ;
}

inline i32 clamp_q(i64 v) {
    if (v < 0) return 0;
    if (v > kQ) return kQ;
    return static_cast<i32>(v);
}

inline void matvec_q(const i32 M[3][3], const i32 in[3], i32 out[3]) {
    for (int row = 0; row < 3; ++row) {
        i64 acc = 0;
        for (int col = 0; col < 3; ++col) {
            acc += static_cast<i64>(M[row][col]) * in[col];
        }
        out[row] = static_cast<i32>(acc / kQ);
    }
}

inline void source_full_chroma_q(u8 r, u8 g, u8 b, u8 value, i32 X[3]) {
    const i32 src[3] = {
        static_cast<i32>((static_cast<i64>(r) * kQ) / value),
        static_cast<i32>((static_cast<i64>(g) * kQ) / value),
        static_cast<i32>((static_cast<i64>(b) * kQ) / value),
    };
    if (g_state.has_source_space) {
        matvec_q(g_state.M_src, src, X);
        return;
    }
    for (int row = 0; row < 3; ++row) {
        const i64 acc =
              static_cast<i64>(g_state.P[0][row]) * src[0]
            + static_cast<i64>(g_state.P[1][row]) * src[1]
            + static_cast<i64>(g_state.P[2][row]) * src[2];
        X[row] = static_cast<i32>(acc / kQ);
    }
}

inline void normalize4_q(i32 out[4]) {
    i32 m = out[0];
    if (out[1] > m) m = out[1];
    if (out[2] > m) m = out[2];
    if (out[3] > m) m = out[3];
    if (m <= kQ) return;
    for (int i = 0; i < 4; ++i) {
        out[i] = static_cast<i32>((static_cast<i64>(out[i]) * kQ) / m);
    }
}

inline u8 q_to_scaled_u8(i32 qv, u8 value) {
    const i64 v = static_cast<i64>(qv) * static_cast<i64>(value);
    const i64 rounded = (v + (kQ / 2)) / kQ;
    if (rounded <= 0) return 0;
    if (rounded >= 255) return 255;
    return static_cast<u8>(rounded);
}

inline bool solve_ls2_endpoint_q(const i32 X[3], const int active[2], i32 out[4]) {
    out[0] = out[1] = out[2] = out[3] = 0;
    const i32* A = g_state.P[active[0]];
    const i32* B = g_state.P[active[1]];
    i64 aa = 0, ab = 0, bb = 0, ax = 0, bx = 0;
    for (int i = 0; i < 3; ++i) {
        aa += mul_q(A[i], A[i]);
        ab += mul_q(A[i], B[i]);
        bb += mul_q(B[i], B[i]);
        ax += mul_q(A[i], X[i]);
        bx += mul_q(B[i], X[i]);
    }
    const i64 det = aa * bb - ab * ab;
    if (det == 0) return false;
    i64 t0 = ((ax * bb - bx * ab) * kQ) / det;
    i64 t1 = ((bx * aa - ax * ab) * kQ) / det;
    if (t0 < 0 && t1 >= 0) {
        t0 = 0;
        t1 = bb > 0 ? (bx * kQ) / bb : 0;
    } else if (t1 < 0 && t0 >= 0) {
        t1 = 0;
        t0 = aa > 0 ? (ax * kQ) / aa : 0;
    } else if (t0 < 0 && t1 < 0) {
        t0 = 0;
        t1 = 0;
    }
    out[active[0]] = clamp_q(t0);
    out[active[1]] = clamp_q(t1);
    normalize4_q(out);
    return true;
}

inline void setup(const DiodeProfile& profile) {
    ProfileCache cache;
    strategy_common::build_default_cache(profile, &cache);
    g_state.native_input = is_native_input_gamut(profile);
    g_state.has_source_space = cache.has_source_space;
    const float* cols[4] = {cache.P_R, cache.P_G, cache.P_B, cache.P_W};
    for (int ch = 0; ch < 4; ++ch) {
        for (int row = 0; row < 3; ++row) {
            g_state.P[ch][row] = q(cols[ch][row]);
        }
    }
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            g_state.M_src[row][col] = q(cache.M_src[row][col]);
            g_state.P_RGB_inv[row][col] = q(cache.P_RGB_inv[row][col]);
        }
        g_state.d_W[row] = q(cache.d_W[row]);
    }
}

inline void solve_with_rho_q(i32 rho_q, u8 r, u8 g, u8 b,
                             u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    const u8 value = fl::max(fl::max(r, g), b);
    if (value == 0) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }
    const int active_count = (r > 0 ? 1 : 0) + (g > 0 ? 1 : 0) + (b > 0 ? 1 : 0);
    if (g_state.native_input && active_count == 1) {
        *out_r = r;
        *out_g = g;
        *out_b = b;
        *out_w = 0;
        return;
    }

    i32 X[3];
    source_full_chroma_q(r, g, b, value, X);
    i32 endpoint[4];
    if (g_state.native_input && active_count == 2) {
        int active[2];
        int n = 0;
        if (r > 0) active[n++] = 0;
        if (g > 0) active[n++] = 1;
        if (b > 0) active[n++] = 2;
        if (!solve_ls2_endpoint_q(X, active, endpoint)) {
            endpoint[0] = endpoint[1] = endpoint[2] = endpoint[3] = 0;
        }
    } else {
        i32 a[3];
        matvec_q(g_state.P_RGB_inv, X, a);
        i32 w_strict = kQ;
        for (int i = 0; i < 3; ++i) {
            if (g_state.d_W[i] > 0 && a[i] >= 0) {
                const i64 lim = (static_cast<i64>(a[i]) * kQ) / g_state.d_W[i];
                if (lim < w_strict) {
                    w_strict = static_cast<i32>(lim);
                }
            }
        }
        w_strict = clamp_q(w_strict);
        const i32 w = clamp_q(w_strict
            + ((static_cast<i64>(rho_q) * (kQ - w_strict)) / kQ));
        endpoint[0] = clamp_q(static_cast<i64>(a[0]) - mul_q(w, g_state.d_W[0]));
        endpoint[1] = clamp_q(static_cast<i64>(a[1]) - mul_q(w, g_state.d_W[1]));
        endpoint[2] = clamp_q(static_cast<i64>(a[2]) - mul_q(w, g_state.d_W[2]));
        endpoint[3] = w;
        normalize4_q(endpoint);
    }

    *out_r = q_to_scaled_u8(endpoint[0], value);
    *out_g = q_to_scaled_u8(endpoint[1], value);
    *out_b = q_to_scaled_u8(endpoint[2], value);
    *out_w = q_to_scaled_u8(endpoint[3], value);
}

inline void solve(u8 r, u8 g, u8 b, u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    solve_with_rho_q(kRhoQ, r, g, b, out_r, out_g, out_b, out_w);
}

inline void track_state_coefficients(Q16RangeStats& stats) {
    for (int ch = 0; ch < 4; ++ch) {
        for (int row = 0; row < 3; ++row) {
            strategy_common::track_abs(g_state.P[ch][row],
                                       &stats.max_abs_coeff_q);
        }
    }
    for (int row = 0; row < 3; ++row) {
        strategy_common::track_abs(g_state.d_W[row], &stats.max_abs_coeff_q);
        for (int col = 0; col < 3; ++col) {
            strategy_common::track_abs(g_state.M_src[row][col],
                                       &stats.max_abs_coeff_q);
            strategy_common::track_abs(g_state.P_RGB_inv[row][col],
                                       &stats.max_abs_coeff_q);
        }
    }
}

inline i64 mul_q_audit(i64 a, i64 b, Q16RangeStats& stats) {
    const i64 product = a * b;
    strategy_common::track_abs(product, &stats.max_abs_product);
    return product / kQ;
}

inline void matvec_q_audit(const i32 M[3][3],
                           const i32 in[3],
                           i32 out[3],
                           Q16RangeStats& stats) {
    for (int row = 0; row < 3; ++row) {
        i64 acc = 0;
        for (int col = 0; col < 3; ++col) {
            const i64 product =
                static_cast<i64>(M[row][col]) * static_cast<i64>(in[col]);
            strategy_common::track_abs(product, &stats.max_abs_product);
            acc += product;
            strategy_common::track_abs(acc, &stats.max_abs_accumulator);
        }
        out[row] = static_cast<i32>(acc / kQ);
        strategy_common::track_abs(out[row], &stats.max_abs_output_q);
    }
}

inline void source_full_chroma_q_audit(u8 r,
                                       u8 g,
                                       u8 b,
                                       u8 value,
                                       i32 X[3],
                                       Q16RangeStats& stats) {
    strategy_common::track_divisor(value, stats);
    const i32 src[3] = {
        static_cast<i32>((static_cast<i64>(r) * kQ) / value),
        static_cast<i32>((static_cast<i64>(g) * kQ) / value),
        static_cast<i32>((static_cast<i64>(b) * kQ) / value),
    };
    for (int i = 0; i < 3; ++i) {
        strategy_common::track_abs(src[i], &stats.max_abs_src_q);
    }
    if (g_state.has_source_space) {
        matvec_q_audit(g_state.M_src, src, X, stats);
        return;
    }
    for (int row = 0; row < 3; ++row) {
        i64 acc = 0;
        for (int ch = 0; ch < 3; ++ch) {
            const i64 product =
                static_cast<i64>(g_state.P[ch][row]) * src[ch];
            strategy_common::track_abs(product, &stats.max_abs_product);
            acc += product;
            strategy_common::track_abs(acc, &stats.max_abs_accumulator);
        }
        X[row] = static_cast<i32>(acc / kQ);
        strategy_common::track_abs(X[row], &stats.max_abs_output_q);
    }
}

inline void track_endpoint4(const i32 out[4], Q16RangeStats& stats) {
    for (int i = 0; i < 4; ++i) {
        strategy_common::track_abs(out[i], &stats.max_abs_endpoint_q);
        strategy_common::track_abs(out[i], &stats.max_abs_output_q);
    }
}

inline bool solve_ls2_endpoint_q_audit(const i32 X[3],
                                       const int active[2],
                                       i32 out[4],
                                       Q16RangeStats& stats) {
    out[0] = out[1] = out[2] = out[3] = 0;
    const i32* A = g_state.P[active[0]];
    const i32* B = g_state.P[active[1]];
    i64 aa = 0, ab = 0, bb = 0, ax = 0, bx = 0;
    for (int i = 0; i < 3; ++i) {
        aa += mul_q_audit(A[i], A[i], stats);
        ab += mul_q_audit(A[i], B[i], stats);
        bb += mul_q_audit(B[i], B[i], stats);
        ax += mul_q_audit(A[i], X[i], stats);
        bx += mul_q_audit(B[i], X[i], stats);
        strategy_common::track_abs(aa, &stats.max_abs_accumulator);
        strategy_common::track_abs(ab, &stats.max_abs_accumulator);
        strategy_common::track_abs(bb, &stats.max_abs_accumulator);
        strategy_common::track_abs(ax, &stats.max_abs_accumulator);
        strategy_common::track_abs(bx, &stats.max_abs_accumulator);
    }
    const i64 det = aa * bb - ab * ab;
    strategy_common::track_det(det, stats);
    if (det == 0) return false;
    strategy_common::track_divisor(det, stats);

    const i64 num0 = ax * bb - bx * ab;
    const i64 num1 = bx * aa - ax * ab;
    const i64 scaled0 = num0 * kQ;
    const i64 scaled1 = num1 * kQ;
    strategy_common::track_abs(scaled0, &stats.max_abs_numerator);
    strategy_common::track_abs(scaled1, &stats.max_abs_numerator);
    i64 t0 = scaled0 / det;
    i64 t1 = scaled1 / det;
    strategy_common::track_abs(t0, &stats.max_abs_output_q);
    strategy_common::track_abs(t1, &stats.max_abs_output_q);
    if (t0 < 0 && t1 >= 0) {
        t0 = 0;
        if (bb > 0) {
            strategy_common::track_divisor(bb, stats);
            t1 = (bx * kQ) / bb;
        } else {
            t1 = 0;
        }
    } else if (t1 < 0 && t0 >= 0) {
        t1 = 0;
        if (aa > 0) {
            strategy_common::track_divisor(aa, stats);
            t0 = (ax * kQ) / aa;
        } else {
            t0 = 0;
        }
    } else if (t0 < 0 && t1 < 0) {
        t0 = 0;
        t1 = 0;
    }
    strategy_common::track_abs(t0, &stats.max_abs_output_q);
    strategy_common::track_abs(t1, &stats.max_abs_output_q);
    out[active[0]] = clamp_q(t0);
    out[active[1]] = clamp_q(t1);
    track_endpoint4(out, stats);
    strategy_common::track_normalize4_divisor(out, stats);
    normalize4_q(out);
    track_endpoint4(out, stats);
    return true;
}

inline Q16RangeStats audit_with_rho_q(const DiodeProfile& profile,
                                      int cube_steps,
                                      i32 rho_q) {
    setup(profile);
    Q16RangeStats stats{};
    if (cube_steps <= 1) return stats;
    stats.measured = true;
    track_state_coefficients(stats);

    for (int ri = 0; ri < cube_steps; ++ri) {
        const u8 r = cube_value(ri, cube_steps);
        for (int gi = 0; gi < cube_steps; ++gi) {
            const u8 g = cube_value(gi, cube_steps);
            for (int bi = 0; bi < cube_steps; ++bi) {
                const u8 b = cube_value(bi, cube_steps);
                ++stats.sample_count;
                const u8 value = fl::max(fl::max(r, g), b);
                if (value == 0) continue;
                const int active_count =
                    (r > 0 ? 1 : 0) + (g > 0 ? 1 : 0) + (b > 0 ? 1 : 0);
                if (g_state.native_input && active_count == 1) {
                    continue;
                }

                i32 X[3];
                source_full_chroma_q_audit(r, g, b, value, X, stats);
                i32 endpoint[4];
                if (g_state.native_input && active_count == 2) {
                    int active[2];
                    int n = 0;
                    if (r > 0) active[n++] = 0;
                    if (g > 0) active[n++] = 1;
                    if (b > 0) active[n++] = 2;
                    if (!solve_ls2_endpoint_q_audit(X, active, endpoint, stats)) {
                        ++stats.fallback_count;
                    }
                    continue;
                }

                i32 a[3];
                matvec_q_audit(g_state.P_RGB_inv, X, a, stats);
                i32 w_strict = kQ;
                for (int i = 0; i < 3; ++i) {
                    if (g_state.d_W[i] > 0 && a[i] >= 0) {
                        const i64 numerator = static_cast<i64>(a[i]) * kQ;
                        strategy_common::track_abs(
                            numerator, &stats.max_abs_numerator);
                        strategy_common::track_divisor(g_state.d_W[i], stats);
                        const i64 lim = numerator / g_state.d_W[i];
                        strategy_common::track_abs(lim, &stats.max_abs_output_q);
                        if (lim < w_strict) {
                            w_strict = static_cast<i32>(lim);
                        }
                    }
                }
                w_strict = clamp_q(w_strict);
                const i64 w_delta =
                    (static_cast<i64>(rho_q) * (kQ - w_strict)) / kQ;
                strategy_common::track_abs(w_delta, &stats.max_abs_output_q);
                const i32 w = clamp_q(w_strict + w_delta);
                endpoint[0] = clamp_q(
                    static_cast<i64>(a[0]) - mul_q_audit(w, g_state.d_W[0], stats));
                endpoint[1] = clamp_q(
                    static_cast<i64>(a[1]) - mul_q_audit(w, g_state.d_W[1], stats));
                endpoint[2] = clamp_q(
                    static_cast<i64>(a[2]) - mul_q_audit(w, g_state.d_W[2], stats));
                endpoint[3] = w;
                track_endpoint4(endpoint, stats);
                strategy_common::track_normalize4_divisor(endpoint, stats);
                normalize4_q(endpoint);
                track_endpoint4(endpoint, stats);
            }
        }
    }
    return stats;
}

inline Q16RangeStats audit_range(const DiodeProfile& profile, int cube_steps) {
    return audit_with_rho_q(profile, cube_steps, kRhoQ);
}

inline void teardown() {}

}  // namespace strategy_wx_overdrive_q16

namespace strategy_wx_lp_q16_residual {

inline void setup(const DiodeProfile& profile) {
    strategy_wx_overdrive_q16::setup(profile);
}

inline void solve(u8 r, u8 g, u8 b, u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    strategy_wx_overdrive_q16::solve_with_rho_q(
        0, r, g, b, out_r, out_g, out_b, out_w);
}

inline Q16RangeStats audit_range(const DiodeProfile& profile, int cube_steps) {
    return strategy_wx_overdrive_q16::audit_with_rho_q(profile, cube_steps, 0);
}

inline void teardown() {
    strategy_wx_overdrive_q16::teardown();
}

}  // namespace strategy_wx_lp_q16_residual

namespace strategy_wx_lp_q16_balanced {

using strategy_wx_overdrive_q16::kQ;

inline void setup(const DiodeProfile& profile) {
    strategy_wx_overdrive_q16::setup(profile);
}

inline i32 clamp_q(i64 v) {
    return strategy_wx_overdrive_q16::clamp_q(v);
}

inline i32 mul_pair_q(i32 a, i32 b) {
    return static_cast<i32>((static_cast<i64>(a) * b) / kQ);
}

inline bool solve_balanced_fraction_for_xy_q(i32 x_q,
                                             i32 y_q,
                                             i32 out[4],
                                             Q16RangeStats* stats = nullptr) {
    out[0] = out[1] = out[2] = out[3] = 0;
    if (x_q <= 0 || y_q <= 0) return false;

    i32 A[2][4];
    const auto& state = strategy_wx_overdrive_q16::g_state;
    for (int ch = 0; ch < 4; ++ch) {
        const i32 s = state.P[ch][0] + state.P[ch][1] + state.P[ch][2];
        if (stats != nullptr) {
            strategy_common::track_abs(s, &stats->max_abs_accumulator);
            strategy_common::track_abs(static_cast<i64>(x_q) * s,
                                       &stats->max_abs_product);
            strategy_common::track_abs(static_cast<i64>(y_q) * s,
                                       &stats->max_abs_product);
        }
        A[0][ch] = state.P[ch][0] - mul_pair_q(x_q, s);
        A[1][ch] = state.P[ch][1] - mul_pair_q(y_q, s);
        if (stats != nullptr) {
            strategy_common::track_abs(A[0][ch], &stats->max_abs_output_q);
            strategy_common::track_abs(A[1][ch], &stats->max_abs_output_q);
        }
    }

    i32 y_max = state.P[0][1];
    for (int ch = 1; ch < 4; ++ch) {
        if (state.P[ch][1] > y_max) y_max = state.P[ch][1];
    }

    bool found = false;
    i64 best_obj = -1;
    i32 best[4] = {0, 0, 0, 0};
    const i32 floors[4] = {kQ / 1024, kQ / 2048, kQ / 4096, 0};
    constexpr i32 kResidualEpsQ = 33;  // 5e-4 in Q16.
    constexpr i32 kBoundEpsQ = 2;

    for (int fidx = 0; fidx < 4 && !found; ++fidx) {
        const i32 lo = floors[fidx];
        const i32 hi = kQ;
        for (int free0 = 0; free0 < 4; ++free0) {
            for (int free1 = free0 + 1; free1 < 4; ++free1) {
                int fixed[2];
                int nf = 0;
                for (int i = 0; i < 4; ++i) {
                    if (i != free0 && i != free1) fixed[nf++] = i;
                }
                for (int b0 = 0; b0 < 2; ++b0) {
                    for (int b1 = 0; b1 < 2; ++b1) {
                        i32 cand[4] = {0, 0, 0, 0};
                        cand[fixed[0]] = b0 ? hi : lo;
                        cand[fixed[1]] = b1 ? hi : lo;

                        const i64 fixed0_0_a =
                            static_cast<i64>(A[0][fixed[0]]) * cand[fixed[0]];
                        const i64 fixed0_0_b =
                            static_cast<i64>(A[0][fixed[1]]) * cand[fixed[1]];
                        const i64 fixed0_1_a =
                            static_cast<i64>(A[1][fixed[0]]) * cand[fixed[0]];
                        const i64 fixed0_1_b =
                            static_cast<i64>(A[1][fixed[1]]) * cand[fixed[1]];
                        const i64 fixed0_0 = fixed0_0_a + fixed0_0_b;
                        const i64 fixed0_1 = fixed0_1_a + fixed0_1_b;
                        if (stats != nullptr) {
                            strategy_common::track_abs(
                                fixed0_0_a, &stats->max_abs_product);
                            strategy_common::track_abs(
                                fixed0_0_b, &stats->max_abs_product);
                            strategy_common::track_abs(
                                fixed0_1_a, &stats->max_abs_product);
                            strategy_common::track_abs(
                                fixed0_1_b, &stats->max_abs_product);
                            strategy_common::track_abs(
                                fixed0_0, &stats->max_abs_accumulator);
                            strategy_common::track_abs(
                                fixed0_1, &stats->max_abs_accumulator);
                        }
                        const i64 rhs0 = -fixed0_0 / kQ;
                        const i64 rhs1 = -fixed0_1 / kQ;
                        const i64 a00 = A[0][free0];
                        const i64 a01 = A[0][free1];
                        const i64 a10 = A[1][free0];
                        const i64 a11 = A[1][free1];
                        const i64 det = a00 * a11 - a01 * a10;
                        if (stats != nullptr) {
                            strategy_common::track_det(det, *stats);
                        }
                        if (det == 0) continue;
                        if (stats != nullptr) {
                            strategy_common::track_divisor(det, *stats);
                        }

                        const i64 num0 = rhs0 * a11 - a01 * rhs1;
                        const i64 num1 = a00 * rhs1 - rhs0 * a10;
                        const i64 scaled0 = num0 * kQ;
                        const i64 scaled1 = num1 * kQ;
                        if (stats != nullptr) {
                            strategy_common::track_abs(
                                rhs0 * a11, &stats->max_abs_product);
                            strategy_common::track_abs(
                                a01 * rhs1, &stats->max_abs_product);
                            strategy_common::track_abs(
                                a00 * rhs1, &stats->max_abs_product);
                            strategy_common::track_abs(
                                rhs0 * a10, &stats->max_abs_product);
                            strategy_common::track_abs(
                                scaled0, &stats->max_abs_numerator);
                            strategy_common::track_abs(
                                scaled1, &stats->max_abs_numerator);
                        }
                        cand[free0] = static_cast<i32>(
                            scaled0 / det);
                        cand[free1] = static_cast<i32>(
                            scaled1 / det);
                        if (stats != nullptr) {
                            strategy_common::track_abs(
                                cand[free0], &stats->max_abs_output_q);
                            strategy_common::track_abs(
                                cand[free1], &stats->max_abs_output_q);
                        }

                        bool ok = true;
                        for (int i = 0; i < 4; ++i) {
                            if (cand[i] < lo - kBoundEpsQ
                                || cand[i] > hi + kBoundEpsQ) {
                                ok = false;
                                break;
                            }
                            cand[i] = clamp_q(cand[i]);
                            if (cand[i] < lo) cand[i] = lo;
                            if (cand[i] > hi) cand[i] = hi;
                        }
                        if (!ok) continue;

                        const i64 residual0 =
                            static_cast<i64>(A[0][0]) * cand[0]
                          + static_cast<i64>(A[0][1]) * cand[1]
                          + static_cast<i64>(A[0][2]) * cand[2]
                          + static_cast<i64>(A[0][3]) * cand[3];
                        const i64 residual1 =
                            static_cast<i64>(A[1][0]) * cand[0]
                          + static_cast<i64>(A[1][1]) * cand[1]
                          + static_cast<i64>(A[1][2]) * cand[2]
                          + static_cast<i64>(A[1][3]) * cand[3];
                        if (stats != nullptr) {
                            strategy_common::track_abs(
                                residual0, &stats->max_abs_accumulator);
                            strategy_common::track_abs(
                                residual1, &stats->max_abs_accumulator);
                        }
                        const i64 r0 = residual0 / kQ;
                        const i64 r1 = residual1 / kQ;
                        if (fl::abs(static_cast<i32>(r0)) > kResidualEpsQ
                            || fl::abs(static_cast<i32>(r1)) > kResidualEpsQ) {
                            continue;
                        }

                        const i64 y_score = y_max > 0
                            ? (static_cast<i64>(state.P[0][1]) * cand[0]
                             + static_cast<i64>(state.P[1][1]) * cand[1]
                             + static_cast<i64>(state.P[2][1]) * cand[2]
                             + static_cast<i64>(state.P[3][1]) * cand[3]) / y_max
                            : 0;
                        if (stats != nullptr && y_max > 0) {
                            strategy_common::track_divisor(y_max, *stats);
                        }
                        if (stats != nullptr) {
                            strategy_common::track_abs(
                                y_score, &stats->max_abs_output_q);
                        }
                        const i64 obj =
                            static_cast<i64>(cand[3]) * 1000000 + y_score;
                        if (stats != nullptr) {
                            strategy_common::track_abs(
                                obj, &stats->max_abs_accumulator);
                        }
                        if (!found || obj > best_obj) {
                            found = true;
                            best_obj = obj;
                            best[0] = cand[0];
                            best[1] = cand[1];
                            best[2] = cand[2];
                            best[3] = cand[3];
                        }
                    }
                }
            }
        }
    }

    if (!found) return false;
    i32 m = best[0];
    if (best[1] > m) m = best[1];
    if (best[2] > m) m = best[2];
    if (best[3] > m) m = best[3];
    if (m <= 0) return false;
    if (stats != nullptr) {
        strategy_common::track_divisor(m, *stats);
    }
    for (int i = 0; i < 4; ++i) {
        out[i] = static_cast<i32>((static_cast<i64>(best[i]) * kQ) / m);
    }
    if (stats != nullptr) {
        strategy_wx_overdrive_q16::track_endpoint4(out, *stats);
    }
    return true;
}

inline void solve(u8 r, u8 g, u8 b, u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    const u8 value = fl::max(fl::max(r, g), b);
    if (value == 0) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }
    const auto& state = strategy_wx_overdrive_q16::g_state;
    const int active_count = (r > 0 ? 1 : 0) + (g > 0 ? 1 : 0) + (b > 0 ? 1 : 0);
    if (state.native_input && active_count < 3) {
        strategy_wx_overdrive_q16::solve_with_rho_q(
            0, r, g, b, out_r, out_g, out_b, out_w);
        return;
    }

    i32 X[3];
    strategy_wx_overdrive_q16::source_full_chroma_q(r, g, b, value, X);
    const i64 sum = static_cast<i64>(X[0]) + X[1] + X[2];
    if (sum <= 0) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }
    const i32 x_q = static_cast<i32>((static_cast<i64>(X[0]) * kQ) / sum);
    const i32 y_q = static_cast<i32>((static_cast<i64>(X[1]) * kQ) / sum);
    i32 endpoint[4];
    if (!solve_balanced_fraction_for_xy_q(x_q, y_q, endpoint)) {
        strategy_wx_overdrive_q16::solve_with_rho_q(
            0, r, g, b, out_r, out_g, out_b, out_w);
        return;
    }
    *out_r = strategy_wx_overdrive_q16::q_to_scaled_u8(endpoint[0], value);
    *out_g = strategy_wx_overdrive_q16::q_to_scaled_u8(endpoint[1], value);
    *out_b = strategy_wx_overdrive_q16::q_to_scaled_u8(endpoint[2], value);
    *out_w = strategy_wx_overdrive_q16::q_to_scaled_u8(endpoint[3], value);
}

inline Q16RangeStats audit_range(const DiodeProfile& profile, int cube_steps) {
    setup(profile);
    Q16RangeStats stats{};
    if (cube_steps <= 1) return stats;
    stats.measured = true;
    strategy_wx_overdrive_q16::track_state_coefficients(stats);
    const auto& state = strategy_wx_overdrive_q16::g_state;

    for (int ri = 0; ri < cube_steps; ++ri) {
        const u8 r = cube_value(ri, cube_steps);
        for (int gi = 0; gi < cube_steps; ++gi) {
            const u8 g = cube_value(gi, cube_steps);
            for (int bi = 0; bi < cube_steps; ++bi) {
                const u8 b = cube_value(bi, cube_steps);
                ++stats.sample_count;
                const u8 value = fl::max(fl::max(r, g), b);
                if (value == 0) continue;
                const int active_count =
                    (r > 0 ? 1 : 0) + (g > 0 ? 1 : 0) + (b > 0 ? 1 : 0);
                if (state.native_input && active_count == 1) {
                    continue;
                }

                i32 X[3];
                strategy_wx_overdrive_q16::source_full_chroma_q_audit(
                    r, g, b, value, X, stats);
                if (state.native_input && active_count == 2) {
                    int active[2];
                    int n = 0;
                    if (r > 0) active[n++] = 0;
                    if (g > 0) active[n++] = 1;
                    if (b > 0) active[n++] = 2;
                    i32 endpoint[4];
                    if (!strategy_wx_overdrive_q16::solve_ls2_endpoint_q_audit(
                            X, active, endpoint, stats)) {
                        ++stats.fallback_count;
                    }
                    continue;
                }

                const i64 sum = static_cast<i64>(X[0]) + X[1] + X[2];
                if (sum <= 0) {
                    ++stats.fallback_count;
                    continue;
                }
                strategy_common::track_divisor(sum, stats);
                const i64 x_num = static_cast<i64>(X[0]) * kQ;
                const i64 y_num = static_cast<i64>(X[1]) * kQ;
                strategy_common::track_abs(x_num, &stats.max_abs_numerator);
                strategy_common::track_abs(y_num, &stats.max_abs_numerator);
                const i32 x_q = static_cast<i32>(x_num / sum);
                const i32 y_q = static_cast<i32>(y_num / sum);
                strategy_common::track_abs(x_q, &stats.max_abs_output_q);
                strategy_common::track_abs(y_q, &stats.max_abs_output_q);

                i32 endpoint[4];
                if (!solve_balanced_fraction_for_xy_q(
                        x_q, y_q, endpoint, &stats)) {
                    ++stats.fallback_count;
                    continue;
                }
                strategy_wx_overdrive_q16::track_endpoint4(endpoint, stats);
            }
        }
    }
    return stats;
}

inline void teardown() {
    strategy_wx_overdrive_q16::teardown();
}

}  // namespace strategy_wx_lp_q16_balanced

namespace strategy_uniform_lut {

struct State {
    ProfileCache cache;
    LutTable lut;
};

static State g_bilinear16;
static State g_hermite16;
static State g_hermite32;

inline void setup_state(State& state, const DiodeProfile& profile,
                        int n, LutInterp interp) {
    strategy_common::build_default_cache(profile, &state.cache);
    state.lut = build_lut(state.cache, n, interp);
}

inline void solve_state(State& state, u8 r, u8 g, u8 b,
                        u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    if (strategy_common::solve_native_topology_before_lut(
            state.cache, r, g, b, out_r, out_g, out_b, out_w)) {
        return;
    }
    float xy[2], Y;
    if (!strategy_common::source_to_xyY(state.cache, r, g, b, xy, &Y)) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }
    float rgbw[4] = {0, 0, 0, 0};
    lookup_lut(state.lut, xy, Y, rgbw);
    strategy_common::quantize_rgbw(rgbw, out_r, out_g, out_b, out_w);
}

inline void teardown_state(State& state) {
    state.lut.cells.reset();
    state.lut.N = 0;
}

inline void setup_bilinear16(const DiodeProfile& profile) {
    setup_state(g_bilinear16, profile, 16, LutInterp::Bilinear);
}
inline void solve_bilinear16(u8 r, u8 g, u8 b, u8* or_, u8* og, u8* ob, u8* ow) {
    solve_state(g_bilinear16, r, g, b, or_, og, ob, ow);
}
inline void teardown_bilinear16() { teardown_state(g_bilinear16); }

inline void setup_hermite16(const DiodeProfile& profile) {
    setup_state(g_hermite16, profile, 16, LutInterp::Hermite);
}
inline void solve_hermite16(u8 r, u8 g, u8 b, u8* or_, u8* og, u8* ob, u8* ow) {
    solve_state(g_hermite16, r, g, b, or_, og, ob, ow);
}
inline void teardown_hermite16() { teardown_state(g_hermite16); }

inline void setup_hermite32(const DiodeProfile& profile) {
    setup_state(g_hermite32, profile, 32, LutInterp::Hermite);
}
inline void solve_hermite32(u8 r, u8 g, u8 b, u8* or_, u8* og, u8* ob, u8* ow) {
    solve_state(g_hermite32, r, g, b, or_, og, ob, ow);
}
inline void teardown_hermite32() { teardown_state(g_hermite32); }

}  // namespace strategy_uniform_lut

namespace strategy_experimental_hermite {

static const int kMaxN = 32;
static const int kStride = 12;

struct State {
    int N = 0;
    int q = 4096;
    ProfileCache cache;
    float xy_min[2] = {0, 0};
    float xy_max[2] = {0, 0};
    float axis_x[kMaxN] = {0};
    float axis_y[kMaxN] = {0};
    fl::unique_ptr<i32[]> cells;
};

static State g_cell_q16;
static State g_hull_adaptive;

inline void domain_from_profile(const ProfileCache& cache, State& state) {
    const float* R = cache.profile->xy_r;
    const float* G = cache.profile->xy_g;
    const float* B = cache.profile->xy_b;
    const float xmin = fl::min(fl::min(R[0], G[0]), B[0]);
    const float xmax = fl::max(fl::max(R[0], G[0]), B[0]);
    const float ymin = fl::min(fl::min(R[1], G[1]), B[1]);
    const float ymax = fl::max(fl::max(R[1], G[1]), B[1]);
    const float xpad = (xmax - xmin) * 0.02f;
    const float ypad = (ymax - ymin) * 0.02f;
    state.xy_min[0] = xmin - xpad;
    state.xy_min[1] = ymin - ypad;
    state.xy_max[0] = xmax + xpad;
    state.xy_max[1] = ymax + ypad;
}

inline float segment_projection(float p, float a, float b) {
    const float lo = fl::min(a, b);
    const float hi = fl::max(a, b);
    return fl::clamp(p, lo, hi);
}

inline double density_at(double x, const float* centers, int count,
                         double span) {
    const double sigma = span * 0.080;
    if (sigma <= 1e-12) return 1.0;
    double d = 1.0;
    for (int i = 0; i < count; ++i) {
        const double z = (x - centers[i]) / sigma;
        d += 5.0 * fl::exp(-0.5 * z * z);
    }
    return d;
}

inline void build_uniform_axis(float lo, float hi, int N, float* out) {
    const float inv = 1.0f / static_cast<float>(N - 1);
    for (int i = 0; i < N; ++i) {
        out[i] = lo + (hi - lo) * static_cast<float>(i) * inv;
    }
}

inline void build_warp_axis(float lo, float hi,
                            const float* centers, int center_count,
                            int N, float* out) {
    static const int kSteps = 384;
    const double span = static_cast<double>(hi - lo);
    if (span <= 1e-9) {
        build_uniform_axis(lo, hi, N, out);
        return;
    }

    double total = 0.0;
    for (int i = 0; i < kSteps; ++i) {
        const double x0 = lo + span * static_cast<double>(i) / kSteps;
        const double x1 = lo + span * static_cast<double>(i + 1) / kSteps;
        total += 0.5 * (density_at(x0, centers, center_count, span)
                      + density_at(x1, centers, center_count, span)) * (x1 - x0);
    }

    out[0] = lo;
    out[N - 1] = hi;
    double accum = 0.0;
    int next = 1;
    for (int i = 0; i < kSteps && next < N - 1; ++i) {
        const double x0 = lo + span * static_cast<double>(i) / kSteps;
        const double x1 = lo + span * static_cast<double>(i + 1) / kSteps;
        const double area = 0.5 * (density_at(x0, centers, center_count, span)
                                 + density_at(x1, centers, center_count, span)) * (x1 - x0);
        while (next < N - 1) {
            const double target = total * static_cast<double>(next) /
                                  static_cast<double>(N - 1);
            if (accum + area < target) break;
            const double t = area > 1e-12 ? (target - accum) / area : 0.0;
            out[next++] = static_cast<float>(x0 + (x1 - x0) * fl::clamp(t, 0.0, 1.0));
        }
        accum += area;
    }

    while (next < N - 1) {
        out[next] = lo + (hi - lo) * static_cast<float>(next) /
                         static_cast<float>(N - 1);
        ++next;
    }

    const float eps = (hi - lo) * 1e-5f;
    for (int i = 1; i < N; ++i) {
        if (out[i] <= out[i - 1] + eps) {
            out[i] = out[i - 1] + eps;
        }
    }
    out[N - 1] = hi;
}

inline void build_axes(State& state, bool warped) {
    if (!warped) {
        build_uniform_axis(state.xy_min[0], state.xy_max[0], state.N, state.axis_x);
        build_uniform_axis(state.xy_min[1], state.xy_max[1], state.N, state.axis_y);
        return;
    }

    const DiodeProfile& p = *state.cache.profile;
    float cx[8];
    float cy[8];
    int n = 0;
    cx[n] = state.cache.xy_w[0];
    cy[n] = state.cache.xy_w[1];
    ++n;
    const float* xs[3] = { p.xy_r, p.xy_g, p.xy_b };
    const int pairs[3][2] = {{0, 1}, {0, 2}, {1, 2}};
    for (int k = 0; k < 3; ++k) {
        const float* a = xs[pairs[k][0]];
        const float* b = xs[pairs[k][1]];
        cx[n] = segment_projection(state.cache.xy_w[0], a[0], b[0]);
        cy[n] = segment_projection(state.cache.xy_w[1], a[1], b[1]);
        ++n;
    }
    build_warp_axis(state.xy_min[0], state.xy_max[0], cx, n, state.N, state.axis_x);
    build_warp_axis(state.xy_min[1], state.xy_max[1], cy, n, state.N, state.axis_y);
}

inline void sample_xy(const ProfileCache& cache, float x, float y, float out[4]) {
    const float xy[2] = {x, y};
    out[0] = out[1] = out[2] = out[3] = 0.0f;
    solve_strict_subgamut_xy(cache, xy, 1.0f, out);
}

inline i32* cell(State& state, int i, int j) {
    return &state.cells[(j * state.N + i) * kStride];
}

inline const i32* cell_const(const State& state, int i, int j) {
    return &state.cells[(j * state.N + i) * kStride];
}

inline void build_state(State& state, const DiodeProfile& profile,
                        int N, int q, bool warped) {
    state.N = N;
    state.q = q;
    strategy_common::build_default_cache(profile, &state.cache);
    domain_from_profile(state.cache, state);
    build_axes(state, warped);
    state.cells = fl::make_unique<i32[]>(static_cast<fl::size_t>(N * N * kStride));

    for (int j = 0; j < N; ++j) {
        for (int i = 0; i < N; ++i) {
            const float x = state.axis_x[i];
            const float y = state.axis_y[j];
            float v[4];
            sample_xy(state.cache, x, y, v);

            const int ilo = i > 0 ? i - 1 : i;
            const int ihi = i < N - 1 ? i + 1 : i;
            const int jlo = j > 0 ? j - 1 : j;
            const int jhi = j < N - 1 ? j + 1 : j;
            float vx0[4], vx1[4], vy0[4], vy1[4];
            sample_xy(state.cache, state.axis_x[ilo], y, vx0);
            sample_xy(state.cache, state.axis_x[ihi], y, vx1);
            sample_xy(state.cache, x, state.axis_y[jlo], vy0);
            sample_xy(state.cache, x, state.axis_y[jhi], vy1);
            const float dx = state.axis_x[ihi] - state.axis_x[ilo];
            const float dy = state.axis_y[jhi] - state.axis_y[jlo];

            i32* c = cell(state, i, j);
            for (int k = 0; k < 4; ++k) {
                c[k] = strategy_common::quantize_q(v[k], q);
                c[4 + k] = strategy_common::quantize_q(dx > 1e-9f ? (vx1[k] - vx0[k]) / dx : 0.0f, q);
                c[8 + k] = strategy_common::quantize_q(dy > 1e-9f ? (vy1[k] - vy0[k]) / dy : 0.0f, q);
            }
        }
    }
}

inline int find_interval(const float* axis, int N, float x) {
    if (x <= axis[0]) return 0;
    if (x >= axis[N - 1]) return N - 2;
    int lo = 0;
    int hi = N - 1;
    while (hi - lo > 1) {
        const int mid = (lo + hi) >> 1;
        if (axis[mid] <= x) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return lo;
}

inline void lookup_state(const State& state, const float xy[2], float Y, float out[4]) {
    out[0] = out[1] = out[2] = out[3] = 0.0f;
    if (state.N < 2 || state.cells.get() == nullptr) return;

    const int i = find_interval(state.axis_x, state.N, xy[0]);
    const int j = find_interval(state.axis_y, state.N, xy[1]);
    const float cell_dx = state.axis_x[i + 1] - state.axis_x[i];
    const float cell_dy = state.axis_y[j + 1] - state.axis_y[j];
    if (cell_dx <= 1e-9f || cell_dy <= 1e-9f) return;
    const float fx = fl::clamp((xy[0] - state.axis_x[i]) / cell_dx, 0.0f, 1.0f);
    const float fy = fl::clamp((xy[1] - state.axis_y[j]) / cell_dy, 0.0f, 1.0f);

    const i32* c00 = cell_const(state, i, j);
    const i32* c10 = cell_const(state, i + 1, j);
    const i32* c01 = cell_const(state, i, j + 1);
    const i32* c11 = cell_const(state, i + 1, j + 1);
    const float inv_q = 1.0f / static_cast<float>(state.q);
    float bx[4], by[4];
    hermite_basis(fx, bx);
    hermite_basis(fy, by);

    for (int k = 0; k < 4; ++k) {
        const float v00 = c00[k] * inv_q;
        const float v10 = c10[k] * inv_q;
        const float v01 = c01[k] * inv_q;
        const float v11 = c11[k] * inv_q;
        const float dx00 = c00[4 + k] * inv_q * cell_dx;
        const float dx10 = c10[4 + k] * inv_q * cell_dx;
        const float dx01 = c01[4 + k] * inv_q * cell_dx;
        const float dx11 = c11[4 + k] * inv_q * cell_dx;
        const float dy00 = c00[8 + k] * inv_q * cell_dy;
        const float dy10 = c10[8 + k] * inv_q * cell_dy;
        const float dy01 = c01[8 + k] * inv_q * cell_dy;
        const float dy11 = c11[8 + k] * inv_q * cell_dy;

        const float per_Y =
              v00 * bx[0] * by[0] + v10 * bx[1] * by[0]
            + v01 * bx[0] * by[1] + v11 * bx[1] * by[1]
            + dx00 * bx[2] * by[0] + dx10 * bx[3] * by[0]
            + dx01 * bx[2] * by[1] + dx11 * bx[3] * by[1]
            + dy00 * bx[0] * by[2] + dy10 * bx[1] * by[2]
            + dy01 * bx[0] * by[3] + dy11 * bx[1] * by[3];
        out[k] = fl::max(per_Y * Y, 0.0f);
    }
    strategy_common::normalize4(out);
}

inline void solve_state(State& state, u8 r, u8 g, u8 b,
                        u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    if (strategy_common::solve_native_topology_before_lut(
            state.cache, r, g, b, out_r, out_g, out_b, out_w)) {
        return;
    }
    float xy[2], Y;
    if (!strategy_common::source_to_xyY(state.cache, r, g, b, xy, &Y)) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }
    float rgbw[4];
    lookup_state(state, xy, Y, rgbw);
    strategy_common::quantize_rgbw(rgbw, out_r, out_g, out_b, out_w);
}

inline void setup_cell_q16(const DiodeProfile& profile) {
    build_state(g_cell_q16, profile, 16, 65536, false);
}
inline void solve_cell_q16(u8 r, u8 g, u8 b, u8* or_, u8* og, u8* ob, u8* ow) {
    solve_state(g_cell_q16, r, g, b, or_, og, ob, ow);
}
inline void teardown_cell_q16() { g_cell_q16.cells.reset(); g_cell_q16.N = 0; }

inline void setup_hull_adaptive(const DiodeProfile& profile) {
    build_state(g_hull_adaptive, profile, 16, 4096, true);
}
inline void solve_hull_adaptive(u8 r, u8 g, u8 b, u8* or_, u8* og, u8* ob, u8* ow) {
    solve_state(g_hull_adaptive, r, g, b, or_, og, ob, ow);
}
inline void teardown_hull_adaptive() { g_hull_adaptive.cells.reset(); g_hull_adaptive.N = 0; }

}  // namespace strategy_experimental_hermite

namespace strategy_tetrahedral_rgb8 {

static const int kN = 8;
static const int kQ = 4096;

struct State {
    ProfileCache cache;
    fl::unique_ptr<i16[]> cells;
};

static State g_state;
static State g_boosted_state;

inline i16* cell(State& state, int r, int g, int b) {
    return &state.cells[(((r * kN) + g) * kN + b) * 4];
}

inline const i16* cell_const(const State& state, int r, int g, int b) {
    return &state.cells[(((r * kN) + g) * kN + b) * 4];
}

inline void setup_state(State& state, const DiodeProfile& profile, bool boosted) {
    strategy_common::build_default_cache(profile, &state.cache);
    state.cells = fl::make_unique<i16[]>(kN * kN * kN * 4);
    for (int ri = 0; ri < kN; ++ri) {
        for (int gi = 0; gi < kN; ++gi) {
            for (int bi = 0; bi < kN; ++bi) {
                const float r = static_cast<float>(ri) / static_cast<float>(kN - 1);
                const float g = static_cast<float>(gi) / static_cast<float>(kN - 1);
                const float b = static_cast<float>(bi) / static_cast<float>(kN - 1);
                float rgbw[4] = {0, 0, 0, 0};
                if (boosted) {
                    solve_wx_overdrive(
                        state.cache, r, g, b, kDefaultOverdriveRatio, rgbw);
                } else {
                    solve_strict_subgamut(state.cache, r, g, b, rgbw);
                }
                i16* c = cell(state, ri, gi, bi);
                for (int k = 0; k < 4; ++k) {
                    c[k] = static_cast<i16>(fl::clamp(strategy_common::quantize_q(rgbw[k], kQ),
                                                      static_cast<i32>(-32768),
                                                      static_cast<i32>(32767)));
                }
            }
        }
    }
}

inline void setup(const DiodeProfile& profile) {
    setup_state(g_state, profile, false);
}

inline void setup_boosted(const DiodeProfile& profile) {
    setup_state(g_boosted_state, profile, true);
}

inline void fetch_state(const State& state, int ri, int gi, int bi, float out[4]) {
    const i16* c = cell_const(state, ri, gi, bi);
    const float inv_q = 1.0f / static_cast<float>(kQ);
    for (int k = 0; k < 4; ++k) out[k] = c[k] * inv_q;
}

inline void lerp_path(const float c0[4], const float c1[4], const float c2[4],
                      const float c3[4], float a, float b, float c, float out[4]) {
    for (int k = 0; k < 4; ++k) {
        out[k] = c0[k]
               + a * (c1[k] - c0[k])
               + b * (c2[k] - c1[k])
               + c * (c3[k] - c2[k]);
    }
}

inline void solve_state(const State& state, u8 r, u8 g, u8 b,
                        u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    if ((r | g | b) == 0) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }
    const float xr = fl::clamp((r * (1.0f / 255.0f)) * (kN - 1), 0.0f,
                               static_cast<float>(kN - 1) - 1e-4f);
    const float xg = fl::clamp((g * (1.0f / 255.0f)) * (kN - 1), 0.0f,
                               static_cast<float>(kN - 1) - 1e-4f);
    const float xb = fl::clamp((b * (1.0f / 255.0f)) * (kN - 1), 0.0f,
                               static_cast<float>(kN - 1) - 1e-4f);
    const int ri = static_cast<int>(xr);
    const int gi = static_cast<int>(xg);
    const int bi = static_cast<int>(xb);
    const float fr = xr - ri;
    const float fg = xg - gi;
    const float fb = xb - bi;

    float c000[4], c100[4], c010[4], c001[4];
    float c110[4], c101[4], c011[4], c111[4], out[4];
    fetch_state(state, ri, gi, bi, c000);
    fetch_state(state, ri + 1, gi, bi, c100);
    fetch_state(state, ri, gi + 1, bi, c010);
    fetch_state(state, ri, gi, bi + 1, c001);
    fetch_state(state, ri + 1, gi + 1, bi, c110);
    fetch_state(state, ri + 1, gi, bi + 1, c101);
    fetch_state(state, ri, gi + 1, bi + 1, c011);
    fetch_state(state, ri + 1, gi + 1, bi + 1, c111);

    if (fr >= fg) {
        if (fg >= fb) {
            lerp_path(c000, c100, c110, c111, fr, fg, fb, out);
        } else if (fr >= fb) {
            lerp_path(c000, c100, c101, c111, fr, fb, fg, out);
        } else {
            lerp_path(c000, c001, c101, c111, fb, fr, fg, out);
        }
    } else {
        if (fr >= fb) {
            lerp_path(c000, c010, c110, c111, fg, fr, fb, out);
        } else if (fg >= fb) {
            lerp_path(c000, c010, c011, c111, fg, fb, fr, out);
        } else {
            lerp_path(c000, c001, c011, c111, fb, fg, fr, out);
        }
    }

    strategy_common::normalize4(out);
    strategy_common::quantize_rgbw(out, out_r, out_g, out_b, out_w);
}

inline void solve(u8 r, u8 g, u8 b, u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    strategy_tetrahedral_rgb8::solve_state(
        g_state, r, g, b, out_r, out_g, out_b, out_w);
}

inline void solve_boosted(u8 r, u8 g, u8 b,
                          u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    strategy_tetrahedral_rgb8::solve_state(
        g_boosted_state, r, g, b, out_r, out_g, out_b, out_w);
}

inline void teardown() {
    g_state.cells.reset();
}

inline void teardown_boosted() {
    g_boosted_state.cells.reset();
}

}  // namespace strategy_tetrahedral_rgb8

namespace strategy_tetrahedral_rgb8_fixed {

static const int kN = strategy_tetrahedral_rgb8::kN;
static const int kQ = strategy_tetrahedral_rgb8::kQ;

using State = strategy_tetrahedral_rgb8::State;

static State g_state;
static State g_boosted_state;

inline i16* cell(State& state, int r, int g, int b) {
    return &state.cells[(((r * kN) + g) * kN + b) * 4];
}

inline const i16* cell_const(const State& state, int r, int g, int b) {
    return &state.cells[(((r * kN) + g) * kN + b) * 4];
}

inline void setup(const DiodeProfile& profile) {
    strategy_tetrahedral_rgb8::setup_state(g_state, profile, false);
}

inline void setup_boosted(const DiodeProfile& profile) {
    strategy_tetrahedral_rgb8::setup_state(g_boosted_state, profile, true);
}

inline void coordinate(u8 v, int* i, i32* f_q) {
    const int scaled = static_cast<int>(v) * (kN - 1);
    int cell_i = scaled / 255;
    int rem = scaled - cell_i * 255;
    if (cell_i >= kN - 1) {
        cell_i = kN - 2;
        rem = 255;
    }
    *i = cell_i;
    *f_q = static_cast<i32>((static_cast<i64>(rem) * kQ + 127) / 255);
}

inline void fetch_state(const State& state, int ri, int gi, int bi, i32 out[4]) {
    const i16* c = strategy_tetrahedral_rgb8_fixed::cell_const(state, ri, gi, bi);
    for (int k = 0; k < 4; ++k) out[k] = c[k];
}

inline void lerp_path_q(const i32 c0[4], const i32 c1[4],
                        const i32 c2[4], const i32 c3[4],
                        i32 a, i32 b, i32 c, i32 out[4]) {
    for (int k = 0; k < 4; ++k) {
        const i64 acc =
              static_cast<i64>(c0[k]) * kQ
            + static_cast<i64>(a) * (c1[k] - c0[k])
            + static_cast<i64>(b) * (c2[k] - c1[k])
            + static_cast<i64>(c) * (c3[k] - c2[k]);
        out[k] = static_cast<i32>((acc + (kQ / 2)) / kQ);
    }
}

inline void normalize4_q(i32 out[4]) {
    i32 m = out[0];
    if (out[1] > m) m = out[1];
    if (out[2] > m) m = out[2];
    if (out[3] > m) m = out[3];
    if (m <= kQ) return;
    for (int k = 0; k < 4; ++k) {
        out[k] = static_cast<i32>((static_cast<i64>(out[k]) * kQ + (m / 2)) / m);
    }
}

inline u8 q_to_u8(i32 qv) {
    if (qv <= 0) return 0;
    const i64 scaled = static_cast<i64>(qv) * 255 + (kQ / 2);
    const i64 out = scaled / kQ;
    if (out >= 255) return 255;
    return static_cast<u8>(out);
}

inline void solve_state(const State& state, u8 r, u8 g, u8 b,
                        u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    if ((r | g | b) == 0) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }

    int ri, gi, bi;
    i32 fr, fg, fb;
    coordinate(r, &ri, &fr);
    coordinate(g, &gi, &fg);
    coordinate(b, &bi, &fb);

    i32 c000[4], c100[4], c010[4], c001[4];
    i32 c110[4], c101[4], c011[4], c111[4], out[4];
    fetch_state(state, ri, gi, bi, c000);
    fetch_state(state, ri + 1, gi, bi, c100);
    fetch_state(state, ri, gi + 1, bi, c010);
    fetch_state(state, ri, gi, bi + 1, c001);
    fetch_state(state, ri + 1, gi + 1, bi, c110);
    fetch_state(state, ri + 1, gi, bi + 1, c101);
    fetch_state(state, ri, gi + 1, bi + 1, c011);
    fetch_state(state, ri + 1, gi + 1, bi + 1, c111);

    if (fr >= fg) {
        if (fg >= fb) {
            lerp_path_q(c000, c100, c110, c111, fr, fg, fb, out);
        } else if (fr >= fb) {
            lerp_path_q(c000, c100, c101, c111, fr, fb, fg, out);
        } else {
            lerp_path_q(c000, c001, c101, c111, fb, fr, fg, out);
        }
    } else {
        if (fr >= fb) {
            lerp_path_q(c000, c010, c110, c111, fg, fr, fb, out);
        } else if (fg >= fb) {
            lerp_path_q(c000, c010, c011, c111, fg, fb, fr, out);
        } else {
            lerp_path_q(c000, c001, c011, c111, fb, fg, fr, out);
        }
    }

    normalize4_q(out);
    *out_r = q_to_u8(out[0]);
    *out_g = q_to_u8(out[1]);
    *out_b = q_to_u8(out[2]);
    *out_w = q_to_u8(out[3]);
}

inline void solve(u8 r, u8 g, u8 b, u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    strategy_tetrahedral_rgb8_fixed::solve_state(
        g_state, r, g, b, out_r, out_g, out_b, out_w);
}

inline void solve_boosted(u8 r, u8 g, u8 b,
                          u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    strategy_tetrahedral_rgb8_fixed::solve_state(
        g_boosted_state, r, g, b, out_r, out_g, out_b, out_w);
}

inline void teardown() {
    g_state.cells.reset();
}

inline void teardown_boosted() {
    g_boosted_state.cells.reset();
}

}  // namespace strategy_tetrahedral_rgb8_fixed

namespace strategy_tetrahedral_rgb8_u8 {

static const int kN = strategy_tetrahedral_rgb8::kN;

struct State {
    ProfileCache cache;
    fl::unique_ptr<u8[]> cells;
};

static State g_state;
static State g_boosted_state;

inline u8* cell(State& state, int r, int g, int b) {
    return &state.cells[(((r * kN) + g) * kN + b) * 4];
}

inline const u8* cell_const(const State& state, int r, int g, int b) {
    return &state.cells[(((r * kN) + g) * kN + b) * 4];
}

inline void setup_state(State& state, const DiodeProfile& profile, bool boosted) {
    strategy_common::build_default_cache(profile, &state.cache);
    state.cells = fl::make_unique<u8[]>(kN * kN * kN * 4);
    for (int ri = 0; ri < kN; ++ri) {
        for (int gi = 0; gi < kN; ++gi) {
            for (int bi = 0; bi < kN; ++bi) {
                const float r = static_cast<float>(ri) / static_cast<float>(kN - 1);
                const float g = static_cast<float>(gi) / static_cast<float>(kN - 1);
                const float b = static_cast<float>(bi) / static_cast<float>(kN - 1);
                float rgbw[4] = {0, 0, 0, 0};
                if (boosted) {
                    solve_wx_overdrive(
                        state.cache, r, g, b, kDefaultOverdriveRatio, rgbw);
                } else {
                    solve_strict_subgamut(state.cache, r, g, b, rgbw);
                }
                u8* c = cell(state, ri, gi, bi);
                for (int k = 0; k < 4; ++k) {
                    c[k] = quantize_u8(rgbw[k]);
                }
            }
        }
    }
}

inline void setup(const DiodeProfile& profile) {
    setup_state(g_state, profile, false);
}

inline void setup_boosted(const DiodeProfile& profile) {
    setup_state(g_boosted_state, profile, true);
}

inline void fetch_state(const State& state, int ri, int gi, int bi, float out[4]) {
    const u8* c = cell_const(state, ri, gi, bi);
    for (int k = 0; k < 4; ++k) {
        out[k] = static_cast<float>(c[k]) * (1.0f / 255.0f);
    }
}

inline void solve_state(const State& state, u8 r, u8 g, u8 b,
                        u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    if ((r | g | b) == 0) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }
    const float xr = fl::clamp((r * (1.0f / 255.0f)) * (kN - 1), 0.0f,
                               static_cast<float>(kN - 1) - 1e-4f);
    const float xg = fl::clamp((g * (1.0f / 255.0f)) * (kN - 1), 0.0f,
                               static_cast<float>(kN - 1) - 1e-4f);
    const float xb = fl::clamp((b * (1.0f / 255.0f)) * (kN - 1), 0.0f,
                               static_cast<float>(kN - 1) - 1e-4f);
    const int ri = static_cast<int>(xr);
    const int gi = static_cast<int>(xg);
    const int bi = static_cast<int>(xb);
    const float fr = xr - ri;
    const float fg = xg - gi;
    const float fb = xb - bi;

    float c000[4], c100[4], c010[4], c001[4];
    float c110[4], c101[4], c011[4], c111[4], out[4];
    fetch_state(state, ri, gi, bi, c000);
    fetch_state(state, ri + 1, gi, bi, c100);
    fetch_state(state, ri, gi + 1, bi, c010);
    fetch_state(state, ri, gi, bi + 1, c001);
    fetch_state(state, ri + 1, gi + 1, bi, c110);
    fetch_state(state, ri + 1, gi, bi + 1, c101);
    fetch_state(state, ri, gi + 1, bi + 1, c011);
    fetch_state(state, ri + 1, gi + 1, bi + 1, c111);

    if (fr >= fg) {
        if (fg >= fb) {
            strategy_tetrahedral_rgb8::lerp_path(c000, c100, c110, c111,
                                                 fr, fg, fb, out);
        } else if (fr >= fb) {
            strategy_tetrahedral_rgb8::lerp_path(c000, c100, c101, c111,
                                                 fr, fb, fg, out);
        } else {
            strategy_tetrahedral_rgb8::lerp_path(c000, c001, c101, c111,
                                                 fb, fr, fg, out);
        }
    } else {
        if (fr >= fb) {
            strategy_tetrahedral_rgb8::lerp_path(c000, c010, c110, c111,
                                                 fg, fr, fb, out);
        } else if (fg >= fb) {
            strategy_tetrahedral_rgb8::lerp_path(c000, c010, c011, c111,
                                                 fg, fb, fr, out);
        } else {
            strategy_tetrahedral_rgb8::lerp_path(c000, c001, c011, c111,
                                                 fb, fg, fr, out);
        }
    }

    strategy_common::normalize4(out);
    strategy_common::quantize_rgbw(out, out_r, out_g, out_b, out_w);
}

inline void solve(u8 r, u8 g, u8 b, u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    strategy_tetrahedral_rgb8_u8::solve_state(
        g_state, r, g, b, out_r, out_g, out_b, out_w);
}

inline void solve_boosted(u8 r, u8 g, u8 b,
                          u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    strategy_tetrahedral_rgb8_u8::solve_state(
        g_boosted_state, r, g, b, out_r, out_g, out_b, out_w);
}

inline void teardown() {
    g_state.cells.reset();
}

inline void teardown_boosted() {
    g_boosted_state.cells.reset();
}

}  // namespace strategy_tetrahedral_rgb8_u8

namespace strategy_tetrahedral_rgb8_u8_fixed {

static const int kQ = strategy_tetrahedral_rgb8::kQ;

using State = strategy_tetrahedral_rgb8_u8::State;

static State g_state;
static State g_boosted_state;

inline void setup(const DiodeProfile& profile) {
    strategy_tetrahedral_rgb8_u8::setup_state(g_state, profile, false);
}

inline void setup_boosted(const DiodeProfile& profile) {
    strategy_tetrahedral_rgb8_u8::setup_state(g_boosted_state, profile, true);
}

inline void fetch_state(const State& state, int ri, int gi, int bi, i32 out[4]) {
    const u8* c = strategy_tetrahedral_rgb8_u8::cell_const(state, ri, gi, bi);
    for (int k = 0; k < 4; ++k) {
        out[k] = static_cast<i32>(
            (static_cast<i64>(c[k]) * kQ + 127) / 255);
    }
}

inline void solve_state(const State& state, u8 r, u8 g, u8 b,
                        u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    if ((r | g | b) == 0) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }

    int ri, gi, bi;
    i32 fr, fg, fb;
    strategy_tetrahedral_rgb8_fixed::coordinate(r, &ri, &fr);
    strategy_tetrahedral_rgb8_fixed::coordinate(g, &gi, &fg);
    strategy_tetrahedral_rgb8_fixed::coordinate(b, &bi, &fb);

    i32 c000[4], c100[4], c010[4], c001[4];
    i32 c110[4], c101[4], c011[4], c111[4], out[4];
    fetch_state(state, ri, gi, bi, c000);
    fetch_state(state, ri + 1, gi, bi, c100);
    fetch_state(state, ri, gi + 1, bi, c010);
    fetch_state(state, ri, gi, bi + 1, c001);
    fetch_state(state, ri + 1, gi + 1, bi, c110);
    fetch_state(state, ri + 1, gi, bi + 1, c101);
    fetch_state(state, ri, gi + 1, bi + 1, c011);
    fetch_state(state, ri + 1, gi + 1, bi + 1, c111);

    if (fr >= fg) {
        if (fg >= fb) {
            strategy_tetrahedral_rgb8_fixed::lerp_path_q(
                c000, c100, c110, c111, fr, fg, fb, out);
        } else if (fr >= fb) {
            strategy_tetrahedral_rgb8_fixed::lerp_path_q(
                c000, c100, c101, c111, fr, fb, fg, out);
        } else {
            strategy_tetrahedral_rgb8_fixed::lerp_path_q(
                c000, c001, c101, c111, fb, fr, fg, out);
        }
    } else {
        if (fr >= fb) {
            strategy_tetrahedral_rgb8_fixed::lerp_path_q(
                c000, c010, c110, c111, fg, fr, fb, out);
        } else if (fg >= fb) {
            strategy_tetrahedral_rgb8_fixed::lerp_path_q(
                c000, c010, c011, c111, fg, fb, fr, out);
        } else {
            strategy_tetrahedral_rgb8_fixed::lerp_path_q(
                c000, c001, c011, c111, fb, fg, fr, out);
        }
    }

    strategy_tetrahedral_rgb8_fixed::normalize4_q(out);
    *out_r = strategy_tetrahedral_rgb8_fixed::q_to_u8(out[0]);
    *out_g = strategy_tetrahedral_rgb8_fixed::q_to_u8(out[1]);
    *out_b = strategy_tetrahedral_rgb8_fixed::q_to_u8(out[2]);
    *out_w = strategy_tetrahedral_rgb8_fixed::q_to_u8(out[3]);
}

inline void solve(u8 r, u8 g, u8 b, u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    strategy_tetrahedral_rgb8_u8_fixed::solve_state(
        g_state, r, g, b, out_r, out_g, out_b, out_w);
}

inline void solve_boosted(u8 r, u8 g, u8 b,
                          u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    strategy_tetrahedral_rgb8_u8_fixed::solve_state(
        g_boosted_state, r, g, b, out_r, out_g, out_b, out_w);
}

inline void teardown() {
    g_state.cells.reset();
}

inline void teardown_boosted() {
    g_boosted_state.cells.reset();
}

}  // namespace strategy_tetrahedral_rgb8_u8_fixed

namespace strategy_poly_piecewise_d3 {

static const int kPieces = 3;
static const int kTerms = 20;
static const int kGrid = 9;

struct State {
    ProfileCache cache;
    double coeff[kPieces][4][kTerms];
};

static State g_state;

inline int piece_for(float r, float g, float b) {
    if (r >= g && r >= b) return 0;
    if (g >= r && g >= b) return 1;
    return 2;
}

inline void basis(float r, float g, float b, double phi[kTerms]) {
    const double R = r;
    const double G = g;
    const double B = b;
    const double R2 = R * R;
    const double G2 = G * G;
    const double B2 = B * B;
    phi[0] = 1.0;
    phi[1] = R;     phi[2] = G;     phi[3] = B;
    phi[4] = R2;    phi[5] = G2;    phi[6] = B2;
    phi[7] = R * G; phi[8] = R * B; phi[9] = G * B;
    phi[10] = R2 * R; phi[11] = G2 * G; phi[12] = B2 * B;
    phi[13] = R2 * G; phi[14] = R2 * B;
    phi[15] = G2 * R; phi[16] = G2 * B;
    phi[17] = B2 * R; phi[18] = B2 * G;
    phi[19] = R * G * B;
}

inline bool solve_linear(double A[kTerms][kTerms],
                         const double rhs[kTerms],
                         double out[kTerms]) {
    double m[kTerms][kTerms + 1];
    for (int r = 0; r < kTerms; ++r) {
        for (int c = 0; c < kTerms; ++c) {
            m[r][c] = A[r][c];
        }
        m[r][kTerms] = rhs[r];
    }

    for (int col = 0; col < kTerms; ++col) {
        int pivot = col;
        double best = fl::fabs(m[col][col]);
        for (int r = col + 1; r < kTerms; ++r) {
            const double a = fl::fabs(m[r][col]);
            if (a > best) {
                best = a;
                pivot = r;
            }
        }
        if (best < 1e-10) {
            return false;
        }
        if (pivot != col) {
            for (int c = col; c <= kTerms; ++c) {
                const double tmp = m[col][c];
                m[col][c] = m[pivot][c];
                m[pivot][c] = tmp;
            }
        }
        const double inv = 1.0 / m[col][col];
        for (int c = col; c <= kTerms; ++c) {
            m[col][c] *= inv;
        }
        for (int r = 0; r < kTerms; ++r) {
            if (r == col) continue;
            const double f = m[r][col];
            if (fl::fabs(f) < 1e-16) continue;
            for (int c = col; c <= kTerms; ++c) {
                m[r][c] -= f * m[col][c];
            }
        }
    }

    for (int i = 0; i < kTerms; ++i) {
        out[i] = m[i][kTerms];
    }
    return true;
}

inline void setup(const DiodeProfile& profile) {
    strategy_common::build_default_cache(profile, &g_state.cache);
    double normal[kPieces][kTerms][kTerms];
    double rhs[kPieces][4][kTerms];
    for (int p = 0; p < kPieces; ++p) {
        for (int i = 0; i < kTerms; ++i) {
            for (int j = 0; j < kTerms; ++j) normal[p][i][j] = 0.0;
            for (int ch = 0; ch < 4; ++ch) rhs[p][ch][i] = 0.0;
        }
    }

    for (int ri = 0; ri < kGrid; ++ri) {
        for (int gi = 0; gi < kGrid; ++gi) {
            for (int bi = 0; bi < kGrid; ++bi) {
                if ((ri | gi | bi) == 0) continue;
                const float r = static_cast<float>(ri) / static_cast<float>(kGrid - 1);
                const float g = static_cast<float>(gi) / static_cast<float>(kGrid - 1);
                const float b = static_cast<float>(bi) / static_cast<float>(kGrid - 1);
                const int piece = piece_for(r, g, b);
                double phi[kTerms];
                basis(r, g, b, phi);

                float y[4] = {0, 0, 0, 0};
                solve_strict_subgamut(g_state.cache, r, g, b, y);
                for (int a = 0; a < kTerms; ++a) {
                    for (int c = 0; c < kTerms; ++c) {
                        normal[piece][a][c] += phi[a] * phi[c];
                    }
                    for (int ch = 0; ch < 4; ++ch) {
                        rhs[piece][ch][a] += phi[a] * y[ch];
                    }
                }
            }
        }
    }

    for (int p = 0; p < kPieces; ++p) {
        for (int i = 0; i < kTerms; ++i) {
            normal[p][i][i] += 1e-6;
        }
        for (int ch = 0; ch < 4; ++ch) {
            if (!solve_linear(normal[p], rhs[p][ch], g_state.coeff[p][ch])) {
                for (int i = 0; i < kTerms; ++i) g_state.coeff[p][ch][i] = 0.0;
            }
        }
    }
}

inline void solve(u8 r8, u8 g8, u8 b8,
                  u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    if ((r8 | g8 | b8) == 0) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }
    if (strategy_common::solve_native_topology_before_lut(
            g_state.cache, r8, g8, b8, out_r, out_g, out_b, out_w)) {
        return;
    }
    const float r = r8 * (1.0f / 255.0f);
    const float g = g8 * (1.0f / 255.0f);
    const float b = b8 * (1.0f / 255.0f);
    const int piece = piece_for(r, g, b);
    double phi[kTerms];
    basis(r, g, b, phi);
    float out[4];
    for (int ch = 0; ch < 4; ++ch) {
        double v = 0.0;
        for (int i = 0; i < kTerms; ++i) {
            v += g_state.coeff[piece][ch][i] * phi[i];
        }
        out[ch] = fl::clamp(static_cast<float>(v), 0.0f, 1.0f);
    }
    strategy_common::normalize4(out);
    strategy_common::quantize_rgbw(out, out_r, out_g, out_b, out_w);
}

inline void teardown() {}

}  // namespace strategy_poly_piecewise_d3

namespace strategy_poly_piecewise_d3_fixed {

static const int kPieces = strategy_poly_piecewise_d3::kPieces;
static const int kTerms = strategy_poly_piecewise_d3::kTerms;
static const int kGrid = strategy_poly_piecewise_d3::kGrid;
static const int kCoeffQ = 256;
static const int kBasisQ = 32768;

struct State {
    ProfileCache cache;
    i16 coeff[kPieces][4][kTerms];
};

static State g_state;

inline i16 quantize_coeff(double v) {
    const double scaled = v * static_cast<double>(kCoeffQ);
    const double rounded = scaled >= 0.0 ? scaled + 0.5 : scaled - 0.5;
    if (rounded > 32767.0) return 32767;
    if (rounded < -32768.0) return -32768;
    return static_cast<i16>(rounded);
}

inline i32 qmul_basis(i32 a, i32 b) {
    return static_cast<i32>((static_cast<i64>(a) * b + (kBasisQ / 2)) / kBasisQ);
}

inline void basis_q(u8 r8, u8 g8, u8 b8, i32 phi[kTerms]) {
    const i32 R = static_cast<i32>((static_cast<i64>(r8) * kBasisQ + 127) / 255);
    const i32 G = static_cast<i32>((static_cast<i64>(g8) * kBasisQ + 127) / 255);
    const i32 B = static_cast<i32>((static_cast<i64>(b8) * kBasisQ + 127) / 255);
    const i32 R2 = qmul_basis(R, R);
    const i32 G2 = qmul_basis(G, G);
    const i32 B2 = qmul_basis(B, B);
    phi[0] = kBasisQ;
    phi[1] = R;     phi[2] = G;     phi[3] = B;
    phi[4] = R2;    phi[5] = G2;    phi[6] = B2;
    phi[7] = qmul_basis(R, G);
    phi[8] = qmul_basis(R, B);
    phi[9] = qmul_basis(G, B);
    phi[10] = qmul_basis(R2, R);
    phi[11] = qmul_basis(G2, G);
    phi[12] = qmul_basis(B2, B);
    phi[13] = qmul_basis(R2, G);
    phi[14] = qmul_basis(R2, B);
    phi[15] = qmul_basis(G2, R);
    phi[16] = qmul_basis(G2, B);
    phi[17] = qmul_basis(B2, R);
    phi[18] = qmul_basis(B2, G);
    phi[19] = qmul_basis(qmul_basis(R, G), B);
}

inline u8 coeff_q_to_u8(i64 qv) {
    if (qv <= 0) return 0;
    const i64 max_q = static_cast<i64>(kCoeffQ);
    if (qv >= max_q) return 255;
    return static_cast<u8>((qv * 255 + (max_q / 2)) / max_q);
}

inline void setup(const DiodeProfile& profile) {
    strategy_common::build_default_cache(profile, &g_state.cache);
    double normal[kPieces][kTerms][kTerms];
    double rhs[kPieces][4][kTerms];
    for (int p = 0; p < kPieces; ++p) {
        for (int i = 0; i < kTerms; ++i) {
            for (int j = 0; j < kTerms; ++j) normal[p][i][j] = 0.0;
            for (int ch = 0; ch < 4; ++ch) rhs[p][ch][i] = 0.0;
        }
    }

    for (int ri = 0; ri < kGrid; ++ri) {
        for (int gi = 0; gi < kGrid; ++gi) {
            for (int bi = 0; bi < kGrid; ++bi) {
                if ((ri | gi | bi) == 0) continue;
                const float r = static_cast<float>(ri) / static_cast<float>(kGrid - 1);
                const float g = static_cast<float>(gi) / static_cast<float>(kGrid - 1);
                const float b = static_cast<float>(bi) / static_cast<float>(kGrid - 1);
                const int piece = strategy_poly_piecewise_d3::piece_for(r, g, b);
                double phi[kTerms];
                strategy_poly_piecewise_d3::basis(r, g, b, phi);

                float y[4] = {0, 0, 0, 0};
                solve_strict_subgamut(g_state.cache, r, g, b, y);
                for (int a = 0; a < kTerms; ++a) {
                    for (int c = 0; c < kTerms; ++c) {
                        normal[piece][a][c] += phi[a] * phi[c];
                    }
                    for (int ch = 0; ch < 4; ++ch) {
                        rhs[piece][ch][a] += phi[a] * y[ch];
                    }
                }
            }
        }
    }

    for (int p = 0; p < kPieces; ++p) {
        for (int i = 0; i < kTerms; ++i) {
            normal[p][i][i] += 1e-6;
        }
        for (int ch = 0; ch < 4; ++ch) {
            double coeff[kTerms];
            if (!strategy_poly_piecewise_d3::solve_linear(normal[p], rhs[p][ch], coeff)) {
                for (int i = 0; i < kTerms; ++i) coeff[i] = 0.0;
            }
            for (int i = 0; i < kTerms; ++i) {
                g_state.coeff[p][ch][i] = quantize_coeff(coeff[i]);
            }
        }
    }
}

inline void solve(u8 r8, u8 g8, u8 b8,
                  u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    if ((r8 | g8 | b8) == 0) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }
    if (strategy_common::solve_native_topology_before_lut(
            g_state.cache, r8, g8, b8, out_r, out_g, out_b, out_w)) {
        return;
    }
    const float rf = r8 * (1.0f / 255.0f);
    const float gf = g8 * (1.0f / 255.0f);
    const float bf = b8 * (1.0f / 255.0f);
    const int piece = strategy_poly_piecewise_d3::piece_for(rf, gf, bf);
    i32 phi[kTerms];
    basis_q(r8, g8, b8, phi);

    i64 out_q[4];
    for (int ch = 0; ch < 4; ++ch) {
        i64 acc = 0;
        for (int i = 0; i < kTerms; ++i) {
            acc += static_cast<i64>(g_state.coeff[piece][ch][i]) * phi[i];
        }
        out_q[ch] = acc / kBasisQ;
        if (out_q[ch] < 0) out_q[ch] = 0;
        if (out_q[ch] > kCoeffQ) out_q[ch] = kCoeffQ;
    }
    i64 m = out_q[0];
    if (out_q[1] > m) m = out_q[1];
    if (out_q[2] > m) m = out_q[2];
    if (out_q[3] > m) m = out_q[3];
    if (m > kCoeffQ) {
        for (int ch = 0; ch < 4; ++ch) {
            out_q[ch] = (out_q[ch] * kCoeffQ + (m / 2)) / m;
        }
    }
    *out_r = coeff_q_to_u8(out_q[0]);
    *out_g = coeff_q_to_u8(out_q[1]);
    *out_b = coeff_q_to_u8(out_q[2]);
    *out_w = coeff_q_to_u8(out_q[3]);
}

inline void teardown() {}

}  // namespace strategy_poly_piecewise_d3_fixed

namespace strategy_fixed_analytical_q16 {

static constexpr i32 kQ = 65536;

struct State {
    ProfileCache float_cache;
    bool has_source_space = false;
    i32 xy_r[2] = {0, 0};
    i32 xy_g[2] = {0, 0};
    i32 xy_b[2] = {0, 0};
    i32 xy_w[2] = {0, 0};
    i32 P[4][3] = {};
    i32 M_src[3][3] = {};
    i32 inv[3][3][3] = {};
};

static State g_state;

inline i32 q(float v) {
    return strategy_common::quantize_q(v, kQ);
}

inline i64 mul_q(i64 a, i64 b) {
    return (a * b) / kQ;
}

inline i32 clamp_q(i64 v) {
    if (v < 0) return 0;
    if (v > kQ) return kQ;
    return static_cast<i32>(v);
}

inline u8 q_to_scaled_u8(i32 qv, u8 value) {
    const i64 v = static_cast<i64>(qv) * static_cast<i64>(value);
    const i64 rounded = (v + (kQ / 2)) / kQ;
    if (rounded <= 0) return 0;
    if (rounded >= 255) return 255;
    return static_cast<u8>(rounded);
}

inline void matvec_q(const i32 M[3][3], const i32 in[3], i32 out[3]) {
    for (int row = 0; row < 3; ++row) {
        i64 acc = 0;
        for (int col = 0; col < 3; ++col) {
            acc += static_cast<i64>(M[row][col]) * in[col];
        }
        out[row] = static_cast<i32>(acc / kQ);
    }
}

inline void source_full_chroma_q(u8 r, u8 g, u8 b, u8 value, i32 X[3]) {
    const i32 src[3] = {
        static_cast<i32>((static_cast<i64>(r) * kQ) / value),
        static_cast<i32>((static_cast<i64>(g) * kQ) / value),
        static_cast<i32>((static_cast<i64>(b) * kQ) / value),
    };
    if (g_state.has_source_space) {
        matvec_q(g_state.M_src, src, X);
        return;
    }
    for (int row = 0; row < 3; ++row) {
        const i64 acc =
              static_cast<i64>(g_state.P[0][row]) * src[0]
            + static_cast<i64>(g_state.P[1][row]) * src[1]
            + static_cast<i64>(g_state.P[2][row]) * src[2];
        X[row] = static_cast<i32>(acc / kQ);
    }
}

inline bool xy_from_X(const i32 X[3], i32 xy[2]) {
    const i64 sum = static_cast<i64>(X[0]) + X[1] + X[2];
    if (sum <= 0) return false;
    xy[0] = static_cast<i32>((static_cast<i64>(X[0]) * kQ) / sum);
    xy[1] = static_cast<i32>((static_cast<i64>(X[1]) * kQ) / sum);
    return true;
}

inline bool xy_from_X_audit(const i32 X[3], i32 xy[2],
                            Q16RangeStats& stats) {
    const i64 sum = static_cast<i64>(X[0]) + X[1] + X[2];
    if (sum <= 0) return false;
    strategy_common::track_divisor(sum, stats);
    const i64 x_num = static_cast<i64>(X[0]) * kQ;
    const i64 y_num = static_cast<i64>(X[1]) * kQ;
    strategy_common::track_abs(x_num, &stats.max_abs_numerator);
    strategy_common::track_abs(y_num, &stats.max_abs_numerator);
    xy[0] = static_cast<i32>(x_num / sum);
    xy[1] = static_cast<i32>(y_num / sum);
    strategy_common::track_abs(xy[0], &stats.max_abs_output_q);
    strategy_common::track_abs(xy[1], &stats.max_abs_output_q);
    return true;
}

inline bool tri_contains(const i32 xy[2],
                         const i32 a[2], const i32 b[2], const i32 c[2]) {
    const i64 x = xy[0], y = xy[1];
    const i64 ax = a[0], ay = a[1];
    const i64 bx = b[0], by = b[1];
    const i64 cx = c[0], cy = c[1];
    i64 den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy);
    if (den == 0) return false;
    i64 n0 = (by - cy) * (x - cx) + (cx - bx) * (y - cy);
    i64 n1 = (cy - ay) * (x - cx) + (ax - cx) * (y - cy);
    i64 n2 = den - n0 - n1;
    if (den < 0) {
        den = -den;
        n0 = -n0;
        n1 = -n1;
        n2 = -n2;
    }
    const i64 eps = -(den / 10000);
    return n0 >= eps && n1 >= eps && n2 >= eps;
}

inline void normalize4_q(i32 out[4]) {
    i32 m = out[0];
    if (out[1] > m) m = out[1];
    if (out[2] > m) m = out[2];
    if (out[3] > m) m = out[3];
    if (m <= kQ) return;
    for (int i = 0; i < 4; ++i) {
        out[i] = static_cast<i32>((static_cast<i64>(out[i]) * kQ) / m);
    }
}

inline bool solve_ls2_endpoint_q(const i32 X[3], const int active[2], i32 out[4]) {
    out[0] = out[1] = out[2] = out[3] = 0;
    const i32* A = g_state.P[active[0]];
    const i32* B = g_state.P[active[1]];
    i64 aa = 0, ab = 0, bb = 0, ax = 0, bx = 0;
    for (int i = 0; i < 3; ++i) {
        aa += mul_q(A[i], A[i]);
        ab += mul_q(A[i], B[i]);
        bb += mul_q(B[i], B[i]);
        ax += mul_q(A[i], X[i]);
        bx += mul_q(B[i], X[i]);
    }
    const i64 det = aa * bb - ab * ab;
    if (det == 0) return false;

    i64 t0 = ((ax * bb - bx * ab) * kQ) / det;
    i64 t1 = ((bx * aa - ax * ab) * kQ) / det;
    if (t0 < 0 && t1 >= 0) {
        t0 = 0;
        t1 = bb > 0 ? (bx * kQ) / bb : 0;
    } else if (t1 < 0 && t0 >= 0) {
        t1 = 0;
        t0 = aa > 0 ? (ax * kQ) / aa : 0;
    } else if (t0 < 0 && t1 < 0) {
        t0 = 0;
        t1 = 0;
    }
    out[active[0]] = clamp_q(t0);
    out[active[1]] = clamp_q(t1);
    normalize4_q(out);
    return true;
}

inline bool solve_interior_endpoint_q(const i32 X[3], i32 out[4]) {
    out[0] = out[1] = out[2] = out[3] = 0;
    i32 xy[2];
    if (!xy_from_X(X, xy)) return true;

    struct SubGamut {
        const i32* a_xy;
        const i32* b_xy;
        const i32* c_xy;
        int inv_idx;
        int out_a;
        int out_b;
        int out_c;
    };
    const SubGamut sgs[3] = {
        {g_state.xy_r, g_state.xy_g, g_state.xy_w, 0, 0, 1, 3},
        {g_state.xy_r, g_state.xy_b, g_state.xy_w, 1, 0, 2, 3},
        {g_state.xy_b, g_state.xy_g, g_state.xy_w, 2, 2, 1, 3},
    };

    for (int k = 0; k < 3; ++k) {
        const SubGamut& sg = sgs[k];
        if (!tri_contains(xy, sg.a_xy, sg.b_xy, sg.c_xy)) continue;
        i32 t[3];
        matvec_q(g_state.inv[sg.inv_idx], X, t);
        const i32 eps = -(kQ / 10000);
        if (t[0] < eps || t[1] < eps || t[2] < eps) continue;
        out[sg.out_a] = clamp_q(t[0]);
        out[sg.out_b] = clamp_q(t[1]);
        out[sg.out_c] = clamp_q(t[2]);
        normalize4_q(out);
        return true;
    }
    return false;
}

inline bool solve_interior_endpoint_seed_q(const i32 X[3], int seed, i32 out[4]) {
    if (seed < 0 || seed > 2) {
        return solve_interior_endpoint_q(X, out);
    }

    out[0] = out[1] = out[2] = out[3] = 0;
    static const int kOutMap[3][3] = {
        {0, 1, 3},
        {0, 2, 3},
        {2, 1, 3},
    };
    i32 t[3];
    matvec_q(g_state.inv[seed], X, t);
    const i32 eps = -(kQ / 10000);
    if (t[0] >= eps && t[1] >= eps && t[2] >= eps) {
        out[kOutMap[seed][0]] = clamp_q(t[0]);
        out[kOutMap[seed][1]] = clamp_q(t[1]);
        out[kOutMap[seed][2]] = clamp_q(t[2]);
        normalize4_q(out);
        return true;
    }
    return solve_interior_endpoint_q(X, out);
}

inline void setup(const DiodeProfile& profile) {
    strategy_common::build_default_cache(profile, &g_state.float_cache);
    const ProfileCache& c = g_state.float_cache;
    g_state.has_source_space = c.has_source_space;
    g_state.xy_r[0] = q(profile.xy_r[0]); g_state.xy_r[1] = q(profile.xy_r[1]);
    g_state.xy_g[0] = q(profile.xy_g[0]); g_state.xy_g[1] = q(profile.xy_g[1]);
    g_state.xy_b[0] = q(profile.xy_b[0]); g_state.xy_b[1] = q(profile.xy_b[1]);
    g_state.xy_w[0] = q(c.xy_w[0]);       g_state.xy_w[1] = q(c.xy_w[1]);

    const float* cols[4] = {c.P_R, c.P_G, c.P_B, c.P_W};
    for (int ch = 0; ch < 4; ++ch) {
        for (int row = 0; row < 3; ++row) {
            g_state.P[ch][row] = q(cols[ch][row]);
        }
    }
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            g_state.M_src[row][col] = q(c.M_src[row][col]);
            g_state.inv[0][row][col] = q(c.P_RGW_inv[row][col]);
            g_state.inv[1][row][col] = q(c.P_RBW_inv[row][col]);
            g_state.inv[2][row][col] = q(c.P_BGW_inv[row][col]);
        }
    }
}

inline void solve(u8 r, u8 g, u8 b, u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    const u8 value = fl::max(fl::max(r, g), b);
    if (value == 0) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }
    const int active_count = (r > 0 ? 1 : 0) + (g > 0 ? 1 : 0) + (b > 0 ? 1 : 0);
    if (is_native_input_gamut(*g_state.float_cache.profile) && active_count == 1) {
        *out_r = r;
        *out_g = g;
        *out_b = b;
        *out_w = 0;
        return;
    }

    i32 X[3];
    source_full_chroma_q(r, g, b, value, X);

    i32 endpoint[4];
    bool ok = false;
    if (is_native_input_gamut(*g_state.float_cache.profile) && active_count == 2) {
        int active[2];
        int n = 0;
        if (r > 0) active[n++] = 0;
        if (g > 0) active[n++] = 1;
        if (b > 0) active[n++] = 2;
        ok = solve_ls2_endpoint_q(X, active, endpoint);
    } else {
        ok = solve_interior_endpoint_q(X, endpoint);
    }

    if (!ok) {
        strategy_common::exact_fallback(r, g, b, out_r, out_g, out_b, out_w);
        return;
    }
    *out_r = q_to_scaled_u8(endpoint[0], value);
    *out_g = q_to_scaled_u8(endpoint[1], value);
    *out_b = q_to_scaled_u8(endpoint[2], value);
    *out_w = q_to_scaled_u8(endpoint[3], value);
}

inline void track_state_coefficients(Q16RangeStats& stats) {
    for (int ch = 0; ch < 4; ++ch) {
        for (int row = 0; row < 3; ++row) {
            strategy_common::track_abs(g_state.P[ch][row],
                                       &stats.max_abs_coeff_q);
        }
    }
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            strategy_common::track_abs(g_state.M_src[row][col],
                                       &stats.max_abs_coeff_q);
            for (int inv_idx = 0; inv_idx < 3; ++inv_idx) {
                strategy_common::track_abs(g_state.inv[inv_idx][row][col],
                                           &stats.max_abs_coeff_q);
            }
        }
    }
}

inline i64 mul_q_audit(i64 a, i64 b, Q16RangeStats& stats) {
    const i64 product = a * b;
    strategy_common::track_abs(product, &stats.max_abs_product);
    return product / kQ;
}

inline void matvec_q_audit(const i32 M[3][3],
                           const i32 in[3],
                           i32 out[3],
                           Q16RangeStats& stats) {
    for (int row = 0; row < 3; ++row) {
        i64 acc = 0;
        for (int col = 0; col < 3; ++col) {
            const i64 product =
                static_cast<i64>(M[row][col]) * static_cast<i64>(in[col]);
            strategy_common::track_abs(product, &stats.max_abs_product);
            acc += product;
            strategy_common::track_abs(acc, &stats.max_abs_accumulator);
        }
        out[row] = static_cast<i32>(acc / kQ);
        strategy_common::track_abs(out[row], &stats.max_abs_output_q);
    }
}

inline void source_full_chroma_q_audit(u8 r,
                                       u8 g,
                                       u8 b,
                                       u8 value,
                                       i32 X[3],
                                       Q16RangeStats& stats) {
    strategy_common::track_divisor(value, stats);
    const i32 src[3] = {
        static_cast<i32>((static_cast<i64>(r) * kQ) / value),
        static_cast<i32>((static_cast<i64>(g) * kQ) / value),
        static_cast<i32>((static_cast<i64>(b) * kQ) / value),
    };
    for (int i = 0; i < 3; ++i) {
        strategy_common::track_abs(src[i], &stats.max_abs_src_q);
    }
    if (g_state.has_source_space) {
        matvec_q_audit(g_state.M_src, src, X, stats);
        return;
    }
    for (int row = 0; row < 3; ++row) {
        i64 acc = 0;
        for (int ch = 0; ch < 3; ++ch) {
            const i64 product =
                static_cast<i64>(g_state.P[ch][row]) * src[ch];
            strategy_common::track_abs(product, &stats.max_abs_product);
            acc += product;
            strategy_common::track_abs(acc, &stats.max_abs_accumulator);
        }
        X[row] = static_cast<i32>(acc / kQ);
        strategy_common::track_abs(X[row], &stats.max_abs_output_q);
    }
}

inline void track_endpoint4(const i32 out[4], Q16RangeStats& stats) {
    for (int i = 0; i < 4; ++i) {
        strategy_common::track_abs(out[i], &stats.max_abs_endpoint_q);
        strategy_common::track_abs(out[i], &stats.max_abs_output_q);
    }
}

inline bool solve_ls2_endpoint_q_audit(const i32 X[3],
                                       const int active[2],
                                       i32 out[4],
                                       Q16RangeStats& stats) {
    out[0] = out[1] = out[2] = out[3] = 0;
    const i32* A = g_state.P[active[0]];
    const i32* B = g_state.P[active[1]];
    i64 aa = 0, ab = 0, bb = 0, ax = 0, bx = 0;
    for (int i = 0; i < 3; ++i) {
        aa += mul_q_audit(A[i], A[i], stats);
        ab += mul_q_audit(A[i], B[i], stats);
        bb += mul_q_audit(B[i], B[i], stats);
        ax += mul_q_audit(A[i], X[i], stats);
        bx += mul_q_audit(B[i], X[i], stats);
        strategy_common::track_abs(aa, &stats.max_abs_accumulator);
        strategy_common::track_abs(ab, &stats.max_abs_accumulator);
        strategy_common::track_abs(bb, &stats.max_abs_accumulator);
        strategy_common::track_abs(ax, &stats.max_abs_accumulator);
        strategy_common::track_abs(bx, &stats.max_abs_accumulator);
    }
    const i64 det = aa * bb - ab * ab;
    strategy_common::track_det(det, stats);
    if (det == 0) return false;
    strategy_common::track_divisor(det, stats);

    const i64 num0 = ax * bb - bx * ab;
    const i64 num1 = bx * aa - ax * ab;
    const i64 scaled0 = num0 * kQ;
    const i64 scaled1 = num1 * kQ;
    strategy_common::track_abs(scaled0, &stats.max_abs_numerator);
    strategy_common::track_abs(scaled1, &stats.max_abs_numerator);
    i64 t0 = scaled0 / det;
    i64 t1 = scaled1 / det;
    strategy_common::track_abs(t0, &stats.max_abs_output_q);
    strategy_common::track_abs(t1, &stats.max_abs_output_q);
    if (t0 < 0 && t1 >= 0) {
        t0 = 0;
        if (bb > 0) {
            strategy_common::track_divisor(bb, stats);
            t1 = (bx * kQ) / bb;
        } else {
            t1 = 0;
        }
    } else if (t1 < 0 && t0 >= 0) {
        t1 = 0;
        if (aa > 0) {
            strategy_common::track_divisor(aa, stats);
            t0 = (ax * kQ) / aa;
        } else {
            t0 = 0;
        }
    } else if (t0 < 0 && t1 < 0) {
        t0 = 0;
        t1 = 0;
    }
    strategy_common::track_abs(t0, &stats.max_abs_output_q);
    strategy_common::track_abs(t1, &stats.max_abs_output_q);
    out[active[0]] = clamp_q(t0);
    out[active[1]] = clamp_q(t1);
    track_endpoint4(out, stats);
    strategy_common::track_normalize4_divisor(out, stats);
    normalize4_q(out);
    track_endpoint4(out, stats);
    return true;
}

inline bool solve_interior_endpoint_q_audit(const i32 X[3],
                                            i32 out[4],
                                            Q16RangeStats& stats) {
    out[0] = out[1] = out[2] = out[3] = 0;
    i32 xy[2];
    if (!xy_from_X_audit(X, xy, stats)) return true;

    struct SubGamut {
        const i32* a_xy;
        const i32* b_xy;
        const i32* c_xy;
        int inv_idx;
        int out_a;
        int out_b;
        int out_c;
    };
    const SubGamut sgs[3] = {
        {g_state.xy_r, g_state.xy_g, g_state.xy_w, 0, 0, 1, 3},
        {g_state.xy_r, g_state.xy_b, g_state.xy_w, 1, 0, 2, 3},
        {g_state.xy_b, g_state.xy_g, g_state.xy_w, 2, 2, 1, 3},
    };

    for (int k = 0; k < 3; ++k) {
        const SubGamut& sg = sgs[k];
        if (!tri_contains(xy, sg.a_xy, sg.b_xy, sg.c_xy)) continue;
        i32 t[3];
        matvec_q_audit(g_state.inv[sg.inv_idx], X, t, stats);
        const i32 eps = -(kQ / 10000);
        if (t[0] < eps || t[1] < eps || t[2] < eps) continue;
        out[sg.out_a] = clamp_q(t[0]);
        out[sg.out_b] = clamp_q(t[1]);
        out[sg.out_c] = clamp_q(t[2]);
        track_endpoint4(out, stats);
        strategy_common::track_normalize4_divisor(out, stats);
        normalize4_q(out);
        track_endpoint4(out, stats);
        return true;
    }
    return false;
}

inline bool solve_interior_endpoint_seed_q_audit(
    const i32 X[3],
    int seed,
    i32 out[4],
    Q16RangeStats& stats) {
    if (seed < 0 || seed > 2) {
        ++stats.seeded_fallback_count;
        return solve_interior_endpoint_q_audit(X, out, stats);
    }

    out[0] = out[1] = out[2] = out[3] = 0;
    static const int kOutMap[3][3] = {
        {0, 1, 3},
        {0, 2, 3},
        {2, 1, 3},
    };
    i32 t[3];
    matvec_q_audit(g_state.inv[seed], X, t, stats);
    const i32 eps = -(kQ / 10000);
    if (t[0] >= eps && t[1] >= eps && t[2] >= eps) {
        out[kOutMap[seed][0]] = clamp_q(t[0]);
        out[kOutMap[seed][1]] = clamp_q(t[1]);
        out[kOutMap[seed][2]] = clamp_q(t[2]);
        track_endpoint4(out, stats);
        strategy_common::track_normalize4_divisor(out, stats);
        normalize4_q(out);
        track_endpoint4(out, stats);
        return true;
    }
    ++stats.seeded_fallback_count;
    return solve_interior_endpoint_q_audit(X, out, stats);
}

inline Q16RangeStats audit_current_state(
    int cube_steps,
    bool use_seed,
    int (*seed_for_X_fn)(const i32 X[3])) {
    Q16RangeStats stats{};
    if (cube_steps <= 1) return stats;
    stats.measured = true;
    track_state_coefficients(stats);

    for (int ri = 0; ri < cube_steps; ++ri) {
        const u8 r = cube_value(ri, cube_steps);
        for (int gi = 0; gi < cube_steps; ++gi) {
            const u8 g = cube_value(gi, cube_steps);
            for (int bi = 0; bi < cube_steps; ++bi) {
                const u8 b = cube_value(bi, cube_steps);
                ++stats.sample_count;
                const u8 value = fl::max(fl::max(r, g), b);
                if (value == 0) continue;
                const int active_count =
                    (r > 0 ? 1 : 0) + (g > 0 ? 1 : 0) + (b > 0 ? 1 : 0);
                if (is_native_input_gamut(*g_state.float_cache.profile)
                    && active_count == 1) {
                    continue;
                }

                i32 X[3];
                source_full_chroma_q_audit(r, g, b, value, X, stats);

                i32 endpoint[4];
                bool ok = false;
                if (is_native_input_gamut(*g_state.float_cache.profile)
                    && active_count == 2) {
                    int active[2];
                    int n = 0;
                    if (r > 0) active[n++] = 0;
                    if (g > 0) active[n++] = 1;
                    if (b > 0) active[n++] = 2;
                    ok = solve_ls2_endpoint_q_audit(X, active, endpoint, stats);
                } else if (use_seed && seed_for_X_fn != nullptr) {
                    ok = solve_interior_endpoint_seed_q_audit(
                        X, seed_for_X_fn(X), endpoint, stats);
                } else {
                    ok = solve_interior_endpoint_q_audit(X, endpoint, stats);
                }
                if (!ok) {
                    ++stats.fallback_count;
                }
            }
        }
    }
    return stats;
}

inline Q16RangeStats audit_range(const DiodeProfile& profile, int cube_steps) {
    setup(profile);
    return audit_current_state(cube_steps, false, nullptr);
}

inline void teardown() {}

}  // namespace strategy_fixed_analytical_q16

namespace strategy_experimental_hermite_fixed_eval {

static const int kN = 16;
static const int kCoordQ = strategy_fixed_analytical_q16::kQ;
static const int kBasisQ = 32768;

struct State {
    strategy_experimental_hermite::State table;
    i32 axis_x_q[strategy_experimental_hermite::kMaxN] = {};
    i32 axis_y_q[strategy_experimental_hermite::kMaxN] = {};
};

static State g_cell_q16;
static State g_hull_adaptive;

inline i32 q_coord(float v) {
    return strategy_common::quantize_q(v, kCoordQ);
}

inline i64 div_round(i64 num, i64 den) {
    if (den <= 0) return 0;
    if (num >= 0) return (num + (den / 2)) / den;
    return -((-num + (den / 2)) / den);
}

inline i32 qmul_basis(i32 a, i32 b) {
    return static_cast<i32>(
        div_round(static_cast<i64>(a) * static_cast<i64>(b), kBasisQ));
}

inline void hermite_basis_q(i32 t, i32 out[4]) {
    const i32 t2 = qmul_basis(t, t);
    const i32 t3 = qmul_basis(t2, t);
    out[0] = 2 * t3 - 3 * t2 + kBasisQ;
    out[1] = -2 * t3 + 3 * t2;
    out[2] = t3 - 2 * t2 + t;
    out[3] = t3 - t2;
}

inline int find_interval_q(const i32* axis, int N, i32 x) {
    if (x <= axis[0]) return 0;
    if (x >= axis[N - 1]) return N - 2;
    int lo = 0;
    int hi = N - 1;
    while (hi - lo > 1) {
        const int mid = (lo + hi) >> 1;
        if (axis[mid] <= x) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return lo;
}

inline i32 frac_basis_q(i32 x, i32 lo, i32 hi) {
    const i32 span = hi - lo;
    if (span <= 0) return 0;
    i64 scaled = (static_cast<i64>(x - lo) * kBasisQ) / span;
    if (scaled < 0) scaled = 0;
    if (scaled > kBasisQ) scaled = kBasisQ;
    return static_cast<i32>(scaled);
}

inline i64 basis2(i64 value_q, i32 bx, i32 by) {
    return div_round(value_q * static_cast<i64>(bx) * static_cast<i64>(by),
                     static_cast<i64>(kBasisQ) * static_cast<i64>(kBasisQ));
}

inline i64 slope_scaled_q(i32 slope_q, i32 cell_span_q) {
    return div_round(static_cast<i64>(slope_q) * static_cast<i64>(cell_span_q),
                     kCoordQ);
}

inline void source_xyz_q(u8 r, u8 g, u8 b, i32 X[3]) {
    using namespace strategy_fixed_analytical_q16;
    const i32 src[3] = {
        static_cast<i32>((static_cast<i64>(r) * kCoordQ + 127) / 255),
        static_cast<i32>((static_cast<i64>(g) * kCoordQ + 127) / 255),
        static_cast<i32>((static_cast<i64>(b) * kCoordQ + 127) / 255),
    };
    if (strategy_fixed_analytical_q16::g_state.has_source_space) {
        matvec_q(strategy_fixed_analytical_q16::g_state.M_src, src, X);
        return;
    }
    for (int row = 0; row < 3; ++row) {
        const i64 acc =
              static_cast<i64>(strategy_fixed_analytical_q16::g_state.P[0][row]) * src[0]
            + static_cast<i64>(strategy_fixed_analytical_q16::g_state.P[1][row]) * src[1]
            + static_cast<i64>(strategy_fixed_analytical_q16::g_state.P[2][row]) * src[2];
        X[row] = static_cast<i32>((acc + (kCoordQ / 2)) / kCoordQ);
    }
}

inline void lookup_state_q(const State& state, const i32 xy_q[2], i32 Y_q,
                           i64 out[4]) {
    out[0] = out[1] = out[2] = out[3] = 0;
    if (state.table.N < 2 || state.table.cells.get() == nullptr) return;

    const int i = find_interval_q(state.axis_x_q, state.table.N, xy_q[0]);
    const int j = find_interval_q(state.axis_y_q, state.table.N, xy_q[1]);
    const i32 cell_dx_q = state.axis_x_q[i + 1] - state.axis_x_q[i];
    const i32 cell_dy_q = state.axis_y_q[j + 1] - state.axis_y_q[j];
    if (cell_dx_q <= 0 || cell_dy_q <= 0) return;

    const i32 fx = frac_basis_q(xy_q[0], state.axis_x_q[i], state.axis_x_q[i + 1]);
    const i32 fy = frac_basis_q(xy_q[1], state.axis_y_q[j], state.axis_y_q[j + 1]);
    i32 bx[4];
    i32 by[4];
    hermite_basis_q(fx, bx);
    hermite_basis_q(fy, by);

    const i32* c00 = strategy_experimental_hermite::cell_const(state.table, i, j);
    const i32* c10 = strategy_experimental_hermite::cell_const(state.table, i + 1, j);
    const i32* c01 = strategy_experimental_hermite::cell_const(state.table, i, j + 1);
    const i32* c11 = strategy_experimental_hermite::cell_const(state.table, i + 1, j + 1);

    for (int k = 0; k < 4; ++k) {
        i64 per_y_q =
              basis2(c00[k], bx[0], by[0]) + basis2(c10[k], bx[1], by[0])
            + basis2(c01[k], bx[0], by[1]) + basis2(c11[k], bx[1], by[1])
            + basis2(slope_scaled_q(c00[4 + k], cell_dx_q), bx[2], by[0])
            + basis2(slope_scaled_q(c10[4 + k], cell_dx_q), bx[3], by[0])
            + basis2(slope_scaled_q(c01[4 + k], cell_dx_q), bx[2], by[1])
            + basis2(slope_scaled_q(c11[4 + k], cell_dx_q), bx[3], by[1])
            + basis2(slope_scaled_q(c00[8 + k], cell_dy_q), bx[0], by[2])
            + basis2(slope_scaled_q(c10[8 + k], cell_dy_q), bx[1], by[2])
            + basis2(slope_scaled_q(c01[8 + k], cell_dy_q), bx[0], by[3])
            + basis2(slope_scaled_q(c11[8 + k], cell_dy_q), bx[1], by[3]);
        out[k] = div_round(per_y_q * static_cast<i64>(Y_q), kCoordQ);
        if (out[k] < 0) out[k] = 0;
    }

    i64 m = out[0];
    if (out[1] > m) m = out[1];
    if (out[2] > m) m = out[2];
    if (out[3] > m) m = out[3];
    if (m > state.table.q) {
        for (int k = 0; k < 4; ++k) {
            out[k] = div_round(out[k] * static_cast<i64>(state.table.q), m);
        }
    }
}

inline u8 table_q_to_u8(i64 qv, int table_q) {
    if (qv <= 0) return 0;
    if (qv >= table_q) return 255;
    return static_cast<u8>(
        (qv * 255 + (static_cast<i64>(table_q) / 2)) / table_q);
}

inline void solve_state(State& state, u8 r, u8 g, u8 b,
                        u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    if ((r | g | b) == 0) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }
    const int active_count = (r > 0 ? 1 : 0) + (g > 0 ? 1 : 0) + (b > 0 ? 1 : 0);
    if (active_count < 3) {
        strategy_fixed_analytical_q16::solve(r, g, b, out_r, out_g, out_b, out_w);
        return;
    }

    i32 X[3];
    source_xyz_q(r, g, b, X);
    i32 xy[2];
    if (!strategy_fixed_analytical_q16::xy_from_X(X, xy)) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }

    i64 out[4];
    lookup_state_q(state, xy, X[1], out);
    *out_r = table_q_to_u8(out[0], state.table.q);
    *out_g = table_q_to_u8(out[1], state.table.q);
    *out_b = table_q_to_u8(out[2], state.table.q);
    *out_w = table_q_to_u8(out[3], state.table.q);
}

inline void setup_state_n(State& state, const DiodeProfile& profile,
                          int n, int table_q, bool warped) {
    strategy_experimental_hermite::build_state(
        state.table, profile, n, table_q, warped);
    strategy_fixed_analytical_q16::setup(profile);
    for (int i = 0; i < state.table.N; ++i) {
        state.axis_x_q[i] = q_coord(state.table.axis_x[i]);
        state.axis_y_q[i] = q_coord(state.table.axis_y[i]);
    }
}

inline void setup_state(State& state, const DiodeProfile& profile,
                        int table_q, bool warped) {
    setup_state_n(state, profile, kN, table_q, warped);
}

inline void setup_cell_q16(const DiodeProfile& profile) {
    setup_state(g_cell_q16, profile, 65536, false);
}
inline void solve_cell_q16(u8 r, u8 g, u8 b, u8* or_, u8* og, u8* ob, u8* ow) {
    solve_state(g_cell_q16, r, g, b, or_, og, ob, ow);
}
inline void teardown_cell_q16() {
    g_cell_q16.table.cells.reset();
    g_cell_q16.table.N = 0;
}

inline void setup_hull_adaptive(const DiodeProfile& profile) {
    setup_state(g_hull_adaptive, profile, 4096, true);
}
inline void solve_hull_adaptive(u8 r, u8 g, u8 b, u8* or_, u8* og, u8* ob, u8* ow) {
    solve_state(g_hull_adaptive, r, g, b, or_, og, ob, ow);
}
inline void teardown_hull_adaptive() {
    g_hull_adaptive.table.cells.reset();
    g_hull_adaptive.table.N = 0;
}

}  // namespace strategy_experimental_hermite_fixed_eval

namespace strategy_bilinear_n16_fixed_interp {

static const int kN = 16;
static const int kLutScale = kLutQ;
static const int kCoordQ = strategy_fixed_analytical_q16::kQ;

struct State {
    ProfileCache cache;
    LutTable lut;
    i32 xy_min_q[2] = {0, 0};
    i32 xy_max_q[2] = {0, 0};
};

static State g_state;

inline i32 q(float v) {
    return strategy_common::quantize_q(v, kCoordQ);
}

inline const i16* cell_const(int i, int j) {
    return &g_state.lut.cells[(j * g_state.lut.N + i) * kLutStrideBilinear];
}

inline void setup(const DiodeProfile& profile) {
    strategy_common::build_default_cache(profile, &g_state.cache);
    strategy_fixed_analytical_q16::setup(profile);
    g_state.lut = build_lut(g_state.cache, kN, LutInterp::Bilinear);
    g_state.xy_min_q[0] = q(g_state.lut.xy_min[0]);
    g_state.xy_min_q[1] = q(g_state.lut.xy_min[1]);
    g_state.xy_max_q[0] = q(g_state.lut.xy_max[0]);
    g_state.xy_max_q[1] = q(g_state.lut.xy_max[1]);
}

inline void source_xyz_q(u8 r, u8 g, u8 b, i32 X[3]) {
    using namespace strategy_fixed_analytical_q16;
    const i32 src[3] = {
        static_cast<i32>((static_cast<i64>(r) * kCoordQ + 127) / 255),
        static_cast<i32>((static_cast<i64>(g) * kCoordQ + 127) / 255),
        static_cast<i32>((static_cast<i64>(b) * kCoordQ + 127) / 255),
    };
    if (strategy_fixed_analytical_q16::g_state.has_source_space) {
        matvec_q(strategy_fixed_analytical_q16::g_state.M_src, src, X);
        return;
    }
    for (int row = 0; row < 3; ++row) {
        const i64 acc =
              static_cast<i64>(strategy_fixed_analytical_q16::g_state.P[0][row]) * src[0]
            + static_cast<i64>(strategy_fixed_analytical_q16::g_state.P[1][row]) * src[1]
            + static_cast<i64>(strategy_fixed_analytical_q16::g_state.P[2][row]) * src[2];
        X[row] = static_cast<i32>((acc + (kCoordQ / 2)) / kCoordQ);
    }
}

inline void coordinate(int axis, i32 xy_q, int* index, i32* frac_q) {
    const i32 lo = g_state.xy_min_q[axis];
    const i32 hi = g_state.xy_max_q[axis];
    const i32 span = hi - lo;
    if (span <= 0) {
        *index = 0;
        *frac_q = 0;
        return;
    }
    i64 scaled = (static_cast<i64>(xy_q - lo) * (kN - 1) * kCoordQ) / span;
    if (scaled < 0) scaled = 0;
    const i64 max_scaled = static_cast<i64>(kN - 1) * kCoordQ;
    if (scaled > max_scaled) scaled = max_scaled;
    int i = static_cast<int>(scaled / kCoordQ);
    i32 f = static_cast<i32>(scaled - static_cast<i64>(i) * kCoordQ);
    if (i >= kN - 1) {
        i = kN - 2;
        f = kCoordQ;
    }
    *index = i;
    *frac_q = f;
}

inline i32 lerp_q(i32 a, i32 b, i32 f_q) {
    return static_cast<i32>(
        (static_cast<i64>(a) * (kCoordQ - f_q)
       + static_cast<i64>(b) * f_q
       + (kCoordQ / 2)) / kCoordQ);
}

inline void normalize4_lut_q(i32 out[4]) {
    i32 m = out[0];
    if (out[1] > m) m = out[1];
    if (out[2] > m) m = out[2];
    if (out[3] > m) m = out[3];
    if (m <= kLutScale) return;
    for (int k = 0; k < 4; ++k) {
        out[k] = static_cast<i32>(
            (static_cast<i64>(out[k]) * kLutScale + (m / 2)) / m);
    }
}

inline u8 lut_q_to_u8(i32 qv) {
    if (qv <= 0) return 0;
    const i64 scaled = static_cast<i64>(qv) * 255 + (kLutScale / 2);
    const i64 out = scaled / kLutScale;
    if (out >= 255) return 255;
    return static_cast<u8>(out);
}

inline void solve(u8 r, u8 g, u8 b, u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    if ((r | g | b) == 0) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }
    const int active_count = (r > 0 ? 1 : 0) + (g > 0 ? 1 : 0) + (b > 0 ? 1 : 0);
    if (active_count < 3) {
        strategy_fixed_analytical_q16::solve(r, g, b, out_r, out_g, out_b, out_w);
        return;
    }

    i32 X[3];
    source_xyz_q(r, g, b, X);
    i32 xy[2];
    if (!strategy_fixed_analytical_q16::xy_from_X(X, xy)) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }

    int ix, iy;
    i32 fx, fy;
    coordinate(0, xy[0], &ix, &fx);
    coordinate(1, xy[1], &iy, &fy);

    const i16* c00 = cell_const(ix, iy);
    const i16* c10 = cell_const(ix + 1, iy);
    const i16* c01 = cell_const(ix, iy + 1);
    const i16* c11 = cell_const(ix + 1, iy + 1);
    i32 out[4];
    for (int k = 0; k < 4; ++k) {
        const i32 x0 = lerp_q(c00[k], c10[k], fx);
        const i32 x1 = lerp_q(c01[k], c11[k], fx);
        const i32 per_y = lerp_q(x0, x1, fy);
        out[k] = static_cast<i32>(
            (static_cast<i64>(per_y) * X[1] + (kCoordQ / 2)) / kCoordQ);
    }
    normalize4_lut_q(out);
    *out_r = lut_q_to_u8(out[0]);
    *out_g = lut_q_to_u8(out[1]);
    *out_b = lut_q_to_u8(out[2]);
    *out_w = lut_q_to_u8(out[3]);
}

inline void teardown() {
    g_state.lut.cells.reset();
    g_state.lut.N = 0;
}

}  // namespace strategy_bilinear_n16_fixed_interp

namespace strategy_hermite_fixed_lookup {

static const int kLutScale = kLutQ;
static const int kCoordQ = strategy_fixed_analytical_q16::kQ;
static const int kBasisQ = 32768;

struct State {
    ProfileCache cache;
    LutTable lut;
    i32 xy_min_q[2] = {0, 0};
    i32 xy_max_q[2] = {0, 0};
};

static State g_hermite16;
static State g_hermite32;

inline i32 q(float v) {
    return strategy_common::quantize_q(v, kCoordQ);
}

inline i64 div_round(i64 num, i64 den) {
    if (den <= 0) return 0;
    if (num >= 0) return (num + (den / 2)) / den;
    return -((-num + (den / 2)) / den);
}

inline i32 qmul_basis(i32 a, i32 b) {
    return static_cast<i32>(
        div_round(static_cast<i64>(a) * static_cast<i64>(b), kBasisQ));
}

inline void hermite_basis_q(i32 t, i32 out[4]) {
    const i32 t2 = qmul_basis(t, t);
    const i32 t3 = qmul_basis(t2, t);
    out[0] = 2 * t3 - 3 * t2 + kBasisQ;
    out[1] = -2 * t3 + 3 * t2;
    out[2] = t3 - 2 * t2 + t;
    out[3] = t3 - t2;
}

inline const i16* cell_const(const State& state, int i, int j) {
    return &state.lut.cells[(j * state.lut.N + i) * kLutStrideHermite];
}

inline void setup_state(State& state, const DiodeProfile& profile, int n) {
    strategy_common::build_default_cache(profile, &state.cache);
    strategy_fixed_analytical_q16::setup(profile);
    state.lut = build_lut(state.cache, n, LutInterp::Hermite);
    state.xy_min_q[0] = q(state.lut.xy_min[0]);
    state.xy_min_q[1] = q(state.lut.xy_min[1]);
    state.xy_max_q[0] = q(state.lut.xy_max[0]);
    state.xy_max_q[1] = q(state.lut.xy_max[1]);
}

inline void source_xyz_q(u8 r, u8 g, u8 b, i32 X[3]) {
    using namespace strategy_fixed_analytical_q16;
    const i32 src[3] = {
        static_cast<i32>((static_cast<i64>(r) * kCoordQ + 127) / 255),
        static_cast<i32>((static_cast<i64>(g) * kCoordQ + 127) / 255),
        static_cast<i32>((static_cast<i64>(b) * kCoordQ + 127) / 255),
    };
    if (strategy_fixed_analytical_q16::g_state.has_source_space) {
        matvec_q(strategy_fixed_analytical_q16::g_state.M_src, src, X);
        return;
    }
    for (int row = 0; row < 3; ++row) {
        const i64 acc =
              static_cast<i64>(strategy_fixed_analytical_q16::g_state.P[0][row]) * src[0]
            + static_cast<i64>(strategy_fixed_analytical_q16::g_state.P[1][row]) * src[1]
            + static_cast<i64>(strategy_fixed_analytical_q16::g_state.P[2][row]) * src[2];
        X[row] = static_cast<i32>((acc + (kCoordQ / 2)) / kCoordQ);
    }
}

inline void coordinate(const State& state, int axis, i32 xy_q,
                       int* index, i32* frac_q) {
    const i32 lo = state.xy_min_q[axis];
    const i32 hi = state.xy_max_q[axis];
    const i32 span = hi - lo;
    if (state.lut.N < 2 || span <= 0) {
        *index = 0;
        *frac_q = 0;
        return;
    }
    i64 scaled = (static_cast<i64>(xy_q - lo) * (state.lut.N - 1) * kBasisQ)
               / span;
    if (scaled < 0) scaled = 0;
    const i64 max_scaled = static_cast<i64>(state.lut.N - 1) * kBasisQ;
    if (scaled > max_scaled) scaled = max_scaled;
    int i = static_cast<int>(scaled / kBasisQ);
    i32 f = static_cast<i32>(scaled - static_cast<i64>(i) * kBasisQ);
    if (i >= state.lut.N - 1) {
        i = state.lut.N - 2;
        f = kBasisQ;
    }
    *index = i;
    *frac_q = f;
}

inline i64 basis2(i64 value_q, i32 bx, i32 by) {
    return div_round(value_q * static_cast<i64>(bx) * static_cast<i64>(by),
                     static_cast<i64>(kBasisQ) * static_cast<i64>(kBasisQ));
}

inline void normalize4_lut_q(i32 out[4]) {
    i32 m = out[0];
    if (out[1] > m) m = out[1];
    if (out[2] > m) m = out[2];
    if (out[3] > m) m = out[3];
    if (m <= kLutScale) return;
    for (int k = 0; k < 4; ++k) {
        out[k] = static_cast<i32>(
            (static_cast<i64>(out[k]) * kLutScale + (m / 2)) / m);
    }
}

inline u8 lut_q_to_u8(i32 qv) {
    if (qv <= 0) return 0;
    const i64 scaled = static_cast<i64>(qv) * 255 + (kLutScale / 2);
    const i64 out = scaled / kLutScale;
    if (out >= 255) return 255;
    return static_cast<u8>(out);
}

inline void solve_state(State& state, u8 r, u8 g, u8 b,
                        u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    if ((r | g | b) == 0) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }
    const int active_count = (r > 0 ? 1 : 0) + (g > 0 ? 1 : 0) + (b > 0 ? 1 : 0);
    if (active_count < 3) {
        strategy_fixed_analytical_q16::solve(r, g, b, out_r, out_g, out_b, out_w);
        return;
    }

    i32 X[3];
    source_xyz_q(r, g, b, X);
    i32 xy[2];
    if (!strategy_fixed_analytical_q16::xy_from_X(X, xy)) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }

    int ix, iy;
    i32 fx, fy;
    coordinate(state, 0, xy[0], &ix, &fx);
    coordinate(state, 1, xy[1], &iy, &fy);
    i32 bx[4];
    i32 by[4];
    hermite_basis_q(fx, bx);
    hermite_basis_q(fy, by);

    const i16* c00 = cell_const(state, ix, iy);
    const i16* c10 = cell_const(state, ix + 1, iy);
    const i16* c01 = cell_const(state, ix, iy + 1);
    const i16* c11 = cell_const(state, ix + 1, iy + 1);

    i32 out[4];
    for (int k = 0; k < 4; ++k) {
        const i64 per_y =
              basis2(c00[k], bx[0], by[0]) + basis2(c10[k], bx[1], by[0])
            + basis2(c01[k], bx[0], by[1]) + basis2(c11[k], bx[1], by[1])
            + basis2(c00[4 + k], bx[2], by[0])
            + basis2(c10[4 + k], bx[3], by[0])
            + basis2(c01[4 + k], bx[2], by[1])
            + basis2(c11[4 + k], bx[3], by[1])
            + basis2(c00[8 + k], bx[0], by[2])
            + basis2(c10[8 + k], bx[1], by[2])
            + basis2(c01[8 + k], bx[0], by[3])
            + basis2(c11[8 + k], bx[1], by[3]);
        i64 v = (per_y * static_cast<i64>(X[1]) + (kCoordQ / 2)) / kCoordQ;
        if (v < 0) v = 0;
        if (v > 2147483647LL) v = 2147483647LL;
        out[k] = static_cast<i32>(v);
    }
    normalize4_lut_q(out);
    *out_r = lut_q_to_u8(out[0]);
    *out_g = lut_q_to_u8(out[1]);
    *out_b = lut_q_to_u8(out[2]);
    *out_w = lut_q_to_u8(out[3]);
}

inline void setup_hermite16(const DiodeProfile& profile) {
    setup_state(g_hermite16, profile, 16);
}
inline void solve_hermite16(u8 r, u8 g, u8 b, u8* or_, u8* og, u8* ob, u8* ow) {
    solve_state(g_hermite16, r, g, b, or_, og, ob, ow);
}
inline void teardown_hermite16() {
    g_hermite16.lut.cells.reset();
    g_hermite16.lut.N = 0;
}

inline void setup_hermite32(const DiodeProfile& profile) {
    setup_state(g_hermite32, profile, 32);
}
inline void solve_hermite32(u8 r, u8 g, u8 b, u8* or_, u8* og, u8* ob, u8* ow) {
    solve_state(g_hermite32, r, g, b, or_, og, ob, ow);
}
inline void teardown_hermite32() {
    g_hermite32.lut.cells.reset();
    g_hermite32.lut.N = 0;
}

}  // namespace strategy_hermite_fixed_lookup

namespace strategy_hybrid_q16_refine {

static constexpr int kN = 8;

struct Router {
    i32 xy_min[2] = {0, 0};
    i32 xy_max[2] = {0, 0};
    u8 seed[kN * kN] = {};
};

static Router g_router;

inline i32 min4(i32 a, i32 b, i32 c, i32 d) {
    return fl::min(fl::min(a, b), fl::min(c, d));
}

inline i32 max4(i32 a, i32 b, i32 c, i32 d) {
    return fl::max(fl::max(a, b), fl::max(c, d));
}

inline int seed_for_xy(const i32 xy[2]) {
    using namespace strategy_fixed_analytical_q16;
    if (tri_contains(xy, g_state.xy_r, g_state.xy_g, g_state.xy_w)) return 0;
    if (tri_contains(xy, g_state.xy_r, g_state.xy_b, g_state.xy_w)) return 1;
    if (tri_contains(xy, g_state.xy_b, g_state.xy_g, g_state.xy_w)) return 2;
    return 3;
}

inline void setup(const DiodeProfile& profile) {
    using namespace strategy_fixed_analytical_q16;
    strategy_fixed_analytical_q16::setup(profile);

    const i32 pad = kQ / 64;
    g_router.xy_min[0] = min4(g_state.xy_r[0], g_state.xy_g[0],
                              g_state.xy_b[0], g_state.xy_w[0]) - pad;
    g_router.xy_min[1] = min4(g_state.xy_r[1], g_state.xy_g[1],
                              g_state.xy_b[1], g_state.xy_w[1]) - pad;
    g_router.xy_max[0] = max4(g_state.xy_r[0], g_state.xy_g[0],
                              g_state.xy_b[0], g_state.xy_w[0]) + pad;
    g_router.xy_max[1] = max4(g_state.xy_r[1], g_state.xy_g[1],
                              g_state.xy_b[1], g_state.xy_w[1]) + pad;

    for (int y = 0; y < kN; ++y) {
        for (int x = 0; x < kN; ++x) {
            const i32 xy[2] = {
                static_cast<i32>(
                    g_router.xy_min[0] +
                    ((static_cast<i64>(g_router.xy_max[0] - g_router.xy_min[0])
                      * (2 * x + 1)) / (2 * kN))),
                static_cast<i32>(
                    g_router.xy_min[1] +
                    ((static_cast<i64>(g_router.xy_max[1] - g_router.xy_min[1])
                      * (2 * y + 1)) / (2 * kN))),
            };
            g_router.seed[y * kN + x] = static_cast<u8>(seed_for_xy(xy));
        }
    }
}

inline int router_seed_for_X(const i32 X[3]) {
    using namespace strategy_fixed_analytical_q16;
    i32 xy[2];
    if (!xy_from_X(X, xy)) return 3;
    const i32 dx = g_router.xy_max[0] - g_router.xy_min[0];
    const i32 dy = g_router.xy_max[1] - g_router.xy_min[1];
    if (dx <= 0 || dy <= 0) return 3;
    int ix = static_cast<int>(
        (static_cast<i64>(xy[0] - g_router.xy_min[0]) * kN) / dx);
    int iy = static_cast<int>(
        (static_cast<i64>(xy[1] - g_router.xy_min[1]) * kN) / dy);
    if (ix < 0) ix = 0;
    if (iy < 0) iy = 0;
    if (ix >= kN) ix = kN - 1;
    if (iy >= kN) iy = kN - 1;
    return static_cast<int>(g_router.seed[iy * kN + ix]);
}

inline void solve(u8 r, u8 g, u8 b, u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    using namespace strategy_fixed_analytical_q16;
    const u8 value = fl::max(fl::max(r, g), b);
    if (value == 0) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }
    const int active_count = (r > 0 ? 1 : 0) + (g > 0 ? 1 : 0) + (b > 0 ? 1 : 0);
    if (is_native_input_gamut(*g_state.float_cache.profile) && active_count == 1) {
        *out_r = r;
        *out_g = g;
        *out_b = b;
        *out_w = 0;
        return;
    }

    i32 X[3];
    source_full_chroma_q(r, g, b, value, X);

    i32 endpoint[4];
    bool ok = false;
    if (is_native_input_gamut(*g_state.float_cache.profile) && active_count == 2) {
        int active[2];
        int n = 0;
        if (r > 0) active[n++] = 0;
        if (g > 0) active[n++] = 1;
        if (b > 0) active[n++] = 2;
        ok = solve_ls2_endpoint_q(X, active, endpoint);
    } else {
        ok = solve_interior_endpoint_seed_q(X, router_seed_for_X(X), endpoint);
    }

    if (!ok) {
        strategy_common::exact_fallback(r, g, b, out_r, out_g, out_b, out_w);
        return;
    }
    *out_r = q_to_scaled_u8(endpoint[0], value);
    *out_g = q_to_scaled_u8(endpoint[1], value);
    *out_b = q_to_scaled_u8(endpoint[2], value);
    *out_w = q_to_scaled_u8(endpoint[3], value);
}

inline Q16RangeStats audit_range(const DiodeProfile& profile, int cube_steps) {
    setup(profile);
    return strategy_fixed_analytical_q16::audit_current_state(
        cube_steps, true, router_seed_for_X);
}

inline void teardown() {}

}  // namespace strategy_hybrid_q16_refine

namespace strategy_rgbww_layered_reference {

inline void setup(const DiodeProfile&) {}

inline void solve5(u8 r, u8 g, u8 b,
                   u8* out_r, u8* out_g, u8* out_b,
                   u8* out_ww, u8* out_wc) {
    rgbww_layered_reference_u8(r, g, b, out_r, out_g, out_b, out_ww, out_wc);
}

inline void teardown() {}

}  // namespace strategy_rgbww_layered_reference

namespace strategy_rgbww_strict_5ch {

static constexpr int kMaxCombos = 9;

struct Combo {
    int idx[3] = {0, 0, 0};
    float inv[3][3] = {};
    bool ok = false;
};

struct State {
    colorimetric_detail::RgbcctProfile profile{};
    ProfileCache warm_cache;
    float P[5][3] = {};
    Combo combos[kMaxCombos];
};

static State g_state;

inline void zero5(float out[5]) {
    out[0] = out[1] = out[2] = out[3] = out[4] = 0.0f;
}

inline void normalize5(float out[5]) {
    float m = out[0];
    for (int i = 1; i < 5; ++i) {
        if (out[i] > m) m = out[i];
    }
    if (m > 1.0f) {
        const float inv = 1.0f / m;
        for (int i = 0; i < 5; ++i) {
            out[i] *= inv;
        }
    }
}

inline void source_full_chroma_xyz(u8 r, u8 g, u8 b, u8 value, float X[3]) {
    const float s[3] = {
        static_cast<float>(r) / static_cast<float>(value),
        static_cast<float>(g) / static_cast<float>(value),
        static_cast<float>(b) / static_cast<float>(value),
    };
    if (g_state.warm_cache.has_source_space) {
        matvec3(g_state.warm_cache.M_src, s, X);
        return;
    }
    for (int row = 0; row < 3; ++row) {
        X[row] = g_state.P[0][row] * s[0]
               + g_state.P[1][row] * s[1]
               + g_state.P[2][row] * s[2];
    }
}

inline void build_combo(Combo& combo, int a, int b, int c) {
    combo.idx[0] = a;
    combo.idx[1] = b;
    combo.idx[2] = c;
    const float M[3][3] = {
        {g_state.P[a][0], g_state.P[b][0], g_state.P[c][0]},
        {g_state.P[a][1], g_state.P[b][1], g_state.P[c][1]},
        {g_state.P[a][2], g_state.P[b][2], g_state.P[c][2]},
    };
    combo.ok = invert3x3(M, combo.inv);
}

inline bool solve_ls2_endpoint(const float X[3], const int active[2], float out[5]) {
    zero5(out);
    const float* A = g_state.P[active[0]];
    const float* B = g_state.P[active[1]];
    const float aa = A[0]*A[0] + A[1]*A[1] + A[2]*A[2];
    const float ab = A[0]*B[0] + A[1]*B[1] + A[2]*B[2];
    const float bb = B[0]*B[0] + B[1]*B[1] + B[2]*B[2];
    const float ax = A[0]*X[0] + A[1]*X[1] + A[2]*X[2];
    const float bx = B[0]*X[0] + B[1]*X[1] + B[2]*X[2];
    const float det = aa * bb - ab * ab;
    if (fl::fabs(det) < 1e-9f) return false;
    float t0 = ( ax * bb - bx * ab) / det;
    float t1 = (-ax * ab + bx * aa) / det;
    if (t0 < 0.0f && t1 >= 0.0f) {
        t0 = 0.0f;
        t1 = (bb > 1e-9f) ? fl::max(bx / bb, 0.0f) : 0.0f;
    } else if (t1 < 0.0f && t0 >= 0.0f) {
        t1 = 0.0f;
        t0 = (aa > 1e-9f) ? fl::max(ax / aa, 0.0f) : 0.0f;
    } else if (t0 < 0.0f && t1 < 0.0f) {
        t0 = t1 = 0.0f;
    }
    out[active[0]] = fl::max(t0, 0.0f);
    out[active[1]] = fl::max(t1, 0.0f);
    normalize5(out);
    return true;
}

inline int solve_endpoint_combo_index(const float X[3], float out[5]) {
    zero5(out);
    float best[5] = {0, 0, 0, 0, 0};
    float best_white = -1.0f;
    int best_combo = -1;
    bool found = false;
    for (int i = 0; i < kMaxCombos; ++i) {
        const Combo& combo = g_state.combos[i];
        if (!combo.ok) continue;
        float t[3];
        matvec3(combo.inv, X, t);
        constexpr float kEps = 1e-4f;
        if (t[0] < -kEps || t[1] < -kEps || t[2] < -kEps) continue;
        float candidate[5] = {0, 0, 0, 0, 0};
        candidate[combo.idx[0]] = fl::max(t[0], 0.0f);
        candidate[combo.idx[1]] = fl::max(t[1], 0.0f);
        candidate[combo.idx[2]] = fl::max(t[2], 0.0f);
        normalize5(candidate);
        const float white = candidate[3] + candidate[4];
        if (!found || white > best_white) {
            found = true;
            best_white = white;
            best_combo = i;
            for (int k = 0; k < 5; ++k) {
                best[k] = candidate[k];
            }
        }
    }
    if (!found) return -1;
    for (int k = 0; k < 5; ++k) {
        out[k] = best[k];
    }
    return best_combo;
}

inline bool solve_endpoint(const float X[3], float out[5]) {
    return solve_endpoint_combo_index(X, out) >= 0;
}

inline void setup(const DiodeProfile&) {
    g_state.profile = kRgbwwDefaultProfile;
    build_profile_cache(&g_state.profile.warm_path, 0, &g_state.warm_cache);

    const DiodeProfile& warm = g_state.profile.warm_path;
    const DiodeProfile& cool = g_state.profile.cool_path;
    xyY_to_XYZ(warm.xy_r[0], warm.xy_r[1], warm.lum_r, g_state.P[0]);
    xyY_to_XYZ(warm.xy_g[0], warm.xy_g[1], warm.lum_g, g_state.P[1]);
    xyY_to_XYZ(warm.xy_b[0], warm.xy_b[1], warm.lum_b, g_state.P[2]);
    xyY_to_XYZ(warm.xy_w[0], warm.xy_w[1], warm.lum_w, g_state.P[3]);
    xyY_to_XYZ(cool.xy_w[0], cool.xy_w[1], cool.lum_w, g_state.P[4]);

    build_combo(g_state.combos[0], 0, 1, 3);  // RG warm
    build_combo(g_state.combos[1], 0, 2, 3);  // RB warm
    build_combo(g_state.combos[2], 1, 2, 3);  // GB warm
    build_combo(g_state.combos[3], 0, 1, 4);  // RG cool
    build_combo(g_state.combos[4], 0, 2, 4);  // RB cool
    build_combo(g_state.combos[5], 1, 2, 4);  // GB cool
    build_combo(g_state.combos[6], 0, 3, 4);  // R warm/cool
    build_combo(g_state.combos[7], 1, 3, 4);  // G warm/cool
    build_combo(g_state.combos[8], 2, 3, 4);  // B warm/cool
}

inline void solve5(u8 r, u8 g, u8 b,
                   u8* out_r, u8* out_g, u8* out_b,
                   u8* out_ww, u8* out_wc) {
    const u8 value = fl::max(fl::max(r, g), b);
    if (value == 0) {
        *out_r = *out_g = *out_b = *out_ww = *out_wc = 0;
        return;
    }
    const int active_count = (r > 0 ? 1 : 0) + (g > 0 ? 1 : 0) + (b > 0 ? 1 : 0);
    if (is_native_input_gamut(g_state.profile.warm_path) && active_count == 1) {
        *out_r = r;
        *out_g = g;
        *out_b = b;
        *out_ww = *out_wc = 0;
        return;
    }

    float X[3];
    source_full_chroma_xyz(r, g, b, value, X);

    float endpoint[5];
    bool ok = false;
    if (is_native_input_gamut(g_state.profile.warm_path) && active_count == 2) {
        int active[2];
        int n = 0;
        if (r > 0) active[n++] = 0;
        if (g > 0) active[n++] = 1;
        if (b > 0) active[n++] = 2;
        ok = solve_ls2_endpoint(X, active, endpoint);
    } else {
        ok = solve_endpoint(X, endpoint);
    }
    if (!ok) {
        rgbww_layered_reference_u8(r, g, b, out_r, out_g, out_b, out_ww, out_wc);
        return;
    }

    const float scale = static_cast<float>(value) * (1.0f / 255.0f);
    *out_r = quantize_u8(endpoint[0] * scale);
    *out_g = quantize_u8(endpoint[1] * scale);
    *out_b = quantize_u8(endpoint[2] * scale);
    *out_ww = quantize_u8(endpoint[3] * scale);
    *out_wc = quantize_u8(endpoint[4] * scale);
}

inline void teardown() {}

}  // namespace strategy_rgbww_strict_5ch

namespace strategy_rgbww_strict_5ch_fixed_eval {

static constexpr int kMaxCombos = strategy_rgbww_strict_5ch::kMaxCombos;
static constexpr i32 kQ = strategy_fixed_analytical_q16::kQ;

struct Combo {
    u8 idx[3] = {0, 0, 0};
    i32 inv[3][3] = {};
    bool ok = false;
};

struct State {
    colorimetric_detail::RgbcctProfile profile{};
    bool has_source_space = false;
    i32 P[5][3] = {};
    i32 M_src[3][3] = {};
    Combo combos[kMaxCombos];
};

static State g_state;

inline i32 q(float v) {
    return strategy_common::quantize_q(v, kQ);
}

inline i64 mul_q(i64 a, i64 b) {
    return (a * b) / kQ;
}

inline i32 clamp_q(i64 v) {
    if (v < 0) return 0;
    if (v > kQ) return kQ;
    return static_cast<i32>(v);
}

inline i32 max0_q(i64 v) {
    if (v < 0) return 0;
    const i32 max_i32 = (fl::numeric_limits<i32>::max)();
    if (v > static_cast<i64>(max_i32)) return max_i32;
    return static_cast<i32>(v);
}

inline void matvec_q(const i32 M[3][3], const i32 in[3], i32 out[3]) {
    strategy_fixed_analytical_q16::matvec_q(M, in, out);
}

inline void zero5_q(i32 out[5]) {
    out[0] = out[1] = out[2] = out[3] = out[4] = 0;
}

inline void normalize5_q(i32 out[5]) {
    i32 m = out[0];
    for (int i = 1; i < 5; ++i) {
        if (out[i] > m) m = out[i];
    }
    if (m <= kQ) return;
    for (int i = 0; i < 5; ++i) {
        out[i] = static_cast<i32>((static_cast<i64>(out[i]) * kQ) / m);
    }
}

inline u8 q_to_scaled_u8(i32 qv, u8 value) {
    const i64 v = static_cast<i64>(qv) * static_cast<i64>(value);
    const i64 rounded = (v + (kQ / 2)) / kQ;
    if (rounded <= 0) return 0;
    if (rounded >= 255) return 255;
    return static_cast<u8>(rounded);
}

inline void source_full_chroma_q(u8 r, u8 g, u8 b, u8 value, i32 X[3]) {
    const i32 src[3] = {
        static_cast<i32>((static_cast<i64>(r) * kQ) / value),
        static_cast<i32>((static_cast<i64>(g) * kQ) / value),
        static_cast<i32>((static_cast<i64>(b) * kQ) / value),
    };
    if (g_state.has_source_space) {
        matvec_q(g_state.M_src, src, X);
        return;
    }
    for (int row = 0; row < 3; ++row) {
        const i64 acc =
              static_cast<i64>(g_state.P[0][row]) * src[0]
            + static_cast<i64>(g_state.P[1][row]) * src[1]
            + static_cast<i64>(g_state.P[2][row]) * src[2];
        X[row] = static_cast<i32>(acc / kQ);
    }
}

inline void build_combo(Combo& combo, const float P[5][3],
                        int a, int b, int c) {
    combo.idx[0] = static_cast<u8>(a);
    combo.idx[1] = static_cast<u8>(b);
    combo.idx[2] = static_cast<u8>(c);
    const float M[3][3] = {
        {P[a][0], P[b][0], P[c][0]},
        {P[a][1], P[b][1], P[c][1]},
        {P[a][2], P[b][2], P[c][2]},
    };
    float inv[3][3];
    combo.ok = invert3x3(M, inv);
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            combo.inv[row][col] = combo.ok ? q(inv[row][col]) : 0;
        }
    }
}

inline bool solve_ls2_endpoint_q(const i32 X[3], const int active[2], i32 out[5]) {
    zero5_q(out);
    const i32* A = g_state.P[active[0]];
    const i32* B = g_state.P[active[1]];
    i64 aa = 0, ab = 0, bb = 0, ax = 0, bx = 0;
    for (int i = 0; i < 3; ++i) {
        aa += mul_q(A[i], A[i]);
        ab += mul_q(A[i], B[i]);
        bb += mul_q(B[i], B[i]);
        ax += mul_q(A[i], X[i]);
        bx += mul_q(B[i], X[i]);
    }
    const i64 det = aa * bb - ab * ab;
    if (det == 0) return false;

    i64 t0 = ((ax * bb - bx * ab) * kQ) / det;
    i64 t1 = ((bx * aa - ax * ab) * kQ) / det;
    if (t0 < 0 && t1 >= 0) {
        t0 = 0;
        t1 = bb > 0 ? (bx * kQ) / bb : 0;
    } else if (t1 < 0 && t0 >= 0) {
        t1 = 0;
        t0 = aa > 0 ? (ax * kQ) / aa : 0;
    } else if (t0 < 0 && t1 < 0) {
        t0 = 0;
        t1 = 0;
    }
    out[active[0]] = max0_q(t0);
    out[active[1]] = max0_q(t1);
    normalize5_q(out);
    return true;
}

inline int solve_endpoint_q_combo_index(const i32 X[3], i32 out[5]) {
    zero5_q(out);
    i32 best[5] = {0, 0, 0, 0, 0};
    i64 best_white = -1;
    int best_combo = -1;
    bool found = false;
    for (int i = 0; i < kMaxCombos; ++i) {
        const Combo& combo = g_state.combos[i];
        if (!combo.ok) continue;
        i32 t[3];
        matvec_q(combo.inv, X, t);
        const i32 eps = -(kQ / 10000);
        if (t[0] < eps || t[1] < eps || t[2] < eps) continue;

        i32 candidate[5] = {0, 0, 0, 0, 0};
        candidate[combo.idx[0]] = max0_q(t[0]);
        candidate[combo.idx[1]] = max0_q(t[1]);
        candidate[combo.idx[2]] = max0_q(t[2]);
        normalize5_q(candidate);
        const i64 white = static_cast<i64>(candidate[3]) + candidate[4];
        if (!found || white > best_white) {
            found = true;
            best_white = white;
            best_combo = i;
            for (int k = 0; k < 5; ++k) {
                best[k] = candidate[k];
            }
        }
    }
    if (!found) return -1;
    for (int k = 0; k < 5; ++k) {
        out[k] = best[k];
    }
    return best_combo;
}

inline bool solve_endpoint_q(const i32 X[3], i32 out[5]) {
    return solve_endpoint_q_combo_index(X, out) >= 0;
}

inline void setup(const DiodeProfile&) {
    g_state.profile = kRgbwwDefaultProfile;
    ProfileCache warm_cache;
    build_profile_cache(&g_state.profile.warm_path, 0, &warm_cache);
    g_state.has_source_space = warm_cache.has_source_space;

    float P[5][3] = {};
    const DiodeProfile& warm = g_state.profile.warm_path;
    const DiodeProfile& cool = g_state.profile.cool_path;
    xyY_to_XYZ(warm.xy_r[0], warm.xy_r[1], warm.lum_r, P[0]);
    xyY_to_XYZ(warm.xy_g[0], warm.xy_g[1], warm.lum_g, P[1]);
    xyY_to_XYZ(warm.xy_b[0], warm.xy_b[1], warm.lum_b, P[2]);
    xyY_to_XYZ(warm.xy_w[0], warm.xy_w[1], warm.lum_w, P[3]);
    xyY_to_XYZ(cool.xy_w[0], cool.xy_w[1], cool.lum_w, P[4]);
    for (int ch = 0; ch < 5; ++ch) {
        for (int row = 0; row < 3; ++row) {
            g_state.P[ch][row] = q(P[ch][row]);
        }
    }
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            g_state.M_src[row][col] = q(warm_cache.M_src[row][col]);
        }
    }

    build_combo(g_state.combos[0], P, 0, 1, 3);  // RG warm
    build_combo(g_state.combos[1], P, 0, 2, 3);  // RB warm
    build_combo(g_state.combos[2], P, 1, 2, 3);  // GB warm
    build_combo(g_state.combos[3], P, 0, 1, 4);  // RG cool
    build_combo(g_state.combos[4], P, 0, 2, 4);  // RB cool
    build_combo(g_state.combos[5], P, 1, 2, 4);  // GB cool
    build_combo(g_state.combos[6], P, 0, 3, 4);  // R warm/cool
    build_combo(g_state.combos[7], P, 1, 3, 4);  // G warm/cool
    build_combo(g_state.combos[8], P, 2, 3, 4);  // B warm/cool
}

inline void solve5(u8 r, u8 g, u8 b,
                   u8* out_r, u8* out_g, u8* out_b,
                   u8* out_ww, u8* out_wc) {
    const u8 value = fl::max(fl::max(r, g), b);
    if (value == 0) {
        *out_r = *out_g = *out_b = *out_ww = *out_wc = 0;
        return;
    }
    const int active_count = (r > 0 ? 1 : 0) + (g > 0 ? 1 : 0) + (b > 0 ? 1 : 0);
    if (is_native_input_gamut(g_state.profile.warm_path) && active_count == 1) {
        *out_r = r;
        *out_g = g;
        *out_b = b;
        *out_ww = *out_wc = 0;
        return;
    }

    i32 X[3];
    source_full_chroma_q(r, g, b, value, X);

    i32 endpoint[5];
    bool ok = false;
    if (is_native_input_gamut(g_state.profile.warm_path) && active_count == 2) {
        int active[2];
        int n = 0;
        if (r > 0) active[n++] = 0;
        if (g > 0) active[n++] = 1;
        if (b > 0) active[n++] = 2;
        ok = solve_ls2_endpoint_q(X, active, endpoint);
    } else {
        ok = solve_endpoint_q(X, endpoint);
    }
    if (!ok) {
        rgbww_layered_reference_u8(r, g, b, out_r, out_g, out_b, out_ww, out_wc);
        return;
    }

    *out_r = q_to_scaled_u8(endpoint[0], value);
    *out_g = q_to_scaled_u8(endpoint[1], value);
    *out_b = q_to_scaled_u8(endpoint[2], value);
    *out_ww = q_to_scaled_u8(endpoint[3], value);
    *out_wc = q_to_scaled_u8(endpoint[4], value);
}

inline void track_state_coefficients(Q16RangeStats& stats) {
    for (int ch = 0; ch < 5; ++ch) {
        for (int row = 0; row < 3; ++row) {
            strategy_common::track_abs(g_state.P[ch][row],
                                       &stats.max_abs_coeff_q);
        }
    }
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            strategy_common::track_abs(g_state.M_src[row][col],
                                       &stats.max_abs_coeff_q);
            for (int combo_idx = 0; combo_idx < kMaxCombos; ++combo_idx) {
                strategy_common::track_abs(
                    g_state.combos[combo_idx].inv[row][col],
                    &stats.max_abs_coeff_q);
            }
        }
    }
}

inline void source_full_chroma_q_audit(u8 r,
                                       u8 g,
                                       u8 b,
                                       u8 value,
                                       i32 X[3],
                                       Q16RangeStats& stats) {
    strategy_common::track_divisor(value, stats);
    const i32 src[3] = {
        static_cast<i32>((static_cast<i64>(r) * kQ) / value),
        static_cast<i32>((static_cast<i64>(g) * kQ) / value),
        static_cast<i32>((static_cast<i64>(b) * kQ) / value),
    };
    for (int i = 0; i < 3; ++i) {
        strategy_common::track_abs(src[i], &stats.max_abs_src_q);
    }
    if (g_state.has_source_space) {
        strategy_fixed_analytical_q16::matvec_q_audit(
            g_state.M_src, src, X, stats);
        return;
    }
    for (int row = 0; row < 3; ++row) {
        i64 acc = 0;
        for (int ch = 0; ch < 3; ++ch) {
            const i64 product =
                static_cast<i64>(g_state.P[ch][row]) * src[ch];
            strategy_common::track_abs(product, &stats.max_abs_product);
            acc += product;
            strategy_common::track_abs(acc, &stats.max_abs_accumulator);
        }
        X[row] = static_cast<i32>(acc / kQ);
        strategy_common::track_abs(X[row], &stats.max_abs_output_q);
    }
}

inline void track_endpoint5(const i32 out[5], Q16RangeStats& stats) {
    for (int i = 0; i < 5; ++i) {
        strategy_common::track_abs(out[i], &stats.max_abs_endpoint_q);
        strategy_common::track_abs(out[i], &stats.max_abs_output_q);
    }
}

inline bool solve_ls2_endpoint_q_audit(const i32 X[3],
                                       const int active[2],
                                       i32 out[5],
                                       Q16RangeStats& stats) {
    zero5_q(out);
    const i32* A = g_state.P[active[0]];
    const i32* B = g_state.P[active[1]];
    i64 aa = 0, ab = 0, bb = 0, ax = 0, bx = 0;
    for (int i = 0; i < 3; ++i) {
        aa += strategy_fixed_analytical_q16::mul_q_audit(A[i], A[i], stats);
        ab += strategy_fixed_analytical_q16::mul_q_audit(A[i], B[i], stats);
        bb += strategy_fixed_analytical_q16::mul_q_audit(B[i], B[i], stats);
        ax += strategy_fixed_analytical_q16::mul_q_audit(A[i], X[i], stats);
        bx += strategy_fixed_analytical_q16::mul_q_audit(B[i], X[i], stats);
        strategy_common::track_abs(aa, &stats.max_abs_accumulator);
        strategy_common::track_abs(ab, &stats.max_abs_accumulator);
        strategy_common::track_abs(bb, &stats.max_abs_accumulator);
        strategy_common::track_abs(ax, &stats.max_abs_accumulator);
        strategy_common::track_abs(bx, &stats.max_abs_accumulator);
    }
    const i64 det = aa * bb - ab * ab;
    strategy_common::track_det(det, stats);
    if (det == 0) return false;
    strategy_common::track_divisor(det, stats);

    const i64 num0 = ax * bb - bx * ab;
    const i64 num1 = bx * aa - ax * ab;
    const i64 scaled0 = num0 * kQ;
    const i64 scaled1 = num1 * kQ;
    strategy_common::track_abs(scaled0, &stats.max_abs_numerator);
    strategy_common::track_abs(scaled1, &stats.max_abs_numerator);
    i64 t0 = scaled0 / det;
    i64 t1 = scaled1 / det;
    strategy_common::track_abs(t0, &stats.max_abs_output_q);
    strategy_common::track_abs(t1, &stats.max_abs_output_q);
    if (t0 < 0 && t1 >= 0) {
        t0 = 0;
        if (bb > 0) {
            strategy_common::track_divisor(bb, stats);
            t1 = (bx * kQ) / bb;
        } else {
            t1 = 0;
        }
    } else if (t1 < 0 && t0 >= 0) {
        t1 = 0;
        if (aa > 0) {
            strategy_common::track_divisor(aa, stats);
            t0 = (ax * kQ) / aa;
        } else {
            t0 = 0;
        }
    } else if (t0 < 0 && t1 < 0) {
        t0 = 0;
        t1 = 0;
    }
    strategy_common::track_abs(t0, &stats.max_abs_output_q);
    strategy_common::track_abs(t1, &stats.max_abs_output_q);
    out[active[0]] = max0_q(t0);
    out[active[1]] = max0_q(t1);
    track_endpoint5(out, stats);
    strategy_common::track_normalize5_divisor(out, stats);
    normalize5_q(out);
    track_endpoint5(out, stats);
    return true;
}

inline bool solve_endpoint_q_audit(const i32 X[3],
                                   i32 out[5],
                                   Q16RangeStats& stats) {
    zero5_q(out);
    i32 best[5] = {0, 0, 0, 0, 0};
    i64 best_white = -1;
    bool found = false;
    for (int i = 0; i < kMaxCombos; ++i) {
        const Combo& combo = g_state.combos[i];
        if (!combo.ok) continue;
        i32 t[3];
        strategy_fixed_analytical_q16::matvec_q_audit(
            combo.inv, X, t, stats);
        const i32 eps = -(kQ / 10000);
        if (t[0] < eps || t[1] < eps || t[2] < eps) continue;

        i32 candidate[5] = {0, 0, 0, 0, 0};
        candidate[combo.idx[0]] = max0_q(t[0]);
        candidate[combo.idx[1]] = max0_q(t[1]);
        candidate[combo.idx[2]] = max0_q(t[2]);
        track_endpoint5(candidate, stats);
        strategy_common::track_normalize5_divisor(candidate, stats);
        normalize5_q(candidate);
        track_endpoint5(candidate, stats);
        const i64 white = static_cast<i64>(candidate[3]) + candidate[4];
        if (!found || white > best_white) {
            found = true;
            best_white = white;
            for (int k = 0; k < 5; ++k) {
                best[k] = candidate[k];
            }
        }
    }
    if (!found) return false;
    for (int k = 0; k < 5; ++k) {
        out[k] = best[k];
    }
    return true;
}

inline Q16RangeStats audit_range(const DiodeProfile& profile, int cube_steps) {
    setup(profile);
    Q16RangeStats stats{};
    if (cube_steps <= 1) return stats;
    stats.measured = true;
    track_state_coefficients(stats);

    for (int ri = 0; ri < cube_steps; ++ri) {
        const u8 r = cube_value(ri, cube_steps);
        for (int gi = 0; gi < cube_steps; ++gi) {
            const u8 g = cube_value(gi, cube_steps);
            for (int bi = 0; bi < cube_steps; ++bi) {
                const u8 b = cube_value(bi, cube_steps);
                ++stats.sample_count;
                const u8 value = fl::max(fl::max(r, g), b);
                if (value == 0) continue;
                const int active_count =
                    (r > 0 ? 1 : 0) + (g > 0 ? 1 : 0) + (b > 0 ? 1 : 0);
                if (is_native_input_gamut(g_state.profile.warm_path)
                    && active_count == 1) {
                    continue;
                }

                i32 X[3];
                source_full_chroma_q_audit(r, g, b, value, X, stats);

                i32 endpoint[5];
                bool ok = false;
                if (is_native_input_gamut(g_state.profile.warm_path)
                    && active_count == 2) {
                    int active[2];
                    int n = 0;
                    if (r > 0) active[n++] = 0;
                    if (g > 0) active[n++] = 1;
                    if (b > 0) active[n++] = 2;
                    ok = solve_ls2_endpoint_q_audit(X, active, endpoint, stats);
                } else {
                    ok = solve_endpoint_q_audit(X, endpoint, stats);
                }
                if (!ok) {
                    ++stats.fallback_count;
                }
            }
        }
    }
    return stats;
}

inline void teardown() {}

}  // namespace strategy_rgbww_strict_5ch_fixed_eval

inline u32 lut_bytes(int n, RgbwLutInterp interp) {
    return static_cast<u32>(rgbw_colorimetric_lut_memory_bytes(n, interp));
}

inline u32 experimental_hermite_cells_bytes(int n) {
    return static_cast<u32>(n * n * strategy_experimental_hermite::kStride
                            * static_cast<int>(sizeof(i32)));
}

inline u32 packed_hermite_cell_bytes(int n) {
    return static_cast<u32>(n * n * strategy_experimental_hermite::kStride
                            * static_cast<int>(sizeof(i16)));
}

inline u32 packed_hull_axis_bytes(int n) {
    // Axis endpoints come from the profile; one u8 warp code per axis sample
    // is enough to reproduce the current 16-point non-uniform grid.
    return static_cast<u32>(n * 2 * sizeof(u8));
}

inline u32 packed_tetrahedral_i16_bytes(int n) {
    return static_cast<u32>(n * n * n * 4 * sizeof(i16));
}

inline u32 packed_tetrahedral_u8_bytes(int n) {
    return static_cast<u32>(n * n * n * 4 * sizeof(u8));
}

inline u32 packed_poly_coeff_bytes() {
    return static_cast<u32>(strategy_poly_piecewise_d3::kPieces
        * 4 * strategy_poly_piecewise_d3::kTerms * sizeof(i16));
}

inline u32 rgbww_strict_xyz_i16_bytes() {
    return static_cast<u32>(5 * 3 * sizeof(i16));
}

inline u32 rgbww_strict_source_matrix_i16_bytes() {
    return static_cast<u32>(3 * 3 * sizeof(i16));
}

inline u32 rgbww_strict_combo_index_bytes() {
    return static_cast<u32>(
        strategy_rgbww_strict_5ch::kMaxCombos * 3 * sizeof(u8));
}

inline u32 rgbww_strict_combo_inverse_i16_bytes() {
    return static_cast<u32>(
        strategy_rgbww_strict_5ch::kMaxCombos * 3 * 3 * sizeof(i16));
}

inline u32 rgbww_strict_combo_flag_bytes() {
    return static_cast<u32>(
        strategy_rgbww_strict_5ch::kMaxCombos * sizeof(u8));
}

inline u32 packed_rgbww_strict_bytes() {
    return rgbww_strict_xyz_i16_bytes()
        + rgbww_strict_combo_index_bytes()
        + rgbww_strict_combo_inverse_i16_bytes()
        + rgbww_strict_combo_flag_bytes();
}

inline u32 packed_rgbww_strict_q16_bytes() {
    const u32 columns = static_cast<u32>(5 * 3 * sizeof(i32));
    const u32 source_matrix = static_cast<u32>(3 * 3 * sizeof(i32));
    const u32 combo_indices = static_cast<u32>(
        strategy_rgbww_strict_5ch_fixed_eval::kMaxCombos * 3 * sizeof(u8));
    const u32 combo_inverses = static_cast<u32>(
        strategy_rgbww_strict_5ch_fixed_eval::kMaxCombos * 3 * 3 * sizeof(i32));
    const u32 combo_flags = static_cast<u32>(
        strategy_rgbww_strict_5ch_fixed_eval::kMaxCombos * sizeof(u8));
    return columns + source_matrix + combo_indices + combo_inverses + combo_flags;
}

inline MemoryInfo memory_closed_form() {
    return MemoryInfo{
        0,
        0,
        static_cast<u32>(sizeof(ProfileCache)),
        0,
        0,
        "no LUT or coefficient table; runtime keeps one ProfileCache",
        "per active profile/CCT cache",
        "0 packed bytes; compile-time colorimetric gate controls code inclusion",
    };
}

inline MemoryInfo memory_wx_overdrive_q16() {
    const u32 packed = static_cast<u32>(
        (12 + 9 + 9 + 3) * sizeof(i32));
    return MemoryInfo{
        packed,
        packed,
        static_cast<u32>(sizeof(strategy_wx_overdrive_q16::State)),
        packed,
        0,
        "Q16 P columns, source matrix, P_RGB_inv, and d_W constants",
        "constant per profile",
        "132B packed constants; fixed hull projection not implemented",
    };
}

inline MemoryInfo memory_wx_lp_q16_residual() {
    const u32 packed = static_cast<u32>(
        (12 + 9 + 9 + 3) * sizeof(i32));
    return MemoryInfo{
        packed,
        packed,
        static_cast<u32>(sizeof(strategy_wx_overdrive_q16::State)),
        packed,
        0,
        "Q16 P columns, source matrix, P_RGB_inv, and d_W constants",
        "constant per profile",
        "132B packed constants; rejected residual path is not balanced fixed LP",
    };
}

inline MemoryInfo memory_wx_lp_q16_balanced() {
    const u32 packed = static_cast<u32>((12 + 9) * sizeof(i32));
    return MemoryInfo{
        packed,
        static_cast<u32>(sizeof(strategy_wx_overdrive_q16::State)),
        static_cast<u32>(sizeof(strategy_wx_overdrive_q16::State)),
        packed,
        0,
        "Q16 P columns and source matrix; prototype state also carries unused residual constants",
        "constant per profile",
        "84B packed constants; hull projection and overflow proof pending",
    };
}

inline MemoryInfo memory_bilinear_n16() {
    const u32 cells = lut_bytes(16, RgbwLutInterp::Bilinear);
    return MemoryInfo{
        cells,
        cells,
        static_cast<u32>(sizeof(strategy_uniform_lut::State)) + cells,
        cells,
        0,
        "rgbw_colorimetric_lut_memory_bytes(16, Bilinear)",
        "N*N*4*i16 cells per profile",
        "2048B table; fits small tier when enabled explicitly",
    };
}

inline MemoryInfo memory_hermite_n16() {
    const u32 cells = lut_bytes(16, RgbwLutInterp::Hermite);
    return MemoryInfo{
        cells,
        cells,
        static_cast<u32>(sizeof(strategy_uniform_lut::State)) + cells,
        cells,
        0,
        "rgbw_colorimetric_lut_memory_bytes(16, Hermite)",
        "N*N*(value+dX+dY)*4*i16 cells per profile",
        "6144B table; fits 8KB small tier before cache overhead",
    };
}

inline MemoryInfo memory_hermite_n32() {
    const u32 cells = lut_bytes(32, RgbwLutInterp::Hermite);
    return MemoryInfo{
        cells,
        cells,
        static_cast<u32>(sizeof(strategy_uniform_lut::State)) + cells,
        cells,
        0,
        "rgbw_colorimetric_lut_memory_bytes(32, Hermite)",
        "N*N*(value+dX+dY)*4*i16 cells per profile",
        "24576B table; fails small-memory tier",
    };
}

inline MemoryInfo memory_cell_encoding_q16_n16() {
    const u32 cells = experimental_hermite_cells_bytes(16);
    const u32 packed = packed_hermite_cell_bytes(16);
    return MemoryInfo{
        packed,
        cells,
        static_cast<u32>(sizeof(strategy_experimental_hermite::State)) + cells,
        packed,
        0,
        "packed target = N*N*12*i16; prototype allocates N*N*12*i32",
        "cells scale as N*N; channels store value+dX+dY coefficients",
        "6144B packed cells; memory scaling sweep emitted; compression format pending",
    };
}

inline MemoryInfo memory_hull_adaptive_n16() {
    const u32 cells = experimental_hermite_cells_bytes(16);
    const u32 active_axes = static_cast<u32>(16 * 2 * sizeof(float));
    const u32 packed = packed_hermite_cell_bytes(16) + packed_hull_axis_bytes(16);
    return MemoryInfo{
        packed,
        cells + active_axes,
        static_cast<u32>(sizeof(strategy_experimental_hermite::State)) + cells,
        packed,
        0,
        "packed target = N*N*12*i16 cells plus two u8 warp axes",
        "cells scale as N*N; axes scale as 2*N",
        "6176B packed table; memory scaling sweep emitted; packed-axis decoder and MCU validation pending",
    };
}

inline MemoryInfo memory_tetrahedral_rgb8() {
    const u32 cells = packed_tetrahedral_i16_bytes(strategy_tetrahedral_rgb8::kN);
    return MemoryInfo{
        cells,
        cells,
        static_cast<u32>(sizeof(strategy_tetrahedral_rgb8::State)) + cells,
        cells,
        0,
        "8*8*8 RGB lattice, four i16 channels per node",
        "N^3*4*i16 per profile",
        "4096B table; fits small tier before cache overhead",
    };
}

inline MemoryInfo memory_tetrahedral_rgb8_u8() {
    const u32 cells = packed_tetrahedral_u8_bytes(strategy_tetrahedral_rgb8::kN);
    return MemoryInfo{
        cells,
        cells,
        static_cast<u32>(sizeof(strategy_tetrahedral_rgb8_u8::State)) + cells,
        cells,
        0,
        "8*8*8 RGB lattice, four u8 channels per node",
        "N^3*4*u8 per profile",
        "2048B u8 table; halves Q12 tetrahedral storage",
    };
}

inline MemoryInfo memory_poly_piecewise_d3() {
    const u32 coeff = static_cast<u32>(strategy_poly_piecewise_d3::kPieces
        * 4 * strategy_poly_piecewise_d3::kTerms * sizeof(double));
    const u32 packed = packed_poly_coeff_bytes();
    return MemoryInfo{
        packed,
        coeff,
        static_cast<u32>(sizeof(strategy_poly_piecewise_d3::State)),
        packed,
        0,
        "packed target = piece*channel*term i16 coefficients; prototype uses double",
        "coefficients scale as pieces*channels*terms",
        "480B packed coefficients; scale-ray drops measured; constrained fit pending",
    };
}

inline u32 fixed_q16_packed_bytes() {
    return static_cast<u32>(
        (8 + 12 + 9 + 27) * sizeof(i32));
}

inline MemoryInfo memory_fixed_analytical_q16() {
    const u32 packed = fixed_q16_packed_bytes();
    return MemoryInfo{
        packed,
        packed,
        static_cast<u32>(sizeof(strategy_fixed_analytical_q16::State)),
        packed,
        0,
        "xy endpoints, P columns, M_src, and three 3x3 inverses as i32 Q16",
        "constant per profile",
        "224B packed constants; best current tiny-memory footprint",
    };
}

inline MemoryInfo memory_hybrid_q16_refine_n8() {
    const u32 packed = fixed_q16_packed_bytes()
        + static_cast<u32>(sizeof(strategy_hybrid_q16_refine::Router));
    return MemoryInfo{
        packed,
        packed,
        static_cast<u32>(sizeof(strategy_fixed_analytical_q16::State))
            + static_cast<u32>(sizeof(strategy_hybrid_q16_refine::Router)),
        packed,
        0,
        "fixed Q16 constants plus 8*8 sub-gamut seed router",
        "constant Q16 profile block plus N*N u8 router seeds",
        "304B packed constants/router; +80B over fixed_analytical_q16",
    };
}

inline MemoryInfo memory_rgbww_strict_5ch() {
    const u32 proto = static_cast<u32>(
        sizeof(float) * 5 * 3
        + sizeof(strategy_rgbww_strict_5ch::Combo)
          * strategy_rgbww_strict_5ch::kMaxCombos);
    const u32 packed = packed_rgbww_strict_bytes();
    return MemoryInfo{
        packed,
        proto,
        static_cast<u32>(sizeof(strategy_rgbww_strict_5ch::State)),
        packed,
        0,
        "native-source packed target = 5*3*i16 XYZ, 9*3 u8 indices, 9*3*3 i16 inverses, 9 u8 flags",
        "combo count scales with channel topology policy; non-native source matrix adds 18B i16",
        "228B formula-derived i16 combo block; host policy invariant audit exists",
    };
}

inline MemoryInfo memory_rgbww_strict_5ch_fixed() {
    const u32 packed = packed_rgbww_strict_q16_bytes();
    return MemoryInfo{
        packed,
        static_cast<u32>(sizeof(strategy_rgbww_strict_5ch_fixed_eval::State)),
        static_cast<u32>(sizeof(strategy_rgbww_strict_5ch_fixed_eval::State)),
        packed,
        0,
        "Q16 packed target = i32 XYZ columns, i32 source matrix, u8 combo indices/flags, i32 inverses",
        "combo count scales with channel topology policy",
        "456B Q16 fixed combo block; host policy audit exists, MCU validation pending",
    };
}

inline MemoryInfo memory_rgbww_layered_reference() {
    return MemoryInfo{
        0,
        0,
        0,
        0,
        0,
        "stateless wrapper over the current layered RGBWW reference",
        "uses the compiled-in default RGBWW profile",
        "0 row-owned bytes; production profile constants live in the library",
    };
}

static const Strategy kStrategies[] = {
    {"float_closed_form", "reference", "REF", "RGBW", "reference row",
     memory_closed_form, "float closed form",
     "current implementation", "soft-float target not benchmarked",
     "host benchmark only; MCU cycles missing", true,
     strategy_closed_form::setup, strategy_closed_form::solve,
     strategy_closed_form::teardown, nullptr},
    {"wx_lp_legacy_float", "policy_lp", "PROD", "RGBW",
     "production max-white chromaticity-preserving policy row",
     memory_closed_form, "float max-white LP",
     "current implementation", "paired row is wx_lp_q16_balanced_proto",
     "host benchmark only; MCU cycles missing", false,
     strategy_wx_lp_legacy::setup, strategy_wx_lp_legacy::solve,
     strategy_wx_lp_legacy::teardown, nullptr},
    {"wx_overdrive_float", "boosted_policy", "PROD", "RGBW",
     "production boosted policy; intentionally trades chroma for luminance",
     memory_closed_form, "float boosted overdrive",
     "current implementation", "paired row is wx_overdrive_q16",
     "host benchmark only; MCU cycles missing", false,
     strategy_wx_overdrive::setup, strategy_wx_overdrive::solve,
     strategy_wx_overdrive::teardown, nullptr},
    {"wx_overdrive_q16", "boosted_policy", "PROTO", "RGBW",
     "fixed Q16 boosted policy prototype; observed range audit exists; fixed hull projection not implemented",
     memory_wx_overdrive_q16, "Q16 boosted overdrive",
     "paired row is wx_overdrive_float", "current implementation",
     "host benchmark and observed Q16 range audit only; hull projection and MCU cycles missing", false,
     strategy_wx_overdrive_q16::setup, strategy_wx_overdrive_q16::solve,
     strategy_wx_overdrive_q16::teardown, nullptr},
    {"bilinear_n16", "baseline_lut", "PROD", "RGBW", "legacy LUT",
     memory_bilinear_n16, "float LUT lookup",
     "current implementation", "paired row is bilinear_n16_fixed_interp",
     "host benchmark only; MCU cycles missing", true,
     strategy_uniform_lut::setup_bilinear16, strategy_uniform_lut::solve_bilinear16,
     strategy_uniform_lut::teardown_bilinear16, nullptr},
    {"hermite_n16", "baseline_lut", "PROD", "RGBW", "legacy LUT",
     memory_hermite_n16, "float Hermite LUT lookup",
     "current implementation", "paired row is hermite_n16_fixed_lookup",
     "host benchmark only; MCU cycles missing", true,
     strategy_uniform_lut::setup_hermite16, strategy_uniform_lut::solve_hermite16,
     strategy_uniform_lut::teardown_hermite16, nullptr},
    {"hermite_n16_fixed_lookup", "baseline_lut", "PROTO", "RGBW",
     "fixed runtime lookup for production Hermite N=16 LUT; needs MCU validation",
     memory_hermite_n16, "Q12 fixed Hermite LUT lookup",
     "paired row is hermite_n16", "current implementation",
     "host fixed-path benchmark only; MCU cycles missing", true,
     strategy_hermite_fixed_lookup::setup_hermite16,
     strategy_hermite_fixed_lookup::solve_hermite16,
     strategy_hermite_fixed_lookup::teardown_hermite16, nullptr},
    {"hermite_n32", "baseline_lut", "PROD", "RGBW", "legacy LUT",
     memory_hermite_n32, "float Hermite LUT lookup",
     "current implementation", "paired row is hermite_n32_fixed_lookup",
     "host benchmark only; MCU cycles missing", true,
     strategy_uniform_lut::setup_hermite32, strategy_uniform_lut::solve_hermite32,
     strategy_uniform_lut::teardown_hermite32, nullptr},
    {"hermite_n32_fixed_lookup", "baseline_lut", "PROTO", "RGBW",
     "fixed runtime lookup for production Hermite N=32 LUT; needs MCU validation",
     memory_hermite_n32, "Q12 fixed Hermite LUT lookup",
     "paired row is hermite_n32", "current implementation",
     "host fixed-path benchmark only; MCU cycles missing", true,
     strategy_hermite_fixed_lookup::setup_hermite32,
     strategy_hermite_fixed_lookup::solve_hermite32,
     strategy_hermite_fixed_lookup::teardown_hermite32, nullptr},
    {"cell_encoding_q16_n16", "hypothesis_5", "PROTO", "RGBW",
     "needs packed cell format and compression sweep",
     memory_cell_encoding_q16_n16, "float eval of Q16 cells",
     "current prototype", "paired row is cell_encoding_q16_n16_fixed_eval",
     "host benchmark only; compression sweep and MCU validation missing", true,
     strategy_experimental_hermite::setup_cell_q16,
     strategy_experimental_hermite::solve_cell_q16,
     strategy_experimental_hermite::teardown_cell_q16, nullptr},
    {"cell_encoding_q16_n16_fixed_eval", "hypothesis_5", "PROTO", "RGBW",
     "fixed evaluator for Q16 cell row; needs compression sweep and MCU validation",
     memory_cell_encoding_q16_n16, "Q16 fixed Hermite cell evaluator",
     "paired row is cell_encoding_q16_n16", "current implementation",
     "host fixed-path benchmark only; compression sweep and MCU cycles missing", true,
     strategy_experimental_hermite_fixed_eval::setup_cell_q16,
     strategy_experimental_hermite_fixed_eval::solve_cell_q16,
     strategy_experimental_hermite_fixed_eval::teardown_cell_q16, nullptr},
    {"hull_adaptive_n16", "hypothesis_1", "PROTO", "RGBW",
     "needs stable encoded axes and MCU validation",
     memory_hull_adaptive_n16, "float eval of warped Q12 cells",
     "current prototype", "paired row is hull_adaptive_n16_fixed_eval",
     "host benchmark only; packed-axis decoder and MCU validation missing", true,
     strategy_experimental_hermite::setup_hull_adaptive,
     strategy_experimental_hermite::solve_hull_adaptive,
     strategy_experimental_hermite::teardown_hull_adaptive, nullptr},
    {"hull_adaptive_n16_fixed_eval", "hypothesis_1", "PROTO", "RGBW",
     "fixed evaluator for warped Q12 cells; packed-axis decoder still pending",
     memory_hull_adaptive_n16, "Q12 fixed Hermite cell evaluator",
     "paired row is hull_adaptive_n16", "current implementation",
     "host fixed-path benchmark only; packed-axis decoder and MCU cycles missing", true,
     strategy_experimental_hermite_fixed_eval::setup_hull_adaptive,
     strategy_experimental_hermite_fixed_eval::solve_hull_adaptive,
     strategy_experimental_hermite_fixed_eval::teardown_hull_adaptive, nullptr},
    {"tetrahedral_rgb8", "hypothesis_2", "PROTO", "RGBW",
     "needs sparse storage and boosted-mode proof",
     memory_tetrahedral_rgb8, "float tetrahedral RGB LUT",
     "current prototype", "paired row is tetrahedral_rgb8_fixed",
     "host benchmark only; MCU cycles missing", true,
     strategy_tetrahedral_rgb8::setup, strategy_tetrahedral_rgb8::solve,
     strategy_tetrahedral_rgb8::teardown, nullptr},
    {"tetrahedral_rgb8_fixed", "hypothesis_2", "PROTO", "RGBW",
     "fixed Q12 evaluator for tetrahedral_rgb8; needs MCU validation",
     memory_tetrahedral_rgb8, "Q12 tetrahedral RGB LUT",
     "paired row is tetrahedral_rgb8", "current implementation",
     "host fixed-path benchmark only; MCU cycles missing", true,
     strategy_tetrahedral_rgb8_fixed::setup, strategy_tetrahedral_rgb8_fixed::solve,
     strategy_tetrahedral_rgb8_fixed::teardown, nullptr},
    {"tetrahedral_rgb8_u8", "hypothesis_2", "PROTO", "RGBW",
     "u8 packed tetrahedral LUT prototype; compression accuracy measured",
     memory_tetrahedral_rgb8_u8, "float tetrahedral RGB LUT from u8 cells",
     "current prototype", "paired row is tetrahedral_rgb8_u8_fixed",
     "host benchmark only; compressed storage MCU cycles missing", true,
     strategy_tetrahedral_rgb8_u8::setup,
     strategy_tetrahedral_rgb8_u8::solve,
     strategy_tetrahedral_rgb8_u8::teardown, nullptr},
    {"tetrahedral_rgb8_u8_fixed", "hypothesis_2", "PROTO", "RGBW",
     "selected balanced/default fixed u8 tetrahedral candidate; needs production integration and MCU validation",
     memory_tetrahedral_rgb8_u8, "Q12 tetrahedral RGB LUT from u8 cells",
     "paired row is tetrahedral_rgb8_u8", "current implementation",
     "host fixed-path benchmark only; compressed storage MCU cycles missing", true,
     strategy_tetrahedral_rgb8_u8_fixed::setup,
     strategy_tetrahedral_rgb8_u8_fixed::solve,
     strategy_tetrahedral_rgb8_u8_fixed::teardown, nullptr},
    {"tetrahedral_rgb8_boosted", "hypothesis_2", "PROTO", "RGBW",
     "boosted-mode tetrahedral LUT prototype; policy accuracy measured",
     memory_tetrahedral_rgb8, "float tetrahedral boosted RGB LUT",
     "current prototype", "paired row is tetrahedral_rgb8_boosted_fixed",
     "host benchmark only; boosted policy row, MCU cycles missing", false,
     strategy_tetrahedral_rgb8::setup_boosted,
     strategy_tetrahedral_rgb8::solve_boosted,
     strategy_tetrahedral_rgb8::teardown_boosted, nullptr},
    {"tetrahedral_rgb8_boosted_fixed", "hypothesis_2", "PROTO", "RGBW",
     "fixed Q12 evaluator for boosted tetrahedral LUT; needs MCU validation",
     memory_tetrahedral_rgb8, "Q12 tetrahedral boosted RGB LUT",
     "paired row is tetrahedral_rgb8_boosted", "current implementation",
     "host fixed-path benchmark only; boosted policy MCU cycles missing", false,
     strategy_tetrahedral_rgb8_fixed::setup_boosted,
     strategy_tetrahedral_rgb8_fixed::solve_boosted,
     strategy_tetrahedral_rgb8_fixed::teardown_boosted, nullptr},
    {"tetrahedral_rgb8_boosted_u8", "hypothesis_2", "PROTO", "RGBW",
     "u8 packed boosted tetrahedral LUT prototype; policy accuracy measured",
     memory_tetrahedral_rgb8_u8, "float tetrahedral boosted RGB LUT from u8 cells",
     "current prototype", "paired row is tetrahedral_rgb8_boosted_u8_fixed",
     "host benchmark only; boosted packed policy MCU cycles missing", false,
     strategy_tetrahedral_rgb8_u8::setup_boosted,
     strategy_tetrahedral_rgb8_u8::solve_boosted,
     strategy_tetrahedral_rgb8_u8::teardown_boosted, nullptr},
    {"tetrahedral_rgb8_boosted_u8_fixed", "hypothesis_2", "PROTO", "RGBW",
     "fixed evaluator for u8 packed boosted tetrahedral LUT; needs MCU validation",
     memory_tetrahedral_rgb8_u8, "Q12 tetrahedral boosted RGB LUT from u8 cells",
     "paired row is tetrahedral_rgb8_boosted_u8", "current implementation",
     "host fixed-path benchmark only; boosted packed policy MCU cycles missing", false,
     strategy_tetrahedral_rgb8_u8_fixed::setup_boosted,
     strategy_tetrahedral_rgb8_u8_fixed::solve_boosted,
     strategy_tetrahedral_rgb8_u8_fixed::teardown_boosted, nullptr},
    {"poly_piecewise_d3", "hypothesis_3", "PROTO", "RGBW",
     "unconstrained polynomial fit has measured scale-ray drops; constrained fit pending",
     memory_poly_piecewise_d3, "double polynomial evaluator",
     "current prototype", "paired row is poly_piecewise_d3_fixed",
     "host benchmark only; fixed coefficient evaluator measured separately", true,
     strategy_poly_piecewise_d3::setup, strategy_poly_piecewise_d3::solve,
     strategy_poly_piecewise_d3::teardown, nullptr},
    {"poly_piecewise_d3_fixed", "hypothesis_3", "PROTO", "RGBW",
     "fixed Q8 evaluator exists; scale-ray drops still measured",
     memory_poly_piecewise_d3, "Q8 coefficient polynomial evaluator",
     "paired row is poly_piecewise_d3", "current implementation",
     "host fixed-path benchmark only; constrained fit and MCU cycles missing", true,
     strategy_poly_piecewise_d3_fixed::setup,
     strategy_poly_piecewise_d3_fixed::solve,
     strategy_poly_piecewise_d3_fixed::teardown, nullptr},
    {"fixed_analytical_q16", "fixed_closed", "PROTO", "RGBW",
     "observed cube17 Q16 range audit exists; formal overflow proof and hull projection pending",
     memory_fixed_analytical_q16, "Q16 integer analytical",
     "paired float row is float_closed_form", "current implementation",
     "host benchmark and observed Q16 range audit only; formal overflow proof missing", true,
     strategy_fixed_analytical_q16::setup, strategy_fixed_analytical_q16::solve,
     strategy_fixed_analytical_q16::teardown, nullptr},
    {"hybrid_q16_refine_n8", "hypothesis_4", "PROTO", "RGBW",
     "de-prioritized seeded analytical variant; no accuracy gain over fixed_analytical_q16 measured",
     memory_hybrid_q16_refine_n8, "Q16 analytical seeded sub-gamut router",
     "paired row is fixed_analytical_q16", "current implementation",
     "host benchmark and observed Q16 range audit only; keep as evidence unless seed speedup is proven", true,
     strategy_hybrid_q16_refine::setup, strategy_hybrid_q16_refine::solve,
     strategy_hybrid_q16_refine::teardown, nullptr},
    {"bilinear_n16_fixed_interp", "baseline_lut", "REJECTED", "RGBW",
     "fixed bilinear baseline rejected; worse paired accuracy with no memory gain",
     memory_bilinear_n16, "Q12 bilinear LUT interpolation",
     "paired row is bilinear_n16", "rejected baseline",
     "rejected baseline; paired error is too high for production fallback", false,
     strategy_bilinear_n16_fixed_interp::setup,
     strategy_bilinear_n16_fixed_interp::solve,
     strategy_bilinear_n16_fixed_interp::teardown, nullptr},
    {"rgbww_layered_reference", "rgbww_reference", "REF", "RGBWW",
     "current layered RGBWW reference row",
     memory_rgbww_layered_reference, "float layered RGBCCT solve",
     "current implementation", "fixed RGBWW solver not implemented",
     "host benchmark only; MCU cycles missing", false,
     strategy_rgbww_layered_reference::setup, nullptr,
     strategy_rgbww_layered_reference::teardown,
     strategy_rgbww_layered_reference::solve5},
    {"rgbww_strict_5ch_proto", "rgbww_future", "PROTO", "RGBWW",
     "float 5-channel topology prototype; host policy audit exists, LED-bin validation missing",
     memory_rgbww_strict_5ch, "float 5-channel combo solve",
     "current prototype", "paired row is rgbww_strict_5ch_fixed_eval",
     "host benchmark and host policy invariant audit only; LED-bin/hardware validation missing", true,
     strategy_rgbww_strict_5ch::setup, nullptr,
     strategy_rgbww_strict_5ch::teardown,
     strategy_rgbww_strict_5ch::solve5},
    {"rgbww_strict_5ch_fixed_eval", "rgbww_future", "PROTO", "RGBWW",
     "fixed Q16 5-channel combo evaluator; host policy and observed range audits exist; LED-bin validation missing",
     memory_rgbww_strict_5ch_fixed, "Q16 5-channel combo solve",
     "paired row is rgbww_strict_5ch_proto", "current implementation",
     "host benchmark, host policy audit, and observed Q16 range audit only; hardware validation and MCU cycles missing", true,
     strategy_rgbww_strict_5ch_fixed_eval::setup, nullptr,
     strategy_rgbww_strict_5ch_fixed_eval::teardown,
     strategy_rgbww_strict_5ch_fixed_eval::solve5},
    {"wx_lp_q16_residual_rejected", "policy_lp", "REJECTED", "RGBW",
     "fixed residual max-W candidate; observed range audit exists; rejected because it is not balanced LP/hull projection",
     memory_wx_lp_q16_residual, "Q16 residual max-W LP candidate",
     "paired row is wx_lp_legacy_float", "rejected; balanced prototype measured separately",
     "host benchmark and observed Q16 range audit only; rejected row, MCU cycles not needed", false,
     strategy_wx_lp_q16_residual::setup, strategy_wx_lp_q16_residual::solve,
     strategy_wx_lp_q16_residual::teardown, nullptr},
    {"wx_lp_q16_balanced_proto", "policy_lp", "PROTO", "RGBW",
     "fixed bounded max-W candidate; observed range audit exists; hull projection and formal overflow proof pending",
     memory_wx_lp_q16_balanced, "Q16 bounded xy max-W solve",
     "paired row is wx_lp_legacy_float", "current prototype",
     "host benchmark and observed Q16 range audit only; hull projection, formal overflow proof, and MCU cycles missing", false,
     strategy_wx_lp_q16_balanced::setup, strategy_wx_lp_q16_balanced::solve,
     strategy_wx_lp_q16_balanced::teardown, nullptr},
};

static constexpr int kStrategyCount = sizeof(kStrategies) / sizeof(kStrategies[0]);

static constexpr int kClosedFormIndex = 0;

static constexpr int kWxLpIndex = 1;

static constexpr int kWxOverdriveIndex = 2;

static constexpr int kWxOverdriveFixedIndex = 3;

static constexpr int kBilinearN16Index = 4;

static constexpr int kHermiteN16Index = 5;

static constexpr int kHermiteN16FixedIndex = 6;

static constexpr int kHermiteN32Index = 7;

static constexpr int kHermiteN32FixedIndex = 8;

static constexpr int kCellEncodingIndex = 9;

static constexpr int kCellEncodingFixedIndex = 10;

static constexpr int kHullAdaptiveIndex = 11;

static constexpr int kHullAdaptiveFixedIndex = 12;

static constexpr int kTetrahedralIndex = 13;

static constexpr int kTetrahedralFixedIndex = 14;

static constexpr int kTetrahedralU8Index = 15;

static constexpr int kTetrahedralU8FixedIndex = 16;

static constexpr int kTetrahedralBoostIndex = 17;

static constexpr int kTetrahedralBoostFixedIndex = 18;

static constexpr int kTetrahedralBoostU8Index = 19;

static constexpr int kTetrahedralBoostU8FixedIndex = 20;

static constexpr int kPolyPiecewiseIndex = 21;

static constexpr int kPolyPiecewiseFixedIndex = 22;

static constexpr int kFixedQ16Index = 23;

static constexpr int kHybridQ16Index = 24;

static constexpr int kBilinearFixedIndex = 25;

static constexpr int kRgbwwLayeredIndex = 26;

static constexpr int kRgbwwStrictIndex = 27;

static constexpr int kRgbwwStrictFixedIndex = 28;

static constexpr int kWxLpResidualIndex = 29;

static constexpr int kWxLpBalancedIndex = 30;

// Accuracy budgets are deliberately separate from memory/CPU readiness. Missing
// target memory formats and unmeasured MCU cycles stay visible in their tables
// instead of being folded into a single pass/fail label.

static constexpr Budget kBudgets[kStrategyCount][2] = {
    {{0, 0, 0, 0.0f},            {9999, 9999, 9999, 999.0f}},   // float_closed_form
    {{9999, 9999, 9999, 999.0f}, {9999, 9999, 9999, 999.0f}},  // wx_lp_legacy_float
    {{9999, 9999, 9999, 999.0f}, {9999, 9999, 9999, 999.0f}},  // wx_overdrive_float
    {{9999, 9999, 9999, 999.0f}, {9999, 9999, 9999, 999.0f}},  // wx_overdrive_q16
    {{12, 36, 80, 0.0f},         {9999, 9999, 9999, 999.0f}},  // bilinear_n16
    {{8, 24, 48, 0.0f},          {9999, 9999, 9999, 999.0f}},  // hermite_n16
    {{8, 24, 48, 0.0f},          {9999, 9999, 9999, 999.0f}},  // hermite_n16_fixed_lookup
    {{4, 12, 24, 0.0f},          {9999, 9999, 9999, 999.0f}},  // hermite_n32
    {{4, 12, 24, 0.0f},          {9999, 9999, 9999, 999.0f}},  // hermite_n32_fixed_lookup
    {{8, 24, 48, 0.0f},          {9999, 9999, 9999, 999.0f}},  // cell_encoding_q16_n16
    {{8, 24, 48, 0.0f},          {9999, 9999, 9999, 999.0f}},  // cell_encoding_q16_n16_fixed_eval
    {{6, 18, 32, 0.0f},          {9999, 9999, 9999, 999.0f}},  // hull_adaptive_n16
    {{6, 18, 32, 0.0f},          {9999, 9999, 9999, 999.0f}},  // hull_adaptive_n16_fixed_eval
    {{16, 48, 96, 0.0f},         {9999, 9999, 9999, 999.0f}},  // tetrahedral_rgb8
    {{16, 48, 96, 0.0f},         {9999, 9999, 9999, 999.0f}},  // tetrahedral_rgb8_fixed
    {{16, 48, 96, 0.0f},         {9999, 9999, 9999, 999.0f}},  // tetrahedral_rgb8_u8
    {{16, 48, 96, 0.0f},         {9999, 9999, 9999, 999.0f}},  // tetrahedral_rgb8_u8_fixed
    {{9999, 9999, 9999, 999.0f}, {9999, 9999, 9999, 999.0f}},  // tetrahedral_rgb8_boosted
    {{9999, 9999, 9999, 999.0f}, {9999, 9999, 9999, 999.0f}},  // tetrahedral_rgb8_boosted_fixed
    {{9999, 9999, 9999, 999.0f}, {9999, 9999, 9999, 999.0f}},  // tetrahedral_rgb8_boosted_u8
    {{9999, 9999, 9999, 999.0f}, {9999, 9999, 9999, 999.0f}},  // tetrahedral_rgb8_boosted_u8_fixed
    {{16, 64, 128, 0.0f},        {9999, 9999, 9999, 999.0f}},  // poly_piecewise_d3
    {{16, 64, 128, 0.0f},        {9999, 9999, 9999, 999.0f}},  // poly_piecewise_d3_fixed
    {{0, 0, 0, 0.0f},            {9999, 9999, 9999, 999.0f}},  // fixed_analytical_q16
    {{1, 1, 1, 0.0f},            {9999, 9999, 9999, 999.0f}},  // hybrid_q16_refine_n8
    {{12, 36, 80, 0.0f},         {9999, 9999, 9999, 999.0f}},  // bilinear_n16_fixed_interp
    {{9999, 9999, 9999, 999.0f}, {9999, 9999, 9999, 999.0f}},  // rgbww_layered_reference
    {{9999, 9999, 9999, 999.0f}, {9999, 9999, 9999, 12.5f}},   // rgbww_strict_5ch_proto
    {{9999, 9999, 9999, 999.0f}, {9999, 9999, 9999, 12.5f}},   // rgbww_strict_5ch_fixed_eval
    {{9999, 9999, 9999, 999.0f}, {9999, 9999, 9999, 999.0f}},  // wx_lp_q16_residual_rejected
    {{9999, 9999, 9999, 999.0f}, {9999, 9999, 9999, 999.0f}},  // wx_lp_q16_balanced_proto
};

inline void signed_memory_delta_string(u32 value, u32 baseline, char out[32]) {
    if (value == kStrategyBytesUnknown || baseline == kStrategyBytesUnknown) {
        fl::snprintf(out, 32, "%s", "missing");
        return;
    }
    const i64 delta = static_cast<i64>(value) - static_cast<i64>(baseline);
    if (delta == 0) {
        fl::snprintf(out, 32, "%s", "0B");
    } else {
        fl::snprintf(out, 32, "%+lldB", static_cast<long long>(delta));
    }
}

inline void memory_ratio_string(u32 value, u32 baseline, char out[32]) {
    if (value == kStrategyBytesUnknown || baseline == kStrategyBytesUnknown) {
        fl::snprintf(out, 32, "%s", "missing");
        return;
    }
    if (baseline == 0) {
        if (value == 0) {
            fl::snprintf(out, 32, "%s", "n/a");
        } else {
            fl::snprintf(out, 32, "%s", "target 0");
        }
        return;
    }
    const double ratio = static_cast<double>(value) / static_cast<double>(baseline);
    fl::snprintf(out, 32, "%.2fx", ratio);
}

inline const char* memory_gap_class(const MemoryInfo& memory) {
    if (memory.target_packed_bytes == 0) {
        if (memory.prototype_storage_bytes == 0 && memory.runtime_ram_bytes == 0) {
            return "row-owned zero";
        }
        return "state/runtime only";
    }
    if (memory.prototype_storage_bytes == memory.target_packed_bytes) {
        return "target/proto aligned";
    }
    if (memory.prototype_storage_bytes > memory.target_packed_bytes) {
        return "prototype over target";
    }
    return "prototype below target";
}

struct MonotonicStats {
    i32 ray_count = 0;
    i32 comparison_count = 0;
    i32 violating_rays = 0;
    i32 violation_count = 0;
    i32 max_drop_lsb = 0;
};

inline void solve_strategy_channels(const Strategy& strategy,
                                    u8 r,
                                    u8 g,
                                    u8 b,
                                    u8 out[5],
                                    int* channel_count) {
    out[0] = out[1] = out[2] = out[3] = out[4] = 0;
    if (strategy.solve5 != nullptr) {
        *channel_count = 5;
        strategy.solve5(r, g, b, &out[0], &out[1], &out[2], &out[3], &out[4]);
        return;
    }
    *channel_count = 4;
    strategy.solve(r, g, b, &out[0], &out[1], &out[2], &out[3]);
}

inline MonotonicStats evaluate_strategy_scale_monotonicity(
    const Strategy& strategy,
    const DiodeProfile& profile,
    int direction_steps,
    int scale_steps) {
    MonotonicStats stats{};
    if (strategy.solve == nullptr && strategy.solve5 == nullptr) return stats;
    if (direction_steps <= 1 || scale_steps <= 1) return stats;

    if (strategy.setup != nullptr) {
        strategy.setup(profile);
    }

    for (int ri = 0; ri < direction_steps; ++ri) {
        for (int gi = 0; gi < direction_steps; ++gi) {
            for (int bi = 0; bi < direction_steps; ++bi) {
                if ((ri | gi | bi) == 0) continue;

                ++stats.ray_count;
                bool ray_has_violation = false;
                u8 prev[5] = {0, 0, 0, 0, 0};
                int prev_channels = strategy.solve5 != nullptr ? 5 : 4;
                for (int si = 0; si < scale_steps; ++si) {
                    const u8 r = static_cast<u8>(ri * si);
                    const u8 g = static_cast<u8>(gi * si);
                    const u8 b = static_cast<u8>(bi * si);
                    u8 cur[5] = {0, 0, 0, 0, 0};
                    int channels = 0;
                    solve_strategy_channels(strategy, r, g, b, cur, &channels);
                    if (si > 0) {
                        const int compare_channels = fl::min(prev_channels, channels);
                        stats.comparison_count += compare_channels;
                        for (int ch = 0; ch < compare_channels; ++ch) {
                            if (cur[ch] < prev[ch]) {
                                const i32 drop = static_cast<i32>(prev[ch])
                                               - static_cast<i32>(cur[ch]);
                                ++stats.violation_count;
                                if (drop > stats.max_drop_lsb) {
                                    stats.max_drop_lsb = drop;
                                }
                                ray_has_violation = true;
                            }
                        }
                    }
                    for (int ch = 0; ch < channels; ++ch) {
                        prev[ch] = cur[ch];
                    }
                    prev_channels = channels;
                }
                if (ray_has_violation) {
                    ++stats.violating_rays;
                }
            }
        }
    }

    if (strategy.teardown != nullptr) {
        strategy.teardown();
    }

    return stats;
}

inline const char* reference_name_for(int index) {
    switch (index) {
    case kWxLpIndex:
        return "strict CF delta; prod max-W";
    case kWxOverdriveIndex:
        return "strict CF delta; prod boost";
    case kWxOverdriveFixedIndex:
        return "strict CF delta; fixed boost";
    case kTetrahedralBoostIndex:
        return "strict CF delta; boost LUT";
    case kTetrahedralBoostFixedIndex:
        return "strict CF delta; fixed boost LUT";
    case kTetrahedralBoostU8Index:
        return "strict CF delta; u8 boost LUT";
    case kTetrahedralBoostU8FixedIndex:
        return "strict CF delta; fixed u8 boost LUT";
    case kWxLpResidualIndex:
        return "strict CF delta; rejected LP";
    case kWxLpBalancedIndex:
        return "strict CF delta; fixed max-W";
    case kRgbwwLayeredIndex:
    case kRgbwwStrictIndex:
    case kRgbwwStrictFixedIndex:
        return "layered RGBWW reference";
    default:
        return "strict closed-form";
    }
}

inline const char* sample_set_for(int index) {
    switch (index) {
    case kWxOverdriveIndex:
    case kWxOverdriveFixedIndex:
    case kTetrahedralBoostIndex:
    case kTetrahedralBoostFixedIndex:
    case kTetrahedralBoostU8Index:
    case kTetrahedralBoostU8FixedIndex:
        return "verifier_known_bad/native/boost-policy";
    case kRgbwwLayeredIndex:
    case kRgbwwStrictIndex:
    case kRgbwwStrictFixedIndex:
        return "verifier_known_bad/native/RGBWW";
    default:
        return "verifier_known_bad/native/RGBW";
    }
}

inline bool has_text(const char* text) {
    return text != nullptr && text[0] != '\0';
}

inline bool text_equals(const char* a, const char* b) {
    if (a == nullptr || b == nullptr) return false;
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) return false;
        ++a;
        ++b;
    }
    return *a == *b;
}

inline bool text_contains(const char* text, const char* needle) {
    if (text == nullptr || needle == nullptr || needle[0] == '\0') return false;
    for (const char* p = text; *p != '\0'; ++p) {
        const char* a = p;
        const char* b = needle;
        while (*a != '\0' && *b != '\0' && *a == *b) {
            ++a;
            ++b;
        }
        if (*b == '\0') return true;
    }
    return false;
}

inline bool has_placeholder_wording(const char* text) {
    return text_contains(text, "conceptual")
        || text_contains(text, "placeholder")
        || text_contains(text, "TBD");
}

inline const char* fpu_cycles_for(int index) {
    switch (index) {
    case kWxOverdriveFixedIndex:
    case kHermiteN16FixedIndex:
    case kHermiteN32FixedIndex:
    case kCellEncodingFixedIndex:
    case kHullAdaptiveFixedIndex:
    case kTetrahedralFixedIndex:
    case kTetrahedralU8FixedIndex:
    case kTetrahedralBoostFixedIndex:
    case kTetrahedralBoostU8FixedIndex:
    case kPolyPiecewiseFixedIndex:
    case kRgbwwStrictFixedIndex:
        return "missing paired FPU MCU";
    case kFixedQ16Index:
    case kHybridQ16Index:
        return "missing float MCU";
    case kWxLpResidualIndex:
    case kBilinearFixedIndex:
        return "n/a rejected";
    case kWxLpBalancedIndex:
        return "missing wx_lp MCU";
    default:
        return "missing FPU MCU";
    }
}

inline const char* fixed_cycles_for(int index) {
    switch (index) {
    case kClosedFormIndex:
        return "missing soft-float MCU";
    case kWxLpIndex:
        return "missing balanced MCU";
    case kWxOverdriveIndex:
    case kBilinearN16Index:
    case kHermiteN16Index:
    case kHermiteN32Index:
    case kCellEncodingIndex:
    case kHullAdaptiveIndex:
    case kTetrahedralIndex:
    case kTetrahedralU8Index:
    case kTetrahedralBoostIndex:
    case kTetrahedralBoostU8Index:
    case kPolyPiecewiseIndex:
    case kRgbwwStrictIndex:
        return "missing paired fixed MCU";
    case kRgbwwLayeredIndex:
        return "missing RGBWW fixed MCU";
    case kWxLpResidualIndex:
    case kBilinearFixedIndex:
        return "rejected";
    case kWxLpBalancedIndex:
        return "missing fixed MCU";
    default:
        return "missing fixed MCU";
    }
}

inline const char* setup_cost_for(int index) {
    switch (index) {
    case kClosedFormIndex:
    case kWxLpIndex:
    case kWxOverdriveIndex:
        return "build ProfileCache once per profile/CCT";
    case kWxOverdriveFixedIndex:
        return "quantize ProfileCache boosted constants to Q16";
    case kWxLpResidualIndex:
        return "quantize ProfileCache residual constants to Q16";
    case kWxLpBalancedIndex:
        return "quantize ProfileCache P columns and source matrix to Q16";
    case kBilinearN16Index:
        return "sample 16*16 strict endpoints";
    case kHermiteN16Index:
        return "sample 16*16 endpoints/slopes";
    case kHermiteN16FixedIndex:
        return "sample production Hermite 16*16 table, fixed runtime eval";
    case kHermiteN32Index:
        return "sample 32*32 endpoints/slopes";
    case kHermiteN32FixedIndex:
        return "sample production Hermite 32*32 table, fixed runtime eval";
    case kCellEncodingIndex:
        return "sample 16*16 endpoints/slopes into i32 cells";
    case kCellEncodingFixedIndex:
        return "sample 16*16 endpoints/slopes, quantize axes for fixed eval";
    case kHullAdaptiveIndex:
        return "build warped axes, then sample 16*16 endpoints/slopes";
    case kHullAdaptiveFixedIndex:
        return "build warped axes/cells, quantize axes for fixed eval";
    case kTetrahedralIndex:
        return "sample 8*8*8 strict RGB lattice";
    case kTetrahedralFixedIndex:
        return "sample 8*8*8 strict RGB lattice into Q12 cells";
    case kTetrahedralU8Index:
        return "sample 8*8*8 strict RGB lattice into u8 cells";
    case kTetrahedralU8FixedIndex:
        return "sample u8 packed strict RGB lattice, fixed runtime eval";
    case kTetrahedralBoostIndex:
        return "sample 8*8*8 boosted RGB lattice";
    case kTetrahedralBoostFixedIndex:
        return "sample 8*8*8 boosted RGB lattice into Q12 cells";
    case kTetrahedralBoostU8Index:
        return "sample 8*8*8 boosted RGB lattice into u8 cells";
    case kTetrahedralBoostU8FixedIndex:
        return "sample u8 packed boosted RGB lattice, fixed runtime eval";
    case kPolyPiecewiseIndex:
        return "fit 3 pieces of 20-term coefficients";
    case kPolyPiecewiseFixedIndex:
        return "fit 3 pieces, then quantize 20-term coefficients to Q8";
    case kFixedQ16Index:
        return "quantize ProfileCache to Q16 constants";
    case kHybridQ16Index:
        return "quantize Q16 constants and build 8*8 sub-gamut seed router";
    case kBilinearFixedIndex:
        return "sample 16*16 strict endpoints into Q12 cells";
    case kRgbwwLayeredIndex:
        return "no row-owned setup";
    case kRgbwwStrictIndex:
        return "build 5 XYZ columns and nine combo inverses";
    case kRgbwwStrictFixedIndex:
        return "build nine combo inverses, quantize columns/M_src/inverses to Q16";
    default:
        return "unknown";
    }
}

inline const char* runtime_ops_for(int index) {
    switch (index) {
    case kClosedFormIndex:
        return "source XYZ, topology route, strict 3x3 solve, normalize";
    case kWxLpIndex:
        return "source XYZ, hull projection, bounded max-W manifold solve";
    case kWxOverdriveIndex:
        return "strict endpoint plus overdrive blend toward W";
    case kWxOverdriveFixedIndex:
        return "Q16 source XYZ, RGB residual solve, overdrive blend toward W";
    case kWxLpResidualIndex:
        return "Q16 source XYZ, RGB residual solve at strict max-W limit";
    case kWxLpBalancedIndex:
        return "Q16 source xy, bounded 4-channel max-W search, normalize";
    case kBilinearN16Index:
        return "xyY transform, grid address, bilinear 4-channel blend";
    case kHermiteN16Index:
    case kHermiteN32Index:
        return "xyY transform, grid address, Hermite basis, 4-cell value/slope blend";
    case kHermiteN16FixedIndex:
    case kHermiteN32FixedIndex:
        return "Q16 source xyY, integer grid address, fixed Hermite value/slope blend";
    case kCellEncodingIndex:
        return "xyY transform, interval search, float Hermite eval of Q cells";
    case kCellEncodingFixedIndex:
        return "Q16 source xyY, integer interval search, fixed Hermite eval";
    case kHullAdaptiveIndex:
        return "xyY transform, warped-axis search, float Hermite eval of Q cells";
    case kHullAdaptiveFixedIndex:
        return "Q16 source xyY, integer warped-axis search, fixed Hermite eval";
    case kTetrahedralIndex:
        return "RGB cube address, eight fetches, tetrahedral 4-node blend";
    case kTetrahedralFixedIndex:
        return "integer RGB cube address, eight Q12 fetches, tetrahedral 4-node blend";
    case kTetrahedralU8Index:
        return "RGB cube address, eight u8 fetches, tetrahedral 4-node blend";
    case kTetrahedralU8FixedIndex:
        return "integer RGB cube address, eight u8 fetches, tetrahedral 4-node blend";
    case kTetrahedralBoostIndex:
        return "boosted RGB cube address, eight fetches, tetrahedral 4-node blend";
    case kTetrahedralBoostFixedIndex:
        return "boosted integer RGB cube address, eight Q12 fetches, tetrahedral 4-node blend";
    case kTetrahedralBoostU8Index:
        return "boosted RGB cube address, eight u8 fetches, tetrahedral 4-node blend";
    case kTetrahedralBoostU8FixedIndex:
        return "boosted integer RGB cube address, eight u8 fetches, tetrahedral 4-node blend";
    case kPolyPiecewiseIndex:
        return "piece select, 20-term basis, 4-channel polynomial eval";
    case kPolyPiecewiseFixedIndex:
        return "piece select, fixed 20-term basis, i16 coefficient eval";
    case kFixedQ16Index:
        return "Q16 source XYZ, topology route, Q16 3x3/LS2 solve";
    case kHybridQ16Index:
        return "Q16 source XYZ, 8*8 seed lookup, seeded Q16 3x3 solve, full-scan fallback";
    case kBilinearFixedIndex:
        return "Q16 source xyY, integer grid address, bilinear 4-channel blend";
    case kRgbwwLayeredIndex:
        return "eta derivation, warm RGBW solve, cool RGBW solve, blend";
    case kRgbwwStrictIndex:
        return "source XYZ, combo scan or LS2 endpoint, normalize, scale";
    case kRgbwwStrictFixedIndex:
        return "Q16 source XYZ, combo scan or LS2 endpoint, normalize, scale";
    default:
        return "unknown";
    }
}

inline const char* cpu_operation_class_for(int index) {
    switch (index) {
    case kClosedFormIndex:
    case kWxLpIndex:
    case kWxOverdriveIndex:
    case kBilinearN16Index:
    case kHermiteN16Index:
    case kHermiteN32Index:
    case kCellEncodingIndex:
    case kHullAdaptiveIndex:
    case kTetrahedralIndex:
    case kTetrahedralU8Index:
    case kTetrahedralBoostIndex:
    case kTetrahedralBoostU8Index:
    case kPolyPiecewiseIndex:
    case kRgbwwLayeredIndex:
    case kRgbwwStrictIndex:
        return "FPU/float runtime";
    case kWxOverdriveFixedIndex:
    case kHermiteN16FixedIndex:
    case kHermiteN32FixedIndex:
    case kCellEncodingFixedIndex:
    case kHullAdaptiveFixedIndex:
    case kTetrahedralFixedIndex:
    case kTetrahedralU8FixedIndex:
    case kTetrahedralBoostFixedIndex:
    case kTetrahedralBoostU8FixedIndex:
    case kPolyPiecewiseFixedIndex:
    case kFixedQ16Index:
    case kHybridQ16Index:
    case kBilinearFixedIndex:
    case kRgbwwStrictFixedIndex:
    case kWxLpResidualIndex:
    case kWxLpBalancedIndex:
        return "fixed/integer runtime";
    default:
        return "not modeled";
    }
}

inline const char* table_fetch_profile_for(int index) {
    switch (index) {
    case kBilinearN16Index:
    case kBilinearFixedIndex:
        return "4 LUT cells";
    case kHermiteN16Index:
    case kHermiteN16FixedIndex:
    case kHermiteN32Index:
    case kHermiteN32FixedIndex:
    case kCellEncodingIndex:
    case kCellEncodingFixedIndex:
    case kHullAdaptiveIndex:
    case kHullAdaptiveFixedIndex:
        return "4 Hermite cells";
    case kTetrahedralIndex:
    case kTetrahedralFixedIndex:
    case kTetrahedralU8Index:
    case kTetrahedralU8FixedIndex:
    case kTetrahedralBoostIndex:
    case kTetrahedralBoostFixedIndex:
    case kTetrahedralBoostU8Index:
    case kTetrahedralBoostU8FixedIndex:
        return "8 cube nodes";
    case kHybridQ16Index:
        return "1 router cell";
    case kRgbwwStrictIndex:
    case kRgbwwStrictFixedIndex:
        return "9 combo records";
    default:
        return "0 table cells";
    }
}

inline const char* division_profile_for(int index) {
    switch (index) {
    case kBilinearN16Index:
    case kHermiteN16Index:
    case kHermiteN32Index:
    case kCellEncodingIndex:
    case kHullAdaptiveIndex:
        return "float xyY + normalize";
    case kClosedFormIndex:
    case kWxLpIndex:
    case kWxOverdriveIndex:
    case kPolyPiecewiseIndex:
    case kRgbwwLayeredIndex:
    case kRgbwwStrictIndex:
        return "float solve/normalize";
    case kBilinearFixedIndex:
    case kHermiteN16FixedIndex:
    case kHermiteN32FixedIndex:
    case kCellEncodingFixedIndex:
    case kHullAdaptiveFixedIndex:
        return "fixed xyY + normalize";
    case kTetrahedralIndex:
    case kTetrahedralU8Index:
    case kTetrahedralBoostIndex:
    case kTetrahedralBoostU8Index:
        return "float blend normalize";
    case kTetrahedralFixedIndex:
    case kTetrahedralU8FixedIndex:
    case kTetrahedralBoostFixedIndex:
    case kTetrahedralBoostU8FixedIndex:
        return "fixed blend normalize";
    case kWxOverdriveFixedIndex:
    case kWxLpResidualIndex:
    case kWxLpBalancedIndex:
    case kFixedQ16Index:
    case kHybridQ16Index:
    case kRgbwwStrictFixedIndex:
        return "audited integer divisions";
    default:
        return "no division model";
    }
}

inline const char* branch_search_profile_for(int index) {
    switch (index) {
    case kClosedFormIndex:
    case kFixedQ16Index:
        return "topology route";
    case kWxLpIndex:
    case kWxLpBalancedIndex:
        return "bounded max-W search";
    case kWxOverdriveIndex:
    case kWxOverdriveFixedIndex:
    case kWxLpResidualIndex:
        return "policy branch";
    case kBilinearN16Index:
    case kBilinearFixedIndex:
    case kHermiteN16Index:
    case kHermiteN16FixedIndex:
    case kHermiteN32Index:
    case kHermiteN32FixedIndex:
        return "uniform grid address";
    case kCellEncodingIndex:
    case kCellEncodingFixedIndex:
        return "uniform interval search";
    case kHullAdaptiveIndex:
    case kHullAdaptiveFixedIndex:
        return "warped-axis search";
    case kTetrahedralIndex:
    case kTetrahedralFixedIndex:
    case kTetrahedralU8Index:
    case kTetrahedralU8FixedIndex:
    case kTetrahedralBoostIndex:
    case kTetrahedralBoostFixedIndex:
    case kTetrahedralBoostU8Index:
    case kTetrahedralBoostU8FixedIndex:
        return "tetra path select";
    case kPolyPiecewiseIndex:
    case kPolyPiecewiseFixedIndex:
        return "3-piece select";
    case kHybridQ16Index:
        return "seed lookup + fallback";
    case kRgbwwLayeredIndex:
        return "eta blend route";
    case kRgbwwStrictIndex:
    case kRgbwwStrictFixedIndex:
        return "combo scan";
    default:
        return "not modeled";
    }
}

inline const char* fpu_sensitivity_for(int index) {
    if (text_contains(cpu_operation_class_for(index), "fixed")) {
        return "fixed-only viable";
    }
    switch (index) {
    case kBilinearN16Index:
    case kHermiteN16Index:
    case kHermiteN32Index:
    case kCellEncodingIndex:
    case kHullAdaptiveIndex:
    case kTetrahedralIndex:
    case kTetrahedralU8Index:
    case kTetrahedralBoostIndex:
    case kTetrahedralBoostU8Index:
        return "moderate FPU sensitivity";
    case kPolyPiecewiseIndex:
    case kRgbwwLayeredIndex:
    case kRgbwwStrictIndex:
    case kClosedFormIndex:
    case kWxLpIndex:
    case kWxOverdriveIndex:
        return "high FPU sensitivity";
    default:
        return "FPU sensitivity pending";
    }
}

inline bool cycle_cell_has_measurement(const char* text) {
    return has_text(text)
        && !text_contains(text, "missing")
        && !text_contains(text, "rejected")
        && !text_contains(text, "n/a");
}

inline void cpu_evidence_status_string(int index, char out[64]) {
    const bool fpu_measured = cycle_cell_has_measurement(fpu_cycles_for(index));
    const bool fixed_measured = cycle_cell_has_measurement(fixed_cycles_for(index));
    const bool rejected = text_equals(kStrategies[index].stage, "REJECTED");
    if (fpu_measured && fixed_measured) {
        fl::snprintf(out, 64, "%s", "complete MCU cycle evidence");
    } else if (rejected) {
        fl::snprintf(out, 64, "%s", "rejected row; cycles optional");
    } else if (fpu_measured) {
        fl::snprintf(out, 64, "%s", "fixed MCU cycles missing");
    } else if (fixed_measured) {
        fl::snprintf(out, 64, "%s", "FPU MCU cycles missing");
    } else {
        fl::snprintf(out, 64, "%s", "FPU+fixed MCU cycles missing");
    }
}

inline bool has_complete_mcu_cycle_evidence(int index) {
    return cycle_cell_has_measurement(fpu_cycles_for(index))
        && cycle_cell_has_measurement(fixed_cycles_for(index));
}

inline void production_gate_string(int index, char out[96]) {
    if (text_equals(kStrategies[index].stage, "REF")) {
        fl::snprintf(out, 96, "%s", "reference row");
        return;
    }
    if (text_equals(kStrategies[index].stage, "REJECTED")) {
        fl::snprintf(out, 96, "%s", "rejected with evidence");
        return;
    }
    if (text_equals(kStrategies[index].stage, "PROD")) {
        fl::snprintf(out, 96, "%s", "production path exists");
        return;
    }
    fl::snprintf(out, 96, "%s", "prototype only");
}

inline void strategy_readiness_string(int index, char out[128]) {
    if (text_equals(kStrategies[index].stage, "REJECTED")) {
        fl::snprintf(out, 128, "%s", "closed as rejected candidate");
        return;
    }
    if (text_equals(kStrategies[index].stage, "REF")) {
        if (has_complete_mcu_cycle_evidence(index)) {
            fl::snprintf(out, 128, "%s", "reference complete");
        } else {
            fl::snprintf(out, 128, "%s", "reference row; target cycles still open");
        }
        return;
    }
    if (!has_complete_mcu_cycle_evidence(index)) {
        fl::snprintf(out, 128, "%s", "not shippable: MCU cycles missing");
        return;
    }
    if (!text_equals(kStrategies[index].stage, "PROD")) {
        fl::snprintf(out, 128, "%s", "not shippable: production integration missing");
        return;
    }
    fl::snprintf(out, 128, "%s", "shippable evidence complete");
}

inline void fill_lsb_stats(fl::vector<fl::u32>& deltas, ErrorStats* stats) {
    if (deltas.empty()) {
        return;
    }
    fl::sort(deltas.begin(), deltas.end());
    stats->p50_lsb = percentile_u32(deltas, 0.50);
    stats->p75_lsb = percentile_u32(deltas, 0.75);
    stats->p95_lsb = percentile_u32(deltas, 0.95);
    stats->p99_lsb = percentile_u32(deltas, 0.99);
    stats->peak_lsb = static_cast<fl::i32>(deltas.back());
    double sum = 0.0;
    for (fl::u32 d : deltas) {
        sum += static_cast<double>(d);
    }
    stats->mean_lsb = sum / static_cast<double>(deltas.size());
}

inline void solve_public_colorimetric_u8(
    const Rgbw& cfg,
    u8 r, u8 g, u8 b, u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    rgb_2_rgbw(cfg, r, g, b, 255, 255, 255,
               out_r, out_g, out_b, out_w);
}

inline ErrorStats evaluate_public_dispatch_fixture_parity(
    const Strategy& reference,
    RGBW_MODE mode,
    const DiodeProfile& profile,
    RowFilter filter) {
    ErrorStats stats{};
    if (reference.solve == nullptr) {
        return stats;
    }

    fl::shared_ptr<const fl::DiodeProfile> profile_ptr =
        make_diode_profile(profile);
    Rgbw cfg(kRGBWDefaultColorTemp, mode, EOrderW::WDefault, profile_ptr);
    prepare_rgbw_colorimetric(cfg);
    disable_rgbw_colorimetric_lut();
    if (reference.setup != nullptr) {
        reference.setup(profile);
    }

    fl::vector<fl::u32> deltas;
    deltas.reserve(kVerifierRowCount);
    for (int i = 0; i < kVerifierRowCount; ++i) {
        const VerifierRow& row = kVerifierRows[i];
        if (!row_included(row, filter)) {
            continue;
        }
        ++stats.sample_count;
        const u8 r = row_u16_to_u8(row.r_in16);
        const u8 g = row_u16_to_u8(row.g_in16);
        const u8 b = row_u16_to_u8(row.b_in16);
        u8 public_r = 0, public_g = 0, public_b = 0, public_w = 0;
        u8 ref_r = 0, ref_g = 0, ref_b = 0, ref_w = 0;
        solve_public_colorimetric_u8(
            cfg, r, g, b, &public_r, &public_g, &public_b, &public_w);
        reference.solve(r, g, b, &ref_r, &ref_g, &ref_b, &ref_w);
        deltas.push_back(max_channel_lsb_delta(
            public_r, public_g, public_b, public_w,
            ref_r, ref_g, ref_b, ref_w));
    }

    if (reference.teardown != nullptr) {
        reference.teardown();
    }
    disable_rgbw_colorimetric_lut();
    fill_lsb_stats(deltas, &stats);
    return stats;
}

inline ErrorStats evaluate_public_dispatch_cube_parity(
    const Strategy& reference,
    RGBW_MODE mode,
    const DiodeProfile& profile,
    int steps) {
    ErrorStats stats{};
    if (reference.solve == nullptr || steps <= 1) {
        return stats;
    }

    fl::shared_ptr<const fl::DiodeProfile> profile_ptr =
        make_diode_profile(profile);
    Rgbw cfg(kRGBWDefaultColorTemp, mode, EOrderW::WDefault, profile_ptr);
    prepare_rgbw_colorimetric(cfg);
    disable_rgbw_colorimetric_lut();
    if (reference.setup != nullptr) {
        reference.setup(profile);
    }

    fl::vector<fl::u32> deltas;
    deltas.reserve(steps * steps * steps);
    for (int ri = 0; ri < steps; ++ri) {
        const u8 r = cube_value(ri, steps);
        for (int gi = 0; gi < steps; ++gi) {
            const u8 g = cube_value(gi, steps);
            for (int bi = 0; bi < steps; ++bi) {
                const u8 b = cube_value(bi, steps);
                ++stats.sample_count;
                u8 public_r = 0, public_g = 0, public_b = 0, public_w = 0;
                u8 ref_r = 0, ref_g = 0, ref_b = 0, ref_w = 0;
                solve_public_colorimetric_u8(
                    cfg, r, g, b, &public_r, &public_g, &public_b, &public_w);
                reference.solve(r, g, b, &ref_r, &ref_g, &ref_b, &ref_w);
                deltas.push_back(max_channel_lsb_delta(
                    public_r, public_g, public_b, public_w,
                    ref_r, ref_g, ref_b, ref_w));
            }
        }
    }

    if (reference.teardown != nullptr) {
        reference.teardown();
    }
    disable_rgbw_colorimetric_lut();
    fill_lsb_stats(deltas, &stats);
    return stats;
}

inline ErrorStats finalize_lsb_deltas(fl::vector<fl::u32>& deltas) {
    ErrorStats stats{};
    if (deltas.empty()) {
        return stats;
    }
    fl::sort(deltas.begin(), deltas.end());
    stats.sample_count = static_cast<i32>(deltas.size());
    stats.p50_lsb = percentile_u32(deltas, 0.50);
    stats.p75_lsb = percentile_u32(deltas, 0.75);
    stats.p95_lsb = percentile_u32(deltas, 0.95);
    stats.p99_lsb = percentile_u32(deltas, 0.99);
    stats.peak_lsb = static_cast<i32>(deltas.back());
    double sum = 0.0;
    for (fl::u32 delta : deltas) {
        sum += static_cast<double>(delta);
    }
    stats.mean_lsb = sum / static_cast<double>(deltas.size());
    return stats;
}

inline ErrorStats evaluate_closed_form_fixture_lock(
    const Strategy& closed_form,
    const DiodeProfile& profile,
    RowFilter filter) {
    fl::vector<fl::u32> deltas;
    deltas.reserve(kVerifierRowCount);

    if (closed_form.setup != nullptr) {
        closed_form.setup(profile);
    }

    for (int i = 0; i < kVerifierRowCount; ++i) {
        const VerifierRow& row = kVerifierRows[i];
        if (!row_included(row, filter)) {
            continue;
        }
        const u8 r_in = row_u16_to_u8(row.r_in16);
        const u8 g_in = row_u16_to_u8(row.g_in16);
        const u8 b_in = row_u16_to_u8(row.b_in16);
        u8 live_r = 0;
        u8 live_g = 0;
        u8 live_b = 0;
        u8 live_w = 0;
        closed_form.solve(r_in, g_in, b_in,
                          &live_r, &live_g, &live_b, &live_w);
        const u8 fixture_r = row_u16_to_u8(row.cf_r16);
        const u8 fixture_g = row_u16_to_u8(row.cf_g16);
        const u8 fixture_b = row_u16_to_u8(row.cf_b16);
        const u8 fixture_w = row_u16_to_u8(row.cf_w16);
        deltas.push_back(max_channel_lsb_delta(
            live_r, live_g, live_b, live_w,
            fixture_r, fixture_g, fixture_b, fixture_w));
    }

    if (closed_form.teardown != nullptr) {
        closed_form.teardown();
    }

    return finalize_lsb_deltas(deltas);
}

}  // namespace test
