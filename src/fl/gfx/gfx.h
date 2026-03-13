#pragma once

/// @file fl/gfx/gfx.h
/// @brief 2D antialiased graphics for LED matrices
///
/// #include "fl/gfx/gfx.h"
///
/// Quick start:
///   CRGB leds[256];
///   fl::CanvasRGB canvas(leds, 16, 16);
///
///   // Antialiased line
///   canvas.drawLine(CRGB::White, 0.0f, 0.0f, 15.0f, 10.0f);
///
///   // Filled circle with soft edges
///   canvas.drawDisc(CRGB::Red, 8.0f, 8.0f, 4.5f);
///
///   // Ring (hollow circle)
///   canvas.drawRing(CRGB::Blue, 8.0f, 8.0f, 5.0f, 1.5f);
///
///   // Thick line with round end caps
///   canvas.drawStrokeLine(CRGB::Green, 2.0f, 8.0f, 14.0f, 8.0f, 3.0f,
///                         fl::LineCap::ROUND);
///
/// Coordinates can be float, int, or fixed-point (fl::s16x16, etc.).
/// Default draw mode is additive (DRAW_MODE_BLEND) — overlapping shapes blend naturally.
/// Use DRAW_MODE_OVERWRITE to replace pixels instead of blending.
///
/// Line cap styles:
///   fl::LineCap::FLAT   — no extension beyond endpoints (default)
///   fl::LineCap::ROUND  — hemispherical caps at each end
///   fl::LineCap::SQUARE — rectangular extension by half-thickness

#include "fl/stl/compiler_control.h"
#include "fl/gfx/crgb.h"
#include "fl/gfx/draw_mode.h"

// IWYU pragma: begin_keep
#include "fl/gfx/canvas.h"
#include "fl/gfx/primitives.h"
// IWYU pragma: end_keep

namespace fl {

/// @brief Line cap style
using LineCap = gfx::LineCap;

/// @brief Generic canvas for any pixel type (e.g. Canvas<CRGB16>).
/// All methods are force-inlined delegates — zero overhead.
template<typename RGB_T>
class Canvas {
    gfx::Canvas<RGB_T> mImpl;

  public:
    FASTLED_FORCE_INLINE Canvas(fl::span<RGB_T> buf, int w, int h) : mImpl(buf, w, h) {}
    FASTLED_FORCE_INLINE Canvas(fl::shared_ptr<RGB_T> ptr, int w, int h) : mImpl(ptr, w, h) {}

    FASTLED_FORCE_INLINE int size() const { return mImpl.size(); }
    FASTLED_FORCE_INLINE RGB_T& at(int x, int y) { return mImpl.at(x, y); }
    FASTLED_FORCE_INLINE const RGB_T& at(int x, int y) const { return mImpl.at(x, y); }
    FASTLED_FORCE_INLINE bool has(int x, int y) const { return mImpl.has(x, y); }

    template<typename Coord>
    FASTLED_FORCE_INLINE void drawLine(const RGB_T& color, Coord x0, Coord y0, Coord x1, Coord y1,
                                       DrawMode mode = DRAW_MODE_BLEND) {
        mImpl.drawLine(color, x0, y0, x1, y1, mode);
    }

    template<typename Coord>
    FASTLED_FORCE_INLINE void drawDisc(const RGB_T& color, Coord cx, Coord cy, Coord r,
                                       DrawMode mode = DRAW_MODE_BLEND) {
        mImpl.drawDisc(color, cx, cy, r, mode);
    }

    template<typename Coord>
    FASTLED_FORCE_INLINE void drawRing(const RGB_T& color, Coord cx, Coord cy, Coord r, Coord thickness,
                                       DrawMode mode = DRAW_MODE_BLEND) {
        mImpl.drawRing(color, cx, cy, r, thickness, mode);
    }

    template<typename Coord>
    FASTLED_FORCE_INLINE void drawStrokeLine(const RGB_T& color, Coord x0, Coord y0, Coord x1, Coord y1,
                                             Coord thickness, LineCap cap = LineCap::FLAT,
                                             DrawMode mode = DRAW_MODE_BLEND) {
        mImpl.drawStrokeLine(color, x0, y0, x1, y1, thickness, cap, mode);
    }
};

/// @brief Convenience alias for CRGB canvas — use fl::CanvasRGB for no-template syntax.
class CanvasRGB : protected Canvas<CRGB> {
  public:
    using Canvas<CRGB>::Canvas;
    using Canvas<CRGB>::size;
    using Canvas<CRGB>::at;
    using Canvas<CRGB>::has;
    using Canvas<CRGB>::drawLine;
    using Canvas<CRGB>::drawDisc;
    using Canvas<CRGB>::drawRing;
    using Canvas<CRGB>::drawStrokeLine;
};

}  // namespace fl
