

#include "fl/stl/stdint.h"

#define FASTLED_INTERNAL
#include "fl/system/fastled.h"

#include "crgb.h"
#include "fl/gfx/blur.h"
#include "fl/gfx/colorutils_misc.h"
#include "fl/stl/compiler_control.h"
#include "fl/log/log.h"
#include "fl/math/xymap.h"
#include "fl/math/scale8.h"
#include "fl/stl/int.h"
#include "fl/stl/span.h"
#include "fl/gfx/crgb.h"
#include "fl/gfx/crgb16.h"
#include "fl/stl/singleton.h"
#include "fl/stl/vector.h"

// Platform-neutral SIMD for blur kernels (SSE2, NEON, Xtensa PIE, scalar).
#if !defined(FL_IS_AVR)
#include "fl/math/simd.h"
#endif

// Force O3 even in debug builds so blur benchmarks don't hit watchdog timeouts.
FL_OPTIMIZATION_LEVEL_O3_BEGIN

// Legacy XY function. This is a weak symbol that can be overridden by the user.
// IMPORTANT: This MUST be in the global namespace (not fl::) for backward compatibility
// with user code from FastLED 3.7.6 that defines: uint16_t XY(uint8_t x, uint8_t y)
fl::u16 XY(fl::u8 x, fl::u8 y) FL_LINK_WEAK;

FL_LINK_WEAK fl::u16 XY(fl::u8 x, fl::u8 y) {
    FASTLED_UNUSED(x);
    FASTLED_UNUSED(y);
    FL_ERROR("XY function not provided - using default [0][0]. Use blur2d with XYMap instead");
    return 0;
}

namespace fl {

// make this a weak symbol
namespace {
fl::u16 xy_legacy_wrapper(fl::u16 x, fl::u16 y, fl::u16 width,
                           fl::u16 height) {
    FASTLED_UNUSED(width);
    FASTLED_UNUSED(height);
    return ::XY(x, y);  // Call global namespace XY
}
} // namespace

namespace gfx {

// blur1d: one-dimensional blur filter. Spreads light to 2 line neighbors.
// blur2d: two-dimensional blur filter. Spreads light to 8 XY neighbors.
//
//           0 = no spread at all
//          64 = moderate spreading
//         172 = maximum smooth, even spreading
//
//         173..255 = wider spreading, but increasing flicker
//
//         Total light is NOT entirely conserved, so many repeated
//         calls to 'blur' will also result in the light fading,
//         eventually all the way to black; this is by design so that
//         it can be used to (slowly) clear the LEDs to black.
void blur1d(fl::span<CRGB> leds, fract8 blur_amount) {
    const fl::u16 numLeds = static_cast<fl::u16>(leds.size());
    fl::u8 keep = 255 - blur_amount;
    fl::u8 seep = blur_amount >> 1;
    CRGB carryover = CRGB::Black;
    for (fl::u16 i = 0; i < numLeds; ++i) {
        CRGB cur = leds[i];
        CRGB part = cur;
        part.nscale8(seep);
        cur.nscale8(keep);
        cur += carryover;
        if (i)
            leds[i - 1] += part;
        leds[i] = cur;
        carryover = part;
    }
}

void blur2d(fl::span<CRGB> leds, fl::u8 width, fl::u8 height,
            fract8 blur_amount, const XYMap &xymap) {
    gfx::blurRows(leds, width, height, blur_amount, xymap);
    gfx::blurColumns(leds, width, height, blur_amount, xymap);
}

void blur2d(CRGB *leds, fl::u8 width, fl::u8 height, fract8 blur_amount) {
    // Legacy path: uses global XY() via XYMap for user-defined layouts.
    // Keeps its own blur algorithm copy because XYMap indexing differs from
    // the cache-coherent rectangular layout used by Canvas.
    XYMap xyMap =
        XYMap::constructWithUserFunction(width, height, xy_legacy_wrapper);
    fl::u8 keep = 255 - blur_amount;
    fl::u8 seep = blur_amount >> 1;
    // blur rows
    for (fl::u8 row = 0; row < height; ++row) {
        CRGB carryover = CRGB::Black;
        for (fl::u8 i = 0; i < width; ++i) {
            CRGB cur = leds[xyMap.mapToIndex(i, row)];
            CRGB part = cur;
            part.nscale8(seep);
            cur.nscale8(keep);
            cur += carryover;
            if (i)
                leds[xyMap.mapToIndex(i - 1, row)] += part;
            leds[xyMap.mapToIndex(i, row)] = cur;
            carryover = part;
        }
    }
    // blur columns
    for (fl::u8 col = 0; col < width; ++col) {
        CRGB carryover = CRGB::Black;
        for (fl::u8 i = 0; i < height; ++i) {
            CRGB cur = leds[xyMap.mapToIndex(col, i)];
            CRGB part = cur;
            part.nscale8(seep);
            cur.nscale8(keep);
            cur += carryover;
            if (i)
                leds[xyMap.mapToIndex(col, i - 1)] += part;
            leds[xyMap.mapToIndex(col, i)] = cur;
            carryover = part;
        }
    }
}

void blurRows(fl::span<CRGB> leds, fl::u8 width, fl::u8 height,
              fract8 blur_amount, const XYMap &xyMap) {
    CRGB *pixels = leds.data();
    fl::u8 keep = 255 - blur_amount;
    fl::u8 seep = blur_amount >> 1;
    if (xyMap.isRectangularGrid()) {
        for (fl::u8 row = 0; row < height; ++row) {
            CRGB carryover = CRGB::Black;
            CRGB *rowBase = pixels + row * width;
            for (fl::u8 col = 0; col < width; ++col) {
                CRGB cur = rowBase[col];
                CRGB part = cur;
                part.nscale8(seep);
                cur.nscale8(keep);
                cur += carryover;
                if (col)
                    rowBase[col - 1] += part;
                rowBase[col] = cur;
                carryover = part;
            }
        }
        return;
    }
    for (fl::u8 row = 0; row < height; ++row) {
        CRGB carryover = CRGB::Black;
        for (fl::u8 i = 0; i < width; ++i) {
            CRGB cur = leds[xyMap.mapToIndex(i, row)];
            CRGB part = cur;
            part.nscale8(seep);
            cur.nscale8(keep);
            cur += carryover;
            if (i)
                leds[xyMap.mapToIndex(i - 1, row)] += part;
            leds[xyMap.mapToIndex(i, row)] = cur;
            carryover = part;
        }
    }
}

void blurColumns(fl::span<CRGB> leds, fl::u8 width, fl::u8 height,
                 fract8 blur_amount, const XYMap &xyMap) {
    CRGB *pixels = leds.data();
    fl::u8 keep = 255 - blur_amount;
    fl::u8 seep = blur_amount >> 1;
    if (xyMap.isRectangularGrid()) {
        for (fl::u8 col = 0; col < width; ++col) {
            CRGB carryover = CRGB::Black;
            for (fl::u8 row = 0; row < height; ++row) {
                CRGB cur = pixels[row * width + col];
                CRGB part = cur;
                part.nscale8(seep);
                cur.nscale8(keep);
                cur += carryover;
                if (row)
                    pixels[(row - 1) * width + col] += part;
                pixels[row * width + col] = cur;
                carryover = part;
            }
        }
        return;
    }
    for (fl::u8 col = 0; col < width; ++col) {
        CRGB carryover = CRGB::Black;
        for (fl::u8 i = 0; i < height; ++i) {
            CRGB cur = leds[xyMap.mapToIndex(col, i)];
            CRGB part = cur;
            part.nscale8(seep);
            cur.nscale8(keep);
            cur += carryover;
            if (i)
                leds[xyMap.mapToIndex(col, i - 1)] += part;
            leds[xyMap.mapToIndex(col, i)] = cur;
            carryover = part;
        }
    }
}

void blurRows(Canvas<CRGB> &canvas, alpha8 blur_amount) {
    const int w = canvas.width;
    const int h = canvas.height;
    CRGB *pixels = canvas.pixels;
    fl::u8 keep = 255 - blur_amount;
    fl::u8 seep = blur_amount >> 1;
    for (int row = 0; row < h; ++row) {
        CRGB carryover = CRGB::Black;
        CRGB *rowBase = pixels + row * w;
        for (int col = 0; col < w; ++col) {
            CRGB cur = rowBase[col];
            CRGB part = cur;
            part.nscale8(seep);
            cur.nscale8(keep);
            cur += carryover;
            if (col)
                rowBase[col - 1] += part;
            rowBase[col] = cur;
            carryover = part;
        }
    }
}

void blurColumns(Canvas<CRGB> &canvas, alpha8 blur_amount) {
    const int w = canvas.width;
    const int h = canvas.height;
    CRGB *pixels = canvas.pixels;
    fl::u8 keep = 255 - blur_amount;
    fl::u8 seep = blur_amount >> 1;
    for (int col = 0; col < w; ++col) {
        CRGB carryover = CRGB::Black;
        for (int row = 0; row < h; ++row) {
            CRGB cur = pixels[row * w + col];
            CRGB part = cur;
            part.nscale8(seep);
            cur.nscale8(keep);
            cur += carryover;
            if (row)
                pixels[(row - 1) * w + col] += part;
            pixels[row * w + col] = cur;
            carryover = part;
        }
    }
}

void blur2d(Canvas<CRGB> &canvas, alpha8 blur_amount) {
    gfx::blurRows(canvas, blur_amount);
    gfx::blurColumns(canvas, blur_amount);
}

} // namespace gfx
} // namespace fl

// ── Separable Gaussian blur — two-pass binomial convolution ──────────
//
// Based on sutaburosu's SKIPSM Gaussian blur implementation.
// Uses binomial coefficient weights (Pascal's triangle) for a fast and
// flexible Gaussian blur approximation, optimized for kernel sizes up to 9×9.
// https://people.videolan.org/~tmatth/papers/Gaussian%20blur%20using%20finite-state%20machines.pdf
//
// Two separable passes (horizontal then vertical), each applying the 1D
// binomial kernel for the given radius:
//   radius 0: [1]                                    sum = 1    shift = 0
//   radius 1: [1, 2, 1]                              sum = 4    shift = 2
//   radius 2: [1, 4, 6, 4, 1]                        sum = 16   shift = 4
//   radius 3: [1, 6, 15, 20, 15, 6, 1]               sum = 64   shift = 6
//   radius 4: [1, 8, 28, 56, 70, 56, 28, 8, 1]       sum = 256  shift = 8
//
// Out-of-bounds pixels are treated as zero (zero-padding).
// Normalization by right-shift of 2*radius bits per pass.

namespace fl {
namespace gfx {


namespace blur_detail {

// Identity alpha value for each type (no dimming).
template <typename AlphaT> constexpr AlphaT alpha_identity();
template <> constexpr alpha8 alpha_identity<alpha8>() { return alpha8(255); }
template <> constexpr alpha16 alpha_identity<alpha16>() { return alpha16(65535); }

// Channel extraction and alpha-scaled pixel construction.
template <typename RGB_T>
struct pixel_ops;

template <>
struct pixel_ops<CRGB> {
    FL_ALWAYS_INLINE u16 ch(u8 v) { return v; }
    FL_ALWAYS_INLINE CRGB zero() { return CRGB(0, 0, 0); }

    FL_ALWAYS_INLINE CRGB make(u16 r, u16 g, u16 b) {
        return CRGB(static_cast<u8>(r), static_cast<u8>(g),
                    static_cast<u8>(b));
    }

    FL_ALWAYS_INLINE CRGB make(u16 r, u16 g, u16 b, alpha8 a) {
        if (a.value == 255) return make(r, g, b);
        u16 a1 = static_cast<u16>(a.value) + 1;
        return CRGB(static_cast<u8>((r * a1) >> 8),
                    static_cast<u8>((g * a1) >> 8),
                    static_cast<u8>((b * a1) >> 8));
    }

    FL_ALWAYS_INLINE CRGB make(u16 r, u16 g, u16 b, alpha16 a) {
        if (a.value >= 65535) return make(r, g, b);
        u32 a1 = static_cast<u32>(a.value) + 1;
        return CRGB(static_cast<u8>((r * a1) >> 16),
                    static_cast<u8>((g * a1) >> 16),
                    static_cast<u8>((b * a1) >> 16));
    }
};

template <>
struct pixel_ops<CRGB16> {
    FL_ALWAYS_INLINE u32 ch(u8x8 v) { return v.raw(); }
    FL_ALWAYS_INLINE CRGB16 zero() { return CRGB16(u8x8(0), u8x8(0), u8x8(0)); }

    FL_ALWAYS_INLINE CRGB16 make(u32 r, u32 g, u32 b) {
        return CRGB16(u8x8::from_raw(static_cast<u16>(r)),
                      u8x8::from_raw(static_cast<u16>(g)),
                      u8x8::from_raw(static_cast<u16>(b)));
    }

    FL_ALWAYS_INLINE CRGB16 make(u32 r, u32 g, u32 b, alpha8 a) {
        if (a.value == 255) return make(r, g, b);
        u32 a1 = static_cast<u32>(a.value) + 1;
        return make((r * a1) >> 8, (g * a1) >> 8, (b * a1) >> 8);
    }

    FL_ALWAYS_INLINE CRGB16 make(u32 r, u32 g, u32 b, alpha16 a) {
        if (a.value >= 65535) return make(r, g, b);
        u32 a1 = static_cast<u32>(a.value) + 1;
        return make((r * a1) >> 16, (g * a1) >> 16, (b * a1) >> 16);
    }
};

// Thread-local padded pixel buffer for zero-padding approach.
template <typename RGB_T>
static fl::span<RGB_T> get_padbuf(int minSize) {
    fl::vector<RGB_T> &buf = SingletonThreadLocal<fl::vector<RGB_T>>::instance();
    if (static_cast<int>(buf.size()) < minSize) {
        buf.resize(minSize);
    }
    return buf;
}

// Interior row pixel — fully-unrolled, no bounds checks.
// Also reused for vertical pass via linearized column data.
// Template-specialized per radius for direct hardcoded weights.
template <int R, typename RGB_T, typename acc_t>
struct interior_row;

template <typename RGB_T, typename acc_t>
struct interior_row<0, RGB_T, acc_t> {
    FL_ALWAYS_INLINE void apply(const RGB_T *row, int x,
                             acc_t &r, acc_t &g, acc_t &b) {
        using P = pixel_ops<RGB_T>;
        r = P::ch(row[x].r); g = P::ch(row[x].g); b = P::ch(row[x].b);
    }
};

template <typename RGB_T, typename acc_t>
struct interior_row<1, RGB_T, acc_t> {
    FL_ALWAYS_INLINE void apply(const RGB_T *row, int x,
                             acc_t &r, acc_t &g, acc_t &b) {
        using P = pixel_ops<RGB_T>;
        P p;
        r = 0; g = 0; b = 0;
        { const RGB_T &px = row[x-1]; r += p.ch(px.r);     g += p.ch(px.g);     b += p.ch(px.b); }
        { const RGB_T &px = row[x];   r += p.ch(px.r) * 2; g += p.ch(px.g) * 2; b += p.ch(px.b) * 2; }
        { const RGB_T &px = row[x+1]; r += p.ch(px.r);     g += p.ch(px.g);     b += p.ch(px.b); }
    }
};

template <typename RGB_T, typename acc_t>
struct interior_row<2, RGB_T, acc_t> {
    FL_ALWAYS_INLINE void apply(const RGB_T *row, int x,
                             acc_t &r, acc_t &g, acc_t &b) {
        using P = pixel_ops<RGB_T>;
        P p;
        // [1, 4, 6, 4, 1] — symmetric: (e0+e4) + 4*(e1+e3) + 6*e2
        const RGB_T &pm2 = row[x-2], &pm1 = row[x-1], &pc = row[x], &pp1 = row[x+1], &pp2 = row[x+2];
        const acc_t s04r = p.ch(pm2.r) + p.ch(pp2.r), s13r = p.ch(pm1.r) + p.ch(pp1.r);
        const acc_t s04g = p.ch(pm2.g) + p.ch(pp2.g), s13g = p.ch(pm1.g) + p.ch(pp1.g);
        const acc_t s04b = p.ch(pm2.b) + p.ch(pp2.b), s13b = p.ch(pm1.b) + p.ch(pp1.b);
        r = s04r + s13r * 4 + p.ch(pc.r) * 6;
        g = s04g + s13g * 4 + p.ch(pc.g) * 6;
        b = s04b + s13b * 4 + p.ch(pc.b) * 6;
    }
};

template <typename RGB_T, typename acc_t>
struct interior_row<3, RGB_T, acc_t> {
    FL_ALWAYS_INLINE void apply(const RGB_T *row, int x,
                             acc_t &r, acc_t &g, acc_t &b) {
        using P = pixel_ops<RGB_T>;
        P p;
        // [1, 6, 15, 20, 15, 6, 1] — symmetric: (e0+e6) + 6*(e1+e5) + 15*(e2+e4) + 20*e3
        const RGB_T &pm3 = row[x-3], &pm2 = row[x-2], &pm1 = row[x-1], &pc = row[x];
        const RGB_T &pp1 = row[x+1], &pp2 = row[x+2], &pp3 = row[x+3];
        const acc_t s06r = p.ch(pm3.r) + p.ch(pp3.r), s15r = p.ch(pm2.r) + p.ch(pp2.r), s24r = p.ch(pm1.r) + p.ch(pp1.r);
        const acc_t s06g = p.ch(pm3.g) + p.ch(pp3.g), s15g = p.ch(pm2.g) + p.ch(pp2.g), s24g = p.ch(pm1.g) + p.ch(pp1.g);
        const acc_t s06b = p.ch(pm3.b) + p.ch(pp3.b), s15b = p.ch(pm2.b) + p.ch(pp2.b), s24b = p.ch(pm1.b) + p.ch(pp1.b);
        r = s06r + s15r * 6 + s24r * 15 + p.ch(pc.r) * 20;
        g = s06g + s15g * 6 + s24g * 15 + p.ch(pc.g) * 20;
        b = s06b + s15b * 6 + s24b * 15 + p.ch(pc.b) * 20;
    }
};

template <typename RGB_T, typename acc_t>
struct interior_row<4, RGB_T, acc_t> {
    FL_ALWAYS_INLINE void apply(const RGB_T *row, int x,
                             acc_t &r, acc_t &g, acc_t &b) {
        using P = pixel_ops<RGB_T>;
        P p;
        // [1, 8, 28, 56, 70, 56, 28, 8, 1] — symmetric: (e0+e8) + 8*(e1+e7) + 28*(e2+e6) + 56*(e3+e5) + 70*e4
        const RGB_T &pm4 = row[x-4], &pm3 = row[x-3], &pm2 = row[x-2], &pm1 = row[x-1], &pc = row[x];
        const RGB_T &pp1 = row[x+1], &pp2 = row[x+2], &pp3 = row[x+3], &pp4 = row[x+4];
        const acc_t s08r = p.ch(pm4.r) + p.ch(pp4.r), s17r = p.ch(pm3.r) + p.ch(pp3.r);
        const acc_t s26r = p.ch(pm2.r) + p.ch(pp2.r), s35r = p.ch(pm1.r) + p.ch(pp1.r);
        const acc_t s08g = p.ch(pm4.g) + p.ch(pp4.g), s17g = p.ch(pm3.g) + p.ch(pp3.g);
        const acc_t s26g = p.ch(pm2.g) + p.ch(pp2.g), s35g = p.ch(pm1.g) + p.ch(pp1.g);
        const acc_t s08b = p.ch(pm4.b) + p.ch(pp4.b), s17b = p.ch(pm3.b) + p.ch(pp3.b);
        const acc_t s26b = p.ch(pm2.b) + p.ch(pp2.b), s35b = p.ch(pm1.b) + p.ch(pp1.b);
        r = s08r + s17r * 8 + s26r * 28 + s35r * 56 + p.ch(pc.r) * 70;
        g = s08g + s17g * 8 + s26g * 28 + s35g * 56 + p.ch(pc.g) * 70;
        b = s08b + s17b * 8 + s26b * 28 + s35b * 56 + p.ch(pc.b) * 70;
    }
};

// ── AVR per-channel convolution kernels ──────────────────────────────
// On AVR, processing one color channel at a time cuts live accumulator
// registers from 6 (r,g,b as u16 pairs) to 2, dramatically reducing
// register spilling in the 32-register AVR architecture.
//
// conv1ch<R>::apply(center): computes the 1D binomial kernel for a
// single u8 channel, where `center` points to the center pixel's
// channel byte and the pixel stride is sizeof(CRGB) = 3 (compile-time
// constant).
#if defined(FL_IS_AVR)

template <int R> struct conv1ch;

// All conv1ch specializations use positive-only offsets from the window
// start (p = center - R*S). This maps cleanly to AVR's LDD instruction
// which supports displacement 0-63.  Max offset = 2*R*S = 24 for R4.
template <> struct conv1ch<0> {
    static inline u16 __attribute__((always_inline)) apply(const u8 *c) {
        return (u16)c[0];
    }
};

template <> struct conv1ch<1> {
    static inline u16 __attribute__((always_inline)) apply(const u8 *c) {
        constexpr int S = sizeof(CRGB); // 3
        const u8 *p = c - S;
        // [64, 128, 64] (rescaled from [1,2,1] so sum=256, shift-by-8 is free)
        return (u16)p[0] * 64 + (u16)p[S] * 128 + (u16)p[2*S] * 64;
    }
};

template <> struct conv1ch<2> {
    static inline u16 __attribute__((always_inline)) apply(const u8 *c) {
        constexpr int S = sizeof(CRGB);
        const u8 *p = c - 2*S;
        // [16, 64, 96, 64, 16] (rescaled from [1,4,6,4,1] so sum=256, shift-by-8 is free)
        return (u16)p[0] * 16 + (u16)p[S] * 64 + (u16)p[2*S] * 96
             + (u16)p[3*S] * 64 + (u16)p[4*S] * 16;
    }
};

template <> struct conv1ch<3> {
    static inline u16 __attribute__((always_inline)) apply(const u8 *c) {
        constexpr int S = sizeof(CRGB);
        const u8 *p = c - 3*S;
        // [4, 24, 60, 80, 60, 24, 4] (rescaled from [1,6,15,20,15,6,1] so sum=256, shift-by-8 is free)
        return (u16)p[0] * 4 + (u16)p[S] * 24  + (u16)p[2*S] * 60
             + (u16)p[3*S] * 80 + (u16)p[4*S] * 60 + (u16)p[5*S] * 24
             + (u16)p[6*S] * 4;
    }
};

template <> struct conv1ch<4> {
    static inline u16 __attribute__((always_inline)) apply(const u8 *c) {
        constexpr int S = sizeof(CRGB);
        const u8 *p = c - 4*S;
        // [1, 8, 28, 56, 70, 56, 28, 8, 1]
        return (u16)p[0] + (u16)p[S] * 8  + (u16)p[2*S] * 28
             + (u16)p[3*S] * 56 + (u16)p[4*S] * 70 + (u16)p[5*S] * 56
             + (u16)p[6*S] * 28 + (u16)p[7*S] * 8 + (u16)p[8*S];
    }
};

// AVR noinline per-channel pass for CRGB (no alpha).
template <int R>
FL_NO_INLINE FL_OPTIMIZE_FUNCTION
static void apply_pass_1ch(const CRGB *pad, CRGB *out, int count, int stride) {
    constexpr int shift = (R == 0) ? 0 : 8;
    for (int i = 0; i < count; ++i) {
        const u8 *base = pad[R + i].raw;
        out->r = static_cast<u8>(conv1ch<R>::apply(base + 0) >> shift);
        out->g = static_cast<u8>(conv1ch<R>::apply(base + 1) >> shift);
        out->b = static_cast<u8>(conv1ch<R>::apply(base + 2) >> shift);
        out += stride;
    }
}

// AVR noinline per-channel pass for CRGB with alpha dim.
template <int R>
FL_NO_INLINE FL_OPTIMIZE_FUNCTION
static void apply_pass_alpha_1ch(const CRGB *pad, CRGB *out, int count,
                                  int stride, alpha8 alpha) {
    constexpr int shift = (R == 0) ? 0 : 8;
    u16 a1 = static_cast<u16>(alpha.value) + 1;
    for (int i = 0; i < count; ++i) {
        const u8 *base = pad[R + i].raw;
        u16 r = conv1ch<R>::apply(base + 0) >> shift;
        u16 g = conv1ch<R>::apply(base + 1) >> shift;
        u16 b = conv1ch<R>::apply(base + 2) >> shift;
        out->r = static_cast<u8>((r * a1) >> 8);
        out->g = static_cast<u8>((g * a1) >> 8);
        out->b = static_cast<u8>((b * a1) >> 8);
        out += stride;
    }
}

// AVR noinline per-channel pass for CRGB with alpha16 dim.
template <int R>
FL_NO_INLINE FL_OPTIMIZE_FUNCTION
static void apply_pass_alpha_1ch(const CRGB *pad, CRGB *out, int count,
                                  int stride, alpha16 alpha) {
    constexpr int shift = (R == 0) ? 0 : 8;
    u32 a1 = static_cast<u32>(alpha.value) + 1;
    for (int i = 0; i < count; ++i) {
        const u8 *base = pad[R + i].raw;
        u16 r = conv1ch<R>::apply(base + 0) >> shift;
        u16 g = conv1ch<R>::apply(base + 1) >> shift;
        u16 b = conv1ch<R>::apply(base + 2) >> shift;
        out->r = static_cast<u8>((r * a1) >> 16);
        out->g = static_cast<u8>((g * a1) >> 16);
        out->b = static_cast<u8>((b * a1) >> 16);
        out += stride;
    }
}

#endif // FL_IS_AVR

// Row-level kernel application — noinline on AVR to isolate register pressure.
// On non-AVR platforms (or CRGB16 on AVR), processes all 3 channels
// simultaneously using the interior_row kernel.
template <int R, typename RGB_T, typename acc_t>
FL_NO_INLINE_IF_AVR FL_OPTIMIZE_FUNCTION
static void apply_pass(const RGB_T *pad, RGB_T *out, int count, int stride) {
    constexpr int shift = 2 * R;
    using P = pixel_ops<RGB_T>;
    for (int i = 0; i < count; ++i) {
        acc_t r, g, b;
        interior_row<R, RGB_T, acc_t>::apply(pad, R + i, r, g, b);
        *out = P::make(static_cast<acc_t>(r >> shift),
                       static_cast<acc_t>(g >> shift),
                       static_cast<acc_t>(b >> shift));
        out += stride;
    }
}

template <int R, typename RGB_T, typename acc_t, typename AlphaT>
FL_NO_INLINE_IF_AVR FL_OPTIMIZE_FUNCTION
static void apply_pass_alpha(const RGB_T *pad, RGB_T *out, int count,
                             int stride, AlphaT alpha) {
    constexpr int shift = 2 * R;
    using P = pixel_ops<RGB_T>;
    for (int i = 0; i < count; ++i) {
        acc_t r, g, b;
        interior_row<R, RGB_T, acc_t>::apply(pad, R + i, r, g, b);
        *out = P::make(static_cast<acc_t>(r >> shift),
                       static_cast<acc_t>(g >> shift),
                       static_cast<acc_t>(b >> shift), alpha);
        out += stride;
    }
}


// ── Platform-neutral SIMD byte-level convolution kernels ────────────────
// Process nbytes of output using stride-S byte-level convolution.
// For horizontal pass: S = sizeof(CRGB) = 3 (neighboring pixels).
// For vertical pass: S = 1 (same column, consecutive row buffers).
// The same kernel weights apply to all bytes (R, G, B treated uniformly).
// Uses fl::simd u8x16/u16x8 operations — compiles to SSE2, NEON, PIE, or scalar.
#if !defined(FL_IS_AVR)

// Kernel: [1, 2, 1] >> 2  ≈  avg(avg(a, c), b)
// Uses hardware byte averaging which computes (x+y+1)>>1.
// Two nested avg ops approximate (a + 2b + c) >> 2 with at most +1 rounding
// per channel — imperceptible for blur and ~3x fewer SIMD instructions.
static void simd_conv_121(const u8 * FL_RESTRICT_PARAM a,
                           const u8 * FL_RESTRICT_PARAM b,
                           const u8 * FL_RESTRICT_PARAM c,
                           u8 * FL_RESTRICT_PARAM out, int nbytes) {
    namespace fsimd = fl::simd; // ok bare using
    int i = 0;
    for (; i + 63 < nbytes; i += 64) {
        auto va0 = fsimd::load_u8_32(a+i), vb0 = fsimd::load_u8_32(b+i), vc0 = fsimd::load_u8_32(c+i);
        auto va1 = fsimd::load_u8_32(a+i+32), vb1 = fsimd::load_u8_32(b+i+32), vc1 = fsimd::load_u8_32(c+i+32);
        fsimd::store_u8_32(out+i, fsimd::avg_round_u8_32(fsimd::avg_round_u8_32(va0, vc0), vb0));
        fsimd::store_u8_32(out+i+32, fsimd::avg_round_u8_32(fsimd::avg_round_u8_32(va1, vc1), vb1));
    }
    for (; i + 31 < nbytes; i += 32) {
        auto va = fsimd::load_u8_32(a+i), vb = fsimd::load_u8_32(b+i), vc = fsimd::load_u8_32(c+i);
        fsimd::store_u8_32(out+i, fsimd::avg_round_u8_32(fsimd::avg_round_u8_32(va, vc), vb));
    }
    for (; i + 15 < nbytes; i += 16) {
        auto va = fsimd::load_u8_16(a+i), vb = fsimd::load_u8_16(b+i), vc = fsimd::load_u8_16(c+i);
        fsimd::store_u8_16(out+i, fsimd::avg_round_u8_16(fsimd::avg_round_u8_16(va, vc), vb));
    }
    for (; i < nbytes; ++i)
        out[i] = (u8)(((u16)a[i] + ((u16)b[i] << 1) + (u16)c[i]) >> 2);
}

// Helper: weighted sum for u16x8 — computes (s_sym + w*s_sym_pair + ... + wc*center) >> shift.
// Used by R=2, R=3, R=4 kernels for both low and high halves of the 16-byte register.

// Kernel: [1, 4, 6, 4, 1] >> 4
static void simd_conv_14641(const u8 *p0, const u8 *p1, const u8 *p2,
                            const u8 *p3, const u8 *p4,
                            u8 *out, int nbytes) {
    namespace fsimd = fl::simd; // ok bare using
    const auto w4w = fsimd::set1_u16_16(4), w6w = fsimd::set1_u16_16(6);
    const auto w4 = fsimd::set1_u16_8(4), w6 = fsimd::set1_u16_8(6);
    int i = 0;
    for (; i + 31 < nbytes; i += 32) {
        auto v0 = fsimd::load_u8_32(p0+i), v1 = fsimd::load_u8_32(p1+i);
        auto v2 = fsimd::load_u8_32(p2+i), v3 = fsimd::load_u8_32(p3+i), v4 = fsimd::load_u8_32(p4+i);
        auto s04 = fsimd::add_u16_16(fsimd::widen_lo_u8x32_to_u16(v0), fsimd::widen_lo_u8x32_to_u16(v4));
        auto s13 = fsimd::add_u16_16(fsimd::widen_lo_u8x32_to_u16(v1), fsimd::widen_lo_u8x32_to_u16(v3));
        auto lo = fsimd::add_u16_16(s04, fsimd::add_u16_16(fsimd::mullo_u16_16(s13, w4w), fsimd::mullo_u16_16(fsimd::widen_lo_u8x32_to_u16(v2), w6w)));
        lo = fsimd::srli_u16_16(lo, 4);
        auto s04h = fsimd::add_u16_16(fsimd::widen_hi_u8x32_to_u16(v0), fsimd::widen_hi_u8x32_to_u16(v4));
        auto s13h = fsimd::add_u16_16(fsimd::widen_hi_u8x32_to_u16(v1), fsimd::widen_hi_u8x32_to_u16(v3));
        auto hi = fsimd::add_u16_16(s04h, fsimd::add_u16_16(fsimd::mullo_u16_16(s13h, w4w), fsimd::mullo_u16_16(fsimd::widen_hi_u8x32_to_u16(v2), w6w)));
        hi = fsimd::srli_u16_16(hi, 4);
        fsimd::store_u8_32(out+i, fsimd::narrow_u16x16_to_u8(lo, hi));
    }
    for (; i + 15 < nbytes; i += 16) {
        auto v0 = fsimd::load_u8_16(p0+i), v1 = fsimd::load_u8_16(p1+i);
        auto v2 = fsimd::load_u8_16(p2+i), v3 = fsimd::load_u8_16(p3+i), v4 = fsimd::load_u8_16(p4+i);
        auto s04 = fsimd::add_u16_8(fsimd::widen_lo_u8_to_u16(v0), fsimd::widen_lo_u8_to_u16(v4));
        auto s13 = fsimd::add_u16_8(fsimd::widen_lo_u8_to_u16(v1), fsimd::widen_lo_u8_to_u16(v3));
        auto lo = fsimd::add_u16_8(s04, fsimd::add_u16_8(fsimd::mullo_u16_8(s13, w4), fsimd::mullo_u16_8(fsimd::widen_lo_u8_to_u16(v2), w6)));
        lo = fsimd::srli_u16_8(lo, 4);
        auto s04h = fsimd::add_u16_8(fsimd::widen_hi_u8_to_u16(v0), fsimd::widen_hi_u8_to_u16(v4));
        auto s13h = fsimd::add_u16_8(fsimd::widen_hi_u8_to_u16(v1), fsimd::widen_hi_u8_to_u16(v3));
        auto hi = fsimd::add_u16_8(s04h, fsimd::add_u16_8(fsimd::mullo_u16_8(s13h, w4), fsimd::mullo_u16_8(fsimd::widen_hi_u8_to_u16(v2), w6)));
        hi = fsimd::srli_u16_8(hi, 4);
        fsimd::store_u8_16(out+i, fsimd::narrow_u16_to_u8(lo, hi));
    }
    for (; i < nbytes; ++i) {
        u16 s04 = (u16)p0[i] + (u16)p4[i];
        u16 s13 = (u16)p1[i] + (u16)p3[i];
        out[i] = (u8)((s04 + s13 * 4 + (u16)p2[i] * 6) >> 4);
    }
}

// Kernel: [1, 6, 15, 20, 15, 6, 1] >> 6
static void simd_conv_r3(const u8 *p0, const u8 *p1, const u8 *p2,
                          const u8 *p3, const u8 *p4, const u8 *p5,
                          const u8 *p6, u8 *out, int nbytes) {
    namespace fsimd = fl::simd; // ok bare using
    const auto w6w = fsimd::set1_u16_16(6), w15w = fsimd::set1_u16_16(15), w20w = fsimd::set1_u16_16(20);
    const auto w6 = fsimd::set1_u16_8(6), w15 = fsimd::set1_u16_8(15), w20 = fsimd::set1_u16_8(20);
    int i = 0;
    for (; i + 31 < nbytes; i += 32) {
        auto v0 = fsimd::load_u8_32(p0+i), v1 = fsimd::load_u8_32(p1+i), v2 = fsimd::load_u8_32(p2+i);
        auto v3 = fsimd::load_u8_32(p3+i), v4 = fsimd::load_u8_32(p4+i), v5 = fsimd::load_u8_32(p5+i);
        auto v6v = fsimd::load_u8_32(p6+i);
        auto s06 = fsimd::add_u16_16(fsimd::widen_lo_u8x32_to_u16(v0), fsimd::widen_lo_u8x32_to_u16(v6v));
        auto s15 = fsimd::add_u16_16(fsimd::widen_lo_u8x32_to_u16(v1), fsimd::widen_lo_u8x32_to_u16(v5));
        auto s24 = fsimd::add_u16_16(fsimd::widen_lo_u8x32_to_u16(v2), fsimd::widen_lo_u8x32_to_u16(v4));
        auto lo = fsimd::add_u16_16(s06, fsimd::add_u16_16(fsimd::mullo_u16_16(s15, w6w),
                   fsimd::add_u16_16(fsimd::mullo_u16_16(s24, w15w), fsimd::mullo_u16_16(fsimd::widen_lo_u8x32_to_u16(v3), w20w))));
        lo = fsimd::srli_u16_16(lo, 6);
        auto s06h = fsimd::add_u16_16(fsimd::widen_hi_u8x32_to_u16(v0), fsimd::widen_hi_u8x32_to_u16(v6v));
        auto s15h = fsimd::add_u16_16(fsimd::widen_hi_u8x32_to_u16(v1), fsimd::widen_hi_u8x32_to_u16(v5));
        auto s24h = fsimd::add_u16_16(fsimd::widen_hi_u8x32_to_u16(v2), fsimd::widen_hi_u8x32_to_u16(v4));
        auto hi = fsimd::add_u16_16(s06h, fsimd::add_u16_16(fsimd::mullo_u16_16(s15h, w6w),
                   fsimd::add_u16_16(fsimd::mullo_u16_16(s24h, w15w), fsimd::mullo_u16_16(fsimd::widen_hi_u8x32_to_u16(v3), w20w))));
        hi = fsimd::srli_u16_16(hi, 6);
        fsimd::store_u8_32(out+i, fsimd::narrow_u16x16_to_u8(lo, hi));
    }
    for (; i + 15 < nbytes; i += 16) {
        auto v0 = fsimd::load_u8_16(p0+i), v1 = fsimd::load_u8_16(p1+i), v2 = fsimd::load_u8_16(p2+i);
        auto v3 = fsimd::load_u8_16(p3+i), v4 = fsimd::load_u8_16(p4+i), v5 = fsimd::load_u8_16(p5+i);
        auto v6v = fsimd::load_u8_16(p6+i);
        auto s06 = fsimd::add_u16_8(fsimd::widen_lo_u8_to_u16(v0), fsimd::widen_lo_u8_to_u16(v6v));
        auto s15 = fsimd::add_u16_8(fsimd::widen_lo_u8_to_u16(v1), fsimd::widen_lo_u8_to_u16(v5));
        auto s24 = fsimd::add_u16_8(fsimd::widen_lo_u8_to_u16(v2), fsimd::widen_lo_u8_to_u16(v4));
        auto lo = fsimd::add_u16_8(s06, fsimd::add_u16_8(fsimd::mullo_u16_8(s15, w6),
                   fsimd::add_u16_8(fsimd::mullo_u16_8(s24, w15), fsimd::mullo_u16_8(fsimd::widen_lo_u8_to_u16(v3), w20))));
        lo = fsimd::srli_u16_8(lo, 6);
        auto s06h = fsimd::add_u16_8(fsimd::widen_hi_u8_to_u16(v0), fsimd::widen_hi_u8_to_u16(v6v));
        auto s15h = fsimd::add_u16_8(fsimd::widen_hi_u8_to_u16(v1), fsimd::widen_hi_u8_to_u16(v5));
        auto s24h = fsimd::add_u16_8(fsimd::widen_hi_u8_to_u16(v2), fsimd::widen_hi_u8_to_u16(v4));
        auto hi = fsimd::add_u16_8(s06h, fsimd::add_u16_8(fsimd::mullo_u16_8(s15h, w6),
                   fsimd::add_u16_8(fsimd::mullo_u16_8(s24h, w15), fsimd::mullo_u16_8(fsimd::widen_hi_u8_to_u16(v3), w20))));
        hi = fsimd::srli_u16_8(hi, 6);
        fsimd::store_u8_16(out+i, fsimd::narrow_u16_to_u8(lo, hi));
    }
    for (; i < nbytes; ++i) {
        u16 s06=(u16)p0[i]+(u16)p6[i], s15=(u16)p1[i]+(u16)p5[i], s24=(u16)p2[i]+(u16)p4[i];
        out[i]=(u8)((s06+s15*6+s24*15+(u16)p3[i]*20)>>6);
    }
}

// Kernel: [1, 8, 28, 56, 70, 56, 28, 8, 1] >> 8
static void simd_conv_r4(const u8 *p0, const u8 *p1, const u8 *p2, const u8 *p3,
                          const u8 *p4, const u8 *p5, const u8 *p6, const u8 *p7,
                          const u8 *p8, u8 *out, int nbytes) {
    namespace fsimd = fl::simd; // ok bare using
    const auto w8w = fsimd::set1_u16_16(8), w28w = fsimd::set1_u16_16(28);
    const auto w56w = fsimd::set1_u16_16(56), w70w = fsimd::set1_u16_16(70);
    const auto w8 = fsimd::set1_u16_8(8), w28 = fsimd::set1_u16_8(28);
    const auto w56 = fsimd::set1_u16_8(56), w70 = fsimd::set1_u16_8(70);
    int i = 0;
    for (; i + 31 < nbytes; i += 32) {
        auto v0 = fsimd::load_u8_32(p0+i), v1 = fsimd::load_u8_32(p1+i), v2 = fsimd::load_u8_32(p2+i);
        auto v3 = fsimd::load_u8_32(p3+i), v4 = fsimd::load_u8_32(p4+i), v5 = fsimd::load_u8_32(p5+i);
        auto v6v = fsimd::load_u8_32(p6+i), v7 = fsimd::load_u8_32(p7+i), v8v = fsimd::load_u8_32(p8+i);
        auto s08 = fsimd::add_u16_16(fsimd::widen_lo_u8x32_to_u16(v0), fsimd::widen_lo_u8x32_to_u16(v8v));
        auto s17 = fsimd::add_u16_16(fsimd::widen_lo_u8x32_to_u16(v1), fsimd::widen_lo_u8x32_to_u16(v7));
        auto s26 = fsimd::add_u16_16(fsimd::widen_lo_u8x32_to_u16(v2), fsimd::widen_lo_u8x32_to_u16(v6v));
        auto s35 = fsimd::add_u16_16(fsimd::widen_lo_u8x32_to_u16(v3), fsimd::widen_lo_u8x32_to_u16(v5));
        auto lo = fsimd::add_u16_16(s08, fsimd::add_u16_16(fsimd::mullo_u16_16(s17, w8w),
                   fsimd::add_u16_16(fsimd::mullo_u16_16(s26, w28w),
                   fsimd::add_u16_16(fsimd::mullo_u16_16(s35, w56w), fsimd::mullo_u16_16(fsimd::widen_lo_u8x32_to_u16(v4), w70w)))));
        lo = fsimd::srli_u16_16(lo, 8);
        auto s08h = fsimd::add_u16_16(fsimd::widen_hi_u8x32_to_u16(v0), fsimd::widen_hi_u8x32_to_u16(v8v));
        auto s17h = fsimd::add_u16_16(fsimd::widen_hi_u8x32_to_u16(v1), fsimd::widen_hi_u8x32_to_u16(v7));
        auto s26h = fsimd::add_u16_16(fsimd::widen_hi_u8x32_to_u16(v2), fsimd::widen_hi_u8x32_to_u16(v6v));
        auto s35h = fsimd::add_u16_16(fsimd::widen_hi_u8x32_to_u16(v3), fsimd::widen_hi_u8x32_to_u16(v5));
        auto hi = fsimd::add_u16_16(s08h, fsimd::add_u16_16(fsimd::mullo_u16_16(s17h, w8w),
                   fsimd::add_u16_16(fsimd::mullo_u16_16(s26h, w28w),
                   fsimd::add_u16_16(fsimd::mullo_u16_16(s35h, w56w), fsimd::mullo_u16_16(fsimd::widen_hi_u8x32_to_u16(v4), w70w)))));
        hi = fsimd::srli_u16_16(hi, 8);
        fsimd::store_u8_32(out+i, fsimd::narrow_u16x16_to_u8(lo, hi));
    }
    for (; i + 15 < nbytes; i += 16) {
        auto v0 = fsimd::load_u8_16(p0+i), v1 = fsimd::load_u8_16(p1+i), v2 = fsimd::load_u8_16(p2+i);
        auto v3 = fsimd::load_u8_16(p3+i), v4 = fsimd::load_u8_16(p4+i), v5 = fsimd::load_u8_16(p5+i);
        auto v6v = fsimd::load_u8_16(p6+i), v7 = fsimd::load_u8_16(p7+i), v8v = fsimd::load_u8_16(p8+i);
        auto s08 = fsimd::add_u16_8(fsimd::widen_lo_u8_to_u16(v0), fsimd::widen_lo_u8_to_u16(v8v));
        auto s17 = fsimd::add_u16_8(fsimd::widen_lo_u8_to_u16(v1), fsimd::widen_lo_u8_to_u16(v7));
        auto s26 = fsimd::add_u16_8(fsimd::widen_lo_u8_to_u16(v2), fsimd::widen_lo_u8_to_u16(v6v));
        auto s35 = fsimd::add_u16_8(fsimd::widen_lo_u8_to_u16(v3), fsimd::widen_lo_u8_to_u16(v5));
        auto lo = fsimd::add_u16_8(s08, fsimd::add_u16_8(fsimd::mullo_u16_8(s17, w8),
                   fsimd::add_u16_8(fsimd::mullo_u16_8(s26, w28),
                   fsimd::add_u16_8(fsimd::mullo_u16_8(s35, w56), fsimd::mullo_u16_8(fsimd::widen_lo_u8_to_u16(v4), w70)))));
        lo = fsimd::srli_u16_8(lo, 8);
        auto s08h = fsimd::add_u16_8(fsimd::widen_hi_u8_to_u16(v0), fsimd::widen_hi_u8_to_u16(v8v));
        auto s17h = fsimd::add_u16_8(fsimd::widen_hi_u8_to_u16(v1), fsimd::widen_hi_u8_to_u16(v7));
        auto s26h = fsimd::add_u16_8(fsimd::widen_hi_u8_to_u16(v2), fsimd::widen_hi_u8_to_u16(v6v));
        auto s35h = fsimd::add_u16_8(fsimd::widen_hi_u8_to_u16(v3), fsimd::widen_hi_u8_to_u16(v5));
        auto hi = fsimd::add_u16_8(s08h, fsimd::add_u16_8(fsimd::mullo_u16_8(s17h, w8),
                   fsimd::add_u16_8(fsimd::mullo_u16_8(s26h, w28),
                   fsimd::add_u16_8(fsimd::mullo_u16_8(s35h, w56), fsimd::mullo_u16_8(fsimd::widen_hi_u8_to_u16(v4), w70)))));
        hi = fsimd::srli_u16_8(hi, 8);
        fsimd::store_u8_16(out+i, fsimd::narrow_u16_to_u8(lo, hi));
    }
    for (; i < nbytes; ++i) {
        u16 s08=(u16)p0[i]+(u16)p8[i], s17=(u16)p1[i]+(u16)p7[i];
        u16 s26=(u16)p2[i]+(u16)p6[i], s35=(u16)p3[i]+(u16)p5[i];
        out[i]=(u8)((s08+s17*8+s26*28+s35*56+(u16)p4[i]*70)>>8);
    }
}

// ── Template dispatch: SIMD vertical convolution by radius ─────────────
// Compile-time selection eliminates runtime if-else chain on R.
template <int R> struct simd_vconv_dispatch;

template <> struct simd_vconv_dispatch<0> {
    template <typename RGB_T>
    static void apply(RGB_T **bufs, const RGB_T **, u8 *out, int nbytes) {
        FL_BUILTIN_MEMCPY(out, (const u8*)bufs[0], nbytes);
    }
};

template <> struct simd_vconv_dispatch<1> {
    template <typename RGB_T>
    static void apply(RGB_T **bufs, const RGB_T **fwd, u8 *out, int nbytes) {
        simd_conv_121((const u8*)bufs[0], (const u8*)bufs[1],
                      (const u8*)fwd[0], out, nbytes);
    }
};

template <> struct simd_vconv_dispatch<2> {
    template <typename RGB_T>
    static void apply(RGB_T **bufs, const RGB_T **fwd, u8 *out, int nbytes) {
        simd_conv_14641((const u8*)bufs[0], (const u8*)bufs[1],
                        (const u8*)bufs[2], (const u8*)fwd[0],
                        (const u8*)fwd[1], out, nbytes);
    }
};

template <> struct simd_vconv_dispatch<3> {
    template <typename RGB_T>
    static void apply(RGB_T **bufs, const RGB_T **fwd, u8 *out, int nbytes) {
        simd_conv_r3((const u8*)bufs[0], (const u8*)bufs[1],
                     (const u8*)bufs[2], (const u8*)bufs[3],
                     (const u8*)fwd[0], (const u8*)fwd[1],
                     (const u8*)fwd[2], out, nbytes);
    }
};

template <> struct simd_vconv_dispatch<4> {
    template <typename RGB_T>
    static void apply(RGB_T **bufs, const RGB_T **fwd, u8 *out, int nbytes) {
        simd_conv_r4((const u8*)bufs[0], (const u8*)bufs[1],
                     (const u8*)bufs[2], (const u8*)bufs[3],
                     (const u8*)bufs[4], (const u8*)fwd[0],
                     (const u8*)fwd[1], (const u8*)fwd[2],
                     (const u8*)fwd[3], out, nbytes);
    }
};

// ── Template dispatch: SIMD horizontal convolution by radius ───────────
template <int R> struct simd_hconv_dispatch;

template <> struct simd_hconv_dispatch<0> {
    static void apply(const u8 *pb, int, u8 *ob, int nbytes, u8 *, int) {
        FL_BUILTIN_MEMCPY(ob, pb, nbytes);
    }
};

template <> struct simd_hconv_dispatch<1> {
    static void apply(const u8 *pb, int S, u8 *ob, int nbytes, u8 *, int) {
        simd_conv_121(pb, pb + S, pb + 2*S, ob, nbytes);
    }
};

template <> struct simd_hconv_dispatch<2> {
    static void apply(const u8 *pb, int S, u8 *ob, int nbytes, u8 *, int) {
        // Direct [1,4,6,4,1] kernel — exact u16 multiply+shift, no cascaded
        // avg_round rounding. Slightly more SIMD ops than cascaded R=1 but
        // produces bit-exact results matching the scalar interior_row path.
        simd_conv_14641(pb, pb+S, pb+2*S, pb+3*S, pb+4*S, ob, nbytes);
    }
};

template <> struct simd_hconv_dispatch<3> {
    static void apply(const u8 *pb, int S, u8 *ob, int nbytes, u8 *, int) {
        simd_conv_r3(pb, pb+S, pb+2*S, pb+3*S, pb+4*S, pb+5*S, pb+6*S, ob, nbytes);
    }
};

template <> struct simd_hconv_dispatch<4> {
    static void apply(const u8 *pb, int S, u8 *ob, int nbytes, u8 *, int) {
        simd_conv_r4(pb, pb+S, pb+2*S, pb+3*S, pb+4*S, pb+5*S, pb+6*S, pb+7*S, pb+8*S, ob, nbytes);
    }
};

// ── Template dispatch: per-pixel vertical convolution by radius ────────
// Used by the generic path (CRGB16 or alpha case).
template <int R, typename RGB_T, typename acc_t> struct vpass_pixel_kernel;

template <typename RGB_T, typename acc_t>
struct vpass_pixel_kernel<0, RGB_T, acc_t> {
    FL_ALWAYS_INLINE void apply(RGB_T **bufs, const RGB_T **, int x,
                                        acc_t &r, acc_t &g, acc_t &b) {
        pixel_ops<RGB_T> p;
        r = p.ch(bufs[0][x].r);
        g = p.ch(bufs[0][x].g);
        b = p.ch(bufs[0][x].b);
    }
};

template <typename RGB_T, typename acc_t>
struct vpass_pixel_kernel<1, RGB_T, acc_t> {
    FL_ALWAYS_INLINE void apply(RGB_T **bufs, const RGB_T **fwd, int x,
                                        acc_t &r, acc_t &g, acc_t &b) {
        pixel_ops<RGB_T> p;
        r = (p.ch(bufs[0][x].r) + p.ch(fwd[0][x].r)) + (p.ch(bufs[1][x].r) << 1);
        g = (p.ch(bufs[0][x].g) + p.ch(fwd[0][x].g)) + (p.ch(bufs[1][x].g) << 1);
        b = (p.ch(bufs[0][x].b) + p.ch(fwd[0][x].b)) + (p.ch(bufs[1][x].b) << 1);
    }
};

template <typename RGB_T, typename acc_t>
struct vpass_pixel_kernel<2, RGB_T, acc_t> {
    FL_ALWAYS_INLINE void apply(RGB_T **bufs, const RGB_T **fwd, int x,
                                        acc_t &r, acc_t &g, acc_t &b) {
        pixel_ops<RGB_T> p;
        const acc_t sr04 = p.ch(bufs[0][x].r) + p.ch(fwd[1][x].r);
        const acc_t sg04 = p.ch(bufs[0][x].g) + p.ch(fwd[1][x].g);
        const acc_t sb04 = p.ch(bufs[0][x].b) + p.ch(fwd[1][x].b);
        const acc_t sr13 = p.ch(bufs[1][x].r) + p.ch(fwd[0][x].r);
        const acc_t sg13 = p.ch(bufs[1][x].g) + p.ch(fwd[0][x].g);
        const acc_t sb13 = p.ch(bufs[1][x].b) + p.ch(fwd[0][x].b);
        r = sr04 + sr13 * 4 + p.ch(bufs[2][x].r) * 6;
        g = sg04 + sg13 * 4 + p.ch(bufs[2][x].g) * 6;
        b = sb04 + sb13 * 4 + p.ch(bufs[2][x].b) * 6;
    }
};

template <typename RGB_T, typename acc_t>
struct vpass_pixel_kernel<3, RGB_T, acc_t> {
    FL_ALWAYS_INLINE void apply(RGB_T **bufs, const RGB_T **fwd, int x,
                                        acc_t &r, acc_t &g, acc_t &b) {
        pixel_ops<RGB_T> p;
        const acc_t sr06 = p.ch(bufs[0][x].r) + p.ch(fwd[2][x].r);
        const acc_t sg06 = p.ch(bufs[0][x].g) + p.ch(fwd[2][x].g);
        const acc_t sb06 = p.ch(bufs[0][x].b) + p.ch(fwd[2][x].b);
        const acc_t sr15 = p.ch(bufs[1][x].r) + p.ch(fwd[1][x].r);
        const acc_t sg15 = p.ch(bufs[1][x].g) + p.ch(fwd[1][x].g);
        const acc_t sb15 = p.ch(bufs[1][x].b) + p.ch(fwd[1][x].b);
        const acc_t sr24 = p.ch(bufs[2][x].r) + p.ch(fwd[0][x].r);
        const acc_t sg24 = p.ch(bufs[2][x].g) + p.ch(fwd[0][x].g);
        const acc_t sb24 = p.ch(bufs[2][x].b) + p.ch(fwd[0][x].b);
        r = sr06 + sr15 * 6 + sr24 * 15 + p.ch(bufs[3][x].r) * 20;
        g = sg06 + sg15 * 6 + sg24 * 15 + p.ch(bufs[3][x].g) * 20;
        b = sb06 + sb15 * 6 + sb24 * 15 + p.ch(bufs[3][x].b) * 20;
    }
};

template <typename RGB_T, typename acc_t>
struct vpass_pixel_kernel<4, RGB_T, acc_t> {
    FL_ALWAYS_INLINE void apply(RGB_T **bufs, const RGB_T **fwd, int x,
                                        acc_t &r, acc_t &g, acc_t &b) {
        pixel_ops<RGB_T> p;
        const acc_t sr08 = p.ch(bufs[0][x].r) + p.ch(fwd[3][x].r);
        const acc_t sg08 = p.ch(bufs[0][x].g) + p.ch(fwd[3][x].g);
        const acc_t sb08 = p.ch(bufs[0][x].b) + p.ch(fwd[3][x].b);
        const acc_t sr17 = p.ch(bufs[1][x].r) + p.ch(fwd[2][x].r);
        const acc_t sg17 = p.ch(bufs[1][x].g) + p.ch(fwd[2][x].g);
        const acc_t sb17 = p.ch(bufs[1][x].b) + p.ch(fwd[2][x].b);
        const acc_t sr26 = p.ch(bufs[2][x].r) + p.ch(fwd[1][x].r);
        const acc_t sg26 = p.ch(bufs[2][x].g) + p.ch(fwd[1][x].g);
        const acc_t sb26 = p.ch(bufs[2][x].b) + p.ch(fwd[1][x].b);
        const acc_t sr35 = p.ch(bufs[3][x].r) + p.ch(fwd[0][x].r);
        const acc_t sg35 = p.ch(bufs[3][x].g) + p.ch(fwd[0][x].g);
        const acc_t sb35 = p.ch(bufs[3][x].b) + p.ch(fwd[0][x].b);
        r = sr08 + sr17 * 8 + sr26 * 28 + sr35 * 56 + p.ch(bufs[4][x].r) * 70;
        g = sg08 + sg17 * 8 + sg26 * 28 + sg35 * 56 + p.ch(bufs[4][x].g) * 70;
        b = sb08 + sb17 * 8 + sb26 * 28 + sb35 * 56 + p.ch(bufs[4][x].b) * 70;
    }
};

#endif // !FL_IS_AVR

// ── Row-major vertical pass (non-AVR) ──────────────────────────────────
// Processes vertical convolution in row-major order for cache efficiency.
// Instead of gathering individual columns into a linear buffer (strided
// reads + writes), iterates row by row with sequential memory access.
// Uses a ring buffer of R+1 saved rows to hold originals of overwritten rows.
// scratch must have at least (R+2)*w elements.
#if !defined(FL_IS_AVR)

template <int R, typename RGB_T, typename acc_t, bool ApplyAlpha, typename AlphaT>
FL_OPTIMIZE_FUNCTION
static void vpass_rowmajor_impl(
    RGB_T *pixels, int w, int h,
    RGB_T *scratch, AlphaT alpha)
{
    constexpr int shift = 2 * R;
    using P = pixel_ops<RGB_T>;

    // Ring buffer: bufs[0..R-1] = saved previous rows, bufs[R] = save slot.
    // Extra zero_row for bottom-boundary padding.
    RGB_T *bufs[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    for (int i = 0; i <= R; ++i)
        bufs[i] = scratch + i * w;
    RGB_T *zero_row = scratch + (R + 1) * w;

    // Zero all: first R buffers (top-boundary padding) + zero_row.
    FL_BUILTIN_MEMSET(scratch, 0, (R + 2) * w * sizeof(RGB_T));

    for (int y = 0; y < h; ++y) {
        RGB_T *out_row = pixels + y * w;

        // Save current row before we overwrite it.
        FL_BUILTIN_MEMCPY(bufs[R], out_row, w * sizeof(RGB_T));

        // Forward row pointers (rows y+1 .. y+R, or zero_row if OOB).
        const RGB_T *fwd[4] = {zero_row, zero_row, zero_row, zero_row};
        for (int k = 0; k < R; ++k)
            fwd[k] = (y + 1 + k < h) ? (pixels + (y + 1 + k) * w) : zero_row;

        // Prefetch the furthest-ahead row needed by the NEXT iteration.
        // At row y, next iteration needs fwd[R-1] = row y+1+R.
        // Prefetching 2 rows ahead gives the memory subsystem time to fetch.
        {
            const int prefetch_y = y + R + 2;
            if (prefetch_y < h) {
                const char *pf = (const char *)(pixels + prefetch_y * w);
                const int row_bytes = w * (int)sizeof(RGB_T);
                for (int off = 0; off < row_bytes; off += 64)
                    __builtin_prefetch(pf + off, 0, 3);
            }
        }

        // Process all pixels in this output row.
        // For u8-channel types (CRGB), process as raw byte stream — all
        // channels use the same kernel weights, so we treat the row as a
        // flat u8 array of w*sizeof(RGB_T) bytes. This produces a simpler
        // loop that the compiler can optimize better at low -O levels.
        if (sizeof(typename RGB_T::fp) == 1 && !ApplyAlpha) {
            // Raw byte fast path (CRGB without alpha).
            const int nbytes = w * (int)sizeof(RGB_T);
            u8 *ob = (u8 *)out_row;

            simd_vconv_dispatch<R>::apply(bufs, fwd, ob, nbytes);
        } else {
            // Generic path: per-pixel struct access (CRGB16 or alpha case).
            for (int x = 0; x < w; ++x) {
                acc_t r, g, b;

                vpass_pixel_kernel<R, RGB_T, acc_t>::apply(bufs, fwd, x, r, g, b);

                if (ApplyAlpha) {
                    out_row[x] = P::make(static_cast<acc_t>(r >> shift),
                                         static_cast<acc_t>(g >> shift),
                                         static_cast<acc_t>(b >> shift), alpha);
                } else {
                    out_row[x] = P::make(static_cast<acc_t>(r >> shift),
                                         static_cast<acc_t>(g >> shift),
                                         static_cast<acc_t>(b >> shift));
                }
            }
        }

        // Rotate ring buffer: discard oldest, current becomes newest saved.
        RGB_T *recycled = bufs[0];
        for (int i = 0; i < R; ++i) bufs[i] = bufs[i + 1];
        bufs[R] = recycled;
    }
}

#endif // !FL_IS_AVR

// ── Helper: pad buffer size calculation ─────────────────────────────────
template <int hR, int vR, typename RGB_T>
static int compute_pad_size(int w, int h) {
    int hPad = 2 * hR + w;
#if defined(FL_IS_AVR)
    int vPad = 2 * vR + h;
#else
    int vPad = vR > 0 ? (vR + 2) * w : 0;
#endif
    return hPad > vPad ? hPad : vPad;
}

// ── Helper: horizontal pass for one padded row ─────────────────────────
// Dispatches to the appropriate kernel based on platform and pixel type.
template <int R, typename RGB_T, typename acc_t, bool ApplyAlpha, typename AlphaT>
FL_ALWAYS_INLINE
void hpass_row(RGB_T *pad, RGB_T *out, int w, AlphaT alpha) {
#if defined(FL_IS_AVR)
    if (ApplyAlpha)
        apply_pass_alpha_1ch<R>(pad, out, w, 1, alpha);
    else
        apply_pass_1ch<R>(pad, out, w, 1);
#else
    // SIMD fast path: u8-channel (CRGB), no alpha on this pass.
    if (sizeof(typename RGB_T::fp) == 1 && !ApplyAlpha) {
        constexpr int S = (int)sizeof(RGB_T);
        const int nbytes = w * S;
        const u8 *pb = (const u8 *)pad;
        u8 *ob = (u8 *)out;
        simd_hconv_dispatch<R>::apply(
            pb, S, ob, nbytes, (u8 *)(pad + 2 * R + w), w);
    } else if (ApplyAlpha) {
        apply_pass_alpha<R, RGB_T, acc_t>(pad, out, w, 1, alpha);
    } else {
        apply_pass<R, RGB_T, acc_t>(pad, out, w, 1);
    }
#endif
}

// ── Helper: vertical pass over entire image ─────────────────────────────
template <int R, typename RGB_T, typename acc_t, bool ApplyAlpha, typename AlphaT>
static void vpass_full(RGB_T *pixels, int w, int h, RGB_T *scratch, AlphaT alpha) {
#if defined(FL_IS_AVR)
    // AVR: column-by-column with per-channel noinline + O3.
    FL_BUILTIN_MEMSET(scratch, 0, R * sizeof(RGB_T));
    FL_BUILTIN_MEMSET(scratch + R + h, 0, R * sizeof(RGB_T));

    for (int x = 0; x < w; ++x) {
        // Linearize column into padded region.
        {
            const RGB_T *src = pixels + x;
            RGB_T *dst = scratch + R;
            for (int i = 0; i < h; ++i) {
                *dst++ = *src;
                src += w;
            }
        }
        if (ApplyAlpha)
            apply_pass_alpha_1ch<R>(scratch, pixels + x, h, w, alpha);
        else
            apply_pass_1ch<R>(scratch, pixels + x, h, w);
    }
#else
    // Non-AVR: row-major vertical pass for cache efficiency.
    // Direct R=2 V-pass using exact [1,4,6,4,1] kernel (no cascaded R=1).
    if (ApplyAlpha) {
        vpass_rowmajor_impl<R, RGB_T, acc_t, true>(
            pixels, w, h, scratch, alpha);
    } else {
        vpass_rowmajor_impl<R, RGB_T, acc_t, false>(
            pixels, w, h, scratch, alpha);
    }
#endif
}

} // namespace blur_detail

// Separable Gaussian blur: horizontal pass then vertical pass.
// Zero-padding approach: copy row/column to a padded buffer with zeros,
// then apply the fast unrolled interior_row kernel to ALL positions.
// This eliminates slow edge handling and reuses interior_row for both passes.
// Dim (alpha) is applied once at the final output.
template <int hRadius, int vRadius, typename RGB_T, typename AlphaT>
FL_OPTIMIZE_FUNCTION
void blurGaussianImpl(Canvas<RGB_T> &canvas, AlphaT alpha) {
    const int w = canvas.width;
    const int h = canvas.height;
    if (w <= 0 || h <= 0)
        return;

    using P = blur_detail::pixel_ops<RGB_T>;

    // Accumulator: u16 for 8-bit channels (max per-pass sum: 255*256=65280),
    // u32 for wider channels.
    using acc_t = fl::conditional_t<sizeof(typename RGB_T::fp) == 1, u16, u32>;

    const bool applyAlpha = !(alpha == blur_detail::alpha_identity<AlphaT>());

    // Handle no-blur case (radius 0 in both dimensions).
    if (hRadius == 0 && vRadius == 0) {
        if (applyAlpha) {
            RGB_T *pixels = canvas.pixels;
            for (int i = 0; i < w * h; ++i) {
                RGB_T &p = pixels[i];
                p = P::make(P::ch(p.r), P::ch(p.g), P::ch(p.b), alpha);
            }
        }
        return;
    }

    fl::span<RGB_T> padbuf = blur_detail::get_padbuf<RGB_T>(
        blur_detail::compute_pad_size<hRadius, vRadius, RGB_T>(w, h));
    RGB_T *pad = padbuf.data();
    RGB_T *pixels = canvas.pixels;

    // ── Horizontal pass ──────────────────────────────────────────────
    if (hRadius > 0) {
        // Zero the fixed padding regions once (reused for every row).
        FL_BUILTIN_MEMSET(pad, 0, hRadius * sizeof(RGB_T));
        FL_BUILTIN_MEMSET(pad + hRadius + w, 0, hRadius * sizeof(RGB_T));

        for (int y = 0; y < h; ++y) {
            RGB_T *row = pixels + y * w;
            FL_BUILTIN_MEMCPY(pad + hRadius, row, w * sizeof(RGB_T));

            if (vRadius == 0 && applyAlpha)
                blur_detail::hpass_row<hRadius, RGB_T, acc_t, true>(
                    pad, row, w, alpha);
            else
                blur_detail::hpass_row<hRadius, RGB_T, acc_t, false>(
                    pad, row, w, alpha);
        }
    }

    // ── Vertical pass ──────────────────────────────────────────────────
    if (vRadius > 0) {
        if (applyAlpha)
            blur_detail::vpass_full<vRadius, RGB_T, acc_t, true>(
                pixels, w, h, pad, alpha);
        else
            blur_detail::vpass_full<vRadius, RGB_T, acc_t, false>(
                pixels, w, h, pad, alpha);
    }
}

// ── alpha8 overload (UNORM8 dim) ─────────────────────────────────────────

template <int hRadius, int vRadius, typename RGB_T>
void blurGaussian(Canvas<RGB_T> &canvas, alpha8 dimFactor) {
    blurGaussianImpl<hRadius, vRadius>(canvas, dimFactor);
}

// ── alpha16 overload (UNORM16 dim — true 16-bit precision) ───────────────

template <int hRadius, int vRadius, typename RGB_T>
void blurGaussian(Canvas<RGB_T> &canvas, alpha16 dimFactor) {
    blurGaussianImpl<hRadius, vRadius>(canvas, dimFactor);
}

// ── CanvasMapped: XYMap-based Gaussian blur ──────────────────────────────
// Delegates to the optimized Canvas path when the XYMap is rectangular.
// Falls back to per-pixel gather/scatter through XYMap otherwise.
template <int hRadius, int vRadius, typename RGB_T, typename AlphaT>
FL_OPTIMIZE_FUNCTION
void blurGaussianMappedImpl(CanvasMapped<RGB_T> &canvas, AlphaT alpha) {
    const int w = canvas.width;
    const int h = canvas.height;
    if (w <= 0 || h <= 0)
        return;

    // Fast path: rectangular XYMap → delegate to optimized Canvas blur.
    if (canvas.xymap->isRectangularGrid()) {
        Canvas<RGB_T> rect(canvas.pixels, w, h);
        blurGaussianImpl<hRadius, vRadius>(rect, alpha);
        return;
    }

    // Slow path: non-rectangular XYMap → per-pixel gather/scatter.
    using P = blur_detail::pixel_ops<RGB_T>;
    using acc_t = fl::conditional_t<sizeof(typename RGB_T::fp) == 1, u16, u32>;

    const bool applyAlpha = !(alpha == blur_detail::alpha_identity<AlphaT>());

    // Handle no-blur case.
    if (hRadius == 0 && vRadius == 0) {
        if (applyAlpha) {
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    RGB_T &p = canvas.at(x, y);
                    P ops;
                    p = ops.make(ops.ch(p.r), ops.ch(p.g), ops.ch(p.b), alpha);
                }
            }
        }
        return;
    }

    fl::span<RGB_T> padbuf = blur_detail::get_padbuf<RGB_T>(
        blur_detail::compute_pad_size<hRadius, vRadius, RGB_T>(w, h));
    RGB_T *pad = padbuf.data();

    // ── Horizontal pass: gather row via XYMap, convolve, scatter back ──
    if (hRadius > 0) {
        FL_BUILTIN_MEMSET(pad, 0, hRadius * sizeof(RGB_T));
        FL_BUILTIN_MEMSET(pad + hRadius + w, 0, hRadius * sizeof(RGB_T));

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x)
                pad[hRadius + x] = canvas.at(x, y);

            constexpr int shift = 2 * hRadius;
            for (int x = 0; x < w; ++x) {
                acc_t r, g, b;
                blur_detail::interior_row<hRadius, RGB_T, acc_t>::apply(pad, hRadius + x, r, g, b);
                RGB_T result;
                if (vRadius == 0 && applyAlpha)
                    result = P().make(static_cast<acc_t>(r >> shift),
                                     static_cast<acc_t>(g >> shift),
                                     static_cast<acc_t>(b >> shift), alpha);
                else
                    result = P().make(static_cast<acc_t>(r >> shift),
                                     static_cast<acc_t>(g >> shift),
                                     static_cast<acc_t>(b >> shift));
                canvas.at(x, y) = result;
            }
        }
    }

    // ── Vertical pass: gather column via XYMap, convolve, scatter back ──
    if (vRadius > 0) {
        FL_BUILTIN_MEMSET(pad, 0, vRadius * sizeof(RGB_T));
        FL_BUILTIN_MEMSET(pad + vRadius + h, 0, vRadius * sizeof(RGB_T));

        for (int x = 0; x < w; ++x) {
            for (int y = 0; y < h; ++y)
                pad[vRadius + y] = canvas.at(x, y);

            constexpr int shift = 2 * vRadius;
            for (int y = 0; y < h; ++y) {
                acc_t r, g, b;
                blur_detail::interior_row<vRadius, RGB_T, acc_t>::apply(pad, vRadius + y, r, g, b);
                RGB_T result;
                if (applyAlpha)
                    result = P().make(static_cast<acc_t>(r >> shift),
                                     static_cast<acc_t>(g >> shift),
                                     static_cast<acc_t>(b >> shift), alpha);
                else
                    result = P().make(static_cast<acc_t>(r >> shift),
                                     static_cast<acc_t>(g >> shift),
                                     static_cast<acc_t>(b >> shift));
                canvas.at(x, y) = result;
            }
        }
    }
}

// ── CanvasMapped alpha8 overload ─────────────────────────────────────────

template <int hRadius, int vRadius, typename RGB_T>
void blurGaussian(CanvasMapped<RGB_T> &canvas, alpha8 dimFactor) {
    blurGaussianMappedImpl<hRadius, vRadius>(canvas, dimFactor);
}

// ── CanvasMapped alpha16 overload ────────────────────────────────────────

template <int hRadius, int vRadius, typename RGB_T>
void blurGaussian(CanvasMapped<RGB_T> &canvas, alpha16 dimFactor) {
    blurGaussianMappedImpl<hRadius, vRadius>(canvas, dimFactor);
}

// ── Explicit instantiations: alpha8 overload ─────────────────────────────

#define BLUR_INST_F8(H, V, T) \
    template void blurGaussian<H, V, T>(Canvas<T> &, alpha8);

// CRGB — symmetric, h-only, v-only, asymmetric.
BLUR_INST_F8(0, 0, CRGB)  BLUR_INST_F8(1, 1, CRGB)
BLUR_INST_F8(2, 2, CRGB)  BLUR_INST_F8(3, 3, CRGB)  BLUR_INST_F8(4, 4, CRGB)
BLUR_INST_F8(1, 0, CRGB)  BLUR_INST_F8(2, 0, CRGB)
BLUR_INST_F8(3, 0, CRGB)  BLUR_INST_F8(4, 0, CRGB)
BLUR_INST_F8(0, 1, CRGB)  BLUR_INST_F8(0, 2, CRGB)
BLUR_INST_F8(0, 3, CRGB)  BLUR_INST_F8(0, 4, CRGB)
BLUR_INST_F8(1, 2, CRGB)  BLUR_INST_F8(2, 1, CRGB)

// CRGB16 — same combos (not available on AVR due to RAM constraints).
#if !defined(FL_IS_AVR)
BLUR_INST_F8(0, 0, CRGB16)  BLUR_INST_F8(1, 1, CRGB16)
BLUR_INST_F8(2, 2, CRGB16)  BLUR_INST_F8(3, 3, CRGB16)  BLUR_INST_F8(4, 4, CRGB16)
BLUR_INST_F8(1, 0, CRGB16)  BLUR_INST_F8(2, 0, CRGB16)
BLUR_INST_F8(3, 0, CRGB16)  BLUR_INST_F8(4, 0, CRGB16)
BLUR_INST_F8(0, 1, CRGB16)  BLUR_INST_F8(0, 2, CRGB16)
BLUR_INST_F8(0, 3, CRGB16)  BLUR_INST_F8(0, 4, CRGB16)
BLUR_INST_F8(1, 2, CRGB16)  BLUR_INST_F8(2, 1, CRGB16)
#endif

#undef BLUR_INST_F8

// ── Explicit instantiations: alpha16 overload ────────────────────────────

#define BLUR_INST_F16(H, V, T) \
    template void blurGaussian<H, V, T>(Canvas<T> &, alpha16);

// CRGB
BLUR_INST_F16(0, 0, CRGB)  BLUR_INST_F16(1, 1, CRGB)
BLUR_INST_F16(2, 2, CRGB)  BLUR_INST_F16(3, 3, CRGB)  BLUR_INST_F16(4, 4, CRGB)
BLUR_INST_F16(1, 0, CRGB)  BLUR_INST_F16(2, 0, CRGB)
BLUR_INST_F16(3, 0, CRGB)  BLUR_INST_F16(4, 0, CRGB)
BLUR_INST_F16(0, 1, CRGB)  BLUR_INST_F16(0, 2, CRGB)
BLUR_INST_F16(0, 3, CRGB)  BLUR_INST_F16(0, 4, CRGB)
BLUR_INST_F16(1, 2, CRGB)  BLUR_INST_F16(2, 1, CRGB)

// CRGB16
#if !defined(FL_IS_AVR)
BLUR_INST_F16(0, 0, CRGB16)  BLUR_INST_F16(1, 1, CRGB16)
BLUR_INST_F16(2, 2, CRGB16)  BLUR_INST_F16(3, 3, CRGB16)  BLUR_INST_F16(4, 4, CRGB16)
BLUR_INST_F16(1, 0, CRGB16)  BLUR_INST_F16(2, 0, CRGB16)
BLUR_INST_F16(3, 0, CRGB16)  BLUR_INST_F16(4, 0, CRGB16)
BLUR_INST_F16(0, 1, CRGB16)  BLUR_INST_F16(0, 2, CRGB16)
BLUR_INST_F16(0, 3, CRGB16)  BLUR_INST_F16(0, 4, CRGB16)
BLUR_INST_F16(1, 2, CRGB16)  BLUR_INST_F16(2, 1, CRGB16)
#endif

#undef BLUR_INST_F16

// ── Explicit instantiations: CanvasMapped alpha8 ─────────────────────────

#define BLUR_MAPPED_INST_F8(H, V, T) \
    template void blurGaussian<H, V, T>(CanvasMapped<T> &, alpha8);

BLUR_MAPPED_INST_F8(0, 0, CRGB)  BLUR_MAPPED_INST_F8(1, 1, CRGB)
BLUR_MAPPED_INST_F8(2, 2, CRGB)  BLUR_MAPPED_INST_F8(3, 3, CRGB)  BLUR_MAPPED_INST_F8(4, 4, CRGB)
BLUR_MAPPED_INST_F8(1, 0, CRGB)  BLUR_MAPPED_INST_F8(2, 0, CRGB)
BLUR_MAPPED_INST_F8(3, 0, CRGB)  BLUR_MAPPED_INST_F8(4, 0, CRGB)
BLUR_MAPPED_INST_F8(0, 1, CRGB)  BLUR_MAPPED_INST_F8(0, 2, CRGB)
BLUR_MAPPED_INST_F8(0, 3, CRGB)  BLUR_MAPPED_INST_F8(0, 4, CRGB)
BLUR_MAPPED_INST_F8(1, 2, CRGB)  BLUR_MAPPED_INST_F8(2, 1, CRGB)

#if !defined(FL_IS_AVR)
BLUR_MAPPED_INST_F8(0, 0, CRGB16)  BLUR_MAPPED_INST_F8(1, 1, CRGB16)
BLUR_MAPPED_INST_F8(2, 2, CRGB16)  BLUR_MAPPED_INST_F8(3, 3, CRGB16)  BLUR_MAPPED_INST_F8(4, 4, CRGB16)
BLUR_MAPPED_INST_F8(1, 0, CRGB16)  BLUR_MAPPED_INST_F8(2, 0, CRGB16)
BLUR_MAPPED_INST_F8(3, 0, CRGB16)  BLUR_MAPPED_INST_F8(4, 0, CRGB16)
BLUR_MAPPED_INST_F8(0, 1, CRGB16)  BLUR_MAPPED_INST_F8(0, 2, CRGB16)
BLUR_MAPPED_INST_F8(0, 3, CRGB16)  BLUR_MAPPED_INST_F8(0, 4, CRGB16)
BLUR_MAPPED_INST_F8(1, 2, CRGB16)  BLUR_MAPPED_INST_F8(2, 1, CRGB16)
#endif

#undef BLUR_MAPPED_INST_F8

// ── Explicit instantiations: CanvasMapped alpha16 ────────────────────────

#define BLUR_MAPPED_INST_F16(H, V, T) \
    template void blurGaussian<H, V, T>(CanvasMapped<T> &, alpha16);

BLUR_MAPPED_INST_F16(0, 0, CRGB)  BLUR_MAPPED_INST_F16(1, 1, CRGB)
BLUR_MAPPED_INST_F16(2, 2, CRGB)  BLUR_MAPPED_INST_F16(3, 3, CRGB)  BLUR_MAPPED_INST_F16(4, 4, CRGB)
BLUR_MAPPED_INST_F16(1, 0, CRGB)  BLUR_MAPPED_INST_F16(2, 0, CRGB)
BLUR_MAPPED_INST_F16(3, 0, CRGB)  BLUR_MAPPED_INST_F16(4, 0, CRGB)
BLUR_MAPPED_INST_F16(0, 1, CRGB)  BLUR_MAPPED_INST_F16(0, 2, CRGB)
BLUR_MAPPED_INST_F16(0, 3, CRGB)  BLUR_MAPPED_INST_F16(0, 4, CRGB)
BLUR_MAPPED_INST_F16(1, 2, CRGB)  BLUR_MAPPED_INST_F16(2, 1, CRGB)

#if !defined(FL_IS_AVR)
BLUR_MAPPED_INST_F16(0, 0, CRGB16)  BLUR_MAPPED_INST_F16(1, 1, CRGB16)
BLUR_MAPPED_INST_F16(2, 2, CRGB16)  BLUR_MAPPED_INST_F16(3, 3, CRGB16)  BLUR_MAPPED_INST_F16(4, 4, CRGB16)
BLUR_MAPPED_INST_F16(1, 0, CRGB16)  BLUR_MAPPED_INST_F16(2, 0, CRGB16)
BLUR_MAPPED_INST_F16(3, 0, CRGB16)  BLUR_MAPPED_INST_F16(4, 0, CRGB16)
BLUR_MAPPED_INST_F16(0, 1, CRGB16)  BLUR_MAPPED_INST_F16(0, 2, CRGB16)
BLUR_MAPPED_INST_F16(0, 3, CRGB16)  BLUR_MAPPED_INST_F16(0, 4, CRGB16)
BLUR_MAPPED_INST_F16(1, 2, CRGB16)  BLUR_MAPPED_INST_F16(2, 1, CRGB16)
#endif

#undef BLUR_MAPPED_INST_F16

} // namespace gfx
} // namespace fl

FL_OPTIMIZATION_LEVEL_O3_END
