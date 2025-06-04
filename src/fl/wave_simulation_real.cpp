// Based on works and code by Shawn Silverman.

#include <stdint.h>

#include "fl/clamp.h"
#include "fl/namespace.h"
#include "fl/wave_simulation_real.h"

namespace fl {

// Define Q15 conversion constants.
// #define FIXED_SCALE (1 << 15) // 32768: 1.0 in Q15
#define INT16_POS (32767)  // Maximum value for int16_t
#define INT16_NEG (-32768) // Minimum value for int16_t

namespace wave_detail { // Anonymous namespace for internal linkage
// Convert float to fixed Q15.
// int16_t float_to_fixed(float f) { return (int16_t)(f * FIXED_SCALE); }

int16_t float_to_fixed(float f) {
    f = fl::clamp(f, -1.0f, 1.0f);
    if (f < 0.0f) {
        return (int16_t)(f * INT16_NEG);
    } else {
        return (int16_t)(f * INT16_POS); // Round to nearest
    }
}

// Convert fixed Q15 to float.
float fixed_to_float(int16_t f) {
    // return ((float)f) / FIXED_SCALE;
    if (f < 0) {
        return ((float)f) / INT16_NEG; // Negative values
    } else {
        return ((float)f) / INT16_POS; // Positive values
    }
}

// // Multiply two Q15 fixed point numbers.
// int16_t fixed_mul(int16_t a, int16_t b) {
//     return (int16_t)(((int32_t)a * b) >> 15);
// }
} // namespace wave_detail

using namespace wave_detail;

WaveSimulation1D_Real::WaveSimulation1D_Real(uint32_t len, float courantSq,
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

int16_t WaveSimulation1D_Real::geti16(size_t x) const {
    if (x >= length) {
        FASTLED_WARN("Out of range.");
        return 0;
    }
    const int16_t *curr = (whichGrid == 0) ? grid1.data() : grid2.data();
    return curr[x + 1];
}

int16_t WaveSimulation1D_Real::geti16Previous(size_t x) const {
    if (x >= length) {
        FASTLED_WARN("Out of range.");
        return 0;
    }
    const int16_t *prev = (whichGrid == 0) ? grid2.data() : grid1.data();
    return prev[x + 1];
}

float WaveSimulation1D_Real::getf(size_t x) const {
    if (x >= length) {
        FASTLED_WARN("Out of range.");
        return 0.0f;
    }
    // Retrieve value from the active grid (offset by 1 for boundary).
    const int16_t *curr = (whichGrid == 0) ? grid1.data() : grid2.data();
    return fixed_to_float(curr[x + 1]);
}

bool WaveSimulation1D_Real::has(size_t x) const { return (x < length); }

void WaveSimulation1D_Real::set(size_t x, float value) {
    if (x >= length) {
        FASTLED_WARN("warning X value too high");
        return;
    }
    int16_t *curr = (whichGrid == 0) ? grid1.data() : grid2.data();
    curr[x + 1] = float_to_fixed(value);
}

void WaveSimulation1D_Real::update() {
    int16_t *curr = (whichGrid == 0) ? grid1.data() : grid2.data();
    int16_t *next = (whichGrid == 0) ? grid2.data() : grid1.data();

    // Update boundaries with a Neumann (zero-gradient) condition:
    curr[0] = curr[1];
    curr[length + 1] = curr[length];

    // Compute dampening factor as an integer value: 2^(mDampenening)
    int32_t dampening_factor = 1 << mDampenening;

    int32_t mCourantSq32 = static_cast<int32_t>(mCourantSq);
    // Iterate over each inner cell.
    for (size_t i = 1; i < length + 1; i++) {
        // Compute the 1D Laplacian:
        // lap = curr[i+1] - 2 * curr[i] + curr[i-1]
        int32_t lap =
            (int32_t)curr[i + 1] - ((int32_t)curr[i] << 1) + curr[i - 1];

        // Multiply the Laplacian by the simulation speed using Q15 arithmetic:
        int32_t term = (mCourantSq32 * lap) >> 15;

        // Compute the new value:
        // f = -next[i] + 2 * curr[i] + term
        int32_t f = -(int32_t)next[i] + ((int32_t)curr[i] << 1) + term;

        // Apply damping:
        f = f - (f / dampening_factor);

        // Clamp the result to the Q15 range [-32768, 32767].
        if (f > 32767)
            f = 32767;
        else if (f < -32768)
            f = -32768;

        next[i] = (int16_t)f;
    }

    if (mHalfDuplex) {
        // Set the negative values to zero.
        for (size_t i = 1; i < length + 1; i++) {
            if (next[i] < 0) {
                next[i] = 0;
            }
        }
    }

    // Toggle the active grid.
    whichGrid ^= 1;
}

WaveSimulation2D_Real::WaveSimulation2D_Real(uint32_t W, uint32_t H,
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

float WaveSimulation2D_Real::getf(size_t x, size_t y) const {
    if (x >= width || y >= height) {
        FASTLED_WARN("Out of range: " << x << ", " << y);
        return 0.0f;
    }
    const int16_t *curr = (whichGrid == 0 ? grid1.data() : grid2.data());
    return fixed_to_float(curr[(y + 1) * stride + (x + 1)]);
}

int16_t WaveSimulation2D_Real::geti16(size_t x, size_t y) const {
    if (x >= width || y >= height) {
        FASTLED_WARN("Out of range: " << x << ", " << y);
        return 0;
    }
    const int16_t *curr = (whichGrid == 0 ? grid1.data() : grid2.data());
    return curr[(y + 1) * stride + (x + 1)];
}

int16_t WaveSimulation2D_Real::geti16Previous(size_t x, size_t y) const {
    if (x >= width || y >= height) {
        FASTLED_WARN("Out of range: " << x << ", " << y);
        return 0;
    }
    const int16_t *prev = (whichGrid == 0 ? grid2.data() : grid1.data());
    return prev[(y + 1) * stride + (x + 1)];
}

bool WaveSimulation2D_Real::has(size_t x, size_t y) const {
    return (x < width && y < height);
}

void WaveSimulation2D_Real::setf(size_t x, size_t y, float value) {
    int16_t v = float_to_fixed(value);
    return seti16(x, y, v);
}

void WaveSimulation2D_Real::seti16(size_t x, size_t y, int16_t value) {
    if (x >= width || y >= height) {
        FASTLED_WARN("Out of range: " << x << ", " << y);
        return;
    }
    int16_t *curr = (whichGrid == 0 ? grid1.data() : grid2.data());
    curr[(y + 1) * stride + (x + 1)] = value;
}

void WaveSimulation2D_Real::update() {
    int16_t *curr = (whichGrid == 0 ? grid1.data() : grid2.data());
    int16_t *next = (whichGrid == 0 ? grid2.data() : grid1.data());

    // Update horizontal boundaries.
    for (size_t j = 0; j < height + 2; ++j) {
        if (mXCylindrical) {
            curr[j * stride + 0] = curr[j * stride + width];
            curr[j * stride + (width + 1)] = curr[j * stride + 1];
        } else {
            curr[j * stride + 0] = curr[j * stride + 1];
            curr[j * stride + (width + 1)] = curr[j * stride + width];
        }
    }

    // Update vertical boundaries.
    for (size_t i = 0; i < width + 2; ++i) {
        curr[0 * stride + i] = curr[1 * stride + i];
        curr[(height + 1) * stride + i] = curr[height * stride + i];
    }

    // Compute the dampening factor as an integer: 2^(dampening).
    int32_t dampening_factor = 1 << mDampening; // e.g., 6 -> 64
    int32_t mCourantSq32 = static_cast<int32_t>(mCourantSq);

    // Update each inner cell.
    for (size_t j = 1; j <= height; ++j) {
        for (size_t i = 1; i <= width; ++i) {
            int index = j * stride + i;
            // Laplacian: sum of four neighbors minus 4 times the center.
            int32_t laplacian = (int32_t)curr[index + 1] + curr[index - 1] +
                                curr[index + stride] + curr[index - stride] -
                                ((int32_t)curr[index] << 2);
            // Compute the new value:
            // f = - next[index] + 2 * curr[index] + mCourantSq * laplacian
            // The multiplication is in Q15, so we shift right by 15.
            int32_t term = (mCourantSq32 * laplacian) >> 15;
            int32_t f =
                -(int32_t)next[index] + ((int32_t)curr[index] << 1) + term;

            // Apply damping:
            f = f - (f / dampening_factor);

            // Clamp f to the Q15 range.
            if (f > 32767)
                f = 32767;
            else if (f < -32768)
                f = -32768;

            next[index] = (int16_t)f;
        }
    }

    if (mHalfDuplex) {
        // Set negative values to zero.
        for (size_t j = 1; j <= height; ++j) {
            for (size_t i = 1; i <= width; ++i) {
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
