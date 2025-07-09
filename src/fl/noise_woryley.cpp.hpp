#include "noise_woryley.h"

namespace fl {
namespace {

constexpr i32 Q15_ONE = 32768; // 1.0 in Q15
// constexpr i32 Q15_HALF = Q15_ONE / 2;

// Helper: multiply two Q15 numbers (result in Q15)
// i32 q15_mul(i32 a, i32 b) {
//     return (i32)(((int64_t)a * b) >> 15);
// }

// Helper: absolute difference
i32 q15_abs(i32 a) { return a < 0 ? -a : a; }

// Pseudo-random hash based on grid coordinates
u16 hash(i32 x, i32 y) {
    u32 n = (u32)(x * 374761393 + y * 668265263);
    n = (n ^ (n >> 13)) * 1274126177;
    return (u16)((n ^ (n >> 16)) & 0xFFFF);
}

// Get fractional feature point inside a grid cell
void feature_point(i32 gx, i32 gy, i32 &fx, i32 &fy) {
    u16 h = hash(gx, gy);
    fx = (h & 0xFF) * 128; // scale to Q15 (0–32767)
    fy = ((h >> 8) & 0xFF) * 128;
}
} // namespace

// Compute 2D Worley noise at (x, y) in Q15
i32 worley_noise_2d_q15(i32 x, i32 y) {
    i32 cell_x = x >> 15;
    i32 cell_y = y >> 15;

    i32 min_dist = INT32_MAX;

    // Check surrounding 9 cells
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            i32 gx = cell_x + dx;
            i32 gy = cell_y + dy;

            i32 fx, fy;
            feature_point(gx, gy, fx, fy);

            i32 feature_x = (gx << 15) + fx;
            i32 feature_y = (gy << 15) + fy;

            i32 dx_q15 = x - feature_x;
            i32 dy_q15 = y - feature_y;

            // Approximate distance using Manhattan (faster) or Euclidean
            // (costlier)
            i32 dist =
                q15_abs(dx_q15) + q15_abs(dy_q15); // Manhattan distance

            if (dist < min_dist)
                min_dist = dist;
        }
    }

    // Normalize: maximum possible distance is roughly 2*Q15_ONE
    return (min_dist << 15) / (2 * Q15_ONE);
}

} // namespace fl
