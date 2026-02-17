#pragma once

// 2D Perlin noise i16-optimized implementation
// Extracted from animartrix2_detail.hpp
//
// i16-optimized Perlin: Uses i16 for lerp/grad hot path (2x faster multiplies)
// Coordinates stay as i32 (s16x16) externally but convert to i16 for interpolation

#include "fl/fixed_point/s16x16.h"
#include "fl/force_inline.h"

namespace fl {

struct perlin_i16_optimized {
    static constexpr int HP_BITS = 16;
    static constexpr fl::i32 HP_ONE = 1 << HP_BITS;

    // Build fade LUT - still i32 for API compatibility
    static void init_fade_lut(fl::i32 *table);

    // Public API: accepts s16x16 raw values
    static fl::s16x16 pnoise2d(fl::s16x16 fx, fl::s16x16 fy,
                                      const fl::i32 *fade_lut,
                                      const fl::u8 *perm);

    // Hot path: uses i16 arithmetic for lerp/grad after extracting fractional part
    static fl::i32 pnoise2d_raw(fl::i32 fx_raw, fl::i32 fy_raw,
                                        const fl::i32 *fade_lut,
                                        const fl::u8 *perm);

  private:
    static constexpr int FP_BITS = fl::s16x16::FRAC_BITS;
    static constexpr fl::i32 FP_ONE = 1 << FP_BITS;
    // For i16 coordinates: use full 16-bit value (no room for 1.0 in i16)
    // Coordinates are fractional parts only (0-65535), HP_ONE stays as i32
    static constexpr fl::i32 HP_ONE_I16 = HP_ONE;

    // Extract fractional part as i16 (range 0-65535)
    static FASTLED_FORCE_INLINE void floor_frac_i16(fl::i32 fp16, int &ifloor, fl::i16 &frac16);

    // Fade optimized for i16 input (coordinates), returns i32 (can be 0-65536)
    static FASTLED_FORCE_INLINE fl::i32 fade(fl::i16 t, const fl::i32 *table);

    // Standard lerp (i32), but benefits from smaller grad results
    static FASTLED_FORCE_INLINE fl::i32 lerp(fl::i32 t, fl::i32 a, fl::i32 b);

    // i16 grad: Takes i16 coordinates (faster than i32), returns i32
    static FASTLED_FORCE_INLINE fl::i32 grad_i16(int hash, fl::i16 x, fl::i16 y);
};

}  // namespace fl
