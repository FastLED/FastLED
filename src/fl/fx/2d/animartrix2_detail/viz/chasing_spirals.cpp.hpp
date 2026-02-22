// Chasing Spirals — three implementations (Float, Q31 scalar, Q31 SIMD).
//
// All variants share setupChasingSpiralFrame() which builds a per-pixel SoA
// geometry cache (base_angle, dist_scaled, radial filters, pixel_idx) and
// a Perlin fade LUT.  Per-frame constants (center, linear/radial offsets)
// are computed once and passed via FrameSetup.
//
// Q31 scalar:  Batches 3 channel sincos into one sincos32_simd call per pixel,
//              then evaluates Perlin noise and radial filter per channel.
// Q31 SIMD:    Processes 4 pixels at a time with full SIMD pipeline (aligned
//              SoA loads → sincos32_simd → Perlin → clamp/scale → scatter).
//              Perlin exits to scalar per-lane (SSE2 has no integer gather).

#include "fl/align.h"
#include "fl/compiler_control.h"
#include "fl/fx/2d/animartrix2_detail/engine.h"
#include "fl/fx/2d/animartrix2_detail/chasing_spiral_state.h"
#include "fl/fx/2d/animartrix2_detail/perlin_s16x16.h"
#include "fl/fx/2d/animartrix2_detail/perlin_s16x16_simd.h"
#include "fl/fx/2d/animartrix2_detail/perlin_float.h"
#include "fl/simd.h"
#include "fl/sin32.h"
#include "fl/fx/2d/animartrix2_detail/viz/chasing_spirals.h"

FL_OPTIMIZATION_LEVEL_O3_BEGIN

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
FASTLED_FORCE_INLINE u32 radiansToA24(i32 base_s16x16, i32 offset_s16x16) {
    constexpr i32 RAD_TO_A24 = 2670177;
    return static_cast<u32>((static_cast<i64>(base_s16x16 + offset_s16x16) * RAD_TO_A24) >> FP::FRAC_BITS);
}

// Compute Perlin coordinate from sincos result and distance
FASTLED_FORCE_INLINE i32 perlinCoord(i32 sc_val, i32 dist_raw, i32 offset) {
    return offset - static_cast<i32>((static_cast<i64>(sc_val) * dist_raw) >> 31);
}

// Clamp s16x16 value to [0, 1] and scale to [0, 255]
FASTLED_FORCE_INLINE i32 clampAndScale255(i32 raw_s16x16) {
    constexpr i32 FP_ONE = 1 << FP::FRAC_BITS;
    if (raw_s16x16 < 0) raw_s16x16 = 0;
    if (raw_s16x16 > FP_ONE) raw_s16x16 = FP_ONE;
    return (raw_s16x16 << 8) - raw_s16x16;
}

// Apply radial filter to noise value and clamp to [0, 255]
FASTLED_FORCE_INLINE i32 applyRadialFilter(i32 noise_255, i32 rf_raw) {
    i32 result = static_cast<i32>((static_cast<i64>(noise_255) * rf_raw) >> (FP::FRAC_BITS * 2));
    if (result < 0) result = 0;
    if (result > 255) result = 255;
    return result;
}

// Load 4 aligned i32 values from an SoA array into a SIMD register.
// All SoA arrays are FL_ALIGNAS(16) and loop stride is 4, so ptr+i is 16-byte aligned.
FASTLED_FORCE_INLINE simd::simd_u32x4 loadAligned(const i32 *arr, int i) {
    return simd::load_u32_4_aligned(
        FL_ASSUME_ALIGNED(reinterpret_cast<const u32*>(arr + i), 16)); // ok reinterpret cast
}

// Write one pixel from per-channel SIMD registers at the given lane.
FASTLED_FORCE_INLINE void scatterPixel(CRGB *leds, u16 idx,
    simd::simd_u32x4 r, simd::simd_u32x4 g, simd::simd_u32x4 b, int lane) {
    leds[idx] = CRGB(static_cast<u8>(simd::extract_u32_4(r, lane)),
                     static_cast<u8>(simd::extract_u32_4(g, lane)),
                     static_cast<u8>(simd::extract_u32_4(b, lane)));
}

// Process one color channel for 4 pixels using a full SIMD pipeline.
// Returns 4 clamped [0, 255] channel values.
simd::simd_u32x4 simd4_processChannel(
    simd::simd_u32x4 base_vec, simd::simd_u32x4 dist_vec,
    i32 radial_offset, i32 linear_offset,
    const i32 *fade_lut, const u8 *perm, i32 cx_raw, i32 cy_raw,
    simd::simd_u32x4 rf_vec) {

    constexpr i32 RAD_TO_A24 = 2670177;

    // Angle conversion: Q16.16 → A24
    auto offset_vec    = simd::set1_u32_4(static_cast<u32>(radial_offset));
    auto sum_vec       = simd::add_i32_4(base_vec, offset_vec);
    auto rad_const_vec = simd::set1_u32_4(static_cast<u32>(RAD_TO_A24));
    auto angles_vec    = simd::mulhi_su32_4(sum_vec, rad_const_vec);

    SinCos32_simd sc = sincos32_simd(angles_vec);

    // Perlin coordinates: nx = lin+cx - cos*dist, ny = cy - sin*dist
    auto lin_cx  = simd::set1_u32_4(static_cast<u32>(linear_offset + cx_raw));
    auto cy_vec  = simd::set1_u32_4(static_cast<u32>(cy_raw));
    auto nx_vec  = simd::sub_i32_4(lin_cx,
        simd::sll_u32_4(simd::mulhi32_i32_4(sc.cos_vals, dist_vec), 1));
    auto ny_vec  = simd::sub_i32_4(cy_vec,
        simd::sll_u32_4(simd::mulhi32_i32_4(sc.sin_vals, dist_vec), 1));

    // Perlin noise (SIMD floor/frac/wrap, scalar fade/perm/grad/lerp per lane)
    auto raw_vec = perlin_s16x16_simd::pnoise2d_raw_simd4_vec(
        nx_vec, ny_vec, fade_lut, perm);

    // Clamp [0, FP_ONE], scale ×255, apply radial filter, clamp [0, 255]
    auto zero   = simd::set1_u32_4(0u);
    auto fp_one = simd::set1_u32_4(static_cast<u32>(1 << FP::FRAC_BITS));
    auto clamped = simd::min_i32_4(simd::max_i32_4(raw_vec, zero), fp_one);
    auto noise_scaled = simd::sub_i32_4(simd::sll_u32_4(clamped, 8), clamped);

    auto max255 = simd::set1_u32_4(255u);
    auto result = simd::mulhi32_i32_4(noise_scaled, rf_vec);
    return simd::min_i32_4(simd::max_i32_4(result, zero), max255);
}

// Extract common frame setup logic shared by all variants.
// Builds SoA geometry cache lazily (once when grid size changes).
// state is the caller's per-instance ChasingSpiralState member (not a global).
FrameSetup setupChasingSpiralFrame(Context &ctx, ChasingSpiralState &state) {
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

    // Build per-pixel SoA geometry (once when grid size changes)
    if (state.count != total_pixels) {
        const int padded = (total_pixels + 3) & ~3;  // multiple of 4 for SIMD safety
        state.base_angle.resize(padded, 0);
        state.dist_scaled.resize(padded, 0);
        state.rf3.resize(padded, 0);
        state.rf_half.resize(padded, 0);
        state.rf_quarter.resize(padded, 0);
        state.pixel_idx.resize(padded, 0);

        const FP inv_radius = one / radius_fp;
        const FP one_third = one / three_fp;
        int idx = 0;
        for (int x = 0; x < num_x; x++) {
            for (int y = 0; y < num_y; y++) {
                const FP theta(e->polar_theta[x][y]);
                const FP dist(e->distance[x][y]);
                const FP rf = (radius_fp - dist) * inv_radius;
                state.base_angle[idx]  = (three_fp * theta - dist * one_third).raw();
                state.dist_scaled[idx] = (dist * scale).raw();
                state.rf3[idx]         = (three_fp * rf).raw();
                state.rf_half[idx]     = (rf >> 1).raw();
                state.rf_quarter[idx]  = (rf >> 2).raw();
                state.pixel_idx[idx]   = e->mCtx->xyMapFn(x, y, e->mCtx->xyMapUserData);
                idx++;
            }
        }
        state.count = total_pixels;
    }

    // Initialize Perlin fade LUT once per state lifetime
    if (!state.fade_lut_initialized) {
        Perlin::init_fade_lut(state.fade_lut);
        state.fade_lut_initialized = true;
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
        state.base_angle.data(),
        state.dist_scaled.data(),
        state.rf3.data(),
        state.rf_half.data(),
        state.rf_quarter.data(),
        state.pixel_idx.data(),
        state.fade_lut,
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

void Chasing_Spirals_Float::draw(Context &ctx) {
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

void Chasing_Spirals_Q31::draw(Context &ctx) {
    auto setup = setupChasingSpiralFrame(ctx, mState);
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

    // Compute one noise channel from a batched SinCos32_simd result.
    auto noise_channel = [&](const SinCos32_simd &sc, int lane,
                             i32 lin_raw, i32 dist_raw) -> i32 {
        i32 cos_v = static_cast<i32>(simd::extract_u32_4(sc.cos_vals, lane));
        i32 sin_v = static_cast<i32>(simd::extract_u32_4(sc.sin_vals, lane));
        i32 nx = perlinCoord(cos_v, dist_raw, lin_raw + cx_raw);
        i32 ny = perlinCoord(sin_v, dist_raw, cy_raw);
        return clampAndScale255(Perlin::pnoise2d_raw(nx, ny, fade_lut, perm));
    };

    for (int i = 0; i < total_pixels; i++) {
        const i32 base_raw = setup.base_angle[i];
        const i32 dist_raw = setup.dist_scaled[i];

        // Batch all 3 channel sincos into one SIMD call (4th lane unused)
        simd::simd_u32x4 angles = simd::set_u32_4(
            radiansToA24(base_raw, rad0_raw),
            radiansToA24(base_raw, rad1_raw),
            radiansToA24(base_raw, rad2_raw), 0);
        SinCos32_simd sc = sincos32_simd(angles);

        i32 s0 = noise_channel(sc, 0, lin0_raw, dist_raw);
        i32 s1 = noise_channel(sc, 1, lin1_raw, dist_raw);
        i32 s2 = noise_channel(sc, 2, lin2_raw, dist_raw);

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

void Chasing_Spirals_Q31_SIMD::draw(Context &ctx) {
    auto setup = setupChasingSpiralFrame(ctx, mState);
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

    // SIMD pixel pipeline: process 4 pixels per iteration
    int i = 0;
    for (; i + 3 < total_pixels; i += 4) {
        // Aligned SoA loads (arrays are FL_ALIGNAS(16), stride is 4)
        auto base_vec    = loadAligned(base_angle,  i);
        auto dist_vec    = loadAligned(dist_scaled, i);
        auto rf3_vec     = loadAligned(rf3_arr,     i);
        auto rf_half_vec = loadAligned(rf_half_arr, i);
        auto rf_qtr_vec  = loadAligned(rf_qtr_arr,  i);

        auto r_vec = simd4_processChannel(
            base_vec, dist_vec, rad0_raw, lin0_raw, fade_lut, perm, cx_raw, cy_raw, rf3_vec);
        auto g_vec = simd4_processChannel(
            base_vec, dist_vec, rad1_raw, lin1_raw, fade_lut, perm, cx_raw, cy_raw, rf_half_vec);
        auto b_vec = simd4_processChannel(
            base_vec, dist_vec, rad2_raw, lin2_raw, fade_lut, perm, cx_raw, cy_raw, rf_qtr_vec);

        // Scatter to LED array (pixel_idx holds arbitrary xyMap-remapped indices)
        scatterPixel(leds, pixel_idx[i+0], r_vec, g_vec, b_vec, 0);
        scatterPixel(leds, pixel_idx[i+1], r_vec, g_vec, b_vec, 1);
        scatterPixel(leds, pixel_idx[i+2], r_vec, g_vec, b_vec, 2);
        scatterPixel(leds, pixel_idx[i+3], r_vec, g_vec, b_vec, 3);
    }

    // Scalar fallback for remaining pixels (when total_pixels % 4 != 0)
    for (; i < total_pixels; i++) {
        const i32 base_raw = base_angle[i];
        const i32 dist_raw = dist_scaled[i];

        auto noise_ch = [&](i32 rad_raw, i32 lin_raw) -> i32 {
            u32 a24 = radiansToA24(base_raw, rad_raw);
            SinCos32 sc = sincos32(a24);
            i32 nx = perlinCoord(sc.cos_val, dist_raw, lin_raw + cx_raw);
            i32 ny = perlinCoord(sc.sin_val, dist_raw, cy_raw);
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

FL_OPTIMIZATION_LEVEL_O3_END
