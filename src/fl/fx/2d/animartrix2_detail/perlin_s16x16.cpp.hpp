#pragma once
// allow-include-after-namespace

// 2D Perlin noise implementation using s16x16 fixed-point arithmetic
// Implementation file - included from perlin_s16x16.h

namespace animartrix2_detail {

inline void perlin_s16x16::init_fade_lut(fl::i32 *table) {
    for (int i = 0; i <= 256; i++) {
        fl::i64 t = static_cast<fl::i64>(i) * (HP_ONE / 256);
        fl::i64 t2 = (t * t) >> HP_BITS;
        fl::i64 t3 = (t2 * t) >> HP_BITS;
        fl::i64 inner = (t * (6LL * HP_ONE)) >> HP_BITS;
        inner -= 15LL * HP_ONE;
        inner = (t * inner) >> HP_BITS;
        inner += 10LL * HP_ONE;
        table[i] = static_cast<fl::i32>((t3 * inner) >> HP_BITS);
    }
}

inline fl::s16x16 perlin_s16x16::pnoise2d(fl::s16x16 fx, fl::s16x16 fy,
                                          const fl::i32 *fade_lut,
                                          const fl::u8 *perm) {
    return fl::s16x16::from_raw(
        pnoise2d_raw(fx.raw(), fy.raw(), fade_lut, perm));
}

inline fl::i32 perlin_s16x16::pnoise2d_raw(fl::i32 fx_raw, fl::i32 fy_raw,
                                            const fl::i32 *fade_lut,
                                            const fl::u8 *perm) {
    int X, Y;
    fl::i32 x, y;
    floor_frac(fx_raw, X, x);
    floor_frac(fy_raw, Y, y);
    X &= 255;
    Y &= 255;

    fl::i32 u = fade(x, fade_lut);
    fl::i32 v = fade(y, fade_lut);

    int A  = perm[X & 255]       + Y;
    int AA = perm[A & 255];
    int AB = perm[(A + 1) & 255];
    int B  = perm[(X + 1) & 255] + Y;
    int BA = perm[B & 255];
    int BB = perm[(B + 1) & 255];

    fl::i32 result = lerp(v,
        lerp(u, grad(perm[AA & 255], x,          y),
                grad(perm[BA & 255], x - HP_ONE, y)),
        lerp(u, grad(perm[AB & 255], x,          y - HP_ONE),
                grad(perm[BB & 255], x - HP_ONE, y - HP_ONE)));

    return result >> (HP_BITS - fl::s16x16::FRAC_BITS);
}

FASTLED_FORCE_INLINE void perlin_s16x16::floor_frac(fl::i32 fp16, int &ifloor,
                                            fl::i32 &frac24) {
    ifloor = fp16 >> FP_BITS;
    frac24 = (fp16 & (FP_ONE - 1)) << (HP_BITS - FP_BITS);
}

FASTLED_FORCE_INLINE fl::i32 perlin_s16x16::fade(fl::i32 t, const fl::i32 *table) {
    fl::u32 idx = static_cast<fl::u32>(t) >> 16;
    fl::i32 frac = t & 0xFFFF;
    fl::i32 a = table[idx];
    fl::i32 b = table[idx + 1];
    return a + static_cast<fl::i32>(
        (static_cast<fl::i64>(frac) * (b - a)) >> 16);
}

FASTLED_FORCE_INLINE fl::i32 perlin_s16x16::lerp(fl::i32 t, fl::i32 a, fl::i32 b) {
    return a + static_cast<fl::i32>(
        (static_cast<fl::i64>(t) * (b - a)) >> HP_BITS);
}

FASTLED_FORCE_INLINE fl::i32 perlin_s16x16::grad(int hash, fl::i32 x, fl::i32 y) {
    struct GradCoeff { fl::i8 cx; fl::i8 cy; };
    constexpr GradCoeff lut[16] = {
        { 1,  1}, {-1,  1}, { 1, -1}, {-1, -1},
        { 1,  0}, {-1,  0}, { 1,  0}, {-1,  0},
        { 0,  1}, { 0, -1}, { 0,  1}, { 0, -1},
        { 1,  1}, { 0, -1}, {-1,  1}, { 0, -1},
    };
    const GradCoeff &g = lut[hash & 15];
    return g.cx * x + g.cy * y;
}

}  // namespace animartrix2_detail
