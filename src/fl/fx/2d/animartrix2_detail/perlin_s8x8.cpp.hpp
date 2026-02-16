#pragma once
// allow-include-after-namespace

// 2D Perlin noise s8x8 implementation
// Implementation file - included from perlin_s8x8.h

namespace animartrix2_detail {

inline void perlin_s8x8::init_fade_lut(fl::i32 *table) {
    for (int i = 0; i <= 256; i++) {
        fl::i16 t = static_cast<fl::i16>((i * HP_ONE) / 256);
        fl::i16 t2 = static_cast<fl::i16>((static_cast<fl::i32>(t) * t) >> HP_BITS);
        fl::i16 t3 = static_cast<fl::i16>((static_cast<fl::i32>(t2) * t) >> HP_BITS);
        fl::i16 inner = static_cast<fl::i16>((static_cast<fl::i32>(t) * (6 * HP_ONE)) >> HP_BITS);
        inner -= 15 * HP_ONE;
        inner = static_cast<fl::i16>((static_cast<fl::i32>(t) * inner) >> HP_BITS);
        inner += 10 * HP_ONE;
        table[i] = static_cast<fl::i32>((static_cast<fl::i32>(t3) * inner) >> HP_BITS);
    }
}

inline fl::s16x16 perlin_s8x8::pnoise2d(fl::s16x16 fx, fl::s16x16 fy,
                                      const fl::i32 *fade_lut,
                                      const fl::u8 *perm) {
    return fl::s16x16::from_raw(
        pnoise2d_raw(fx.raw(), fy.raw(), fade_lut, perm));
}

inline fl::i32 perlin_s8x8::pnoise2d_raw(fl::i32 fx_raw, fl::i32 fy_raw,
                                        const fl::i32 *fade_lut,
                                        const fl::u8 *perm) {
    int X, Y;
    fl::i16 x, y;
    floor_frac(fx_raw, X, x);
    floor_frac(fy_raw, Y, y);
    X &= 255;
    Y &= 255;

    fl::i16 u = fade(x, fade_lut);
    fl::i16 v = fade(y, fade_lut);

    int A  = perm[X & 255]       + Y;
    int AA = perm[A & 255];
    int AB = perm[(A + 1) & 255];
    int B  = perm[(X + 1) & 255] + Y;
    int BA = perm[B & 255];
    int BB = perm[(B + 1) & 255];

    fl::i16 result = lerp(v,
        lerp(u, grad(perm[AA & 255], x,          y),
                grad(perm[BA & 255], x - HP_ONE, y)),
        lerp(u, grad(perm[AB & 255], x,          y - HP_ONE),
                grad(perm[BB & 255], x - HP_ONE, y - HP_ONE)));

    // Shift from Q8 to s16x16's Q16 format
    return static_cast<fl::i32>(result) << (fl::s16x16::FRAC_BITS - HP_BITS);
}

FASTLED_FORCE_INLINE void perlin_s8x8::floor_frac(fl::i32 fp16, int &ifloor,
                                            fl::i16 &frac8) {
    ifloor = fp16 >> FP_BITS;
    fl::i32 frac16 = fp16 & (FP_ONE - 1);
    // Shift from 16 frac bits to 8 frac bits
    frac8 = static_cast<fl::i16>(frac16 >> (FP_BITS - HP_BITS));
}

FASTLED_FORCE_INLINE fl::i16 perlin_s8x8::fade(fl::i16 t, const fl::i32 *table) {
    // t is Q8 (0-255 range), use directly as index
    fl::u8 idx = static_cast<fl::u8>(t);
    // Return LUT value, convert from i32 to i16 (values fit in Q8 range)
    return static_cast<fl::i16>(table[idx]);
}

FASTLED_FORCE_INLINE fl::i16 perlin_s8x8::lerp(fl::i16 t, fl::i16 a, fl::i16 b) {
    // All values in Q8, result stays Q8
    return static_cast<fl::i16>(
        a + (((static_cast<fl::i32>(t) * (b - a)) >> HP_BITS)));
}

FASTLED_FORCE_INLINE fl::i16 perlin_s8x8::grad(int hash, fl::i16 x, fl::i16 y) {
    struct GradCoeff { fl::i8 cx; fl::i8 cy; };
    constexpr GradCoeff lut[16] = {
        { 1,  1}, {-1,  1}, { 1, -1}, {-1, -1},
        { 1,  0}, {-1,  0}, { 1,  0}, {-1,  0},
        { 0,  1}, { 0, -1}, { 0,  1}, { 0, -1},
        { 1,  1}, { 0, -1}, {-1,  1}, { 0, -1},
    };
    const GradCoeff &g = lut[hash & 15];
    return static_cast<fl::i16>(g.cx * x + g.cy * y);
}

}  // namespace animartrix2_detail
