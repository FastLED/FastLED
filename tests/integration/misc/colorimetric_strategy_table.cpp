// Colorimetric strategy comparison table (#3422), integration tier.
//
// Issue #3422 asks for every proposed approximation strategy to appear as a
// row in a harness and for each run to emit a table that answers which
// strategy is most accurate against the live closed-form solver. The table
// costs several seconds of CPU, so it is not part of the default
// `bash test` run. Run it with `bash test --integration` or
// `bash test colorimetric_strategy_table`. The strategies themselves live in
// tests/unit/misc/colorimetric_strategies.hpp.

#include "test.h"

#include "tests/unit/misc/colorimetric_strategies.hpp"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;
using namespace fl::colorimetric_detail;
using namespace fl::colorimetric_test;
using namespace fl::colorimetric_test::fixture;

struct Q16StaticBoundStats {
    bool measured = false;
    i64 max_abs_coeff_q = 0;
    double max_source_x_q = 0.0;
    double max_matvec_product = 0.0;
    double max_matvec_accumulator = 0.0;
    double max_matvec_output_q = 0.0;
    double max_ls2_scaled_numerator = 0.0;
    double max_bound = 0.0;
    const char* evidence = "n/a";
};

struct RgbwwPolicyAuditStats {
    i32 sample_count = 0;
    i32 off_count = 0;
    i32 native_single_count = 0;
    i32 ls2_count = 0;
    i32 combo_count[9] = {};
    i32 fallback_count = 0;
};

static constexpr int kRgbwwRouteSlotCount = 13;

struct RgbwwRouteErrorAuditStats {
    i32 sample_count = 0;
    ErrorStats route[kRgbwwRouteSlotCount] = {};
};

struct RgbwwPhysicalPolicyStats {
    i32 sample_count = 0;
    i32 fallback_count = 0;
    i32 native_single_samples = 0;
    i32 native_single_white_violations = 0;
    i32 two_channel_samples = 0;
    i32 two_channel_white_violations = 0;
    i32 combo_samples = 0;
    i32 combo_without_white_count = 0;
    i32 warm_only_combo_count = 0;
    i32 cool_only_combo_count = 0;
    i32 warm_cool_combo_count = 0;
    i32 fixed_route_mismatch_count = 0;
    ErrorStats fixed_output_delta;
};

static constexpr bool kReferenceSurfaceLocked = false;

static constexpr bool kFpuTolerancePolicyLocked = false;

struct CpuPair {
    const char* label;
    int fpu_index;
    int fixed_index;
    const char* scope;
};

static const CpuPair kCpuPairs[] = {
    {"boosted policy", kWxOverdriveIndex, kWxOverdriveFixedIndex,
     "FPU vs fixed boosted policy"},
    {"LP balanced proto", kWxLpIndex, kWxLpBalancedIndex,
     "production LP vs fixed bounded max-W prototype"},
    {"LP residual rejected", kWxLpIndex, kWxLpResidualIndex,
     "production LP vs rejected residual fixed candidate"},
    {"bilinear N16", kBilinearN16Index, kBilinearFixedIndex,
     "float LUT vs rejected fixed interpolation baseline"},
    {"Hermite N16", kHermiteN16Index, kHermiteN16FixedIndex,
     "float LUT vs fixed runtime lookup"},
    {"Hermite N32", kHermiteN32Index, kHermiteN32FixedIndex,
     "float LUT vs fixed runtime lookup"},
    {"Q16 cell N16", kCellEncodingIndex, kCellEncodingFixedIndex,
     "float cell evaluator vs fixed cell evaluator"},
    {"hull adaptive N16", kHullAdaptiveIndex, kHullAdaptiveFixedIndex,
     "float warped-cell evaluator vs fixed warped-cell evaluator"},
    {"tetra RGB8 i16", kTetrahedralIndex, kTetrahedralFixedIndex,
     "float tetrahedral LUT vs Q12 fixed evaluator"},
    {"tetra RGB8 u8", kTetrahedralU8Index, kTetrahedralU8FixedIndex,
     "float u8 LUT vs fixed u8 evaluator"},
    {"boosted tetra i16", kTetrahedralBoostIndex, kTetrahedralBoostFixedIndex,
     "float boosted LUT vs fixed boosted evaluator"},
    {"boosted tetra u8", kTetrahedralBoostU8Index,
     kTetrahedralBoostU8FixedIndex,
     "float boosted u8 LUT vs fixed boosted u8 evaluator"},
    {"piecewise D3", kPolyPiecewiseIndex, kPolyPiecewiseFixedIndex,
     "double polynomial evaluator vs Q8 fixed evaluator"},
    {"closed-form Q16", kClosedFormIndex, kFixedQ16Index,
     "float strict closed-form vs Q16 analytical prototype"},
    {"hybrid seed router", kFixedQ16Index, kHybridQ16Index,
     "Q16 analytical baseline vs seeded Q16 router"},
    {"RGBWW strict", kRgbwwStrictIndex, kRgbwwStrictFixedIndex,
     "float 5-channel combo solve vs Q16 combo solve"},
};

static constexpr int kCpuPairCount = sizeof(kCpuPairs) / sizeof(kCpuPairs[0]);

inline const char* status_for(const Strategy& strategy,
                              const ErrorStats& cf_stats,
                              const ErrorStats& target_stats,
                              const Budget& cf_budget,
                              const Budget& target_budget) {
    if (!strategy.budget_gated) return strategy.stage;
    if (cf_stats.p50_lsb > cf_budget.p50_lsb) return "FAIL p50";
    if (cf_stats.p95_lsb > cf_budget.p95_lsb) return "FAIL p95";
    if (cf_stats.peak_lsb > cf_budget.peak_lsb) return "FAIL peak";
    if (target_stats.p95_dE > target_budget.p95_dE) return "FAIL dE";
    return "OK";
}

inline void memory_string(u32 bytes, char out[24]) {
    if (bytes == kStrategyBytesUnknown) {
        fl::snprintf(out, 24, "%s", "missing");
        return;
    }
    if (bytes == 0) {
        fl::snprintf(out, 24, "%s", "0B");
    } else {
        fl::snprintf(out, 24, "%luB", static_cast<unsigned long>(bytes));
    }
}

inline void relative_string(const Strategy& strategy,
                            const MemoryInfo& memory,
                            const ErrorStats& stats,
                            const ErrorStats& hermite_stats,
                            char out[64]) {
    if (stats.sample_count == 0) {
        fl::snprintf(out, 64, "n/a");
        return;
    }
    if (stats.peak_lsb == 0) {
        fl::snprintf(out, 64, "exact / no LUT");
        return;
    }
    const double err = static_cast<double>(hermite_stats.peak_lsb) /
                       static_cast<double>(stats.peak_lsb);
    const MemoryInfo hermite_memory = kStrategies[kHermiteN16Index].memory_info();
    if (memory.target_packed_bytes == kStrategyBytesUnknown) {
        fl::snprintf(out, 64, "%.1fx err / mem missing", err);
        return;
    }
    if (memory.target_packed_bytes == 0) {
        fl::snprintf(out, 64, "%.1fx err / no table", err);
        return;
    }
    const double mem = static_cast<double>(hermite_memory.target_packed_bytes) /
                       static_cast<double>(memory.target_packed_bytes);
    fl::snprintf(out, 64, "%.1fx err / %.1fx mem", err, mem);
}

struct BenchStats {
    bool measured = false;
    double host_ns_per_pixel = 0.0;
    u32 checksum = 0;
};

static volatile u32 g_bench_sink = 0;

inline BenchStats benchmark_strategy(const Strategy& strategy,
                                     const DiodeProfile& profile) {
    BenchStats out;
    if (strategy.solve == nullptr && strategy.solve5 == nullptr) {
        return out;
    }

    static constexpr int kWarmupRepeats = 8;
    static constexpr int kMeasureRepeats = 256;

    if (strategy.setup != nullptr) {
        strategy.setup(profile);
    }

    u32 checksum = 0;
    for (int pass = 0; pass < kWarmupRepeats; ++pass) {
        for (int i = 0; i < kVerifierRowCount; ++i) {
            const VerifierRow& row = kVerifierRows[i];
            const u8 r = row_u16_to_u8(row.r_in16);
            const u8 g = row_u16_to_u8(row.g_in16);
            const u8 b = row_u16_to_u8(row.b_in16);
            u8 ro = 0, go = 0, bo = 0, wo = 0, wc = 0;
            if (strategy.solve5 != nullptr) {
                strategy.solve5(r, g, b, &ro, &go, &bo, &wo, &wc);
            } else {
                strategy.solve(r, g, b, &ro, &go, &bo, &wo);
            }
            checksum += ro + (static_cast<u32>(go) << 1)
                      + (static_cast<u32>(bo) << 2)
                      + (static_cast<u32>(wo) << 3)
                      + (static_cast<u32>(wc) << 4);
        }
    }

    const u32 t0 = fl::micros();
    for (int pass = 0; pass < kMeasureRepeats; ++pass) {
        for (int i = 0; i < kVerifierRowCount; ++i) {
            const VerifierRow& row = kVerifierRows[i];
            const u8 r = row_u16_to_u8(row.r_in16);
            const u8 g = row_u16_to_u8(row.g_in16);
            const u8 b = row_u16_to_u8(row.b_in16);
            u8 ro = 0, go = 0, bo = 0, wo = 0, wc = 0;
            if (strategy.solve5 != nullptr) {
                strategy.solve5(r, g, b, &ro, &go, &bo, &wo, &wc);
            } else {
                strategy.solve(r, g, b, &ro, &go, &bo, &wo);
            }
            checksum += ro + (static_cast<u32>(go) << 1)
                      + (static_cast<u32>(bo) << 2)
                      + (static_cast<u32>(wo) << 3)
                      + (static_cast<u32>(wc) << 4);
        }
    }
    const u32 t1 = fl::micros();

    if (strategy.teardown != nullptr) {
        strategy.teardown();
    }

    const u32 elapsed_us = t1 - t0;
    const double samples = static_cast<double>(kMeasureRepeats)
                         * static_cast<double>(kVerifierRowCount);
    out.measured = elapsed_us > 0;
    out.host_ns_per_pixel = out.measured
        ? (static_cast<double>(elapsed_us) * 1000.0) / samples
        : 0.0;
    out.checksum = checksum;
    g_bench_sink ^= checksum;
    return out;
}

inline void monotonic_string(const MonotonicStats& stats, char out[48]) {
    if (stats.ray_count == 0) {
        fl::snprintf(out, 48, "%s", "n/a");
        return;
    }
    fl::snprintf(out, 48, "%ld rays / %ld drops / %ld max",
             static_cast<long>(stats.violating_rays),
             static_cast<long>(stats.violation_count),
             static_cast<long>(stats.max_drop_lsb));
}

inline bool has_q16_range_audit(int index) {
    return index == kWxOverdriveFixedIndex
        || index == kFixedQ16Index
        || index == kHybridQ16Index
        || index == kRgbwwStrictFixedIndex
        || index == kWxLpResidualIndex
        || index == kWxLpBalancedIndex;
}

inline Q16RangeStats evaluate_q16_range_audit(int index,
                                              const DiodeProfile& profile,
                                              int cube_steps) {
    if (index == kFixedQ16Index) {
        return strategy_fixed_analytical_q16::audit_range(profile, cube_steps);
    }
    if (index == kWxOverdriveFixedIndex) {
        return strategy_wx_overdrive_q16::audit_range(profile, cube_steps);
    }
    if (index == kHybridQ16Index) {
        return strategy_hybrid_q16_refine::audit_range(profile, cube_steps);
    }
    if (index == kRgbwwStrictFixedIndex) {
        return strategy_rgbww_strict_5ch_fixed_eval::audit_range(
            profile, cube_steps);
    }
    if (index == kWxLpResidualIndex) {
        return strategy_wx_lp_q16_residual::audit_range(profile, cube_steps);
    }
    if (index == kWxLpBalancedIndex) {
        return strategy_wx_lp_q16_balanced::audit_range(profile, cube_steps);
    }
    return Q16RangeStats{};
}

inline const char* q16_range_evidence_for(int index) {
    if (index == kWxOverdriveFixedIndex) {
        return "observed cube17 boosted policy; hull projection still pending";
    }
    if (index == kFixedQ16Index) {
        return "observed cube17; overflow proof and hull projection still pending";
    }
    if (index == kHybridQ16Index) {
        return "observed cube17 with seeded route; seed fallback count measured";
    }
    if (index == kRgbwwStrictFixedIndex) {
        return "observed cube17 RGBWW combo solve; physical policy still pending";
    }
    if (index == kWxLpResidualIndex) {
        return "observed cube17 residual LP row; row remains rejected";
    }
    if (index == kWxLpBalancedIndex) {
        return "observed cube17 balanced LP search; hull projection still pending";
    }
    return "n/a";
}

inline void track_bound(double value, double* max_value) {
    if (value < 0.0) {
        value = -value;
    }
    if (value > *max_value) {
        *max_value = value;
    }
}

inline void track_bound_coeff(i64 value, Q16StaticBoundStats& stats) {
    strategy_common::track_abs(value, &stats.max_abs_coeff_q);
}

inline void finalize_static_bound(Q16StaticBoundStats& stats) {
    stats.max_bound = stats.max_matvec_product;
    track_bound(stats.max_matvec_accumulator, &stats.max_bound);
    track_bound(stats.max_ls2_scaled_numerator, &stats.max_bound);
}

inline void bound_source_from_matrix(const i32 M[3][3],
                                     const double in_bound[3],
                                     double out_bound[3],
                                     Q16StaticBoundStats& stats) {
    for (int row = 0; row < 3; ++row) {
        double acc = 0.0;
        for (int col = 0; col < 3; ++col) {
            track_bound_coeff(M[row][col], stats);
            const double product =
                fl::fabs(static_cast<double>(M[row][col])) * in_bound[col];
            track_bound(product, &stats.max_matvec_product);
            acc += product;
            track_bound(acc, &stats.max_matvec_accumulator);
        }
        out_bound[row] = acc / static_cast<double>(strategy_fixed_analytical_q16::kQ);
        track_bound(out_bound[row], &stats.max_matvec_output_q);
        track_bound(out_bound[row], &stats.max_source_x_q);
    }
}

inline void bound_source_from_columns(const i32 P[][3],
                                      int column_count,
                                      double out_bound[3],
                                      Q16StaticBoundStats& stats) {
    for (int row = 0; row < 3; ++row) {
        double acc = 0.0;
        for (int ch = 0; ch < column_count; ++ch) {
            track_bound_coeff(P[ch][row], stats);
            const double product =
                fl::fabs(static_cast<double>(P[ch][row]))
                * static_cast<double>(strategy_fixed_analytical_q16::kQ);
            track_bound(product, &stats.max_matvec_product);
            acc += product;
            track_bound(acc, &stats.max_matvec_accumulator);
        }
        out_bound[row] = acc / static_cast<double>(strategy_fixed_analytical_q16::kQ);
        track_bound(out_bound[row], &stats.max_matvec_output_q);
        track_bound(out_bound[row], &stats.max_source_x_q);
    }
}

inline void bound_endpoint_matvec(const i32 M[3][3],
                                  const double in_bound[3],
                                  Q16StaticBoundStats& stats) {
    double out_bound[3] = {};
    bound_source_from_matrix(M, in_bound, out_bound, stats);
}

inline void bound_ls2_pair(const i32 A[3],
                           const i32 B[3],
                           const double x_bound[3],
                           Q16StaticBoundStats& stats) {
    double aa = 0.0;
    double ab = 0.0;
    double bb = 0.0;
    double ax = 0.0;
    double bx = 0.0;
    const double q =
        static_cast<double>(strategy_fixed_analytical_q16::kQ);
    for (int i = 0; i < 3; ++i) {
        track_bound_coeff(A[i], stats);
        track_bound_coeff(B[i], stats);
        aa += (fl::fabs(static_cast<double>(A[i]))
             * fl::fabs(static_cast<double>(A[i]))) / q;
        ab += (fl::fabs(static_cast<double>(A[i]))
             * fl::fabs(static_cast<double>(B[i]))) / q;
        bb += (fl::fabs(static_cast<double>(B[i]))
             * fl::fabs(static_cast<double>(B[i]))) / q;
        ax += (fl::fabs(static_cast<double>(A[i])) * x_bound[i]) / q;
        bx += (fl::fabs(static_cast<double>(B[i])) * x_bound[i]) / q;
    }
    const double num0 = (ax * bb + bx * ab) * q;
    const double num1 = (bx * aa + ax * ab) * q;
    track_bound(num0, &stats.max_ls2_scaled_numerator);
    track_bound(num1, &stats.max_ls2_scaled_numerator);
}

inline void bound_rgb_ls2_pairs(const i32 P[][3],
                                const double x_bound[3],
                                Q16StaticBoundStats& stats) {
    bound_ls2_pair(P[0], P[1], x_bound, stats);
    bound_ls2_pair(P[0], P[2], x_bound, stats);
    bound_ls2_pair(P[1], P[2], x_bound, stats);
}

inline Q16StaticBoundStats evaluate_wx_q16_static_bounds(
    const DiodeProfile& profile, const char* evidence, bool balanced_lp) {
    strategy_wx_overdrive_q16::setup(profile);
    Q16StaticBoundStats stats;
    stats.measured = true;
    stats.evidence = evidence;
    const auto& state = strategy_wx_overdrive_q16::g_state;
    const double src_bound[3] = {
        static_cast<double>(strategy_wx_overdrive_q16::kQ),
        static_cast<double>(strategy_wx_overdrive_q16::kQ),
        static_cast<double>(strategy_wx_overdrive_q16::kQ),
    };
    double x_bound[3] = {};
    if (state.has_source_space) {
        bound_source_from_matrix(state.M_src, src_bound, x_bound, stats);
    } else {
        bound_source_from_columns(state.P, 3, x_bound, stats);
    }
    bound_endpoint_matvec(state.P_RGB_inv, x_bound, stats);
    bound_rgb_ls2_pairs(state.P, x_bound, stats);
    for (int row = 0; row < 3; ++row) {
        track_bound_coeff(state.d_W[row], stats);
        const double product =
            fl::fabs(static_cast<double>(state.d_W[row]))
            * static_cast<double>(strategy_wx_overdrive_q16::kQ);
        track_bound(product, &stats.max_matvec_product);
        track_bound(product, &stats.max_matvec_accumulator);
    }
    if (balanced_lp) {
        for (int ch = 0; ch < 4; ++ch) {
            const double s =
                fl::fabs(static_cast<double>(state.P[ch][0]))
              + fl::fabs(static_cast<double>(state.P[ch][1]))
              + fl::fabs(static_cast<double>(state.P[ch][2]));
            const double xy_product =
                static_cast<double>(strategy_wx_overdrive_q16::kQ) * s;
            track_bound(xy_product, &stats.max_matvec_product);
            track_bound(xy_product, &stats.max_matvec_accumulator);
        }
    }
    finalize_static_bound(stats);
    strategy_wx_overdrive_q16::teardown();
    return stats;
}

inline Q16StaticBoundStats evaluate_fixed_q16_static_bounds(
    const DiodeProfile& profile, const char* evidence) {
    strategy_fixed_analytical_q16::setup(profile);
    Q16StaticBoundStats stats;
    stats.measured = true;
    stats.evidence = evidence;
    const auto& state = strategy_fixed_analytical_q16::g_state;
    const double src_bound[3] = {
        static_cast<double>(strategy_fixed_analytical_q16::kQ),
        static_cast<double>(strategy_fixed_analytical_q16::kQ),
        static_cast<double>(strategy_fixed_analytical_q16::kQ),
    };
    double x_bound[3] = {};
    if (state.has_source_space) {
        bound_source_from_matrix(state.M_src, src_bound, x_bound, stats);
    } else {
        bound_source_from_columns(state.P, 3, x_bound, stats);
    }
    for (int inv_idx = 0; inv_idx < 3; ++inv_idx) {
        bound_endpoint_matvec(state.inv[inv_idx], x_bound, stats);
    }
    bound_rgb_ls2_pairs(state.P, x_bound, stats);
    finalize_static_bound(stats);
    strategy_fixed_analytical_q16::teardown();
    return stats;
}

inline Q16StaticBoundStats evaluate_rgbww_q16_static_bounds(
    const DiodeProfile& profile) {
    strategy_rgbww_strict_5ch_fixed_eval::setup(profile);
    Q16StaticBoundStats stats;
    stats.measured = true;
    stats.evidence = "coefficient bounds for RGBWW fixed combo solve";
    const auto& state = strategy_rgbww_strict_5ch_fixed_eval::g_state;
    const double src_bound[3] = {
        static_cast<double>(strategy_rgbww_strict_5ch_fixed_eval::kQ),
        static_cast<double>(strategy_rgbww_strict_5ch_fixed_eval::kQ),
        static_cast<double>(strategy_rgbww_strict_5ch_fixed_eval::kQ),
    };
    double x_bound[3] = {};
    if (state.has_source_space) {
        bound_source_from_matrix(state.M_src, src_bound, x_bound, stats);
    } else {
        bound_source_from_columns(state.P, 3, x_bound, stats);
    }
    for (int combo_idx = 0;
         combo_idx < strategy_rgbww_strict_5ch_fixed_eval::kMaxCombos;
         ++combo_idx) {
        if (state.combos[combo_idx].ok) {
            bound_endpoint_matvec(state.combos[combo_idx].inv, x_bound, stats);
        }
    }
    bound_rgb_ls2_pairs(state.P, x_bound, stats);
    finalize_static_bound(stats);
    strategy_rgbww_strict_5ch_fixed_eval::teardown();
    return stats;
}

inline Q16StaticBoundStats evaluate_q16_static_bounds(int index,
                                                      const DiodeProfile& profile) {
    if (index == kWxOverdriveFixedIndex) {
        return evaluate_wx_q16_static_bounds(
            profile, "coefficient bounds for boosted Q16 residual solve", false);
    }
    if (index == kWxLpResidualIndex) {
        return evaluate_wx_q16_static_bounds(
            profile, "coefficient bounds for rejected residual LP row", false);
    }
    if (index == kWxLpBalancedIndex) {
        return evaluate_wx_q16_static_bounds(
            profile, "coefficient bounds plus balanced xy products", true);
    }
    if (index == kFixedQ16Index) {
        return evaluate_fixed_q16_static_bounds(
            profile, "coefficient bounds for fixed analytical solve");
    }
    if (index == kHybridQ16Index) {
        return evaluate_fixed_q16_static_bounds(
            profile, "same fixed analytical bounds; seed router changes route order");
    }
    if (index == kRgbwwStrictFixedIndex) {
        return evaluate_rgbww_q16_static_bounds(profile);
    }
    return Q16StaticBoundStats{};
}

inline void i64_value_string(i64 value, char out[32]) {
    fl::snprintf(out, 32, "%lld", static_cast<long long>(value));
}

inline void double_value_string(double value, char out[40]) {
    if (value <= 0.0) {
        fl::snprintf(out, 40, "%s", "n/a");
        return;
    }
    const double u64_limit =
        static_cast<double>((fl::numeric_limits<unsigned long long>::max)());
    if (value > u64_limit - 1.0) {
        fl::snprintf(out, 40, "%s", ">u64");
        return;
    }
    const unsigned long long rounded =
        static_cast<unsigned long long>(value + 0.5);
    fl::snprintf(out, 40, "%llu", rounded);
}

inline void double_headroom_string(double value, char out[32]) {
    if (value <= 0.0) {
        fl::snprintf(out, 32, "%s", "n/a");
        return;
    }
    const double limit =
        static_cast<double>((fl::numeric_limits<i64>::max)());
    if (value > limit) {
        fl::snprintf(out, 32, "%s", "<1x");
        return;
    }
    const unsigned long long headroom =
        static_cast<unsigned long long>(limit / value);
    fl::snprintf(out, 32, "%llux", headroom);
}

inline void i64_headroom_string(i64 value, char out[32]) {
    if (value <= 0) {
        fl::snprintf(out, 32, "%s", "n/a");
        return;
    }
    const i64 limit = (fl::numeric_limits<i64>::max)();
    const i64 headroom = limit / value;
    fl::snprintf(out, 32, "%lldx", static_cast<long long>(headroom));
}

inline void bench_string(const BenchStats& bench, char out[24]) {
    if (!bench.measured) {
        fl::snprintf(out, 24, "%10s", "n/a");
        return;
    }
    fl::snprintf(out, 24, "%9.1f", bench.host_ns_per_pixel);
}

inline void lsb_stats_string(const ErrorStats& stats, char out[48]) {
    if (stats.sample_count == 0) {
        fl::snprintf(out, 48, "%29s", "n/a");
        return;
    }
    fl::snprintf(out, 48, "%4ld/%4ld/%4ld/%4ld",
             static_cast<long>(stats.p50_lsb),
             static_cast<long>(stats.p95_lsb),
             static_cast<long>(stats.p99_lsb),
             static_cast<long>(stats.peak_lsb));
}

inline void mean_lsb_string(const ErrorStats& stats, char out[16]) {
    if (stats.sample_count == 0) {
        fl::snprintf(out, 16, "%s", "n/a");
        return;
    }
    fl::snprintf(out, 16, "%.2f", stats.mean_lsb);
}

inline void target_de_string(const ErrorStats& stats, char out[64]) {
    if (stats.sample_count == 0) {
        fl::snprintf(out, 64, "%s", "n/a");
        return;
    }
    fl::snprintf(out, 64, "%.2f/%.2f/%.2f/%.2f",
             static_cast<double>(stats.p50_dE),
             static_cast<double>(stats.p95_dE),
             static_cast<double>(stats.peak_dE),
             stats.mean_dE);
}

inline int paired_accuracy_index_for(int index) {
    switch (index) {
    case kWxLpIndex:
        return kWxLpBalancedIndex;
    case kWxLpResidualIndex:
        return kWxLpIndex;
    case kWxLpBalancedIndex:
        return kWxLpIndex;
    case kWxOverdriveIndex:
        return kWxOverdriveFixedIndex;
    case kWxOverdriveFixedIndex:
        return kWxOverdriveIndex;
    case kBilinearN16Index:
        return kBilinearFixedIndex;
    case kBilinearFixedIndex:
        return kBilinearN16Index;
    case kHermiteN16Index:
        return kHermiteN16FixedIndex;
    case kHermiteN16FixedIndex:
        return kHermiteN16Index;
    case kHermiteN32Index:
        return kHermiteN32FixedIndex;
    case kHermiteN32FixedIndex:
        return kHermiteN32Index;
    case kCellEncodingIndex:
        return kCellEncodingFixedIndex;
    case kCellEncodingFixedIndex:
        return kCellEncodingIndex;
    case kHullAdaptiveIndex:
        return kHullAdaptiveFixedIndex;
    case kHullAdaptiveFixedIndex:
        return kHullAdaptiveIndex;
    case kTetrahedralIndex:
        return kTetrahedralFixedIndex;
    case kTetrahedralFixedIndex:
        return kTetrahedralIndex;
    case kTetrahedralU8Index:
        return kTetrahedralU8FixedIndex;
    case kTetrahedralU8FixedIndex:
        return kTetrahedralU8Index;
    case kTetrahedralBoostIndex:
        return kTetrahedralBoostFixedIndex;
    case kTetrahedralBoostFixedIndex:
        return kTetrahedralBoostIndex;
    case kTetrahedralBoostU8Index:
        return kTetrahedralBoostU8FixedIndex;
    case kTetrahedralBoostU8FixedIndex:
        return kTetrahedralBoostU8Index;
    case kPolyPiecewiseIndex:
        return kPolyPiecewiseFixedIndex;
    case kPolyPiecewiseFixedIndex:
        return kPolyPiecewiseIndex;
    case kFixedQ16Index:
        return kClosedFormIndex;
    case kHybridQ16Index:
        return kFixedQ16Index;
    case kRgbwwStrictIndex:
        return kRgbwwStrictFixedIndex;
    case kRgbwwStrictFixedIndex:
        return kRgbwwStrictIndex;
    default:
        return -1;
    }
}

inline bool cpu_pair_covers_indices(int a, int b) {
    if (a < 0 || b < 0) return false;
    for (int i = 0; i < kCpuPairCount; ++i) {
        const CpuPair& pair = kCpuPairs[i];
        if ((pair.fpu_index == a && pair.fixed_index == b)
            || (pair.fpu_index == b && pair.fixed_index == a)) {
            return true;
        }
    }
    return false;
}

inline int policy_accuracy_index_for(int index) {
    switch (index) {
    case kWxOverdriveFixedIndex:
    case kTetrahedralBoostIndex:
    case kTetrahedralBoostFixedIndex:
    case kTetrahedralBoostU8Index:
    case kTetrahedralBoostU8FixedIndex:
        return kWxOverdriveIndex;
    case kWxLpResidualIndex:
    case kWxLpBalancedIndex:
        return kWxLpIndex;
    default:
        return -1;
    }
}

inline void host_fixed_fpu_ratio_string(int index,
                                        const BenchStats benches[kStrategyCount],
                                        char out[64]) {
    if (!benches[index].measured || !benches[kClosedFormIndex].measured) {
        fl::snprintf(out, 64, "n/a");
        return;
    }
    if (index == kBilinearFixedIndex && benches[kBilinearN16Index].measured) {
        const double ratio = benches[index].host_ns_per_pixel /
                             benches[kBilinearN16Index].host_ns_per_pixel;
        fl::snprintf(out, 64, "%.2fx vs bilinear_n16 host", ratio);
        return;
    }
    if (index == kBilinearN16Index && benches[kBilinearFixedIndex].measured) {
        const double ratio = benches[kBilinearFixedIndex].host_ns_per_pixel /
                             benches[kBilinearN16Index].host_ns_per_pixel;
        fl::snprintf(out, 64, "fixed paired row %.2fx host", ratio);
        return;
    }
    if (index == kWxOverdriveFixedIndex && benches[kWxOverdriveIndex].measured) {
        const double ratio = benches[index].host_ns_per_pixel /
                             benches[kWxOverdriveIndex].host_ns_per_pixel;
        fl::snprintf(out, 64, "%.2fx vs wx_overdrive_float host", ratio);
        return;
    }
    if (index == kWxOverdriveIndex && benches[kWxOverdriveFixedIndex].measured) {
        const double ratio = benches[kWxOverdriveFixedIndex].host_ns_per_pixel /
                             benches[kWxOverdriveIndex].host_ns_per_pixel;
        fl::snprintf(out, 64, "fixed paired row %.2fx host", ratio);
        return;
    }
    if (index == kWxLpResidualIndex && benches[kWxLpIndex].measured) {
        const double ratio = benches[index].host_ns_per_pixel /
                             benches[kWxLpIndex].host_ns_per_pixel;
        fl::snprintf(out, 64, "%.2fx vs wx_lp_legacy_float host", ratio);
        return;
    }
    if (index == kWxLpBalancedIndex && benches[kWxLpIndex].measured) {
        const double ratio = benches[index].host_ns_per_pixel /
                             benches[kWxLpIndex].host_ns_per_pixel;
        fl::snprintf(out, 64, "%.2fx vs wx_lp_legacy_float host", ratio);
        return;
    }
    if (index == kWxLpIndex && benches[kWxLpBalancedIndex].measured) {
        const double ratio = benches[kWxLpBalancedIndex].host_ns_per_pixel /
                             benches[kWxLpIndex].host_ns_per_pixel;
        fl::snprintf(out, 64, "fixed balanced row %.2fx host", ratio);
        return;
    }
    if (index == kWxLpIndex && benches[kWxLpResidualIndex].measured) {
        const double ratio = benches[kWxLpResidualIndex].host_ns_per_pixel /
                             benches[kWxLpIndex].host_ns_per_pixel;
        fl::snprintf(out, 64, "rejected fixed row %.2fx host", ratio);
        return;
    }
    if (index == kCellEncodingFixedIndex && benches[kCellEncodingIndex].measured) {
        const double ratio = benches[index].host_ns_per_pixel /
                             benches[kCellEncodingIndex].host_ns_per_pixel;
        fl::snprintf(out, 64, "%.2fx vs cell_encoding_q16_n16 host", ratio);
        return;
    }
    if (index == kCellEncodingIndex && benches[kCellEncodingFixedIndex].measured) {
        const double ratio = benches[kCellEncodingFixedIndex].host_ns_per_pixel /
                             benches[kCellEncodingIndex].host_ns_per_pixel;
        fl::snprintf(out, 64, "fixed paired row %.2fx host", ratio);
        return;
    }
    if (index == kHermiteN16FixedIndex && benches[kHermiteN16Index].measured) {
        const double ratio = benches[index].host_ns_per_pixel /
                             benches[kHermiteN16Index].host_ns_per_pixel;
        fl::snprintf(out, 64, "%.2fx vs hermite_n16 host", ratio);
        return;
    }
    if (index == kHermiteN16Index && benches[kHermiteN16FixedIndex].measured) {
        const double ratio = benches[kHermiteN16FixedIndex].host_ns_per_pixel /
                             benches[kHermiteN16Index].host_ns_per_pixel;
        fl::snprintf(out, 64, "fixed paired row %.2fx host", ratio);
        return;
    }
    if (index == kHermiteN32FixedIndex && benches[kHermiteN32Index].measured) {
        const double ratio = benches[index].host_ns_per_pixel /
                             benches[kHermiteN32Index].host_ns_per_pixel;
        fl::snprintf(out, 64, "%.2fx vs hermite_n32 host", ratio);
        return;
    }
    if (index == kHermiteN32Index && benches[kHermiteN32FixedIndex].measured) {
        const double ratio = benches[kHermiteN32FixedIndex].host_ns_per_pixel /
                             benches[kHermiteN32Index].host_ns_per_pixel;
        fl::snprintf(out, 64, "fixed paired row %.2fx host", ratio);
        return;
    }
    if (index == kHullAdaptiveFixedIndex && benches[kHullAdaptiveIndex].measured) {
        const double ratio = benches[index].host_ns_per_pixel /
                             benches[kHullAdaptiveIndex].host_ns_per_pixel;
        fl::snprintf(out, 64, "%.2fx vs hull_adaptive_n16 host", ratio);
        return;
    }
    if (index == kHullAdaptiveIndex && benches[kHullAdaptiveFixedIndex].measured) {
        const double ratio = benches[kHullAdaptiveFixedIndex].host_ns_per_pixel /
                             benches[kHullAdaptiveIndex].host_ns_per_pixel;
        fl::snprintf(out, 64, "fixed paired row %.2fx host", ratio);
        return;
    }
    if (index == kPolyPiecewiseFixedIndex && benches[kPolyPiecewiseIndex].measured) {
        const double ratio = benches[index].host_ns_per_pixel /
                             benches[kPolyPiecewiseIndex].host_ns_per_pixel;
        fl::snprintf(out, 64, "%.2fx vs poly_piecewise_d3 host", ratio);
        return;
    }
    if (index == kPolyPiecewiseIndex && benches[kPolyPiecewiseFixedIndex].measured) {
        const double ratio = benches[kPolyPiecewiseFixedIndex].host_ns_per_pixel /
                             benches[kPolyPiecewiseIndex].host_ns_per_pixel;
        fl::snprintf(out, 64, "fixed paired row %.2fx host", ratio);
        return;
    }
    if (index == kTetrahedralFixedIndex && benches[kTetrahedralIndex].measured) {
        const double ratio = benches[index].host_ns_per_pixel /
                             benches[kTetrahedralIndex].host_ns_per_pixel;
        fl::snprintf(out, 64, "%.2fx vs tetrahedral_rgb8 host", ratio);
        return;
    }
    if (index == kTetrahedralIndex && benches[kTetrahedralFixedIndex].measured) {
        const double ratio = benches[kTetrahedralFixedIndex].host_ns_per_pixel /
                             benches[kTetrahedralIndex].host_ns_per_pixel;
        fl::snprintf(out, 64, "fixed paired row %.2fx host", ratio);
        return;
    }
    if (index == kTetrahedralU8FixedIndex && benches[kTetrahedralU8Index].measured) {
        const double ratio = benches[index].host_ns_per_pixel /
                             benches[kTetrahedralU8Index].host_ns_per_pixel;
        fl::snprintf(out, 64, "%.2fx vs tetrahedral_rgb8_u8 host", ratio);
        return;
    }
    if (index == kTetrahedralU8Index && benches[kTetrahedralU8FixedIndex].measured) {
        const double ratio = benches[kTetrahedralU8FixedIndex].host_ns_per_pixel /
                             benches[kTetrahedralU8Index].host_ns_per_pixel;
        fl::snprintf(out, 64, "fixed paired row %.2fx host", ratio);
        return;
    }
    if (index == kTetrahedralBoostFixedIndex
        && benches[kTetrahedralBoostIndex].measured) {
        const double ratio = benches[index].host_ns_per_pixel /
                             benches[kTetrahedralBoostIndex].host_ns_per_pixel;
        fl::snprintf(out, 64, "%.2fx vs tetrahedral_rgb8_boosted host", ratio);
        return;
    }
    if (index == kTetrahedralBoostIndex
        && benches[kTetrahedralBoostFixedIndex].measured) {
        const double ratio = benches[kTetrahedralBoostFixedIndex].host_ns_per_pixel /
                             benches[kTetrahedralBoostIndex].host_ns_per_pixel;
        fl::snprintf(out, 64, "fixed paired row %.2fx host", ratio);
        return;
    }
    if (index == kTetrahedralBoostU8FixedIndex
        && benches[kTetrahedralBoostU8Index].measured) {
        const double ratio = benches[index].host_ns_per_pixel /
                             benches[kTetrahedralBoostU8Index].host_ns_per_pixel;
        fl::snprintf(out, 64, "%.2fx vs tetrahedral_rgb8_boosted_u8 host", ratio);
        return;
    }
    if (index == kTetrahedralBoostU8Index
        && benches[kTetrahedralBoostU8FixedIndex].measured) {
        const double ratio = benches[kTetrahedralBoostU8FixedIndex].host_ns_per_pixel /
                             benches[kTetrahedralBoostU8Index].host_ns_per_pixel;
        fl::snprintf(out, 64, "fixed paired row %.2fx host", ratio);
        return;
    }
    if (index == kFixedQ16Index) {
        const double ratio = benches[index].host_ns_per_pixel /
                             benches[kClosedFormIndex].host_ns_per_pixel;
        fl::snprintf(out, 64, "%.2fx vs float_closed_form host", ratio);
        return;
    }
    if (index == kHybridQ16Index && benches[kFixedQ16Index].measured) {
        const double ratio = benches[index].host_ns_per_pixel /
                             benches[kFixedQ16Index].host_ns_per_pixel;
        fl::snprintf(out, 64, "%.2fx vs fixed_analytical_q16 host", ratio);
        return;
    }
    if (index == kClosedFormIndex && benches[kFixedQ16Index].measured) {
        const double ratio = benches[kFixedQ16Index].host_ns_per_pixel /
                             benches[kClosedFormIndex].host_ns_per_pixel;
        fl::snprintf(out, 64, "Q16 paired row %.2fx host", ratio);
        return;
    }
    if (index == kRgbwwStrictFixedIndex && benches[kRgbwwStrictIndex].measured) {
        const double ratio = benches[index].host_ns_per_pixel /
                             benches[kRgbwwStrictIndex].host_ns_per_pixel;
        fl::snprintf(out, 64, "%.2fx vs rgbww_strict_5ch_proto host", ratio);
        return;
    }
    if (index == kRgbwwStrictIndex && benches[kRgbwwStrictFixedIndex].measured) {
        const double ratio = benches[kRgbwwStrictFixedIndex].host_ns_per_pixel /
                             benches[kRgbwwStrictIndex].host_ns_per_pixel;
        fl::snprintf(out, 64, "fixed paired row %.2fx host", ratio);
        return;
    }
    fl::snprintf(out, 64, "no paired fixed/FPU row");
}

inline void strategy_mcu_fixed_fpu_ratio_string(int index, char out[64]);

inline void decision_cpu_string(int index,
                                const BenchStats benches[kStrategyCount],
                                char out[192]) {
    char mcu_ratio[64];
    strategy_mcu_fixed_fpu_ratio_string(index, mcu_ratio);
    if (!benches[index].measured) {
        fl::snprintf(out, 192, "host n/a; FPU %s; fixed %s; ratio %s",
                 fpu_cycles_for(index),
                 fixed_cycles_for(index),
                 mcu_ratio);
        return;
    }
    fl::snprintf(out, 192, "%.1f host; FPU %s; fixed %s; ratio %s",
             benches[index].host_ns_per_pixel,
             fpu_cycles_for(index),
             fixed_cycles_for(index),
             mcu_ratio);
}

inline bool cycle_cell_has_explicit_evidence_or_reason(const char* text) {
    return cycle_cell_has_measurement(text)
        || text_contains(text, "missing")
        || text_contains(text, "rejected")
        || text_contains(text, "n/a");
}

inline bool parse_cycle_cell_value(const char* text, double* out) {
    if (!has_text(text)) return false;
    char* end = nullptr;
    const double value = fl::strtod(text, &end);
    if (end == text || value <= 0.0) return false;
    *out = value;
    return true;
}

inline void mcu_fixed_fpu_ratio_string(const char* fpu_cycles,
                                       const char* fixed_cycles,
                                       char out[64]) {
    double fpu_value = 0.0;
    double fixed_value = 0.0;
    if (parse_cycle_cell_value(fpu_cycles, &fpu_value)
        && parse_cycle_cell_value(fixed_cycles, &fixed_value)) {
        fl::snprintf(out, 64, "%.2fx MCU", fixed_value / fpu_value);
        return;
    }
    if (text_contains(fpu_cycles, "rejected")
        || text_contains(fixed_cycles, "rejected")) {
        fl::snprintf(out, 64, "%s", "n/a rejected");
        return;
    }
    if (text_contains(fpu_cycles, "missing")
        && text_contains(fixed_cycles, "missing")) {
        fl::snprintf(out, 64, "%s", "missing FPU+fixed MCU");
        return;
    }
    if (text_contains(fpu_cycles, "missing")) {
        fl::snprintf(out, 64, "%s", "missing FPU MCU");
        return;
    }
    if (text_contains(fixed_cycles, "missing")) {
        fl::snprintf(out, 64, "%s", "missing fixed MCU");
        return;
    }
    fl::snprintf(out, 64, "%s", "missing MCU pair");
}

inline void strategy_mcu_fixed_fpu_ratio_string(int index, char out[64]) {
    mcu_fixed_fpu_ratio_string(
        fpu_cycles_for(index), fixed_cycles_for(index), out);
}

inline void pair_mcu_fixed_fpu_ratio_string(const CpuPair& pair, char out[64]) {
    mcu_fixed_fpu_ratio_string(
        fpu_cycles_for(pair.fpu_index),
        fixed_cycles_for(pair.fixed_index),
        out);
}

inline bool mcu_ratio_cell_has_measurement(const char* text) {
    return text_contains(text, "x MCU")
        && !text_contains(text, "missing")
        && !text_contains(text, "rejected")
        && !text_contains(text, "n/a");
}

enum StrategyBlockerBits : u32 {
    kBlockerNone = 0,
    kBlockerMcuCycles = 1u << 0,
    kBlockerProductionIntegration = 1u << 1,
    kBlockerPackedStorage = 1u << 2,
    kBlockerAccuracyBudget = 1u << 3,
    kBlockerFormalProofProjection = 1u << 4,
    kBlockerConstrainedFit = 1u << 5,
    kBlockerHardwarePolicy = 1u << 6,
    kBlockerFixedPathDecision = 1u << 7,
    kBlockerReferenceSurface = 1u << 8,
};

inline void append_blocker_token(char out[192], const char* token) {
    size_t used = 0;
    while (used < 191 && out[used] != '\0') {
        ++used;
    }
    if (used >= 191) return;
    const char* prefix = used > 0 ? "; " : "";
    fl::snprintf(out + used, 192 - used, "%s%s", prefix, token);
}

inline u32 strategy_blocker_mask(
    int index,
    const MemoryInfo& memory,
    const ErrorStats& cf_stats,
    const ErrorStats& target_stats) {
    u32 mask = kBlockerNone;
    const Strategy& strategy = kStrategies[index];
    const bool rejected = text_equals(strategy.stage, "REJECTED");
    const bool reference = text_equals(strategy.stage, "REF");
    const char* accuracy_status = status_for(
        strategy,
        cf_stats,
        target_stats,
        kBudgets[index][0],
        kBudgets[index][1]);

    if (!rejected && !kReferenceSurfaceLocked) {
        mask |= kBlockerReferenceSurface;
    }
    if (!rejected && !has_complete_mcu_cycle_evidence(index)) {
        mask |= kBlockerMcuCycles;
    }
    if (text_equals(strategy.stage, "PROTO")) {
        mask |= kBlockerProductionIntegration;
    }
    if (!rejected && memory.target_packed_bytes != 0
        && memory.prototype_storage_bytes > memory.target_packed_bytes) {
        mask |= kBlockerPackedStorage;
    }
    if (text_contains(accuracy_status, "FAIL")) {
        mask |= kBlockerAccuracyBudget;
    }

    switch (index) {
    case kWxOverdriveFixedIndex:
    case kFixedQ16Index:
    case kHybridQ16Index:
    case kWxLpBalancedIndex:
        mask |= kBlockerFormalProofProjection;
        break;
    case kCellEncodingIndex:
    case kCellEncodingFixedIndex:
    case kHullAdaptiveIndex:
    case kHullAdaptiveFixedIndex:
        mask |= kBlockerPackedStorage;
        break;
    case kPolyPiecewiseIndex:
    case kPolyPiecewiseFixedIndex:
        mask |= kBlockerConstrainedFit;
        break;
    case kRgbwwStrictIndex:
    case kRgbwwStrictFixedIndex:
        mask |= kBlockerHardwarePolicy;
        break;
    case kRgbwwLayeredIndex:
        mask |= kBlockerFixedPathDecision;
        break;
    default:
        break;
    }

    if (reference) {
        mask &= ~kBlockerProductionIntegration;
        mask &= ~kBlockerPackedStorage;
        mask &= ~kBlockerAccuracyBudget;
    }
    if (rejected) {
        mask = kBlockerNone;
    }
    return mask;
}

inline void strategy_blocker_string(u32 mask, char out[192]) {
    out[0] = '\0';
    if (mask == kBlockerNone) {
        fl::snprintf(out, 192, "%s", "none");
        return;
    }
    if ((mask & kBlockerReferenceSurface) != 0) {
        append_blocker_token(out, "reference surface");
    }
    if ((mask & kBlockerMcuCycles) != 0) {
        append_blocker_token(out, "MCU cycles");
    }
    if ((mask & kBlockerProductionIntegration) != 0) {
        append_blocker_token(out, "production integration");
    }
    if ((mask & kBlockerPackedStorage) != 0) {
        append_blocker_token(out, "packed storage");
    }
    if ((mask & kBlockerAccuracyBudget) != 0) {
        append_blocker_token(out, "accuracy budget");
    }
    if ((mask & kBlockerFormalProofProjection) != 0) {
        append_blocker_token(out, "proof/projection");
    }
    if ((mask & kBlockerConstrainedFit) != 0) {
        append_blocker_token(out, "constrained fit");
    }
    if ((mask & kBlockerHardwarePolicy) != 0) {
        append_blocker_token(out, "hardware policy");
    }
    if ((mask & kBlockerFixedPathDecision) != 0) {
        append_blocker_token(out, "fixed-path decision");
    }
}

inline void strategy_blocker_close_action(u32 mask, char out[192]) {
    if (mask == kBlockerNone) {
        fl::snprintf(out, 192, "%s", "no open blocker for this row state");
        return;
    }
    if ((mask & kBlockerReferenceSurface) != 0) {
        fl::snprintf(out, 192, "%s", "lock canonical reference and FPU LSB tolerance");
        return;
    }
    if ((mask & kBlockerMcuCycles) != 0) {
        fl::snprintf(out, 192, "%s", "measure named MCU/simulator FPU+fixed cycles");
        return;
    }
    if ((mask & kBlockerProductionIntegration) != 0) {
        fl::snprintf(out, 192, "%s", "land selected implementation outside harness");
        return;
    }
    if ((mask & kBlockerPackedStorage) != 0) {
        fl::snprintf(out, 192, "%s", "replace prototype layout with packed production format");
        return;
    }
    if ((mask & kBlockerAccuracyBudget) != 0) {
        fl::snprintf(out, 192, "%s", "improve accuracy or keep as failing/rejected evidence");
        return;
    }
    if ((mask & kBlockerFormalProofProjection) != 0) {
        fl::snprintf(out, 192, "%s", "finish hull projection and overflow proof");
        return;
    }
    if ((mask & kBlockerConstrainedFit) != 0) {
        fl::snprintf(out, 192, "%s", "generate constrained monotonic coefficients");
        return;
    }
    if ((mask & kBlockerHardwarePolicy) != 0) {
        fl::snprintf(out, 192, "%s", "validate RGBWW policy against hardware/LED bins");
        return;
    }
    fl::snprintf(out, 192, "%s", "resolve fixed-path production decision");
}

inline bool prototype_row_has_production_plan(int index, u32 mask) {
    return text_equals(kStrategies[index].stage, "PROTO")
        && mask != kBlockerNone
        && has_text(kStrategies[index].missing)
        && !has_placeholder_wording(kStrategies[index].missing);
}

inline bool rejected_row_has_evidence(int index) {
    return text_equals(kStrategies[index].stage, "REJECTED")
        && (text_contains(kStrategies[index].missing, "rejected")
            || text_contains(kStrategies[index].fixed_path, "rejected")
            || text_contains(kStrategies[index].cpu_validation, "rejected"));
}

inline void prototype_disposition_string(int index, u32 mask, char out[128]) {
    if (text_equals(kStrategies[index].stage, "PROTO")) {
        fl::snprintf(out, 128, "%s",
                 prototype_row_has_production_plan(index, mask)
                     ? "production plan recorded"
                     : "production plan missing");
        return;
    }
    if (text_equals(kStrategies[index].stage, "REJECTED")) {
        fl::snprintf(out, 128, "%s",
                 rejected_row_has_evidence(index)
                     ? "rejection evidence recorded"
                     : "rejection evidence missing");
        return;
    }
    if (text_equals(kStrategies[index].stage, "PROD")) {
        fl::snprintf(out, 128, "%s", "production implementation exists");
        return;
    }
    fl::snprintf(out, 128, "%s", "reference row; not prototype");
}

inline void print_cpu_pair_line(const CpuPair& pair,
                                const MemoryInfo memories[kStrategyCount],
                                const BenchStats benches[kStrategyCount],
                                const ErrorStats pair_stats[kStrategyCount]) {
    char fpu_host[24];
    char fixed_host[24];
    char fpu_mem[24];
    char fixed_mem[24];
    char mem_delta[32];
    char mem_ratio[32];
    char lsb[48];
    char mean_lsb[16];
    char mcu_ratio[64];
    bench_string(benches[pair.fpu_index], fpu_host);
    bench_string(benches[pair.fixed_index], fixed_host);
    memory_string(memories[pair.fpu_index].target_packed_bytes, fpu_mem);
    memory_string(memories[pair.fixed_index].target_packed_bytes, fixed_mem);
    signed_memory_delta_string(
        memories[pair.fixed_index].target_packed_bytes,
        memories[pair.fpu_index].target_packed_bytes,
        mem_delta);
    memory_ratio_string(
        memories[pair.fixed_index].target_packed_bytes,
        memories[pair.fpu_index].target_packed_bytes,
        mem_ratio);
    lsb_stats_string(pair_stats[pair.fixed_index], lsb);
    mean_lsb_string(pair_stats[pair.fixed_index], mean_lsb);

    char ratio[24];
    if (benches[pair.fpu_index].measured && benches[pair.fixed_index].measured) {
        const double r = benches[pair.fixed_index].host_ns_per_pixel
                       / benches[pair.fpu_index].host_ns_per_pixel;
        fl::snprintf(ratio, sizeof(ratio), "%.2fx", r);
    } else {
        fl::snprintf(ratio, sizeof(ratio), "%s", "n/a");
    }

    char mcu[96];
    fl::snprintf(mcu, sizeof(mcu), "FPU %s; fixed %s",
             fpu_cycles_for(pair.fpu_index),
             fixed_cycles_for(pair.fixed_index));
    pair_mcu_fixed_fpu_ratio_string(pair, mcu_ratio);

    char line[1536];
    fl::snprintf(line, sizeof(line),
             "%-21s | %-26s | %-30s | %11s | %12s | %14s | %-22s | %8s | %9s | %13s | %13s | %29s | %8s | %-41s | %s",
             pair.label,
             kStrategies[pair.fpu_index].name,
             kStrategies[pair.fixed_index].name,
             fpu_host,
             fixed_host,
             ratio,
             mcu_ratio,
             fpu_mem,
             fixed_mem,
             mem_delta,
             mem_ratio,
             lsb,
             mean_lsb,
             mcu,
             pair.scope);
    fl::cout << line << "\n";
}

inline void print_strategy_readiness_line(
    int index,
    const MemoryInfo memories[kStrategyCount],
    const BenchStats benches[kStrategyCount],
    const ErrorStats cf_stats[kStrategyCount],
    const ErrorStats target_stats[kStrategyCount]) {
    char accuracy[32];
    char host[24];
    char mcu[64];
    char production[96];
    char readiness[128];
    const char* status = status_for(
        kStrategies[index],
        cf_stats[index],
        target_stats[index],
        kBudgets[index][0],
        kBudgets[index][1]);
    fl::snprintf(accuracy, sizeof(accuracy), "%s", status);
    bench_string(benches[index], host);
    cpu_evidence_status_string(index, mcu);
    production_gate_string(index, production);
    strategy_readiness_string(index, readiness);

    char line[1408];
    fl::snprintf(line, sizeof(line),
             "%-26s| %-8s | %-10s | %-21s | %10s | %-31s | %-29s | %-43s | %s",
             kStrategies[index].name,
             kStrategies[index].stage,
             accuracy,
             memory_gap_class(memories[index]),
             host,
             mcu,
             production,
             readiness,
             kStrategies[index].missing);
    fl::cout << line << "\n";
}

inline void print_strategy_blocker_line(
    int index,
    u32 mask,
    const MemoryInfo memories[kStrategyCount],
    const ErrorStats cf_stats[kStrategyCount],
    const ErrorStats target_stats[kStrategyCount]) {
    char blockers[192];
    char close_action[192];
    char memory_gap[32];
    strategy_blocker_string(mask, blockers);
    strategy_blocker_close_action(mask, close_action);
    fl::snprintf(memory_gap, sizeof(memory_gap), "%s", memory_gap_class(memories[index]));
    const char* accuracy = status_for(
        kStrategies[index],
        cf_stats[index],
        target_stats[index],
        kBudgets[index][0],
        kBudgets[index][1]);

    char line[1280];
    fl::snprintf(line, sizeof(line),
             "%-26s| %-8s | %-10s | %-21s | %-58s | %s",
             kStrategies[index].name,
             kStrategies[index].stage,
             accuracy,
             memory_gap,
             blockers,
             close_action);
    fl::cout << line << "\n";
}

inline void print_prototype_disposition_line(int index, u32 mask) {
    char disposition[128];
    char blockers[192];
    char close_action[192];
    prototype_disposition_string(index, mask, disposition);
    strategy_blocker_string(mask, blockers);
    if (text_equals(kStrategies[index].stage, "REJECTED")) {
        fl::snprintf(close_action, sizeof(close_action), "%s",
                 "keep rejected row as negative evidence");
    } else {
        strategy_blocker_close_action(mask, close_action);
    }

    char line[1536];
    fl::snprintf(line, sizeof(line),
             "%-34s| %-8s | %-28s | %-58s | %-47s | %s",
             kStrategies[index].name,
             kStrategies[index].stage,
             disposition,
             blockers,
             close_action,
             kStrategies[index].missing);
    fl::cout << line << "\n";
}

inline void print_cpu_evidence_coverage_line(
    int index, const BenchStats benches[kStrategyCount]) {
    char host[24];
    char paired_host[64];
    char mcu_ratio[64];
    char status[64];
    bench_string(benches[index], host);
    host_fixed_fpu_ratio_string(index, benches, paired_host);
    strategy_mcu_fixed_fpu_ratio_string(index, mcu_ratio);
    cpu_evidence_status_string(index, status);

    char line[1024];
    fl::snprintf(line, sizeof(line),
             "%-26s| %-8s | %10s | %-20s | %-22s | %-31s | %-23s | %-29s | %s",
             kStrategies[index].name,
             kStrategies[index].stage,
             host,
             fpu_cycles_for(index),
             fixed_cycles_for(index),
             status,
             mcu_ratio,
             paired_host,
             kStrategies[index].cpu_validation);
    fl::cout << line << "\n";
}

inline bool completion_gate_status_known(const char* status) {
    return text_equals(status, "PASS")
        || text_equals(status, "PARTIAL")
        || text_equals(status, "BLOCKED");
}

inline void record_completion_gate_status(const char* status,
                                          int* pass_count,
                                          int* partial_count,
                                          int* blocked_count) {
    if (text_equals(status, "PASS")) {
        ++(*pass_count);
    } else if (text_equals(status, "PARTIAL")) {
        ++(*partial_count);
    } else if (text_equals(status, "BLOCKED")) {
        ++(*blocked_count);
    }
}

inline void print_completion_gate_line(const char* gate,
                                       const char* status,
                                       const char* evidence,
                                       const char* blocker) {
    char line[1024];
    fl::snprintf(line, sizeof(line),
             "%-34s | %-8s | %-58s | %s",
             gate,
             status,
             evidence,
             blocker);
    fl::cout << line << "\n";
}

inline void print_memory_gap_line(int index,
                                  const MemoryInfo memories[kStrategyCount]) {
    const MemoryInfo& memory = memories[index];
    char target[24];
    char proto[24];
    char runtime[24];
    char proto_delta[32];
    char runtime_delta[32];
    char proto_ratio[32];
    char runtime_ratio[32];
    memory_string(memory.target_packed_bytes, target);
    memory_string(memory.prototype_storage_bytes, proto);
    memory_string(memory.runtime_ram_bytes, runtime);
    signed_memory_delta_string(
        memory.prototype_storage_bytes, memory.target_packed_bytes, proto_delta);
    signed_memory_delta_string(
        memory.runtime_ram_bytes, memory.target_packed_bytes, runtime_delta);
    memory_ratio_string(
        memory.prototype_storage_bytes, memory.target_packed_bytes, proto_ratio);
    memory_ratio_string(
        memory.runtime_ram_bytes, memory.target_packed_bytes, runtime_ratio);

    char line[1280];
    fl::snprintf(line, sizeof(line),
             "%-26s| %-8s | %13s | %9s | %13s | %12s | %11s | %13s | %10s | %-21s | %s",
             kStrategies[index].name,
             kStrategies[index].stage,
             target,
             proto,
             proto_delta,
             proto_ratio,
             runtime,
             runtime_delta,
             runtime_ratio,
             memory_gap_class(memory),
             kStrategies[index].missing);
    fl::cout << line << "\n";
}

inline void print_decision_line(const char* category,
                                int index,
                                const char* why,
                                const char* blockers,
                                const MemoryInfo memories[kStrategyCount],
                                const BenchStats benches[kStrategyCount],
                                const ErrorStats cf_stats[kStrategyCount]) {
    char mem[24];
    char cpu[192];
    char lsb[48];
    memory_string(memories[index].target_packed_bytes, mem);
    decision_cpu_string(index, benches, cpu);
    lsb_stats_string(cf_stats[index], lsb);
    char line[960];
    fl::snprintf(line, sizeof(line),
             "%-22s | %-26s | %-40s | %10s | %-72s | %19s | %s",
             category,
             kStrategies[index].name,
             why,
             mem,
             cpu,
             lsb,
             blockers);
    fl::cout << line << "\n";
}

inline void print_memory_scaling_line(int n) {
    char bilinear[24];
    char hermite[24];
    char cell[24];
    char hull[24];
    char tetra_i16[24];
    char tetra_u8[24];
    memory_string(lut_bytes(n, RgbwLutInterp::Bilinear), bilinear);
    memory_string(lut_bytes(n, RgbwLutInterp::Hermite), hermite);
    memory_string(packed_hermite_cell_bytes(n), cell);
    memory_string(packed_hermite_cell_bytes(n) + packed_hull_axis_bytes(n), hull);
    memory_string(packed_tetrahedral_i16_bytes(n), tetra_i16);
    memory_string(packed_tetrahedral_u8_bytes(n), tetra_u8);

    char line[512];
    fl::snprintf(line, sizeof(line),
             "%6ld | %15s | %13s | %15s | %20s | %20s | %19s",
             static_cast<long>(n),
             bilinear,
             hermite,
             cell,
             hull,
             tetra_i16,
             tetra_u8);
    fl::cout << line << "\n";
}

inline void print_reference_parity_line(
    const char* scope, const ErrorStats& stats, const char* meaning) {
    char lsb[48];
    lsb_stats_string(stats, lsb);
    char line[640];
    fl::snprintf(line, sizeof(line),
             "%-14s | %7ld | %33s | %8.2f | %s",
             scope,
             static_cast<long>(stats.sample_count),
             lsb,
             stats.mean_lsb,
             meaning);
    fl::cout << line << "\n";
}

inline ErrorStats evaluate_uniform_lut_grid_cube_lsb(
    int n, LutInterp interp, const DiodeProfile& profile, int steps) {
    ErrorStats stats{};
    if (n <= 1 || steps <= 1) {
        return stats;
    }

    strategy_uniform_lut::State state;
    strategy_uniform_lut::setup_state(state, profile, n, interp);
    strategy_closed_form::setup(profile);

    fl::vector<fl::u32> deltas;
    deltas.reserve(steps * steps * steps);

    for (int ri = 0; ri < steps; ++ri) {
        const fl::u8 r_in = cube_value(ri, steps);
        for (int gi = 0; gi < steps; ++gi) {
            const fl::u8 g_in = cube_value(gi, steps);
            for (int bi = 0; bi < steps; ++bi) {
                const fl::u8 b_in = cube_value(bi, steps);
                ++stats.sample_count;
                fl::u8 lut_r = 0, lut_g = 0, lut_b = 0, lut_w = 0;
                fl::u8 ref_r = 0, ref_g = 0, ref_b = 0, ref_w = 0;
                strategy_uniform_lut::solve_state(
                    state, r_in, g_in, b_in, &lut_r, &lut_g, &lut_b, &lut_w);
                strategy_closed_form::solve(
                    r_in, g_in, b_in, &ref_r, &ref_g, &ref_b, &ref_w);
                deltas.push_back(max_channel_lsb_delta(
                    lut_r, lut_g, lut_b, lut_w, ref_r, ref_g, ref_b, ref_w));
            }
        }
    }

    strategy_uniform_lut::teardown_state(state);
    strategy_closed_form::teardown();

    if (!deltas.empty()) {
        fl::sort(deltas.begin(), deltas.end());
        stats.p50_lsb = percentile_u32(deltas, 0.50);
        stats.p75_lsb = percentile_u32(deltas, 0.75);
        stats.p95_lsb = percentile_u32(deltas, 0.95);
        stats.p99_lsb = percentile_u32(deltas, 0.99);
        stats.peak_lsb = static_cast<fl::i32>(deltas.back());
        double sum = 0.0;
        for (fl::u32 d : deltas) {
            sum += static_cast<double>(d);
        }
        stats.mean_lsb = sum / static_cast<double>(deltas.size());
    }

    return stats;
}

inline BenchStats benchmark_uniform_lut_grid(int n, LutInterp interp,
                                             const DiodeProfile& profile) {
    BenchStats out;
    if (n <= 1) {
        return out;
    }

    static constexpr int kWarmupRepeats = 8;
    static constexpr int kMeasureRepeats = 256;

    strategy_uniform_lut::State state;
    strategy_uniform_lut::setup_state(state, profile, n, interp);

    u32 checksum = 0;
    for (int pass = 0; pass < kWarmupRepeats; ++pass) {
        for (int i = 0; i < kVerifierRowCount; ++i) {
            const VerifierRow& row = kVerifierRows[i];
            const u8 r = row_u16_to_u8(row.r_in16);
            const u8 g = row_u16_to_u8(row.g_in16);
            const u8 b = row_u16_to_u8(row.b_in16);
            u8 ro = 0, go = 0, bo = 0, wo = 0;
            strategy_uniform_lut::solve_state(state, r, g, b, &ro, &go, &bo, &wo);
            checksum += ro + (static_cast<u32>(go) << 1)
                      + (static_cast<u32>(bo) << 2)
                      + (static_cast<u32>(wo) << 3);
        }
    }

    const u32 t0 = fl::micros();
    for (int pass = 0; pass < kMeasureRepeats; ++pass) {
        for (int i = 0; i < kVerifierRowCount; ++i) {
            const VerifierRow& row = kVerifierRows[i];
            const u8 r = row_u16_to_u8(row.r_in16);
            const u8 g = row_u16_to_u8(row.g_in16);
            const u8 b = row_u16_to_u8(row.b_in16);
            u8 ro = 0, go = 0, bo = 0, wo = 0;
            strategy_uniform_lut::solve_state(state, r, g, b, &ro, &go, &bo, &wo);
            checksum += ro + (static_cast<u32>(go) << 1)
                      + (static_cast<u32>(bo) << 2)
                      + (static_cast<u32>(wo) << 3);
        }
    }
    const u32 elapsed_us = fl::micros() - t0;
    const double pixels = static_cast<double>(kVerifierRowCount)
                        * static_cast<double>(kMeasureRepeats);
    if (elapsed_us > 0 && pixels > 0.0) {
        out.measured = true;
        out.host_ns_per_pixel =
            (static_cast<double>(elapsed_us) * 1000.0) / pixels;
    }
    out.checksum = checksum;
    g_bench_sink = checksum;

    strategy_uniform_lut::teardown_state(state);
    return out;
}

inline void print_lut_grid_accuracy_line(int n,
                                         const ErrorStats& bilinear,
                                         const BenchStats& bilinear_bench,
                                         const ErrorStats& hermite,
                                         const BenchStats& hermite_bench) {
    char bilinear_bytes[24];
    char hermite_bytes[24];
    char bilinear_cpu[24];
    char hermite_cpu[24];
    char bilinear_lsb[48];
    char hermite_lsb[48];
    char bilinear_mean[16];
    char hermite_mean[16];
    memory_string(lut_bytes(n, RgbwLutInterp::Bilinear), bilinear_bytes);
    memory_string(lut_bytes(n, RgbwLutInterp::Hermite), hermite_bytes);
    bench_string(bilinear_bench, bilinear_cpu);
    bench_string(hermite_bench, hermite_cpu);
    lsb_stats_string(bilinear, bilinear_lsb);
    lsb_stats_string(hermite, hermite_lsb);
    mean_lsb_string(bilinear, bilinear_mean);
    mean_lsb_string(hermite, hermite_mean);

    char line[896];
    fl::snprintf(line, sizeof(line),
             "%6ld | %14s | %18s | %33s | %11s | %13s | %17s | %31s | %11s | %7ld",
             static_cast<long>(n),
             bilinear_bytes,
             bilinear_cpu,
             bilinear_lsb,
             bilinear_mean,
             hermite_bytes,
             hermite_cpu,
             hermite_lsb,
             hermite_mean,
             static_cast<long>(bilinear.sample_count));
    fl::cout << line << "\n";
}

inline int experimental_hermite_table_q(bool warped) {
    return warped ? 4096 : 65536;
}

inline u32 experimental_hermite_packed_bytes(int n, bool warped) {
    u32 bytes = packed_hermite_cell_bytes(n);
    if (warped) {
        bytes += packed_hull_axis_bytes(n);
    }
    return bytes;
}

inline void solve_experimental_hermite_float_grid(
    strategy_experimental_hermite::State& state,
    u8 r, u8 g, u8 b, u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    strategy_experimental_hermite::solve_state(
        state, r, g, b, out_r, out_g, out_b, out_w);
}

inline void solve_experimental_hermite_fixed_grid(
    strategy_experimental_hermite_fixed_eval::State& state,
    u8 r, u8 g, u8 b, u8* out_r, u8* out_g, u8* out_b, u8* out_w) {
    strategy_experimental_hermite_fixed_eval::solve_state(
        state, r, g, b, out_r, out_g, out_b, out_w);
}

inline ErrorStats evaluate_experimental_hermite_grid_cube_lsb(
    int n, bool warped, bool fixed_eval,
    const DiodeProfile& profile, int steps) {
    ErrorStats stats{};
    if (n <= 1 || n > strategy_experimental_hermite::kMaxN || steps <= 1) {
        return stats;
    }

    strategy_closed_form::setup(profile);
    fl::vector<fl::u32> deltas;
    deltas.reserve(steps * steps * steps);

    if (fixed_eval) {
        strategy_experimental_hermite_fixed_eval::State state;
        strategy_experimental_hermite_fixed_eval::setup_state_n(
            state, profile, n, experimental_hermite_table_q(warped), warped);
        for (int ri = 0; ri < steps; ++ri) {
            const fl::u8 r_in = cube_value(ri, steps);
            for (int gi = 0; gi < steps; ++gi) {
                const fl::u8 g_in = cube_value(gi, steps);
                for (int bi = 0; bi < steps; ++bi) {
                    const fl::u8 b_in = cube_value(bi, steps);
                    ++stats.sample_count;
                    fl::u8 grid_r = 0, grid_g = 0, grid_b = 0, grid_w = 0;
                    fl::u8 ref_r = 0, ref_g = 0, ref_b = 0, ref_w = 0;
                    solve_experimental_hermite_fixed_grid(
                        state, r_in, g_in, b_in,
                        &grid_r, &grid_g, &grid_b, &grid_w);
                    strategy_closed_form::solve(
                        r_in, g_in, b_in, &ref_r, &ref_g, &ref_b, &ref_w);
                    deltas.push_back(max_channel_lsb_delta(
                        grid_r, grid_g, grid_b, grid_w,
                        ref_r, ref_g, ref_b, ref_w));
                }
            }
        }
        state.table.cells.reset();
        state.table.N = 0;
    } else {
        strategy_experimental_hermite::State state;
        strategy_experimental_hermite::build_state(
            state, profile, n, experimental_hermite_table_q(warped), warped);
        for (int ri = 0; ri < steps; ++ri) {
            const fl::u8 r_in = cube_value(ri, steps);
            for (int gi = 0; gi < steps; ++gi) {
                const fl::u8 g_in = cube_value(gi, steps);
                for (int bi = 0; bi < steps; ++bi) {
                    const fl::u8 b_in = cube_value(bi, steps);
                    ++stats.sample_count;
                    fl::u8 grid_r = 0, grid_g = 0, grid_b = 0, grid_w = 0;
                    fl::u8 ref_r = 0, ref_g = 0, ref_b = 0, ref_w = 0;
                    solve_experimental_hermite_float_grid(
                        state, r_in, g_in, b_in,
                        &grid_r, &grid_g, &grid_b, &grid_w);
                    strategy_closed_form::solve(
                        r_in, g_in, b_in, &ref_r, &ref_g, &ref_b, &ref_w);
                    deltas.push_back(max_channel_lsb_delta(
                        grid_r, grid_g, grid_b, grid_w,
                        ref_r, ref_g, ref_b, ref_w));
                }
            }
        }
        state.cells.reset();
        state.N = 0;
    }

    strategy_closed_form::teardown();

    if (!deltas.empty()) {
        fl::sort(deltas.begin(), deltas.end());
        stats.p50_lsb = percentile_u32(deltas, 0.50);
        stats.p75_lsb = percentile_u32(deltas, 0.75);
        stats.p95_lsb = percentile_u32(deltas, 0.95);
        stats.p99_lsb = percentile_u32(deltas, 0.99);
        stats.peak_lsb = static_cast<fl::i32>(deltas.back());
        double sum = 0.0;
        for (fl::u32 d : deltas) {
            sum += static_cast<double>(d);
        }
        stats.mean_lsb = sum / static_cast<double>(deltas.size());
    }

    return stats;
}

inline BenchStats benchmark_experimental_hermite_grid(
    int n, bool warped, bool fixed_eval, const DiodeProfile& profile) {
    BenchStats out;
    if (n <= 1 || n > strategy_experimental_hermite::kMaxN) {
        return out;
    }

    static constexpr int kWarmupRepeats = 8;
    static constexpr int kMeasureRepeats = 256;
    u32 checksum = 0;

    if (fixed_eval) {
        strategy_experimental_hermite_fixed_eval::State state;
        strategy_experimental_hermite_fixed_eval::setup_state_n(
            state, profile, n, experimental_hermite_table_q(warped), warped);
        for (int pass = 0; pass < kWarmupRepeats; ++pass) {
            for (int i = 0; i < kVerifierRowCount; ++i) {
                const VerifierRow& row = kVerifierRows[i];
                const u8 r = row_u16_to_u8(row.r_in16);
                const u8 g = row_u16_to_u8(row.g_in16);
                const u8 b = row_u16_to_u8(row.b_in16);
                u8 ro = 0, go = 0, bo = 0, wo = 0;
                solve_experimental_hermite_fixed_grid(
                    state, r, g, b, &ro, &go, &bo, &wo);
                checksum += ro + (static_cast<u32>(go) << 1)
                          + (static_cast<u32>(bo) << 2)
                          + (static_cast<u32>(wo) << 3);
            }
        }
        const u32 t0 = fl::micros();
        for (int pass = 0; pass < kMeasureRepeats; ++pass) {
            for (int i = 0; i < kVerifierRowCount; ++i) {
                const VerifierRow& row = kVerifierRows[i];
                const u8 r = row_u16_to_u8(row.r_in16);
                const u8 g = row_u16_to_u8(row.g_in16);
                const u8 b = row_u16_to_u8(row.b_in16);
                u8 ro = 0, go = 0, bo = 0, wo = 0;
                solve_experimental_hermite_fixed_grid(
                    state, r, g, b, &ro, &go, &bo, &wo);
                checksum += ro + (static_cast<u32>(go) << 1)
                          + (static_cast<u32>(bo) << 2)
                          + (static_cast<u32>(wo) << 3);
            }
        }
        const u32 elapsed_us = fl::micros() - t0;
        const double pixels = static_cast<double>(kVerifierRowCount)
                            * static_cast<double>(kMeasureRepeats);
        if (elapsed_us > 0 && pixels > 0.0) {
            out.measured = true;
            out.host_ns_per_pixel =
                (static_cast<double>(elapsed_us) * 1000.0) / pixels;
        }
        state.table.cells.reset();
        state.table.N = 0;
    } else {
        strategy_experimental_hermite::State state;
        strategy_experimental_hermite::build_state(
            state, profile, n, experimental_hermite_table_q(warped), warped);
        for (int pass = 0; pass < kWarmupRepeats; ++pass) {
            for (int i = 0; i < kVerifierRowCount; ++i) {
                const VerifierRow& row = kVerifierRows[i];
                const u8 r = row_u16_to_u8(row.r_in16);
                const u8 g = row_u16_to_u8(row.g_in16);
                const u8 b = row_u16_to_u8(row.b_in16);
                u8 ro = 0, go = 0, bo = 0, wo = 0;
                solve_experimental_hermite_float_grid(
                    state, r, g, b, &ro, &go, &bo, &wo);
                checksum += ro + (static_cast<u32>(go) << 1)
                          + (static_cast<u32>(bo) << 2)
                          + (static_cast<u32>(wo) << 3);
            }
        }
        const u32 t0 = fl::micros();
        for (int pass = 0; pass < kMeasureRepeats; ++pass) {
            for (int i = 0; i < kVerifierRowCount; ++i) {
                const VerifierRow& row = kVerifierRows[i];
                const u8 r = row_u16_to_u8(row.r_in16);
                const u8 g = row_u16_to_u8(row.g_in16);
                const u8 b = row_u16_to_u8(row.b_in16);
                u8 ro = 0, go = 0, bo = 0, wo = 0;
                solve_experimental_hermite_float_grid(
                    state, r, g, b, &ro, &go, &bo, &wo);
                checksum += ro + (static_cast<u32>(go) << 1)
                          + (static_cast<u32>(bo) << 2)
                          + (static_cast<u32>(wo) << 3);
            }
        }
        const u32 elapsed_us = fl::micros() - t0;
        const double pixels = static_cast<double>(kVerifierRowCount)
                            * static_cast<double>(kMeasureRepeats);
        if (elapsed_us > 0 && pixels > 0.0) {
            out.measured = true;
            out.host_ns_per_pixel =
                (static_cast<double>(elapsed_us) * 1000.0) / pixels;
        }
        state.cells.reset();
        state.N = 0;
    }

    out.checksum = checksum;
    g_bench_sink = checksum;
    return out;
}

inline void print_experimental_hermite_grid_line(
    int n,
    bool warped,
    const ErrorStats& float_stats,
    const BenchStats& float_bench,
    const ErrorStats& fixed_stats,
    const BenchStats& fixed_bench) {
    char bytes[24];
    char float_cpu[24];
    char fixed_cpu[24];
    char float_lsb[48];
    char fixed_lsb[48];
    char float_mean[16];
    char fixed_mean[16];
    memory_string(experimental_hermite_packed_bytes(n, warped), bytes);
    bench_string(float_bench, float_cpu);
    bench_string(fixed_bench, fixed_cpu);
    lsb_stats_string(float_stats, float_lsb);
    lsb_stats_string(fixed_stats, fixed_lsb);
    mean_lsb_string(float_stats, float_mean);
    mean_lsb_string(fixed_stats, fixed_mean);

    char line[896];
    fl::snprintf(line, sizeof(line),
             "%6ld | %12s | %16s | %32s | %8s | %16s | %32s | %8s | %7ld",
             static_cast<long>(n),
             bytes,
             float_cpu,
             float_lsb,
             float_mean,
             fixed_cpu,
             fixed_lsb,
             fixed_mean,
             static_cast<long>(float_stats.sample_count));
    fl::cout << line << "\n";
}

enum class TetraGridStorage : u8 {
    I16,
    U8,
};

struct TetraGridState {
    int n = 0;
    TetraGridStorage storage = TetraGridStorage::I16;
    ProfileCache cache;
    fl::unique_ptr<i16[]> cells_i16;
    fl::unique_ptr<u8[]> cells_u8;
};

struct TetraCompressionStats {
    int n = 0;
    bool boosted = false;
    u32 node_count = 0;
    u32 zero_channel_entries = 0;
    u32 unique_i16_cells = 0;
    u32 unique_u8_cells = 0;
    u32 raw_i16_bytes = 0;
    u32 raw_u8_bytes = 0;
    u32 palette_i16_bytes = 0;
    u32 palette_u8_bytes = 0;
};

struct HybridSeedAuditStats {
    i32 sample_count = 0;
    i32 off_count = 0;
    i32 native_single_count = 0;
    i32 ls2_count = 0;
    i32 interior_count = 0;
    i32 seed_count[4] = {};
    i32 seed_direct_count = 0;
    i32 seed_fallback_count = 0;
    i32 output_diff_count = 0;
    ErrorStats output_delta;
};

inline int tetra_grid_cell_offset(const TetraGridState& state,
                                  int r, int g, int b) {
    return (((r * state.n) + g) * state.n + b) * 4;
}

inline void setup_tetra_grid_state(TetraGridState& state,
                                   const DiodeProfile& profile,
                                   int n,
                                   TetraGridStorage storage,
                                   bool boosted) {
    state.n = n;
    state.storage = storage;
    strategy_common::build_default_cache(profile, &state.cache);
    const int cell_count = n * n * n * 4;
    if (storage == TetraGridStorage::U8) {
        state.cells_u8 = fl::make_unique<u8[]>(cell_count);
        state.cells_i16.reset();
    } else {
        state.cells_i16 = fl::make_unique<i16[]>(cell_count);
        state.cells_u8.reset();
    }

    for (int ri = 0; ri < n; ++ri) {
        for (int gi = 0; gi < n; ++gi) {
            for (int bi = 0; bi < n; ++bi) {
                const float r = static_cast<float>(ri) / static_cast<float>(n - 1);
                const float g = static_cast<float>(gi) / static_cast<float>(n - 1);
                const float b = static_cast<float>(bi) / static_cast<float>(n - 1);
                float rgbw[4] = {0, 0, 0, 0};
                if (boosted) {
                    solve_wx_overdrive(
                        state.cache, r, g, b, kDefaultOverdriveRatio, rgbw);
                } else {
                    solve_strict_subgamut(state.cache, r, g, b, rgbw);
                }
                const int offset = tetra_grid_cell_offset(state, ri, gi, bi);
                for (int k = 0; k < 4; ++k) {
                    if (storage == TetraGridStorage::U8) {
                        state.cells_u8[offset + k] = quantize_u8(rgbw[k]);
                    } else {
                        state.cells_i16[offset + k] = static_cast<i16>(
                            fl::clamp(strategy_common::quantize_q(
                                          rgbw[k], strategy_tetrahedral_rgb8::kQ),
                                      static_cast<i32>(-32768),
                                      static_cast<i32>(32767)));
                    }
                }
            }
        }
    }
}

inline void fetch_tetra_grid_state(const TetraGridState& state,
                                   int ri, int gi, int bi, float out[4]) {
    const int offset = tetra_grid_cell_offset(state, ri, gi, bi);
    if (state.storage == TetraGridStorage::U8) {
        for (int k = 0; k < 4; ++k) {
            out[k] = static_cast<float>(state.cells_u8[offset + k]) * (1.0f / 255.0f);
        }
        return;
    }
    const float inv_q = 1.0f / static_cast<float>(strategy_tetrahedral_rgb8::kQ);
    for (int k = 0; k < 4; ++k) {
        out[k] = static_cast<float>(state.cells_i16[offset + k]) * inv_q;
    }
}

inline void solve_tetra_grid_state(const TetraGridState& state,
                                   u8 r, u8 g, u8 b,
                                   u8* out_r, u8* out_g,
                                   u8* out_b, u8* out_w) {
    if ((r | g | b) == 0) {
        *out_r = *out_g = *out_b = *out_w = 0;
        return;
    }
    const int n = state.n;
    const float xr = fl::clamp((r * (1.0f / 255.0f)) * (n - 1), 0.0f,
                               static_cast<float>(n - 1) - 1e-4f);
    const float xg = fl::clamp((g * (1.0f / 255.0f)) * (n - 1), 0.0f,
                               static_cast<float>(n - 1) - 1e-4f);
    const float xb = fl::clamp((b * (1.0f / 255.0f)) * (n - 1), 0.0f,
                               static_cast<float>(n - 1) - 1e-4f);
    const int ri = static_cast<int>(xr);
    const int gi = static_cast<int>(xg);
    const int bi = static_cast<int>(xb);
    const float fr = xr - ri;
    const float fg = xg - gi;
    const float fb = xb - bi;

    float c000[4], c100[4], c010[4], c001[4];
    float c110[4], c101[4], c011[4], c111[4], out[4];
    fetch_tetra_grid_state(state, ri, gi, bi, c000);
    fetch_tetra_grid_state(state, ri + 1, gi, bi, c100);
    fetch_tetra_grid_state(state, ri, gi + 1, bi, c010);
    fetch_tetra_grid_state(state, ri, gi, bi + 1, c001);
    fetch_tetra_grid_state(state, ri + 1, gi + 1, bi, c110);
    fetch_tetra_grid_state(state, ri + 1, gi, bi + 1, c101);
    fetch_tetra_grid_state(state, ri, gi + 1, bi + 1, c011);
    fetch_tetra_grid_state(state, ri + 1, gi + 1, bi + 1, c111);

    if (fr >= fg) {
        if (fg >= fb) {
            strategy_tetrahedral_rgb8::lerp_path(
                c000, c100, c110, c111, fr, fg, fb, out);
        } else if (fr >= fb) {
            strategy_tetrahedral_rgb8::lerp_path(
                c000, c100, c101, c111, fr, fb, fg, out);
        } else {
            strategy_tetrahedral_rgb8::lerp_path(
                c000, c001, c101, c111, fb, fr, fg, out);
        }
    } else {
        if (fr >= fb) {
            strategy_tetrahedral_rgb8::lerp_path(
                c000, c010, c110, c111, fg, fr, fb, out);
        } else if (fg >= fb) {
            strategy_tetrahedral_rgb8::lerp_path(
                c000, c010, c011, c111, fg, fb, fr, out);
        } else {
            strategy_tetrahedral_rgb8::lerp_path(
                c000, c001, c011, c111, fb, fg, fr, out);
        }
    }

    strategy_common::normalize4(out);
    strategy_common::quantize_rgbw(out, out_r, out_g, out_b, out_w);
}

inline ErrorStats evaluate_tetra_grid_cube_lsb(
    int n, TetraGridStorage storage, const DiodeProfile& profile,
    int steps, bool boosted, bool policy_reference) {
    ErrorStats stats{};
    if (n <= 1 || steps <= 1) {
        return stats;
    }

    TetraGridState state;
    setup_tetra_grid_state(state, profile, n, storage, boosted);
    if (policy_reference) {
        strategy_wx_overdrive::setup(profile);
    } else {
        strategy_closed_form::setup(profile);
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
                fl::u8 lut_r = 0, lut_g = 0, lut_b = 0, lut_w = 0;
                fl::u8 ref_r = 0, ref_g = 0, ref_b = 0, ref_w = 0;
                solve_tetra_grid_state(
                    state, r_in, g_in, b_in, &lut_r, &lut_g, &lut_b, &lut_w);
                if (policy_reference) {
                    strategy_wx_overdrive::solve(
                        r_in, g_in, b_in, &ref_r, &ref_g, &ref_b, &ref_w);
                } else {
                    strategy_closed_form::solve(
                        r_in, g_in, b_in, &ref_r, &ref_g, &ref_b, &ref_w);
                }
                deltas.push_back(max_channel_lsb_delta(
                    lut_r, lut_g, lut_b, lut_w, ref_r, ref_g, ref_b, ref_w));
            }
        }
    }

    state.cells_i16.reset();
    state.cells_u8.reset();
    if (policy_reference) {
        strategy_wx_overdrive::teardown();
    } else {
        strategy_closed_form::teardown();
    }

    if (!deltas.empty()) {
        fl::sort(deltas.begin(), deltas.end());
        stats.p50_lsb = percentile_u32(deltas, 0.50);
        stats.p75_lsb = percentile_u32(deltas, 0.75);
        stats.p95_lsb = percentile_u32(deltas, 0.95);
        stats.p99_lsb = percentile_u32(deltas, 0.99);
        stats.peak_lsb = static_cast<fl::i32>(deltas.back());
        double sum = 0.0;
        for (fl::u32 d : deltas) {
            sum += static_cast<double>(d);
        }
        stats.mean_lsb = sum / static_cast<double>(deltas.size());
    }

    return stats;
}

inline BenchStats benchmark_tetra_grid(int n, TetraGridStorage storage,
                                       const DiodeProfile& profile,
                                       bool boosted) {
    BenchStats out;
    if (n <= 1) {
        return out;
    }

    static constexpr int kWarmupRepeats = 8;
    static constexpr int kMeasureRepeats = 256;

    TetraGridState state;
    setup_tetra_grid_state(state, profile, n, storage, boosted);

    u32 checksum = 0;
    for (int pass = 0; pass < kWarmupRepeats; ++pass) {
        for (int i = 0; i < kVerifierRowCount; ++i) {
            const VerifierRow& row = kVerifierRows[i];
            const u8 r = row_u16_to_u8(row.r_in16);
            const u8 g = row_u16_to_u8(row.g_in16);
            const u8 b = row_u16_to_u8(row.b_in16);
            u8 ro = 0, go = 0, bo = 0, wo = 0;
            solve_tetra_grid_state(state, r, g, b, &ro, &go, &bo, &wo);
            checksum += ro + (static_cast<u32>(go) << 1)
                      + (static_cast<u32>(bo) << 2)
                      + (static_cast<u32>(wo) << 3);
        }
    }

    const u32 t0 = fl::micros();
    for (int pass = 0; pass < kMeasureRepeats; ++pass) {
        for (int i = 0; i < kVerifierRowCount; ++i) {
            const VerifierRow& row = kVerifierRows[i];
            const u8 r = row_u16_to_u8(row.r_in16);
            const u8 g = row_u16_to_u8(row.g_in16);
            const u8 b = row_u16_to_u8(row.b_in16);
            u8 ro = 0, go = 0, bo = 0, wo = 0;
            solve_tetra_grid_state(state, r, g, b, &ro, &go, &bo, &wo);
            checksum += ro + (static_cast<u32>(go) << 1)
                      + (static_cast<u32>(bo) << 2)
                      + (static_cast<u32>(wo) << 3);
        }
    }
    const u32 elapsed_us = fl::micros() - t0;
    const double pixels = static_cast<double>(kVerifierRowCount)
                        * static_cast<double>(kMeasureRepeats);
    if (elapsed_us > 0 && pixels > 0.0) {
        out.measured = true;
        out.host_ns_per_pixel =
            (static_cast<double>(elapsed_us) * 1000.0) / pixels;
    }
    out.checksum = checksum;
    g_bench_sink = checksum;

    state.cells_i16.reset();
    state.cells_u8.reset();
    return out;
}

inline void print_tetra_grid_scaling_line(int n,
                                          const ErrorStats& i16_stats,
                                          const BenchStats& i16_bench,
                                          const ErrorStats& u8_stats,
                                          const BenchStats& u8_bench) {
    char i16_bytes[24];
    char u8_bytes[24];
    char i16_cpu[24];
    char u8_cpu[24];
    char i16_lsb[48];
    char u8_lsb[48];
    char i16_mean[16];
    char u8_mean[16];
    memory_string(packed_tetrahedral_i16_bytes(n), i16_bytes);
    memory_string(packed_tetrahedral_u8_bytes(n), u8_bytes);
    bench_string(i16_bench, i16_cpu);
    bench_string(u8_bench, u8_cpu);
    lsb_stats_string(i16_stats, i16_lsb);
    lsb_stats_string(u8_stats, u8_lsb);
    mean_lsb_string(i16_stats, i16_mean);
    mean_lsb_string(u8_stats, u8_mean);

    char line[896];
    fl::snprintf(line, sizeof(line),
             "%6ld | %9s | %13s | %27s | %8s | %8s | %12s | %25s | %8s | %7ld",
             static_cast<long>(n),
             i16_bytes,
             i16_cpu,
             i16_lsb,
             i16_mean,
             u8_bytes,
             u8_cpu,
             u8_lsb,
             u8_mean,
             static_cast<long>(i16_stats.sample_count));
    fl::cout << line << "\n";
}

inline u32 count_unique_u64(fl::vector<u64>& values) {
    if (values.empty()) {
        return 0;
    }
    fl::sort(values.begin(), values.end());
    u32 unique_count = 1;
    for (fl::size_t i = 1; i < values.size(); ++i) {
        if (values[i] != values[i - 1]) {
            ++unique_count;
        }
    }
    return unique_count;
}

inline u32 palette_index_bytes(u32 unique_count) {
    if (unique_count <= 256) {
        return 1;
    }
    if (unique_count <= 65536) {
        return 2;
    }
    return 4;
}

inline TetraCompressionStats evaluate_tetra_compression_signal(
    int n, bool boosted, const DiodeProfile& profile) {
    TetraCompressionStats stats;
    stats.n = n;
    stats.boosted = boosted;
    if (n <= 1) {
        return stats;
    }

    TetraGridState i16_state;
    TetraGridState u8_state;
    setup_tetra_grid_state(i16_state, profile, n, TetraGridStorage::I16, boosted);
    setup_tetra_grid_state(u8_state, profile, n, TetraGridStorage::U8, boosted);

    const u32 nodes = static_cast<u32>(n * n * n);
    stats.node_count = nodes;
    stats.raw_i16_bytes = packed_tetrahedral_i16_bytes(n);
    stats.raw_u8_bytes = packed_tetrahedral_u8_bytes(n);

    fl::vector<u64> i16_cells;
    fl::vector<u64> u8_cells;
    i16_cells.reserve(nodes);
    u8_cells.reserve(nodes);

    for (u32 node = 0; node < nodes; ++node) {
        const u32 offset = node * 4;
        u64 packed_i16 = 0;
        u64 packed_u8 = 0;
        for (int ch = 0; ch < 4; ++ch) {
            const u16 q = static_cast<u16>(i16_state.cells_i16[offset + ch]);
            const u8 u = u8_state.cells_u8[offset + ch];
            packed_i16 |= static_cast<u64>(q) << (ch * 16);
            packed_u8 |= static_cast<u64>(u) << (ch * 8);
            if (u == 0) {
                ++stats.zero_channel_entries;
            }
        }
        i16_cells.push_back(packed_i16);
        u8_cells.push_back(packed_u8);
    }

    stats.unique_i16_cells = count_unique_u64(i16_cells);
    stats.unique_u8_cells = count_unique_u64(u8_cells);
    stats.palette_i16_bytes =
        stats.unique_i16_cells * 8 + nodes * palette_index_bytes(stats.unique_i16_cells);
    stats.palette_u8_bytes =
        stats.unique_u8_cells * 4 + nodes * palette_index_bytes(stats.unique_u8_cells);

    i16_state.cells_i16.reset();
    u8_state.cells_u8.reset();
    return stats;
}

inline void percent_string(u32 numerator, u32 denominator, char out[24]) {
    if (denominator == 0) {
        fl::snprintf(out, 24, "n/a");
        return;
    }
    const double pct =
        100.0 * static_cast<double>(numerator) / static_cast<double>(denominator);
    fl::snprintf(out, 24, "%.1f%%", pct);
}

inline void print_tetra_compression_line(const TetraCompressionStats& stats) {
    char zero_pct[24];
    char raw_i16[24];
    char pal_i16[24];
    char raw_u8[24];
    char pal_u8[24];
    percent_string(stats.zero_channel_entries, stats.node_count * 4, zero_pct);
    memory_string(stats.raw_i16_bytes, raw_i16);
    memory_string(stats.palette_i16_bytes, pal_i16);
    memory_string(stats.raw_u8_bytes, raw_u8);
    memory_string(stats.palette_u8_bytes, pal_u8);

    char line[768];
    fl::snprintf(line, sizeof(line),
             "%-7s | %6ld | %5lu | %13s | %16lu | %8s | %18s | %15lu | %8s | %17s",
             stats.boosted ? "boost" : "strict",
             static_cast<long>(stats.n),
             static_cast<unsigned long>(stats.node_count),
             zero_pct,
             static_cast<unsigned long>(stats.unique_i16_cells),
             raw_i16,
             pal_i16,
             static_cast<unsigned long>(stats.unique_u8_cells),
             raw_u8,
             pal_u8);
    fl::cout << line << "\n";
}

inline bool hybrid_seed_direct_accepts(const i32 X[3], int seed) {
    if (seed < 0 || seed > 2) {
        return false;
    }
    i32 t[3];
    strategy_fixed_analytical_q16::matvec_q(
        strategy_fixed_analytical_q16::g_state.inv[seed], X, t);
    const i32 eps = -(strategy_fixed_analytical_q16::kQ / 10000);
    return t[0] >= eps && t[1] >= eps && t[2] >= eps;
}

inline HybridSeedAuditStats evaluate_hybrid_seed_router_audit(
    const DiodeProfile& profile, int steps) {
    HybridSeedAuditStats stats;
    if (steps <= 1) {
        return stats;
    }

    strategy_hybrid_q16_refine::setup(profile);
    const bool native_input =
        is_native_input_gamut(*strategy_fixed_analytical_q16::g_state.float_cache.profile);

    fl::vector<fl::u32> deltas;
    deltas.reserve(steps * steps * steps);

    for (int ri = 0; ri < steps; ++ri) {
        const fl::u8 r_in = cube_value(ri, steps);
        for (int gi = 0; gi < steps; ++gi) {
            const fl::u8 g_in = cube_value(gi, steps);
            for (int bi = 0; bi < steps; ++bi) {
                const fl::u8 b_in = cube_value(bi, steps);
                ++stats.sample_count;

                fl::u8 fixed_r = 0, fixed_g = 0, fixed_b = 0, fixed_w = 0;
                fl::u8 hybrid_r = 0, hybrid_g = 0, hybrid_b = 0, hybrid_w = 0;
                strategy_fixed_analytical_q16::solve(
                    r_in, g_in, b_in, &fixed_r, &fixed_g, &fixed_b, &fixed_w);
                strategy_hybrid_q16_refine::solve(
                    r_in, g_in, b_in, &hybrid_r, &hybrid_g, &hybrid_b, &hybrid_w);
                const fl::u32 delta = max_channel_lsb_delta(
                    fixed_r, fixed_g, fixed_b, fixed_w,
                    hybrid_r, hybrid_g, hybrid_b, hybrid_w);
                deltas.push_back(delta);
                if (delta != 0) {
                    ++stats.output_diff_count;
                }

                const u8 value = fl::max(fl::max(r_in, g_in), b_in);
                if (value == 0) {
                    ++stats.off_count;
                    continue;
                }
                const int active_count =
                    (r_in > 0 ? 1 : 0) + (g_in > 0 ? 1 : 0) + (b_in > 0 ? 1 : 0);
                if (native_input && active_count == 1) {
                    ++stats.native_single_count;
                    continue;
                }
                if (native_input && active_count == 2) {
                    ++stats.ls2_count;
                    continue;
                }

                ++stats.interior_count;
                i32 X[3];
                strategy_fixed_analytical_q16::source_full_chroma_q(
                    r_in, g_in, b_in, value, X);
                int seed = strategy_hybrid_q16_refine::router_seed_for_X(X);
                if (seed < 0 || seed > 3) {
                    seed = 3;
                }
                ++stats.seed_count[seed];
                if (hybrid_seed_direct_accepts(X, seed)) {
                    ++stats.seed_direct_count;
                } else {
                    ++stats.seed_fallback_count;
                }
            }
        }
    }

    strategy_hybrid_q16_refine::teardown();

    if (!deltas.empty()) {
        fl::sort(deltas.begin(), deltas.end());
        stats.output_delta.sample_count = static_cast<i32>(deltas.size());
        stats.output_delta.p50_lsb = percentile_u32(deltas, 0.50);
        stats.output_delta.p75_lsb = percentile_u32(deltas, 0.75);
        stats.output_delta.p95_lsb = percentile_u32(deltas, 0.95);
        stats.output_delta.p99_lsb = percentile_u32(deltas, 0.99);
        stats.output_delta.peak_lsb = static_cast<i32>(deltas.back());
        double sum = 0.0;
        for (fl::u32 delta : deltas) {
            sum += static_cast<double>(delta);
        }
        stats.output_delta.mean_lsb =
            sum / static_cast<double>(deltas.size());
    }

    return stats;
}

inline void print_hybrid_seed_audit_line(
    const HybridSeedAuditStats& stats,
    const BenchStats& fixed_bench,
    const BenchStats& hybrid_bench) {
    char fallback_pct[24];
    char direct_pct[24];
    char lsb[48];
    char host_ratio[32];
    percent_string(static_cast<u32>(stats.seed_fallback_count),
                   static_cast<u32>(stats.interior_count),
                   fallback_pct);
    percent_string(static_cast<u32>(stats.seed_direct_count),
                   static_cast<u32>(stats.interior_count),
                   direct_pct);
    lsb_stats_string(stats.output_delta, lsb);
    if (fixed_bench.measured && hybrid_bench.measured
        && fixed_bench.host_ns_per_pixel > 0.0) {
        const double ratio =
            hybrid_bench.host_ns_per_pixel / fixed_bench.host_ns_per_pixel;
        fl::snprintf(host_ratio, sizeof(host_ratio), "%.2fx fixed host", ratio);
    } else {
        fl::snprintf(host_ratio, sizeof(host_ratio), "host n/a");
    }

    char line[1024];
    fl::snprintf(line, sizeof(line),
             "%7ld | %3ld | %13ld | %4ld | %8ld | %4ld/%4ld/%4ld/%4ld | %11ld | %8s | %13ld | %8s | %11ld | %27s | %s",
             static_cast<long>(stats.sample_count),
             static_cast<long>(stats.off_count),
             static_cast<long>(stats.native_single_count),
             static_cast<long>(stats.ls2_count),
             static_cast<long>(stats.interior_count),
             static_cast<long>(stats.seed_count[0]),
             static_cast<long>(stats.seed_count[1]),
             static_cast<long>(stats.seed_count[2]),
             static_cast<long>(stats.seed_count[3]),
             static_cast<long>(stats.seed_direct_count),
             direct_pct,
             static_cast<long>(stats.seed_fallback_count),
             fallback_pct,
             static_cast<long>(stats.output_diff_count),
             lsb,
             host_ratio);
    fl::cout << line << "\n";
}

inline const char* rgbww_combo_name(int combo) {
    switch (combo) {
    case 0: return "RG+warm";
    case 1: return "RB+warm";
    case 2: return "GB+warm";
    case 3: return "RG+cool";
    case 4: return "RB+cool";
    case 5: return "GB+cool";
    case 6: return "R+warm+cool";
    case 7: return "G+warm+cool";
    case 8: return "B+warm+cool";
    default: return "unknown";
    }
}

inline RgbwwPolicyAuditStats evaluate_rgbww_strict_policy_audit(
    const DiodeProfile& profile, int steps) {
    RgbwwPolicyAuditStats stats;
    if (steps <= 1) {
        return stats;
    }

    strategy_rgbww_strict_5ch::setup(profile);
    const bool native_input =
        is_native_input_gamut(strategy_rgbww_strict_5ch::g_state.profile.warm_path);

    for (int ri = 0; ri < steps; ++ri) {
        const fl::u8 r_in = cube_value(ri, steps);
        for (int gi = 0; gi < steps; ++gi) {
            const fl::u8 g_in = cube_value(gi, steps);
            for (int bi = 0; bi < steps; ++bi) {
                const fl::u8 b_in = cube_value(bi, steps);
                ++stats.sample_count;
                const u8 value = fl::max(fl::max(r_in, g_in), b_in);
                if (value == 0) {
                    ++stats.off_count;
                    continue;
                }
                const int active_count =
                    (r_in > 0 ? 1 : 0) + (g_in > 0 ? 1 : 0) + (b_in > 0 ? 1 : 0);
                if (native_input && active_count == 1) {
                    ++stats.native_single_count;
                    continue;
                }

                float X[3];
                strategy_rgbww_strict_5ch::source_full_chroma_xyz(
                    r_in, g_in, b_in, value, X);

                float endpoint[5];
                if (native_input && active_count == 2) {
                    int active[2];
                    int n = 0;
                    if (r_in > 0) active[n++] = 0;
                    if (g_in > 0) active[n++] = 1;
                    if (b_in > 0) active[n++] = 2;
                    if (strategy_rgbww_strict_5ch::solve_ls2_endpoint(
                            X, active, endpoint)) {
                        ++stats.ls2_count;
                    } else {
                        ++stats.fallback_count;
                    }
                    continue;
                }

                const int combo =
                    strategy_rgbww_strict_5ch::solve_endpoint_combo_index(
                        X, endpoint);
                if (combo >= 0 && combo < strategy_rgbww_strict_5ch::kMaxCombos) {
                    ++stats.combo_count[combo];
                } else {
                    ++stats.fallback_count;
                }
            }
        }
    }

    strategy_rgbww_strict_5ch::teardown();
    return stats;
}

inline RgbwwPolicyAuditStats evaluate_rgbww_strict_fixed_policy_audit(
    const DiodeProfile& profile, int steps) {
    RgbwwPolicyAuditStats stats;
    if (steps <= 1) {
        return stats;
    }

    strategy_rgbww_strict_5ch_fixed_eval::setup(profile);
    const bool native_input =
        is_native_input_gamut(
            strategy_rgbww_strict_5ch_fixed_eval::g_state.profile.warm_path);

    for (int ri = 0; ri < steps; ++ri) {
        const fl::u8 r_in = cube_value(ri, steps);
        for (int gi = 0; gi < steps; ++gi) {
            const fl::u8 g_in = cube_value(gi, steps);
            for (int bi = 0; bi < steps; ++bi) {
                const fl::u8 b_in = cube_value(bi, steps);
                ++stats.sample_count;
                const u8 value = fl::max(fl::max(r_in, g_in), b_in);
                if (value == 0) {
                    ++stats.off_count;
                    continue;
                }
                const int active_count =
                    (r_in > 0 ? 1 : 0) + (g_in > 0 ? 1 : 0) + (b_in > 0 ? 1 : 0);
                if (native_input && active_count == 1) {
                    ++stats.native_single_count;
                    continue;
                }

                i32 X[3];
                strategy_rgbww_strict_5ch_fixed_eval::source_full_chroma_q(
                    r_in, g_in, b_in, value, X);

                i32 endpoint[5];
                if (native_input && active_count == 2) {
                    int active[2];
                    int n = 0;
                    if (r_in > 0) active[n++] = 0;
                    if (g_in > 0) active[n++] = 1;
                    if (b_in > 0) active[n++] = 2;
                    if (strategy_rgbww_strict_5ch_fixed_eval::solve_ls2_endpoint_q(
                            X, active, endpoint)) {
                        ++stats.ls2_count;
                    } else {
                        ++stats.fallback_count;
                    }
                    continue;
                }

                const int combo =
                    strategy_rgbww_strict_5ch_fixed_eval::solve_endpoint_q_combo_index(
                        X, endpoint);
                if (combo >= 0
                    && combo < strategy_rgbww_strict_5ch_fixed_eval::kMaxCombos) {
                    ++stats.combo_count[combo];
                } else {
                    ++stats.fallback_count;
                }
            }
        }
    }

    strategy_rgbww_strict_5ch_fixed_eval::teardown();
    return stats;
}

inline i32 rgbww_policy_audit_total(const RgbwwPolicyAuditStats& stats) {
    i32 total = stats.off_count + stats.native_single_count
              + stats.ls2_count + stats.fallback_count;
    for (int i = 0; i < strategy_rgbww_strict_5ch::kMaxCombos; ++i) {
        total += stats.combo_count[i];
    }
    return total;
}

inline i32 rgbww_policy_route_count(const RgbwwPolicyAuditStats& stats,
                                    int route) {
    switch (route) {
    case -4: return stats.off_count;
    case -3: return stats.native_single_count;
    case -2: return stats.ls2_count;
    case -1: return stats.fallback_count;
    default:
        if (route >= 0 && route < strategy_rgbww_strict_5ch::kMaxCombos) {
            return stats.combo_count[route];
        }
        return 0;
    }
}

inline int rgbww_route_slot_for_code(int route) {
    switch (route) {
    case -4: return 0;
    case -3: return 1;
    case -2: return 2;
    case -1: return 12;
    default:
        if (route >= 0 && route < strategy_rgbww_strict_5ch::kMaxCombos) {
            return 3 + route;
        }
        return 12;
    }
}

inline int rgbww_route_code_for_slot(int slot) {
    switch (slot) {
    case 0: return -4;
    case 1: return -3;
    case 2: return -2;
    case 12: return -1;
    default:
        if (slot >= 3 && slot < 12) {
            return slot - 3;
        }
        return -1;
    }
}

inline const char* rgbww_route_name_for_slot(int slot) {
    switch (slot) {
    case 0: return "off";
    case 1: return "native-single";
    case 2: return "two-ch LS2";
    case 12: return "fallback";
    default:
        return rgbww_combo_name(slot - 3);
    }
}

struct FixtureDriftExample {
    int row_index = -1;
    u32 delta = 0;
    u8 live_r = 0;
    u8 live_g = 0;
    u8 live_b = 0;
    u8 live_w = 0;
    u8 fixture_r = 0;
    u8 fixture_g = 0;
    u8 fixture_b = 0;
    u8 fixture_w = 0;
};

struct FixtureDriftClassificationStats {
    i32 sample_count = 0;
    i32 nonzero_count = 0;
    i32 live_value_preserved_count = 0;
    i32 legacy_value_expanded_count = 0;
    i32 legacy_value_expanded_live_preserved_count = 0;
    i32 legacy_value_expanded_ok_count = 0;
    i32 legacy_value_expanded_fail_count = 0;
    i32 top_drift_legacy_value_expanded_count = 0;
    i32 max_live_value_delta = 0;
    i32 max_legacy_value_delta = 0;
};

static constexpr int kFixtureDriftTopCount = 8;

static constexpr i32 kFixtureValuePreserveTolerance = 1;

static constexpr i32 kLegacyValueExpansionThreshold = 32;

inline const char* verifier_status_name(VerifierRow::Status status) {
    switch (status) {
    case VerifierRow::kOk:
        return "ok";
    case VerifierRow::kFail:
        return "fail";
    case VerifierRow::kSkip:
        return "skip";
    }
    return "unknown";
}

inline void rgb16_string(u16 r, u16 g, u16 b, char out[40]) {
    fl::snprintf(out, 40, "%lu/%lu/%lu",
             static_cast<unsigned long>(r),
             static_cast<unsigned long>(g),
             static_cast<unsigned long>(b));
}

inline void rgbw8_string(u8 r, u8 g, u8 b, u8 w, char out[32]) {
    fl::snprintf(out, 32, "%u/%u/%u/%u",
             static_cast<unsigned>(r),
             static_cast<unsigned>(g),
             static_cast<unsigned>(b),
             static_cast<unsigned>(w));
}

inline u8 max3_u8(u8 a, u8 b, u8 c) {
    return fl::max(fl::max(a, b), c);
}

inline u8 max4_u8(u8 a, u8 b, u8 c, u8 d) {
    return fl::max(fl::max(a, b), fl::max(c, d));
}

inline i32 abs_delta_u8(u8 a, u8 b) {
    return a > b ? static_cast<i32>(a - b) : static_cast<i32>(b - a);
}

inline bool fixture_example_has_legacy_value_expansion(
    const FixtureDriftExample& example) {
    if (example.row_index < 0) {
        return false;
    }
    const VerifierRow& row = kVerifierRows[example.row_index];
    const u8 input_value = max3_u8(row_u16_to_u8(row.r_in16),
                                  row_u16_to_u8(row.g_in16),
                                  row_u16_to_u8(row.b_in16));
    const u8 live_value = max4_u8(example.live_r, example.live_g,
                                  example.live_b, example.live_w);
    const u8 legacy_value = max4_u8(example.fixture_r, example.fixture_g,
                                    example.fixture_b, example.fixture_w);
    const bool live_preserved =
        abs_delta_u8(live_value, input_value) <= kFixtureValuePreserveTolerance;
    const bool legacy_expanded =
        legacy_value > input_value + kLegacyValueExpansionThreshold;
    return live_preserved && legacy_expanded;
}

inline void insert_fixture_drift_example(
    FixtureDriftExample top[kFixtureDriftTopCount],
    const FixtureDriftExample& candidate) {
    for (int slot = 0; slot < kFixtureDriftTopCount; ++slot) {
        if (candidate.delta <= top[slot].delta) {
            continue;
        }
        for (int move = kFixtureDriftTopCount - 1; move > slot; --move) {
            top[move] = top[move - 1];
        }
        top[slot] = candidate;
        return;
    }
}

inline FixtureDriftClassificationStats classify_closed_form_fixture_drift(
    const Strategy& closed_form,
    const DiodeProfile& profile,
    const FixtureDriftExample top[kFixtureDriftTopCount]) {
    FixtureDriftClassificationStats stats;

    if (closed_form.setup != nullptr) {
        closed_form.setup(profile);
    }

    for (int i = 0; i < kVerifierRowCount; ++i) {
        const VerifierRow& row = kVerifierRows[i];
        ++stats.sample_count;
        const u8 r_in = row_u16_to_u8(row.r_in16);
        const u8 g_in = row_u16_to_u8(row.g_in16);
        const u8 b_in = row_u16_to_u8(row.b_in16);
        const u8 input_value = max3_u8(r_in, g_in, b_in);
        if (input_value == 0) {
            continue;
        }
        ++stats.nonzero_count;

        u8 live_r = 0;
        u8 live_g = 0;
        u8 live_b = 0;
        u8 live_w = 0;
        closed_form.solve(r_in, g_in, b_in, &live_r, &live_g, &live_b, &live_w);
        const u8 live_value = max4_u8(live_r, live_g, live_b, live_w);
        const u8 legacy_value = max4_u8(row_u16_to_u8(row.cf_r16),
                                        row_u16_to_u8(row.cf_g16),
                                        row_u16_to_u8(row.cf_b16),
                                        row_u16_to_u8(row.cf_w16));
        const i32 live_delta = abs_delta_u8(live_value, input_value);
        const i32 legacy_delta = abs_delta_u8(legacy_value, input_value);
        if (live_delta > stats.max_live_value_delta) {
            stats.max_live_value_delta = live_delta;
        }
        if (legacy_delta > stats.max_legacy_value_delta) {
            stats.max_legacy_value_delta = legacy_delta;
        }
        const bool live_preserved =
            live_delta <= kFixtureValuePreserveTolerance;
        const bool legacy_expanded =
            legacy_value > input_value + kLegacyValueExpansionThreshold;
        if (live_preserved) {
            ++stats.live_value_preserved_count;
        }
        if (legacy_expanded) {
            ++stats.legacy_value_expanded_count;
            if (row.status == VerifierRow::kOk) {
                ++stats.legacy_value_expanded_ok_count;
            } else if (row.status == VerifierRow::kFail) {
                ++stats.legacy_value_expanded_fail_count;
            }
            if (live_preserved) {
                ++stats.legacy_value_expanded_live_preserved_count;
            }
        }
    }

    if (closed_form.teardown != nullptr) {
        closed_form.teardown();
    }

    for (int i = 0; i < kFixtureDriftTopCount; ++i) {
        if (fixture_example_has_legacy_value_expansion(top[i])) {
            ++stats.top_drift_legacy_value_expanded_count;
        }
    }
    return stats;
}

inline void collect_closed_form_fixture_drift_examples(
    const Strategy& closed_form,
    const DiodeProfile& profile,
    FixtureDriftExample top[kFixtureDriftTopCount]) {
    for (int i = 0; i < kFixtureDriftTopCount; ++i) {
        top[i] = FixtureDriftExample{};
    }

    if (closed_form.setup != nullptr) {
        closed_form.setup(profile);
    }

    for (int i = 0; i < kVerifierRowCount; ++i) {
        const VerifierRow& row = kVerifierRows[i];
        const u8 r_in = row_u16_to_u8(row.r_in16);
        const u8 g_in = row_u16_to_u8(row.g_in16);
        const u8 b_in = row_u16_to_u8(row.b_in16);
        FixtureDriftExample candidate;
        candidate.row_index = i;
        closed_form.solve(r_in, g_in, b_in,
                          &candidate.live_r,
                          &candidate.live_g,
                          &candidate.live_b,
                          &candidate.live_w);
        candidate.fixture_r = row_u16_to_u8(row.cf_r16);
        candidate.fixture_g = row_u16_to_u8(row.cf_g16);
        candidate.fixture_b = row_u16_to_u8(row.cf_b16);
        candidate.fixture_w = row_u16_to_u8(row.cf_w16);
        candidate.delta = max_channel_lsb_delta(
            candidate.live_r,
            candidate.live_g,
            candidate.live_b,
            candidate.live_w,
            candidate.fixture_r,
            candidate.fixture_g,
            candidate.fixture_b,
            candidate.fixture_w);
        insert_fixture_drift_example(top, candidate);
    }

    if (closed_form.teardown != nullptr) {
        closed_form.teardown();
    }
}

inline void print_fixture_drift_line(const char* scope,
                                     const ErrorStats& stats,
                                     const char* meaning) {
    char lsb[48];
    char mean[16];
    lsb_stats_string(stats, lsb);
    mean_lsb_string(stats, mean);

    char line[512];
    fl::snprintf(line, sizeof(line),
             "%-12s | %7ld | %31s | %8s | %s",
             scope,
             static_cast<long>(stats.sample_count),
             lsb,
             mean,
             meaning);
    fl::cout << line << "\n";
}

inline void print_fixture_classification_line(const char* signal,
                                              i32 count,
                                              i32 total,
                                              const char* meaning) {
    char share[24];
    percent_string(static_cast<u32>(count), static_cast<u32>(total), share);
    char line[512];
    fl::snprintf(line, sizeof(line),
             "%-36s | %7ld/%-7ld | %8s | %s",
             signal,
             static_cast<long>(count),
             static_cast<long>(total),
             share,
             meaning);
    fl::cout << line << "\n";
}

inline void print_fixture_drift_example_line(
    int rank,
    const FixtureDriftExample& example) {
    if (example.row_index < 0) {
        return;
    }
    const VerifierRow& row = kVerifierRows[example.row_index];
    char input[40];
    char live[32];
    char fixture[32];
    rgb16_string(row.r_in16, row.g_in16, row.b_in16, input);
    rgbw8_string(example.live_r, example.live_g, example.live_b,
                 example.live_w, live);
    rgbw8_string(example.fixture_r, example.fixture_g, example.fixture_b,
                 example.fixture_w, fixture);

    char line[768];
    fl::snprintf(line, sizeof(line),
             "%4ld | %-20s | %-5s | %-20s | %-15s | %-15s | %5lu",
             static_cast<long>(rank),
             row.patch,
             verifier_status_name(row.status),
             input,
             live,
             fixture,
             static_cast<unsigned long>(example.delta));
    fl::cout << line << "\n";
}

inline void rgbww_quantize_float_endpoint(const float endpoint[5],
                                          u8 value,
                                          u8* out_r,
                                          u8* out_g,
                                          u8* out_b,
                                          u8* out_ww,
                                          u8* out_wc) {
    const float scale = static_cast<float>(value) * (1.0f / 255.0f);
    *out_r = quantize_u8(endpoint[0] * scale);
    *out_g = quantize_u8(endpoint[1] * scale);
    *out_b = quantize_u8(endpoint[2] * scale);
    *out_ww = quantize_u8(endpoint[3] * scale);
    *out_wc = quantize_u8(endpoint[4] * scale);
}

inline int solve_rgbww_strict_float_route(
    u8 r, u8 g, u8 b, bool native_input,
    u8* out_r, u8* out_g, u8* out_b, u8* out_ww, u8* out_wc) {
    const u8 value = fl::max(fl::max(r, g), b);
    if (value == 0) {
        *out_r = *out_g = *out_b = *out_ww = *out_wc = 0;
        return -4;
    }
    const int active_count = (r > 0 ? 1 : 0) + (g > 0 ? 1 : 0) + (b > 0 ? 1 : 0);
    if (native_input && active_count == 1) {
        *out_r = r;
        *out_g = g;
        *out_b = b;
        *out_ww = *out_wc = 0;
        return -3;
    }

    float X[3];
    strategy_rgbww_strict_5ch::source_full_chroma_xyz(r, g, b, value, X);
    float endpoint[5];
    if (native_input && active_count == 2) {
        int active[2];
        int n = 0;
        if (r > 0) active[n++] = 0;
        if (g > 0) active[n++] = 1;
        if (b > 0) active[n++] = 2;
        if (strategy_rgbww_strict_5ch::solve_ls2_endpoint(X, active, endpoint)) {
            rgbww_quantize_float_endpoint(
                endpoint, value, out_r, out_g, out_b, out_ww, out_wc);
            return -2;
        }
        rgbww_layered_reference_u8(r, g, b, out_r, out_g, out_b, out_ww, out_wc);
        return -1;
    }

    const int combo =
        strategy_rgbww_strict_5ch::solve_endpoint_combo_index(X, endpoint);
    if (combo >= 0 && combo < strategy_rgbww_strict_5ch::kMaxCombos) {
        rgbww_quantize_float_endpoint(
            endpoint, value, out_r, out_g, out_b, out_ww, out_wc);
        return combo;
    }
    rgbww_layered_reference_u8(r, g, b, out_r, out_g, out_b, out_ww, out_wc);
    return -1;
}

inline int solve_rgbww_strict_fixed_route(
    u8 r, u8 g, u8 b, bool native_input,
    u8* out_r, u8* out_g, u8* out_b, u8* out_ww, u8* out_wc) {
    const u8 value = fl::max(fl::max(r, g), b);
    if (value == 0) {
        *out_r = *out_g = *out_b = *out_ww = *out_wc = 0;
        return -4;
    }
    const int active_count = (r > 0 ? 1 : 0) + (g > 0 ? 1 : 0) + (b > 0 ? 1 : 0);
    if (native_input && active_count == 1) {
        *out_r = r;
        *out_g = g;
        *out_b = b;
        *out_ww = *out_wc = 0;
        return -3;
    }

    i32 X[3];
    strategy_rgbww_strict_5ch_fixed_eval::source_full_chroma_q(
        r, g, b, value, X);
    i32 endpoint[5];
    if (native_input && active_count == 2) {
        int active[2];
        int n = 0;
        if (r > 0) active[n++] = 0;
        if (g > 0) active[n++] = 1;
        if (b > 0) active[n++] = 2;
        if (strategy_rgbww_strict_5ch_fixed_eval::solve_ls2_endpoint_q(
                X, active, endpoint)) {
            *out_r = strategy_rgbww_strict_5ch_fixed_eval::q_to_scaled_u8(
                endpoint[0], value);
            *out_g = strategy_rgbww_strict_5ch_fixed_eval::q_to_scaled_u8(
                endpoint[1], value);
            *out_b = strategy_rgbww_strict_5ch_fixed_eval::q_to_scaled_u8(
                endpoint[2], value);
            *out_ww = strategy_rgbww_strict_5ch_fixed_eval::q_to_scaled_u8(
                endpoint[3], value);
            *out_wc = strategy_rgbww_strict_5ch_fixed_eval::q_to_scaled_u8(
                endpoint[4], value);
            return -2;
        }
        rgbww_layered_reference_u8(r, g, b, out_r, out_g, out_b, out_ww, out_wc);
        return -1;
    }

    const int combo =
        strategy_rgbww_strict_5ch_fixed_eval::solve_endpoint_q_combo_index(
            X, endpoint);
    if (combo >= 0 && combo < strategy_rgbww_strict_5ch_fixed_eval::kMaxCombos) {
        *out_r = strategy_rgbww_strict_5ch_fixed_eval::q_to_scaled_u8(
            endpoint[0], value);
        *out_g = strategy_rgbww_strict_5ch_fixed_eval::q_to_scaled_u8(
            endpoint[1], value);
        *out_b = strategy_rgbww_strict_5ch_fixed_eval::q_to_scaled_u8(
            endpoint[2], value);
        *out_ww = strategy_rgbww_strict_5ch_fixed_eval::q_to_scaled_u8(
            endpoint[3], value);
        *out_wc = strategy_rgbww_strict_5ch_fixed_eval::q_to_scaled_u8(
            endpoint[4], value);
        return combo;
    }
    rgbww_layered_reference_u8(r, g, b, out_r, out_g, out_b, out_ww, out_wc);
    return -1;
}

inline RgbwwRouteErrorAuditStats evaluate_rgbww_strict_route_error_audit(
    const DiodeProfile& profile, int steps, bool fixed_eval) {
    RgbwwRouteErrorAuditStats stats;
    if (steps <= 1) {
        return stats;
    }

    fl::vector<fl::u32> deltas[kRgbwwRouteSlotCount];
    for (int i = 0; i < kRgbwwRouteSlotCount; ++i) {
        deltas[i].reserve(steps * steps);
    }

    bool native_input = false;
    if (fixed_eval) {
        strategy_rgbww_strict_5ch_fixed_eval::setup(profile);
        native_input = is_native_input_gamut(
            strategy_rgbww_strict_5ch_fixed_eval::g_state.profile.warm_path);
    } else {
        strategy_rgbww_strict_5ch::setup(profile);
        native_input =
            is_native_input_gamut(strategy_rgbww_strict_5ch::g_state.profile.warm_path);
    }

    for (int ri = 0; ri < steps; ++ri) {
        const fl::u8 r_in = cube_value(ri, steps);
        for (int gi = 0; gi < steps; ++gi) {
            const fl::u8 g_in = cube_value(gi, steps);
            for (int bi = 0; bi < steps; ++bi) {
                const fl::u8 b_in = cube_value(bi, steps);
                ++stats.sample_count;
                u8 strict_r = 0, strict_g = 0, strict_b = 0;
                u8 strict_ww = 0, strict_wc = 0;
                const int route = fixed_eval
                    ? solve_rgbww_strict_fixed_route(
                        r_in, g_in, b_in, native_input,
                        &strict_r, &strict_g, &strict_b, &strict_ww, &strict_wc)
                    : solve_rgbww_strict_float_route(
                        r_in, g_in, b_in, native_input,
                        &strict_r, &strict_g, &strict_b, &strict_ww, &strict_wc);
                u8 ref_r = 0, ref_g = 0, ref_b = 0;
                u8 ref_ww = 0, ref_wc = 0;
                rgbww_layered_reference_u8(
                    r_in, g_in, b_in, &ref_r, &ref_g, &ref_b, &ref_ww, &ref_wc);
                deltas[rgbww_route_slot_for_code(route)].push_back(
                    max_channel_lsb_delta5(
                        strict_r, strict_g, strict_b, strict_ww, strict_wc,
                        ref_r, ref_g, ref_b, ref_ww, ref_wc));
            }
        }
    }

    if (fixed_eval) {
        strategy_rgbww_strict_5ch_fixed_eval::teardown();
    } else {
        strategy_rgbww_strict_5ch::teardown();
    }

    for (int i = 0; i < kRgbwwRouteSlotCount; ++i) {
        stats.route[i] = finalize_lsb_deltas(deltas[i]);
    }
    return stats;
}

inline bool rgbww_combo_has_white(int combo) {
    if (combo < 0 || combo >= strategy_rgbww_strict_5ch::kMaxCombos) {
        return false;
    }
    switch (combo) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
        return true;
    default:
        return false;
    }
}

inline bool rgbww_combo_uses_warm_only(int combo) {
    return combo >= 0 && combo <= 2;
}

inline bool rgbww_combo_uses_cool_only(int combo) {
    return combo >= 3 && combo <= 5;
}

inline bool rgbww_combo_uses_warm_and_cool(int combo) {
    return combo >= 6 && combo <= 8;
}

inline RgbwwPhysicalPolicyStats evaluate_rgbww_physical_policy_audit(
    const DiodeProfile& profile, int steps) {
    RgbwwPhysicalPolicyStats stats;
    if (steps <= 1) {
        return stats;
    }

    strategy_rgbww_strict_5ch::setup(profile);
    strategy_rgbww_strict_5ch_fixed_eval::setup(profile);
    const bool native_float =
        is_native_input_gamut(strategy_rgbww_strict_5ch::g_state.profile.warm_path);
    const bool native_fixed =
        is_native_input_gamut(
            strategy_rgbww_strict_5ch_fixed_eval::g_state.profile.warm_path);

    fl::vector<fl::u32> fixed_deltas;
    fixed_deltas.reserve(steps * steps * steps);

    for (int ri = 0; ri < steps; ++ri) {
        const fl::u8 r_in = cube_value(ri, steps);
        for (int gi = 0; gi < steps; ++gi) {
            const fl::u8 g_in = cube_value(gi, steps);
            for (int bi = 0; bi < steps; ++bi) {
                const fl::u8 b_in = cube_value(bi, steps);
                ++stats.sample_count;

                u8 fr = 0, fg = 0, fb = 0, fww = 0, fwc = 0;
                u8 qr = 0, qg = 0, qb = 0, qww = 0, qwc = 0;
                const int float_route = solve_rgbww_strict_float_route(
                    r_in, g_in, b_in, native_float, &fr, &fg, &fb, &fww, &fwc);
                const int fixed_route = solve_rgbww_strict_fixed_route(
                    r_in, g_in, b_in, native_fixed, &qr, &qg, &qb, &qww, &qwc);
                fixed_deltas.push_back(max_channel_lsb_delta5(
                    fr, fg, fb, fww, fwc, qr, qg, qb, qww, qwc));
                if (float_route != fixed_route) {
                    ++stats.fixed_route_mismatch_count;
                }

                if (float_route == -1) {
                    ++stats.fallback_count;
                } else if (float_route == -3) {
                    ++stats.native_single_samples;
                    if (fww != 0 || fwc != 0) {
                        ++stats.native_single_white_violations;
                    }
                } else if (float_route == -2) {
                    ++stats.two_channel_samples;
                    if (fww != 0 || fwc != 0) {
                        ++stats.two_channel_white_violations;
                    }
                } else if (float_route >= 0) {
                    ++stats.combo_samples;
                    if (!rgbww_combo_has_white(float_route)) {
                        ++stats.combo_without_white_count;
                    }
                    if (rgbww_combo_uses_warm_only(float_route)) {
                        ++stats.warm_only_combo_count;
                    } else if (rgbww_combo_uses_cool_only(float_route)) {
                        ++stats.cool_only_combo_count;
                    } else if (rgbww_combo_uses_warm_and_cool(float_route)) {
                        ++stats.warm_cool_combo_count;
                    }
                }
            }
        }
    }

    strategy_rgbww_strict_5ch_fixed_eval::teardown();
    strategy_rgbww_strict_5ch::teardown();
    stats.fixed_output_delta = finalize_lsb_deltas(fixed_deltas);
    return stats;
}

inline void print_rgbww_physical_policy_audit_line(
    const RgbwwPhysicalPolicyStats& stats) {
    char combo_share[24];
    char fallback_share[24];
    char warm_only_share[24];
    char cool_only_share[24];
    char warm_cool_share[24];
    char fixed_lsb[48];
    percent_string(static_cast<u32>(stats.combo_samples),
                   static_cast<u32>(stats.sample_count),
                   combo_share);
    percent_string(static_cast<u32>(stats.fallback_count),
                   static_cast<u32>(stats.sample_count),
                   fallback_share);
    percent_string(static_cast<u32>(stats.warm_only_combo_count),
                   static_cast<u32>(stats.combo_samples),
                   warm_only_share);
    percent_string(static_cast<u32>(stats.cool_only_combo_count),
                   static_cast<u32>(stats.combo_samples),
                   cool_only_share);
    percent_string(static_cast<u32>(stats.warm_cool_combo_count),
                   static_cast<u32>(stats.combo_samples),
                   warm_cool_share);
    lsb_stats_string(stats.fixed_output_delta, fixed_lsb);

    char line[1024];
    fl::snprintf(line, sizeof(line),
             "%7ld | %13ld | %15ld | %14ld | %9s | %8ld | %8s | %11ld | %10s | %14ld | %13s | %13ld | %14ld | %27s",
             static_cast<long>(stats.sample_count),
             static_cast<long>(stats.native_single_white_violations),
             static_cast<long>(stats.two_channel_white_violations),
             static_cast<long>(stats.combo_samples),
             combo_share,
             static_cast<long>(stats.fallback_count),
             fallback_share,
             static_cast<long>(stats.warm_only_combo_count),
             warm_only_share,
             static_cast<long>(stats.cool_only_combo_count),
             cool_only_share,
             static_cast<long>(stats.warm_cool_combo_count),
             static_cast<long>(stats.fixed_route_mismatch_count),
             fixed_lsb);
    fl::cout << line << "\n";
}

inline void print_rgbww_policy_line(const char* route,
                                    i32 count,
                                    i32 sample_count,
                                    const char* meaning) {
    char pct[24];
    percent_string(static_cast<u32>(count), static_cast<u32>(sample_count), pct);
    char line[512];
    fl::snprintf(line, sizeof(line),
             "%-14s | %7ld | %8s | %s",
             route,
             static_cast<long>(count),
             pct,
             meaning);
    fl::cout << line << "\n";
}

inline void print_rgbww_policy_parity_line(const char* route,
                                           int route_index,
                                           const RgbwwPolicyAuditStats& fpu,
                                           const RgbwwPolicyAuditStats& fixed) {
    const i32 fpu_count = rgbww_policy_route_count(fpu, route_index);
    const i32 fixed_count = rgbww_policy_route_count(fixed, route_index);
    const i32 delta = fixed_count - fpu_count;
    char fpu_pct[24];
    char fixed_pct[24];
    percent_string(static_cast<u32>(fpu_count), static_cast<u32>(fpu.sample_count),
                   fpu_pct);
    percent_string(static_cast<u32>(fixed_count),
                   static_cast<u32>(fixed.sample_count), fixed_pct);
    char line[512];
    fl::snprintf(line, sizeof(line),
             "%-14s | %9ld | %8s | %11ld | %9s | %+6ld",
             route,
             static_cast<long>(fpu_count),
             fpu_pct,
             static_cast<long>(fixed_count),
             fixed_pct,
             static_cast<long>(delta));
    fl::cout << line << "\n";
}

inline i32 rgbww_route_error_total(const RgbwwRouteErrorAuditStats& stats) {
    i32 total = 0;
    for (int i = 0; i < kRgbwwRouteSlotCount; ++i) {
        total += stats.route[i].sample_count;
    }
    return total;
}

inline void print_rgbww_route_error_line(
    int slot,
    i32 total_samples,
    const ErrorStats& fpu,
    const ErrorStats& fixed) {
    char share[24];
    char fpu_lsb[48];
    char fixed_lsb[48];
    char fpu_mean[16];
    char fixed_mean[16];
    percent_string(static_cast<u32>(fpu.sample_count),
                   static_cast<u32>(total_samples), share);
    lsb_stats_string(fpu, fpu_lsb);
    lsb_stats_string(fixed, fixed_lsb);
    mean_lsb_string(fpu, fpu_mean);
    mean_lsb_string(fixed, fixed_mean);

    char line[768];
    fl::snprintf(line, sizeof(line),
             "%-14s | %7ld | %8s | %31s | %8s | %31s | %8s",
             rgbww_route_name_for_slot(slot),
             static_cast<long>(fpu.sample_count),
             share,
             fpu_lsb,
             fpu_mean,
             fixed_lsb,
             fixed_mean);
    fl::cout << line << "\n";
}

FL_TEST_CASE("colorimetric strategy comparison table") {
    DiodeProfile p = kRgbwDefaultProfile;
    set_input_gamut(p, InputGamut::Native);

    ErrorStats cf_stats[kStrategyCount];
    ErrorStats target_stats[kStrategyCount];
    ErrorStats pair_stats[kStrategyCount];
    ErrorStats policy_stats[kStrategyCount];
    ErrorStats cube_stats[kStrategyCount];
    ErrorStats cube_policy_stats[kStrategyCount];
    MonotonicStats monotonic_stats[kStrategyCount];
    Q16RangeStats q16_range_stats[kStrategyCount];
    Q16StaticBoundStats q16_static_bounds[kStrategyCount];
    RgbwwPolicyAuditStats rgbww_policy_stats;
    RgbwwPolicyAuditStats rgbww_fixed_policy_stats;
    RgbwwRouteErrorAuditStats rgbww_route_error_stats;
    RgbwwRouteErrorAuditStats rgbww_fixed_route_error_stats;
    RgbwwPhysicalPolicyStats rgbww_physical_policy_stats;
    HybridSeedAuditStats hybrid_seed_stats;
    BenchStats benches[kStrategyCount];
    MemoryInfo memories[kStrategyCount];
    static constexpr int kCubeSteps = 17;
    static constexpr int kCubeSamples = kCubeSteps * kCubeSteps * kCubeSteps;
    static constexpr int kLutGridSweepCount = 3;
    const int lut_grid_sweep_n[kLutGridSweepCount] = {8, 16, 32};
    ErrorStats bilinear_grid_sweep[kLutGridSweepCount];
    ErrorStats hermite_grid_sweep[kLutGridSweepCount];
    BenchStats bilinear_grid_bench[kLutGridSweepCount];
    BenchStats hermite_grid_bench[kLutGridSweepCount];
    ErrorStats cell_float_grid_sweep[kLutGridSweepCount];
    ErrorStats cell_fixed_grid_sweep[kLutGridSweepCount];
    BenchStats cell_float_grid_bench[kLutGridSweepCount];
    BenchStats cell_fixed_grid_bench[kLutGridSweepCount];
    ErrorStats hull_float_grid_sweep[kLutGridSweepCount];
    ErrorStats hull_fixed_grid_sweep[kLutGridSweepCount];
    BenchStats hull_float_grid_bench[kLutGridSweepCount];
    BenchStats hull_fixed_grid_bench[kLutGridSweepCount];
    ErrorStats tetra_i16_grid_sweep[kLutGridSweepCount];
    ErrorStats tetra_u8_grid_sweep[kLutGridSweepCount];
    BenchStats tetra_i16_grid_bench[kLutGridSweepCount];
    BenchStats tetra_u8_grid_bench[kLutGridSweepCount];
    ErrorStats tetra_boost_i16_grid_sweep[kLutGridSweepCount];
    ErrorStats tetra_boost_u8_grid_sweep[kLutGridSweepCount];
    BenchStats tetra_boost_i16_grid_bench[kLutGridSweepCount];
    BenchStats tetra_boost_u8_grid_bench[kLutGridSweepCount];
    TetraCompressionStats tetra_strict_compression[kLutGridSweepCount];
    TetraCompressionStats tetra_boost_compression[kLutGridSweepCount];
    static constexpr int kScaleDirectionSteps = 16;
    static constexpr int kScaleSteps = 17;
    static constexpr int kScaleRays =
        kScaleDirectionSteps * kScaleDirectionSteps * kScaleDirectionSteps - 1;
    for (int i = 0; i < kStrategyCount; ++i) {
        const Strategy& strategy = kStrategies[i];
        memories[i] = strategy.memory_info();
        cf_stats[i] = evaluate_strategy(
            strategy, kStrategies[kClosedFormIndex],
            p, Reference::ClosedForm, RowFilter::All);
        target_stats[i] = evaluate_strategy(
            strategy, kStrategies[kClosedFormIndex],
            p, Reference::TargetChroma, RowFilter::All);
        const int pair_index = paired_accuracy_index_for(i);
        pair_stats[i] = pair_index >= 0
            ? evaluate_strategy_pair_lsb(strategy, kStrategies[pair_index],
                                         p, RowFilter::All)
            : ErrorStats{};
        if (pair_index >= 0) {
            FL_CHECK(pair_stats[i].sample_count == kVerifierRowCount);
            FL_CHECK(cpu_pair_covers_indices(i, pair_index));
        }
        const int policy_index = policy_accuracy_index_for(i);
        policy_stats[i] = policy_index >= 0
            ? evaluate_strategy_pair_lsb(strategy, kStrategies[policy_index],
                                         p, RowFilter::All)
            : ErrorStats{};
        if (policy_index >= 0) {
            FL_CHECK(policy_stats[i].sample_count == kVerifierRowCount);
        }
        const int cube_reference_index =
            strategy.solve5 != nullptr ? kRgbwwLayeredIndex : kClosedFormIndex;
        cube_stats[i] = evaluate_strategy_cube_lsb(
            strategy, kStrategies[cube_reference_index], p, kCubeSteps);
        FL_CHECK(cube_stats[i].sample_count == kCubeSamples);
        cube_policy_stats[i] = policy_index >= 0
            ? evaluate_strategy_cube_lsb(strategy, kStrategies[policy_index],
                                         p, kCubeSteps)
            : ErrorStats{};
        if (policy_index >= 0) {
            FL_CHECK(cube_policy_stats[i].sample_count == kCubeSamples);
        }
        monotonic_stats[i] = evaluate_strategy_scale_monotonicity(
            strategy, p, kScaleDirectionSteps, kScaleSteps);
        FL_CHECK(monotonic_stats[i].ray_count == kScaleRays);
        FL_CHECK(monotonic_stats[i].comparison_count > 0);
        q16_range_stats[i] = evaluate_q16_range_audit(i, p, kCubeSteps);
        if (has_q16_range_audit(i)) {
            FL_CHECK(q16_range_stats[i].measured);
            FL_CHECK(q16_range_stats[i].sample_count == kCubeSamples);
            FL_CHECK(q16_range_stats[i].max_abs_product > 0);
            FL_CHECK(q16_range_stats[i].max_abs_accumulator > 0);
            FL_CHECK(q16_range_stats[i].max_abs_output_q > 0);
            FL_CHECK(q16_range_stats[i].divisor_count > 0);
            FL_CHECK(q16_range_stats[i].min_abs_divisor > 0);
            FL_CHECK(q16_range_stats[i].zero_divisor_count == 0);
            if (q16_range_stats[i].max_abs_det > 0) {
                FL_CHECK(q16_range_stats[i].min_abs_det > 0);
            }
            q16_static_bounds[i] = evaluate_q16_static_bounds(i, p);
            FL_CHECK(q16_static_bounds[i].measured);
            FL_CHECK(q16_static_bounds[i].max_abs_coeff_q > 0);
            FL_CHECK(q16_static_bounds[i].max_matvec_product > 0.0);
            FL_CHECK(q16_static_bounds[i].max_matvec_accumulator > 0.0);
            FL_CHECK(q16_static_bounds[i].max_bound > 0.0);
            FL_CHECK(q16_range_stats[i].max_abs_product
                     <= q16_static_bounds[i].max_bound);
        }
        benches[i] = benchmark_strategy(strategy, p);
    }
    const ErrorStats public_dispatch_fixture_stats =
        evaluate_public_dispatch_fixture_parity(
            kStrategies[kClosedFormIndex],
            RGBW_MODE::kRGBWColorimetric,
            p,
            RowFilter::All);
    const ErrorStats public_dispatch_cube_stats =
        evaluate_public_dispatch_cube_parity(
            kStrategies[kClosedFormIndex],
            RGBW_MODE::kRGBWColorimetric,
            p,
            kCubeSteps);
    const ErrorStats public_boosted_fixture_stats =
        evaluate_public_dispatch_fixture_parity(
            kStrategies[kWxOverdriveIndex],
            RGBW_MODE::kRGBWColorimetricBoosted,
            p,
            RowFilter::All);
    const ErrorStats public_boosted_cube_stats =
        evaluate_public_dispatch_cube_parity(
            kStrategies[kWxOverdriveIndex],
            RGBW_MODE::kRGBWColorimetricBoosted,
            p,
            kCubeSteps);
    FL_CHECK(public_dispatch_fixture_stats.sample_count == kVerifierRowCount);
    FL_CHECK(public_dispatch_cube_stats.sample_count == kCubeSamples);
    FL_CHECK(public_dispatch_fixture_stats.peak_lsb == 0);
    FL_CHECK(public_dispatch_cube_stats.peak_lsb == 0);
    FL_CHECK(public_boosted_fixture_stats.sample_count == kVerifierRowCount);
    FL_CHECK(public_boosted_cube_stats.sample_count == kCubeSamples);
    FL_CHECK(public_boosted_fixture_stats.peak_lsb == 0);
    FL_CHECK(public_boosted_cube_stats.peak_lsb == 0);
    const ErrorStats closed_form_fixture_stats =
        evaluate_closed_form_fixture_lock(
            kStrategies[kClosedFormIndex], p, RowFilter::All);
    const ErrorStats closed_form_fixture_ok_stats =
        evaluate_closed_form_fixture_lock(
            kStrategies[kClosedFormIndex], p, RowFilter::OkOnly);
    const ErrorStats closed_form_fixture_fail_stats =
        evaluate_closed_form_fixture_lock(
            kStrategies[kClosedFormIndex], p, RowFilter::FailOnly);
    FixtureDriftExample fixture_drift_examples[kFixtureDriftTopCount];
    collect_closed_form_fixture_drift_examples(
        kStrategies[kClosedFormIndex], p, fixture_drift_examples);
    const FixtureDriftClassificationStats fixture_drift_classification =
        classify_closed_form_fixture_drift(
            kStrategies[kClosedFormIndex], p, fixture_drift_examples);
    FL_CHECK(closed_form_fixture_stats.sample_count == kVerifierRowCount);
    FL_CHECK(closed_form_fixture_stats.peak_lsb
             >= closed_form_fixture_stats.p95_lsb);
    FL_CHECK(closed_form_fixture_ok_stats.sample_count > 0);
    FL_CHECK(closed_form_fixture_fail_stats.sample_count > 0);
    FL_CHECK(fixture_drift_examples[0].delta
             == static_cast<u32>(closed_form_fixture_stats.peak_lsb));
    FL_CHECK(fixture_drift_classification.sample_count == kVerifierRowCount);
    FL_CHECK(fixture_drift_classification.nonzero_count > 0);
    FL_CHECK(fixture_drift_classification.live_value_preserved_count > 0);
    FL_CHECK(fixture_drift_classification.legacy_value_expanded_count > 0);
    FL_CHECK(fixture_drift_classification.top_drift_legacy_value_expanded_count > 0);

    for (int i = 0; i < kCpuPairCount; ++i) {
        const CpuPair& pair = kCpuPairs[i];
        FL_CHECK(has_text(pair.label));
        FL_CHECK(has_text(pair.scope));
        FL_CHECK(!has_placeholder_wording(pair.label));
        FL_CHECK(!has_placeholder_wording(pair.scope));
        FL_CHECK(pair.fpu_index >= 0 && pair.fpu_index < kStrategyCount);
        FL_CHECK(pair.fixed_index >= 0 && pair.fixed_index < kStrategyCount);
        FL_CHECK(benches[pair.fpu_index].measured);
        FL_CHECK(benches[pair.fixed_index].measured);
        FL_CHECK(pair_stats[pair.fixed_index].sample_count == kVerifierRowCount);
        char pair_mem_delta[32];
        char pair_mem_ratio[32];
        signed_memory_delta_string(
            memories[pair.fixed_index].target_packed_bytes,
            memories[pair.fpu_index].target_packed_bytes,
            pair_mem_delta);
        memory_ratio_string(
            memories[pair.fixed_index].target_packed_bytes,
            memories[pair.fpu_index].target_packed_bytes,
            pair_mem_ratio);
        FL_CHECK(has_text(pair_mem_delta));
        FL_CHECK(has_text(pair_mem_ratio));
        FL_CHECK(!has_placeholder_wording(pair_mem_delta));
        FL_CHECK(!has_placeholder_wording(pair_mem_ratio));
    }
    for (int i = 0; i < kLutGridSweepCount; ++i) {
        bilinear_grid_sweep[i] = evaluate_uniform_lut_grid_cube_lsb(
            lut_grid_sweep_n[i], LutInterp::Bilinear, p, kCubeSteps);
        hermite_grid_sweep[i] = evaluate_uniform_lut_grid_cube_lsb(
            lut_grid_sweep_n[i], LutInterp::Hermite, p, kCubeSteps);
        bilinear_grid_bench[i] = benchmark_uniform_lut_grid(
            lut_grid_sweep_n[i], LutInterp::Bilinear, p);
        hermite_grid_bench[i] = benchmark_uniform_lut_grid(
            lut_grid_sweep_n[i], LutInterp::Hermite, p);
        FL_CHECK(bilinear_grid_sweep[i].sample_count == kCubeSamples);
        FL_CHECK(hermite_grid_sweep[i].sample_count == kCubeSamples);
        FL_CHECK(bilinear_grid_bench[i].measured);
        FL_CHECK(hermite_grid_bench[i].measured);
        cell_float_grid_sweep[i] = evaluate_experimental_hermite_grid_cube_lsb(
            lut_grid_sweep_n[i], false, false, p, kCubeSteps);
        cell_fixed_grid_sweep[i] = evaluate_experimental_hermite_grid_cube_lsb(
            lut_grid_sweep_n[i], false, true, p, kCubeSteps);
        cell_float_grid_bench[i] = benchmark_experimental_hermite_grid(
            lut_grid_sweep_n[i], false, false, p);
        cell_fixed_grid_bench[i] = benchmark_experimental_hermite_grid(
            lut_grid_sweep_n[i], false, true, p);
        hull_float_grid_sweep[i] = evaluate_experimental_hermite_grid_cube_lsb(
            lut_grid_sweep_n[i], true, false, p, kCubeSteps);
        hull_fixed_grid_sweep[i] = evaluate_experimental_hermite_grid_cube_lsb(
            lut_grid_sweep_n[i], true, true, p, kCubeSteps);
        hull_float_grid_bench[i] = benchmark_experimental_hermite_grid(
            lut_grid_sweep_n[i], true, false, p);
        hull_fixed_grid_bench[i] = benchmark_experimental_hermite_grid(
            lut_grid_sweep_n[i], true, true, p);
        FL_CHECK(cell_float_grid_sweep[i].sample_count == kCubeSamples);
        FL_CHECK(cell_fixed_grid_sweep[i].sample_count == kCubeSamples);
        FL_CHECK(hull_float_grid_sweep[i].sample_count == kCubeSamples);
        FL_CHECK(hull_fixed_grid_sweep[i].sample_count == kCubeSamples);
        FL_CHECK(cell_float_grid_bench[i].measured);
        FL_CHECK(cell_fixed_grid_bench[i].measured);
        FL_CHECK(hull_float_grid_bench[i].measured);
        FL_CHECK(hull_fixed_grid_bench[i].measured);
        tetra_i16_grid_sweep[i] = evaluate_tetra_grid_cube_lsb(
            lut_grid_sweep_n[i], TetraGridStorage::I16, p,
            kCubeSteps, false, false);
        tetra_u8_grid_sweep[i] = evaluate_tetra_grid_cube_lsb(
            lut_grid_sweep_n[i], TetraGridStorage::U8, p,
            kCubeSteps, false, false);
        tetra_i16_grid_bench[i] = benchmark_tetra_grid(
            lut_grid_sweep_n[i], TetraGridStorage::I16, p, false);
        tetra_u8_grid_bench[i] = benchmark_tetra_grid(
            lut_grid_sweep_n[i], TetraGridStorage::U8, p, false);
        tetra_boost_i16_grid_sweep[i] = evaluate_tetra_grid_cube_lsb(
            lut_grid_sweep_n[i], TetraGridStorage::I16, p,
            kCubeSteps, true, true);
        tetra_boost_u8_grid_sweep[i] = evaluate_tetra_grid_cube_lsb(
            lut_grid_sweep_n[i], TetraGridStorage::U8, p,
            kCubeSteps, true, true);
        tetra_boost_i16_grid_bench[i] = benchmark_tetra_grid(
            lut_grid_sweep_n[i], TetraGridStorage::I16, p, true);
        tetra_boost_u8_grid_bench[i] = benchmark_tetra_grid(
            lut_grid_sweep_n[i], TetraGridStorage::U8, p, true);
        FL_CHECK(tetra_i16_grid_sweep[i].sample_count == kCubeSamples);
        FL_CHECK(tetra_u8_grid_sweep[i].sample_count == kCubeSamples);
        FL_CHECK(tetra_i16_grid_bench[i].measured);
        FL_CHECK(tetra_u8_grid_bench[i].measured);
        FL_CHECK(tetra_boost_i16_grid_sweep[i].sample_count == kCubeSamples);
        FL_CHECK(tetra_boost_u8_grid_sweep[i].sample_count == kCubeSamples);
        FL_CHECK(tetra_boost_i16_grid_bench[i].measured);
        FL_CHECK(tetra_boost_u8_grid_bench[i].measured);
        tetra_strict_compression[i] = evaluate_tetra_compression_signal(
            lut_grid_sweep_n[i], false, p);
        tetra_boost_compression[i] = evaluate_tetra_compression_signal(
            lut_grid_sweep_n[i], true, p);
        FL_CHECK(tetra_strict_compression[i].node_count
                 == static_cast<u32>(lut_grid_sweep_n[i]
                                     * lut_grid_sweep_n[i]
                                     * lut_grid_sweep_n[i]));
        FL_CHECK(tetra_boost_compression[i].node_count
                 == tetra_strict_compression[i].node_count);
        FL_CHECK(tetra_strict_compression[i].raw_i16_bytes
                 == packed_tetrahedral_i16_bytes(lut_grid_sweep_n[i]));
        FL_CHECK(tetra_strict_compression[i].raw_u8_bytes
                 == packed_tetrahedral_u8_bytes(lut_grid_sweep_n[i]));
        FL_CHECK(tetra_boost_compression[i].raw_i16_bytes
                 == tetra_strict_compression[i].raw_i16_bytes);
        FL_CHECK(tetra_boost_compression[i].raw_u8_bytes
                 == tetra_strict_compression[i].raw_u8_bytes);
    }
    FL_CHECK(cube_stats[kClosedFormIndex].peak_lsb == 0);
    FL_CHECK(cube_stats[kRgbwwLayeredIndex].peak_lsb == 0);
    FL_CHECK(monotonic_stats[kClosedFormIndex].violation_count == 0);
    FL_CHECK(monotonic_stats[kRgbwwLayeredIndex].violation_count == 0);
    FL_CHECK(pair_stats[kHybridQ16Index].peak_lsb == 0);
    FL_CHECK(pair_stats[kRgbwwStrictIndex].peak_lsb <= 1);
    FL_CHECK(pair_stats[kRgbwwStrictFixedIndex].peak_lsb <= 1);
    FL_CHECK(bilinear_grid_sweep[1].p95_lsb == cube_stats[kBilinearN16Index].p95_lsb);
    FL_CHECK(bilinear_grid_sweep[1].peak_lsb == cube_stats[kBilinearN16Index].peak_lsb);
    FL_CHECK(hermite_grid_sweep[1].p95_lsb == cube_stats[kHermiteN16Index].p95_lsb);
    FL_CHECK(hermite_grid_sweep[1].peak_lsb == cube_stats[kHermiteN16Index].peak_lsb);
    FL_CHECK(hermite_grid_sweep[2].p95_lsb == cube_stats[kHermiteN32Index].p95_lsb);
    FL_CHECK(hermite_grid_sweep[2].peak_lsb == cube_stats[kHermiteN32Index].peak_lsb);
    FL_CHECK(cell_float_grid_sweep[1].p95_lsb
             == cube_stats[kCellEncodingIndex].p95_lsb);
    FL_CHECK(cell_float_grid_sweep[1].peak_lsb
             == cube_stats[kCellEncodingIndex].peak_lsb);
    FL_CHECK(cell_fixed_grid_sweep[1].p95_lsb
             == cube_stats[kCellEncodingFixedIndex].p95_lsb);
    FL_CHECK(cell_fixed_grid_sweep[1].peak_lsb
             == cube_stats[kCellEncodingFixedIndex].peak_lsb);
    FL_CHECK(hull_float_grid_sweep[1].p95_lsb
             == cube_stats[kHullAdaptiveIndex].p95_lsb);
    FL_CHECK(hull_float_grid_sweep[1].peak_lsb
             == cube_stats[kHullAdaptiveIndex].peak_lsb);
    FL_CHECK(hull_fixed_grid_sweep[1].p95_lsb
             == cube_stats[kHullAdaptiveFixedIndex].p95_lsb);
    FL_CHECK(hull_fixed_grid_sweep[1].peak_lsb
             == cube_stats[kHullAdaptiveFixedIndex].peak_lsb);
    FL_CHECK(tetra_i16_grid_sweep[0].p95_lsb == cube_stats[kTetrahedralIndex].p95_lsb);
    FL_CHECK(tetra_i16_grid_sweep[0].peak_lsb == cube_stats[kTetrahedralIndex].peak_lsb);
    FL_CHECK(tetra_u8_grid_sweep[0].p95_lsb == cube_stats[kTetrahedralU8Index].p95_lsb);
    FL_CHECK(tetra_u8_grid_sweep[0].peak_lsb == cube_stats[kTetrahedralU8Index].peak_lsb);
    FL_CHECK(tetra_boost_i16_grid_sweep[0].p95_lsb
             == cube_policy_stats[kTetrahedralBoostIndex].p95_lsb);
    FL_CHECK(tetra_boost_i16_grid_sweep[0].peak_lsb
             == cube_policy_stats[kTetrahedralBoostIndex].peak_lsb);
    FL_CHECK(tetra_boost_u8_grid_sweep[0].p95_lsb
             == cube_policy_stats[kTetrahedralBoostU8Index].p95_lsb);
    FL_CHECK(tetra_boost_u8_grid_sweep[0].peak_lsb
             == cube_policy_stats[kTetrahedralBoostU8Index].peak_lsb);
    hybrid_seed_stats = evaluate_hybrid_seed_router_audit(p, kCubeSteps);
    FL_CHECK(hybrid_seed_stats.sample_count == kCubeSamples);
    FL_CHECK(hybrid_seed_stats.off_count
             + hybrid_seed_stats.native_single_count
             + hybrid_seed_stats.ls2_count
             + hybrid_seed_stats.interior_count == kCubeSamples);
    FL_CHECK(hybrid_seed_stats.seed_direct_count
             + hybrid_seed_stats.seed_fallback_count
             == hybrid_seed_stats.interior_count);
    FL_CHECK(hybrid_seed_stats.output_delta.sample_count == kCubeSamples);
    FL_CHECK(hybrid_seed_stats.output_delta.peak_lsb == 0);
    rgbww_policy_stats = evaluate_rgbww_strict_policy_audit(p, kCubeSteps);
    rgbww_fixed_policy_stats =
        evaluate_rgbww_strict_fixed_policy_audit(p, kCubeSteps);
    rgbww_route_error_stats =
        evaluate_rgbww_strict_route_error_audit(p, kCubeSteps, false);
    rgbww_fixed_route_error_stats =
        evaluate_rgbww_strict_route_error_audit(p, kCubeSteps, true);
    rgbww_physical_policy_stats =
        evaluate_rgbww_physical_policy_audit(p, kCubeSteps);
    FL_CHECK(rgbww_policy_stats.sample_count == kCubeSamples);
    FL_CHECK(rgbww_fixed_policy_stats.sample_count == kCubeSamples);
    FL_CHECK(rgbww_policy_audit_total(rgbww_policy_stats) == kCubeSamples);
    FL_CHECK(rgbww_policy_audit_total(rgbww_fixed_policy_stats) == kCubeSamples);
    FL_CHECK(rgbww_route_error_stats.sample_count == kCubeSamples);
    FL_CHECK(rgbww_fixed_route_error_stats.sample_count == kCubeSamples);
    FL_CHECK(rgbww_route_error_total(rgbww_route_error_stats) == kCubeSamples);
    FL_CHECK(rgbww_route_error_total(rgbww_fixed_route_error_stats)
             == kCubeSamples);
    for (int slot = 0; slot < kRgbwwRouteSlotCount; ++slot) {
        const int route = rgbww_route_code_for_slot(slot);
        FL_CHECK(rgbww_route_error_stats.route[slot].sample_count
                 == rgbww_policy_route_count(rgbww_policy_stats, route));
        FL_CHECK(rgbww_fixed_route_error_stats.route[slot].sample_count
                 == rgbww_policy_route_count(rgbww_fixed_policy_stats, route));
    }
    FL_CHECK(rgbww_physical_policy_stats.sample_count == kCubeSamples);
    FL_CHECK(rgbww_physical_policy_stats.fallback_count
             == rgbww_policy_stats.fallback_count);
    FL_CHECK(rgbww_physical_policy_stats.native_single_samples
             == rgbww_policy_stats.native_single_count);
    FL_CHECK(rgbww_physical_policy_stats.two_channel_samples
             == rgbww_policy_stats.ls2_count);
    FL_CHECK(rgbww_physical_policy_stats.native_single_white_violations == 0);
    FL_CHECK(rgbww_physical_policy_stats.two_channel_white_violations == 0);
    FL_CHECK(rgbww_physical_policy_stats.combo_without_white_count == 0);
    FL_CHECK(rgbww_physical_policy_stats.fixed_output_delta.sample_count
             == kCubeSamples);

    i32 best_rgbw_peak = 1000000;
    i32 best_rgbw_approx_peak = 1000000;
    float best_rgbw_target = 1000000.0f;
    float best_rgbw_approx_target = 1000000.0f;
    const char* best_rgbw_peak_name = "";
    const char* best_rgbw_approx_peak_name = "";
    const char* best_rgbw_target_name = "";
    const char* best_rgbw_approx_target_name = "";
    const ErrorStats hermite_n16_stats = cf_stats[kHermiteN16Index];

    for (int i = 0; i < kStrategyCount; ++i) {
        const Strategy& strategy = kStrategies[i];
        if (strategy.solve != nullptr && cf_stats[i].peak_lsb < best_rgbw_peak) {
            best_rgbw_peak = cf_stats[i].peak_lsb;
            best_rgbw_peak_name = strategy.name;
        }
        if (strategy.solve != nullptr && strategy.budget_gated
            && cf_stats[i].peak_lsb > 0
            && cf_stats[i].peak_lsb < best_rgbw_approx_peak) {
            best_rgbw_approx_peak = cf_stats[i].peak_lsb;
            best_rgbw_approx_peak_name = strategy.name;
        }
        if (strategy.solve != nullptr && target_stats[i].p95_dE < best_rgbw_target) {
            best_rgbw_target = target_stats[i].p95_dE;
            best_rgbw_target_name = strategy.name;
        }
        if (strategy.solve != nullptr && strategy.budget_gated
            && cf_stats[i].peak_lsb > 0
            && target_stats[i].p95_dE < best_rgbw_approx_target) {
            best_rgbw_approx_target = target_stats[i].p95_dE;
            best_rgbw_approx_target_name = strategy.name;
        }
    }

    fl::cout << "\n=== Colorimetric Strategy Accuracy Table ===\n";
    fl::cout << "Strategy                  | Stage  | Topology | Reference/role                  | max-channel LSB p50/p95/p99/peak | paired LSB p50/p95/p99/peak | policy LSB p50/p95/p99/peak | mean LSB | target dE p50/p95/peak/mean | Samples | Sample set                         | Status     | vs Hermite N=16\n";
    fl::cout << "--------------------------|--------|----------|---------------------------------|----------------------------------|-----------------------------|-----------------------------|----------|-----------------------------|---------|------------------------------------|------------|-----------------\n";

    for (int i = 0; i < kStrategyCount; ++i) {
        const Strategy& strategy = kStrategies[i];
        char lsb[48];
        char pair_lsb[48];
        char policy_lsb[48];
        char mean_lsb[16];
        char de[64];
        char rel[64];
        lsb_stats_string(cf_stats[i], lsb);
        lsb_stats_string(pair_stats[i], pair_lsb);
        lsb_stats_string(policy_stats[i], policy_lsb);
        mean_lsb_string(cf_stats[i], mean_lsb);
        target_de_string(target_stats[i], de);
        relative_string(strategy, memories[i], cf_stats[i], hermite_n16_stats, rel);
        const char* status = status_for(strategy, cf_stats[i], target_stats[i],
                                        kBudgets[i][0], kBudgets[i][1]);

        char line[1024];
        fl::snprintf(line, sizeof(line),
                 "%-26s| %-6s | %-8s | %-31s | %32s | %27s | %27s | %8s | %27s | %7ld | %-34s | %-10s | %s",
                 strategy.name,
                 strategy.stage,
                 strategy.topology,
                 reference_name_for(i),
                 lsb,
                 pair_lsb,
                 policy_lsb,
                 mean_lsb,
                 de,
                 static_cast<long>(cf_stats[i].sample_count),
                 sample_set_for(i),
                 status,
                 rel);
        fl::cout << line << "\n";
    }

    fl::cout << "\n=== Uniform RGB Cube LSB Sweep ===\n";
    fl::cout << "Strategy                  | Topology | Reference/role                  | cube17 LSB p50/p95/p99/peak | mean LSB | Samples | Sample set\n";
    fl::cout << "--------------------------|----------|---------------------------------|-----------------------------|----------|---------|--------------------\n";
    for (int i = 0; i < kStrategyCount; ++i) {
        char lsb[48];
        char mean_lsb[16];
        lsb_stats_string(cube_stats[i], lsb);
        mean_lsb_string(cube_stats[i], mean_lsb);
        const char* sample_set =
            kStrategies[i].solve5 != nullptr ? "cube17/native/RGBWW"
                                             : "cube17/native/RGBW";
        char line[1024];
        fl::snprintf(line, sizeof(line),
                 "%-26s| %-8s | %-31s | %27s | %8s | %7ld | %s",
                 kStrategies[i].name,
                 kStrategies[i].topology,
                 reference_name_for(i),
                 lsb,
                 mean_lsb,
                 static_cast<long>(cube_stats[i].sample_count),
                 sample_set);
        fl::cout << line << "\n";
    }

    fl::cout << "\n=== Uniform RGB Cube Policy LSB Sweep ===\n";
    fl::cout << "Strategy                  | Policy target             | cube17 policy LSB p50/p95/p99/peak | mean LSB | Samples | Sample set\n";
    fl::cout << "--------------------------|---------------------------|------------------------------------|----------|---------|--------------------\n";
    for (int i = 0; i < kStrategyCount; ++i) {
        const int policy_index = policy_accuracy_index_for(i);
        if (policy_index < 0) continue;
        char lsb[48];
        char mean_lsb[16];
        lsb_stats_string(cube_policy_stats[i], lsb);
        mean_lsb_string(cube_policy_stats[i], mean_lsb);
        char line[1024];
        fl::snprintf(line, sizeof(line),
                 "%-26s| %-25s | %34s | %8s | %7ld | cube17/native/policy",
                 kStrategies[i].name,
                 kStrategies[policy_index].name,
                 lsb,
                 mean_lsb,
                 static_cast<long>(cube_policy_stats[i].sample_count));
        fl::cout << line << "\n";
    }

    fl::cout << "\n=== RGBWW Strict Policy Choice Audit ===\n";
    fl::cout << "Route          | Samples | Share    | Meaning\n";
    fl::cout << "---------------|---------|----------|-------------------------------\n";
    print_rgbww_policy_line(
        "off", rgbww_policy_stats.off_count, rgbww_policy_stats.sample_count,
        "zero RGB input");
    print_rgbww_policy_line(
        "native-single", rgbww_policy_stats.native_single_count,
        rgbww_policy_stats.sample_count,
        "native one-channel passthrough, no white");
    print_rgbww_policy_line(
        "two-ch LS2", rgbww_policy_stats.ls2_count,
        rgbww_policy_stats.sample_count,
        "native two-channel least-squares endpoint");
    for (int i = 0; i < strategy_rgbww_strict_5ch::kMaxCombos; ++i) {
        print_rgbww_policy_line(
            rgbww_combo_name(i),
            rgbww_policy_stats.combo_count[i],
            rgbww_policy_stats.sample_count,
            "three-channel combo selected by max white");
    }
    print_rgbww_policy_line(
        "fallback", rgbww_policy_stats.fallback_count,
        rgbww_policy_stats.sample_count,
        "layered RGBWW fallback after strict solve miss");
    fl::cout << "RGBWW strict policy audit uses cube17/native/RGBW inputs "
                "and records the route selected by the float 5-channel strict "
                "solver before output quantization. It is route evidence, not "
                "physical validation by itself.\n";

    fl::cout << "\n=== RGBWW Strict Fixed Policy Parity Audit ===\n";
    fl::cout << "Route          | FPU count | FPU share | Fixed count | Fixed share | Delta\n";
    fl::cout << "---------------|-----------|-----------|-------------|-------------|-------\n";
    print_rgbww_policy_parity_line(
        "off", -4, rgbww_policy_stats, rgbww_fixed_policy_stats);
    print_rgbww_policy_parity_line(
        "native-single", -3, rgbww_policy_stats, rgbww_fixed_policy_stats);
    print_rgbww_policy_parity_line(
        "two-ch LS2", -2, rgbww_policy_stats, rgbww_fixed_policy_stats);
    for (int i = 0; i < strategy_rgbww_strict_5ch::kMaxCombos; ++i) {
        print_rgbww_policy_parity_line(
            rgbww_combo_name(i), i, rgbww_policy_stats, rgbww_fixed_policy_stats);
    }
    print_rgbww_policy_parity_line(
        "fallback", -1, rgbww_policy_stats, rgbww_fixed_policy_stats);
    fl::cout << "RGBWW fixed policy parity audit compares route counts before "
                "output quantization. Nonzero deltas mean the Q16 evaluator "
                "chose a different physical combo for some cube17 inputs even "
                "if output LSB parity remains small.\n";

    fl::cout << "\n=== RGBWW Strict Physical Policy Invariant Audit ===\n";
    fl::cout << "Samples | native white violations | two-ch white violations | Combo samples | Combo %  | Fallback | Fallback % | Warm-only | Warm-only % | Cool-only | Cool-only % | Warm+cool | Fixed route mismatches | fixed/FPU LSB p50/p95/p99/peak\n";
    fl::cout << "--------|-------------------------|-------------------------|---------------|----------|----------|------------|-----------|-------------|-----------|-------------|-----------|------------------------|-----------------------------\n";
    print_rgbww_physical_policy_audit_line(rgbww_physical_policy_stats);
    fl::cout << "RGBWW physical policy invariant audit checks the strict solver's "
                "current route policy on cube17/native/RGBW: native one-channel "
                "and two-channel routes must emit no warm/cool white, strict "
                "combo routes must be one of the explicit white-containing "
                "triples, strict fallback remains visible, and the fixed Q16 "
                "route is compared against the float route. This is host "
                "policy validation, not LED-bin or hardware validation.\n";

    fl::cout << "\n=== RGBWW Strict Route Error Audit ===\n";
    fl::cout << "Route          | Samples | Share    | FPU LSB p50/p95/p99/peak | mean LSB | Fixed LSB p50/p95/p99/peak | mean LSB\n";
    fl::cout << "---------------|---------|----------|---------------------------|----------|-----------------------------|----------\n";
    for (int slot = 0; slot < kRgbwwRouteSlotCount; ++slot) {
        print_rgbww_route_error_line(
            slot,
            rgbww_route_error_stats.sample_count,
            rgbww_route_error_stats.route[slot],
            rgbww_fixed_route_error_stats.route[slot]);
    }
    fl::cout << "RGBWW route error audit scores each strict route against the "
                "layered RGBWW reference using max absolute LSB error across "
                "all five output channels. Empty routes print n/a; fallback "
                "routes would compare as zero when they call the layered "
                "reference directly.\n";

    fl::cout << "\n=== Scale-Ray Monotonicity Audit ===\n";
    fl::cout << "Strategy                  | Topology | Scale rays | Comparisons | Violating rays | Drops | Max drop LSB | Summary\n";
    fl::cout << "--------------------------|----------|------------|-------------|----------------|-------|--------------|------------------------------\n";
    for (int i = 0; i < kStrategyCount; ++i) {
        char summary[48];
        monotonic_string(monotonic_stats[i], summary);
        char line[1024];
        fl::snprintf(line, sizeof(line),
                 "%-26s| %-8s | %10ld | %11ld | %14ld | %5ld | %12ld | %s",
                 kStrategies[i].name,
                 kStrategies[i].topology,
                 static_cast<long>(monotonic_stats[i].ray_count),
                 static_cast<long>(monotonic_stats[i].comparison_count),
                 static_cast<long>(monotonic_stats[i].violating_rays),
                 static_cast<long>(monotonic_stats[i].violation_count),
                 static_cast<long>(monotonic_stats[i].max_drop_lsb),
                 summary);
        fl::cout << line << "\n";
    }

    fl::cout << "\n=== Q16 Solver Integer Range Audit ===\n";
    fl::cout << "Strategy                  | Samples | Fallbacks | Seed fallbacks | max src q | max coeff q | max product | max acc | max out q | max det | min det | zero det | max numerator | max endpoint q | min divisor | divisions | zero div | i64 numerator headroom | Evidence\n";
    fl::cout << "--------------------------|---------|-----------|----------------|-----------|-------------|-------------|---------|-----------|---------|---------|----------|---------------|----------------|-------------|-----------|----------|------------------------|---------\n";
    for (int i = 0; i < kStrategyCount; ++i) {
        if (!has_q16_range_audit(i)) continue;
        char src[32];
        char coeff[32];
        char product[32];
        char acc[32];
        char out_q[32];
        char det[32];
        char min_det[32];
        char numerator[32];
        char endpoint[32];
        char min_divisor[32];
        char headroom[32];
        i64_value_string(q16_range_stats[i].max_abs_src_q, src);
        i64_value_string(q16_range_stats[i].max_abs_coeff_q, coeff);
        i64_value_string(q16_range_stats[i].max_abs_product, product);
        i64_value_string(q16_range_stats[i].max_abs_accumulator, acc);
        i64_value_string(q16_range_stats[i].max_abs_output_q, out_q);
        i64_value_string(q16_range_stats[i].max_abs_det, det);
        i64_value_string(q16_range_stats[i].min_abs_det, min_det);
        i64_value_string(q16_range_stats[i].max_abs_numerator, numerator);
        i64_value_string(q16_range_stats[i].max_abs_endpoint_q, endpoint);
        i64_value_string(q16_range_stats[i].min_abs_divisor, min_divisor);
        i64_headroom_string(q16_range_stats[i].max_abs_numerator, headroom);
        char line[2048];
        fl::snprintf(line, sizeof(line),
                 "%-26s| %7ld | %9ld | %14ld | %9s | %11s | %11s | %7s | %9s | %7s | %7s | %8ld | %13s | %14s | %11s | %9ld | %8ld | %22s | %s",
                 kStrategies[i].name,
                 static_cast<long>(q16_range_stats[i].sample_count),
                 static_cast<long>(q16_range_stats[i].fallback_count),
                 static_cast<long>(q16_range_stats[i].seeded_fallback_count),
                 src,
                 coeff,
                 product,
                 acc,
                 out_q,
                 det,
                 min_det,
                 static_cast<long>(q16_range_stats[i].zero_det_count),
                 numerator,
                 endpoint,
                 min_divisor,
                 static_cast<long>(q16_range_stats[i].divisor_count),
                 static_cast<long>(q16_range_stats[i].zero_divisor_count),
                 headroom,
                 q16_range_evidence_for(i));
        fl::cout << line << "\n";
    }

    fl::cout << "\n=== Q16 Static Coefficient Bound Audit ===\n";
    fl::cout << "Strategy                  | max coeff q | source X bound q | matvec product bound | matvec acc bound | LS2 numerator bound | i64 headroom | Evidence\n";
    fl::cout << "--------------------------|-------------|------------------|----------------------|------------------|---------------------|--------------|---------\n";
    for (int i = 0; i < kStrategyCount; ++i) {
        if (!has_q16_range_audit(i)) continue;
        char coeff[32];
        char source[40];
        char product[40];
        char acc[40];
        char numerator[40];
        char headroom[32];
        i64_value_string(q16_static_bounds[i].max_abs_coeff_q, coeff);
        double_value_string(q16_static_bounds[i].max_source_x_q, source);
        double_value_string(q16_static_bounds[i].max_matvec_product, product);
        double_value_string(q16_static_bounds[i].max_matvec_accumulator, acc);
        double_value_string(q16_static_bounds[i].max_ls2_scaled_numerator, numerator);
        double_headroom_string(q16_static_bounds[i].max_bound, headroom);
        char line[1280];
        fl::snprintf(line, sizeof(line),
                 "%-26s| %11s | %16s | %20s | %16s | %19s | %12s | %s",
                 kStrategies[i].name,
                 coeff,
                 source,
                 product,
                 acc,
                 numerator,
                 headroom,
                 q16_static_bounds[i].evidence);
        fl::cout << line << "\n";
    }
    fl::cout << "Static coefficient bounds use the current quantized profile "
                "constants and assume each normalized RGB source component is "
                "within [0,Q16]. They bound matrix products/accumulators and "
                "the LS2 scaled numerator shape, but they are not a complete "
                "projection or division-safety proof by themselves.\n";

    fl::cout << "\n=== Hybrid Q16 Seed Router Audit ===\n";
    fl::cout << "Samples | off | native-single | LS2  | interior | seed RGW/RBW/BGW/full | direct hits | direct % | fallbacks     | fallback % | output diffs | fixed-vs-hybrid LSB p50/p95/p99/peak | Host ratio\n";
    fl::cout << "--------|-----|---------------|------|----------|-----------------------|-------------|----------|---------------|------------|--------------|---------------------------------------|-----------\n";
    print_hybrid_seed_audit_line(
        hybrid_seed_stats,
        benches[kFixedQ16Index],
        benches[kHybridQ16Index]);
    fl::cout << "Hybrid seed routing covers only the 3-channel interior solve. "
                "Single-channel native passthrough and two-channel LS2 routes "
                "bypass the router. The output-diff column compares "
                "hybrid_q16_refine_n8 directly against fixed_analytical_q16 "
                "on cube17/native/RGBW, so zero means the seed router changed "
                "path cost evidence without changing output accuracy.\n";

    fl::cout << "\n=== Colorimetric Strategy Memory Table ===\n";
    fl::cout << "Strategy                  | Target packed | Prototype storage | Runtime RAM | Per-profile | Per-controller | Tiny tier                                    | Compression/scaling knobs                                     | Source\n";
    fl::cout << "--------------------------|---------------|-------------------|-------------|-------------|----------------|----------------------------------------------|---------------------------------------------------------------|-------\n";
    for (int i = 0; i < kStrategyCount; ++i) {
        char target[24];
        char proto[24];
        char runtime[24];
        char per_profile[24];
        char per_controller[24];
        memory_string(memories[i].target_packed_bytes, target);
        memory_string(memories[i].prototype_storage_bytes, proto);
        memory_string(memories[i].runtime_ram_bytes, runtime);
        memory_string(memories[i].per_profile_bytes, per_profile);
        memory_string(memories[i].per_controller_bytes, per_controller);
        char line[1536];
        fl::snprintf(line, sizeof(line),
                 "%-26s| %13s | %17s | %11s | %11s | %14s | %-44s | %-61s | %s",
                 kStrategies[i].name,
                 target,
                 proto,
                 runtime,
                 per_profile,
                 per_controller,
                 memories[i].tiny_tier,
                 memories[i].scaling,
                 memories[i].source);
        fl::cout << line << "\n";
    }

    fl::cout << "\n=== Memory Target vs Prototype Gap Audit ===\n";
    fl::cout << "Strategy                  | Stage    | Target packed | Prototype | Proto-target | Proto/target | Runtime RAM | RAM-target   | RAM/target | Gap class             | Deployment blocker\n";
    fl::cout << "--------------------------|----------|---------------|-----------|--------------|--------------|-------------|--------------|------------|-----------------------|-------------------\n";
    for (int i = 0; i < kStrategyCount; ++i) {
        print_memory_gap_line(i, memories);
    }
    fl::cout << "Memory gap audit is derived from each row's MemoryInfo. "
                "Positive prototype deltas mean the harness representation "
                "is larger than the intended packed deployable form. Runtime "
                "RAM includes current row-owned state/table storage, so rows "
                "with target 0 can still show runtime-only cache or state cost.\n";

    fl::cout << "\n=== Memory Scaling Sweep ===\n";
    fl::cout << "Grid N | Bilinear xy LUT | Hermite xy LUT | Q16 cell packed | Hull-adaptive packed | Tetra RGB cube i16 | Tetra RGB cube u8\n";
    fl::cout << "-------|-----------------|---------------|-----------------|----------------------|--------------------|-------------------\n";
    print_memory_scaling_line(8);
    print_memory_scaling_line(16);
    print_memory_scaling_line(32);
    fl::cout << "Memory sweep values are formula-derived packed target bytes. "
                "They do not imply those grid sizes have separate measured "
                "accuracy rows unless a matching strategy row exists above.\n";

    fl::cout << "\n=== Uniform LUT Grid Accuracy Sweep ===\n";
    fl::cout << "Grid N | Bilinear bytes | Bilinear host ns/px | Bilinear cube17 LSB p50/p95/p99/peak | mean LSB | Hermite bytes | Hermite host ns/px | Hermite cube17 LSB p50/p95/p99/peak | mean LSB | Samples\n";
    fl::cout << "-------|----------------|--------------------|-----------------------------------|----------|---------------|-------------------|---------------------------------|----------|--------\n";
    for (int i = 0; i < kLutGridSweepCount; ++i) {
        print_lut_grid_accuracy_line(
            lut_grid_sweep_n[i],
            bilinear_grid_sweep[i],
            bilinear_grid_bench[i],
            hermite_grid_sweep[i],
            hermite_grid_bench[i]);
    }
    fl::cout << "LUT grid accuracy is measured against strict closed-form on "
                "cube17/native/RGBW. This sweep builds temporary LUTs for "
                "comparison only; registered strategy rows remain the source "
                "for fixture accuracy. Host ns/px is measured here, while "
                "representative MCU cycle cells remain explicit evidence gaps "
                "until board data exists.\n";

    fl::cout << "\n=== Q16 Cell Encoding Grid Sweep ===\n";
    fl::cout << "Grid N | packed bytes | float host ns/px | float cube17 LSB p50/p95/p99/peak | mean LSB | fixed host ns/px | fixed cube17 LSB p50/p95/p99/peak | mean LSB | Samples\n";
    fl::cout << "-------|--------------|------------------|----------------------------------|----------|------------------|----------------------------------|----------|--------\n";
    for (int i = 0; i < kLutGridSweepCount; ++i) {
        print_experimental_hermite_grid_line(
            lut_grid_sweep_n[i],
            false,
            cell_float_grid_sweep[i],
            cell_float_grid_bench[i],
            cell_fixed_grid_sweep[i],
            cell_fixed_grid_bench[i]);
    }
    fl::cout << "Q16 cell grid scaling is measured against strict closed-form "
                "on cube17/native/RGBW with temporary uniform xy Hermite-cell "
                "tables. N=16 is guard-checked against the registered "
                "cell_encoding_q16_n16 rows; other grid sizes are evidence "
                "sweeps, not production rows.\n";

    fl::cout << "\n=== Hull-Adaptive Cell Grid Sweep ===\n";
    fl::cout << "Grid N | packed bytes | float host ns/px | float cube17 LSB p50/p95/p99/peak | mean LSB | fixed host ns/px | fixed cube17 LSB p50/p95/p99/peak | mean LSB | Samples\n";
    fl::cout << "-------|--------------|------------------|----------------------------------|----------|------------------|----------------------------------|----------|--------\n";
    for (int i = 0; i < kLutGridSweepCount; ++i) {
        print_experimental_hermite_grid_line(
            lut_grid_sweep_n[i],
            true,
            hull_float_grid_sweep[i],
            hull_float_grid_bench[i],
            hull_fixed_grid_sweep[i],
            hull_fixed_grid_bench[i]);
    }
    fl::cout << "Hull-adaptive grid scaling is measured against strict "
                "closed-form on cube17/native/RGBW with temporary warped-axis "
                "Hermite-cell tables. N=16 is guard-checked against the "
                "registered hull_adaptive_n16 rows; other grid sizes are "
                "evidence sweeps, not production rows. Packed bytes include "
                "the i16 cell block plus two u8 warp axes.\n";

    fl::cout << "\n=== Tetrahedral RGB Grid Scaling Sweep ===\n";
    fl::cout << "Grid N | i16 bytes | i16 host ns/px | i16 cube17 LSB p50/p95/p99/peak | mean LSB | u8 bytes | u8 host ns/px | u8 cube17 LSB p50/p95/p99/peak | mean LSB | Samples\n";
    fl::cout << "-------|-----------|---------------|-----------------------------|----------|----------|--------------|---------------------------|----------|--------\n";
    for (int i = 0; i < kLutGridSweepCount; ++i) {
        print_tetra_grid_scaling_line(
            lut_grid_sweep_n[i],
            tetra_i16_grid_sweep[i],
            tetra_i16_grid_bench[i],
            tetra_u8_grid_sweep[i],
            tetra_u8_grid_bench[i]);
    }
    fl::cout << "Tetrahedral grid scaling is measured against strict closed-form "
                "on cube17/native/RGBW with temporary strict RGB cube tables. "
                "N=8 is guard-checked against the registered tetrahedral rows; "
                "larger grids are evidence sweeps, not production rows.\n";

    fl::cout << "\n=== Boosted Tetrahedral Policy Grid Sweep ===\n";
    fl::cout << "Grid N | i16 bytes | i16 host ns/px | i16 policy LSB p50/p95/p99/peak | mean LSB | u8 bytes | u8 host ns/px | u8 policy LSB p50/p95/p99/peak | mean LSB | Samples\n";
    fl::cout << "-------|-----------|---------------|-----------------------------|----------|----------|--------------|---------------------------|----------|--------\n";
    for (int i = 0; i < kLutGridSweepCount; ++i) {
        print_tetra_grid_scaling_line(
            lut_grid_sweep_n[i],
            tetra_boost_i16_grid_sweep[i],
            tetra_boost_i16_grid_bench[i],
            tetra_boost_u8_grid_sweep[i],
            tetra_boost_u8_grid_bench[i]);
    }
    fl::cout << "Boosted tetrahedral grid scaling is measured against "
                "wx_overdrive_float on cube17/native/RGBW with temporary "
                "boosted RGB cube tables. N=8 is guard-checked against the "
                "registered boosted tetrahedral policy rows; larger grids are "
                "evidence sweeps, not production rows.\n";

    fl::cout << "\n=== Tetrahedral Cube Compression Signal Audit ===\n";
    fl::cout << "Mode    | Grid N | Nodes | zero channels | unique i16 cells | i16 raw | i16 palette signal | unique u8 cells | u8 raw   | u8 palette signal\n";
    fl::cout << "--------|--------|-------|---------------|------------------|---------|--------------------|-----------------|----------|------------------\n";
    for (int i = 0; i < kLutGridSweepCount; ++i) {
        print_tetra_compression_line(tetra_strict_compression[i]);
        print_tetra_compression_line(tetra_boost_compression[i]);
    }
    fl::cout << "Compression signal uses measured generated cube contents. "
                "Palette signal is unique-cell payload plus a 1/2/4-byte "
                "per-node index, not an implemented production format. It is "
                "evidence for deciding whether palette/sparse storage is worth "
                "pursuing.\n";

    fl::cout << "\n=== Host Paired CPU/Accuracy Cost Audit ===\n";
    fl::cout << "Pair                  | Anchor/FPU row             | Fixed/compare row              | FPU host ns | fixed host ns | fixed/FPU host | fixed/FPU MCU          | FPU mem  | fixed mem | fixed-FPU mem | fixed/FPU mem | paired LSB p50/p95/p99/peak | mean LSB | MCU cycle evidence                      | Scope\n";
    fl::cout << "----------------------|----------------------------|--------------------------------|-------------|---------------|----------------|------------------------|----------|-----------|---------------|---------------|-----------------------------|----------|-----------------------------------------|------\n";
    for (int i = 0; i < kCpuPairCount; ++i) {
        print_cpu_pair_line(kCpuPairs[i], memories, benches, pair_stats);
    }
    fl::cout << "Host paired cost audit compares runnable rows on the same host "
                "benchmark loop and reports max-channel LSB delta between the "
                "paired outputs. These ratios are useful relative evidence, "
                "and the memory deltas compare fixed packed target bytes "
                "against the anchor/FPU row's packed target bytes. Host ratios "
                "are not MCU cycle measurements; the CPU table keeps MCU cycle "
                "cells and fixed/FPU MCU ratios missing until representative "
                "board or simulator data exists.\n";

    fl::cout << "\n=== CPU Evidence Coverage Audit ===\n";
    fl::cout << "Strategy                  | Stage    | host ns/px | FPU cycles/px      | Fixed cycles/px        | MCU evidence status             | fixed/FPU MCU ratio    | Paired host evidence          | Validation\n";
    fl::cout << "--------------------------|----------|------------|--------------------|------------------------|---------------------------------|------------------------|-------------------------------|-----------\n";
    int cpu_complete_count = 0;
    int cpu_partial_count = 0;
    int cpu_missing_count = 0;
    int cpu_rejected_count = 0;
    for (int i = 0; i < kStrategyCount; ++i) {
        print_cpu_evidence_coverage_line(i, benches);
        const bool fpu_measured = cycle_cell_has_measurement(fpu_cycles_for(i));
        const bool fixed_measured = cycle_cell_has_measurement(fixed_cycles_for(i));
        if (text_equals(kStrategies[i].stage, "REJECTED")) {
            ++cpu_rejected_count;
        } else if (fpu_measured && fixed_measured) {
            ++cpu_complete_count;
        } else if (fpu_measured || fixed_measured) {
            ++cpu_partial_count;
        } else {
            ++cpu_missing_count;
        }
    }
    fl::cout << "CPU evidence summary: complete " << cpu_complete_count
             << ", partial " << cpu_partial_count
             << ", missing " << cpu_missing_count
             << ", rejected " << cpu_rejected_count
             << ". Host ns/px and paired host ratios are live harness evidence; "
                "MCU cycle cells stay separate so missing board/simulator "
                "measurements remain visible.\n";

    fl::cout << "\n=== Colorimetric Strategy CPU Table ===\n";
    fl::cout << "Strategy                  | Runtime math                         | host ns/px | FPU cycles/px      | Fixed cycles/px      | fixed/FPU MCU ratio    | FPU-enabled path                  | Fixed-only path                         | host fixed/FPU evidence       | Setup/build cost                                   | Runtime operations                                                   | Validation\n";
    fl::cout << "--------------------------|--------------------------------------|------------|--------------------|----------------------|------------------------|-----------------------------------|-----------------------------------------|-------------------------------|----------------------------------------------------|----------------------------------------------------------------------|-----------\n";
    for (int i = 0; i < kStrategyCount; ++i) {
        char bench_text[24];
        char ratio[64];
        char mcu_ratio[64];
        bench_string(benches[i], bench_text);
        host_fixed_fpu_ratio_string(i, benches, ratio);
        strategy_mcu_fixed_fpu_ratio_string(i, mcu_ratio);
        char line[1536];
        fl::snprintf(line, sizeof(line),
                 "%-26s| %-36s | %10s | %-18s | %-20s | %-22s | %-33s | %-39s | %-29s | %-50s | %-68s | %s",
                 kStrategies[i].name,
                 kStrategies[i].runtime_math,
                 bench_text,
                 fpu_cycles_for(i),
                 fixed_cycles_for(i),
                 mcu_ratio,
                 kStrategies[i].fpu_path,
                 kStrategies[i].fixed_path,
                 ratio,
                 setup_cost_for(i),
                 runtime_ops_for(i),
                 kStrategies[i].cpu_validation);
        fl::cout << line << "\n";
    }

    int cpu_operation_model_count = 0;
    int cpu_operation_float_count = 0;
    int cpu_operation_fixed_count = 0;
    int cpu_operation_integer_division_count = 0;
    fl::cout << "\n=== CPU Operation Class Audit ===\n";
    fl::cout << "Strategy                  | Operation class       | Table fetches     | Division/normalize profile     | Branch/search profile      | FPU sensitivity\n";
    fl::cout << "--------------------------|-----------------------|-------------------|--------------------------------|----------------------------|--------------------\n";
    for (int i = 0; i < kStrategyCount; ++i) {
        const char* op_class = cpu_operation_class_for(i);
        const char* fetch_profile = table_fetch_profile_for(i);
        const char* div_profile = division_profile_for(i);
        const char* branch_profile = branch_search_profile_for(i);
        const char* fpu_sensitivity = fpu_sensitivity_for(i);
        ++cpu_operation_model_count;
        if (text_contains(op_class, "FPU")) {
            ++cpu_operation_float_count;
        }
        if (text_contains(op_class, "fixed")) {
            ++cpu_operation_fixed_count;
        }
        if (text_contains(div_profile, "integer")) {
            ++cpu_operation_integer_division_count;
        }
        char line[1024];
        fl::snprintf(line, sizeof(line),
                 "%-26s| %-21s | %-17s | %-30s | %-26s | %s",
                 kStrategies[i].name,
                 op_class,
                 fetch_profile,
                 div_profile,
                 branch_profile,
                 fpu_sensitivity);
        fl::cout << line << "\n";
    }
    fl::cout << "CPU operation-class summary: modeled "
             << cpu_operation_model_count
             << "/" << kStrategyCount
             << " rows; FPU/float runtime " << cpu_operation_float_count
             << ", fixed/integer runtime " << cpu_operation_fixed_count
             << ", audited integer-division rows "
             << cpu_operation_integer_division_count
             << ". This is static topology evidence from the registered "
                "evaluators, not a cycle estimate.\n";

    FL_CHECK(cpu_operation_model_count == kStrategyCount);
    FL_CHECK(cpu_operation_float_count > 0);
    FL_CHECK(cpu_operation_fixed_count > 0);
    FL_CHECK(cpu_operation_integer_division_count > 0);

    fl::cout << "\n=== Strategy Readiness Gate Audit ===\n";
    fl::cout << "Strategy                  | Stage    | Accuracy   | Memory gap            | host ns/px | MCU evidence status             | Production gate               | Readiness                                   | Blocker/evidence\n";
    fl::cout << "--------------------------|----------|------------|-----------------------|------------|---------------------------------|-------------------------------|---------------------------------------------|-----------------\n";
    int readiness_reference_count = 0;
    int readiness_rejected_count = 0;
    int readiness_not_shippable_count = 0;
    int readiness_complete_count = 0;
    for (int i = 0; i < kStrategyCount; ++i) {
        print_strategy_readiness_line(
            i, memories, benches, cf_stats, target_stats);
        char readiness[128];
        strategy_readiness_string(i, readiness);
        if (text_contains(readiness, "reference")) {
            ++readiness_reference_count;
        } else if (text_contains(readiness, "rejected")) {
            ++readiness_rejected_count;
        } else if (text_contains(readiness, "not shippable")) {
            ++readiness_not_shippable_count;
        } else {
            ++readiness_complete_count;
        }
    }
    fl::cout << "Readiness summary: reference "
             << readiness_reference_count
             << ", rejected " << readiness_rejected_count
             << ", not shippable " << readiness_not_shippable_count
             << ", complete " << readiness_complete_count
             << ". A strategy is not shippable until representative MCU "
                "cycle evidence and production integration are both present; "
                "host accuracy and host ns/px alone are insufficient.\n";

    int blocker_open_rows = 0;
    int blocker_mcu_count = 0;
    int blocker_integration_count = 0;
    int blocker_storage_count = 0;
    int blocker_accuracy_count = 0;
    int blocker_proof_count = 0;
    int blocker_fit_count = 0;
    int blocker_hardware_count = 0;
    int blocker_fixed_path_count = 0;
    int blocker_reference_surface_count = 0;

    fl::cout << "\n=== Structured Strategy Blocker Matrix ===\n";
    fl::cout << "Strategy                  | Stage    | Accuracy   | Memory gap            | Structured blockers                                       | First close action\n";
    fl::cout << "--------------------------|----------|------------|-----------------------|------------------------------------------------------------|-------------------\n";
    for (int i = 0; i < kStrategyCount; ++i) {
        const u32 mask = strategy_blocker_mask(
            i, memories[i], cf_stats[i], target_stats[i]);
        print_strategy_blocker_line(
            i, mask, memories, cf_stats, target_stats);
        char blockers[192];
        char close_action[192];
        strategy_blocker_string(mask, blockers);
        strategy_blocker_close_action(mask, close_action);
        FL_CHECK(has_text(blockers));
        FL_CHECK(has_text(close_action));
        FL_CHECK(!has_placeholder_wording(blockers));
        FL_CHECK(!has_placeholder_wording(close_action));
        if (mask != kBlockerNone) {
            ++blocker_open_rows;
        }
        if ((mask & kBlockerMcuCycles) != 0) {
            ++blocker_mcu_count;
        }
        if ((mask & kBlockerProductionIntegration) != 0) {
            ++blocker_integration_count;
        }
        if ((mask & kBlockerPackedStorage) != 0) {
            ++blocker_storage_count;
        }
        if ((mask & kBlockerAccuracyBudget) != 0) {
            ++blocker_accuracy_count;
        }
        if ((mask & kBlockerFormalProofProjection) != 0) {
            ++blocker_proof_count;
        }
        if ((mask & kBlockerConstrainedFit) != 0) {
            ++blocker_fit_count;
        }
        if ((mask & kBlockerHardwarePolicy) != 0) {
            ++blocker_hardware_count;
        }
        if ((mask & kBlockerFixedPathDecision) != 0) {
            ++blocker_fixed_path_count;
        }
        if ((mask & kBlockerReferenceSurface) != 0) {
            ++blocker_reference_surface_count;
        }
        if (!text_equals(kStrategies[i].stage, "REJECTED")
            && !kReferenceSurfaceLocked) {
            FL_CHECK((mask & kBlockerReferenceSurface) != 0);
        }
        if (!text_equals(kStrategies[i].stage, "REF")
            && !text_equals(kStrategies[i].stage, "REJECTED")
            && !has_complete_mcu_cycle_evidence(i)) {
            FL_CHECK((mask & kBlockerMcuCycles) != 0);
        }
        if (text_equals(kStrategies[i].stage, "REJECTED")) {
            FL_CHECK(mask == kBlockerNone);
        }
    }
    fl::cout << "Structured blocker summary: open rows "
             << blocker_open_rows
             << "; reference surface " << blocker_reference_surface_count
             << ", MCU cycles " << blocker_mcu_count
             << ", integration " << blocker_integration_count
             << ", storage " << blocker_storage_count
             << ", accuracy " << blocker_accuracy_count
             << ", proof/projection " << blocker_proof_count
             << ", constrained fit " << blocker_fit_count
             << ", hardware policy " << blocker_hardware_count
             << ", fixed-path decision " << blocker_fixed_path_count
             << ". These categories are computed separately from the prose "
                "missing-work field.\n";

    FL_CHECK(blocker_open_rows > 0);
    FL_CHECK(blocker_reference_surface_count > 0);
    FL_CHECK(blocker_mcu_count > 0);
    FL_CHECK(blocker_integration_count > 0);
    FL_CHECK(blocker_hardware_count > 0);

    int prototype_row_count = 0;
    int prototype_plan_count = 0;
    int rejected_disposition_count = 0;
    int rejected_evidence_count = 0;

    fl::cout << "\n=== Prototype Disposition Audit ===\n";
    fl::cout << "Strategy                          | Stage    | Disposition                  | Structured blockers                                       | First close action                              | Production plan / rejection evidence\n";
    fl::cout << "----------------------------------|----------|------------------------------|------------------------------------------------------------|-------------------------------------------------|--------------------------------------\n";
    for (int i = 0; i < kStrategyCount; ++i) {
        const bool prototype = text_equals(kStrategies[i].stage, "PROTO");
        const bool rejected = text_equals(kStrategies[i].stage, "REJECTED");
        if (!prototype && !rejected) {
            continue;
        }

        const u32 mask = strategy_blocker_mask(
            i, memories[i], cf_stats[i], target_stats[i]);
        char disposition[128];
        prototype_disposition_string(i, mask, disposition);
        print_prototype_disposition_line(i, mask);
        FL_CHECK(has_text(disposition));
        FL_CHECK(!has_placeholder_wording(disposition));
        FL_CHECK(!has_placeholder_wording(kStrategies[i].missing));
        if (prototype) {
            ++prototype_row_count;
            if (prototype_row_has_production_plan(i, mask)) {
                ++prototype_plan_count;
            }
            FL_CHECK(mask != kBlockerNone);
            FL_CHECK((mask & kBlockerProductionIntegration) != 0);
        }
        if (rejected) {
            ++rejected_disposition_count;
            if (rejected_row_has_evidence(i)) {
                ++rejected_evidence_count;
            }
            FL_CHECK(mask == kBlockerNone);
        }
    }
    fl::cout << "Prototype disposition summary: "
             << prototype_plan_count << "/" << prototype_row_count
             << " PROTO rows have structured production close actions; "
             << rejected_evidence_count << "/" << rejected_disposition_count
             << " REJECTED rows retain rejection evidence.\n";

    FL_CHECK(prototype_row_count > 0);
    FL_CHECK(rejected_disposition_count > 0);
    FL_CHECK(prototype_plan_count == prototype_row_count);
    FL_CHECK(rejected_evidence_count == rejected_disposition_count);

    int hypothesis_pass_count = 0;
    int hypothesis_partial_count = 0;
    int hypothesis_blocked_count = 0;

    fl::cout << "\n=== Strategy Hypothesis Checklist Audit ===\n";
    fl::cout << "Checklist item                     | Status   | Evidence                                                   | Blocker to close\n";
    fl::cout << "-----------------------------------|----------|------------------------------------------------------------|-----------------\n";

    auto emit_hypothesis_gate = [&](const char* gate,
                                    const char* status,
                                    const char* evidence,
                                    const char* blocker) {
        FL_CHECK(has_text(gate));
        FL_CHECK(has_text(status));
        FL_CHECK(has_text(evidence));
        FL_CHECK(has_text(blocker));
        FL_CHECK(completion_gate_status_known(status));
        FL_CHECK(!has_placeholder_wording(gate));
        FL_CHECK(!has_placeholder_wording(status));
        FL_CHECK(!has_placeholder_wording(evidence));
        FL_CHECK(!has_placeholder_wording(blocker));
        record_completion_gate_status(
            status,
            &hypothesis_pass_count,
            &hypothesis_partial_count,
            &hypothesis_blocked_count);
        print_completion_gate_line(gate, status, evidence, blocker);
    };

    char hypothesis_evidence[192];
    int baseline_measured_count = 0;
    const int baseline_indices[] = {
        kClosedFormIndex,
        kWxLpIndex,
        kWxOverdriveIndex,
        kBilinearN16Index,
        kHermiteN16Index,
        kHermiteN32Index,
    };
    for (int index : baseline_indices) {
        if (benches[index].measured && cf_stats[index].sample_count > 0) {
            ++baseline_measured_count;
        }
    }
    fl::snprintf(hypothesis_evidence, sizeof(hypothesis_evidence),
             "%ld/%ld baseline/reference rows measured",
             static_cast<long>(baseline_measured_count),
             static_cast<long>(sizeof(baseline_indices) / sizeof(baseline_indices[0])));
    emit_hypothesis_gate(
        "baseline/reference rows",
        baseline_measured_count == 6 ? "PASS" : "BLOCKED",
        hypothesis_evidence,
        baseline_measured_count == 6
            ? "keep legacy/reference rows runnable while comparing candidates"
            : "restore missing baseline/reference measurement rows");

    fl::snprintf(hypothesis_evidence, sizeof(hypothesis_evidence),
             "float/fixed rows measured; memory %luB target, %luB prototype",
             static_cast<unsigned long>(
                 memories[kHullAdaptiveIndex].target_packed_bytes),
             static_cast<unsigned long>(
                 memories[kHullAdaptiveIndex].prototype_storage_bytes));
    emit_hypothesis_gate(
        "hypothesis #1 hull adaptive",
        "PARTIAL",
        hypothesis_evidence,
        "production packed-axis decoder and representative MCU validation missing");

    fl::snprintf(hypothesis_evidence, sizeof(hypothesis_evidence),
             "selected u8 fixed row measured: %luB, p95/peak %ld/%ld LSB",
             static_cast<unsigned long>(
                 memories[kTetrahedralU8FixedIndex].target_packed_bytes),
             static_cast<long>(cf_stats[kTetrahedralU8FixedIndex].p95_lsb),
             static_cast<long>(cf_stats[kTetrahedralU8FixedIndex].peak_lsb));
    emit_hypothesis_gate(
        "hypothesis #2 tetrahedral",
        "PARTIAL",
        hypothesis_evidence,
        "integrate selected u8 fixed table and measure MCU cycles");

    fl::snprintf(hypothesis_evidence, sizeof(hypothesis_evidence),
             "double/fixed rows measured; fixed paired peak %ld LSB",
             static_cast<long>(pair_stats[kPolyPiecewiseFixedIndex].peak_lsb));
    emit_hypothesis_gate(
        "hypothesis #3 piecewise poly",
        "PARTIAL",
        hypothesis_evidence,
        "offline coefficient generator and constrained monotonic fit missing");

    fl::snprintf(hypothesis_evidence, sizeof(hypothesis_evidence),
             "seeded row measured; paired delta peak %ld LSB, +%luB router",
             static_cast<long>(pair_stats[kHybridQ16Index].peak_lsb),
             static_cast<unsigned long>(
                 memories[kHybridQ16Index].target_packed_bytes
                 - memories[kFixedQ16Index].target_packed_bytes));
    emit_hypothesis_gate(
        "hypothesis #4 hybrid refine",
        "PARTIAL",
        hypothesis_evidence,
        "de-prioritized unless seed speedup beats selected tetra path");

    fl::snprintf(hypothesis_evidence, sizeof(hypothesis_evidence),
             "float/fixed rows measured; memory %luB target, %luB prototype",
             static_cast<unsigned long>(
                 memories[kCellEncodingIndex].target_packed_bytes),
             static_cast<unsigned long>(
                 memories[kCellEncodingIndex].prototype_storage_bytes));
    emit_hypothesis_gate(
        "hypothesis #5 cell encoding",
        "PARTIAL",
        hypothesis_evidence,
        "packed cell format decision, compression sweep, and MCU validation missing");

    fl::snprintf(hypothesis_evidence, sizeof(hypothesis_evidence),
             "range/static audits measured; p95 %ld LSB vs strict reference",
             static_cast<long>(cf_stats[kFixedQ16Index].p95_lsb));
    emit_hypothesis_gate(
        "fixed analytical Q16",
        "PARTIAL",
        hypothesis_evidence,
        "hull projection, parity target, and formal overflow proof missing");

    fl::snprintf(hypothesis_evidence, sizeof(hypothesis_evidence),
             "rejected row measured; p95 %ld LSB with no memory gain",
             static_cast<long>(cf_stats[kBilinearFixedIndex].p95_lsb));
    emit_hypothesis_gate(
        "bilinear fixed baseline",
        "PASS",
        hypothesis_evidence,
        "keep rejected evidence visible instead of deleting the row");

    fl::snprintf(hypothesis_evidence, sizeof(hypothesis_evidence),
             "layered reference measured; strict fixed row paired peak %ld LSB",
             static_cast<long>(pair_stats[kRgbwwStrictFixedIndex].peak_lsb));
    emit_hypothesis_gate(
        "RGBWW reference and strict",
        "PARTIAL",
        hypothesis_evidence,
        "LED-bin/hardware policy validation, production integration, and MCU data missing");

    fl::cout << "Hypothesis checklist summary: pass " << hypothesis_pass_count
             << ", partial " << hypothesis_partial_count
             << ", blocked " << hypothesis_blocked_count
             << ". Partial items have runnable evidence but remain issue "
                "blockers until their close condition is satisfied.\n";

    FL_CHECK(hypothesis_pass_count > 0);
    FL_CHECK(hypothesis_partial_count > 0);
    FL_CHECK(hypothesis_blocked_count >= 0);

    fl::cout << "\n=== RGBW Public Dispatch Parity ===\n";
    fl::cout << "Scope          | Samples | max-channel LSB p50/p95/p99/peak | mean LSB | Meaning\n";
    fl::cout << "---------------|---------|-----------------------------------|----------|-----------------\n";
    print_reference_parity_line(
        "strict fixture",
        public_dispatch_fixture_stats,
        "public rgb_2_rgbw(Rgbw profile cfg) vs scorecard float_closed_form");
    print_reference_parity_line(
        "strict cube17",
        public_dispatch_cube_stats,
        "public rgb_2_rgbw(Rgbw profile cfg) vs scorecard float_closed_form");
    print_reference_parity_line(
        "boost fixture",
        public_boosted_fixture_stats,
        "public rgb_2_rgbw(boosted Rgbw profile cfg) vs wx_overdrive_float");
    print_reference_parity_line(
        "boost cube17",
        public_boosted_cube_stats,
        "public rgb_2_rgbw(boosted Rgbw profile cfg) vs wx_overdrive_float");
    fl::cout << "Public dispatch parity proves the scorecard strict reference "
                "and boosted policy rows follow the production public API "
                "with the LUT disabled. It is provenance evidence, not an "
                "independent physical proof of the closed-form target or "
                "boosted policy.\n";

    fl::cout << "\n=== Reference Fixture Drift Detail ===\n";
    fl::cout << "Scope        | Samples | max-channel LSB p50/p95/p99/peak | mean LSB | Meaning\n";
    fl::cout << "-------------|---------|-----------------------------------|----------|-----------------\n";
    print_fixture_drift_line(
        "all",
        closed_form_fixture_stats,
        "live public u8 closed-form vs legacy verifier cf_*");
    print_fixture_drift_line(
        "ok rows",
        closed_form_fixture_ok_stats,
        "fixture rows whose measured hardware dE was within budget");
    print_fixture_drift_line(
        "fail rows",
        closed_form_fixture_fail_stats,
        "known-bad physical rows; still part of frozen reference drift");
    fl::cout << "\n=== Reference Fixture Top Drift Rows ===\n";
    fl::cout << "Rank | Patch                | State | input RGB16          | live RGBW u8    | fixture RGBW u8 | Delta\n";
    fl::cout << "-----|----------------------|-------|----------------------|-----------------|-----------------|------\n";
    for (int i = 0; i < kFixtureDriftTopCount; ++i) {
        print_fixture_drift_example_line(i + 1, fixture_drift_examples[i]);
    }
    fl::cout << "\n=== Reference Fixture Drift Classification ===\n";
    fl::cout << "Signal                               | Count          | Share    | Meaning\n";
    fl::cout << "-------------------------------------|----------------|----------|-----------------\n";
    print_fixture_classification_line(
        "live value preserved",
        fixture_drift_classification.live_value_preserved_count,
        fixture_drift_classification.nonzero_count,
        "live output max channel stays within 1 LSB of input max");
    print_fixture_classification_line(
        "legacy value expanded",
        fixture_drift_classification.legacy_value_expanded_count,
        fixture_drift_classification.nonzero_count,
        "legacy cf_* output max exceeds input max by more than 32 LSB");
    print_fixture_classification_line(
        "expanded while live preserved",
        fixture_drift_classification.legacy_value_expanded_live_preserved_count,
        fixture_drift_classification.nonzero_count,
        "matches stale value-normalized surface signature");
    print_fixture_classification_line(
        "ok-row legacy expansion",
        fixture_drift_classification.legacy_value_expanded_ok_count,
        closed_form_fixture_ok_stats.sample_count,
        "same signature inside rows whose legacy hardware dE passed");
    print_fixture_classification_line(
        "fail-row legacy expansion",
        fixture_drift_classification.legacy_value_expanded_fail_count,
        closed_form_fixture_fail_stats.sample_count,
        "same signature inside known-bad legacy hardware rows");
    print_fixture_classification_line(
        "top-drift rows matching signature",
        fixture_drift_classification.top_drift_legacy_value_expanded_count,
        kFixtureDriftTopCount,
        "top drift rows consistent with stale value-normalized legacy surface");
    fl::cout << "Fixture drift classification: max live value delta "
             << fixture_drift_classification.max_live_value_delta
             << " LSB, max legacy value delta "
             << fixture_drift_classification.max_legacy_value_delta
             << " LSB. This classifies a likely stale-surface mechanism; it "
                "does not lock the reference or prove physical correctness.\n";

    int accuracy_complete_count = 0;
    int accuracy_label_count = 0;
    int memory_complete_count = 0;
    int memory_note_count = 0;
    int host_bench_count = 0;
    int cpu_cycle_cell_count = 0;
    int q16_expected_audit_count = 0;
    int q16_range_audit_count = 0;
    int q16_static_bound_count = 0;
    for (int i = 0; i < kStrategyCount; ++i) {
        if (cf_stats[i].sample_count > 0 && target_stats[i].sample_count > 0) {
            ++accuracy_complete_count;
        }
        if (has_text(reference_name_for(i)) && has_text(sample_set_for(i))) {
            ++accuracy_label_count;
        }
        if (memories[i].target_packed_bytes != kStrategyBytesUnknown
            && memories[i].prototype_storage_bytes != kStrategyBytesUnknown
            && memories[i].runtime_ram_bytes != kStrategyBytesUnknown
            && memories[i].per_profile_bytes != kStrategyBytesUnknown
            && memories[i].per_controller_bytes != kStrategyBytesUnknown) {
            ++memory_complete_count;
        }
        if (has_text(memories[i].source)
            && has_text(memories[i].scaling)
            && has_text(memories[i].tiny_tier)) {
            ++memory_note_count;
        }
        if (benches[i].measured) {
            ++host_bench_count;
        }
        if (cycle_cell_has_explicit_evidence_or_reason(fpu_cycles_for(i))
            && cycle_cell_has_explicit_evidence_or_reason(fixed_cycles_for(i))) {
            ++cpu_cycle_cell_count;
        }
        if (has_q16_range_audit(i)) {
            ++q16_expected_audit_count;
            if (q16_range_stats[i].measured) {
                ++q16_range_audit_count;
            }
            if (q16_static_bounds[i].measured) {
                ++q16_static_bound_count;
            }
        }
    }

    int paired_output_count = 0;
    int paired_memory_count = 0;
    int paired_mcu_ratio_cell_count = 0;
    int paired_mcu_ratio_measured_count = 0;
    for (int i = 0; i < kCpuPairCount; ++i) {
        if (pair_stats[kCpuPairs[i].fixed_index].sample_count == kVerifierRowCount) {
            ++paired_output_count;
        }
        const CpuPair& pair = kCpuPairs[i];
        if (memories[pair.fpu_index].target_packed_bytes != kStrategyBytesUnknown
            && memories[pair.fixed_index].target_packed_bytes != kStrategyBytesUnknown) {
            ++paired_memory_count;
        }
        char mcu_ratio[64];
        pair_mcu_fixed_fpu_ratio_string(pair, mcu_ratio);
        if (has_text(mcu_ratio)) {
            ++paired_mcu_ratio_cell_count;
        }
        if (mcu_ratio_cell_has_measurement(mcu_ratio)) {
            ++paired_mcu_ratio_measured_count;
        }
    }

    int reference_surface_pass_count = 0;
    int reference_surface_partial_count = 0;
    int reference_surface_blocked_count = 0;

    fl::cout << "\n=== Reference Surface Lock Audit ===\n";
    fl::cout << "Gate                               | Status   | Evidence                                                   | Blocker to close\n";
    fl::cout << "-----------------------------------|----------|------------------------------------------------------------|-----------------\n";

    auto emit_reference_surface_gate = [&](const char* gate,
                                           const char* status,
                                           const char* evidence,
                                           const char* blocker) {
        FL_CHECK(has_text(gate));
        FL_CHECK(has_text(status));
        FL_CHECK(has_text(evidence));
        FL_CHECK(has_text(blocker));
        FL_CHECK(completion_gate_status_known(status));
        FL_CHECK(!has_placeholder_wording(gate));
        FL_CHECK(!has_placeholder_wording(status));
        FL_CHECK(!has_placeholder_wording(evidence));
        FL_CHECK(!has_placeholder_wording(blocker));
        record_completion_gate_status(
            status,
            &reference_surface_pass_count,
            &reference_surface_partial_count,
            &reference_surface_blocked_count);
        print_completion_gate_line(gate, status, evidence, blocker);
    };

    char reference_evidence[192];
    const bool closed_form_self_consistent =
        cf_stats[kClosedFormIndex].sample_count == kVerifierRowCount
        && cf_stats[kClosedFormIndex].peak_lsb == 0
        && cube_stats[kClosedFormIndex].sample_count == kCubeSamples
        && cube_stats[kClosedFormIndex].peak_lsb == 0;
    fl::snprintf(reference_evidence, sizeof(reference_evidence),
             "%ld verifier rows, %ld cube17 rows, peak %ld/%ld LSB",
             static_cast<long>(cf_stats[kClosedFormIndex].sample_count),
             static_cast<long>(cube_stats[kClosedFormIndex].sample_count),
             static_cast<long>(cf_stats[kClosedFormIndex].peak_lsb),
             static_cast<long>(cube_stats[kClosedFormIndex].peak_lsb));
    emit_reference_surface_gate(
        "closed-form self-check",
        closed_form_self_consistent ? "PASS" : "BLOCKED",
        reference_evidence,
        closed_form_self_consistent
            ? "keep self-check before comparing approximation rows"
            : "fix the reference harness before comparing strategies");

    const bool public_dispatch_parity_locked =
        public_dispatch_fixture_stats.sample_count == kVerifierRowCount
        && public_dispatch_fixture_stats.peak_lsb == 0
        && public_dispatch_cube_stats.sample_count == kCubeSamples
        && public_dispatch_cube_stats.peak_lsb == 0;
    fl::snprintf(reference_evidence, sizeof(reference_evidence),
             "%ld verifier, %ld cube17 rows, peak %ld/%ld LSB",
             static_cast<long>(public_dispatch_fixture_stats.sample_count),
             static_cast<long>(public_dispatch_cube_stats.sample_count),
             static_cast<long>(public_dispatch_fixture_stats.peak_lsb),
             static_cast<long>(public_dispatch_cube_stats.peak_lsb));
    emit_reference_surface_gate(
        "public dispatch parity",
        public_dispatch_parity_locked ? "PASS" : "BLOCKED",
        reference_evidence,
        public_dispatch_parity_locked
            ? "keep scorecard reference tied to public API"
            : "fix scorecard reference before comparing approximation rows");

    const bool public_boosted_parity_locked =
        public_boosted_fixture_stats.sample_count == kVerifierRowCount
        && public_boosted_fixture_stats.peak_lsb == 0
        && public_boosted_cube_stats.sample_count == kCubeSamples
        && public_boosted_cube_stats.peak_lsb == 0;
    fl::snprintf(reference_evidence, sizeof(reference_evidence),
             "%ld verifier, %ld cube17 rows, peak %ld/%ld LSB",
             static_cast<long>(public_boosted_fixture_stats.sample_count),
             static_cast<long>(public_boosted_cube_stats.sample_count),
             static_cast<long>(public_boosted_fixture_stats.peak_lsb),
             static_cast<long>(public_boosted_cube_stats.peak_lsb));
    emit_reference_surface_gate(
        "public boosted parity",
        public_boosted_parity_locked ? "PASS" : "BLOCKED",
        reference_evidence,
        public_boosted_parity_locked
            ? "keep boosted policy row tied to public API"
            : "fix boosted policy row before comparing boosted approximations");

    const bool closed_form_fixture_locked =
        closed_form_fixture_stats.sample_count == kVerifierRowCount
        && closed_form_fixture_stats.peak_lsb == 0;
    fl::snprintf(reference_evidence, sizeof(reference_evidence),
             "%ld fixture rows, p95/peak %ld/%ld LSB vs legacy cf_*",
             static_cast<long>(closed_form_fixture_stats.sample_count),
             static_cast<long>(closed_form_fixture_stats.p95_lsb),
             static_cast<long>(closed_form_fixture_stats.peak_lsb));
    emit_reference_surface_gate(
        "frozen verifier fixture",
        closed_form_fixture_locked ? "PASS" : "BLOCKED",
        reference_evidence,
        closed_form_fixture_locked
            ? "keep fixture parity as the live closed-form drift alarm"
            : "classify stale verifier data vs current-surface change before locking winners");

    fl::snprintf(reference_evidence, sizeof(reference_evidence),
             "%ld/%ld top rows and %ld/%ld nonzero rows match value expansion",
             static_cast<long>(
                 fixture_drift_classification.top_drift_legacy_value_expanded_count),
             static_cast<long>(kFixtureDriftTopCount),
             static_cast<long>(
                 fixture_drift_classification
                     .legacy_value_expanded_live_preserved_count),
             static_cast<long>(fixture_drift_classification.nonzero_count));
    emit_reference_surface_gate(
        "fixture drift classification",
        "PARTIAL",
        reference_evidence,
        "decide whether classified drift is stale fixture data or a target change");

    fl::snprintf(reference_evidence, sizeof(reference_evidence),
             "%ld rows scored against current closed form; no independent proof",
             static_cast<long>(accuracy_complete_count));
    emit_reference_surface_gate(
        "independent reference proof",
        kReferenceSurfaceLocked ? "PASS" : "BLOCKED",
        reference_evidence,
        "prove or approve the canonical floating reference surface");

    fl::snprintf(reference_evidence, sizeof(reference_evidence),
             "max-channel LSB metric exists; platform tolerance not frozen");
    emit_reference_surface_gate(
        "FPU deviation policy",
        kFpuTolerancePolicyLocked ? "PASS" : "PARTIAL",
        reference_evidence,
        "define allowed FPU drift in max-channel LSB per target platform");

    fl::snprintf(reference_evidence, sizeof(reference_evidence),
             "six decision candidates remain provisional while target can move");
    emit_reference_surface_gate(
        "topology promotion freeze",
        kReferenceSurfaceLocked ? "PASS" : "BLOCKED",
        reference_evidence,
        "do not promote topology winners until the reference surface is locked");

    fl::cout << "Reference surface summary: pass "
             << reference_surface_pass_count
             << ", partial " << reference_surface_partial_count
             << ", blocked " << reference_surface_blocked_count
             << ". Optimization rows are provisional until the canonical "
                "floating reference and FPU LSB tolerance are locked.\n";

    FL_CHECK(reference_surface_pass_count > 0);
    FL_CHECK(reference_surface_partial_count > 0);
    FL_CHECK(reference_surface_blocked_count > 0);
    FL_CHECK(!kReferenceSurfaceLocked);

    int completion_pass_count = 0;
    int completion_partial_count = 0;
    int completion_blocked_count = 0;

    fl::cout << "\n=== Issue Completion Gate Audit ===\n";
    fl::cout << "Gate                               | Status   | Evidence                                                   | Blocker to close\n";
    fl::cout << "-----------------------------------|----------|------------------------------------------------------------|-----------------\n";

    auto emit_completion_gate = [&](const char* gate,
                                    const char* status,
                                    const char* evidence,
                                    const char* blocker) {
        FL_CHECK(has_text(gate));
        FL_CHECK(has_text(status));
        FL_CHECK(has_text(evidence));
        FL_CHECK(has_text(blocker));
        FL_CHECK(completion_gate_status_known(status));
        FL_CHECK(!has_placeholder_wording(gate));
        FL_CHECK(!has_placeholder_wording(status));
        FL_CHECK(!has_placeholder_wording(evidence));
        FL_CHECK(!has_placeholder_wording(blocker));
        record_completion_gate_status(
            status,
            &completion_pass_count,
            &completion_partial_count,
            &completion_blocked_count);
        print_completion_gate_line(gate, status, evidence, blocker);
    };

    char gate_evidence[192];
    fl::snprintf(gate_evidence, sizeof(gate_evidence),
             "%ld strategies registered with metadata and runnable rows",
             static_cast<long>(kStrategyCount));
    emit_completion_gate("strategy registry", "PASS", gate_evidence,
                         "keep new hypotheses registered instead of hidden");

    fl::snprintf(gate_evidence, sizeof(gate_evidence),
             "%ld pass, %ld partial, %ld blocked reference audit rows",
             static_cast<long>(reference_surface_pass_count),
             static_cast<long>(reference_surface_partial_count),
             static_cast<long>(reference_surface_blocked_count));
    emit_completion_gate(
        "reference surface lock",
        kReferenceSurfaceLocked && kFpuTolerancePolicyLocked
            && reference_surface_blocked_count == 0 ? "PASS" : "BLOCKED",
        gate_evidence,
        "lock canonical float output and FPU LSB tolerance before topology decisions");

    fl::snprintf(gate_evidence, sizeof(gate_evidence),
             "%ld pass, %ld partial, %ld blocked hypothesis checklist items",
             static_cast<long>(hypothesis_pass_count),
             static_cast<long>(hypothesis_partial_count),
             static_cast<long>(hypothesis_blocked_count));
    emit_completion_gate(
        "strategy checklist coverage",
        hypothesis_partial_count == 0 && hypothesis_blocked_count == 0
            ? "PASS"
            : "PARTIAL",
        gate_evidence,
        "finish each named hypothesis close condition before closing the issue");

    fl::snprintf(gate_evidence, sizeof(gate_evidence),
             "%ld open rows; ref %ld, MCU %ld, integration %ld, hardware %ld",
             static_cast<long>(blocker_open_rows),
             static_cast<long>(blocker_reference_surface_count),
             static_cast<long>(blocker_mcu_count),
             static_cast<long>(blocker_integration_count),
             static_cast<long>(blocker_hardware_count));
    emit_completion_gate(
        "structured blocker matrix",
        blocker_open_rows == 0 ? "PASS" : "BLOCKED",
        gate_evidence,
        "clear all structured blockers or mark rows rejected with evidence");

    fl::snprintf(gate_evidence, sizeof(gate_evidence),
             "%ld/%ld PROTO plans, %ld/%ld REJECTED evidence rows",
             static_cast<long>(prototype_plan_count),
             static_cast<long>(prototype_row_count),
             static_cast<long>(rejected_evidence_count),
             static_cast<long>(rejected_disposition_count));
    emit_completion_gate(
        "prototype disposition coverage",
        prototype_plan_count == prototype_row_count
            && rejected_evidence_count == rejected_disposition_count
                ? "PASS"
                : "BLOCKED",
        gate_evidence,
        prototype_plan_count == prototype_row_count
            && rejected_evidence_count == rejected_disposition_count
                ? "keep production plans and rejection evidence explicit"
                : "add production close actions or rejection evidence");

    fl::snprintf(gate_evidence, sizeof(gate_evidence),
             "%ld/%ld stats rows, %ld/%ld reference/sample labels",
             static_cast<long>(accuracy_complete_count),
             static_cast<long>(kStrategyCount),
             static_cast<long>(accuracy_label_count),
             static_cast<long>(kStrategyCount));
    emit_completion_gate(
        "accuracy table coverage",
        accuracy_complete_count == kStrategyCount
            && accuracy_label_count == kStrategyCount ? "PASS" : "BLOCKED",
        gate_evidence,
        accuracy_complete_count == kStrategyCount
            && accuracy_label_count == kStrategyCount
            ? "keep reference/sample labels explicit"
            : "fill missing max-channel LSB and target dE cells");

    fl::snprintf(gate_evidence, sizeof(gate_evidence),
             "%ld/%ld byte rows, %ld/%ld source/scaling/tiny notes",
             static_cast<long>(memory_complete_count),
             static_cast<long>(kStrategyCount),
             static_cast<long>(memory_note_count),
             static_cast<long>(kStrategyCount));
    emit_completion_gate(
        "memory table coverage",
        memory_complete_count == kStrategyCount
            && memory_note_count == kStrategyCount ? "PASS" : "BLOCKED",
        gate_evidence,
        memory_complete_count == kStrategyCount
            && memory_note_count == kStrategyCount
            ? "converge over-target prototype formats before shipping"
            : "fill missing packed/prototype/runtime memory cells");

    fl::snprintf(gate_evidence, sizeof(gate_evidence),
             "%ld/%ld rows have measured host ns/px",
             static_cast<long>(host_bench_count),
             static_cast<long>(kStrategyCount));
    emit_completion_gate(
        "host CPU coverage",
        host_bench_count == kStrategyCount ? "PASS" : "BLOCKED",
        gate_evidence,
        host_bench_count == kStrategyCount
            ? "do not treat host ns as MCU cycles"
            : "add runnable host benchmark rows");

    fl::snprintf(gate_evidence, sizeof(gate_evidence),
             "%ld/%ld rows modeled; FPU %ld, fixed %ld, int-div %ld",
             static_cast<long>(cpu_operation_model_count),
             static_cast<long>(kStrategyCount),
             static_cast<long>(cpu_operation_float_count),
             static_cast<long>(cpu_operation_fixed_count),
             static_cast<long>(cpu_operation_integer_division_count));
    emit_completion_gate(
        "CPU operation topology",
        cpu_operation_model_count == kStrategyCount ? "PASS" : "BLOCKED",
        gate_evidence,
        "static op classes exist; representative cycle measurements still separate");

    fl::snprintf(gate_evidence, sizeof(gate_evidence),
             "%ld/%ld rows have explicit FPU+fixed cycle cells",
             static_cast<long>(cpu_cycle_cell_count),
             static_cast<long>(kStrategyCount));
    emit_completion_gate(
        "CPU evidence cell coverage",
        cpu_cycle_cell_count == kStrategyCount ? "PASS" : "BLOCKED",
        gate_evidence,
        cpu_cycle_cell_count == kStrategyCount
            ? "keep measured cycles separate from missing/not-applicable reasons"
            : "fill every FPU/fixed cell with measured evidence or explicit reason");

    fl::snprintf(gate_evidence, sizeof(gate_evidence),
             "%ld complete, %ld partial, %ld missing, %ld rejected",
             static_cast<long>(cpu_complete_count),
             static_cast<long>(cpu_partial_count),
             static_cast<long>(cpu_missing_count),
             static_cast<long>(cpu_rejected_count));
    emit_completion_gate("representative MCU cycles", "BLOCKED", gate_evidence,
                         "measure FPU and fixed cycles on named target/simulator");

    fl::snprintf(gate_evidence, sizeof(gate_evidence),
             "%ld/%ld host, %ld/%ld memory, %ld/%ld MCU cells, %ld measured",
             static_cast<long>(paired_output_count),
             static_cast<long>(kCpuPairCount),
             static_cast<long>(paired_memory_count),
             static_cast<long>(kCpuPairCount),
             static_cast<long>(paired_mcu_ratio_cell_count),
             static_cast<long>(kCpuPairCount),
             static_cast<long>(paired_mcu_ratio_measured_count));
    emit_completion_gate(
        "fixed/FPU relative cost",
        paired_output_count == kCpuPairCount
            && paired_memory_count == kCpuPairCount
            && paired_mcu_ratio_cell_count == kCpuPairCount ? "PARTIAL" : "BLOCKED",
        gate_evidence,
        "host ratios exist; MCU fixed/FPU cycle ratios still missing");

    fl::snprintf(gate_evidence, sizeof(gate_evidence),
             "%ld complete, %ld not shippable, %ld rejected",
             static_cast<long>(readiness_complete_count),
             static_cast<long>(readiness_not_shippable_count),
             static_cast<long>(readiness_rejected_count));
    emit_completion_gate("production readiness", "BLOCKED", gate_evidence,
                         "land selected candidates outside the harness");

    fl::snprintf(gate_evidence, sizeof(gate_evidence),
             "%ld/%ld observed range audits and %ld/%ld static bounds",
             static_cast<long>(q16_range_audit_count),
             static_cast<long>(q16_expected_audit_count),
             static_cast<long>(q16_static_bound_count),
             static_cast<long>(q16_expected_audit_count));
    emit_completion_gate(
        "Q16 safety evidence",
        q16_range_audit_count == q16_expected_audit_count
            && q16_static_bound_count == q16_expected_audit_count
                ? "PARTIAL"
                : "BLOCKED",
        gate_evidence,
        "add formal overflow/projection proof for shippable Q16 rows");

    fl::snprintf(gate_evidence, sizeof(gate_evidence),
             "%ld samples, native/two-channel white violations %ld/%ld",
             static_cast<long>(rgbww_physical_policy_stats.sample_count),
             static_cast<long>(
                 rgbww_physical_policy_stats.native_single_white_violations),
             static_cast<long>(
                 rgbww_physical_policy_stats.two_channel_white_violations));
    emit_completion_gate(
        "RGBWW strict policy evidence",
        rgbww_physical_policy_stats.sample_count == kCubeSamples
            && rgbww_physical_policy_stats.native_single_white_violations == 0
            && rgbww_physical_policy_stats.two_channel_white_violations == 0
            && rgbww_physical_policy_stats.combo_without_white_count == 0
                ? "PARTIAL"
                : "BLOCKED",
        gate_evidence,
        "host invariant audit exists; LED-bin/hardware validation still missing");

    fl::snprintf(gate_evidence, sizeof(gate_evidence),
             "default tetrahedral_rgb8_u8_fixed; small poly fixed; compare tetrahedral_rgb8");
    emit_completion_gate(
        "selected implementation focus",
        "PASS",
        gate_evidence,
        "integrate selected fixed u8 tetra row outside harness and measure MCU cycles");

    fl::snprintf(gate_evidence, sizeof(gate_evidence),
             "six provisional candidates printed; %ld shippable rows",
             static_cast<long>(readiness_complete_count));
    emit_completion_gate("provisional decision matrix", "PARTIAL", gate_evidence,
                         "candidate rows are evidence only until gates pass");

    const int final_winner_count =
        kReferenceSurfaceLocked && kFpuTolerancePolicyLocked
        && readiness_complete_count >= 6
        && paired_mcu_ratio_measured_count == kCpuPairCount ? 6 : 0;
    fl::snprintf(gate_evidence, sizeof(gate_evidence),
             "%ld final winners named from %ld shippable rows",
             static_cast<long>(final_winner_count),
             static_cast<long>(readiness_complete_count));
    emit_completion_gate("final winner promotion",
                         final_winner_count == 6 ? "PASS" : "BLOCKED",
                         gate_evidence,
                         "name final winners only after reference, MCU, integration, and hardware gates pass");

    fl::cout << "Completion gate summary: pass " << completion_pass_count
             << ", partial " << completion_partial_count
             << ", blocked " << completion_blocked_count
             << ". Issue #3429 cannot close while any gate is blocked.\n";

    FL_CHECK(completion_pass_count > 0);
    FL_CHECK(completion_partial_count > 0);
    FL_CHECK(completion_blocked_count > 0);
    FL_CHECK(accuracy_label_count == kStrategyCount);
    FL_CHECK(memory_note_count == kStrategyCount);
    FL_CHECK(cpu_cycle_cell_count == kStrategyCount);
    FL_CHECK(paired_mcu_ratio_cell_count == kCpuPairCount);
    FL_CHECK(paired_mcu_ratio_measured_count == 0);
    FL_CHECK(final_winner_count == 0);
    FL_CHECK(readiness_complete_count == 0);

    fl::cout << "\n=== Selected Implementation Track Audit ===\n";
    fl::cout << "Role                   | Selected row               | Evidence                                 | Memory     | CPU host/FPU/fixed/ratio                                                | LSB p50/p95/p99/peak | Remaining work\n";
    fl::cout << "-----------------------|----------------------------|------------------------------------------|------------|-------------------------------------------------------------------------|-----------------------|---------------\n";
    print_decision_line("Default RGBW", kTetrahedralU8FixedIndex,
                        "fixed u8 tetra: 2KB with tetra accuracy",
                        "production integration and MCU cycles missing",
                        memories, benches, cf_stats);
    print_decision_line("Small flash/RAM", kPolyPiecewiseFixedIndex,
                        "480B fixed fallback if 2KB is too high",
                        "constrained coefficients and MCU cycles missing",
                        memories, benches, cf_stats);
    print_decision_line("High accuracy cmp", kTetrahedralIndex,
                        "i16 tetra reference for accuracy comparison",
                        "comparison row only unless 2KB u8 error is rejected",
                        memories, benches, cf_stats);
    print_decision_line("Paused topology", kHybridQ16Index,
                        "no gain over fixed analytical, seed unproven",
                        "do not integrate unless seed speedup appears",
                        memories, benches, cf_stats);

    fl::cout << "\n=== Provisional Decision Candidate Matrix ===\n";
    fl::cout << "Category               | Provisional candidate      | Evidence                                 | Memory     | CPU host/FPU/fixed/ratio                                                | LSB p50/p95/p99/peak | Blockers\n";
    fl::cout << "-----------------------|----------------------------|------------------------------------------|------------|-------------------------------------------------------------------------|-----------------------|---------\n";
    print_decision_line("Small flash/RAM", kPolyPiecewiseFixedIndex,
                        "smallest passing fixed RGBW row",
                        "reference not locked; needs coefficients, constrained fit, MCU validation",
                        memories, benches, cf_stats);
    print_decision_line("Balanced RGBW", kTetrahedralU8FixedIndex,
                        "selected fixed u8 tetra default track",
                        "reference not locked; production integration and MCU validation missing",
                        memories, benches, cf_stats);
    print_decision_line("Highest RGBW accuracy", kTetrahedralIndex,
                        "lowest non-exact RGBW p95/p99/peak LSB",
                        "reference not locked; MCU validation and production integration missing",
                        memories, benches, cf_stats);
    print_decision_line("Fixed-point-only MCU", kTetrahedralU8FixedIndex,
                        "fixed path exists at 2048B target memory",
                        "reference not locked; MCU validation and integration missing",
                        memories, benches, cf_stats);
    print_decision_line("FPU-enabled MCU", kTetrahedralIndex,
                        "best measured p99/host among float tetra rows",
                        "reference not locked; target MCU cycles missing; u8 row halves memory",
                        memories, benches, cf_stats);
    print_decision_line("RGBWW strict", kRgbwwStrictIndex,
                        "best current 5-channel strict reference row",
                        "reference not locked; LED-bin/hardware policy and MCU data missing",
                        memories, benches, cf_stats);
    fl::cout << "Decision matrix is provisional candidate evidence, not a "
                "winner list. Do not promote topology winners until the "
                "closed-form reference surface, FPU max-channel LSB tolerance "
                "policy, representative MCU cycles, production integration, "
                "and hardware/RGBWW policy gates are closed.\n";

    fl::cout << "\n=== End comparison tables ===\n";
    fl::cout << "Strategies registered: " << kStrategyCount
             << " | Sample set: verifier_known_bad (" << kVerifierRowCount
             << " rows)\n";
    fl::cout << "Most accurate RGBW vs-ref peak: " << best_rgbw_peak_name
             << " (peak " << best_rgbw_peak << " LSB)\n";
    fl::cout << "Best non-exact RGBW approximation vs-ref peak: "
             << best_rgbw_approx_peak_name << " (peak " << best_rgbw_approx_peak
             << " LSB)\n";
    fl::cout << "Lowest measured RGBW target p95 dE: " << best_rgbw_target_name
             << " (" << best_rgbw_target << ")\n";
    fl::cout << "Lowest non-exact measured RGBW target p95 dE: "
             << best_rgbw_approx_target_name << " (" << best_rgbw_approx_target
             << ")\n\n";
    fl::cout << "LSB columns are max absolute error in any output channel. "
                "RGBW rows compare to strict closed-form; policy rows name "
                "their policy role in the reference cell. RGBWW rows compare "
                "to the current layered RGBWW reference.\n";
    fl::cout << "Reference-surface lock status is explicit: the current "
                "closed-form row is self-consistent, but the canonical float "
                "surface and allowed FPU LSB drift are not locked yet. The "
                "public dispatch parity audit ties that row and the boosted "
                "policy row to rgb_2_rgbw(Rgbw profile cfg) with the LUT "
                "disabled, but does not prove the target is physically "
                "canonical. The "
                "vendored verifier cf_* drift row compares the current public "
                "u8 solver to a legacy 16-bit analytical snapshot; it is a "
                "staleness/current-surface check, not an independent physical "
                "proof. Strategy rankings are therefore provisional topology "
                "evidence.\n";
    fl::cout << "Paired LSB compares a fixed/prototype row with its paired FPU row "
                "when one exists; n/a means no single paired row is defined.\n";
    fl::cout << "Policy LSB compares policy approximation rows against their "
                "production policy target, for example boosted rows against "
                "wx_overdrive_float and LP rows against wx_lp_legacy_float.\n";
    fl::cout << "Accuracy-table sample-set labels name fixture/gamut/topology; "
                "boost-policy rows use the verifier fixture with the boosted "
                "policy enabled.\n";
    fl::cout << "The cube17 sweep is an independent 17x17x17 RGB grid "
                "(4913 samples) scored only by max-channel LSB against the "
                "same row reference; it has no fixture target dE column.\n";
    fl::cout << "The cube17 policy sweep scores only rows with an explicit "
                "production policy target, using the same 4913 RGB inputs.\n";
    fl::cout << "The scale-ray monotonicity audit checks 4095 exact integer RGB "
                "directions, components 0..15, with 17 brightness steps each; "
                "drops are output-channel decreases along a fixed RGB ratio.\n";
    fl::cout << "The Q16 solver integer range audit is an observed cube17 runtime-math "
                "range check for policy, analytical/refine, and RGBWW strict "
                "fixed rows. It records real intermediate maxima, determinant "
                "minima, skipped zero determinants, actually-used divisor "
                "minima, zero used-divisor counts, and i64 numerator headroom, "
                "but does not replace the remaining formal overflow/projection "
                "proof.\n";
    fl::cout << "Memory columns are formula-derived from current layouts or explicit "
                "packed target layouts in this harness.\n";
    fl::cout << "CPU host ns/px is measured here. FPU cycles/px and fixed cycles/px "
                "are explicit evidence cells: missing means no representative MCU "
                "measurement is recorded, not an inferred host-derived cycle count. "
                "\"missing paired ... MCU\" means the companion implementation "
                "exists, but that target measurement is still absent. The "
                "fixed/FPU MCU ratio is computed only from measured MCU cycle "
                "cells; host ratios stay in their own column.\n\n";
    fl::cout << "Host paired CPU/accuracy cost is side-by-side host evidence only. "
                "It is intentionally kept separate from MCU cycle columns so "
                "the table does not smuggle host ratios in as target-cycle data.\n\n";

    fl::cout << "\nStrategy stage notes:\n";
    for (int i = 0; i < kStrategyCount; ++i) {
        fl::cout << "  " << kStrategies[i].name << " [" << kStrategies[i].stage
                 << "]: " << kStrategies[i].missing
                 << " | memory scaling: " << memories[i].scaling << "\n";
    }
    fl::cout << "\n";

    FL_CHECK(kStrategyCount == 31);
    FL_CHECK(best_rgbw_peak == 0);
    FL_CHECK(memories[kHybridQ16Index].target_packed_bytes == 304);
    FL_CHECK(g_bench_sink != 0);
}

}  // FL_TEST_FILE
