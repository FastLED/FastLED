#pragma once
// allow-include-after-namespace

// 2D Perlin noise Q16 implementation
// Implementation file - included from perlin_q16.h

namespace animartrix2_detail {

inline void perlin_q16::init_fade_lut(fl::i32 *table) {
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

inline fl::s16x16 perlin_q16::pnoise2d(fl::s16x16 fx, fl::s16x16 fy,
                                      const fl::i32 *fade_lut,
                                      const fl::u8 *perm) {
    return fl::s16x16::from_raw(
        pnoise2d_raw(fx.raw(), fy.raw(), fade_lut, perm));
}

inline fl::i32 perlin_q16::pnoise2d_raw(fl::i32 fx_raw, fl::i32 fy_raw,
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

    // No shift needed: already in Q16 format, matches s16x16::FRAC_BITS
    return result;
}

FASTLED_FORCE_INLINE void perlin_q16::floor_frac(fl::i32 fp16, int &ifloor,
                                            fl::i32 &frac16) {
    ifloor = fp16 >> FP_BITS;
    frac16 = fp16 & (FP_ONE - 1); // Already Q16, no shift needed
}

FASTLED_FORCE_INLINE fl::i32 perlin_q16::fade(fl::i32 t, const fl::i32 *table) {
    fl::u32 idx = static_cast<fl::u32>(t) >> 8; // Q16 → 8-bit index
    fl::i32 frac = t & 0xFF;
    fl::i32 a = table[idx];
    fl::i32 b = table[idx + 1];
    // Lerp in Q16: frac is 8 bits, expand to 16 for precision
    return a + static_cast<fl::i32>(
        (static_cast<fl::i32>(frac << 8) * (b - a)) >> 16);
}

FASTLED_FORCE_INLINE fl::i32 perlin_q16::lerp(fl::i32 t, fl::i32 a, fl::i32 b) {
    // All values in Q16, result stays Q16
    return a + static_cast<fl::i32>(
        (static_cast<fl::i64>(t) * (b - a)) >> HP_BITS);
}

FASTLED_FORCE_INLINE fl::i32 perlin_q16::grad(int hash, fl::i32 x, fl::i32 y) {
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
