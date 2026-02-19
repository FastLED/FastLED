// Chasing_Spirals Q31 SIMD implementation (fixed-point, 4-wide vectorized)
//
// ============================================================================
// PIPELINE SUMMARY (SoA layout + full SIMD optimization)
// ============================================================================
//
// Per-frame state is kept in ChasingSpiralState (SoA layout, engine.h):
//   base_angle[], dist_scaled[], rf3[], rf_half[], rf_quarter[], pixel_idx[]
//   fade_lut[257]  (FL_ALIGNAS(16) for aligned Perlin LUT loads)
//
// SIMD inner loop (simd4_processChannel, 4 pixels per batch):
//   SoA load (load_u32_4)              ← was: AoS gather [Boundary 1 — GONE]
//   Q16.16 → A24 (mulhi_su32_4)
//   sincos32_simd                      ← fully SIMD (unchanged)
//   Perlin coords (mulhi32_i32_4 << 1) ← was: unpack→scalar i64 [Boundary 3 — GONE]
//   store nx/ny to 4×i32 stack arrays
//   Permutation table (scalar)         ← fundamental SSE2 limit, unavoidable
//   Fade LUT + lerp tree (scalar)      ← exact-match tests forbid vectorization
//   load result into SIMD
//   Clamp [0,FP_ONE] + scale ×255      ← was: scalar loop [Boundary 4b — GONE]
//   Radial filter (mulhi32_i32_4)      ← was: AoS gather + scalar [Boundary 6 — GONE]
//   extract_u32_4 × 4 + scatter        ← pixel_idx scatter remains scalar
//
// Precision: mulhi32_i32_4(cos_Q31, dist_Q16) << 1 loses bit 31 of the product
//   — at most ±1 ULP in Q16.16 coordinate space ≈ 1/65536 of the Perlin period.
//   Within the 6-LSB pixel tolerance (avg<1%, max≤6 at t=1000).
// ============================================================================

#include "fl/align.h"
#include "fl/fx/2d/animartrix2_detail/engine.h"
#include "fl/fx/2d/animartrix2_detail/chasing_spiral_state.h"
#include "fl/fx/2d/animartrix2_detail/perlin_s16x16.h"
#include "fl/fx/2d/animartrix2_detail/perlin_s16x16_simd.h"
#include "fl/fx/2d/animartrix2_detail/perlin_float.h"
#include "fl/simd.h"
#include "fl/sin32.h"
#include "fl/fx/2d/animartrix2_detail/chasing_spirals.h"

namespace fl {

namespace {

using FP = fl::s16x16;
using Perlin = perlin_s16x16;

// Common setup values returned by setupChasingSpiralFrame.
// Carries raw SoA pointers (no PixelLUT AoS struct).
struct FrameSetup {
    int total_pixels;
    const fl::i32 *base_angle;
    const fl::i32 *dist_scaled;
    const fl::i32 *rf3;
    const fl::i32 *rf_half;
    const fl::i32 *rf_quarter;
    const fl::u16 *pixel_idx;
    const fl::i32 *fade_lut;
    const fl::u8  *perm;
    fl::i32 cx_raw;
    fl::i32 cy_raw;
    fl::i32 lin0_raw;
    fl::i32 lin1_raw;
    fl::i32 lin2_raw;
    fl::i32 rad0_raw;
    fl::i32 rad1_raw;
    fl::i32 rad2_raw;
    CRGB *leds;
};

// Convert s16x16 angle (radians) to A24 format for sincos32
u32 radiansToA24(i32 base_s16x16, i32 offset_s16x16) {
    constexpr i32 RAD_TO_A24 = 2670177;
    return static_cast<u32>((static_cast<i64>(base_s16x16 + offset_s16x16) * RAD_TO_A24) >> FP::FRAC_BITS);
}

// Clamp s16x16 value to [0, 1] and scale to [0, 255]
i32 clampAndScale255(i32 raw_s16x16) {
    constexpr i32 FP_ONE = 1 << FP::FRAC_BITS;
    if (raw_s16x16 < 0) raw_s16x16 = 0;
    if (raw_s16x16 > FP_ONE) raw_s16x16 = FP_ONE;
    return raw_s16x16 * 255;
}

// Apply radial filter to noise value and clamp to [0, 255]
i32 applyRadialFilter(i32 noise_255, i32 rf_raw) {
    i32 result = static_cast<i32>((static_cast<i64>(noise_255) * rf_raw) >> (FP::FRAC_BITS * 2));
    if (result < 0) result = 0;
    if (result > 255) result = 255;
    return result;
}

// Process one color channel for 4 pixels using a full SIMD pipeline.
// Returns 4 clamped [0, 255] channel values in a simd_u32x4 register.
//
// base_vec, dist_vec: SoA fields already loaded into SIMD (no AoS gather).
// rf_vec: radial-filter multiplier for this channel, also from SoA.
simd::simd_u32x4 simd4_processChannel(
    simd::simd_u32x4 base_vec, simd::simd_u32x4 dist_vec,
    i32 radial_offset, i32 linear_offset,
    const i32 *fade_lut, const u8 *perm, i32 cx_raw, i32 cy_raw,
    simd::simd_u32x4 rf_vec) {

    constexpr i32 RAD_TO_A24 = 2670177;

    // Q16.16 → A24: (base + radial_offset) * RAD_TO_A24 >> 16
    simd::simd_u32x4 offset_vec    = simd::set1_u32_4(static_cast<u32>(radial_offset));
    simd::simd_u32x4 sum_vec       = simd::add_i32_4(base_vec, offset_vec);
    simd::simd_u32x4 rad_const_vec = simd::set1_u32_4(static_cast<u32>(RAD_TO_A24));
    simd::simd_u32x4 angles_vec    = simd::mulhi_su32_4(sum_vec, rad_const_vec);

    // sincos32_simd returns Q31 values (cos_vals, sin_vals)
    SinCos32_simd sc = sincos32_simd(angles_vec);

    // Perlin coordinates (stays in SIMD — no scalar round-trip):
    //   (cos_Q31 * dist_Q16) >> 31 == mulhi32_i32_4(cos, dist) << 1
    //   Precision: loses bit 31 of the 64-bit product → ±1 ULP in Q16.16 coords.
    simd::simd_u32x4 lin_cx = simd::set1_u32_4(static_cast<u32>(linear_offset + cx_raw));
    simd::simd_u32x4 cy_vec  = simd::set1_u32_4(static_cast<u32>(cy_raw));
    simd::simd_u32x4 nx_vec  = simd::sub_i32_4(lin_cx,
        simd::sll_u32_4(simd::mulhi32_i32_4(sc.cos_vals, dist_vec), 1));
    simd::simd_u32x4 ny_vec  = simd::sub_i32_4(cy_vec,
        simd::sll_u32_4(simd::mulhi32_i32_4(sc.sin_vals, dist_vec), 1));

    // Store to aligned stack arrays for permutation table lookup (no SSE2 gather)
    FL_ALIGNAS(16) i32 nx[4], ny[4];
    simd::store_u32_4(reinterpret_cast<u32*>(nx), nx_vec); // ok reinterpret cast
    simd::store_u32_4(reinterpret_cast<u32*>(ny), ny_vec); // ok reinterpret cast

    // Perlin noise — exact scalar lerp tree (preserves scalar==SIMD test invariant)
    simd::simd_u32x4 raw_vec = perlin_s16x16_simd::pnoise2d_raw_simd4_vec(
        nx, ny, fade_lut, perm);

    // Clamp to [0, FP_ONE] and scale by 255: (val << 8) - val
    simd::simd_u32x4 zero   = simd::set1_u32_4(0u);
    simd::simd_u32x4 fp_one = simd::set1_u32_4(static_cast<u32>(1 << FP::FRAC_BITS));
    simd::simd_u32x4 clamped = simd::min_i32_4(simd::max_i32_4(raw_vec, zero), fp_one);
    simd::simd_u32x4 noise_scaled = simd::sub_i32_4(simd::sll_u32_4(clamped, 8), clamped);

    // Radial filter: (noise_scaled * rf) >> 32, clamped to [0, 255]
    simd::simd_u32x4 max255 = simd::set1_u32_4(255u);
    simd::simd_u32x4 result = simd::mulhi32_i32_4(noise_scaled, rf_vec);
    result = simd::max_i32_4(result, zero);
    result = simd::min_i32_4(result, max255);
    return result;
}

// Extract common frame setup logic shared by all variants.
// Builds SoA geometry cache lazily (once when grid size changes).
FrameSetup setupChasingSpiralFrame(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    // Timing (once per frame, float is fine here)
    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.1;
    e->timings.ratio[1] = 0.13;
    e->timings.ratio[2] = 0.16;
    e->timings.offset[1] = 10;
    e->timings.offset[2] = 20;
    e->timings.offset[3] = 30;
    e->calculate_oscillators(e->timings);

    const int num_x = e->num_x;
    const int num_y = e->num_y;
    const int total_pixels = num_x * num_y;

    // Per-frame constants (float->FP boundary conversions)
    constexpr FP scale(0.1f);
    const FP radius_fp(e->radial_filter_radius);
    const FP center_x_scaled = FP(e->animation.center_x * 0.1f);
    const FP center_y_scaled = FP(e->animation.center_y * 0.1f);

    const FP radial0(e->move.radial[0]);
    const FP radial1(e->move.radial[1]);
    const FP radial2(e->move.radial[2]);

    // Reduce linear offsets modulo the Perlin noise period before converting
    // to s16x16. Two reasons:
    //   1. Prevents s16x16 overflow (range ±32767 in integer part).
    //   2. Float32 precision fix: matches the same reduction applied in
    //      Chasing_Spirals_Float (animartrix v1 and v2 float paths) so both
    //      paths compute identical Perlin coordinates at all time values.
    //      Without this reduction, float32 loses per-pixel coordinate precision
    //      when move.linear grows large (ULP at 200,000 ≈ 0.024 > pixel step 0.1).
    // Perlin noise is exactly periodic with period 256 at integer coordinates,
    // so with scale_x=0.1 the effective period for offset_x is 256/0.1 = 2560.
    // See: tests/fl/fx/2d/animartrix2.cpp "period reduction" test cases.
    constexpr float perlin_period = 2560.0f; // 256.0f / scale_x(0.1f)
    constexpr float scale_f = 0.1f;
    const FP linear0_scaled = FP(fmodf(e->move.linear[0], perlin_period) * scale_f);
    const FP linear1_scaled = FP(fmodf(e->move.linear[1], perlin_period) * scale_f);
    const FP linear2_scaled = FP(fmodf(e->move.linear[2], perlin_period) * scale_f);

    constexpr FP three_fp(3.0f);
    constexpr FP one(1.0f);

    // Allocate per-animation state lazily
    if (!e->mChasingSpiralState) {
        e->mChasingSpiralState = new ChasingSpiralState();
    }
    ChasingSpiralState *state = e->mChasingSpiralState;

    // Build per-pixel SoA geometry (once when grid size changes)
    if (state->count != total_pixels) {
        const int padded = (total_pixels + 3) & ~3;  // multiple of 4 for SIMD safety
        state->base_angle.resize(padded, 0);
        state->dist_scaled.resize(padded, 0);
        state->rf3.resize(padded, 0);
        state->rf_half.resize(padded, 0);
        state->rf_quarter.resize(padded, 0);
        state->pixel_idx.resize(padded, 0);

        const FP inv_radius = one / radius_fp;
        const FP one_third = one / three_fp;
        int idx = 0;
        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {
                const FP theta(e->polar_theta[x][y]);
                const FP dist(e->distance[x][y]);
                const FP rf = (radius_fp - dist) * inv_radius;
                state->base_angle[idx]  = (three_fp * theta - dist * one_third).raw();
                state->dist_scaled[idx] = (dist * scale).raw();
                state->rf3[idx]         = (three_fp * rf).raw();
                state->rf_half[idx]     = (rf >> 1).raw();
                state->rf_quarter[idx]  = (rf >> 2).raw();
                state->pixel_idx[idx]   = e->mCtx->xyMapFn(x, y, e->mCtx->xyMapUserData);
                idx++;
            }
        }
        state->count = total_pixels;
    }

    // Initialize Perlin fade LUT once per state lifetime
    if (!state->fade_lut_initialized) {
        Perlin::init_fade_lut(state->fade_lut);
        state->fade_lut_initialized = true;
    }

    const i32 cx_raw   = center_x_scaled.raw();
    const i32 cy_raw   = center_y_scaled.raw();
    const i32 lin0_raw = linear0_scaled.raw();
    const i32 lin1_raw = linear1_scaled.raw();
    const i32 lin2_raw = linear2_scaled.raw();
    const i32 rad0_raw = radial0.raw();
    const i32 rad1_raw = radial1.raw();
    const i32 rad2_raw = radial2.raw();

    return FrameSetup{
        total_pixels,
        state->base_angle.data(),
        state->dist_scaled.data(),
        state->rf3.data(),
        state->rf_half.data(),
        state->rf_quarter.data(),
        state->pixel_idx.data(),
        state->fade_lut,
        PERLIN_NOISE,
        cx_raw,
        cy_raw,
        lin0_raw,
        lin1_raw,
        lin2_raw,
        rad0_raw,
        rad1_raw,
        rad2_raw,
        e->mCtx->leds
    };
}

} // anonymous namespace

// ============================================================================
// Float Implementation (original algorithm, uses v2 Engine)
// ============================================================================

void Chasing_Spirals_Float(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    // Perlin noise is periodic with period 256 at integer coordinates.
    // scale_x = 0.1, so the effective period for offset_x is 256/0.1 = 2560.
    // Reducing move.linear[i] modulo this period before use keeps float32
    // coordinate arithmetic precise even at extreme uptime values.
    // Without this, float32 loses per-pixel precision when adding a small
    // per-pixel term (~0.1) to a large offset (e.g. 200,000), since float32
    // ULP at that magnitude (~0.024) is coarser than the per-pixel step.
    // This matches the reduction already applied in setupChasingSpiralFrame
    // for the Q31 path, keeping both paths in agreement at all time values.
    static constexpr float perlin_period = 2560.0f; // 256.0f / scale_x(0.1f)

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.1;
    e->timings.ratio[1] = 0.13;
    e->timings.ratio[2] = 0.16;
    e->timings.offset[1] = 10;
    e->timings.offset[2] = 20;
    e->timings.offset[3] = 30;
    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.angle =
                3 * e->polar_theta[x][y] + e->move.radial[0] -
                e->distance[x][y] / 3;
            e->animation.dist = e->distance[x][y];
            e->animation.scale_z = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.scale_x = 0.1;
            e->animation.offset_x = fl::fmodf(e->move.linear[0], perlin_period);
            e->animation.offset_y = 0;
            e->animation.offset_z = 0;
            e->animation.z = 0;
            float show1 = e->render_value(e->animation);

            e->animation.angle =
                3 * e->polar_theta[x][y] + e->move.radial[1] -
                e->distance[x][y] / 3;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_x = fl::fmodf(e->move.linear[1], perlin_period);
            float show2 = e->render_value(e->animation);

            e->animation.angle =
                3 * e->polar_theta[x][y] + e->move.radial[2] -
                e->distance[x][y] / 3;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_x = fl::fmodf(e->move.linear[2], perlin_period);
            float show3 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial_filter = (radius - e->distance[x][y]) / radius;

            e->pixel.red = 3 * show1 * radial_filter;
            e->pixel.green = show2 * radial_filter / 2;
            e->pixel.blue = show3 * radial_filter / 4;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

// ============================================================================
// Q31 Scalar Implementation (fixed-point, non-vectorized)
// ============================================================================

void Chasing_Spirals_Q31(Context &ctx) {
    auto setup = setupChasingSpiralFrame(ctx);
    const int total_pixels  = setup.total_pixels;
    const i32 *fade_lut     = setup.fade_lut;
    const u8  *perm         = setup.perm;
    const i32  cx_raw       = setup.cx_raw;
    const i32  cy_raw       = setup.cy_raw;
    const i32  lin0_raw     = setup.lin0_raw;
    const i32  lin1_raw     = setup.lin1_raw;
    const i32  lin2_raw     = setup.lin2_raw;
    const i32  rad0_raw     = setup.rad0_raw;
    const i32  rad1_raw     = setup.rad1_raw;
    const i32  rad2_raw     = setup.rad2_raw;
    CRGB      *leds         = setup.leds;

    constexpr i32 FP_ONE    = 1 << FP::FRAC_BITS;
    constexpr i32 RAD_TO_A24 = 2670177;

    auto noise_channel = [&](i32 base_raw, i32 rad_raw,
                             i32 lin_raw, i32 dist_raw) -> i32 {
        u32 a24 = static_cast<u32>(
            (static_cast<i64>(base_raw + rad_raw) * RAD_TO_A24) >> FP::FRAC_BITS);
        SinCos32 sc = sincos32(a24);
        i32 nx = lin_raw + cx_raw -
            static_cast<i32>((static_cast<i64>(sc.cos_val) * dist_raw) >> 31);
        i32 ny = cy_raw -
            static_cast<i32>((static_cast<i64>(sc.sin_val) * dist_raw) >> 31);

        i32 raw = Perlin::pnoise2d_raw(nx, ny, fade_lut, perm);
        if (raw < 0) raw = 0;
        if (raw > FP_ONE) raw = FP_ONE;
        return raw * 255;
    };

    for (int i = 0; i < total_pixels; i++) {
        const i32 base_raw = setup.base_angle[i];
        const i32 dist_raw = setup.dist_scaled[i];

        i32 s0 = noise_channel(base_raw, rad0_raw, lin0_raw, dist_raw);
        i32 s1 = noise_channel(base_raw, rad1_raw, lin1_raw, dist_raw);
        i32 s2 = noise_channel(base_raw, rad2_raw, lin2_raw, dist_raw);

        i32 r = applyRadialFilter(s0, setup.rf3[i]);
        i32 g = applyRadialFilter(s1, setup.rf_half[i]);
        i32 b = applyRadialFilter(s2, setup.rf_quarter[i]);

        leds[setup.pixel_idx[i]] = CRGB(static_cast<u8>(r),
                                         static_cast<u8>(g),
                                         static_cast<u8>(b));
    }
}

// ============================================================================
// SIMD Implementation (vectorized 4-wide processing)
// ============================================================================

void Chasing_Spirals_Q31_SIMD(Context &ctx) {
    auto setup = setupChasingSpiralFrame(ctx);
    const int   total_pixels  = setup.total_pixels;
    const i32  *base_angle    = setup.base_angle;
    const i32  *dist_scaled   = setup.dist_scaled;
    const i32  *rf3_arr       = setup.rf3;
    const i32  *rf_half_arr   = setup.rf_half;
    const i32  *rf_qtr_arr    = setup.rf_quarter;
    const u16  *pixel_idx     = setup.pixel_idx;
    const i32  *fade_lut      = setup.fade_lut;
    const u8   *perm          = setup.perm;
    const i32   cx_raw        = setup.cx_raw;
    const i32   cy_raw        = setup.cy_raw;
    const i32   lin0_raw      = setup.lin0_raw;
    const i32   lin1_raw      = setup.lin1_raw;
    const i32   lin2_raw      = setup.lin2_raw;
    const i32   rad0_raw      = setup.rad0_raw;
    const i32   rad1_raw      = setup.rad1_raw;
    const i32   rad2_raw      = setup.rad2_raw;
    CRGB       *leds          = setup.leds;

    // SIMD Pixel Pipeline (4-wide batches)
    int i = 0;
    for (; i + 3 < total_pixels; i += 4) {
        // Direct SIMD load from SoA arrays (no AoS gather)
        simd::simd_u32x4 base_vec    = simd::load_u32_4(reinterpret_cast<const u32*>(base_angle  + i)); // ok reinterpret cast
        simd::simd_u32x4 dist_vec    = simd::load_u32_4(reinterpret_cast<const u32*>(dist_scaled + i)); // ok reinterpret cast
        simd::simd_u32x4 rf3_vec     = simd::load_u32_4(reinterpret_cast<const u32*>(rf3_arr     + i)); // ok reinterpret cast
        simd::simd_u32x4 rf_half_vec = simd::load_u32_4(reinterpret_cast<const u32*>(rf_half_arr + i)); // ok reinterpret cast
        simd::simd_u32x4 rf_qtr_vec  = simd::load_u32_4(reinterpret_cast<const u32*>(rf_qtr_arr  + i)); // ok reinterpret cast

        simd::simd_u32x4 r_vec = simd4_processChannel(
            base_vec, dist_vec, rad0_raw, lin0_raw, fade_lut, perm, cx_raw, cy_raw, rf3_vec);
        simd::simd_u32x4 g_vec = simd4_processChannel(
            base_vec, dist_vec, rad1_raw, lin1_raw, fade_lut, perm, cx_raw, cy_raw, rf_half_vec);
        simd::simd_u32x4 b_vec = simd4_processChannel(
            base_vec, dist_vec, rad2_raw, lin2_raw, fade_lut, perm, cx_raw, cy_raw, rf_qtr_vec);

        leds[pixel_idx[i+0]] = CRGB(
            static_cast<u8>(simd::extract_u32_4(r_vec, 0)),
            static_cast<u8>(simd::extract_u32_4(g_vec, 0)),
            static_cast<u8>(simd::extract_u32_4(b_vec, 0)));
        leds[pixel_idx[i+1]] = CRGB(
            static_cast<u8>(simd::extract_u32_4(r_vec, 1)),
            static_cast<u8>(simd::extract_u32_4(g_vec, 1)),
            static_cast<u8>(simd::extract_u32_4(b_vec, 1)));
        leds[pixel_idx[i+2]] = CRGB(
            static_cast<u8>(simd::extract_u32_4(r_vec, 2)),
            static_cast<u8>(simd::extract_u32_4(g_vec, 2)),
            static_cast<u8>(simd::extract_u32_4(b_vec, 2)));
        leds[pixel_idx[i+3]] = CRGB(
            static_cast<u8>(simd::extract_u32_4(r_vec, 3)),
            static_cast<u8>(simd::extract_u32_4(g_vec, 3)),
            static_cast<u8>(simd::extract_u32_4(b_vec, 3)));
    }

    // Scalar fallback for remaining pixels (when total_pixels % 4 != 0)
    for (; i < total_pixels; i++) {
        const i32 base_raw = base_angle[i];
        const i32 dist_raw = dist_scaled[i];

        auto noise_ch = [&](i32 rad_raw, i32 lin_raw) -> i32 {
            u32 a24 = radiansToA24(base_raw, rad_raw);
            SinCos32 sc = sincos32(a24);
            i32 nx = lin_raw + cx_raw -
                static_cast<i32>((static_cast<i64>(sc.cos_val) * dist_raw) >> 31);
            i32 ny = cy_raw -
                static_cast<i32>((static_cast<i64>(sc.sin_val) * dist_raw) >> 31);
            i32 raw = Perlin::pnoise2d_raw(nx, ny, fade_lut, perm);
            return clampAndScale255(raw);
        };

        i32 s0 = noise_ch(rad0_raw, lin0_raw);
        i32 s1 = noise_ch(rad1_raw, lin1_raw);
        i32 s2 = noise_ch(rad2_raw, lin2_raw);

        i32 r = applyRadialFilter(s0, rf3_arr[i]);
        i32 g = applyRadialFilter(s1, rf_half_arr[i]);
        i32 b = applyRadialFilter(s2, rf_qtr_arr[i]);

        leds[pixel_idx[i]] = CRGB(static_cast<u8>(r), static_cast<u8>(g), static_cast<u8>(b));
    }
}

} // namespace fl
