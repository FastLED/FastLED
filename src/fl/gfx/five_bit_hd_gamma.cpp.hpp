#include "fl/gfx/five_bit_hd_gamma.h"
#include "fl/stl/align.h"
#include "fl/gfx/ease.h"
#include "fl/fastled.h"

namespace fl {

namespace {
namespace five_bit_impl {

// ix/31 * 255/65536 * 256 scaling factors, valid for indexes 1..31.
// Uses flash storage on AVR/ESP, zero heap allocation on all platforms.
FL_ALIGN_PROGMEM(4) static constexpr u32 BRIGHT_SCALE[32] FL_PROGMEM = {
    0,      2023680, 1011840, 674560, 505920, 404736, 337280, 289097,
    252960, 224853,  202368,  183971, 168640, 155668, 144549, 134912,
    126480, 119040,  112427,  106509, 101184, 96366,  91985,  87986,
    84320,  80947,   77834,   74951,  72274,  69782,  67456,  65280};

// Inlined gamma lookup using assume_aligned on the shared GAMMA_2_8_LUT.
// Avoids cross-TU function call overhead of gamma_2_8() in hot span loops.
FL_ALWAYS_INLINE u16 gamma_lut_read(const u16 *lut, u8 idx) {
    return FL_PGM_READ_WORD_ALIGNED(&lut[idx]);
}

// Branchless scale16by8: (i * (1 + scale)) >> 8.
// Eliminates the if(scale==0) branch from the standard scale16by8.
// The caller is responsible for skipping the call when scale==0.
FL_ALWAYS_INLINE u16 scale16by8_nozero(u16 i, u16 scale_plus_one) {
    return static_cast<u16>((static_cast<u32>(i) * scale_plus_one) >> 8);
}

// Core per-pixel transform, fully inlined. All uniform values are
// precomputed by the caller and passed in to avoid per-pixel branches.
FL_ALWAYS_INLINE void five_bit_pixel(
    u16 r16, u16 g16, u16 b16, u8 brightness,
    // Precomputed: (1 + brightness), or 0 if brightness==0xff (skip scaling)
    u16 bright_p1, bool apply_brightness,
    CRGB *out, u8 *out_power_5bit) {

    // All-zero fast path (rare but worth checking — writes are cheap).
    if ((r16 | g16 | b16) == 0) {
        *out = CRGB(0, 0, 0);
        *out_power_5bit = (brightness <= 31) ? brightness : 31;
        return;
    }

    // Apply brightness: branchless multiply, skip only if brightness==0xff.
    if (apply_brightness) {
        r16 = scale16by8_nozero(r16, bright_p1);
        g16 = scale16by8_nozero(g16, bright_p1);
        b16 = scale16by8_nozero(b16, bright_p1);
    }

    // max3 via branchless ternary (compiler emits cmov on x86, csel on ARM).
    u16 scale = r16;
    if (g16 > scale) scale = g16;
    if (b16 > scale) scale = b16;

    if (scale == 0) {
        *out = CRGB(0, 0, 0);
        *out_power_5bit = 0;
        return;
    }

    // 5-bit quantized scale at or above maximum value.
    scale = (scale + (2047 - (scale >> 5))) >> 11;

    u32 scalef = FL_PGM_READ_DWORD_ALIGNED(&BRIGHT_SCALE[scale]);
    u8 r8 = static_cast<u8>((r16 * scalef + 0x808000) >> 24);
    u8 g8 = static_cast<u8>((g16 * scalef + 0x808000) >> 24);
    u8 b8 = static_cast<u8>((b16 * scalef + 0x808000) >> 24);

    *out = CRGB(r8, g8, b8);
    *out_power_5bit = static_cast<u8>(scale);
}

} // namespace five_bit_impl
} // anonymous namespace

FL_OPTIMIZE_FUNCTION
void five_bit_hd_gamma_bitshift(
    fl::span<const CRGB> colors, CRGB colors_scale, u8 global_brightness,
    fl::span<CRGB> out_colors, fl::span<u8> out_power_5bit) {

    u16 n = static_cast<u16>(colors.size());
    if (out_colors.size() < n) n = static_cast<u16>(out_colors.size());
    if (out_power_5bit.size() < n) n = static_cast<u16>(out_power_5bit.size());

    // Brightness==0: zero everything and return immediately.
    if (global_brightness == 0) {
        for (u16 i = 0; i < n; ++i) {
            out_colors[i] = CRGB(0, 0, 0);
            out_power_5bit[i] = 0;
        }
        return;
    }

    const u16 *glut = assume_aligned<64>(GAMMA_2_8_LUT);

    // Precompute color-scale multipliers once (avoid per-pixel branches).
    const bool apply_r_scale = (colors_scale.r != 0xff);
    const bool apply_g_scale = (colors_scale.g != 0xff);
    const bool apply_b_scale = (colors_scale.b != 0xff);
    const u16 rscale_p1 = 1u + static_cast<u16>(colors_scale.r);
    const u16 gscale_p1 = 1u + static_cast<u16>(colors_scale.g);
    const u16 bscale_p1 = 1u + static_cast<u16>(colors_scale.b);

    // Precompute brightness multiplier once.
    const bool apply_brightness = (global_brightness != 0xff);
    const u16 bright_p1 = 1u + static_cast<u16>(global_brightness);

    for (u16 i = 0; i < n; ++i) {
        const CRGB &c = colors[i];

        u16 r16 = five_bit_impl::gamma_lut_read(glut, c.r);
        u16 g16 = five_bit_impl::gamma_lut_read(glut, c.g);
        u16 b16 = five_bit_impl::gamma_lut_read(glut, c.b);

        // Color-scale: branchless multiply, guarded by precomputed bools.
        if (apply_r_scale) r16 = five_bit_impl::scale16by8_nozero(r16, rscale_p1);
        if (apply_g_scale) g16 = five_bit_impl::scale16by8_nozero(g16, gscale_p1);
        if (apply_b_scale) b16 = five_bit_impl::scale16by8_nozero(b16, bscale_p1);

        five_bit_impl::five_bit_pixel(r16, g16, b16, global_brightness,
                                      bright_p1, apply_brightness,
                                      &out_colors[i], &out_power_5bit[i]);
    }
}

FL_OPTIMIZE_FUNCTION
void five_bit_hd_gamma_bitshift(
    fl::span<const CRGB> colors, CRGB colors_scale, u8 global_brightness,
    fl::span<CRGBA5> out) {

    u16 n = static_cast<u16>(colors.size());
    if (out.size() < n) n = static_cast<u16>(out.size());

    // Brightness==0: zero everything and return immediately.
    if (global_brightness == 0) {
        for (u16 i = 0; i < n; ++i) {
            out[i].color = CRGB(0, 0, 0);
            out[i].brightness_5bit = 0;
        }
        return;
    }

    const u16 *glut = assume_aligned<64>(GAMMA_2_8_LUT);

    // Precompute color-scale multipliers once (avoid per-pixel branches).
    const bool apply_r_scale = (colors_scale.r != 0xff);
    const bool apply_g_scale = (colors_scale.g != 0xff);
    const bool apply_b_scale = (colors_scale.b != 0xff);
    const u16 rscale_p1 = 1u + static_cast<u16>(colors_scale.r);
    const u16 gscale_p1 = 1u + static_cast<u16>(colors_scale.g);
    const u16 bscale_p1 = 1u + static_cast<u16>(colors_scale.b);

    // Precompute brightness multiplier once.
    const bool apply_brightness = (global_brightness != 0xff);
    const u16 bright_p1 = 1u + static_cast<u16>(global_brightness);

    for (u16 i = 0; i < n; ++i) {
        const CRGB &c = colors[i];

        u16 r16 = five_bit_impl::gamma_lut_read(glut, c.r);
        u16 g16 = five_bit_impl::gamma_lut_read(glut, c.g);
        u16 b16 = five_bit_impl::gamma_lut_read(glut, c.b);

        // Color-scale: branchless multiply, guarded by precomputed bools.
        if (apply_r_scale) r16 = five_bit_impl::scale16by8_nozero(r16, rscale_p1);
        if (apply_g_scale) g16 = five_bit_impl::scale16by8_nozero(g16, gscale_p1);
        if (apply_b_scale) b16 = five_bit_impl::scale16by8_nozero(b16, bscale_p1);

        five_bit_impl::five_bit_pixel(r16, g16, b16, global_brightness,
                                      bright_p1, apply_brightness,
                                      &out[i].color, &out[i].brightness_5bit);
    }
}

} // namespace fl
