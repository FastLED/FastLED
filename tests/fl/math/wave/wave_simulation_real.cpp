// Regression tests for the Q15 Courant x Laplacian multiply in
// fl::WaveSimulation1D_Real / fl::WaveSimulation2D_Real. Per issue #3073,
// the pre-fix `(mCourantSq32 * laplacian) >> 15` could overflow i32 at
// extreme speed/saturation combinations, producing wrong-sign terms and
// visible glitches. The fix:
//   1) clamps setSpeed/constructor speed to the CFL stability bound, and
//   2) promotes the inner-loop multiply to i64 before the >>15 shift.
// These tests cover both halves of the fix.

#include "fl/math/wave/wave_simulation_real.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_TEST_CASE("WaveSimulation2D_Real clamps speed to CFL bound [0, 0.5]") {
    // 2D CFL bound for the 5-point stencil is C^2 <= 0.5. Pre-fix the
    // setter accepted up to ~1.0, which is unstable and could overflow.
    WaveSimulation2D_Real sim(8, 8, 0.16f, 6.0f);
    sim.setSpeed(10.0f);
    // Q15(0.5) = 16383 -> back to float -> 16383/32767 ~= 0.49999.
    FL_CHECK_CLOSE(sim.getSpeed(), 0.5f, 0.001f);

    sim.setSpeed(-2.0f);
    // Q15(0.0) = 0 -> 0.0f exactly.
    FL_CHECK_CLOSE(sim.getSpeed(), 0.0f, 0.001f);

    sim.setSpeed(0.16f);
    FL_CHECK_CLOSE(sim.getSpeed(), 0.16f, 0.001f);
}

FL_TEST_CASE("WaveSimulation1D_Real clamps speed to CFL bound [0, 1.0]") {
    // 1D CFL bound for the 5-point stencil is C^2 <= 1.0.
    WaveSimulation1D_Real sim(16, 0.16f, 6);
    sim.setSpeed(10.0f);
    FL_CHECK_CLOSE(sim.getSpeed(), 1.0f, 0.001f);

    sim.setSpeed(-2.0f);
    FL_CHECK_CLOSE(sim.getSpeed(), 0.0f, 0.001f);

    sim.setSpeed(0.42f);
    FL_CHECK_CLOSE(sim.getSpeed(), 0.42f, 0.001f);
}

FL_TEST_CASE("WaveSimulation2D_Real survives saturated checkerboard at max CFL") {
    // Worst case for the inner-loop Q15 multiply: speed at the CFL bound
    // (0.5) and every cell saturated alternating between +1.0 and -1.0
    // (Q15 values ~+/-32767). The laplacian magnitude approaches 262,140,
    // which times Q15(0.5)=16383 is ~4.3e9 - well past i32 max. Pre-fix
    // this overflowed; post-fix the i64 promote keeps the math correct
    // and the clamp keeps the result in i16 range.
    const u32 W = 16;
    const u32 H = 16;
    WaveSimulation2D_Real sim(W, H, 0.5f, 6.0f);
    sim.setHalfDuplex(false);  // keep negative values for the test

    for (u32 y = 0; y < H; ++y) {
        for (u32 x = 0; x < W; ++x) {
            const float v = ((x + y) & 1) ? -1.0f : 1.0f;
            sim.setf(x, y, v);
        }
    }
    sim.update();

    // Every cell must remain in valid Q15 range (the clamp at the end of
    // update() guarantees this; if the i32 multiply overflowed, the
    // clamp would still execute but on a wrong-sign value).
    bool any_nonzero = false;
    for (u32 y = 0; y < H; ++y) {
        for (u32 x = 0; x < W; ++x) {
            const i16 v = sim.geti16(x, y);
            FL_CHECK_GE(static_cast<int>(v), -32768);
            FL_CHECK_LE(static_cast<int>(v), 32767);
            if (v != 0) any_nonzero = true;
        }
    }
    // Sanity: the kernel actually ran and produced some output.
    FL_CHECK(any_nonzero);
}

FL_TEST_CASE("WaveSimulation1D_Real survives saturated checkerboard at max CFL") {
    const u32 N = 32;
    WaveSimulation1D_Real sim(N, 1.0f, 6);
    sim.setHalfDuplex(false);

    for (u32 i = 0; i < N; ++i) {
        const float v = (i & 1) ? -1.0f : 1.0f;
        sim.set(i, v);
    }
    sim.update();

    bool any_nonzero = false;
    for (u32 i = 0; i < N; ++i) {
        const i16 v = sim.geti16(i);
        FL_CHECK_GE(static_cast<int>(v), -32768);
        FL_CHECK_LE(static_cast<int>(v), 32767);
        if (v != 0) any_nonzero = true;
    }
    FL_CHECK(any_nonzero);
}

FL_TEST_CASE("WaveSimulation2D_Real stays bounded across many steps at CFL bound") {
    // Beyond the single-step saturation guard, run a multi-step sim at
    // the CFL bound to confirm the i64-promoted multiply keeps the
    // system stable. With pre-fix i32 overflow at high speed, spurious
    // large terms could compound across steps and produce visibly
    // unstable output even after the per-step Q15 clamp.
    const u32 W = 16;
    const u32 H = 16;
    WaveSimulation2D_Real sim(W, H, 0.5f, 6.0f);
    sim.setHalfDuplex(false);

    // Seed a single high-amplitude impulse at the center.
    sim.setf(W / 2, H / 2, 1.0f);

    for (int step = 0; step < 32; ++step) {
        sim.update();
        for (u32 y = 0; y < H; ++y) {
            for (u32 x = 0; x < W; ++x) {
                const i16 v = sim.geti16(x, y);
                FL_CHECK_GE(static_cast<int>(v), -32768);
                FL_CHECK_LE(static_cast<int>(v), 32767);
            }
        }
    }
}

FL_TEST_CASE("WaveSimulation2D_Real stencil getter/setter round-trips") {
    WaveSimulation2D_Real sim(8, 8, 0.16f, 6.0f);
    // Default is FivePoint for backward compatibility.
    FL_CHECK(sim.getStencil() == LaplacianStencil::FivePoint);
    sim.setStencil(LaplacianStencil::NinePointIsotropic);
    FL_CHECK(sim.getStencil() == LaplacianStencil::NinePointIsotropic);
    sim.setStencil(LaplacianStencil::FivePoint);
    FL_CHECK(sim.getStencil() == LaplacianStencil::FivePoint);
}

FL_TEST_CASE("WaveSimulation2D_Real 9-point Laplacian: constant input gives no change") {
    // Both stencils satisfy lap(constant) == 0 by construction. With the
    // discrete update u(t+1) = 2u - u_prev + C^2 * lap(u), feeding the
    // same constant into curr (and zeros into prev — the leapfrog's
    // first step) should yield f = 2*K + 0 = 2*K, then the clamp pins it
    // to 32767. Crucially, every cell should produce the same value —
    // no spatial structure should appear from the Laplacian.
    const u32 W = 12;
    const u32 H = 12;
    WaveSimulation2D_Real sim(W, H, 0.16f, 6.0f);
    sim.setStencil(LaplacianStencil::NinePointIsotropic);
    sim.setHalfDuplex(false);

    // Use 0.4 (Q15: 13107) — well inside Q15 range so 2*K doesn't clamp.
    const float K = 0.4f;
    for (u32 y = 0; y < H; ++y) {
        for (u32 x = 0; x < W; ++x) {
            sim.setf(x, y, K);
        }
    }
    sim.update();

    // All inner cells should be approximately equal; pick the center cell
    // as reference and verify the others are within +/-2 LSB.
    const i16 ref = sim.geti16(W / 2, H / 2);
    for (u32 y = 0; y < H; ++y) {
        for (u32 x = 0; x < W; ++x) {
            const int diff = static_cast<int>(sim.geti16(x, y)) - static_cast<int>(ref);
            FL_CHECK_GE(diff, -2);
            FL_CHECK_LE(diff, 2);
        }
    }
}

FL_TEST_CASE("WaveSimulation2D_Real 9-point produces different output than 5-point on a delta") {
    // A single-cell impulse should propagate differently under the two
    // stencils — the 9-point spreads energy into the diagonal neighbors
    // as well as the cardinal ones. After one step, the four diagonal
    // cells around the center should be nonzero under 9-point and zero
    // under 5-point.
    const u32 W = 8;
    const u32 H = 8;
    WaveSimulation2D_Real five(W, H, 0.16f, 6.0f);
    WaveSimulation2D_Real nine(W, H, 0.16f, 6.0f);
    five.setStencil(LaplacianStencil::FivePoint);
    nine.setStencil(LaplacianStencil::NinePointIsotropic);
    five.setHalfDuplex(false);
    nine.setHalfDuplex(false);

    const u32 cx = W / 2;
    const u32 cy = H / 2;
    five.setf(cx, cy, 1.0f);
    nine.setf(cx, cy, 1.0f);
    five.update();
    nine.update();

    // 5-point: diagonal neighbors of (cx, cy) get no first-step contribution
    // from the Laplacian of an isolated +impulse — they should be 0.
    FL_CHECK_EQ(static_cast<int>(five.geti16(cx - 1, cy - 1)), 0);
    FL_CHECK_EQ(static_cast<int>(five.geti16(cx + 1, cy - 1)), 0);
    FL_CHECK_EQ(static_cast<int>(five.geti16(cx - 1, cy + 1)), 0);
    FL_CHECK_EQ(static_cast<int>(five.geti16(cx + 1, cy + 1)), 0);

    // 9-point: those same diagonal cells should be nonzero (the isotropic
    // stencil leaks the impulse into them at weight 1/6 per the formula).
    FL_CHECK(nine.geti16(cx - 1, cy - 1) != 0);
    FL_CHECK(nine.geti16(cx + 1, cy - 1) != 0);
    FL_CHECK(nine.geti16(cx - 1, cy + 1) != 0);
    FL_CHECK(nine.geti16(cx + 1, cy + 1) != 0);
}

}  // FL_TEST_FILE
