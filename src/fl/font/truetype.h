#pragma once

// TrueType Font API for FastLED
//
// Minimal example:
//
//   #include "fl/font/truetype.h"
//   #include "fl/font/truetype.cpp.hpp"  // Include ONCE in your project
//
//   // Option 1: Use default embedded font (Covenant5x5 - 9.9KB, 5x5 pixel)
//   auto font = fl::Font::loadDefault();
//   fl::FontRenderer renderer(font, 10.0f);  // 10px height
//
//   // Option 2: Load custom font
//   auto font = fl::Font::load(fl::span<const uint8_t>(ttf_data, ttf_size));
//   fl::FontRenderer renderer(font, 14.0f);  // 14px height
//
//   // Render a character
//   fl::GlyphBitmap glyph = renderer.render('A');
//   for (int y = 0; y < glyph.height; ++y) {
//       for (int x = 0; x < glyph.width; ++x) {
//           uint8_t alpha = glyph.getPixel(x, y);  // 0-255
//           // Draw to LED at (screenX + glyph.xOffset + x, screenY + glyph.yOffset + y)
//       }
//   }
//
//   // String measurement and kerning
//   float width = renderer.measureString("Hello");
//   float kern = renderer.getKerning('A', 'V');

#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/stdint.h"
#include "fl/stl/string.h"

namespace fl {

// Forward declarations
class FontImpl;
class FontRendererImpl;

// Font metrics returned by Font::getMetrics()
struct FontMetrics {
    i32 ascent;     // Units above baseline
    i32 descent;    // Units below baseline (typically negative)
    i32 lineGap;    // Additional spacing between lines
    i32 x0, y0;     // Bounding box min
    i32 x1, y1;     // Bounding box max
};

// Glyph (single character) metrics
struct GlyphMetrics {
    i32 advanceWidth;      // Horizontal advance after glyph
    i32 leftSideBearing;   // Left side bearing
    i32 x0, y0;            // Bounding box min
    i32 x1, y1;            // Bounding box max
    bool isEmpty;              // True if glyph has no visual representation
};

// Rendered glyph data
struct GlyphBitmap {
    fl::vector<u8> data;  // Grayscale bitmap (0=transparent, 255=opaque)
    i32 width;                    // Bitmap width in pixels
    i32 height;                   // Bitmap height in pixels
    i32 xOffset;                  // X offset from origin to top-left of bitmap
    i32 yOffset;                  // Y offset from origin to top-left of bitmap (typically negative)

    GlyphBitmap() : width(0), height(0), xOffset(0), yOffset(0) {}

    // Returns true if this bitmap has valid data
    bool valid() const { return !data.empty() && width > 0 && height > 0; }

    // Get pixel value at (x, y) - returns 0 if out of bounds
    u8 getPixel(i32 x, i32 y) const {
        if (data.empty() || x < 0 || y < 0 || x >= width || y >= height) {
            return 0;
        }
        return data[y * width + x];
    }
};

// Font class - represents a loaded TrueType font
class Font {
public:
    // Load the default embedded font (Covenant5x5 - 9.9KB, 5x5 pixel font)
    // Returns nullptr if loading fails
    static fl::shared_ptr<Font> loadDefault();

    // Load a font from raw TrueType data (.ttf file contents)
    // Returns nullptr if the font data is invalid
    static fl::shared_ptr<Font> load(fl::span<const u8> fontData);

    // Load a specific font from a TrueType collection (.ttc file)
    // fontIndex: 0-based index of the font in the collection
    static fl::shared_ptr<Font> load(fl::span<const u8> fontData, i32 fontIndex);

    virtual ~Font() = default;

    // Get the number of fonts in this file (1 for .ttf, possibly more for .ttc)
    virtual i32 getNumFonts() const = 0;

    // Get overall font metrics (unscaled)
    virtual FontMetrics getMetrics() const = 0;

    // Get scale factor to achieve a specific pixel height
    virtual float getScaleForPixelHeight(float pixelHeight) const = 0;

    // Get glyph metrics for a unicode codepoint (unscaled)
    virtual GlyphMetrics getGlyphMetrics(i32 codepoint) const = 0;

    // Get kerning adjustment between two characters (unscaled)
    // Returns the adjustment to add to advance width
    virtual i32 getKerning(i32 codepoint1, i32 codepoint2) const = 0;

    // Render a single character to a grayscale bitmap
    // Returns an empty GlyphBitmap if the character doesn't exist
    virtual GlyphBitmap renderGlyph(i32 codepoint, float scale) const = 0;

    // Render with antialiasing control
    // oversampleX/Y: 1 = no oversampling, 2+ = oversample for smoother edges
    virtual GlyphBitmap renderGlyph(i32 codepoint, float scale,
                                    i32 oversampleX, i32 oversampleY) const = 0;

protected:
    Font() = default;
};

using FontPtr = fl::shared_ptr<Font>;

// FontRenderer - convenient wrapper for rendering at a specific size
class FontRenderer {
public:
    // Create a renderer for the given font at the specified pixel height
    FontRenderer(FontPtr font, float pixelHeight);

    ~FontRenderer();

    // Check if renderer is valid
    bool valid() const { return mFont != nullptr; }

    // Get the pixel height this renderer was created with
    float pixelHeight() const { return mPixelHeight; }

    // Get the scale factor being used
    float scale() const { return mScale; }

    // Get scaled font metrics
    struct ScaledMetrics {
        float ascent;
        float descent;
        float lineGap;
        float lineHeight() const { return ascent - descent + lineGap; }
    };
    ScaledMetrics getScaledMetrics() const;

    // Render a character at the current size
    // Uses 2x2 oversampling by default for smooth edges on LED displays
    GlyphBitmap render(i32 codepoint) const;

    // Render with custom oversampling
    GlyphBitmap render(i32 codepoint, i32 oversampleX, i32 oversampleY) const;

    // Render without antialiasing (1x1 oversampling)
    GlyphBitmap renderNoAA(i32 codepoint) const;

    // Get the advance width for a character (in pixels)
    float getAdvance(i32 codepoint) const;

    // Get kerning between two characters (in pixels)
    float getKerning(i32 codepoint1, i32 codepoint2) const;

    // Calculate the width of a string (in pixels)
    // Includes kerning between characters
    float measureString(const char* str) const;
    float measureString(fl::span<const char> str) const;

private:
    FontPtr mFont;
    float mPixelHeight;
    float mScale;
};

} // namespace fl
