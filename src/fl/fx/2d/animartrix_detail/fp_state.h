#pragma once

// Generic fixed-point state for Animartrix2 visualizers.
// Holds SoA (Structure-of-Arrays) per-pixel geometry cache and Perlin fade LUT.
// Each FP visualizer owns an FPVizState instance as a private member.

#include "fl/stl/align.h"
#include "fl/stl/fixed_point/s16x16.h"
#include "fl/fx/2d/animartrix_detail/perlin_s16x16.h"
#include "fl/fx/2d/animartrix_detail/engine.h"
#include "fl/stl/stdint.h"
#include "fl/stl/vector.h"

namespace fl {

// FL_ALIGNAS(16): aligns the struct to 16 bytes for potential SIMD loads.
struct FL_ALIGNAS(16) FPVizState {
    // SoA pixel geometry — built once when grid size changes, reused every frame.
    // Padded to next multiple of 4 for SIMD safety.
    fl::vector<fl::i32> polar_theta_raw;   // atan2(dy, dx), raw s16x16
    fl::vector<fl::i32> distance_raw;      // hypot(dx, dy), raw s16x16
    fl::vector<fl::i32> sqrt_distance_raw; // sqrt(distance), raw s16x16
    fl::vector<fl::u16> pixel_idx;         // pre-mapped xyMap(x,y) LED index
    int count = 0;

    // Perlin fade LUT (257 entries, Q8.24 format).
    FL_ALIGNAS(16) fl::i32 fade_lut[257];
    bool fade_lut_initialized = false;

    FPVizState() : fade_lut{}, fade_lut_initialized(false) {}

    // Rebuild per-pixel cache when grid changes.
    // Called once per frame; rebuilds only if pixel count changed.
    void ensureCache(Engine *e) {
        const int num_x = e->num_x;
        const int num_y = e->num_y;
        const int total_pixels = num_x * num_y;

        if (count != total_pixels) {
            const int padded = (total_pixels + 3) & ~3;
            polar_theta_raw.resize(padded, 0);
            distance_raw.resize(padded, 0);
            sqrt_distance_raw.resize(padded, 0);
            pixel_idx.resize(padded, 0);

            int idx = 0;
            for (int x = 0; x < num_x; x++) {
                for (int y = 0; y < num_y; y++) {
                    polar_theta_raw[idx] = fl::s16x16(e->polar_theta[x][y]).raw();
                    distance_raw[idx] = fl::s16x16(e->distance[x][y]).raw();
                    sqrt_distance_raw[idx] = fl::s16x16(e->distance[x][y]).sqrt().raw();
                    pixel_idx[idx] = e->mCtx->xyMapFn(x, y, e->mCtx->xyMapUserData);
                    idx++;
                }
            }
            count = total_pixels;
        }

        if (!fade_lut_initialized) {
            perlin_s16x16::init_fade_lut(fade_lut);
            fade_lut_initialized = true;
        }
    }
};

}  // namespace fl
