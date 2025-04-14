#include "wave.h"

#include "fl/namespace.h"

FASTLED_NAMESPACE_BEGIN

// Define Q15 conversion constants.
#define FIXED_SCALE (1 << 15) // 32768: 1.0 in Q15
#define FIXED_ONE (FIXED_SCALE)

// Convert float to fixed Q15.
static inline int16_t float_to_fixed(float f) {
    return (int16_t)(f * FIXED_SCALE);
}

// Convert fixed Q15 to float.
static inline float fixed_to_float(int16_t f) {
    return ((float)f) / FIXED_SCALE;
}

// Multiply two Q15 fixed point numbers.
static inline int16_t fixed_mul(int16_t a, int16_t b) {
    return (int16_t)(((int32_t)a * b) >> 15);
}

WaveSimulation2D::WaveSimulation2D(uint32_t W, uint32_t H, float courantSq, float dampening)
    : width(W), height(H), stride(W + 2),
      grid1(new int16_t[(W + 2) * (H + 2)]()),
      grid2(new int16_t[(W + 2) * (H + 2)]()),
      whichGrid(0),
      // Initialize speed 0.16 in fixed Q15
      mCourantSq(float_to_fixed(courantSq)),
      // Dampening exponent; e.g., 6 means a factor of 2^6 = 64.
      mDampening(dampening) {}

void WaveSimulation2D::setSpeed(float something) {
    mCourantSq = float_to_fixed(something);
}

void WaveSimulation2D::setDampenening(int damp) { mDampening = damp; }

int WaveSimulation2D::getDampenening() const { return mDampening; }

float WaveSimulation2D::getSpeed() const { return fixed_to_float(mCourantSq); }

float WaveSimulation2D::getf(size_t x, size_t y) const {
    if (x >= width || y >= height) {
        FASTLED_WARN("Out of range: " << x << ", " << y);
        return 0.0f;
    }
    const int16_t *curr = (whichGrid == 0 ? grid1.get() : grid2.get());
    return fixed_to_float(curr[(y + 1) * stride + (x + 1)]);
}

bool WaveSimulation2D::has(size_t x, size_t y) const {
    return (x < width && y < height);
}

void WaveSimulation2D::set(size_t x, size_t y, float value) {
    if (x >= width || y >= height) {
        FASTLED_WARN("Out of range: " << x << ", " << y);
        return;
    }
    int16_t *curr = (whichGrid == 0 ? grid1.get() : grid2.get());
    curr[(y + 1) * stride + (x + 1)] = float_to_fixed(value);
}

void WaveSimulation2D::update() {
    int16_t *curr = (whichGrid == 0 ? grid1.get() : grid2.get());
    int16_t *next = (whichGrid == 0 ? grid2.get() : grid1.get());

    // Update horizontal boundaries.
    for (size_t j = 0; j < height + 2; ++j) {
        curr[j * stride + 0] = curr[j * stride + 1];
        curr[j * stride + (width + 1)] = curr[j * stride + width];
    }

    // Update vertical boundaries.
    for (size_t i = 0; i < width + 2; ++i) {
        curr[0 * stride + i] = curr[1 * stride + i];
        curr[(height + 1) * stride + i] = curr[height * stride + i];
    }

    // Compute the dampening factor as an integer: 2^(dampening).
    int32_t dampening_factor = 1 << mDampening; // e.g., 6 -> 64

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
            int32_t term = ((int32_t)mCourantSq * laplacian) >> 15;
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

    // Swap the roles of the grids.
    whichGrid ^= 1;
}

FASTLED_NAMESPACE_END
