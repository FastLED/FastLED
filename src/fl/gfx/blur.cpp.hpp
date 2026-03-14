

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

// ── SKIPSM Gaussian blur — template implementation with alpha integration ────
//
// Separable two-pass (horizontal then vertical) blur using binomial
// coefficients from Pascal's triangle. Normalization is pure bit-shift
// since weights sum to 2^(2*radius).
//
// Alpha (dim factor) is applied directly in the blur element functions
// via pixel_ops::make() overloads, fusing blur and dim into one pass.
//
// Original implementation by sutaburosu, adapted for fl::gfx::Canvas.

namespace fl {
namespace gfx {

namespace blur_detail {

// Identity alpha value for each type (no dimming).
template <typename AlphaT> constexpr AlphaT alpha_identity();
template <> constexpr alpha8 alpha_identity<alpha8>() { return alpha8(255); }
template <> constexpr alpha16 alpha_identity<alpha16>() { return alpha16(65535); }

// Thread-local temp buffer for blur passes (one per RGB_T).
// Grows to fit the largest dimension seen, never shrinks.
template <typename RGB_T>
inline RGB_T* get_temp_buffer(int minSize) {
    static fl::ThreadLocal<fl::vector<RGB_T>> tl_temp;
    fl::vector<RGB_T> &buf = tl_temp.access();
    if (static_cast<int>(buf.size()) < minSize) {
        buf.resize(minSize);
    }
    return buf.data();
}

// Channel extraction and alpha-scaled pixel construction.
// make(r, g, b) — no dim.
// make(r, g, b, alpha8) — 8-bit UNORM dim.
// make(r, g, b, alpha16) — 16-bit UNORM dim (true precision for wide pixels).
template <typename RGB_T>
struct pixel_ops;

template <>
struct pixel_ops<CRGB> {
    static inline u32 ch(u8 v) { return v; }

    static inline CRGB make(u32 r, u32 g, u32 b) {
        return CRGB(static_cast<u8>(r), static_cast<u8>(g),
                    static_cast<u8>(b));
    }

    static inline CRGB make(u32 r, u32 g, u32 b, alpha8 a) {
        if (a.value == 255) return make(r, g, b);
        u32 a1 = static_cast<u32>(a.value) + 1;
        return CRGB(static_cast<u8>((r * a1) >> 8),
                    static_cast<u8>((g * a1) >> 8),
                    static_cast<u8>((b * a1) >> 8));
    }

    static inline CRGB make(u32 r, u32 g, u32 b, alpha16 a) {
        if (a.value >= 65535) return make(r, g, b);
        u32 a1 = static_cast<u32>(a.value) + 1;
        return CRGB(static_cast<u8>((r * a1) >> 16),
                    static_cast<u8>((g * a1) >> 16),
                    static_cast<u8>((b * a1) >> 16));
    }
};

template <>
struct pixel_ops<CRGB16> {
    static inline u32 ch(u8x8 v) { return v.raw(); }

    static inline CRGB16 make(u32 r, u32 g, u32 b) {
        return CRGB16(u8x8::from_raw(static_cast<u16>(r)),
                      u8x8::from_raw(static_cast<u16>(g)),
                      u8x8::from_raw(static_cast<u16>(b)));
    }

    static inline CRGB16 make(u32 r, u32 g, u32 b, alpha8 a) {
        if (a.value == 255) return make(r, g, b);
        u32 a1 = static_cast<u32>(a.value) + 1;
        return make((r * a1) >> 8, (g * a1) >> 8, (b * a1) >> 8);
    }

    static inline CRGB16 make(u32 r, u32 g, u32 b, alpha16 a) {
        if (a.value >= 65535) return make(r, g, b);
        u32 a1 = static_cast<u32>(a.value) + 1;
        return make((r * a1) >> 16, (g * a1) >> 16, (b * a1) >> 16);
    }
};

// Generic edge-pixel helper for horizontal pass with bounds checking.
template <typename acc_t, typename AlphaT, typename RGB_T>
inline RGB_T hPassGeneric(const RGB_T *row, int x, int w,
                          const int *weights, int radius, int shift,
                          AlphaT alpha) {
    using P = pixel_ops<RGB_T>;
    acc_t rsum = 0, gsum = 0, bsum = 0;
    for (int k = -radius; k <= radius; ++k) {
        int xx = x + k;
        if (xx >= 0 && xx < w) {
            const RGB_T &p = row[xx];
            int wt = weights[k + radius];
            rsum += P::ch(p.r) * wt;
            gsum += P::ch(p.g) * wt;
            bsum += P::ch(p.b) * wt;
        }
    }
    return P::make(rsum >> shift, gsum >> shift, bsum >> shift, alpha);
}

// Generic edge-pixel helper for vertical pass with bounds checking.
template <typename acc_t, typename AlphaT, typename RGB_T>
inline RGB_T vPassGeneric(const RGB_T *colBase, int y, int h, int stride,
                          const int *weights, int radius, int shift,
                          AlphaT alpha) {
    using P = pixel_ops<RGB_T>;
    acc_t rsum = 0, gsum = 0, bsum = 0;
    for (int k = -radius; k <= radius; ++k) {
        int yy = y + k;
        if (yy >= 0 && yy < h) {
            const RGB_T &p = colBase[yy * stride];
            int wt = weights[k + radius];
            rsum += P::ch(p.r) * wt;
            gsum += P::ch(p.g) * wt;
            bsum += P::ch(p.b) * wt;
        }
    }
    return P::make(rsum >> shift, gsum >> shift, bsum >> shift, alpha);
}

// ── Horizontal pass dispatch ─────────────────────────────────────────────

// Primary template — radius 0 = copy with alpha.
template <int Radius, typename acc_t, typename RGB_T>
struct HPass {
    template <typename AlphaT>
    static void run(const RGB_T *row, RGB_T *out, int w, AlphaT alpha) {
        using P = pixel_ops<RGB_T>;
        for (int x = 0; x < w; ++x) {
            out[x] = P::make(P::ch(row[x].r), P::ch(row[x].g),
                             P::ch(row[x].b), alpha);
        }
    }
};

// Radius 1: kernel [1 2 1], shift=2
template <typename acc_t, typename RGB_T>
struct HPass<1, acc_t, RGB_T> {
    template <typename AlphaT>
    static void run(const RGB_T *row, RGB_T *out, int w, AlphaT alpha) {
        using P = pixel_ops<RGB_T>;
        enum { SHIFT = 2 };
        // First pixel
        {
            acc_t r0 = P::ch(row[0].r) * 2 +
                       ((w > 1) ? P::ch(row[1].r) : P::ch(row[0].r));
            acc_t g0 = P::ch(row[0].g) * 2 +
                       ((w > 1) ? P::ch(row[1].g) : P::ch(row[0].g));
            acc_t b0 = P::ch(row[0].b) * 2 +
                       ((w > 1) ? P::ch(row[1].b) : P::ch(row[0].b));
            out[0] = P::make(r0 >> SHIFT, g0 >> SHIFT, b0 >> SHIFT, alpha);
        }
        // Interior
        for (int x = 1; x + 1 < w; ++x) {
            acc_t rsum = P::ch(row[x - 1].r) + P::ch(row[x].r) * 2 +
                         P::ch(row[x + 1].r);
            acc_t gsum = P::ch(row[x - 1].g) + P::ch(row[x].g) * 2 +
                         P::ch(row[x + 1].g);
            acc_t bsum = P::ch(row[x - 1].b) + P::ch(row[x].b) * 2 +
                         P::ch(row[x + 1].b);
            out[x] = P::make(rsum >> SHIFT, gsum >> SHIFT, bsum >> SHIFT, alpha);
        }
        // Last pixel
        if (w > 1) {
            int x = w - 1;
            acc_t r1 = P::ch(row[x].r) * 2 + P::ch(row[x - 1].r);
            acc_t g1 = P::ch(row[x].g) * 2 + P::ch(row[x - 1].g);
            acc_t b1 = P::ch(row[x].b) * 2 + P::ch(row[x - 1].b);
            out[x] = P::make(r1 >> SHIFT, g1 >> SHIFT, b1 >> SHIFT, alpha);
        }
    }
};

// Radius 2: kernel [1 4 6 4 1], shift=4
template <typename acc_t, typename RGB_T>
struct HPass<2, acc_t, RGB_T> {
    template <typename AlphaT>
    static void run(const RGB_T *row, RGB_T *out, int w, AlphaT alpha) {
        using P = pixel_ops<RGB_T>;
        enum { SHIFT = 4, R = 2 };
        static const int wts[] = {1, 4, 6, 4, 1};
        // Edge pixels (first 2)
        for (int x = 0; x < w && x < R; ++x) {
            out[x] = hPassGeneric<acc_t>(row, x, w, wts, R, SHIFT, alpha);
        }
        // Interior (no bounds check)
        for (int x = R; x + R < w; ++x) {
            acc_t rsum = P::ch(row[x - 2].r) + P::ch(row[x - 1].r) * 4 +
                         P::ch(row[x].r) * 6 + P::ch(row[x + 1].r) * 4 +
                         P::ch(row[x + 2].r);
            acc_t gsum = P::ch(row[x - 2].g) + P::ch(row[x - 1].g) * 4 +
                         P::ch(row[x].g) * 6 + P::ch(row[x + 1].g) * 4 +
                         P::ch(row[x + 2].g);
            acc_t bsum = P::ch(row[x - 2].b) + P::ch(row[x - 1].b) * 4 +
                         P::ch(row[x].b) * 6 + P::ch(row[x + 1].b) * 4 +
                         P::ch(row[x + 2].b);
            out[x] = P::make(rsum >> SHIFT, gsum >> SHIFT, bsum >> SHIFT, alpha);
        }
        // Edge pixels (last 2)
        int start = w - R;
        if (start < R) start = R;
        for (int x = start; x < w; ++x) {
            out[x] = hPassGeneric<acc_t>(row, x, w, wts, R, SHIFT, alpha);
        }
    }
};

// Radius 3: kernel [1 6 15 20 15 6 1], shift=6
template <typename acc_t, typename RGB_T>
struct HPass<3, acc_t, RGB_T> {
    template <typename AlphaT>
    static void run(const RGB_T *row, RGB_T *out, int w, AlphaT alpha) {
        using P = pixel_ops<RGB_T>;
        enum { SHIFT = 6, R = 3 };
        static const int wts[] = {1, 6, 15, 20, 15, 6, 1};
        for (int x = 0; x < w && x < R; ++x) {
            out[x] = hPassGeneric<acc_t>(row, x, w, wts, R, SHIFT, alpha);
        }
        for (int x = R; x + R < w; ++x) {
            acc_t rsum = P::ch(row[x - 3].r) + P::ch(row[x - 2].r) * 6 +
                         P::ch(row[x - 1].r) * 15 + P::ch(row[x].r) * 20 +
                         P::ch(row[x + 1].r) * 15 + P::ch(row[x + 2].r) * 6 +
                         P::ch(row[x + 3].r);
            acc_t gsum = P::ch(row[x - 3].g) + P::ch(row[x - 2].g) * 6 +
                         P::ch(row[x - 1].g) * 15 + P::ch(row[x].g) * 20 +
                         P::ch(row[x + 1].g) * 15 + P::ch(row[x + 2].g) * 6 +
                         P::ch(row[x + 3].g);
            acc_t bsum = P::ch(row[x - 3].b) + P::ch(row[x - 2].b) * 6 +
                         P::ch(row[x - 1].b) * 15 + P::ch(row[x].b) * 20 +
                         P::ch(row[x + 1].b) * 15 + P::ch(row[x + 2].b) * 6 +
                         P::ch(row[x + 3].b);
            out[x] = P::make(rsum >> SHIFT, gsum >> SHIFT, bsum >> SHIFT, alpha);
        }
        int start = w - R;
        if (start < R) start = R;
        for (int x = start; x < w; ++x) {
            out[x] = hPassGeneric<acc_t>(row, x, w, wts, R, SHIFT, alpha);
        }
    }
};

// Radius 4: kernel [1 8 28 56 70 56 28 8 1], shift=8
template <typename acc_t, typename RGB_T>
struct HPass<4, acc_t, RGB_T> {
    template <typename AlphaT>
    static void run(const RGB_T *row, RGB_T *out, int w, AlphaT alpha) {
        using P = pixel_ops<RGB_T>;
        enum { SHIFT = 8, R = 4 };
        static const int wts[] = {1, 8, 28, 56, 70, 56, 28, 8, 1};
        for (int x = 0; x < w && x < R; ++x) {
            out[x] = hPassGeneric<acc_t>(row, x, w, wts, R, SHIFT, alpha);
        }
        for (int x = R; x + R < w; ++x) {
            acc_t rsum = P::ch(row[x - 4].r) + P::ch(row[x - 3].r) * 8 +
                         P::ch(row[x - 2].r) * 28 + P::ch(row[x - 1].r) * 56 +
                         P::ch(row[x].r) * 70 + P::ch(row[x + 1].r) * 56 +
                         P::ch(row[x + 2].r) * 28 + P::ch(row[x + 3].r) * 8 +
                         P::ch(row[x + 4].r);
            acc_t gsum = P::ch(row[x - 4].g) + P::ch(row[x - 3].g) * 8 +
                         P::ch(row[x - 2].g) * 28 + P::ch(row[x - 1].g) * 56 +
                         P::ch(row[x].g) * 70 + P::ch(row[x + 1].g) * 56 +
                         P::ch(row[x + 2].g) * 28 + P::ch(row[x + 3].g) * 8 +
                         P::ch(row[x + 4].g);
            acc_t bsum = P::ch(row[x - 4].b) + P::ch(row[x - 3].b) * 8 +
                         P::ch(row[x - 2].b) * 28 + P::ch(row[x - 1].b) * 56 +
                         P::ch(row[x].b) * 70 + P::ch(row[x + 1].b) * 56 +
                         P::ch(row[x + 2].b) * 28 + P::ch(row[x + 3].b) * 8 +
                         P::ch(row[x + 4].b);
            out[x] = P::make(rsum >> SHIFT, gsum >> SHIFT, bsum >> SHIFT, alpha);
        }
        int start = w - R;
        if (start < R) start = R;
        for (int x = start; x < w; ++x) {
            out[x] = hPassGeneric<acc_t>(row, x, w, wts, R, SHIFT, alpha);
        }
    }
};

// ── Vertical pass dispatch ───────────────────────────────────────────────

// Primary template — radius 0 = copy with alpha.
template <int Radius, typename acc_t, typename RGB_T>
struct VPass {
    template <typename AlphaT>
    static void run(const RGB_T *colBase, RGB_T *out, int h, int stride,
                    AlphaT alpha) {
        using P = pixel_ops<RGB_T>;
        for (int y = 0; y < h; ++y) {
            const RGB_T &p = colBase[y * stride];
            out[y] = P::make(P::ch(p.r), P::ch(p.g), P::ch(p.b), alpha);
        }
    }
};

template <typename acc_t, typename RGB_T>
struct VPass<1, acc_t, RGB_T> {
    template <typename AlphaT>
    static void run(const RGB_T *colBase, RGB_T *out, int h, int w,
                    AlphaT alpha) {
        using P = pixel_ops<RGB_T>;
        enum { SHIFT = 2 };
        {
            acc_t r0 = P::ch(colBase[0].r) * 2 +
                       ((h > 1) ? P::ch(colBase[w].r) : P::ch(colBase[0].r));
            acc_t g0 = P::ch(colBase[0].g) * 2 +
                       ((h > 1) ? P::ch(colBase[w].g) : P::ch(colBase[0].g));
            acc_t b0 = P::ch(colBase[0].b) * 2 +
                       ((h > 1) ? P::ch(colBase[w].b) : P::ch(colBase[0].b));
            out[0] = P::make(r0 >> SHIFT, g0 >> SHIFT, b0 >> SHIFT, alpha);
        }
        for (int y = 1; y + 1 < h; ++y) {
            acc_t rsum = P::ch(colBase[(y - 1) * w].r) +
                         P::ch(colBase[y * w].r) * 2 +
                         P::ch(colBase[(y + 1) * w].r);
            acc_t gsum = P::ch(colBase[(y - 1) * w].g) +
                         P::ch(colBase[y * w].g) * 2 +
                         P::ch(colBase[(y + 1) * w].g);
            acc_t bsum = P::ch(colBase[(y - 1) * w].b) +
                         P::ch(colBase[y * w].b) * 2 +
                         P::ch(colBase[(y + 1) * w].b);
            out[y] = P::make(rsum >> SHIFT, gsum >> SHIFT, bsum >> SHIFT, alpha);
        }
        if (h > 1) {
            int y = h - 1;
            acc_t r1 = P::ch(colBase[y * w].r) * 2 +
                       P::ch(colBase[(y - 1) * w].r);
            acc_t g1 = P::ch(colBase[y * w].g) * 2 +
                       P::ch(colBase[(y - 1) * w].g);
            acc_t b1 = P::ch(colBase[y * w].b) * 2 +
                       P::ch(colBase[(y - 1) * w].b);
            out[y] = P::make(r1 >> SHIFT, g1 >> SHIFT, b1 >> SHIFT, alpha);
        }
    }
};

template <typename acc_t, typename RGB_T>
struct VPass<2, acc_t, RGB_T> {
    template <typename AlphaT>
    static void run(const RGB_T *colBase, RGB_T *out, int h, int w,
                    AlphaT alpha) {
        using P = pixel_ops<RGB_T>;
        enum { SHIFT = 4, R = 2 };
        static const int wts[] = {1, 4, 6, 4, 1};
        for (int y = 0; y < h && y < R; ++y) {
            out[y] = vPassGeneric<acc_t>(colBase, y, h, w, wts, R, SHIFT, alpha);
        }
        for (int y = R; y + R < h; ++y) {
            acc_t rsum = P::ch(colBase[(y - 2) * w].r) +
                         P::ch(colBase[(y - 1) * w].r) * 4 +
                         P::ch(colBase[y * w].r) * 6 +
                         P::ch(colBase[(y + 1) * w].r) * 4 +
                         P::ch(colBase[(y + 2) * w].r);
            acc_t gsum = P::ch(colBase[(y - 2) * w].g) +
                         P::ch(colBase[(y - 1) * w].g) * 4 +
                         P::ch(colBase[y * w].g) * 6 +
                         P::ch(colBase[(y + 1) * w].g) * 4 +
                         P::ch(colBase[(y + 2) * w].g);
            acc_t bsum = P::ch(colBase[(y - 2) * w].b) +
                         P::ch(colBase[(y - 1) * w].b) * 4 +
                         P::ch(colBase[y * w].b) * 6 +
                         P::ch(colBase[(y + 1) * w].b) * 4 +
                         P::ch(colBase[(y + 2) * w].b);
            out[y] = P::make(rsum >> SHIFT, gsum >> SHIFT, bsum >> SHIFT, alpha);
        }
        int start = h - R;
        if (start < R) start = R;
        for (int y = start; y < h; ++y) {
            out[y] = vPassGeneric<acc_t>(colBase, y, h, w, wts, R, SHIFT, alpha);
        }
    }
};

template <typename acc_t, typename RGB_T>
struct VPass<3, acc_t, RGB_T> {
    template <typename AlphaT>
    static void run(const RGB_T *colBase, RGB_T *out, int h, int w,
                    AlphaT alpha) {
        using P = pixel_ops<RGB_T>;
        enum { SHIFT = 6, R = 3 };
        static const int wts[] = {1, 6, 15, 20, 15, 6, 1};
        for (int y = 0; y < h && y < R; ++y) {
            out[y] = vPassGeneric<acc_t>(colBase, y, h, w, wts, R, SHIFT, alpha);
        }
        for (int y = R; y + R < h; ++y) {
            acc_t rsum = P::ch(colBase[(y - 3) * w].r) +
                         P::ch(colBase[(y - 2) * w].r) * 6 +
                         P::ch(colBase[(y - 1) * w].r) * 15 +
                         P::ch(colBase[y * w].r) * 20 +
                         P::ch(colBase[(y + 1) * w].r) * 15 +
                         P::ch(colBase[(y + 2) * w].r) * 6 +
                         P::ch(colBase[(y + 3) * w].r);
            acc_t gsum = P::ch(colBase[(y - 3) * w].g) +
                         P::ch(colBase[(y - 2) * w].g) * 6 +
                         P::ch(colBase[(y - 1) * w].g) * 15 +
                         P::ch(colBase[y * w].g) * 20 +
                         P::ch(colBase[(y + 1) * w].g) * 15 +
                         P::ch(colBase[(y + 2) * w].g) * 6 +
                         P::ch(colBase[(y + 3) * w].g);
            acc_t bsum = P::ch(colBase[(y - 3) * w].b) +
                         P::ch(colBase[(y - 2) * w].b) * 6 +
                         P::ch(colBase[(y - 1) * w].b) * 15 +
                         P::ch(colBase[y * w].b) * 20 +
                         P::ch(colBase[(y + 1) * w].b) * 15 +
                         P::ch(colBase[(y + 2) * w].b) * 6 +
                         P::ch(colBase[(y + 3) * w].b);
            out[y] = P::make(rsum >> SHIFT, gsum >> SHIFT, bsum >> SHIFT, alpha);
        }
        int start = h - R;
        if (start < R) start = R;
        for (int y = start; y < h; ++y) {
            out[y] = vPassGeneric<acc_t>(colBase, y, h, w, wts, R, SHIFT, alpha);
        }
    }
};

template <typename acc_t, typename RGB_T>
struct VPass<4, acc_t, RGB_T> {
    template <typename AlphaT>
    static void run(const RGB_T *colBase, RGB_T *out, int h, int w,
                    AlphaT alpha) {
        using P = pixel_ops<RGB_T>;
        enum { SHIFT = 8, R = 4 };
        static const int wts[] = {1, 8, 28, 56, 70, 56, 28, 8, 1};
        for (int y = 0; y < h && y < R; ++y) {
            out[y] = vPassGeneric<acc_t>(colBase, y, h, w, wts, R, SHIFT, alpha);
        }
        for (int y = R; y + R < h; ++y) {
            acc_t rsum = P::ch(colBase[(y - 4) * w].r) +
                         P::ch(colBase[(y - 3) * w].r) * 8 +
                         P::ch(colBase[(y - 2) * w].r) * 28 +
                         P::ch(colBase[(y - 1) * w].r) * 56 +
                         P::ch(colBase[y * w].r) * 70 +
                         P::ch(colBase[(y + 1) * w].r) * 56 +
                         P::ch(colBase[(y + 2) * w].r) * 28 +
                         P::ch(colBase[(y + 3) * w].r) * 8 +
                         P::ch(colBase[(y + 4) * w].r);
            acc_t gsum = P::ch(colBase[(y - 4) * w].g) +
                         P::ch(colBase[(y - 3) * w].g) * 8 +
                         P::ch(colBase[(y - 2) * w].g) * 28 +
                         P::ch(colBase[(y - 1) * w].g) * 56 +
                         P::ch(colBase[y * w].g) * 70 +
                         P::ch(colBase[(y + 1) * w].g) * 56 +
                         P::ch(colBase[(y + 2) * w].g) * 28 +
                         P::ch(colBase[(y + 3) * w].g) * 8 +
                         P::ch(colBase[(y + 4) * w].g);
            acc_t bsum = P::ch(colBase[(y - 4) * w].b) +
                         P::ch(colBase[(y - 3) * w].b) * 8 +
                         P::ch(colBase[(y - 2) * w].b) * 28 +
                         P::ch(colBase[(y - 1) * w].b) * 56 +
                         P::ch(colBase[y * w].b) * 70 +
                         P::ch(colBase[(y + 1) * w].b) * 56 +
                         P::ch(colBase[(y + 2) * w].b) * 28 +
                         P::ch(colBase[(y + 3) * w].b) * 8 +
                         P::ch(colBase[(y + 4) * w].b);
            out[y] = P::make(rsum >> SHIFT, gsum >> SHIFT, bsum >> SHIFT, alpha);
        }
        int start = h - R;
        if (start < R) start = R;
        for (int y = start; y < h; ++y) {
            out[y] = vPassGeneric<acc_t>(colBase, y, h, w, wts, R, SHIFT, alpha);
        }
    }
};

} // namespace blur_detail

// Internal blur implementation with alpha integrated into element functions.
// Alpha is applied in exactly one pass (H or V) to avoid double-dimming:
//   - If hRadius > 0: alpha applied in horizontal pass, vertical gets identity.
//   - If hRadius == 0: alpha applied in vertical pass.
template <int hRadius, int vRadius, typename RGB_T, typename AlphaT>
void blurGaussianImpl(Canvas<RGB_T> &canvas, AlphaT alpha) {
    const int w = canvas.width;
    const int h = canvas.height;
    if (w <= 0 || h <= 0) {
        return;
    }

    // Use u32 accumulators for wide channels (CRGB16) to avoid overflow.
    using acc_t = fl::conditional_t<
        (sizeof(typename RGB_T::fp) == 1) && (hRadius < 5 && vRadius < 5),
        u16, u32>;

    // Thread-local temp buffer sized to the larger dimension.
    const int maxDim = (w > h) ? w : h;
    RGB_T *tempRow = blur_detail::get_temp_buffer<RGB_T>(maxDim);

    RGB_T *pixels = canvas.pixels;

    // Determine which pass applies alpha (avoid double-dimming).
    // hRadius > 0 is compile-time constant — dead branch is optimized out.
    const AlphaT hAlpha = (hRadius > 0)
        ? alpha
        : blur_detail::alpha_identity<AlphaT>();
    const AlphaT vAlpha = (hRadius > 0)
        ? blur_detail::alpha_identity<AlphaT>()
        : alpha;

    // ── Horizontal pass ──────────────────────────────────────────────────

    for (int y = 0; y < h; ++y) {
        RGB_T *row = pixels + y * w;

        blur_detail::HPass<hRadius, acc_t, RGB_T>::run(
            row, tempRow, w, hAlpha);

        // Write back.
        for (int x = 0; x < w; ++x) {
            row[x] = tempRow[x];
        }
    }

    // ── Vertical pass ────────────────────────────────────────────────────

    for (int x = 0; x < w; ++x) {
        RGB_T *colBase = pixels + x;

        blur_detail::VPass<vRadius, acc_t, RGB_T>::run(
            colBase, tempRow, h, w, vAlpha);

        // Write back.
        for (int y = 0; y < h; ++y) {
            pixels[y * w + x] = tempRow[y];
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

// CRGB16 — same combos.
BLUR_INST_F8(0, 0, CRGB16)  BLUR_INST_F8(1, 1, CRGB16)
BLUR_INST_F8(2, 2, CRGB16)  BLUR_INST_F8(3, 3, CRGB16)  BLUR_INST_F8(4, 4, CRGB16)
BLUR_INST_F8(1, 0, CRGB16)  BLUR_INST_F8(2, 0, CRGB16)
BLUR_INST_F8(3, 0, CRGB16)  BLUR_INST_F8(4, 0, CRGB16)
BLUR_INST_F8(0, 1, CRGB16)  BLUR_INST_F8(0, 2, CRGB16)
BLUR_INST_F8(0, 3, CRGB16)  BLUR_INST_F8(0, 4, CRGB16)
BLUR_INST_F8(1, 2, CRGB16)  BLUR_INST_F8(2, 1, CRGB16)

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
BLUR_INST_F16(0, 0, CRGB16)  BLUR_INST_F16(1, 1, CRGB16)
BLUR_INST_F16(2, 2, CRGB16)  BLUR_INST_F16(3, 3, CRGB16)  BLUR_INST_F16(4, 4, CRGB16)
BLUR_INST_F16(1, 0, CRGB16)  BLUR_INST_F16(2, 0, CRGB16)
BLUR_INST_F16(3, 0, CRGB16)  BLUR_INST_F16(4, 0, CRGB16)
BLUR_INST_F16(0, 1, CRGB16)  BLUR_INST_F16(0, 2, CRGB16)
BLUR_INST_F16(0, 3, CRGB16)  BLUR_INST_F16(0, 4, CRGB16)
BLUR_INST_F16(1, 2, CRGB16)  BLUR_INST_F16(2, 1, CRGB16)

#undef BLUR_INST_F16

} // namespace gfx
} // namespace fl
