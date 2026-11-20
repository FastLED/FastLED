#pragma once

/// @file fl/gfx/poly_scratch.h
/// @brief drawPolygon kernel working sets and the default polygon tier.
///
/// Both drawPolygon kernels (Exact: drawPolygonCore in fl/gfx/primitives.h;
/// ABuffer: drawPolygonABufferCore in fl/gfx/poly_abuffer.h) keep their
/// large locals in one struct, leased through PolyScratchLease below: from
/// fl::scratch's fixed side-stack arena (fl/stl/scratch.h) when
/// FL_SCRATCH_STACK_BYTES > 0 and the struct fits it, else as a plain local
/// on the call stack. The structs depend
/// only on the tier (MAXN, CHUNK), never on the pixel or coordinate type,
/// so every drawPolygon instantiation of a tier shares one scratch type.
///
/// The default tier (FL_GFX_POLYGON_DEFAULT_MAXN / _CHUNK in
/// fl/gfx/canvas.h) is chosen from where that scratch can live.

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/scratch.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/type_traits.h"
#include "fl/gfx/canvas.h"  // FL_GFX_POLYGON_LARGE_TIER_ARENA_BYTES
#include "fl/system/sketch_macros.h"

namespace fl {
namespace gfx {
namespace detail {

/// Per-edge scanline state for drawPolygon (even-odd fill, band model).
/// x is the crossing at the CURRENT row's center (Q16.16, floored -- the
/// anchor polyLineT28 integrates from) and advances exactly per row via a
/// Bresenham-style remainder recurrence: x += step; t += rem; if (t >= dy)
/// { t -= dy; x += 1; } where step = floor(65536 * B / dy) and rem =
/// 65536 * B - step * dy with rem in [0, dy). Single-row edges (dy < 65536)
/// keep step = rem = t = 0: their span can still cross a row boundary (two
/// active rows), and the committed behavior is that x stays at the py0-row
/// value on the second row -- the kernel then integrates the infinite line
/// through that (stale) center crossing, and the extent below must box THAT
/// line, not the segment's endpoints.
/// hw = ceil(32768 * |B| / dy) is the line's x half-span over a full row
/// (|x(tau) - x| <= hw for |tau| <= 32768), so the per-row extent
/// lx0 = (x - hw) >> 16, lx1 = (x + hw) >> 16 is division-free (shifts) and
/// a valid bound for every row the edge is active in: px outside [lx0, lx1]
/// gets the same full/zero shortcut a polyLineT28 call would compute
/// exactly. hw is derived from (step, rem) at setup -- one i64 division per
/// edge, none per row. hw < 2^45 for i32 B/dy, so it needs i64.
/// x is i64: for a steep edge spanning < 1 px, the center crossing
/// extrapolates off the segment by up to 32768 * |B| / dy, which can exceed
/// i32 (verified reachable: ~2^45). t + rem < 2*dy < 2^31 (dy < 2^30), so
/// t/rem are i32-safe. An edge is active in every canvas row it geometrically
/// occupies (py0..py1, including the partial row containing each endpoint).
struct PolyEdge {
    fl::i64 x;      // crossing x at current row center, Q16.16
    fl::i64 step;   // per-row increment, Q16.16 (0 for single-row edges)
    fl::i32 rem;    // 65536*B - step*dy, in [0, dy) (0 for single-row)
    fl::i32 t;      // running remainder accumulator
    fl::i32 dy;     // span height, Q16.16 (> 0)
    fl::i32 B;      // dx in the top-to-bottom direction, Q16.16 (signed)
    fl::i32 ytop;   // top endpoint y, Q16.16
    fl::i32 ybot;   // bottom endpoint y, Q16.16
    fl::i64 hw;     // per-row x half-span, ceil(32768*|B|/dy) (see above)
    int py0;        // first active canvas row (row containing ytop)
    int py1;        // last active canvas row (row containing ybot - 1)
};

/// Per-edge scanline state for drawPolygonABufferCore (ABuffer).
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

/// drawPolygonCore's (Exact) working set for one call. Widest alignment
/// first, so the struct carries no interior padding.
template <int MAXN, int CHUNK>
struct PolyScratch {
    PolyEdge mEdges[MAXN];        // per-edge scanline state
    fl::i64 mKeys[MAXN];          // per-edge midpoint sort keys (polySortBand)
    fl::i32 mVx[MAXN];            // vertices, Q16.16
    fl::i32 mVy[MAXN];
    fl::i32 mEv[MAXN + 2];        // band boundaries: row ends + vertex y's in row
    fl::i32 mAccb[CHUNK];         // per-row CHUNK-px coverage accumulator
    fl::i32 mBfull[MAXN + 1];     // per-band full-pixel coverage
    int mLx0a[MAXN];              // per-row per-edge column extents
    int mLx1a[MAXN];
    fl::u8 mIdx[MAXN];            // a band's active edges, sorted
    fl::u8 mBnc[MAXN + 1];        // per-band active-edge count
    fl::u8 mBidx[MAXN + 1][MAXN]; // per-band active-edge lists
};

/// drawPolygonABufferCore's (ABuffer) working set for one call.
template <int MAXN>
struct AbScratch {
    AbEdge mEdges[MAXN];     // per-edge scanline state
    fl::i32 mVx[MAXN];       // vertices, Q16.16
    fl::i32 mVy[MAXN];
    fl::i32 mCp[4 * MAXN];   // per-row crossing events
};

/// True when fl::scratch's side-stack arena can hold a T: sizeof(T) plus
/// worst-case alignment padding within FL_SCRATCH_STACK_BYTES (fl::scratch's
/// own compile-time admission rule). Always false without an arena.
template <typename T>
constexpr bool polyScratchFitsArena() FL_NO_EXCEPT {
    return sizeof(T) + alignof(T) - 1 <= static_cast<fl::size>(FL_SCRATCH_STACK_BYTES);
}

/// Where a kernel's working set T lives: fl::scratch (the side-stack arena)
/// when it fits the arena, else a plain stack local (fl::scratch's
/// capacity-0 form) -- the pre-arena behaviour, so an explicit tier larger
/// than the arena, or an arena too small for any tier, still compiles and
/// draws. An arena lease can be empty at run time (arena exhausted); the
/// kernels then draw nothing. The stack form never is.
template <typename T>
using PolyScratchLease =
    typename fl::conditional<polyScratchFitsArena<T>(), fl::scratch<T>,
                             fl::detail::inline_scratch<T> >::type;

// FL_GFX_POLYGON_LARGE_TIER_ARENA_BYTES (fl/gfx/canvas.h) is the arena size
// from which the 32/64 tier becomes the default on small-stack targets. It
// is a number so the default-tier macros stay preprocessor numbers; these
// checks keep it from ever being smaller than what 32/64 really needs on
// this target, which would put 32/64 on a small stack.
FL_STATIC_ASSERT(sizeof(PolyScratch<32, 64>) + alignof(PolyScratch<32, 64>) - 1 <=
                     FL_GFX_POLYGON_LARGE_TIER_ARENA_BYTES,
                 "FL_GFX_POLYGON_LARGE_TIER_ARENA_BYTES is below the 32/64 Exact working set");
FL_STATIC_ASSERT(sizeof(AbScratch<32>) + alignof(AbScratch<32>) - 1 <=
                     FL_GFX_POLYGON_LARGE_TIER_ARENA_BYTES,
                 "FL_GFX_POLYGON_LARGE_TIER_ARENA_BYTES is below the 32/64 ABuffer working set");

}  // namespace detail
}  // namespace gfx
}  // namespace fl
