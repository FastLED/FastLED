#pragma once
//
// Colorimetric strategy harness - types and evaluator used by
// tests/misc/colorimetric_strategy_table.cpp (issue #3422).
//
// The fixture provides the sample set and measured target chromaticities. The
// closed-form baseline is computed live from the current solver so the table
// remains valid after solver fixes land.

#include "fl/gfx/rgbw.h"
#include "fl/gfx/rgbw_colorimetric.h"
#include "fl/gfx/rgbww.h"
#include "fl/math/math.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/stdint.h"
#include "fl/stl/vector.h"
#include "tests/misc/golden/verifier_known_bad.hpp"

namespace fl {
namespace colorimetric_test {

static constexpr fl::u32 kStrategyBytesUnknown = 0xffffffffu;

struct MemoryInfo {
    fl::u32 target_packed_bytes;
    fl::u32 prototype_storage_bytes;
    fl::u32 runtime_ram_bytes;
    fl::u32 per_profile_bytes;
    fl::u32 per_controller_bytes;
    const char* source;
    const char* scaling;
    const char* tiny_tier;
};

struct Strategy {
    using Solve4 = void (*)(fl::u8 r, fl::u8 g, fl::u8 b,
                            fl::u8* out_r, fl::u8* out_g,
                            fl::u8* out_b, fl::u8* out_w);
    using Solve5 = void (*)(fl::u8 r, fl::u8 g, fl::u8 b,
                            fl::u8* out_r, fl::u8* out_g,
                            fl::u8* out_b, fl::u8* out_ww,
                            fl::u8* out_wc);
    using Setup = void (*)(const DiodeProfile&);
    using Teardown = void (*)();
    using Memory = MemoryInfo (*)();

    const char* name;
    const char* family;
    const char* stage;
    const char* topology;
    const char* missing;
    Memory memory_info;
    const char* runtime_math;
    const char* fpu_path;
    const char* fixed_path;
    const char* cpu_validation;
    bool budget_gated;
    Setup setup;
    Solve4 solve;
    Teardown teardown;
    Solve5 solve5;

    Strategy(const char* n, const char* f, const char* st, const char* topo,
             const char* miss, Memory memory_fn, const char* math,
             const char* fpu, const char* fixed, const char* validation,
             bool gated, Setup setup_fn,
             Solve4 solve_fn, Teardown teardown_fn,
             Solve5 solve5_fn = nullptr)
        : name(n),
          family(f),
          stage(st),
          topology(topo),
          missing(miss),
          memory_info(memory_fn),
          runtime_math(math),
          fpu_path(fpu),
          fixed_path(fixed),
          cpu_validation(validation),
          budget_gated(gated),
          setup(setup_fn),
          solve(solve_fn),
          teardown(teardown_fn),
          solve5(solve5_fn) {}
};

struct Budget {
    fl::i32 p50_lsb;
    fl::i32 p95_lsb;
    fl::i32 peak_lsb;
    float p95_dE;
};

struct ErrorStats {
    fl::i32 sample_count;
    fl::i32 p50_lsb;
    fl::i32 p75_lsb;
    fl::i32 p95_lsb;
    fl::i32 p99_lsb;
    fl::i32 peak_lsb;
    double mean_lsb;
    float p50_dE;
    float p95_dE;
    float peak_dE;
    double mean_dE;
};

enum class Reference : fl::u8 {
    ClosedForm = 0,
    TargetChroma = 1,
};

enum class RowFilter : fl::u8 {
    All = 0,
    OkOnly = 1,
    FailOnly = 2,
};

inline fl::u8 row_u16_to_u8(fl::u16 v) {
    return static_cast<fl::u8>(v >> 8);
}

inline bool row_included(const fixture::VerifierRow& row, RowFilter filter) {
    using fixture::VerifierRow;
    if (row.status == VerifierRow::kSkip) {
        return filter == RowFilter::All;
    }
    switch (filter) {
    case RowFilter::All:
        return true;
    case RowFilter::OkOnly:
        return row.status == VerifierRow::kOk;
    case RowFilter::FailOnly:
        return row.status == VerifierRow::kFail;
    }
    return false;
}

inline fl::u32 max_channel_lsb_delta(fl::u8 a_r, fl::u8 a_g, fl::u8 a_b, fl::u8 a_w,
                                     fl::u8 b_r, fl::u8 b_g, fl::u8 b_b, fl::u8 b_w) {
    const fl::u32 dr = a_r > b_r ? a_r - b_r : b_r - a_r;
    const fl::u32 dg = a_g > b_g ? a_g - b_g : b_g - a_g;
    const fl::u32 db = a_b > b_b ? a_b - b_b : b_b - a_b;
    const fl::u32 dw = a_w > b_w ? a_w - b_w : b_w - a_w;
    fl::u32 m = dr;
    if (dg > m) m = dg;
    if (db > m) m = db;
    if (dw > m) m = dw;
    return m;
}

inline fl::u32 max_channel_lsb_delta5(fl::u8 a_r, fl::u8 a_g, fl::u8 a_b,
                                      fl::u8 a_ww, fl::u8 a_wc,
                                      fl::u8 b_r, fl::u8 b_g, fl::u8 b_b,
                                      fl::u8 b_ww, fl::u8 b_wc) {
    fl::u32 m = max_channel_lsb_delta(a_r, a_g, a_b, a_ww,
                                      b_r, b_g, b_b, b_ww);
    const fl::u32 dwc = a_wc > b_wc ? a_wc - b_wc : b_wc - a_wc;
    if (dwc > m) m = dwc;
    return m;
}

inline void profile_rgbw_to_xyz(const DiodeProfile& p,
                                fl::u8 r, fl::u8 g, fl::u8 b, fl::u8 w,
                                double out[3]) {
    float P_R[3], P_G[3], P_B[3], P_W[3];
    colorimetric_detail::xyY_to_XYZ(p.xy_r[0], p.xy_r[1], p.lum_r, P_R);
    colorimetric_detail::xyY_to_XYZ(p.xy_g[0], p.xy_g[1], p.lum_g, P_G);
    colorimetric_detail::xyY_to_XYZ(p.xy_b[0], p.xy_b[1], p.lum_b, P_B);
    colorimetric_detail::xyY_to_XYZ(p.xy_w[0], p.xy_w[1], p.lum_w, P_W);

    const double sr = static_cast<double>(r) * (1.0 / 255.0);
    const double sg = static_cast<double>(g) * (1.0 / 255.0);
    const double sb = static_cast<double>(b) * (1.0 / 255.0);
    const double sw = static_cast<double>(w) * (1.0 / 255.0);
    out[0] = sr * P_R[0] + sg * P_G[0] + sb * P_B[0] + sw * P_W[0];
    out[1] = sr * P_R[1] + sg * P_G[1] + sb * P_B[1] + sw * P_W[1];
    out[2] = sr * P_R[2] + sg * P_G[2] + sb * P_B[2] + sw * P_W[2];
}

inline void profile_rgbww_to_xyz(const colorimetric_detail::RgbcctProfile& p,
                                 fl::u8 r, fl::u8 g, fl::u8 b,
                                 fl::u8 ww, fl::u8 wc,
                                 double out[3]) {
    float P_R[3], P_G[3], P_B[3], P_WW[3], P_WC[3];
    const DiodeProfile& warm = p.warm_path;
    const DiodeProfile& cool = p.cool_path;
    colorimetric_detail::xyY_to_XYZ(warm.xy_r[0], warm.xy_r[1], warm.lum_r, P_R);
    colorimetric_detail::xyY_to_XYZ(warm.xy_g[0], warm.xy_g[1], warm.lum_g, P_G);
    colorimetric_detail::xyY_to_XYZ(warm.xy_b[0], warm.xy_b[1], warm.lum_b, P_B);
    colorimetric_detail::xyY_to_XYZ(warm.xy_w[0], warm.xy_w[1], warm.lum_w, P_WW);
    colorimetric_detail::xyY_to_XYZ(cool.xy_w[0], cool.xy_w[1], cool.lum_w, P_WC);

    const double sr = static_cast<double>(r) * (1.0 / 255.0);
    const double sg = static_cast<double>(g) * (1.0 / 255.0);
    const double sb = static_cast<double>(b) * (1.0 / 255.0);
    const double sww = static_cast<double>(ww) * (1.0 / 255.0);
    const double swc = static_cast<double>(wc) * (1.0 / 255.0);
    out[0] = sr * P_R[0] + sg * P_G[0] + sb * P_B[0] + sww * P_WW[0] + swc * P_WC[0];
    out[1] = sr * P_R[1] + sg * P_G[1] + sb * P_B[1] + sww * P_WW[1] + swc * P_WC[1];
    out[2] = sr * P_R[2] + sg * P_G[2] + sb * P_B[2] + sww * P_WW[2] + swc * P_WC[2];
}

inline double lab_f(double t) {
    const double epsilon = 216.0 / 24389.0;
    const double kappa = 24389.0 / 27.0;
    if (t > epsilon) {
        return fl::pow(t, 1.0 / 3.0);
    }
    return (kappa * t + 16.0) / 116.0;
}

inline void xyz_to_lab(const double xyz[3], const double white[3], double lab[3]) {
    const double xr = white[0] > 1e-12 ? xyz[0] / white[0] : 0.0;
    const double yr = white[1] > 1e-12 ? xyz[1] / white[1] : 0.0;
    const double zr = white[2] > 1e-12 ? xyz[2] / white[2] : 0.0;
    const double fx = lab_f(xr);
    const double fy = lab_f(yr);
    const double fz = lab_f(zr);
    lab[0] = 116.0 * fy - 16.0;
    lab[1] = 500.0 * (fx - fy);
    lab[2] = 200.0 * (fy - fz);
}

inline double target_delta_e(const fixture::VerifierRow& row,
                             const DiodeProfile& profile,
                             fl::u8 r, fl::u8 g, fl::u8 b, fl::u8 w) {
    if (row.exp_x <= 0.0f || row.exp_y <= 0.0f) {
        return 0.0;
    }

    double out_xyz[3];
    profile_rgbw_to_xyz(profile, r, g, b, w, out_xyz);
    if (out_xyz[1] <= 1e-12) {
        return 0.0;
    }

    float target_xyz_f[3];
    colorimetric_detail::xyY_to_XYZ(row.exp_x, row.exp_y,
                                    static_cast<float>(out_xyz[1]), target_xyz_f);
    const double target_xyz[3] = {
        target_xyz_f[0],
        target_xyz_f[1],
        target_xyz_f[2],
    };

    float white_f[3];
    const float white_x = profile.input_xy_w[1] > 0.0f
        ? profile.input_xy_w[0]
        : 0.31272f;
    const float white_y = profile.input_xy_w[1] > 0.0f
        ? profile.input_xy_w[1]
        : 0.32903f;
    const float white_Y = profile.lum_r + profile.lum_g + profile.lum_b + profile.lum_w;
    colorimetric_detail::xyY_to_XYZ(white_x, white_y, white_Y, white_f);
    const double white_xyz[3] = { white_f[0], white_f[1], white_f[2] };

    double out_lab[3], target_lab[3];
    xyz_to_lab(out_xyz, white_xyz, out_lab);
    xyz_to_lab(target_xyz, white_xyz, target_lab);
    const double dL = out_lab[0] - target_lab[0];
    const double da = out_lab[1] - target_lab[1];
    const double db = out_lab[2] - target_lab[2];
    return fl::sqrt(dL * dL + da * da + db * db);
}

inline double target_delta_e_rgbww(const fixture::VerifierRow& row,
                                   const colorimetric_detail::RgbcctProfile& profile,
                                   fl::u8 r, fl::u8 g, fl::u8 b,
                                   fl::u8 ww, fl::u8 wc) {
    if (row.exp_x <= 0.0f || row.exp_y <= 0.0f) {
        return 0.0;
    }

    double out_xyz[3];
    profile_rgbww_to_xyz(profile, r, g, b, ww, wc, out_xyz);
    if (out_xyz[1] <= 1e-12) {
        return 0.0;
    }

    float target_xyz_f[3];
    colorimetric_detail::xyY_to_XYZ(row.exp_x, row.exp_y,
                                    static_cast<float>(out_xyz[1]), target_xyz_f);
    const double target_xyz[3] = {
        target_xyz_f[0],
        target_xyz_f[1],
        target_xyz_f[2],
    };

    float white_f[3];
    const float white_x = profile.warm_path.input_xy_w[1] > 0.0f
        ? profile.warm_path.input_xy_w[0]
        : 0.31272f;
    const float white_y = profile.warm_path.input_xy_w[1] > 0.0f
        ? profile.warm_path.input_xy_w[1]
        : 0.32903f;
    const float white_Y = profile.warm_path.lum_r + profile.warm_path.lum_g
                        + profile.warm_path.lum_b + profile.warm_path.lum_w
                        + profile.cool_path.lum_w;
    colorimetric_detail::xyY_to_XYZ(white_x, white_y, white_Y, white_f);
    const double white_xyz[3] = { white_f[0], white_f[1], white_f[2] };

    double out_lab[3], target_lab[3];
    xyz_to_lab(out_xyz, white_xyz, out_lab);
    xyz_to_lab(target_xyz, white_xyz, target_lab);
    const double dL = out_lab[0] - target_lab[0];
    const double da = out_lab[1] - target_lab[1];
    const double db = out_lab[2] - target_lab[2];
    return fl::sqrt(dL * dL + da * da + db * db);
}

inline float rgbww_eta_for_input(const colorimetric_detail::RgbcctProfile& profile,
                                 float s_r, float s_g, float s_b) {
    float M_src[3][3];
    float X = 0.0f, Y = 0.0f, Z = 0.0f;
    const DiodeProfile& wp = profile.warm_path;
    if (wp.input_xy_w[1] > 1e-6f &&
        colorimetric_detail::build_source_matrix(wp.input_xy_r, wp.input_xy_g,
                                                 wp.input_xy_b, wp.input_xy_w,
                                                 M_src)) {
        const float s[3] = { s_r, s_g, s_b };
        float xyz[3];
        colorimetric_detail::matvec3(M_src, s, xyz);
        X = xyz[0]; Y = xyz[1]; Z = xyz[2];
    } else {
        float P_R[3], P_G[3], P_B[3];
        colorimetric_detail::xyY_to_XYZ(wp.xy_r[0], wp.xy_r[1], wp.lum_r, P_R);
        colorimetric_detail::xyY_to_XYZ(wp.xy_g[0], wp.xy_g[1], wp.lum_g, P_G);
        colorimetric_detail::xyY_to_XYZ(wp.xy_b[0], wp.xy_b[1], wp.lum_b, P_B);
        X = P_R[0] * s_r + P_G[0] * s_g + P_B[0] * s_b;
        Y = P_R[1] * s_r + P_G[1] * s_g + P_B[1] * s_b;
        Z = P_R[2] * s_r + P_G[2] * s_g + P_B[2] * s_b;
    }
    const float sum = X + Y + Z;
    if (sum < 1e-9f) return 0.5f;
    const float input_x = X / sum;
    const float warm_x = profile.warm_path.xy_w[0];
    const float cool_x = profile.cool_path.xy_w[0];
    if (cool_x == warm_x) return 0.5f;
    return fl::clamp((input_x - warm_x) / (cool_x - warm_x), 0.0f, 1.0f);
}

inline void rgbww_layered_reference_u8(fl::u8 r, fl::u8 g, fl::u8 b,
                                       fl::u8* out_r, fl::u8* out_g,
                                       fl::u8* out_b, fl::u8* out_ww,
                                       fl::u8* out_wc) {
    if ((r | g | b) == 0) {
        *out_r = *out_g = *out_b = *out_ww = *out_wc = 0;
        return;
    }
    const float s_r = r * (1.0f / 255.0f);
    const float s_g = g * (1.0f / 255.0f);
    const float s_b = b * (1.0f / 255.0f);
    const auto& profile = kRgbwwDefaultProfile;
    const float eta = rgbww_eta_for_input(profile, s_r, s_g, s_b);
    float out[5];
    colorimetric_detail::solve_rgbcct(profile, s_r, s_g, s_b, eta, out);
    *out_r = colorimetric_detail::quantize_u8(out[0]);
    *out_g = colorimetric_detail::quantize_u8(out[1]);
    *out_b = colorimetric_detail::quantize_u8(out[2]);
    *out_ww = colorimetric_detail::quantize_u8(out[3]);
    *out_wc = colorimetric_detail::quantize_u8(out[4]);
}

inline fl::i32 percentile_u32(const fl::vector<fl::u32>& values, double p) {
    if (values.empty()) return 0;
    int idx = static_cast<int>(p * static_cast<double>(values.size() - 1));
    if (idx < 0) idx = 0;
    if (idx >= static_cast<int>(values.size())) idx = static_cast<int>(values.size()) - 1;
    return static_cast<fl::i32>(values[idx]);
}

inline float percentile_double(const fl::vector<double>& values, double p) {
    if (values.empty()) return 0.0f;
    int idx = static_cast<int>(p * static_cast<double>(values.size() - 1));
    if (idx < 0) idx = 0;
    if (idx >= static_cast<int>(values.size())) idx = static_cast<int>(values.size()) - 1;
    return static_cast<float>(values[idx]);
}

inline ErrorStats evaluate_strategy(const Strategy& strategy,
                                    const Strategy& closed_form,
                                    const DiodeProfile& profile,
                                    Reference reference,
                                    RowFilter filter) {
    using namespace fixture;

    ErrorStats stats{};
    if (strategy.solve == nullptr && strategy.solve5 == nullptr) {
        return stats;
    }

    const bool needs_closed_form =
        reference == Reference::ClosedForm
        && strategy.solve5 == nullptr
        && strategy.solve != closed_form.solve;

    if (strategy.setup != nullptr) {
        strategy.setup(profile);
    }
    if (needs_closed_form) {
        if (closed_form.setup != nullptr) {
            closed_form.setup(profile);
        }
    }

    fl::vector<fl::u32> deltas;
    fl::vector<double> dEs;
    deltas.reserve(kVerifierRowCount);
    dEs.reserve(kVerifierRowCount);

    for (int i = 0; i < kVerifierRowCount; ++i) {
        const VerifierRow& row = kVerifierRows[i];
        if (!row_included(row, filter)) continue;
        ++stats.sample_count;

        const fl::u8 r_in = row_u16_to_u8(row.r_in16);
        const fl::u8 g_in = row_u16_to_u8(row.g_in16);
        const fl::u8 b_in = row_u16_to_u8(row.b_in16);

        if (strategy.solve5 != nullptr) {
            fl::u8 s_r = 0, s_g = 0, s_b = 0, s_ww = 0, s_wc = 0;
            strategy.solve5(r_in, g_in, b_in, &s_r, &s_g, &s_b, &s_ww, &s_wc);
            if (reference == Reference::ClosedForm) {
                fl::u8 c_r = 0, c_g = 0, c_b = 0, c_ww = 0, c_wc = 0;
                rgbww_layered_reference_u8(r_in, g_in, b_in,
                                           &c_r, &c_g, &c_b, &c_ww, &c_wc);
                deltas.push_back(max_channel_lsb_delta5(
                    s_r, s_g, s_b, s_ww, s_wc,
                    c_r, c_g, c_b, c_ww, c_wc));
            } else {
                dEs.push_back(target_delta_e_rgbww(
                    row, kRgbwwDefaultProfile, s_r, s_g, s_b, s_ww, s_wc));
            }
        } else if (reference == Reference::ClosedForm) {
            fl::u8 s_r = 0, s_g = 0, s_b = 0, s_w = 0;
            strategy.solve(r_in, g_in, b_in, &s_r, &s_g, &s_b, &s_w);
            fl::u8 c_r = s_r, c_g = s_g, c_b = s_b, c_w = s_w;
            if (needs_closed_form) {
                closed_form.solve(r_in, g_in, b_in, &c_r, &c_g, &c_b, &c_w);
            }
            deltas.push_back(max_channel_lsb_delta(s_r, s_g, s_b, s_w,
                                                   c_r, c_g, c_b, c_w));
        } else {
            fl::u8 s_r = 0, s_g = 0, s_b = 0, s_w = 0;
            strategy.solve(r_in, g_in, b_in, &s_r, &s_g, &s_b, &s_w);
            dEs.push_back(target_delta_e(row, profile, s_r, s_g, s_b, s_w));
        }
    }

    if (needs_closed_form) {
        if (closed_form.teardown != nullptr) {
            closed_form.teardown();
        }
    }
    if (strategy.teardown != nullptr) {
        strategy.teardown();
    }

    if (!deltas.empty()) {
        fl::sort(deltas.begin(), deltas.end());
        stats.p50_lsb = percentile_u32(deltas, 0.50);
        stats.p75_lsb = percentile_u32(deltas, 0.75);
        stats.p95_lsb = percentile_u32(deltas, 0.95);
        stats.p99_lsb = percentile_u32(deltas, 0.99);
        stats.peak_lsb = static_cast<fl::i32>(deltas.back());
        double sum = 0.0;
        for (fl::u32 d : deltas) sum += static_cast<double>(d);
        stats.mean_lsb = sum / static_cast<double>(deltas.size());
    }

    if (!dEs.empty()) {
        fl::sort(dEs.begin(), dEs.end());
        stats.p50_dE = percentile_double(dEs, 0.50);
        stats.p95_dE = percentile_double(dEs, 0.95);
        stats.peak_dE = static_cast<float>(dEs.back());
        double sum = 0.0;
        for (double d : dEs) sum += d;
        stats.mean_dE = sum / static_cast<double>(dEs.size());
    }

    return stats;
}

inline ErrorStats evaluate_strategy_pair_lsb(const Strategy& strategy,
                                             const Strategy& reference,
                                             const DiodeProfile& profile,
                                             RowFilter filter) {
    using namespace fixture;

    ErrorStats stats{};
    const bool strategy_is_rgbww = strategy.solve5 != nullptr;
    const bool reference_is_rgbww = reference.solve5 != nullptr;
    if (strategy_is_rgbww != reference_is_rgbww) {
        return stats;
    }
    if (!strategy_is_rgbww && (strategy.solve == nullptr || reference.solve == nullptr)) {
        return stats;
    }
    if (strategy_is_rgbww && (strategy.solve5 == nullptr || reference.solve5 == nullptr)) {
        return stats;
    }

    if (reference.setup != nullptr) {
        reference.setup(profile);
    }
    if (strategy.setup != nullptr) {
        strategy.setup(profile);
    }

    fl::vector<fl::u32> deltas;
    deltas.reserve(kVerifierRowCount);

    for (int i = 0; i < kVerifierRowCount; ++i) {
        const VerifierRow& row = kVerifierRows[i];
        if (!row_included(row, filter)) continue;
        ++stats.sample_count;

        const fl::u8 r_in = row_u16_to_u8(row.r_in16);
        const fl::u8 g_in = row_u16_to_u8(row.g_in16);
        const fl::u8 b_in = row_u16_to_u8(row.b_in16);

        if (strategy_is_rgbww) {
            fl::u8 s_r = 0, s_g = 0, s_b = 0, s_ww = 0, s_wc = 0;
            fl::u8 r_r = 0, r_g = 0, r_b = 0, r_ww = 0, r_wc = 0;
            strategy.solve5(r_in, g_in, b_in, &s_r, &s_g, &s_b, &s_ww, &s_wc);
            reference.solve5(r_in, g_in, b_in, &r_r, &r_g, &r_b, &r_ww, &r_wc);
            deltas.push_back(max_channel_lsb_delta5(
                s_r, s_g, s_b, s_ww, s_wc,
                r_r, r_g, r_b, r_ww, r_wc));
        } else {
            fl::u8 s_r = 0, s_g = 0, s_b = 0, s_w = 0;
            fl::u8 r_r = 0, r_g = 0, r_b = 0, r_w = 0;
            strategy.solve(r_in, g_in, b_in, &s_r, &s_g, &s_b, &s_w);
            reference.solve(r_in, g_in, b_in, &r_r, &r_g, &r_b, &r_w);
            deltas.push_back(max_channel_lsb_delta(s_r, s_g, s_b, s_w,
                                                   r_r, r_g, r_b, r_w));
        }
    }

    if (strategy.teardown != nullptr) {
        strategy.teardown();
    }
    if (reference.teardown != nullptr) {
        reference.teardown();
    }

    if (!deltas.empty()) {
        fl::sort(deltas.begin(), deltas.end());
        stats.p50_lsb = percentile_u32(deltas, 0.50);
        stats.p75_lsb = percentile_u32(deltas, 0.75);
        stats.p95_lsb = percentile_u32(deltas, 0.95);
        stats.p99_lsb = percentile_u32(deltas, 0.99);
        stats.peak_lsb = static_cast<fl::i32>(deltas.back());
        double sum = 0.0;
        for (fl::u32 d : deltas) sum += static_cast<double>(d);
        stats.mean_lsb = sum / static_cast<double>(deltas.size());
    }

    return stats;
}

inline fl::u8 cube_value(int index, int steps) {
    if (steps <= 1) return 0;
    const int value = (index * 255 + ((steps - 1) / 2)) / (steps - 1);
    return static_cast<fl::u8>(value < 0 ? 0 : (value > 255 ? 255 : value));
}

inline ErrorStats evaluate_strategy_cube_lsb(const Strategy& strategy,
                                             const Strategy& reference,
                                             const DiodeProfile& profile,
                                             int steps) {
    ErrorStats stats{};
    if (steps <= 1) return stats;

    const bool strategy_is_rgbww = strategy.solve5 != nullptr;
    const bool reference_is_rgbww = reference.solve5 != nullptr;
    if (strategy_is_rgbww != reference_is_rgbww) return stats;
    if (!strategy_is_rgbww && (strategy.solve == nullptr || reference.solve == nullptr)) {
        return stats;
    }
    if (strategy_is_rgbww && (strategy.solve5 == nullptr || reference.solve5 == nullptr)) {
        return stats;
    }

    if (reference.setup != nullptr) {
        reference.setup(profile);
    }
    if (strategy.setup != nullptr) {
        strategy.setup(profile);
    }

    fl::vector<fl::u32> deltas;
    deltas.reserve(steps * steps * steps);

    for (int ri = 0; ri < steps; ++ri) {
        const fl::u8 r_in = cube_value(ri, steps);
        for (int gi = 0; gi < steps; ++gi) {
            const fl::u8 g_in = cube_value(gi, steps);
            for (int bi = 0; bi < steps; ++bi) {
                const fl::u8 b_in = cube_value(bi, steps);
                ++stats.sample_count;
                if (strategy_is_rgbww) {
                    fl::u8 s_r = 0, s_g = 0, s_b = 0, s_ww = 0, s_wc = 0;
                    fl::u8 r_r = 0, r_g = 0, r_b = 0, r_ww = 0, r_wc = 0;
                    strategy.solve5(
                        r_in, g_in, b_in, &s_r, &s_g, &s_b, &s_ww, &s_wc);
                    reference.solve5(
                        r_in, g_in, b_in, &r_r, &r_g, &r_b, &r_ww, &r_wc);
                    deltas.push_back(max_channel_lsb_delta5(
                        s_r, s_g, s_b, s_ww, s_wc,
                        r_r, r_g, r_b, r_ww, r_wc));
                } else {
                    fl::u8 s_r = 0, s_g = 0, s_b = 0, s_w = 0;
                    fl::u8 r_r = 0, r_g = 0, r_b = 0, r_w = 0;
                    strategy.solve(r_in, g_in, b_in, &s_r, &s_g, &s_b, &s_w);
                    reference.solve(r_in, g_in, b_in, &r_r, &r_g, &r_b, &r_w);
                    deltas.push_back(max_channel_lsb_delta(
                        s_r, s_g, s_b, s_w, r_r, r_g, r_b, r_w));
                }
            }
        }
    }

    if (strategy.teardown != nullptr) {
        strategy.teardown();
    }
    if (reference.teardown != nullptr) {
        reference.teardown();
    }

    if (!deltas.empty()) {
        fl::sort(deltas.begin(), deltas.end());
        stats.p50_lsb = percentile_u32(deltas, 0.50);
        stats.p75_lsb = percentile_u32(deltas, 0.75);
        stats.p95_lsb = percentile_u32(deltas, 0.95);
        stats.p99_lsb = percentile_u32(deltas, 0.99);
        stats.peak_lsb = static_cast<fl::i32>(deltas.back());
        double sum = 0.0;
        for (fl::u32 d : deltas) sum += static_cast<double>(d);
        stats.mean_lsb = sum / static_cast<double>(deltas.size());
    }

    return stats;
}

inline bool stats_within_budget(const ErrorStats& stats, const Budget& budget) {
    if (stats.p50_lsb > budget.p50_lsb) return false;
    if (stats.p95_lsb > budget.p95_lsb) return false;
    if (stats.peak_lsb > budget.peak_lsb) return false;
    if (stats.p95_dE > budget.p95_dE) return false;
    return true;
}

}  // namespace colorimetric_test
}  // namespace fl
