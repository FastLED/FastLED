// IWYU pragma: private

#pragma once

/// @file platforms/esp/32/core/clock_divider.h
/// Fractional clock-divider solver shared by ESP32 peripherals (FastLED#3562).
///
/// Several classic-ESP32 peripherals derive their clock from a reference
/// (usually the 80 MHz APB) through an `(N, A, B)` triple, where
///
///     f_out = base_hz / (N + B/A)
///
/// I2S spells it `clkm_conf.clkm_div_{num,a,b}`; SPI and RMT use different
/// register layouts but the same arithmetic. This header owns the math once so
/// each driver only has to marshal the result into its own registers.
///
/// Pure integer/double arithmetic with no ESP headers, so it compiles and is
/// unit-testable on the host.

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace platforms {
namespace esp32 {

/// Reference clock for APB-derived peripherals on the classic ESP32.
constexpr u32 kApbClockHz = 80000000u;

/// Solved divider triple: `f_out = base_hz / (n + b/a)`.
///
/// Lowercase members per the plain-aggregate convention; the uppercase
/// N/A/B in the formulas below is the register-datasheet spelling
/// (`clkm_conf.clkm_div_{num,a,b}`).
struct ClkDividerNAB {
    int n;
    int a;
    int b;
};

/// Largest integer divider the solver will report. The real hardware fields
/// are far narrower (I2S `clkm_div_num` is 8 bits), so this is not a
/// hardware limit — it exists so `base_hz / target_hz` cannot overflow the
/// `int` it is cast into. A u32 base over a target of 1 Hz reaches ~4.29e9,
/// which is past INT_MAX and would make the cast undefined.
constexpr int kMaxDivider = 2147483000;

/// @brief Solve `base_hz / (N + B/A)` for the closest match to `target_hz`.
///
/// Adapted from Yves's `i2s_define_bit_patterns` clock math, generalised over
/// the base clock and the hardware limits.
///
/// @param base_hz  Reference clock feeding the divider (e.g. kApbClockHz).
/// @param target_hz Desired output frequency. Must be non-zero — see below.
/// @param max_A    Largest denominator the hardware accepts (inclusive).
///                 I2S allows 63.
/// @param min_N    Smallest integer divider the hardware accepts. I2S needs 2.
///
/// @return The `(N, A, B)` minimising `|target_hz - base_hz/(N + B/A)|` within
///         those limits.
///
/// A `target_hz` of 0 has no meaningful answer (the divider would be
/// infinite), so it is clamped to `min_N` with no fractional part rather than
/// dividing by zero. Callers that want a specific fallback frequency should
/// substitute it themselves before calling — that keeps the policy choice at
/// the call site instead of buried in shared math.
inline ClkDividerNAB solveFractionalDivider(u32 base_hz, u32 target_hz,
                                            int max_A = 63,
                                            int min_N = 2) FL_NO_EXCEPT {
    ClkDividerNAB out;
    if (target_hz == 0 || base_hz == 0) {
        out.n = min_N;
        out.a = 1;
        out.b = 0;
        return out;
    }

    const double f_target = static_cast<double>(target_hz);
    const double f_base = static_cast<double>(base_hz);

    // Clamp before the cast, not after: a ratio past INT_MAX makes
    // static_cast<int> undefined, so there would be no well-defined value
    // left to clamp. Saturating here keeps the result monotonic and
    // representable for any u32 pair.
    const double ratio = f_base / f_target;
    if (ratio >= static_cast<double>(kMaxDivider)) {
        out.n = kMaxDivider;
        out.a = 1;
        out.b = 0;
        return out;
    }

    int N = static_cast<int>(ratio);
    if (N < min_N) N = min_N;
    const double residual = ratio - static_cast<double>(N);

    // Fractional part B/A. Sweep A within the hardware limit and pick the
    // (A, B) minimising |residual - B/A|. Seeding best_err with `residual`
    // means B/A = 0/1 stays the answer unless some pair beats it.
    int best_A = 1;
    int best_B = 0;
    double best_err = residual;
    for (int A = 1; A <= max_A; ++A) {
        for (int B = 0; B < A; ++B) {
            const double err = residual - (static_cast<double>(B) / A);
            const double abs_err = err < 0 ? -err : err;
            if (abs_err < best_err) {
                best_err = abs_err;
                best_A = A;
                best_B = B;
            }
        }
    }

    // Double-precision 0.9999 corner: A == B collapses to the next integer
    // up. Kept from Yves's original code.
    if (best_A == best_B) {
        best_A = 1;
        best_B = 0;
        ++N;
    }

    out.n = N;
    out.a = best_A;
    out.b = best_B;
    return out;
}

}  // namespace esp32
}  // namespace platforms
}  // namespace fl
