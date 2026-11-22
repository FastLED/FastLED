// Colorimetric strategy harness checks (#3422).
//
// Fast checks over the strategy registry and the shipped production path.
// The full strategy comparison table lives in the integration tier:
// tests/integration/misc/colorimetric_strategy_table.cpp.

#include "test.h"

#include "tests/unit/misc/colorimetric_strategies.hpp"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;
using namespace fl::colorimetric_detail;
using namespace fl::colorimetric_test;
using namespace fl::colorimetric_test::fixture;

FL_TEST_CASE("colorimetric strategy harness: fixture loads cleanly") {
    FL_CHECK(kVerifierRowCount == 381);
    FL_CHECK(kVerifierRows[0].patch != nullptr);
    FL_CHECK(kVerifierRows[0].status == VerifierRow::kSkip);
}

FL_TEST_CASE("colorimetric strategy harness: issue strategies are registered") {
    FL_CHECK(kStrategyCount == 31);
    FL_CHECK(kStrategies[kHermiteN16Index].family[0] != '\0');
    FL_CHECK(kStrategies[kWxOverdriveFixedIndex].solve != nullptr);
    FL_CHECK(kStrategies[kWxLpResidualIndex].solve != nullptr);
    FL_CHECK(kStrategies[kWxLpBalancedIndex].solve != nullptr);
    FL_CHECK(kStrategies[kHermiteN16FixedIndex].solve != nullptr);
    FL_CHECK(kStrategies[kHermiteN32FixedIndex].solve != nullptr);
    FL_CHECK(kStrategies[kCellEncodingFixedIndex].solve != nullptr);
    FL_CHECK(kStrategies[kHullAdaptiveFixedIndex].solve != nullptr);
    FL_CHECK(kStrategies[kHybridQ16Index].memory_info().target_packed_bytes == 304);
    FL_CHECK(kStrategies[kTetrahedralU8Index].memory_info().target_packed_bytes * 2
             == kStrategies[kTetrahedralIndex].memory_info().target_packed_bytes);
    FL_CHECK(kStrategies[kTetrahedralU8FixedIndex].memory_info().target_packed_bytes
             == kStrategies[kTetrahedralU8Index].memory_info().target_packed_bytes);
    FL_CHECK(kStrategies[kTetrahedralBoostU8Index].memory_info().target_packed_bytes * 2
             == kStrategies[kTetrahedralBoostIndex].memory_info().target_packed_bytes);
    FL_CHECK(kStrategies[kTetrahedralBoostU8FixedIndex].memory_info().target_packed_bytes
             == kStrategies[kTetrahedralBoostU8Index].memory_info().target_packed_bytes);
    FL_CHECK(lut_bytes(16, RgbwLutInterp::Bilinear) == 2048);
    FL_CHECK(lut_bytes(16, RgbwLutInterp::Hermite) == 6144);
    FL_CHECK(packed_hermite_cell_bytes(16) == 6144);
    FL_CHECK(packed_hull_axis_bytes(16) == 32);
    FL_CHECK(packed_hermite_cell_bytes(16) + packed_hull_axis_bytes(16) == 6176);
    FL_CHECK(packed_tetrahedral_i16_bytes(8) == 4096);
    FL_CHECK(packed_tetrahedral_u8_bytes(8) == 2048);
    FL_CHECK(rgbww_strict_xyz_i16_bytes() == 30);
    FL_CHECK(rgbww_strict_source_matrix_i16_bytes() == 18);
    FL_CHECK(rgbww_strict_combo_index_bytes() == 27);
    FL_CHECK(rgbww_strict_combo_inverse_i16_bytes() == 162);
    FL_CHECK(rgbww_strict_combo_flag_bytes() == 9);
    FL_CHECK(packed_rgbww_strict_bytes() == 228);
    FL_CHECK(packed_rgbww_strict_bytes()
             + rgbww_strict_source_matrix_i16_bytes() == 246);
    FL_CHECK(packed_rgbww_strict_q16_bytes() == 456);
    FL_CHECK(kStrategies[kRgbwwStrictIndex].solve5 != nullptr);
    FL_CHECK(kStrategies[kRgbwwStrictFixedIndex].solve5 != nullptr);

    int aligned_memory_count = 0;
    int prototype_over_target_count = 0;
    int zero_target_runtime_count = 0;
    for (int i = 0; i < kStrategyCount; ++i) {
        const Strategy& strategy = kStrategies[i];
        FL_CHECK(has_text(strategy.name));
        FL_CHECK(has_text(strategy.family));
        FL_CHECK(has_text(strategy.stage));
        FL_CHECK(has_text(strategy.topology));
        FL_CHECK(has_text(strategy.missing));
        FL_CHECK(has_text(strategy.runtime_math));
        FL_CHECK(has_text(strategy.fpu_path));
        FL_CHECK(has_text(strategy.fixed_path));
        FL_CHECK(has_text(strategy.cpu_validation));
        FL_CHECK(!has_placeholder_wording(strategy.name));
        FL_CHECK(!has_placeholder_wording(strategy.family));
        FL_CHECK(!has_placeholder_wording(strategy.stage));
        FL_CHECK(!has_placeholder_wording(strategy.topology));
        FL_CHECK(!has_placeholder_wording(strategy.missing));
        FL_CHECK(!has_placeholder_wording(strategy.runtime_math));
        FL_CHECK(!has_placeholder_wording(strategy.fpu_path));
        FL_CHECK(!has_placeholder_wording(strategy.fixed_path));
        FL_CHECK(!has_placeholder_wording(strategy.cpu_validation));
        FL_CHECK(has_text(reference_name_for(i)));
        FL_CHECK(has_text(sample_set_for(i)));
        FL_CHECK(has_text(fpu_cycles_for(i)));
        FL_CHECK(has_text(fixed_cycles_for(i)));
        FL_CHECK(has_text(setup_cost_for(i)));
        FL_CHECK(has_text(runtime_ops_for(i)));
        FL_CHECK(has_text(cpu_operation_class_for(i)));
        FL_CHECK(has_text(table_fetch_profile_for(i)));
        FL_CHECK(has_text(division_profile_for(i)));
        FL_CHECK(has_text(branch_search_profile_for(i)));
        FL_CHECK(has_text(fpu_sensitivity_for(i)));
        FL_CHECK(!has_placeholder_wording(reference_name_for(i)));
        FL_CHECK(!has_placeholder_wording(sample_set_for(i)));
        FL_CHECK(!has_placeholder_wording(fpu_cycles_for(i)));
        FL_CHECK(!has_placeholder_wording(fixed_cycles_for(i)));
        FL_CHECK(!has_placeholder_wording(setup_cost_for(i)));
        FL_CHECK(!has_placeholder_wording(runtime_ops_for(i)));
        FL_CHECK(!has_placeholder_wording(cpu_operation_class_for(i)));
        FL_CHECK(!has_placeholder_wording(table_fetch_profile_for(i)));
        FL_CHECK(!has_placeholder_wording(division_profile_for(i)));
        FL_CHECK(!has_placeholder_wording(branch_search_profile_for(i)));
        FL_CHECK(!has_placeholder_wording(fpu_sensitivity_for(i)));
        FL_CHECK(!text_equals(setup_cost_for(i), "unknown"));
        FL_CHECK(!text_equals(runtime_ops_for(i), "unknown"));
        FL_CHECK(!text_equals(cpu_operation_class_for(i), "not modeled"));
        FL_CHECK(!text_equals(branch_search_profile_for(i), "not modeled"));
        FL_CHECK(!text_equals(fpu_sensitivity_for(i), "FPU sensitivity pending"));
        char cpu_evidence_status[64];
        cpu_evidence_status_string(i, cpu_evidence_status);
        FL_CHECK(has_text(cpu_evidence_status));
        FL_CHECK(!has_placeholder_wording(cpu_evidence_status));
        char production_gate[96];
        char readiness[128];
        production_gate_string(i, production_gate);
        strategy_readiness_string(i, readiness);
        FL_CHECK(has_text(production_gate));
        FL_CHECK(has_text(readiness));
        FL_CHECK(!has_placeholder_wording(production_gate));
        FL_CHECK(!has_placeholder_wording(readiness));
        if (text_equals(strategy.stage, "PROTO")) {
            FL_CHECK(text_contains(readiness, "not shippable"));
        }
        if (text_equals(strategy.stage, "REJECTED")) {
            FL_CHECK(text_contains(readiness, "rejected"));
        }
        if (!text_equals(strategy.stage, "REF")
            && !text_equals(strategy.stage, "REJECTED")
            && !has_complete_mcu_cycle_evidence(i)) {
            FL_CHECK(text_contains(readiness, "not shippable"));
        }
        FL_CHECK(strategy.memory_info != nullptr);
        const MemoryInfo memory = strategy.memory_info();
        FL_CHECK(memory.target_packed_bytes != kStrategyBytesUnknown);
        FL_CHECK(memory.prototype_storage_bytes != kStrategyBytesUnknown);
        FL_CHECK(memory.runtime_ram_bytes != kStrategyBytesUnknown);
        FL_CHECK(memory.per_profile_bytes != kStrategyBytesUnknown);
        FL_CHECK(memory.per_controller_bytes != kStrategyBytesUnknown);
        FL_CHECK(has_text(memory.source));
        FL_CHECK(has_text(memory.scaling));
        FL_CHECK(has_text(memory.tiny_tier));
        FL_CHECK(!has_placeholder_wording(memory.source));
        FL_CHECK(!has_placeholder_wording(memory.scaling));
        FL_CHECK(!has_placeholder_wording(memory.tiny_tier));
        FL_CHECK(has_text(memory_gap_class(memory)));
        FL_CHECK(!has_placeholder_wording(memory_gap_class(memory)));
        char memory_delta[32];
        char memory_ratio[32];
        signed_memory_delta_string(
            memory.prototype_storage_bytes,
            memory.target_packed_bytes,
            memory_delta);
        memory_ratio_string(
            memory.runtime_ram_bytes,
            memory.target_packed_bytes,
            memory_ratio);
        FL_CHECK(has_text(memory_delta));
        FL_CHECK(has_text(memory_ratio));
        FL_CHECK(!has_placeholder_wording(memory_delta));
        FL_CHECK(!has_placeholder_wording(memory_ratio));
        if (memory.prototype_storage_bytes == memory.target_packed_bytes) {
            ++aligned_memory_count;
        }
        if (memory.prototype_storage_bytes > memory.target_packed_bytes) {
            ++prototype_over_target_count;
        }
        if (memory.target_packed_bytes == 0 && memory.runtime_ram_bytes > 0) {
            ++zero_target_runtime_count;
        }
        FL_CHECK(strategy.solve != nullptr || strategy.solve5 != nullptr);
    }
    FL_CHECK(aligned_memory_count > 0);
    FL_CHECK(prototype_over_target_count > 0);
    FL_CHECK(zero_target_runtime_count > 0);
}

FL_TEST_CASE("colorimetric strategy harness: subsets are non-empty") {
    DiodeProfile p = kRgbwDefaultProfile;
    set_input_gamut(p, InputGamut::Native);
    const ErrorStats ok_stats = evaluate_strategy(
        kStrategies[kClosedFormIndex], kStrategies[kClosedFormIndex],
        p, Reference::ClosedForm, RowFilter::OkOnly);
    const ErrorStats fail_stats = evaluate_strategy(
        kStrategies[kClosedFormIndex], kStrategies[kClosedFormIndex],
        p, Reference::ClosedForm, RowFilter::FailOnly);
    FL_CHECK(ok_stats.sample_count > 0);
    FL_CHECK(fail_stats.sample_count > 0);
    FL_CHECK(ok_stats.sample_count + fail_stats.sample_count < kVerifierRowCount);
}

FL_TEST_CASE("colorimetric strategy harness: closed-form baseline is exact") {
    DiodeProfile p = kRgbwDefaultProfile;
    set_input_gamut(p, InputGamut::Native);
    const ErrorStats stats = evaluate_strategy(
        kStrategies[kClosedFormIndex], kStrategies[kClosedFormIndex],
        p, Reference::ClosedForm, RowFilter::All);
    FL_CHECK(stats.sample_count == kVerifierRowCount);
    FL_CHECK(stats.peak_lsb == 0);
    FL_CHECK(stats_within_budget(stats, kBudgets[0][0]));
}

FL_TEST_CASE("colorimetric strategy harness: scores frozen fixture drift") {
    DiodeProfile p = kRgbwDefaultProfile;
    set_input_gamut(p, InputGamut::Native);
    const ErrorStats stats =
        evaluate_closed_form_fixture_lock(
            kStrategies[kClosedFormIndex], p, RowFilter::All);
    FL_CHECK(stats.sample_count == kVerifierRowCount);
    FL_CHECK(stats.peak_lsb >= stats.p95_lsb);
}

// The production-path checks the comparison table makes, extracted so they run
// on every build. The table below sweeps every research strategy, builds all
// the LUT grids and runs 256-pass micro-benchmarks, which costs seconds of CPU.
FL_TEST_CASE("colorimetric production path: dispatch parity and monotonicity") {
    DiodeProfile p = kRgbwDefaultProfile;
    set_input_gamut(p, InputGamut::Native);
    static constexpr int kCubeSteps = 17;
    static constexpr int kCubeSamples = kCubeSteps * kCubeSteps * kCubeSteps;

    // The public RGBW_MODE dispatch must reproduce the harness solvers exactly.
    const ErrorStats dispatch_fixture = evaluate_public_dispatch_fixture_parity(
        kStrategies[kClosedFormIndex], RGBW_MODE::kRGBWColorimetric, p,
        RowFilter::All);
    const ErrorStats dispatch_cube = evaluate_public_dispatch_cube_parity(
        kStrategies[kClosedFormIndex], RGBW_MODE::kRGBWColorimetric, p,
        kCubeSteps);
    const ErrorStats boosted_fixture = evaluate_public_dispatch_fixture_parity(
        kStrategies[kWxOverdriveIndex], RGBW_MODE::kRGBWColorimetricBoosted, p,
        RowFilter::All);
    const ErrorStats boosted_cube = evaluate_public_dispatch_cube_parity(
        kStrategies[kWxOverdriveIndex], RGBW_MODE::kRGBWColorimetricBoosted, p,
        kCubeSteps);
    FL_CHECK(dispatch_fixture.sample_count == kVerifierRowCount);
    FL_CHECK(dispatch_cube.sample_count == kCubeSamples);
    FL_CHECK(boosted_fixture.sample_count == kVerifierRowCount);
    FL_CHECK(boosted_cube.sample_count == kCubeSamples);
    FL_CHECK(dispatch_fixture.peak_lsb == 0);
    FL_CHECK(dispatch_cube.peak_lsb == 0);
    FL_CHECK(boosted_fixture.peak_lsb == 0);
    FL_CHECK(boosted_cube.peak_lsb == 0);

    // The shipped solvers are deterministic across the cube and never dim a
    // channel when the input is scaled up.
    const int shipped[] = {kClosedFormIndex, kRgbwwLayeredIndex};
    for (int idx : shipped) {
        const ErrorStats cube = evaluate_strategy_cube_lsb(
            kStrategies[idx], kStrategies[idx], p, kCubeSteps);
        FL_CHECK(cube.sample_count == kCubeSamples);
        FL_CHECK(cube.peak_lsb == 0);
        const MonotonicStats mono =
            evaluate_strategy_scale_monotonicity(kStrategies[idx], p, 16, 17);
        FL_CHECK(mono.comparison_count > 0);
        FL_CHECK(mono.violation_count == 0);
    }
}

}  // FL_TEST_FILE
