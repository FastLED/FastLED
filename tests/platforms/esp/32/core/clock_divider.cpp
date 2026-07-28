/// @file tests/platforms/esp/32/core/clock_divider.cpp
/// Host tests for the shared ESP32 fractional clock-divider solver (#3562).
///
/// The solver was hoisted out of the I2S peripheral impl, so the most useful
/// thing these tests can do is pin the arithmetic: a refactor that quietly
/// changed the chosen (N, A, B) would retune every driver's output clock
/// without any build failure to catch it.

#include "test.h"

#include "platforms/esp/32/core/clock_divider.h"

using fl::platforms::esp32::ClkDividerNAB;
using fl::platforms::esp32::kApbClockHz;
using fl::platforms::esp32::solveFractionalDivider;

namespace {

// Reference implementation: the exact pre-hoist body from
// i2s_peripheral_esp32dev_esp.cpp.hpp, kept verbatim so the tests below
// compare against what the driver used to do rather than against the new
// code restated.
void referenceSolve(fl::u32 target_hz, int *out_N, int *out_A, int *out_B) {
    if (target_hz == 0) {
        *out_N = 10;
        *out_A = 1;
        *out_B = 0;
        return;
    }
    const double f_target = static_cast<double>(target_hz);
    const double f_base = static_cast<double>(80000000u);
    int N = static_cast<int>(f_base / f_target);
    if (N < 2) N = 2;
    const double residual = (f_base / f_target) - static_cast<double>(N);

    int best_A = 1;
    int best_B = 0;
    double best_err = residual;
    for (int A = 1; A < 64; ++A) {
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
    if (best_A == best_B) {
        best_A = 1;
        best_B = 0;
        ++N;
    }
    *out_N = N;
    *out_A = best_A;
    *out_B = best_B;
}

// Realised output frequency for a solved triple.
double realisedHz(fl::u32 base_hz, const ClkDividerNAB &d) {
    return static_cast<double>(base_hz) /
           (static_cast<double>(d.n) + static_cast<double>(d.b) / d.a);
}

}  // namespace

FL_TEST_CASE("esp32 clock divider: matches the pre-hoist I2S solver") {
    // Sweep the band the I2S clockless driver actually uses, plus the edges.
    const fl::u32 targets[] = {
        100000u,   500000u,   800000u,   1000000u,  2000000u,
        3200000u,  4000000u,  6400000u,  8000000u,  10000000u,
        13333333u, 16000000u, 20000000u, 26666666u, 40000000u,
    };

    for (fl::u32 hz : targets) {
        int rN = 0, rA = 0, rB = 0;
        referenceSolve(hz, &rN, &rA, &rB);
        const ClkDividerNAB d = solveFractionalDivider(kApbClockHz, hz, 63, 2);
        FL_CHECK(d.n == rN);
        FL_CHECK(d.a == rA);
        FL_CHECK(d.b == rB);
    }
}

FL_TEST_CASE("esp32 clock divider: 8 MHz default resolves to N=10") {
    // The wave8 @ 800 kHz default. 80 MHz / 10 == 8 MHz exactly, so the
    // fractional part must stay empty rather than picking a near-equal B/A.
    const ClkDividerNAB d = solveFractionalDivider(kApbClockHz, 8000000u);
    FL_CHECK(d.n == 10);
    FL_CHECK(d.a == 1);
    FL_CHECK(d.b == 0);
}

FL_TEST_CASE("esp32 clock divider: respects hardware limits") {
    for (fl::u32 hz = 300000u; hz <= 20000000u; hz += 700000u) {
        const ClkDividerNAB d = solveFractionalDivider(kApbClockHz, hz, 63, 2);
        FL_CHECK(d.n >= 2);          // min_N floor
        FL_CHECK(d.a >= 1);          // never a zero denominator
        FL_CHECK(d.a <= 63);         // I2S clkm_div_a field width
        FL_CHECK(d.b >= 0);
        FL_CHECK(d.b < d.a);         // B/A stays a proper fraction
    }
}

FL_TEST_CASE("esp32 clock divider: lands close to the requested frequency") {
    // Not an exactness claim -- the divider is quantised. But a solver that
    // silently degraded would show up as a large relative error here.
    for (fl::u32 hz = 1000000u; hz <= 20000000u; hz += 1000000u) {
        const ClkDividerNAB d = solveFractionalDivider(kApbClockHz, hz, 63, 2);
        const double got = realisedHz(kApbClockHz, d);
        const double rel = (got - static_cast<double>(hz)) / static_cast<double>(hz);
        FL_CHECK((rel < 0 ? -rel : rel) < 0.02);
    }
}

FL_TEST_CASE("esp32 clock divider: generalises over base clock") {
    // The point of the hoist: peripherals on a different reference get the
    // same math. Halving the base halves the integer divider for the same
    // target.
    const ClkDividerNAB apb = solveFractionalDivider(80000000u, 8000000u);
    const ClkDividerNAB half = solveFractionalDivider(40000000u, 8000000u);
    FL_CHECK(apb.n == 10);
    FL_CHECK(half.n == 5);
}

FL_TEST_CASE("esp32 clock divider: min_N floor is honoured") {
    // Target above base/min_N cannot be reached; the solver must clamp to
    // min_N rather than emitting N=1 or N=0 that the hardware would reject.
    const ClkDividerNAB d = solveFractionalDivider(kApbClockHz, 79000000u, 63, 2);
    FL_CHECK(d.n >= 2);

    const ClkDividerNAB d4 = solveFractionalDivider(kApbClockHz, 79000000u, 63, 4);
    FL_CHECK(d4.n >= 4);
}

FL_TEST_CASE("esp32 clock divider: oversized ratios stay representable") {
    using fl::platforms::esp32::kMaxDivider;

    // base/target can exceed INT_MAX for extreme u32 pairs -- 4.29e9 / 1 is
    // past 2.147e9. Casting that to int is undefined, so the solver has to
    // saturate *before* the cast; there would be no defined value to clamp
    // afterwards.
    const ClkDividerNAB huge = solveFractionalDivider(4294967295u, 1u);
    FL_CHECK(huge.n == kMaxDivider);
    FL_CHECK(huge.a == 1);
    FL_CHECK(huge.b == 0);

    // Just under the clamp: still a normal solve, no saturation.
    const ClkDividerNAB ok = solveFractionalDivider(kApbClockHz, 1u);
    FL_CHECK(ok.n > 0);
    FL_CHECK(ok.n < kMaxDivider);
    FL_CHECK(ok.a >= 1);
    FL_CHECK(ok.b < ok.a);

    // Saturation must never produce a divider the caller can't use.
    FL_CHECK(huge.n > 0);
}

FL_TEST_CASE("esp32 clock divider: degenerate inputs do not divide by zero") {
    // Callers are expected to substitute their own fallback frequency, but a
    // zero must not produce inf/NaN or an out-of-range divider.
    const ClkDividerNAB zero_target = solveFractionalDivider(kApbClockHz, 0u);
    FL_CHECK(zero_target.n == 2);
    FL_CHECK(zero_target.a == 1);
    FL_CHECK(zero_target.b == 0);

    const ClkDividerNAB zero_base = solveFractionalDivider(0u, 8000000u);
    FL_CHECK(zero_base.n == 2);
    FL_CHECK(zero_base.a == 1);
    FL_CHECK(zero_base.b == 0);
}
