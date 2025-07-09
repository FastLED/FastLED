// Based on works and code by Shawn Silverman.

#include "fl/stdint.h"

#include "fl/clamp.h"
#include "fl/namespace.h"
#include "fl/wave_simulation_real.h"

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
} // namespace wave_detail

using namespace wave_detail;

WaveSimulation1D_Real::WaveSimulation1D_Real(u32 len, float courantSq,
                                             int dampening)
    : length(len),
      grid1(length + 2), // Initialize vector with correct size
      grid2(length + 2), // Initialize vector with correct size
      whichGrid(0),
      mCourantSq(float_to_fixed(courantSq)), mDampenening(dampening) {
    // Additional initialization can be added here if needed.
}

void WaveSimulation1D_Real::setSpeed(float something) {
    mCourantSq = float_to_fixed(something);
}

void WaveSimulation1D_Real::setDampening(int damp) { mDampenening = damp; }

int WaveSimulation1D_Real::getDampenening() const { return mDampenening; }

float WaveSimulation1D_Real::getSpeed() const {
    return fixed_to_float(mCourantSq);
}

i16 WaveSimulation1D_Real::geti16(fl::size x) const {
    if (x >= length) {
        FASTLED_WARN("Out of range.");
        return 0;
    }
    const i16 *curr = (whichGrid == 0) ? grid1.data() : grid2.data();
    return curr[x + 1];
}

i16 WaveSimulation1D_Real::geti16Previous(fl::size x) const {
    if (x >= length) {
        FASTLED_WARN("Out of range.");
        return 0;
    }
    const i16 *prev = (whichGrid == 0) ? grid2.data() : grid1.data();
    return prev[x + 1];
}

float WaveSimulation1D_Real::getf(fl::size x) const {
    if (x >= length) {
        FASTLED_WARN("Out of range.");
        return 0.0f;
    }
    // Retrieve value from the active grid (offset by 1 for boundary).
    const i16 *curr = (whichGrid == 0) ? grid1.data() : grid2.data();
    return fixed_to_float(curr[x + 1]);
}

bool WaveSimulation1D_Real::has(fl::size x) const { return (x < length); }

void WaveSimulation1D_Real::set(fl::size x, float value) {
    if (x >= length) {
        FASTLED_WARN("warning X value too high");
        return;
    }
    i16 *curr = (whichGrid == 0) ? grid1.data() : grid2.data();
    curr[x + 1] = float_to_fixed(value);
}

void WaveSimulation1D_Real::update() {
    i16 *curr = (whichGrid == 0) ? grid1.data() : grid2.data();
    i16 *next = (whichGrid == 0) ? grid2.data() : grid1.data();

    // Update boundaries with a Neumann (zero-gradient) condition:
    curr[0] = curr[1];
    curr[length + 1] = curr[length];

    // Compute dampening factor as an integer value: 2^(mDampenening)
    i32 dampening_factor = 1 << mDampenening;

    i32 mCourantSq32 = static_cast<i32>(mCourantSq);
    // Iterate over each inner cell.
    for (fl::size i = 1; i < length + 1; i++) {
        // Compute the 1D Laplacian:
        // lap = curr[i+1] - 2 * curr[i] + curr[i-1]
        i32 lap =
            (i32)curr[i + 1] - ((i32)curr[i] << 1) + curr[i - 1];

        // Multiply the Laplacian by the simulation speed using Q15 arithmetic:
        i32 term = (mCourantSq32 * lap) >> 15;

        // Compute the new value:
        // f = -next[i] + 2 * curr[i] + term
        i32 f = -(i32)next[i] + ((i32)curr[i] << 1) + term;

        // Apply damping:
        f = f - (f / dampening_factor);

        // Clamp the result to the Q15 range [-32768, 32767].
        if (f > 32767)
            f = 32767;
        else if (f < -32768)
            f = -32768;

        next[i] = (i16)f;
    }

    if (mHalfDuplex) {
        // Set the negative values to zero.
        for (fl::size i = 1; i < length + 1; i++) {
            if (next[i] < 0) {
                next[i] = 0;
            }
        }
    }

    // Toggle the active grid.
    whichGrid ^= 1;
}

WaveSimulation2D_Real::WaveSimulation2D_Real(u32 W, u32 H,
                                             float speed, float dampening)
    : width(W), height(H), stride(W + 2),
      grid1((W + 2) * (H + 2)),
      grid2((W + 2) * (H + 2)), whichGrid(0),
      // Initialize speed 0.16 in fixed Q15
      mCourantSq(float_to_fixed(speed)),
      // Dampening exponent; e.g., 6 means a factor of 2^6 = 64.
      mDampening(dampening) {}

void WaveSimulation2D_Real::setSpeed(float something) {
    mCourantSq = float_to_fixed(something);
}

void WaveSimulation2D_Real::setDampening(int damp) { mDampening = damp; }

int WaveSimulation2D_Real::getDampenening() const { return mDampening; }

float WaveSimulation2D_Real::getSpeed() const {
    return fixed_to_float(mCourantSq);
}

float WaveSimulation2D_Real::getf(fl::size x, fl::size y) const {
    if (x >= width || y >= height) {
        FASTLED_WARN("Out of range: " << x << ", " << y);
        return 0.0f;
    }
    const i16 *curr = (whichGrid == 0 ? grid1.data() : grid2.data());
    return fixed_to_float(curr[(y + 1) * stride + (x + 1)]);
}

i16 WaveSimulation2D_Real::geti16(fl::size x, fl::size y) const {
    if (x >= width || y >= height) {
        FASTLED_WARN("Out of range: " << x << ", " << y);
        return 0;
    }
    const i16 *curr = (whichGrid == 0 ? grid1.data() : grid2.data());
    return curr[(y + 1) * stride + (x + 1)];
}

i16 WaveSimulation2D_Real::geti16Previous(fl::size x, fl::size y) const {
    if (x >= width || y >= height) {
        FASTLED_WARN("Out of range: " << x << ", " << y);
        return 0;
    }
    const i16 *prev = (whichGrid == 0 ? grid2.data() : grid1.data());
    return prev[(y + 1) * stride + (x + 1)];
}

bool WaveSimulation2D_Real::has(fl::size x, fl::size y) const {
    return (x < width && y < height);
}

void WaveSimulation2D_Real::setf(fl::size x, fl::size y, float value) {
    i16 v = float_to_fixed(value);
    return seti16(x, y, v);
}

void WaveSimulation2D_Real::seti16(fl::size x, fl::size y, i16 value) {
    if (x >= width || y >= height) {
        FASTLED_WARN("Out of range: " << x << ", " << y);
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

    // Compute the dampening factor as an integer: 2^(dampening).
    i32 dampening_factor = 1 << mDampening; // e.g., 6 -> 64
    i32 mCourantSq32 = static_cast<i32>(mCourantSq);

    // Update each inner cell.
    for (fl::size j = 1; j <= height; ++j) {
        for (fl::size i = 1; i <= width; ++i) {
            int index = j * stride + i;
            // Laplacian: sum of four neighbors minus 4 times the center.
            i32 laplacian = (i32)curr[index + 1] + curr[index - 1] +
                                curr[index + stride] + curr[index - stride] -
                                ((i32)curr[index] << 2);
            // Compute the new value:
            // f = - next[index] + 2 * curr[index] + mCourantSq * laplacian
            // The multiplication is in Q15, so we shift right by 15.
            i32 term = (mCourantSq32 * laplacian) >> 15;
            i32 f =
                -(i32)next[index] + ((i32)curr[index] << 1) + term;

            // Apply damping:
            f = f - (f / dampening_factor);

            // Clamp f to the Q15 range.
            if (f > 32767)
                f = 32767;
            else if (f < -32768)
                f = -32768;

            next[index] = (i16)f;
        }
    }

    if (mHalfDuplex) {
        // Set negative values to zero.
        for (fl::size j = 1; j <= height; ++j) {
            for (fl::size i = 1; i <= width; ++i) {
                int index = j * stride + i;
                if (next[index] < 0) {
                    next[index] = 0;
                }
            }
        }
    }

    // Swap the roles of the grids.
    whichGrid ^= 1;
}

} // namespace fl
