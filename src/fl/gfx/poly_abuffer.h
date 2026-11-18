#pragma once

/// @file fl/gfx/poly_abuffer.h
/// @brief A-buffer 8x4 antialiased polygon fill (fast, low-stack backend)
///
/// Alternate fill backend for drawPolygon (see fl::gfx::PolyFill::ABuffer).
///
/// # Algorithm
///
/// Based on the A-buffer scanline approach described at
/// https://aykevl.nl/2024/02/tinygl-polygon/ (TinyGL, aykevl), which in turn
/// follows the A-buffer algorithm (Grossetete, 1991). Each pixel carries a
/// 32-bit mask — an 8 x 4 grid of subpixels — and the alpha channel is the
/// popcount of the mask (32 distinct levels, br = min(255, popcount * 8)).
///
/// Every non-horizontal polygon edge is treated as a trapezoid: the edge
/// itself plus the horizontal region to its right. XOR-ing the trapezoid
/// masks of all edges reproduces the polygon under the even-odd rule (the
/// same rule the exact backend, drawPolygonCore, uses). Scanlines are
/// rendered left to right; when an edge crosses a pixel its per-sub-row
/// mask is XOR-ed into the running mask, and between pixels the mask is
/// "extended" with `mask = (mask & 0x01010101) * 255` — two operations that
/// turn a set rightmost subpixel of each row into a full row for the next
/// pixel (the trapezoid continues to the right).
///
/// # Integer model
///
/// Integer-only (repository convention): coordinates in Q16.16, sub-rows
/// in .2 (4 per pixel). A sub-row is active for an edge when its TOP
/// grid line lies within the edge's [ytop_q, ybot_q) span — the edge's
/// segment reaches the sub-row's top. On the grid, a vertex shared by
/// two edges belongs to exactly one of them; mid-sub-row, the vertex's
/// sub-row belongs to the edge that ends there (its partner starts at
/// the next sub-row), so no sub-row is claimed twice and the per-sub-row
/// even-odd crossing set stays consistent. The edge's crossing of a
/// sub-row is its line's x at the sub-row's top grid line, measured
/// exactly from the top vertex in Q16 (never snapped to the sub-row
/// grid), so a vertex mid-sub-row introduces no quantization shift. Per
/// edge, once at setup:
///   xinc = floor(65536 * B / dy)          (one i64 division, i64 result)
/// Per scanline, per active edge: x0 += xinc (i64 add, no libgcc).
/// Per sub-row: X_r = X_0 + r * q + corr[r] (shifts/adds + a 2-bit table,
/// no per-sub-row multiply). The per-pixel work is shifts, XORs and a
/// popcount — **no division anywhere in the scan loops**, which is what
/// keeps the kernel off the deep 64-bit libgcc chain the exact backend
/// needs for its per-pixel area integration.
///
/// # Deviations from the Go reference (documented)
///
/// 1. **No static edge ordering.** The reference advances edgeStart/
///    edgeEnd monotonically through a pre-sorted edge list, which requires
///    the edges' x-order to be constant across all scanlines. That holds
///    for the parallelogram (line) shapes TinyGL builds, but breaks for
///    self-intersecting polygons: once two edges' x-order flips (the
///    crossing point), the scan starts at the wrong edge and the part of
///    the polygon on the far side is never painted. This kernel instead
///    collects every crossing of every active edge up front (per scanline,
///    in O(nedges) work) and jumps directly between crossing pixels —
///    correct for arbitrary (even self-intersecting) polygons at
///    negligible cost for the small vertex counts this library targets.
/// 2. **i64 edge state.** The reference uses int32 for xinc/xstart and
///    int32-wrapping products in `sublineY * xinc / 4`; that wraps for
///    steep or long edges (even on small canvases with sub-pixel-dy
///    edges). This kernel uses i64 (exact across the ±8191 px domain).
/// 3. **Off-canvas-left edges.** The reference's x-loop starts at the
///    first edge's xleft and rebuilds mask state from there; polygons that
///    extend past the left canvas edge need that state reconstructed.
///    This kernel seeds the row mask with the parity of edges already left
///    of the canvas (X_r < 0) so fully clipped polygons fill correctly.
/// 4. **Floor (not truncate) division.** The reference's integer divisions
///    truncate toward zero (Go semantics); this kernel floors (repository
///    convention). At most a 1/8-px (one subpixel) AA difference.
/// 5. **Full pixels saturate at 255.** A mask with all 32 bits set yields
///    br = 255 (the reference's popcount << 3 would wrap to 0 there; its
///    full-span fast path is what actually keeps that case from shipping).
/// 6. **Off-grid vertices.** The reference's shapes (line parallelograms)
///    have every vertex on the sub-row grid by construction, so its
///    half-open [ytop, ybottom) edge ranges never claim one sub-row
///    twice. Arbitrary Q16 vertices can sit mid-sub-row, where the naive
///    floor/ceil ranges make both edges sharing the vertex claim its
///    sub-row — three crossings on one sub-row, and the even-odd rule
///    fills a spurious tail to the canvas edge. This kernel instead
///    activates a sub-row only when the edge's segment reaches its TOP
///    grid line (ceil/ceil ranges, see the integer model above), which
///    assigns the vertex's sub-row to exactly one of its edges.
///
/// Sampling geometry (kept faithful to the reference): sub-row r covers
/// the half-open band [y + r/4, y + (r+1)/4) and sub-column c counts as
/// covered when its RIGHT edge x + (c+1)/8 lies strictly to the right of
/// the edge crossing (the edge's line at the sub-row's top grid line,
/// see the integer model above; a sample exactly on the edge is outside).

#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/int.h"
#include "fl/math/fixed_point/s16x16.h"
#include "fl/gfx/canvas.h"
#include "fastled_progmem.h"

namespace fl {
namespace gfx {
namespace detail {

/// Floor-divide a by positive b (rounds toward -inf). (Self-contained
/// copy of the primitives.h helper: this header is included from the top
/// of that file, before its own detail helpers are declared.)
inline fl::i64 polyABFloorDivI64(fl::i64 a, fl::i64 b) FL_NO_EXCEPT {
    fl::i64 q = a / b;
    if ((a % b) < 0) --q;
    return q;
}

/// Maximum |vertex coordinate| accepted, in Q16.16 units (8191 px keeps
/// all values and edge deltas in i32 with headroom — same bound as the
/// exact kernel).
static const fl::i32 polyABQ16Max = 8191L << 16;

/// Float -> Q16.16, clamped to ±8191 px. NaN maps to 0 and out-of-range
/// values saturate, so the float->int cast is always in range (an
/// out-of-range or NaN cast is UB).
inline fl::i32 polyABClampPx(float px) FL_NO_EXCEPT {
    if (!(px == px)) return 0;  // NaN
    if (px > 8191.0f) return polyABQ16Max;
    if (px < -8191.0f) return -polyABQ16Max;
    return static_cast<fl::i32>(px * 65536.0f);
}

/// Convert any Coord type to Q16.16, clamped to ±8191 px. This is the one
/// conversion both polygon kernels use: primitives.h forwards its
/// toQ16Clamped here, because this header is included at the top of
/// primitives.h, before that header's own helpers are declared.
template<typename Coord>
inline fl::i32 polyABToQ16(Coord val) FL_NO_EXCEPT {
    return polyABClampPx(val.to_float());
}

template<>
inline fl::i32 polyABToQ16<float>(float val) FL_NO_EXCEPT {
    return polyABClampPx(val);
}

template<>
inline fl::i32 polyABToQ16<double>(double val) FL_NO_EXCEPT {
    if (!(val == val)) return 0;  // NaN
    if (val > 8191.0) return polyABQ16Max;
    if (val < -8191.0) return -polyABQ16Max;
    return static_cast<fl::i32>(val * 65536.0);
}

template<>
// 16-bit int coordinate overload: the public API accepts int PolyPoint
// coords (int on AVR); values clamp to ±8191 px, so the i32 result is exact.
inline fl::i32 polyABToQ16<int>(int val) FL_NO_EXCEPT {  // fl-lint: int-template-ok int coord API
    if (val > 8191) return polyABQ16Max;
    if (val < -8191) return -polyABQ16Max;
    return static_cast<fl::i32>(val) << 16;
}

template<>
// s16x16 spans ±32767 px, so its raw value must be clamped too: unclamped,
// edge deltas (vx[j] - vx[i]) overflow i32.
inline fl::i32 polyABToQ16<fl::s16x16>(fl::s16x16 val) FL_NO_EXCEPT {
    const fl::i32 r = val.raw();
    if (r > polyABQ16Max) return polyABQ16Max;
    if (r < -polyABQ16Max) return -polyABQ16Max;
    return r;
}

/// Popcount of a byte. AVR: 256 B PROGMEM LUT (4 lookups per u32); the
/// blog's memory budget explicitly allows small LUTs. Host: builtin.
inline fl::u8 polyABPop8(fl::u8 v) FL_NO_EXCEPT {
#if defined(__AVR__)
    static const fl::u8 kPop8[256] FL_PROGMEM = {
        0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,
        1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
        1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
        2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
        1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
        2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,
        2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
        3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
        1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
        2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,
        2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
        3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
        2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
        3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
        3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
        4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8
    };
    return (fl::u8)FL_PGM_READ_BYTE_NEAR(&kPop8[v]);
#elif defined(__GNUC__) || defined(__clang__)
    return (fl::u8)__builtin_popcount((unsigned)v);
#else
    v = v - ((v >> 1) & 0x55u);
    v = (v & 0x33u) + ((v >> 2) & 0x33u);
    return (fl::u8)((v + (v >> 4)) & 0x0Fu);
#endif
}

inline fl::u8 polyABPopcount32(fl::u32 v) FL_NO_EXCEPT {
    return (fl::u8)(polyABPop8((fl::u8)(v & 0xFFu))
                   + polyABPop8((fl::u8)((v >> 8) & 0xFFu))
                   + polyABPop8((fl::u8)((v >> 16) & 0xFFu))
                   + polyABPop8((fl::u8)(v >> 24)));
}

/// Alpha (0..255) for an 8x4 mask: 8 per subpixel, saturating at 255
/// (32 subpixels would otherwise wrap a u8).
inline fl::u8 polyABAlpha(fl::u32 mask) FL_NO_EXCEPT {
    const fl::u8 p = polyABPopcount32(mask);
    const fl::u8 br = (fl::u8)(p * 8u);
    return (p >= 32u) ? 255u : br;
}

/// Per-edge scanline state.
///
/// x0 is the Q16.16 crossing of sub-row 0 of the CURRENT scanline and
/// advances exactly per scanline (x0 += xinc). Sub-row r's crossing is
/// X_r = x0 + r*q + corr[r]:
///   q    = xinc >> 2                 (integer part of the per-sub-row step)
///   corr = floor((n0 + r*16384*m)/65536),
///          n0 = (off*xinc) & 65535,  (off = 65536*py0 - ytop_q, the
///          m  = xinc & 3             vertex's offset from py0's top)
/// n0 is invariant across scanlines because off advances by exactly 65536
/// (one pixel) per scanline. X_r is the edge's line at sub-row r's TOP
/// grid line, measured exactly from the top vertex in Q16, so a vertex
/// mid-sub-row introduces no quantization shift. ytop/ybot bound the
/// in-range sub-rows in .2 (4 per pixel, half-open [ytop, ybot)): a
/// sub-row is in range when its TOP grid line lies in the edge's
/// [ytop_q, ybot_q) span (the segment reaches the sub-row's top), so a
/// vertex shared by two edges (on the grid or mid-sub-row) never makes a
/// sub-row claimed twice. py0/py1 are the canvas rows they span
/// (identical row convention to the exact kernel).
struct AbEdge {
    fl::i64 x0;      // sub-row-0 crossing, Q16.16, current scanline
    fl::i64 xinc;    // per-scanline step, Q16.16: floor(65536 * B / dy)
    fl::i64 q;       // xinc >> 2
    fl::u8  corr[4]; // per-sub-row correction, 0..3
    fl::i32 ytop;    // first in-range sub-row, .2 (ceil of top vertex)
    fl::i32 ybot;    // one past last in-range sub-row, .2 (ceil of bottom)
    int py0;         // first active canvas row
    int py1;         // last active canvas row
};

/// Fill a run of [x0, x1) pixels of row py with `color` at alpha br.
/// The caller guarantees br > 0.
template<typename PixelT>
inline void polyABFillRun(PixelT* pixels, int py, int width,
                                 int x0, int x1, const PixelT& color,
                                 fl::u8 br, bool overwrite) FL_NO_EXCEPT {
    if (x1 <= x0) return;
    PixelT c = color;
    if (br < 255u) c.nscale8(br);
    PixelT* row = pixels + py * width + x0;
    if (overwrite) {
        for (int i = 0; i < x1 - x0; ++i) row[i] = c;
    } else {
        for (int i = 0; i < x1 - x0; ++i) row[i] += c;
    }
}

/// A-buffer 8x4 antialiased polygon fill (see file header).
/// MAXN caps the vertex count (vertices beyond it are ignored, polygon
/// closes from #MAXN-1 back to #0 — same as the exact kernel). There is
/// no accumulator to size: the working set is the per-edge state plus
/// one u32 mask, so the stack stays shallow regardless of canvas size.
/// It still scales with MAXN: vertices (8*MAXN B) + edges (40-48*MAXN B) +
/// crossing events (16*MAXN B), too much for an AVR Uno at MAXN = 32, so
/// the default MAXN is platform-dependent (measured frames at
/// FL_GFX_POLYGON_DEFAULT_MAXN in fl/gfx/canvas.h).
template<typename PixelT, typename Coord, bool Overwrite, int MAXN>
inline void drawPolygonABufferCore(Canvas<PixelT>& canvas, const PixelT& color,
                                   fl::span<const PolyPoint<Coord>> pts) FL_NO_EXCEPT {
    FL_STATIC_ASSERT(MAXN >= 3 && MAXN <= 256, "drawPolygon: MAXN must be in [3, 256]");
    const fl::i32 n = static_cast<fl::i32>(pts.size());
    if (n < 3) return;
    PixelT* pixels = canvas.pixels;
    int width = canvas.width;
    int height = canvas.height;
    if (width <= 0 || height <= 0) return;

    const int nv = (n < MAXN) ? n : MAXN;

    // Convert vertices to Q16.16 (clamped to ±8191 px — i32-safe)
    fl::i32 vx[MAXN];
    fl::i32 vy[MAXN];
    for (int i = 0; i < nv; ++i) {
        vx[i] = polyABToQ16(pts[i].x);
        vy[i] = polyABToQ16(pts[i].y);
    }

    // Build per-edge scanline state. One i64 floor division per edge is
    // the only division in this function (it sets xinc exactly).
    AbEdge edges[MAXN];
    int nedges = 0;
    int ymin = height, ymax = -1;
    for (int i = 0; i < nv; ++i) {
        const int j = (i + 1 == nv) ? 0 : (i + 1);
        const fl::i32 dy = vy[j] - vy[i];
        if (dy == 0) continue;  // horizontal edge: no trapezoid (even-odd convention)
        // Orient edge top-to-bottom: B measured from the top vertex
        fl::i32 ytop_q, xtop, B;
        if (dy > 0) { ytop_q = vy[i]; xtop = vx[i]; B = vx[j] - vx[i]; }
        else        { ytop_q = vy[j]; xtop = vx[j]; B = vx[i] - vx[j]; }
        const fl::i32 dy2 = (dy > 0) ? dy : -dy;  // span height, > 0
        const fl::i32 ybot_q = ytop_q + dy2;

        // Active pixel rows: same floor/ceil row rule as the exact kernel
        // (the row containing ytop through the row containing ybot - 1)
        int py0 = ytop_q >> 16;
        int py1 = (ybot_q - 1) >> 16;
        if (py0 < 0) py0 = 0;
        if (py1 >= height) py1 = height - 1;
        if (py0 > py1) continue;  // edge off-canvas vertically

        AbEdge& e = edges[nedges++];
        e.py0 = py0;
        e.py1 = py1;
        // In-range sub-rows, .2 (4 per px), half-open [ytop, ybot): a
        // sub-row is in range when its TOP grid line (s*16384 Q16) lies in
        // the edge's [ytop_q, ybot_q) span — the segment reaches the
        // sub-row's top. On the grid, a vertex shared by two edges
        // belongs to exactly one of them; mid-sub-row, the vertex's
        // sub-row belongs to the edge that ENDS there (its partner starts
        // at the next sub-row), so no sub-row is claimed twice and the
        // per-sub-row even-odd crossing set stays consistent.
        e.ytop = (ytop_q + 16383) >> 14;
        e.ybot = (ybot_q + 16383) >> 14;
        // i64 step: |65536 * B| < 2^47, dy >= 1 -> exact floor, no wrap
        e.xinc = polyABFloorDivI64((fl::i64)65536L * (fl::i64)B, (fl::i64)dy2);
        e.q = e.xinc >> 2;

        // x0: the edge's line at the TOP grid line of py0's sub-row 0,
        // measured exactly from the top vertex in Q16: off = 65536*py0 -
        // ytop_q, so a vertex mid-sub-row introduces no quantization
        // shift. Every in-range sub-row's top lies within the segment
        // (see [ytop, ybot) above), so the line value there is valid for
        // either xinc sign — no clamping needed. off*xinc stays in i64
        // (same bound as the vertex-snapped seed this replaces).
        const fl::i64 off = ((fl::i64)py0 << 16) - (fl::i64)ytop_q;
        e.x0 = (fl::i64)xtop + polyABFloorDivI64(off * e.xinc, 65536L);
        // corr[r] = floor((n0 + r*16384*m)/65536), n0 = (off*xinc) mod
        // 65536 in [0, 65536): the exact per-sub-row correction for the
        // fractional part of xinc/4. X_r = xtop + floor((off + r*16384)
        // * xinc / 65536) holds exactly, and the row advance (x0 +=
        // xinc) is exact because off gains a multiple of 65536 per row.
        const fl::i32 m = (fl::i32)(e.xinc & 3);           // in [0, 4)
        const fl::i64 n0 = ((off & 65535L) * (e.xinc & 65535L)) & 65535L;
        e.corr[0] = 0;
        e.corr[1] = (fl::u8)((n0 + 16384L * (fl::i64)m) >> 16);
        e.corr[2] = (fl::u8)((n0 + 2L * 16384L * (fl::i64)m) >> 16);
        e.corr[3] = (fl::u8)((n0 + 3L * 16384L * (fl::i64)m) >> 16);

        if (e.py0 < ymin) ymin = e.py0;
        if (e.py1 > ymax) ymax = e.py1;
    }
    if (nedges == 0) return;

    // Per-row scratch: crossing events, packed (pixel << 5) | (row << 3) |
    // bitOffset. A sub-row crosses at most ONE pixel (its crossing X is
    // fixed for the scanline), so a row has at most 4*nedges events.
    fl::i32 cp[4 * MAXN];

    for (int py = ymin; py <= ymax; ++py) {
        fl::u32 mask = 0;   // the running 8x4 mask (per row: 0x00 or 0xFF)
        int ncp = 0;

        for (int e = 0; e < nedges; ++e) {
            AbEdge& ed = edges[e];
            if (py < ed.py0 || py > ed.py1) continue;
            if (py > ed.py0) ed.x0 += ed.xinc;
            // In-range sub-rows: ytop <= 4*py + r < ybot, r in [0, 4)
            int rlo = (int)ed.ytop - 4 * py;  // in [-3, 4]; 4 only at py0
            // (vertex below the row's top) -> skipped via rlo > rhi
            if (rlo < 0) rlo = 0;
            int rhi = (int)ed.ybot - 1 - 4 * py;  // in [-3, 3]
            if (rhi < 0) rhi = 0;
            if (rhi > 3) rhi = 3;
            if (rlo > rhi) continue;
            // Sub-row crossings, incremental (no multiply): X_{r+1} =
            // X_r + q + (corr[r+1] - corr[r]), the difference in {0, 1}
            fl::i64 X = ed.x0;
            for (int r = 0; r <= rhi; ++r) {
                if (r > 0) X += ed.q + (fl::i64)((fl::i32)(ed.corr[r] - ed.corr[r - 1]));
                if (r < rlo) continue;
                if (X < 0) {
                    // Edge already left of the canvas: full-row
                    // contribution — seed the parity, never crosses again
                    mask ^= (fl::u32)0xFFu << (8 * r);
                } else {
                    const fl::i32 p = (fl::i32)(X >> 16);  // pixel crossed
                    if (p < width) {
                        const fl::u32 bo = (fl::u32)((X - ((fl::i64)p << 16)) >> 13);  // in [0, 8)
                        cp[ncp] = (p << 5) | (r << 3) | (fl::i32)bo;
                        ++ncp;
                    }
                }
            }
        }

        if (ncp == 0) {
            // No crossings this row: uniform mask over the whole row
            if (mask != 0)
                polyABFillRun(pixels, py, width, 0, width, color,
                              polyABAlpha(mask), Overwrite);
            continue;
        }

        // Sort events by pixel (packed key is pixel-major); insertion sort
        for (int a = 1; a < ncp; ++a) {
            const fl::i32 v = cp[a];
            int b = a - 1;
            while (b >= 0 && cp[b] > v) { cp[b + 1] = cp[b]; --b; }
            cp[b + 1] = v;
        }

        // Walk the row: the mask (per row 0x00/0xFF after each extend) is
        // constant between crossing pixels, so whole runs are painted in
        // one go and only crossing pixels pay for the popcount.
        int x = 0;
        for (int k = 0; k < ncp; ) {
            const int p = cp[k] >> 5;
            if (p > x) {
                if (mask != 0)
                    polyABFillRun(pixels, py, width, x, p, color,
                                  polyABAlpha(mask), Overwrite);
                x = p;
            }
            // Fold every event at p (XOR is commutative — no edge order
            // is needed, self-intersecting polygons included)
            fl::u32 rowxor = 0;
            while (k < ncp && (cp[k] >> 5) == p) {
                rowxor ^= (fl::u32)((1u << (8 - (cp[k] & 7u))) - 1u)
                        << (8 * ((cp[k] >> 3) & 3u));
                ++k;
            }
            mask ^= rowxor;
            if (mask != 0) {
                const fl::u8 br = polyABAlpha(mask);
                PixelT c = color;
                if (br < 255u) c.nscale8(br);
                PixelT& px = pixels[py * width + p];
                if (Overwrite) px = c; else px += c;
            }
            // Extend to the next pixel: a set rightmost subpixel of a row
            // (bit 0) means the trapezoid continues past the pixel, so the
            // whole row starts full. (mask & 0x01010101) * 255 ==
            // (b << 8) - b per row — shifts and a subtract, no multiply.
            mask = (mask & 0x01010101u) * 255u;
            x = p + 1;
        }
        if (x < width && mask != 0)
            polyABFillRun(pixels, py, width, x, width, color,
                          polyABAlpha(mask), Overwrite);
    }
}

}  // namespace detail
}  // namespace gfx
}  // namespace fl
