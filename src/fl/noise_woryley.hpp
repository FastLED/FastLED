#include "noise_woryley.h"

namespace fl {
namespace {

constexpr int32_t Q15_ONE = 32768; // 1.0 in Q15
// constexpr int32_t Q15_HALF = Q15_ONE / 2;

// Helper: multiply two Q15 numbers (result in Q15)
// int32_t q15_mul(int32_t a, int32_t b) {
//     return (int32_t)(((int64_t)a * b) >> 15);
// }

// Helper: absolute difference
int32_t q15_abs(int32_t a) { return a < 0 ? -a : a; }

// Pseudo-random hash based on grid coordinates
uint16_t hash(int32_t x, int32_t y) {
    uint32_t n = (uint32_t)(x * 374761393 + y * 668265263);
    n = (n ^ (n >> 13)) * 1274126177;
    return (uint16_t)((n ^ (n >> 16)) & 0xFFFF);
}

// Get fractional feature point inside a grid cell
void feature_point(int32_t gx, int32_t gy, int32_t &fx, int32_t &fy) {
    uint16_t h = hash(gx, gy);
    fx = (h & 0xFF) * 128; // scale to Q15 (0–32767)
    fy = ((h >> 8) & 0xFF) * 128;
}
} // namespace

// Compute 2D Worley noise at (x, y) in Q15
int32_t worley_noise_2d_q15(int32_t x, int32_t y) {
    int32_t cell_x = x >> 15;
    int32_t cell_y = y >> 15;

    int32_t min_dist = INT32_MAX;

    // Check surrounding 9 cells
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int32_t gx = cell_x + dx;
            int32_t gy = cell_y + dy;

            int32_t fx, fy;
            feature_point(gx, gy, fx, fy);

            int32_t feature_x = (gx << 15) + fx;
            int32_t feature_y = (gy << 15) + fy;

            int32_t dx_q15 = x - feature_x;
            int32_t dy_q15 = y - feature_y;

            // Approximate distance using Manhattan (faster) or Euclidean
            // (costlier)
            int32_t dist =
                q15_abs(dx_q15) + q15_abs(dy_q15); // Manhattan distance

            if (dist < min_dist)
                min_dist = dist;
        }
    }

    // Normalize: maximum possible distance is roughly 2*Q15_ONE
    return (min_dist << 15) / (2 * Q15_ONE);
}

} // namespace fl
