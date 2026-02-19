#pragma once

// Per-animation persistent cache for Chasing Spirals variants.
// Holds the SoA (Structure-of-Arrays) pixel geometry and the Perlin fade LUT.
//
// Lifetime: declared as a module-level global in chasing_spirals.cpp.hpp.
// Assumes single-threaded access (no locking).
// Future: convert to a class with private instance data when multiple
// concurrent animation instances are needed.

#include "fl/align.h"
#include "fl/compiler_control.h"
#include "fl/stl/stdint.h"
#include "fl/stl/vector.h"

namespace fl {

// FL_ALIGNAS(16): aligns the struct itself to 16 bytes, satisfying SSE2
// requirements for load_u32_4_aligned at BOUNDARY A in the SIMD inner loop.
//
// The fl::vector heap buffers inside this struct rely on the platform allocator
// providing >= 16-byte alignment for allocations >= 16 bytes (guaranteed by
// glibc, musl, Darwin, Windows CRT, and ESP-IDF heap on all SIMD-capable targets).
struct FL_ALIGNAS(16) ChasingSpiralState {
    // SoA pixel geometry — built once when grid size changes, reused every frame.
    // Each array has `count` valid entries, padded to the next multiple of 4
    // (so SIMD loads never read past the allocation).
    fl::vector<fl::i32> base_angle;    // 3*theta - dist/3, raw s16x16
    fl::vector<fl::i32> dist_scaled;   // distance * 0.1, raw s16x16
    fl::vector<fl::i32> rf3;           // 3 * radial_filter, raw s16x16 (red)
    fl::vector<fl::i32> rf_half;       // radial_filter / 2, raw s16x16 (green)
    fl::vector<fl::i32> rf_quarter;    // radial_filter / 4, raw s16x16 (blue)
    fl::vector<fl::u16> pixel_idx;     // pre-mapped xyMap(x,y) LED index
    int count = 0;

    // Perlin fade LUT (257 entries, Q8.24 format).
    // FL_ALIGNAS(16): enables aligned SIMD loads in pnoise2d_raw_simd4.
    FL_ALIGNAS(16) fl::i32 fade_lut[257];
    bool fade_lut_initialized = false;

    ChasingSpiralState() : fade_lut{}, fade_lut_initialized(false) {}
};

}  // namespace fl
