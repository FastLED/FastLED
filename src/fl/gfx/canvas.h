#pragma once

/// @file fl/gfx/canvas.h
/// @brief Canvas types for gfx primitives (implementation)

#include "fl/math/alpha.h"
#include "fl/math/xymap.h"
#include "fl/gfx/draw_mode.h"
#include "fl/stl/int.h"
#include "fl/stl/span.h"
#include "fl/stl/variant.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/scratch_config.h"
#include "fl/system/sketch_macros.h"

namespace fl {
namespace gfx {

/// @brief Line cap styles for stroke operations
enum class LineCap { FLAT, ROUND, SQUARE };

/// @brief Polygon vertex (x, y coordinate pair) for drawPolygon
/// C++11 aggregate: PolyPoint<float> p{3.5f, 2.0f};
template<typename Coord>
struct PolyPoint {
    Coord x;
    Coord y;
};

/// @brief Fill backend for drawPolygon
/// Both backends use the even-odd rule and the same row/vertex
/// conventions; they differ in the per-pixel coverage model:
///  - Exact: exact per-pixel sub-pixel area integration (256 alpha
///    levels). The default; byte-stable across builds.
///  - ABuffer: 8x4 subpixel A-buffer sampling (32 alpha levels, 32-bit
///    masks, no division in the scan loops, small working set). Faster on
///    dense fills and low-memory targets; see fl/gfx/poly_abuffer.h.
enum class PolyFill { Exact, ABuffer };

/// @brief Default drawPolygon tier (MAXN vertex cap, CHUNK accumulator px)
/// Both kernels keep their whole working set (fl/gfx/poly_scratch.h:
/// PolyScratch / AbScratch, sized by MAXN, and CHUNK for Exact) in one
/// struct, leased from fl::scratch's side-stack arena (fl/stl/scratch.h)
/// when FL_SCRATCH_STACK_BYTES > 0 and it fits there, else kept on the call
/// stack. Kernel frame on AVR with the stack local (avr-gcc 7.3 -Os,
/// -fstack-usage; callees add ~0.2 KB):
///  - Exact:   MAXN = 32, CHUNK = 64: 4134 B;  MAXN = 8, CHUNK = 16:  992 B
///  - ABuffer: MAXN = 32:             2139 B;  MAXN = 8:              603 B
/// On an Uno (2 KB SRAM), a 16x16 CRGB canvas (768 B) plus FastLED and
/// Serial leaves ~950 B of stack: enough for ABuffer, not for Exact's
/// ~1.2 KB call chain, which overflows the stack (verified in simavr).
/// Use PolyFill::ABuffer there, or a 16x8 canvas for Exact. fl::scratch
/// does not help: the arena and the stack share the same SRAM. This is a
/// documented limit, not handled automatically (#5328): about half the
/// Exact 8/16 set is its 8 per-edge records, so trimming CHUNK or the band
/// tables cannot save the ~250 B needed without dropping vertices;
/// free stack depends on the sketch's RAM map, so no compile-time check
/// can see it; and a silent ABuffer fallback would change the output (32
/// vs 256 alpha levels) and link both kernels into a 32 KB-flash part.
/// The default is 32/64 when that working set has a home, else 8/16:
///  - a big stack: !FL_PLATFORM_HAS_SMALL_STACK (fl/system/sketch_macros.h
///    marks AVR, ESP8266's 4 KB loop() stack, Teensy LC, LPC804, ... as
///    small). That is a stack-size predicate, not the flash-size
///    FL_PLATFORM_HAS_LARGE_MEMORY tier: STM32F103C8, Teensy 3.x, Uno R4 and
///    LPC845 sit on the low flash tier yet have the stack for 32/64; or
///  - an fl::scratch arena of at least FL_GFX_POLYGON_LARGE_TIER_ARENA_BYTES
///    (both 32/64 working sets plus alignment padding; fl/gfx/poly_scratch.h
///    static-asserts the bound on every target). ESP8266's default arena is
///    4608 B, so it gets 32/64 while its loop() stack only holds the
///    kernels' small locals.
/// Vertices beyond MAXN are ignored; pass MAXN explicitly for larger
/// polygons. To change the default, set these build-wide (-D compiler flag)
/// so every translation unit sees the same Canvas default template
/// arguments.
#define FL_GFX_POLYGON_LARGE_TIER_ARENA_BYTES 4215

#if !FL_PLATFORM_HAS_SMALL_STACK || \
    (FL_SCRATCH_STACK_BYTES >= FL_GFX_POLYGON_LARGE_TIER_ARENA_BYTES)
#define FL_GFX_POLYGON_LARGE_TIER_DEFAULT 1
#else
#define FL_GFX_POLYGON_LARGE_TIER_DEFAULT 0
#endif

#ifndef FL_GFX_POLYGON_DEFAULT_MAXN
#if FL_GFX_POLYGON_LARGE_TIER_DEFAULT
#define FL_GFX_POLYGON_DEFAULT_MAXN 32
#else
#define FL_GFX_POLYGON_DEFAULT_MAXN 8
#endif
#endif

#ifndef FL_GFX_POLYGON_DEFAULT_CHUNK
#if FL_GFX_POLYGON_LARGE_TIER_DEFAULT
#define FL_GFX_POLYGON_DEFAULT_CHUNK 64
#else
#define FL_GFX_POLYGON_DEFAULT_CHUNK 16
#endif
#endif

// Forward declarations
template<typename RGB_T> struct Canvas;
template<typename RGB_T> struct CanvasMapped;

// Free function forward declarations
template<int hRadius, int vRadius, typename RGB_T>
void blurGaussian(Canvas<RGB_T>& canvas, fl::alpha8 dimFactor) FL_NO_EXCEPT;

template<int hRadius, int vRadius, typename RGB_T>
void blurGaussian(Canvas<RGB_T>& canvas, fl::alpha16 dimFactor) FL_NO_EXCEPT;

template<int hRadius, int vRadius, typename RGB_T>
void blurGaussian(Canvas<RGB_T>& canvas) FL_NO_EXCEPT;

// CanvasMapped blurGaussian forward declarations
template<int hRadius, int vRadius, typename RGB_T>
void blurGaussian(CanvasMapped<RGB_T>& canvas, fl::alpha8 dimFactor) FL_NO_EXCEPT;

template<int hRadius, int vRadius, typename RGB_T>
void blurGaussian(CanvasMapped<RGB_T>& canvas, fl::alpha16 dimFactor) FL_NO_EXCEPT;

template<int hRadius, int vRadius, typename RGB_T>
inline void blurGaussian(CanvasMapped<RGB_T>& canvas) FL_NO_EXCEPT {
    blurGaussian<hRadius, vRadius>(canvas, alpha8(255));
}

template<typename PixelT, typename Coord>
void drawLine(Canvas<PixelT>& canvas, const PixelT& color, Coord x0, Coord y0, Coord x1, Coord y1,
              fl::DrawMode mode = fl::DrawMode::DRAW_MODE_BLEND) FL_NO_EXCEPT;

template<typename PixelT, typename Coord>
void drawDisc(Canvas<PixelT>& canvas, const PixelT& color, Coord cx, Coord cy, Coord r,
              fl::DrawMode mode = fl::DrawMode::DRAW_MODE_BLEND) FL_NO_EXCEPT;

template<typename PixelT, typename Coord>
void drawRing(Canvas<PixelT>& canvas, const PixelT& color, Coord cx, Coord cy, Coord r, Coord thickness,
              fl::DrawMode mode = fl::DrawMode::DRAW_MODE_BLEND) FL_NO_EXCEPT;

template<typename PixelT, typename Coord>
void drawStrokeLine(Canvas<PixelT>& canvas, const PixelT& color, Coord x0, Coord y0, Coord x1, Coord y1,
                    Coord thickness, LineCap cap, fl::DrawMode mode = fl::DrawMode::DRAW_MODE_BLEND) FL_NO_EXCEPT;

/// Fill selects the polygon backend (PolyFill::Exact by default;
/// PolyFill::ABuffer for the faster 8x4 A-buffer kernel, which ignores
/// CHUNK).
template<typename PixelT, typename Coord, int MAXN = FL_GFX_POLYGON_DEFAULT_MAXN,
         int CHUNK = FL_GFX_POLYGON_DEFAULT_CHUNK,
         PolyFill Fill = PolyFill::Exact>
void drawPolygon(Canvas<PixelT>& canvas, const PixelT& color,
                 fl::span<const PolyPoint<Coord>> pts,
                 fl::DrawMode mode = fl::DrawMode::DRAW_MODE_BLEND) FL_NO_EXCEPT;

// Array overload: lets Coord be deduced from a C array (span is a
// converting constructor, so it cannot participate in deduction).
template<typename PixelT, typename Coord, int N, int MAXN = FL_GFX_POLYGON_DEFAULT_MAXN,
         int CHUNK = FL_GFX_POLYGON_DEFAULT_CHUNK,
         PolyFill Fill = PolyFill::Exact>
void drawPolygon(Canvas<PixelT>& canvas, const PixelT& color,
                 const PolyPoint<Coord> (&pts)[N],
                 fl::DrawMode mode = fl::DrawMode::DRAW_MODE_BLEND) FL_NO_EXCEPT;

/// @brief Simple rectangular canvas for graphics operations
/// Combines a pixel buffer with dimensions for cache-optimal drawing.
template<typename RGB_T>
struct Canvas {
    fl::variant<
        fl::span<RGB_T>,
        fl::shared_ptr<RGB_T>
    > ownership;

    RGB_T* pixels;
    int width;
    int height;

    Canvas(fl::span<RGB_T> buf, int w, int h) FL_NO_EXCEPT
        : ownership(buf), pixels(buf.data()), width(w), height(h) {}
    Canvas(fl::shared_ptr<RGB_T> ptr, int w, int h) FL_NO_EXCEPT
        : ownership(ptr), pixels(ptr.get()), width(w), height(h) {}

    int size() const FL_NO_EXCEPT { return width * height; }
    RGB_T& at(int x, int y) FL_NO_EXCEPT { return pixels[y * width + x]; }
    const RGB_T& at(int x, int y) const FL_NO_EXCEPT { return pixels[y * width + x]; }
    bool has(int x, int y) const FL_NO_EXCEPT { return x >= 0 && x < width && y >= 0 && y < height; }

    template<int hRadius, int vRadius>
    inline void blurGaussian(fl::alpha8 dimFactor) FL_NO_EXCEPT {
        gfx::blurGaussian<hRadius, vRadius>(*this, dimFactor);
    }

    template<int hRadius, int vRadius>
    inline void blurGaussian(fl::alpha16 dimFactor) FL_NO_EXCEPT {
        gfx::blurGaussian<hRadius, vRadius>(*this, dimFactor);
    }

    template<int hRadius, int vRadius>
    inline void blurGaussian() FL_NO_EXCEPT {
        gfx::blurGaussian<hRadius, vRadius>(*this);
    }

    template<typename Coord>
    inline void drawLine(const RGB_T& color, Coord x0, Coord y0, Coord x1, Coord y1,
                         fl::DrawMode mode = fl::DrawMode::DRAW_MODE_BLEND) FL_NO_EXCEPT {
        gfx::drawLine(*this, color, x0, y0, x1, y1, mode);
    }

    template<typename Coord>
    inline void drawDisc(const RGB_T& color, Coord cx, Coord cy, Coord r,
                         fl::DrawMode mode = fl::DrawMode::DRAW_MODE_BLEND) FL_NO_EXCEPT {
        gfx::drawDisc(*this, color, cx, cy, r, mode);
    }

    template<typename Coord>
    inline void drawRing(const RGB_T& color, Coord cx, Coord cy, Coord r, Coord thickness,
                         fl::DrawMode mode = fl::DrawMode::DRAW_MODE_BLEND) FL_NO_EXCEPT {
        gfx::drawRing(*this, color, cx, cy, r, thickness, mode);
    }

    template<typename Coord>
    inline void drawStrokeLine(const RGB_T& color, Coord x0, Coord y0, Coord x1, Coord y1, Coord thickness,
                               LineCap cap = LineCap::FLAT, fl::DrawMode mode = fl::DrawMode::DRAW_MODE_BLEND) FL_NO_EXCEPT {
        gfx::drawStrokeLine(*this, color, x0, y0, x1, y1, thickness, cap, mode);
    }

    template<typename Coord, int MAXN = FL_GFX_POLYGON_DEFAULT_MAXN,
             int CHUNK = FL_GFX_POLYGON_DEFAULT_CHUNK,
             PolyFill Fill = PolyFill::Exact>
    inline void drawPolygon(const RGB_T& color, fl::span<const PolyPoint<Coord>> pts,
                            fl::DrawMode mode = fl::DrawMode::DRAW_MODE_BLEND) FL_NO_EXCEPT {
        gfx::drawPolygon<RGB_T, Coord, MAXN, CHUNK, Fill>(*this, color, pts, mode);
    }

    template<typename Coord, int N, int MAXN = FL_GFX_POLYGON_DEFAULT_MAXN,
             int CHUNK = FL_GFX_POLYGON_DEFAULT_CHUNK,
             PolyFill Fill = PolyFill::Exact>
    inline void drawPolygon(const RGB_T& color, const PolyPoint<Coord> (&pts)[N],
                            fl::DrawMode mode = fl::DrawMode::DRAW_MODE_BLEND) FL_NO_EXCEPT {
        gfx::drawPolygon<RGB_T, Coord, N, MAXN, CHUNK, Fill>(*this, color, pts, mode);
    }
};

/// @brief XYMap-backed canvas for non-rectangular or remapped layouts.
/// Uses XYMap for all pixel access — no SIMD or cache-layout optimizations.
/// With a rectangular XYMap, produces identical results to Canvas<RGB_T>.
template<typename RGB_T>
struct CanvasMapped {
    fl::span<RGB_T> pixels;
    const XYMap* xymap;
    int width;
    int height;

    CanvasMapped(fl::span<RGB_T> buf, const XYMap& map) FL_NO_EXCEPT
        : pixels(buf), xymap(&map),
          width(map.getWidth()), height(map.getHeight()) {}

    int size() const FL_NO_EXCEPT { return width * height; }
    RGB_T& at(int x, int y) FL_NO_EXCEPT { return pixels[xymap->mapToIndex(x, y)]; }
    const RGB_T& at(int x, int y) const FL_NO_EXCEPT { return pixels[xymap->mapToIndex(x, y)]; }
    bool has(int x, int y) const FL_NO_EXCEPT { return x >= 0 && x < width && y >= 0 && y < height; }

    template<int hRadius, int vRadius>
    inline void blurGaussian(fl::alpha8 dimFactor) FL_NO_EXCEPT {
        gfx::blurGaussian<hRadius, vRadius>(*this, dimFactor);
    }

    template<int hRadius, int vRadius>
    inline void blurGaussian(fl::alpha16 dimFactor) FL_NO_EXCEPT {
        gfx::blurGaussian<hRadius, vRadius>(*this, dimFactor);
    }

    template<int hRadius, int vRadius>
    inline void blurGaussian() FL_NO_EXCEPT {
        gfx::blurGaussian<hRadius, vRadius>(*this);
    }
};

}  // namespace gfx
}  // namespace fl
