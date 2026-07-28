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

FL_TEST_CASE("esp32 clock divider: max_N bounds the integer divider (#3745)") {
    // clkm_div_num on classic-ESP32 I2S is 8 bits, so 255 is the largest
    // value that survives the register assignment. Assigning 800 to it would
    // silently truncate to 800 & 0xFF == 32, running the peripheral at
    // 80 MHz / 32 = 2.5 MHz instead of the requested 100 kHz -- a 25x error
    // with no diagnostic. The solver clamps instead.
    constexpr int kFieldMax = 255;

    constexpr int kMaxA = 63;

    const ClkDividerNAB reachable =
        solveFractionalDivider(kApbClockHz, 320000u, kMaxA, 2, kFieldMax);
    FL_CHECK(reachable.n <= kFieldMax);
    FL_CHECK(!reachable.saturated);

    // The divisor is N + B/A, so the slowest reachable clock is
    // 80 MHz / (255 + 62/63) == 312519 Hz, NOT 80 MHz / 255 == 313725 Hz.
    // Targets in that band need N == 255 *plus* a fraction; bounding N must
    // not cost them their fractional part or mislabel them unreachable.
    const ClkDividerNAB in_fractional_band =
        solveFractionalDivider(kApbClockHz, 313000u, kMaxA, 2, kFieldMax);
    FL_CHECK(in_fractional_band.n == kFieldMax);
    FL_CHECK(in_fractional_band.b > 0);       // fraction preserved
    FL_CHECK(!in_fractional_band.saturated);  // genuinely reachable

    // The exact boundary: 80 MHz / 255 must land on N == 255 and not tip over.
    const ClkDividerNAB boundary =
        solveFractionalDivider(kApbClockHz, kApbClockHz / kFieldMax, kMaxA, 2,
                               kFieldMax);
    FL_CHECK(boundary.n <= kFieldMax);
    FL_CHECK(!boundary.saturated);

    // Genuinely out of range: 100 kHz needs N == 800. Must clamp, say so, and
    // still return the *closest* bounded triple -- 255 + 62/63, the largest
    // legal divisor -- rather than a coarse 255/1 that discards the fraction.
    const ClkDividerNAB over =
        solveFractionalDivider(kApbClockHz, 100000u, kMaxA, 2, kFieldMax);
    FL_CHECK(over.n == kFieldMax);
    FL_CHECK(over.saturated);
    FL_CHECK(over.a == kMaxA);
    FL_CHECK(over.b == kMaxA - 1);
    // A clamped result must still be a legal triple, not a half-updated one.
    FL_CHECK(over.b < over.a);

    // The truncation this guards against: without the bound the solver
    // returns 800, and 800 & 0xFF is 32 -- a value that looks perfectly
    // plausible in the register.
    const ClkDividerNAB unbounded =
        solveFractionalDivider(kApbClockHz, 100000u, 63, 2);
    FL_CHECK(unbounded.n == 800);
    FL_CHECK((unbounded.n & 0xFF) == 32);
    FL_CHECK(!unbounded.saturated);
}

FL_TEST_CASE("esp32 clock divider: existing callers keep their behavior") {
    // max_N defaults to kMaxDivider, so every pre-#3745 call site must solve
    // to exactly what it did before and never report saturation.
    using fl::platforms::esp32::kMaxDivider;

    const u32 rates[] = {8000000u, 3200000u, 2400000u, 1000000u, 400000u};
    for (u32 hz : rates) {
        const ClkDividerNAB with_default = solveFractionalDivider(kApbClockHz, hz);
        const ClkDividerNAB explicit_max =
            solveFractionalDivider(kApbClockHz, hz, 63, 2, kMaxDivider);
        FL_CHECK(with_default.n == explicit_max.n);
        FL_CHECK(with_default.a == explicit_max.a);
        FL_CHECK(with_default.b == explicit_max.b);
        FL_CHECK(!with_default.saturated);
    }

    // Hitting the int-representability guard is not a hardware saturation --
    // it means the caller asked for a sub-hertz clock, a different failure.
    const ClkDividerNAB huge = solveFractionalDivider(4294967295u, 1u);
    FL_CHECK(huge.n == kMaxDivider);
    FL_CHECK(!huge.saturated);
}

FL_TEST_CASE("esp32 clock divider: max_N below min_N cannot fabricate an illegal N") {
    // Nonsensical limits must still yield something the hardware accepts:
    // the min_N floor wins, because a divider under it is unwritable.
    const ClkDividerNAB d =
        solveFractionalDivider(kApbClockHz, 8000000u, 63, /*min_N=*/2,
                               /*max_N=*/1);
    FL_CHECK(d.n == 2);
    // 8 MHz needs a divisor of 10, unreachable once N is pinned to 2, so this
    // saturates and returns the largest legal fraction on top of that N. The
    // point of the case is that `n` never drops below min_N to chase the
    // target -- an N the hardware cannot encode is worse than a slow clock.
    FL_CHECK(d.saturated);
    FL_CHECK(d.b < d.a);
    FL_CHECK(d.a <= 63);
}
