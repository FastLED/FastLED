#pragma once
// allow-include-after-namespace

// 2D Perlin noise i16-optimized implementation
// Implementation file - included from perlin_i16_optimized.h

namespace animartrix2_detail {

inline void perlin_i16_optimized::init_fade_lut(fl::i32 *table) {
    for (int i = 0; i <= 256; i++) {
        fl::i32 t = (i * HP_ONE) / 256;
        fl::i32 t2 = static_cast<fl::i32>((static_cast<fl::i64>(t) * t) >> HP_BITS);
        fl::i32 t3 = static_cast<fl::i32>((static_cast<fl::i64>(t2) * t) >> HP_BITS);
        fl::i32 inner = static_cast<fl::i32>((static_cast<fl::i64>(t) * (6 * HP_ONE)) >> HP_BITS);
        inner -= 15 * HP_ONE;
        inner = static_cast<fl::i32>((static_cast<fl::i64>(t) * inner) >> HP_BITS);
        inner += 10 * HP_ONE;
        table[i] = static_cast<fl::i32>((static_cast<fl::i64>(t3) * inner) >> HP_BITS);
    }
}

inline fl::s16x16 perlin_i16_optimized::pnoise2d(fl::s16x16 fx, fl::s16x16 fy,
                                      const fl::i32 *fade_lut,
                                      const fl::u8 *perm) {
    return fl::s16x16::from_raw(
        pnoise2d_raw(fx.raw(), fy.raw(), fade_lut, perm));
}

inline fl::i32 perlin_i16_optimized::pnoise2d_raw(fl::i32 fx_raw, fl::i32 fy_raw,
                                        const fl::i32 *fade_lut,
                                        const fl::u8 *perm) {
    int X, Y;
    fl::i16 x16, y16;  // i16 fractional parts!
    floor_frac_i16(fx_raw, X, x16);
    floor_frac_i16(fy_raw, Y, y16);
    X &= 255;
    Y &= 255;

    // Fade values can be up to 65536, need to stay i32
    // But we can still optimize grad and internal lerp to use i16
    fl::i32 u = fade(x16, fade_lut);
    fl::i32 v = fade(y16, fade_lut);

    int A  = perm[X & 255]       + Y;
    int AA = perm[A & 255];
    int AB = perm[(A + 1) & 255];
    int B  = perm[(X + 1) & 255] + Y;
    int BA = perm[B & 255];
    int BB = perm[(B + 1) & 255];

    // grad returns i16 (coordinates are i16), lerp takes i32 fade values
    fl::i32 g00 = grad_i16(perm[AA & 255], x16,              y16);
    fl::i32 g10 = grad_i16(perm[BA & 255], x16 - HP_ONE_I16, y16);
    fl::i32 g01 = grad_i16(perm[AB & 255], x16,              y16 - HP_ONE_I16);
    fl::i32 g11 = grad_i16(perm[BB & 255], x16 - HP_ONE_I16, y16 - HP_ONE_I16);

    // Lerp with i32 (grad results fit in i16 range but lerp needs i32 for fade values)
    fl::i32 lerp0 = lerp(u, g00, g10);
    fl::i32 lerp1 = lerp(u, g01, g11);
    fl::i32 result = lerp(v, lerp0, lerp1);

    return result;
}

FASTLED_FORCE_INLINE void perlin_i16_optimized::floor_frac_i16(fl::i32 fp16, int &ifloor, fl::i16 &frac16) {
    ifloor = fp16 >> FP_BITS;
    frac16 = static_cast<fl::i16>(fp16 & (FP_ONE - 1));  // Cast to i16
}

FASTLED_FORCE_INLINE fl::i32 perlin_i16_optimized::fade(fl::i16 t, const fl::i32 *table) {
    fl::u32 idx = static_cast<fl::u32>(t) >> 8;  // i16 → 8-bit index
    fl::i32 frac = t & 0xFF;
    fl::i32 a = table[idx];
    fl::i32 b = table[idx + 1];
    return a + ((static_cast<fl::i32>(frac << 8) * (b - a)) >> 16);
}

FASTLED_FORCE_INLINE fl::i32 perlin_i16_optimized::lerp(fl::i32 t, fl::i32 a, fl::i32 b) {
    return a + static_cast<fl::i32>(
        (static_cast<fl::i64>(t) * (b - a)) >> HP_BITS);
}

FASTLED_FORCE_INLINE fl::i32 perlin_i16_optimized::grad_i16(int hash, fl::i16 x, fl::i16 y) {
    struct GradCoeff { fl::i8 cx; fl::i8 cy; };
    constexpr GradCoeff lut[16] = {
        { 1,  1}, {-1,  1}, { 1, -1}, {-1, -1},
        { 1,  0}, {-1,  0}, { 1,  0}, {-1,  0},
        { 0,  1}, { 0, -1}, { 0,  1}, { 0, -1},
        { 1,  1}, { 0, -1}, {-1,  1}, { 0, -1},
    };
    const GradCoeff &g = lut[hash & 15];
    // i8 × i16 → i32 (safe, result in range ±65535)
    return static_cast<fl::i32>(g.cx * x) + static_cast<fl::i32>(g.cy * y);
}

}  // namespace animartrix2_detail
