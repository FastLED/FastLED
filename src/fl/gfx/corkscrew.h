#pragma once

/**
 * @file corkscrew.h
 * @brief Corkscrew LED strip projection and rendering
 *
 * The Corkscrew class provides a complete solution for drawing to densely-wrapped
 * helical LED strips. It maps a cylindrical coordinate system to a linear LED
 * strip, allowing you to draw on a rectangular surface and have it correctly
 * projected onto the corkscrew topology.
 *
 * Usage:
 * 1. Create a Corkscrew with the number of turns and LEDs
 * 2. Draw patterns on the input surface using surface()
 * 3. Call draw() to map the surface to LED pixels
 * 4. Access the final LED data via rawData()
 *
 * The class handles:
 * - Automatic cylindrical dimension calculation
 * - Pixel storage (external span or internal allocation)
 * - Multi-sampling for smooth projections
 * - Gap compensation for non-continuous wrapping
 * - Iterator interface for advanced coordinate access
 *
 * Parameters:
 * - totalTurns: Number of helical turns around the cylinder
 * - numLeds: Total number of LEDs in the strip
 * - invert: Reverse the mapping direction (default: false)
 * - gapParams: Optional gap compensation for solder points in a strip.
 */

#include "fl/stl/allocator.h"
#include "fl/math/geometry.h"
#include "fl/math/math.h"
#include "fl/gfx/tile2x2.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/variant.h"
#include "fl/stl/span.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

// Forward declarations
class Leds;
class ScreenMap;
template<typename T> class Grid;

// Simple constexpr functions for compile-time corkscrew dimension calculation
constexpr fl::u16 calculateCorkscrewWidth(float totalTurns, fl::u16 numLeds) FL_NO_EXCEPT {
    return static_cast<fl::u16>(ceil_constexpr(static_cast<float>(numLeds) / totalTurns));
}

constexpr fl::u16 calculateCorkscrewHeight(float totalTurns, fl::u16 numLeds) FL_NO_EXCEPT {
    return (calculateCorkscrewWidth(totalTurns, numLeds) * static_cast<int>(ceil_constexpr(totalTurns)) > numLeds) ?
        static_cast<fl::u16>(ceil_constexpr(static_cast<float>(numLeds) / static_cast<float>(calculateCorkscrewWidth(totalTurns, numLeds)))) :
        static_cast<fl::u16>(ceil_constexpr(totalTurns));
}

/**
 * Struct representing gap parameters for corkscrew mapping
 */
struct Gap {
    int num_leds = 0;   // Number of LEDs after which gap is activated, 0 = no gap
    float gap = 0.0f;   // Gap value from 0 to 1, represents percentage of width unit to add
    
    Gap() FL_NO_EXCEPT = default;
    Gap(float g) FL_NO_EXCEPT : num_leds(0), gap(g) {} // Backwards compatibility constructor
    Gap(int n, float g) FL_NO_EXCEPT : num_leds(n), gap(g) {} // New constructor with num_leds
    
    // Rule of 5 for POD data
    Gap(const Gap &other) FL_NO_EXCEPT = default;
    Gap &operator=(const Gap &other) = default;
    Gap(Gap &&other) FL_NO_EXCEPT = default;
    Gap &operator=(Gap &&other) FL_NO_EXCEPT = default;
};

// Maps a Corkscrew defined by the input to a cylindrical mapping for rendering
// a densly wrapped LED corkscrew.
class Corkscrew {
  public:
    
    // Pixel storage variants - can hold either external span or owned vector
    using PixelStorage = fl::variant<fl::span<CRGB>, fl::vector_psram<CRGB>>;

    // Iterator class moved from CorkscrewState
    class iterator {
      public:
        using value_type = vec2f;
        using difference_type = fl::i32;
        using pointer = vec2f *;
        using reference = vec2f &;
        using iterator_category = fl::bidirectional_iterator_tag;

        iterator(const Corkscrew *corkscrew, fl::size position)
            FL_NO_EXCEPT : mCorkscrew(corkscrew), mPosition(position) {}

        vec2f operator*() const FL_NO_EXCEPT;

        iterator &operator++() FL_NO_EXCEPT {
            ++mPosition;
            return *this;
        }

        iterator operator++(int) FL_NO_EXCEPT {
            iterator temp = *this;
            ++mPosition;
            return temp;
        }

        iterator &operator--() FL_NO_EXCEPT {
            --mPosition;
            return *this;
        }

        iterator operator--(int) FL_NO_EXCEPT {
            iterator temp = *this;
            --mPosition;
            return temp;
        }

        bool operator==(const iterator &other) const FL_NO_EXCEPT {
            return mPosition == other.mPosition;
        }

        bool operator!=(const iterator &other) const FL_NO_EXCEPT {
            return mPosition != other.mPosition;
        }

        difference_type operator-(const iterator &other) const FL_NO_EXCEPT {
            return static_cast<difference_type>(mPosition) -
                   static_cast<difference_type>(other.mPosition);
        }

      private:
        const Corkscrew *mCorkscrew;
        fl::size mPosition;
    };

    // Constructors that integrate input parameters directly
    // Primary constructor with default values for invert and gapParams
    Corkscrew(float totalTurns, fl::u16 numLeds, bool invert = false, const Gap& gapParams = Gap()) FL_NO_EXCEPT;
    
    // Constructor with external pixel buffer - these pixels will be drawn to directly
    Corkscrew(float totalTurns, fl::span<CRGB> dstPixels, bool invert = false, const Gap& gapParams = Gap()) FL_NO_EXCEPT;
    
    Corkscrew(const Corkscrew &) FL_NO_EXCEPT = default;
    Corkscrew(Corkscrew &&) FL_NO_EXCEPT = default;

    // Caching control
    void setCachingEnabled(bool enabled) FL_NO_EXCEPT;

    // Essential API - Core functionality
    fl::u16 cylinderWidth() const FL_NO_EXCEPT { return mWidth; }
    fl::u16 cylinderHeight() const FL_NO_EXCEPT { return mHeight; }

    // Enhanced surface handling with shared_ptr
    // Note: Input surface will be created on first call
    fl::shared_ptr<fl::Grid<CRGB>>& getOrCreateInputSurface() FL_NO_EXCEPT;
    
    // Draw like a regular rectangle surface - access input surface directly
    fl::Grid<CRGB>& surface() FL_NO_EXCEPT;

    // Draw the corkscrew by reading from the internal surface and populating LED pixels
    void draw(bool use_multi_sampling = true) FL_NO_EXCEPT;

    // Pixel storage access - works with both external and owned pixels
    // This represents the pixels that will be drawn after draw() is called
    CRGB* rawData() FL_NO_EXCEPT;
    
    // Returns span of pixels that will be written to when draw() is called
    fl::span<CRGB> data() FL_NO_EXCEPT;
    
    fl::size pixelCount() const FL_NO_EXCEPT;
    // Create and return a fully constructed ScreenMap for this corkscrew
    // Each LED index will be mapped to its exact position on the cylindrical surface
    fl::ScreenMap toScreenMap(float diameter = 0.5f) const FL_NO_EXCEPT;

    // STL-style container interface
    fl::size size() const FL_NO_EXCEPT;
    iterator begin() FL_NO_EXCEPT { return iterator(this, 0); }
    iterator end() FL_NO_EXCEPT { return iterator(this, size()); }

    // Non-essential API - Lower level access
    vec2f at_no_wrap(fl::u16 i) const FL_NO_EXCEPT;
    vec2f at_exact(fl::u16 i) const FL_NO_EXCEPT;
    Tile2x2_u8_wrap at_wrap(float i) const FL_NO_EXCEPT;

    // Clear all buffers and free memory
    void clear() FL_NO_EXCEPT;
    
    // Fill the input surface with a color
    void fillInputSurface(const CRGB& color) FL_NO_EXCEPT;

  private:
    // For internal use. Splats the pixel on the surface which
    // extends past the width. This extended Tile2x2 is designed
    // to be wrapped around with a Tile2x2_u8_wrap.
    Tile2x2_u8 at_splat_extrapolate(float i) const FL_NO_EXCEPT;

    // Read from fl::Grid<CRGB> object and populate our internal rectangular buffer
    // by sampling from the XY coordinates mapped to each corkscrew LED position
    // use_multi_sampling = true will use multi-sampling to sample from the source grid,
    // this will give a little bit better accuracy and the screenmap will be more accurate.
    void readFrom(const fl::Grid<CRGB>& source_grid, bool use_multi_sampling = true) FL_NO_EXCEPT;
    
    // Read from rectangular buffer using multi-sampling and store in target grid
    // Uses Tile2x2_u8_wrap for sub-pixel accurate sampling with proper blending
    void readFromMulti(const fl::Grid<CRGB>& target_grid) const FL_NO_EXCEPT;
    
    // Initialize the rectangular buffer if not already done
    void initializeBuffer() const FL_NO_EXCEPT;
    
    // Initialize the cache if not already done and caching is enabled
    void initializeCache() const FL_NO_EXCEPT;
    
    // Calculate the tile at position i without using cache
    Tile2x2_u8_wrap calculateTileAtWrap(float i) const FL_NO_EXCEPT;

    // Core corkscrew parameters (moved from CorkscrewInput)
    float mTotalTurns = 19.0f;   // Total turns of the corkscrew
    fl::u16 mNumLeds = 144;      // Number of LEDs
    Gap mGapParams;              // Gap parameters for gap accounting  
    bool mInvert = false;        // If true, reverse the mapping order
    
    // Cylindrical mapping dimensions (moved from CorkscrewState)
    fl::u16 mWidth = 0;          // Width of cylindrical map (circumference of one turn)
    fl::u16 mHeight = 0;         // Height of cylindrical map (total vertical segments)
    
    // Enhanced pixel storage - variant supports both external and owned pixels
    PixelStorage mPixelStorage;
    bool mOwnsPixels = false; // Track whether we own the pixel data
    
    // Input surface for drawing operations
    fl::shared_ptr<fl::Grid<CRGB>> mInputSurface;
    
    // Caching for Tile2x2_u8_wrap objects
    mutable fl::vector<Tile2x2_u8_wrap> mTileCache;
    mutable bool mCacheInitialized = false;
    bool mCachingEnabled = true; // Default to enabled
};

} // namespace fl
