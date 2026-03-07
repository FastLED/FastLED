#pragma once

#include "fl/span.h"
#include "fl/xymap.h"
#include "fl/stl/variant.h"
#include "fl/stl/shared_ptr.h"

namespace fl {
struct CRGB;
class Leds;

namespace gfx {

/// @brief Line cap styles for stroke operations
enum class LineCap { FLAT, ROUND, SQUARE };

/// @brief Simple rectangular canvas for graphics operations
/// Combines a pixel buffer with dimensions for cache-optimal drawing.
/// Supports raw pointers, spans, and shared_ptr for flexible ownership.
/// Keeps a cached pointer for fast pixel access.
template<typename PixelType = CRGB>
struct Canvas {
    // Ownership variants (keeps buffer alive)
    fl::variant<
        fl::span<PixelType>,
        fl::shared_ptr<PixelType>
    > ownership;

    // Cached pointer for fast access (dereferenced from ownership on construction)
    PixelType* pixels;

    int width;
    int height;

    /// Construct canvas from raw buffer pointer
    /// Ownership is managed externally; data must outlive canvas
    Canvas(PixelType* data, int w, int h)
        : ownership(fl::span<PixelType>(data, w * h)), pixels(data), width(w), height(h) {}

    /// Construct canvas from span
    /// Ownership via span; underlying data must remain valid
    Canvas(fl::span<PixelType> buf, int w, int h)
        : ownership(buf), pixels(buf.data()), width(w), height(h) {}

    /// Construct canvas from shared_ptr
    /// Ownership via shared_ptr; data lifetime managed automatically
    Canvas(fl::shared_ptr<PixelType> ptr, int w, int h)
        : ownership(ptr), pixels(ptr.get()), width(w), height(h) {}

    /// Get total pixel count
    int size() const { return width * height; }

    /// Get pixel at coordinates (with bounds checking)
    PixelType& at(int x, int y) {
        return pixels[y * width + x];
    }

    /// Get pixel at coordinates (const)
    const PixelType& at(int x, int y) const {
        return pixels[y * width + x];
    }

    /// Check if coordinates are in bounds
    bool has(int x, int y) const {
        return x >= 0 && x < width && y >= 0 && y < height;
    }

    // Drawing methods (discoverable via intellisense)
    // Note: These delegate to the free functions in gfx namespace
    // Only specialized for CRGB canvas - see specialization below

    /// @brief Wu antialiased line drawing
    /// Supports any coordinate type: float, int, s16x16, or other arithmetic types
    template<typename Coord>
    void drawLine(Coord x0, Coord y0, Coord x1, Coord y1, const PixelType& color);

    /// @brief Antialiased filled circle
    /// Supports any coordinate type: float, int, s16x16, or other arithmetic types
    template<typename Coord>
    void drawDisc(Coord cx, Coord cy, Coord r, const PixelType& color);

    /// @brief Antialiased circle outline
    /// Supports any coordinate type: float, int, s16x16, or other arithmetic types
    template<typename Coord>
    void drawRing(Coord cx, Coord cy, Coord r, Coord thickness, const PixelType& color);

    /// @brief Antialiased stroked line with configurable end caps
    /// Supports any coordinate type: float, int, s16x16, or other arithmetic types
    template<typename Coord>
    void drawStrokeLine(Coord x0, Coord y0, Coord x1, Coord y1, Coord thickness, const PixelType& color,
                        LineCap cap = LineCap::FLAT);
};

/// @brief Raster target for drawing operations
/// Represents a 2D grid of CRGB pixels with layout information
struct DrawCanvas {
    fl::span<CRGB> buffer;
    fl::XYMap xymap;

    int width() const { return xymap.getWidth(); }
    int height() const { return xymap.getHeight(); }

    bool has(int x, int y) const {
        return xymap.has(x, y);
    }

    void setPixel(int x, int y, const CRGB& c) {
        if (has(x, y)) {
            buffer[xymap(x, y)] = c;
        }
    }

    void addPixel(int x, int y, const CRGB& c) {
        if (has(x, y)) {
            buffer[xymap(x, y)] += c;
        }
    }
};

/// @brief Variant type representing a raster target (Leds* or DrawCanvas)
using RasterTarget = fl::variant<fl::Leds*, DrawCanvas>;

/// @brief Resolve RasterTarget variant to DrawCanvas
/// If the variant holds an fl::Leds*, converts it to DrawCanvas
/// Otherwise returns the DrawCanvas directly
/// This is resolved once per draw call, with zero per-pixel overhead
DrawCanvas resolveCanvas(const RasterTarget& target);

// Forward declarations for member function implementations
// All draw functions are templated on coordinate type
template<typename Coord>
void drawLine(Canvas<CRGB>& canvas, Coord x0, Coord y0, Coord x1, Coord y1, const CRGB& color);

template<typename Coord>
void drawDisc(Canvas<CRGB>& canvas, Coord cx, Coord cy, Coord r, const CRGB& color);

template<typename Coord>
void drawRing(Canvas<CRGB>& canvas, Coord cx, Coord cy, Coord r, Coord thickness, const CRGB& color);

template<typename Coord>
void drawStrokeLine(Canvas<CRGB>& canvas, Coord x0, Coord y0, Coord x1, Coord y1, Coord thickness, const CRGB& color,
                    LineCap cap);

/// Template specialization: Canvas<CRGB> member function implementations
template<>
struct Canvas<CRGB> {
    // Ownership variants
    fl::variant<
        fl::span<CRGB>,
        fl::shared_ptr<CRGB>
    > ownership;

    CRGB* pixels;
    int width;
    int height;

    Canvas(CRGB* data, int w, int h)
        : ownership(fl::span<CRGB>(data, w * h)), pixels(data), width(w), height(h) {}
    Canvas(fl::span<CRGB> buf, int w, int h)
        : ownership(buf), pixels(buf.data()), width(w), height(h) {}
    Canvas(fl::shared_ptr<CRGB> ptr, int w, int h)
        : ownership(ptr), pixels(ptr.get()), width(w), height(h) {}

    int size() const { return width * height; }
    CRGB& at(int x, int y) { return pixels[y * width + x]; }
    const CRGB& at(int x, int y) const { return pixels[y * width + x]; }
    bool has(int x, int y) const { return x >= 0 && x < width && y >= 0 && y < height; }

    // Drawing methods with implementations (all templated on coordinate type)
    template<typename Coord>
    inline void drawLine(Coord x0, Coord y0, Coord x1, Coord y1, const CRGB& color) {
        gfx::drawLine(*this, x0, y0, x1, y1, color);
    }

    template<typename Coord>
    inline void drawDisc(Coord cx, Coord cy, Coord r, const CRGB& color) {
        gfx::drawDisc(*this, cx, cy, r, color);
    }

    template<typename Coord>
    inline void drawRing(Coord cx, Coord cy, Coord r, Coord thickness, const CRGB& color) {
        gfx::drawRing(*this, cx, cy, r, thickness, color);
    }

    template<typename Coord>
    inline void drawStrokeLine(Coord x0, Coord y0, Coord x1, Coord y1, Coord thickness, const CRGB& color,
                               LineCap cap = LineCap::FLAT) {
        gfx::drawStrokeLine(*this, x0, y0, x1, y1, thickness, color, cap);
    }
};

}  // namespace gfx
}  // namespace fl
