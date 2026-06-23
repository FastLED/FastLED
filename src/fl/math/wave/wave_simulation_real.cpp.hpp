// Based on works and code by Shawn Silverman.

#include "fl/stl/stdint.h"

#include "fl/math/math.h"
#include "fl/math/wave/wave_simulation_real.h"

namespace fl {

// Define Q15 conversion constants.
// #define FIXED_SCALE (1 << 15) // 32768: 1.0 in Q15
#define INT16_POS (32767)  // Maximum value for i16
#define INT16_NEG (-32768) // Minimum value for i16

namespace wave_detail { // Anonymous namespace for internal linkage
// Convert float to fixed Q15.
// i16 float_to_fixed(float f) { return (i16)(f * FIXED_SCALE); }

i16 float_to_fixed(float f) {
    f = fl::clamp(f, -1.0f, 1.0f);
    if (f < 0.0f) {
        return (i16)(f * INT16_NEG);
    } else {
        return (i16)(f * INT16_POS); // Round to nearest
    }
}

// Convert fixed Q15 to float.
float fixed_to_float(i16 f) {
    // return ((float)f) / FIXED_SCALE;
    if (f < 0) {
        return ((float)f) / INT16_NEG; // Negative values
    } else {
        return ((float)f) / INT16_POS; // Positive values
    }
}

// // Multiply two Q15 fixed point numbers.
// i16 fixed_mul(i16 a, i16 b) {
//     return (i16)(((i32)a * b) >> 15);
// }

// Precompute the Q15 damping decay factor for a power-of-two exponent.
// The damping update is `f_new = f * (1 - 1/2^damp)`, equivalent to the
// arithmetic-shift form `f -= f >> damp` (modulo 1-LSB rounding). Caching
// it as Q15 here lets the kernel use a single Q15 multiply per cell and
// opens the door to non-power-of-two damping if the public API ever
// needs it (today it's still int-only). damp <= 0 means no decay.
i16 compute_damp_decay_q15(int damp) FL_NOEXCEPT {
    if (damp <= 0) return INT16_POS;  // ~1.0 — no decay
    const float decay = 1.0f - 1.0f / static_cast<float>(1 << damp);
    return float_to_fixed(decay);
}
} // namespace wave_detail

WaveSimulation1D_Real::WaveSimulation1D_Real(u32 len, float courantSq,
                                             int dampening) FL_NOEXCEPT
    : length(len),
      grid1(length + 2), // Initialize vector with correct size
      grid2(length + 2), // Initialize vector with correct size
      whichGrid(0),
      // CFL stability bound for the explicit leapfrog 5-point stencil in 1D
      // is C^2 <= 1. Negative speed has no physical meaning. Clamp at the
      // boundary to keep the Q15 fixed-point kernel both stable and within
      // i32 overflow margins (paired with the i64 promote in update()).
      mCourantSq(wave_detail::float_to_fixed(fl::clamp(courantSq, 0.0f, 1.0f))),
      mDampenening(dampening),
      mDampDecayQ15(wave_detail::compute_damp_decay_q15(dampening)) {
    // Additional initialization can be added here if needed.
}

void WaveSimulation1D_Real::setSpeed(float something) {
    // See constructor for clamp rationale.
    mCourantSq = wave_detail::float_to_fixed(fl::clamp(something, 0.0f, 1.0f));
}

void WaveSimulation1D_Real::setDampening(int damp) FL_NOEXCEPT {
    mDampenening = damp;
    mDampDecayQ15 = wave_detail::compute_damp_decay_q15(damp);
}

int WaveSimulation1D_Real::getDampenening() const { return mDampenening; }

float WaveSimulation1D_Real::getSpeed() const {
    return wave_detail::fixed_to_float(mCourantSq);
}

i16 WaveSimulation1D_Real::geti16(fl::size x) const {
    if (x >= length) {
        FL_WARN("Out of range.");
        return 0;
    }
    const i16 *curr = (whichGrid == 0) ? grid1.data() : grid2.data();
    return curr[x + 1];
}

i16 WaveSimulation1D_Real::geti16Previous(fl::size x) const {
    if (x >= length) {
        FL_WARN("Out of range.");
        return 0;
    }
    const i16 *prev = (whichGrid == 0) ? grid2.data() : grid1.data();
    return prev[x + 1];
}

float WaveSimulation1D_Real::getf(fl::size x) const {
    if (x >= length) {
        FL_WARN("Out of range.");
        return 0.0f;
    }
    // Retrieve value from the active grid (offset by 1 for boundary).
    const i16 *curr = (whichGrid == 0) ? grid1.data() : grid2.data();
    return wave_detail::fixed_to_float(curr[x + 1]);
}

bool WaveSimulation1D_Real::has(fl::size x) const { return (x < length); }

void WaveSimulation1D_Real::set(fl::size x, float value) {
    if (x >= length) {
        FL_WARN("warning X value too high");
        return;
    }
    i16 *curr = (whichGrid == 0) ? grid1.data() : grid2.data();
    curr[x + 1] = wave_detail::float_to_fixed(value);
}

void WaveSimulation1D_Real::update() {
    i16 *curr = (whichGrid == 0) ? grid1.data() : grid2.data();
    i16 *next = (whichGrid == 0) ? grid2.data() : grid1.data();

    // Update boundaries with a Neumann (zero-gradient) condition:
    curr[0] = curr[1];
    curr[length + 1] = curr[length];

    i32 mCourantSq32 = static_cast<i32>(mCourantSq);
    // Hoist the lower-saturation bound: in half-duplex mode the new value
    // must be >= 0 (negative parts of the wave are clipped); in full-duplex
    // mode the full Q15 negative range is allowed. Folding this into the
    // per-cell clamp via fl::clamp lets us drop the dedicated second pass
    // that used to walk the whole grid after the update loop.
    const i32 q15_min = mHalfDuplex ? 0 : -32768;
    // Iterate over each inner cell.
    for (fl::size i = 1; i < length + 1; i++) {
        // Compute the 1D Laplacian:
        // lap = curr[i+1] - 2 * curr[i] + curr[i-1]
        i32 lap =
            (i32)curr[i + 1] - ((i32)curr[i] << 1) + curr[i - 1];

        // Multiply the Laplacian by the simulation speed using Q15 arithmetic.
        // Promote to i64 before the multiply. With the 1D CFL clamp at 1.0,
        // max |mCourantSq32| = 32767 and max |lap| ~131,070 (saturated
        // alternating cells) give a worst-case product of ~4.3e9 — past
        // i32 max (2.15e9). The i64 promote also acts as a safety net if
        // the clamp is ever regressed; pre-clamp the same product could
        // already reach ~4.3e9 in 1D and ~8.6e9 in 2D.
        i32 term = static_cast<i32>(
            (static_cast<i64>(mCourantSq32) * lap) >> 15);

        // Compute the new value:
        // f = -next[i] + 2 * curr[i] + term
        i32 f = -(i32)next[i] + ((i32)curr[i] << 1) + term;

        // Apply damping: use a precomputed Q15 multiplier in place of the
        // arithmetic shift. Functionally equivalent for power-of-two damp
        // (modulo 1-LSB rounding) but generalizes cleanly to non-power-of-
        // two damping if the public API ever needs it. On Cortex-M4+ this
        // lowers to a single smmlsr/smulwb; on AVR it's a 16x32 multiply
        // (~30 cycles) — same ballpark as the shift but cleaner semantics.
        f = static_cast<i32>(
            (static_cast<i64>(f) * mDampDecayQ15) >> 15);

        // Clamp f into [q15_min, 32767] in a single step. q15_min is 0 when
        // half-duplex is on, -32768 otherwise. This subsumes both the Q15
        // saturation clamp and the post-pass that used to zero negatives.
        next[i] = static_cast<i16>(fl::clamp(f, q15_min, static_cast<i32>(32767)));
    }

    // Toggle the active grid.
    whichGrid ^= 1;
}

WaveSimulation2D_Real::WaveSimulation2D_Real(u32 W, u32 H,
                                             float speed, float dampening) FL_NOEXCEPT
    : width(W), height(H), stride(W + 2),
      grid1((W + 2) * (H + 2)),
      grid2((W + 2) * (H + 2)), whichGrid(0),
      // CFL stability bound for the explicit leapfrog 5-point stencil in 2D
      // is C^2 <= 1/2. Negative speed has no physical meaning. Clamp at the
      // boundary to keep the Q15 fixed-point kernel both stable and within
      // i32 overflow margins (paired with the i64 promote in update()).
      mCourantSq(wave_detail::float_to_fixed(fl::clamp(speed, 0.0f, 0.5f))),
      // Dampening exponent; e.g., 6 means a factor of 2^6 = 64.
      mDampening(static_cast<int>(dampening)),
      mDampDecayQ15(wave_detail::compute_damp_decay_q15(static_cast<int>(dampening))) {}

void WaveSimulation2D_Real::setSpeed(float something) {
    // See constructor for clamp rationale.
    mCourantSq = wave_detail::float_to_fixed(fl::clamp(something, 0.0f, 0.5f));
}

void WaveSimulation2D_Real::setDampening(int damp) FL_NOEXCEPT {
    mDampening = damp;
    mDampDecayQ15 = wave_detail::compute_damp_decay_q15(damp);
}

int WaveSimulation2D_Real::getDampenening() const { return mDampening; }

float WaveSimulation2D_Real::getSpeed() const {
    return wave_detail::fixed_to_float(mCourantSq);
}

float WaveSimulation2D_Real::getf(fl::size x, fl::size y) const {
    if (x >= width || y >= height) {
        FL_WARN("Out of range: " << x << ", " << y);
        return 0.0f;
    }
    const i16 *curr = (whichGrid == 0 ? grid1.data() : grid2.data());
    return wave_detail::fixed_to_float(curr[(y + 1) * stride + (x + 1)]);
}

i16 WaveSimulation2D_Real::geti16(fl::size x, fl::size y) const {
    if (x >= width || y >= height) {
        FL_WARN("Out of range: " << x << ", " << y);
        return 0;
    }
    const i16 *curr = (whichGrid == 0 ? grid1.data() : grid2.data());
    return curr[(y + 1) * stride + (x + 1)];
}

i16 WaveSimulation2D_Real::geti16Previous(fl::size x, fl::size y) const {
    if (x >= width || y >= height) {
        FL_WARN("Out of range: " << x << ", " << y);
        return 0;
    }
    const i16 *prev = (whichGrid == 0 ? grid2.data() : grid1.data());
    return prev[(y + 1) * stride + (x + 1)];
}

bool WaveSimulation2D_Real::has(fl::size x, fl::size y) const {
    return (x < width && y < height);
}

void WaveSimulation2D_Real::setf(fl::size x, fl::size y, float value) {
    i16 v = wave_detail::float_to_fixed(value);
    return seti16(x, y, v);
}

void WaveSimulation2D_Real::seti16(fl::size x, fl::size y, i16 value) {
    if (x >= width || y >= height) {
        FL_WARN("Out of range: " << x << ", " << y);
        return;
    }
    i16 *curr = (whichGrid == 0 ? grid1.data() : grid2.data());
    curr[(y + 1) * stride + (x + 1)] = value;
}

void WaveSimulation2D_Real::update() {
    i16 *curr = (whichGrid == 0 ? grid1.data() : grid2.data());
    i16 *next = (whichGrid == 0 ? grid2.data() : grid1.data());

    // Update horizontal boundaries.
    for (fl::size j = 0; j < height + 2; ++j) {
        if (mXCylindrical) {
            curr[j * stride + 0] = curr[j * stride + width];
            curr[j * stride + (width + 1)] = curr[j * stride + 1];
        } else {
            curr[j * stride + 0] = curr[j * stride + 1];
            curr[j * stride + (width + 1)] = curr[j * stride + width];
        }
    }

    // Update vertical boundaries.
    for (fl::size i = 0; i < width + 2; ++i) {
        curr[0 * stride + i] = curr[1 * stride + i];
        curr[(height + 1) * stride + i] = curr[height * stride + i];
    }

    i32 mCourantSq32 = static_cast<i32>(mCourantSq);
    // Hoist the lower-saturation bound — see WaveSimulation1D_Real::update()
    // for the rationale. Fold half-duplex into the per-cell clamp instead
    // of running a second pass over the grid (especially expensive on
    // PSRAM-backed grids).
    const i32 q15_min = mHalfDuplex ? 0 : -32768;

    // Update each inner cell. Per-row hoist: lift j*stride out of the inner
    // loop, set up row pointers to curr/next/above/below, and mark them
    // FL_RESTRICT_PARAM so the optimizer knows curr and next don't alias.
    // This gives the compiler a clean dependency picture across iterations
    // (it can vectorize / interleave loads without re-deriving the address).
    //
    // Outer-level branch on the stencil keeps the inner loop branchless;
    // the two kernels are otherwise identical (Q15 multiply, damping,
    // clamp). FivePoint is the backward-compatible default; the wrapper
    // class WaveSimulation2D auto-selects NinePointIsotropic at high
    // super-sample factors where the anisotropy of the 5-point stencil
    // becomes visually obvious.
    const bool nine_point = (mStencil == LaplacianStencil::NinePointIsotropic);
    for (fl::size j = 1; j <= height; ++j) {
        const fl::size row = j * stride;
        const i16* FL_RESTRICT_PARAM row_curr  = curr + row;
        const i16* FL_RESTRICT_PARAM row_above = curr + (row - stride);
        const i16* FL_RESTRICT_PARAM row_below = curr + (row + stride);
        i16*       FL_RESTRICT_PARAM row_next  = next + row;
        for (fl::size i = 1; i <= width; ++i) {
            const i32 c = row_curr[i];
            i32 laplacian;
            if (nine_point) {
                // 9-point isotropic Laplacian, scaled up by 6 to keep
                // everything in integer arithmetic:
                //   6 * lap = (NW+NE+SW+SE) + 4*(N+S+E+W) - 20*C
                // The /6 is folded into the term computation below so we
                // pay it once per cell rather than four times.
                const i32 diag = (i32)row_above[i - 1] + row_above[i + 1] +
                                 row_below[i - 1] + row_below[i + 1];
                const i32 nbr  = (i32)row_above[i] + row_below[i] +
                                 row_curr[i - 1] + row_curr[i + 1];
                laplacian = diag + (nbr << 2) - 20 * c;
            } else {
                // Standard 5-point Laplacian: N + S + E + W - 4*C.
                laplacian = (i32)row_curr[i + 1] + row_curr[i - 1] +
                            row_above[i] + row_below[i] - (c << 2);
            }
            // Promote to i64 before the multiply. With the 2D CFL clamp at
            // 0.5, the 5-point worst case product is ~4.3e9 and the
            // 9-point (scaled by 6, so |lap| ~6x larger) is ~2.6e10 — both
            // past i32 max (2.15e9). The i64 promote also acts as a safety
            // net if the clamp is ever regressed.
            i64 product = static_cast<i64>(mCourantSq32) * laplacian;
            i32 term;
            if (nine_point) {
                // Undo the x6 scaling on the 9-point Laplacian here.
                // Integer division by a compile-time-constant 6 is lowered
                // to a reciprocal-multiply on Cortex-M4+ and to a small
                // shift+add on AVR; either way it's well under the cost
                // of computing the Laplacian itself.
                term = static_cast<i32>((product >> 15) / 6);
            } else {
                term = static_cast<i32>(product >> 15);
            }
            // f = -next[index] + 2 * curr[index] + mCourantSq * laplacian.
            i32 f = -(i32)row_next[i] + (c << 1) + term;

            // Apply damping with the precomputed Q15 decay multiplier —
            // see the 1D update for the rationale.
            f = static_cast<i32>(
                (static_cast<i64>(f) * mDampDecayQ15) >> 15);

            // Clamp f into [q15_min, 32767] in a single step — subsumes
            // both the Q15 saturation clamp and the half-duplex zero pass.
            row_next[i] = static_cast<i16>(fl::clamp(f, q15_min, static_cast<i32>(32767)));
        }
    }

    // Swap the roles of the grids.
    whichGrid ^= 1;
}

} // namespace fl
