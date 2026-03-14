

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

// ── SKIPSM Gaussian blur — FSM-based single-pass implementation ──────────
//
// Based on: "An efficient algorithm for Gaussian blur using finite-state
// machines" (Waltz & Miller, SPIE 1998).
//
// Processes the image in a single raster scan using cascaded [1 1] additions
// that naturally build Pascal's triangle (binomial) coefficients:
//   [1] -> [1 1] -> [1 2 1] -> [1 3 3 1] -> [1 4 6 4 1] -> ...
//
// For radius R: 2R row state registers + 2R column state buffers.
// Only additions, no multiplications. Each pixel fetched exactly once.
// Normalization by right-shift of 4R bits (sum of weights = 2^(4R)).
//
// Virtual zero-padded pixels flush the pipeline to cover all output positions.

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

// Thread-local column state buffer for the FSM.
template <typename acc_t>
static acc_t *get_colbuf(int minSize) {
    static fl::ThreadLocal<fl::vector<acc_t>> tl_colbuf;
    fl::vector<acc_t> &buf = tl_colbuf.access();
    if (static_cast<int>(buf.size()) < minSize) {
        buf.resize(minSize);
    }
    return buf.data();
}

} // namespace blur_detail

// FSM-based Gaussian blur: single-pass raster scan with cascaded [1 1]
// state machines.  Alpha is applied once at the output.
template <int hRadius, int vRadius, typename RGB_T, typename AlphaT>
void blurGaussianImpl(Canvas<RGB_T> &canvas, AlphaT alpha) {
    const int w = canvas.width;
    const int h = canvas.height;
    if (w <= 0 || h <= 0)
        return;

    using P = blur_detail::pixel_ops<RGB_T>;

    // Handle no-blur case (radius 0 in both dimensions).
    if (hRadius == 0 && vRadius == 0) {
        if (!(alpha == blur_detail::alpha_identity<AlphaT>())) {
            RGB_T *pixels = canvas.pixels;
            for (int i = 0; i < w * h; ++i) {
                RGB_T &p = pixels[i];
                p = P::make(P::ch(p.r), P::ch(p.g), P::ch(p.b), alpha);
            }
        }
        return;
    }

    // State counts per dimension.
    enum {
        HR = 2 * hRadius, // row state register count
        VR = 2 * vRadius, // column state buffer count
        SHIFT = HR + VR   // total normalization shift
    };

    // Accumulator type: u16 when max intermediate (255 * 2^SHIFT) fits,
    // u32 otherwise. For 8-bit pixels, u16 works when SHIFT <= 8.
    using acc_t = fl::conditional_t<
        (sizeof(typename RGB_T::fp) == 1) && (SHIFT <= 8), u16, u32>;

    const acc_t ROUND =
        (SHIFT > 0) ? (acc_t(1) << (SHIFT - 1)) : acc_t(0);

    // Virtual pixels for pipeline flush.
    const int totalCols = w + hRadius;
    const int totalRows = h + vRadius;

    // Column state buffers: VR stages x totalCols, per channel.
    const int colBufStride = totalCols;
    const int colBufElems = (VR > 0 ? VR : 1) * colBufStride;
    const int totalBufSize = colBufElems * 3; // R, G, B

    acc_t *buf = blur_detail::get_colbuf<acc_t>(totalBufSize);
    for (int i = 0; i < totalBufSize; ++i)
        buf[i] = 0;

    acc_t *const sc_r = buf;
    acc_t *const sc_g = buf + colBufElems;
    acc_t *const sc_b = buf + colBufElems * 2;

    RGB_T *pixels = canvas.pixels;

    for (int j = 0; j < totalRows; ++j) {
        // Row state registers — reset at each row start.
        acc_t sr_r[HR > 0 ? HR : 1];
        acc_t sr_g[HR > 0 ? HR : 1];
        acc_t sr_b[HR > 0 ? HR : 1];
        for (int s = 0; s < (HR > 0 ? HR : 1); ++s) {
            sr_r[s] = 0;
            sr_g[s] = 0;
            sr_b[s] = 0;
        }

        for (int i = 0; i < totalCols; ++i) {
            // Fetch input (zero for virtual padding pixels).
            acc_t in_r, in_g, in_b;
            if (j < h && i < w) {
                const RGB_T &p = pixels[j * w + i];
                in_r = P::ch(p.r);
                in_g = P::ch(p.g);
                in_b = P::ch(p.b);
            } else {
                in_r = 0;
                in_g = 0;
                in_b = 0;
            }

            // ── Row FSM: HR cascaded [1 1] addition stages ──────────
            acc_t tr = in_r, tg = in_g, tb = in_b;
            for (int s = 0; s < HR; ++s) {
                acc_t nr = sr_r[s] + tr;
                acc_t ng = sr_g[s] + tg;
                acc_t nb = sr_b[s] + tb;
                sr_r[s] = tr;
                sr_g[s] = tg;
                sr_b[s] = tb;
                tr = nr;
                tg = ng;
                tb = nb;
            }

            // ── Column FSM: VR cascaded [1 1] addition stages ──────
            for (int s = 0; s < VR; ++s) {
                int idx = s * colBufStride + i;
                acc_t nr = sc_r[idx] + tr;
                acc_t ng = sc_g[idx] + tg;
                acc_t nb = sc_b[idx] + tb;
                sc_r[idx] = tr;
                sc_g[idx] = tg;
                sc_b[idx] = tb;
                tr = nr;
                tg = ng;
                tb = nb;
            }

            // ── Write output at (j - vRadius, i - hRadius) ─────────
            int oy = j - vRadius;
            int ox = i - hRadius;
            if (oy >= 0 && oy < h && ox >= 0 && ox < w) {
                pixels[oy * w + ox] =
                    P::make((ROUND + tr) >> SHIFT, (ROUND + tg) >> SHIFT,
                            (ROUND + tb) >> SHIFT, alpha);
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
