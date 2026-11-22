#pragma once

/// @file tests/unit/fl/gfx/poly_tinygl_ref.h
/// @brief Test/benchmark-only faithful C++ port of aykevl/tinygl's polygon fill
///
/// Faithful port of the A-buffer scanline polygon fill from aykevl/tinygl
/// (https://github.com/aykevl/tinygl, MIT license, Copyright (c) 2023 Ayke
/// van Laethem; see https://aykevl.nl/2024/02/tinygl-polygon/). Source
/// files: gfx/polygon.go (render loop) and gfx/line.go (edge construction
/// and slope math).
///
/// This is a BENCHMARK REFERENCE, not a product backend: it is not part of
/// the public API (no PolyFill enumerator) and is kept in tests/ so it can
/// be retired without an API change. It is deliberately unoptimized beyond
/// the port itself — the point is to measure the reference algorithm's
/// structure (monotonic edge pointers, per-pixel sub-scanline recomputation,
/// per-pixel popcount) against fl::gfx::PolyFill::Exact and
/// fl::gfx::PolyFill::ABuffer.
///
/// # Go int32 semantics (emulated)
///
/// The Go reference does all scanline math in int32 with wrapping products
/// and truncating division (a latent overflow for steep/sub-pixel edges).
/// This port emulates that faithfully:
///  - wrapping i32 products go through unsigned math (no UB),
///  - division truncates toward zero (C++ i32 division, like Go),
///  - right shifts are arithmetic (gcc/clang, like Go on the targets
///    tinygl ships for).
///
/// # Documented deviations from the Go reference
///
/// 1. **xinc setup.** Go computes `int32((x2-x1) << 16 / (y2-y1))`, which
///    is only defined for whole-pixel dy (tinygl lines have int16
///    endpoints). Our vertices are fractional Q16.16, so the slope is
///    computed as i32-trunc(i64(dx16) * 65536 / dy16) — bit-identical to
///    the Go expression for whole-pixel vertices, defined for all
///    non-horizontal edges.
/// 2. **Edge order.** The reference line shapes use a fixed edge order
///    that is sorted left-to-right; the blog's general polygon requires a
///    static edge order, so this port sorts edges once at setup by
///    (xstart, ytop, xinc). Two edges that leave a shared top vertex (same
///    xstart and ytop) are therefore adjacent and ordered by xinc: the one
///    with the smaller xinc is the left one on every row below it, and
///    ordering them the other way round
///    (input order) left convex polygons half filled (diamond, wide
///    triangles; see the scene tests in poly_abuffer.hpp). The monotonic
///    edgeStart/edgeEnd pointer walk is kept verbatim, including its
///    limitation for self-intersecting polygons (the x-order flips at the
///    crossing and the far side mis-renders) and for concave outlines whose
///    edges' x-order on a row differs from the static order.
/// 3. **Left of the canvas.** The reference starts the x-walk at the
///    unclamped xleft of the first edge (possibly negative) and
///    bounds-checks every write (blendPixel/drawLine). This port does the
///    same: the walk starts at the unclamped xleft and writes are
///    clamped to the canvas — identical behavior, no out-of-bounds access.
/// 4. **Compositing.** The reference's naiveBlend is an "over" blend and
///    its full-span fast path overwrites even mid-composition (its canvas
///    is repainted from scratch every frame). Here both use FastLED's
///    model — OVERWRITE: assign; BLEND: additive — so all three polygon
///    backends compose the same way and quality comparisons measure the
///    sampling, not the compositing.
/// 5. **Pixel-column width.** Go's `int` is 64-bit, so `x = edgeEndLeft`
///    carries the `0x7fffffff` "no more edges" sentinel intact and the
///    `x < width` walk ends the row. C++ `int` is only 16-bit on AVR, so
///    truncating that sentinel into an `int` column gives -1 and restarts
///    the walk at x = -1 forever (reproduced in simavr with avr-gcc 7.3
///    and clang 21: the bowtie and diamond scenes never finish a row).
///    The x-walk is therefore explicitly 16-bit on every target (cheap
///    on AVR): edge columns are saturated to `fl::i16` and the sentinel
///    is `0x7fff`. This is exact, not an approximation. Vertices are
///    clamped to ±8191 px, so every non-wrapped edge column lies in
///    ±8192, and saturation only touches the already-garbage results of
///    the Go int32 wrap. Canvases must be narrower than 0x7fff px. The
///    division, loops, and edge pointers are the plain Go forms.
///
/// # Sampling geometry (kept faithful)
///
/// Sub-scanline s = 4*y + r is in range for an edge when 0 <= s - ytop <
/// ybottom - ytop (half-open .2 bands). The edge crossing is
/// `xstart + (s - ytop) * xinc / 4` (wrapping i32 product, truncating
/// division by 4). A sub-column c of pixel x is covered when
/// `bitOffsetX = (xoffset - x*2^16) >> 13` falls in [0, 8): sub-columns
/// bitOffsetX..7 (right edge of the sub-column strictly right of the
/// crossing). Mask rows extend with `(mask & 0x01010101) * 255`. Alpha is
/// `popcount(mask) * 8` (max 248; the all-32 case is the full-span fast
/// path).

#include "fl/stl/noexcept.h"
#include "fl/stl/int.h"
#include "fl/math/fixed_point/s16x16.h"
#include "fl/gfx/canvas.h"
#include "fl/gfx/poly_abuffer.h"  // polyABToQ16 (vertex -> Q16.16, clamped)

namespace fl {
namespace gfx {
namespace detail {

/// Wrapping int32 multiply (Go's int32 * int32), without UB.
inline fl::i32 polyTGWrapMul(fl::i32 a, fl::i32 b) FL_NO_EXCEPT {
    return static_cast<fl::i32>(static_cast<fl::u32>(a) * static_cast<fl::u32>(b));
}

/// Wrapping int32 add (Go's int32 + int32), without UB.
inline fl::i32 polyTGWrapAdd(fl::i32 a, fl::i32 b) FL_NO_EXCEPT {
    return static_cast<fl::i32>(static_cast<fl::u32>(a) + static_cast<fl::u32>(b));
}

/// "No more edges" sentinel for the 16-bit x-walk (deviation 5).
constexpr fl::i16 kPolyTGNoEdge = 0x7fff;

/// Saturate a pixel column (Q0) to i16 (deviation 5).
inline fl::i16 polyTGSat16(fl::i32 v) FL_NO_EXCEPT {
    if (v > 0x7fff) return 0x7fff;
    if (v < -0x7fff) return -0x7fff;
    return static_cast<fl::i16>(v);
}

/// Go's int32 `v / 4`: signed division truncating toward zero, written as
/// shifts. Bit-identical to `p / 4` for every int32. The shift form is a
/// speed choice, not a correctness fix: at -Os avr-gcc lowers a signed
/// 32-bit `p / 4` to a `__divmodsi4` library call (~3x slower kernel on an
/// ATmega2560) rather than a biased shift.
inline fl::i32 polyTGDiv4(fl::i32 p) FL_NO_EXCEPT {
    return (p >= 0) ? (p >> 2) : ((p + 3) >> 2);
}

/// One reference edge (trapezoid), verbatim from polygon.go.
struct TgEdge {
    fl::i32 ytop;     // .2 (4 sub-scanlines per pixel), half-open
    fl::i32 ybottom;  // .2
    fl::i32 xstart;   // .16, X at ytop
    fl::i32 xinc;     // .16 per pixel
};

/// polygon.go: (e polygonEdge) inrange — Go int32 division (truncating).
inline bool polyTGInrange(const TgEdge& e, int y) FL_NO_EXCEPT {
    return y >= polyTGDiv4(e.ytop) && y < polyTGDiv4(e.ybottom + 3);
}

/// polygon.go: (e polygonEdge) xleft — Go precedence: the product/division
/// first, then the add of xstart. Wrapping product, truncating /4,
/// arithmetic >>16 (all Go semantics), saturated to i16 (deviation 5).
inline fl::i16 polyTGXleft(const TgEdge& e, int y) FL_NO_EXCEPT {
    const fl::i32 edgeY = (fl::i32)y * 4 - e.ytop;
    const fl::i32 dy = (e.xinc > 0) ? edgeY : edgeY + 3;
    return polyTGSat16(polyTGWrapAdd(e.xstart, polyTGDiv4(polyTGWrapMul(dy, e.xinc))) >> 16);
}

/// polygon.go: (e polygonEdge) xright.
inline fl::i16 polyTGXright(const TgEdge& e, int y) FL_NO_EXCEPT {
    const fl::i32 edgeY = (fl::i32)y * 4 - e.ytop;
    const fl::i32 dy = (e.xinc > 0) ? edgeY + 3 : edgeY;
    return polyTGSat16(polyTGWrapAdd(e.xstart, polyTGDiv4(polyTGWrapMul(dy, e.xinc))) >> 16);
}

/// Popcount of a u32 (Go's bits.OnesCount32; gcc/clang builtin on host and
/// AVR). Must use popcountl: on AVR `unsigned int` is 16-bit, so
/// __builtin_popcount would truncate v to its low 16 bits before counting.
inline fl::u8 polyTGPopcount32(fl::u32 v) FL_NO_EXCEPT {
#if defined(__GNUC__) || defined(__clang__)
    return (fl::u8)__builtin_popcountl(v);
#else
    v = v - ((v >> 1) & 0x55555555u);
    v = (v & 0x33333333u) + ((v >> 2) & 0x33333333u);
    return (fl::u8)((v + (v >> 4) & 0x0F0F0F0Fu) * 0x01010101u >> 24);
#endif
}

/// Static edge order for the port (deviation 2): lexicographic
/// (xstart, ytop, xinc), a strict total order on distinct edge keys.
inline bool polyTGEdgeLess(const TgEdge& a, const TgEdge& b) FL_NO_EXCEPT {
    if (a.xstart != b.xstart) return a.xstart < b.xstart;
    if (a.ytop != b.ytop) return a.ytop < b.ytop;
    return a.xinc < b.xinc;
}

/// Faithful port of polygon.go drawPolygon (see file header).
/// MAXN caps the vertex count like the other backends.
template<typename PixelT, typename Coord, bool Overwrite, int MAXN>
inline void drawPolygonTinyGLRef(Canvas<PixelT>& canvas, const PixelT& color,
                                 fl::span<const PolyPoint<Coord>> pts) FL_NO_EXCEPT {
    const int n = static_cast<int>(pts.size());
    if (n < 3) return;
    PixelT* pixels = canvas.pixels;
    const int width = canvas.width;
    const int height = canvas.height;
    if (width <= 0 || height <= 0) return;

    const int nv = (n < MAXN) ? n : MAXN;

    // Vertices to Q16.16 (clamped to ±8191 px, same domain as the other
    // backends).
    fl::i32 vx[MAXN];
    fl::i32 vy[MAXN];
    for (int i = 0; i < nv; ++i) {
        vx[i] = polyABToQ16(pts[i].x);
        vy[i] = polyABToQ16(pts[i].y);
    }

    // Edge setup: orient top-to-bottom (line.go), skip horizontal edges,
    // xinc = i32-trunc(dx16 * 65536 / dy16) (deviation 1).
    TgEdge edges[MAXN];
    int ne = 0;
    for (int i = 0; i < nv; ++i) {
        const int j = (i + 1 == nv) ? 0 : i + 1;
        fl::i32 y1 = vy[i], y2 = vy[j];
        fl::i32 x1 = vx[i], x2 = vx[j];
        if (y1 > y2) {
            const fl::i32 ty = y1; y1 = y2; y2 = ty;
            const fl::i32 tx = x1; x1 = x2; x2 = tx;
        }
        if (y1 == y2) continue;
        TgEdge& e = edges[ne++];
        e.ytop = y1 >> 14;      // .2 (arithmetic shift, Go semantics)
        e.ybottom = y2 >> 14;
        e.xstart = x1;
        // C++ i64 division truncates toward zero, like Go.
        const fl::i64 num = (fl::i64)(x2 - x1) * 65536L;
        const fl::i64 den = (fl::i64)(y2 - y1);
        e.xinc = static_cast<fl::i32>(num / den);
    }
    if (ne == 0) return;

    // Static edge order (deviation 2): sort by the total order (xstart,
    // ytop, xinc). Edges leaving a shared top vertex become adjacent and
    // the one heading further left goes first, even when other edges with
    // the same xstart but a different ytop are interleaved in input order.
    for (int a = 1; a < ne; ++a) {
        const TgEdge v = edges[a];
        int b = a - 1;
        while (b >= 0 && polyTGEdgeLess(v, edges[b])) {
            edges[b + 1] = edges[b];
            --b;
        }
        edges[b + 1] = v;
    }

    // Vertical bounds (polygon.go updateBounds, Go truncating division).
    int ymin = height, ymax = -1;
    for (int i = 0; i < ne; ++i) {
        const int yt = polyTGDiv4(edges[i].ytop);
        const int yb = polyTGDiv4(edges[i].ybottom + 3);
        if (yt < ymin) ymin = yt;
        if (yb > ymax) ymax = yb;
    }
    if (ymin < 0) ymin = 0;
    if (ymax > height) ymax = height;
    if (ymin >= ymax) return;

    // The scanline walk (polygon.go drawPolygon), verbatim structure.
    for (int y = ymin; y < ymax; ++y) {
        int edgeStart = 0;
        while (edgeStart < ne && !polyTGInrange(edges[edgeStart], y)) ++edgeStart;
        if (edgeStart >= ne) continue;

        fl::i16 edgeStartRight = polyTGXright(edges[edgeStart], y);
        int edgeEnd = edgeStart;
        fl::i16 edgeEndLeft = polyTGXleft(edges[edgeEnd], y);

        fl::u32 mask = 0;
        // The x-walk starts at the UNCLAMPED xleft (possibly negative;
        // deviation 3 keeps the walk faithful, writes are clamped). x is
        // i16 on every target (deviation 5).
        fl::i16 x = polyTGXleft(edges[edgeStart], y);
        while (x < width) {
            while (edgeStartRight < x) {
                ++edgeStart;
                while (edgeStart < ne && !polyTGInrange(edges[edgeStart], y)) ++edgeStart;
                edgeStartRight = (edgeStart < ne) ? polyTGXright(edges[edgeStart], y)
                                                  : kPolyTGNoEdge;
            }
            if (edgeStart >= ne) break;
            while (edgeEndLeft <= x) {
                ++edgeEnd;
                while (edgeEnd < ne && !polyTGInrange(edges[edgeEnd], y)) ++edgeEnd;
                edgeEndLeft = (edgeEnd < ne) ? polyTGXleft(edges[edgeEnd], y)
                                             : kPolyTGNoEdge;
            }

            for (int i = edgeStart; i < edgeEnd; ++i) {
                const TgEdge& ed = edges[i];
                if (!polyTGInrange(ed, y)) continue;
                for (int subline = 0; subline < 4; ++subline) {
                    const fl::i32 sublineY = y * 4 + subline - ed.ytop;  // .2
                    if (static_cast<fl::u32>(sublineY) >=
                        static_cast<fl::u32>(ed.ybottom - ed.ytop)) continue;
                    const fl::i32 xoffset = polyTGWrapAdd(
                        ed.xstart, polyTGDiv4(polyTGWrapMul(sublineY, ed.xinc)));
                    const fl::i32 bitOffsetX =
                        polyTGWrapAdd(xoffset, polyTGWrapMul(-(fl::i32)x, 65536L)) >> 13;
                    if (static_cast<fl::u32>(bitOffsetX) >= 8u) continue;
                    const fl::u32 bits = (1UL << (8u - static_cast<fl::u32>(bitOffsetX))) - 1UL;
                    mask ^= bits << (subline * 8);
                }
            }

            if (mask != 0) {
                if (mask == 0xffffffffUL) {
                    // Full-span fast path (polygon.go drawLine): the whole
                    // run up to the next edge's xleft is solid color, then
                    // jump there. Past the last edge edgeEndLeft is the
                    // kPolyTGNoEdge sentinel, which ends the row via x < width.
                    const int runX0 = (x < 0) ? 0 : x;
                    const int runX1 = (edgeEndLeft < width) ? edgeEndLeft : width;
                    if (runX1 > runX0) {
                        PixelT* row = pixels + y * width + runX0;
                        const int count = runX1 - runX0;
                        if (Overwrite) {
                            for (int i = 0; i < count; ++i) row[i] = color;
                        } else {
                            for (int i = 0; i < count; ++i) row[i] += color;
                        }
                    }
                    x = edgeEndLeft;
                    continue;
                }
                // alpha = popcount(mask) << 3 (0..248; all-32 is the fast
                // path above, so the u8 cast never wraps).
                const fl::u8 alpha = (fl::u8)(polyTGPopcount32(mask) * 8u);
                PixelT c = color;
                if (alpha < 255u) c.nscale8(alpha);
                // Go's blendPixel bounds-checks every write; y is in
                // range (the y-walk is clamped to the canvas) but x may
                // be negative (deviation 3).
                if (x >= 0) {
                    PixelT& px = pixels[y * width + x];
                    if (Overwrite) px = c;
                    else px += c;
                }
                // Extend rightmost subpixels to full sub-scanlines.
                mask = (mask & 0x01010101UL) * 255UL;
            }
            ++x;
        }
    }
}

}  // namespace detail
}  // namespace gfx
}  // namespace fl
