

#include "fl/stl/stdint.h"

#define FASTLED_INTERNAL
#include "fl/fastled.h"

#include "crgb.h"
#include "fl/gfx/blur.h"
#include "fl/gfx/colorutils_misc.h"
#include "fl/stl/compiler_control.h"
#include "fl/system/log.h"
#include "fl/gfx/xymap.h"
#include "lib8tion/scale8.h"
#include "fl/stl/int.h"
#include "fl/stl/span.h"
#include "fl/gfx/crgb.h"
#include "fl/gfx/crgb16.h"
#include "fl/stl/thread_local.h"
#include "fl/stl/vector.h"

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
    FASTLED_UNUSED(xymap);
    Canvas<CRGB> canvas(leds, width, height);
    gfx::blur2d(canvas, alpha8(blur_amount));
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
    FASTLED_UNUSED(xyMap);
    Canvas<CRGB> canvas(leds, width, height);
    gfx::blurRows(canvas, alpha8(blur_amount));
}

void blurColumns(fl::span<CRGB> leds, fl::u8 width, fl::u8 height,
                 fract8 blur_amount, const XYMap &xyMap) {
    FASTLED_UNUSED(xyMap);
    Canvas<CRGB> canvas(leds, width, height);
    gfx::blurColumns(canvas, alpha8(blur_amount));
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
static RGB_T *get_padbuf(int minSize) {
    static fl::ThreadLocal<fl::vector<RGB_T>> tl_padbuf;
    fl::vector<RGB_T> &buf = tl_padbuf.access();
    if (static_cast<int>(buf.size()) < minSize) {
        buf.resize(minSize);
    }
    return buf.data();
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
        r = 0; g = 0; b = 0;
        { const RGB_T &px = row[x-2]; r += p.ch(px.r);     g += p.ch(px.g);     b += p.ch(px.b); }
        { const RGB_T &px = row[x-1]; r += p.ch(px.r) * 4; g += p.ch(px.g) * 4; b += p.ch(px.b) * 4; }
        { const RGB_T &px = row[x];   r += p.ch(px.r) * 6; g += p.ch(px.g) * 6; b += p.ch(px.b) * 6; }
        { const RGB_T &px = row[x+1]; r += p.ch(px.r) * 4; g += p.ch(px.g) * 4; b += p.ch(px.b) * 4; }
        { const RGB_T &px = row[x+2]; r += p.ch(px.r);     g += p.ch(px.g);     b += p.ch(px.b); }
    }
};

template <typename RGB_T, typename acc_t>
struct interior_row<3, RGB_T, acc_t> {
    FL_ALWAYS_INLINE void apply(const RGB_T *row, int x,
                             acc_t &r, acc_t &g, acc_t &b) {
        using P = pixel_ops<RGB_T>;
        P p;
        r = 0; g = 0; b = 0;
        { const RGB_T &px = row[x-3]; r += p.ch(px.r);      g += p.ch(px.g);      b += p.ch(px.b); }
        { const RGB_T &px = row[x-2]; r += p.ch(px.r) * 6;  g += p.ch(px.g) * 6;  b += p.ch(px.b) * 6; }
        { const RGB_T &px = row[x-1]; r += p.ch(px.r) * 15; g += p.ch(px.g) * 15; b += p.ch(px.b) * 15; }
        { const RGB_T &px = row[x];   r += p.ch(px.r) * 20; g += p.ch(px.g) * 20; b += p.ch(px.b) * 20; }
        { const RGB_T &px = row[x+1]; r += p.ch(px.r) * 15; g += p.ch(px.g) * 15; b += p.ch(px.b) * 15; }
        { const RGB_T &px = row[x+2]; r += p.ch(px.r) * 6;  g += p.ch(px.g) * 6;  b += p.ch(px.b) * 6; }
        { const RGB_T &px = row[x+3]; r += p.ch(px.r);      g += p.ch(px.g);      b += p.ch(px.b); }
    }
};

template <typename RGB_T, typename acc_t>
struct interior_row<4, RGB_T, acc_t> {
    FL_ALWAYS_INLINE void apply(const RGB_T *row, int x,
                             acc_t &r, acc_t &g, acc_t &b) {
        using P = pixel_ops<RGB_T>;
        P p;
        r = 0; g = 0; b = 0;
        { const RGB_T &px = row[x-4]; r += p.ch(px.r);      g += p.ch(px.g);      b += p.ch(px.b); }
        { const RGB_T &px = row[x-3]; r += p.ch(px.r) * 8;  g += p.ch(px.g) * 8;  b += p.ch(px.b) * 8; }
        { const RGB_T &px = row[x-2]; r += p.ch(px.r) * 28; g += p.ch(px.g) * 28; b += p.ch(px.b) * 28; }
        { const RGB_T &px = row[x-1]; r += p.ch(px.r) * 56; g += p.ch(px.g) * 56; b += p.ch(px.b) * 56; }
        { const RGB_T &px = row[x];   r += p.ch(px.r) * 70; g += p.ch(px.g) * 70; b += p.ch(px.b) * 70; }
        { const RGB_T &px = row[x+1]; r += p.ch(px.r) * 56; g += p.ch(px.g) * 56; b += p.ch(px.b) * 56; }
        { const RGB_T &px = row[x+2]; r += p.ch(px.r) * 28; g += p.ch(px.g) * 28; b += p.ch(px.b) * 28; }
        { const RGB_T &px = row[x+3]; r += p.ch(px.r) * 8;  g += p.ch(px.g) * 8;  b += p.ch(px.b) * 8; }
        { const RGB_T &px = row[x+4]; r += p.ch(px.r);      g += p.ch(px.g);      b += p.ch(px.b); }
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
        // [1, 2, 1]
        return (u16)p[0] + ((u16)p[S] << 1) + (u16)p[2*S];
    }
};

template <> struct conv1ch<2> {
    static inline u16 __attribute__((always_inline)) apply(const u8 *c) {
        constexpr int S = sizeof(CRGB);
        const u8 *p = c - 2*S;
        // [1, 4, 6, 4, 1]
        return (u16)p[0] + (u16)p[S] * 4 + (u16)p[2*S] * 6
             + (u16)p[3*S] * 4 + (u16)p[4*S];
    }
};

template <> struct conv1ch<3> {
    static inline u16 __attribute__((always_inline)) apply(const u8 *c) {
        constexpr int S = sizeof(CRGB);
        const u8 *p = c - 3*S;
        // [1, 6, 15, 20, 15, 6, 1]
        return (u16)p[0] + (u16)p[S] * 6  + (u16)p[2*S] * 15
             + (u16)p[3*S] * 20 + (u16)p[4*S] * 15 + (u16)p[5*S] * 6
             + (u16)p[6*S];
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
__attribute__((noinline))
static void apply_pass_1ch(const CRGB *pad, CRGB *out, int count, int stride) {
    constexpr int shift = 2 * R;
    for (int i = 0; i < count; ++i) {
        const u8 *base = pad[R + i].raw;
        out->r = static_cast<u8>(conv1ch<R>::apply(base + 0) >> shift);
        out->g = static_cast<u8>(conv1ch<R>::apply(base + 1) >> shift);
        out->b = static_cast<u8>(conv1ch<R>::apply(base + 2) >> shift);
        out += stride;
    }
}

// AVR noinline per-channel pass for CRGB with alpha8 dim.
template <int R>
__attribute__((noinline))
static void apply_pass_alpha_1ch(const CRGB *pad, CRGB *out, int count,
                                  int stride, alpha8 alpha) {
    constexpr int shift = 2 * R;
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
__attribute__((noinline))
static void apply_pass_alpha_1ch(const CRGB *pad, CRGB *out, int count,
                                  int stride, alpha16 alpha) {
    constexpr int shift = 2 * R;
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
FL_NO_INLINE_IF_AVR
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
FL_NO_INLINE_IF_AVR
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


} // namespace blur_detail

// Separable Gaussian blur: horizontal pass then vertical pass.
// Zero-padding approach: copy row/column to a padded buffer with zeros,
// then apply the fast unrolled interior_row kernel to ALL positions.
// This eliminates slow edge handling and reuses interior_row for both passes.
// Dim (alpha) is applied once at the final output.
template <int hRadius, int vRadius, typename RGB_T, typename AlphaT>
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

    // Padded pixel buffer: max(2*hRadius + w, 2*vRadius + h).
    const int hPadSize = 2 * hRadius + w;
    const int vPadSize = 2 * vRadius + h;
    const int padSize = hPadSize > vPadSize ? hPadSize : vPadSize;
    RGB_T *pad = blur_detail::get_padbuf<RGB_T>(padSize);

    RGB_T *pixels = canvas.pixels;

    // ── Horizontal pass ──────────────────────────────────────────────
    if (hRadius > 0) {
        constexpr int hShift = 2 * hRadius;

        // Zero the fixed padding regions once (reused for every row).
        __builtin_memset(pad, 0, hRadius * sizeof(RGB_T));
        __builtin_memset(pad + hRadius + w, 0, hRadius * sizeof(RGB_T));

        for (int y = 0; y < h; ++y) {
            RGB_T *row = pixels + y * w;

            // Copy row data into padded region.
            FL_BUILTIN_MEMCPY(pad + hRadius, row, w * sizeof(RGB_T));

            // Apply interior kernel to ALL positions (zero-padding handles edges).
            // R <= 1: inline loop (small kernels; extracting to a function adds
            //         ~17 us call overhead that exceeds the register benefit).
            // R >= 2: noinline call on AVR (register pressure relief saves 47-125 us).
            if (hRadius <= 1) {
                if (vRadius == 0 && applyAlpha) {
                    for (int x = 0; x < w; ++x) {
                        acc_t r, g, b;
                        blur_detail::interior_row<hRadius, RGB_T, acc_t>::apply(
                            pad, hRadius + x, r, g, b);
                        row[x] = P::make(static_cast<acc_t>(r >> hShift),
                                         static_cast<acc_t>(g >> hShift),
                                         static_cast<acc_t>(b >> hShift), alpha);
                    }
                } else {
                    for (int x = 0; x < w; ++x) {
                        acc_t r, g, b;
                        blur_detail::interior_row<hRadius, RGB_T, acc_t>::apply(
                            pad, hRadius + x, r, g, b);
                        row[x] = P::make(static_cast<acc_t>(r >> hShift),
                                         static_cast<acc_t>(g >> hShift),
                                         static_cast<acc_t>(b >> hShift));
                    }
                }
            } else {
#if defined(FL_IS_AVR)
                // AVR R >= 4: per-channel noinline cuts register pressure
                // from 6 to 2 accumulators, a big win for the 9-tap kernel.
                if (hRadius >= 4) {
                    if (vRadius == 0 && applyAlpha)
                        blur_detail::apply_pass_alpha_1ch<hRadius>(
                            pad, row, w, 1, alpha);
                    else
                        blur_detail::apply_pass_1ch<hRadius>(
                            pad, row, w, 1);
                } else
#endif
                {
                    if (vRadius == 0 && applyAlpha) {
                        blur_detail::apply_pass_alpha<hRadius, RGB_T, acc_t>(
                            pad, row, w, 1, alpha);
                    } else {
                        blur_detail::apply_pass<hRadius, RGB_T, acc_t>(
                            pad, row, w, 1);
                    }
                }
            }
        }
    }

    // ── Vertical pass (linearized column → reuse interior_row kernel) ──
    if (vRadius > 0) {
        constexpr int vShift = 2 * vRadius;

        // Zero the fixed padding regions once (reused for every column).
        __builtin_memset(pad, 0, vRadius * sizeof(RGB_T));
        __builtin_memset(pad + vRadius + h, 0, vRadius * sizeof(RGB_T));

        for (int x = 0; x < w; ++x) {

            // Linearize column into padded region (pointer increment avoids multiply).
            {
                const RGB_T *src = pixels + x;
                RGB_T *dst = pad + vRadius;
                for (int i = 0; i < h; ++i) {
                    *dst++ = *src;
                    src += w;
                }
            }

            // Apply interior_row kernel (reused for columns via linearization).
            // Write back with stride=w to scatter back to column positions.
            if (vRadius <= 1) {
                // Inline loop for small kernels (pointer increment avoids multiply).
                RGB_T *dst = pixels + x;
                if (applyAlpha) {
                    for (int y = 0; y < h; ++y) {
                        acc_t r, g, b;
                        blur_detail::interior_row<vRadius, RGB_T, acc_t>::apply(
                            pad, vRadius + y, r, g, b);
                        *dst = P::make(static_cast<acc_t>(r >> vShift),
                                       static_cast<acc_t>(g >> vShift),
                                       static_cast<acc_t>(b >> vShift), alpha);
                        dst += w;
                    }
                } else {
                    for (int y = 0; y < h; ++y) {
                        acc_t r, g, b;
                        blur_detail::interior_row<vRadius, RGB_T, acc_t>::apply(
                            pad, vRadius + y, r, g, b);
                        *dst = P::make(static_cast<acc_t>(r >> vShift),
                                       static_cast<acc_t>(g >> vShift),
                                       static_cast<acc_t>(b >> vShift));
                        dst += w;
                    }
                }
            } else {
#if defined(FL_IS_AVR)
                // AVR R >= 4: per-channel noinline for severe register pressure.
                if (vRadius >= 4) {
                    if (applyAlpha)
                        blur_detail::apply_pass_alpha_1ch<vRadius>(
                            pad, pixels + x, h, w, alpha);
                    else
                        blur_detail::apply_pass_1ch<vRadius>(
                            pad, pixels + x, h, w);
                } else
#endif
                {
                    // Noinline call for large kernels (register pressure relief).
                    if (applyAlpha) {
                        blur_detail::apply_pass_alpha<vRadius, RGB_T, acc_t>(
                            pad, pixels + x, h, w, alpha);
                    } else {
                        blur_detail::apply_pass<vRadius, RGB_T, acc_t>(
                            pad, pixels + x, h, w);
                    }
                }
            }
        }
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

} // namespace gfx
} // namespace fl
