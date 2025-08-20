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

#include "fl/allocator.h"
#include "fl/geometry.h"
#include "fl/math.h"
#include "fl/math_macros.h"
#include "fl/pair.h"
#include "fl/tile2x2.h"
#include "fl/vector.h"
#include "fl/shared_ptr.h"
#include "fl/variant.h"
#include "fl/span.h"
#include "crgb.h"
#include "fl/int.h"

namespace fl {

// Forward declarations
class Leds;
class ScreenMap;
template<typename T> class Grid;

// Simple constexpr functions for compile-time corkscrew dimension calculation
constexpr fl::u16 calculateCorkscrewWidth(float totalTurns, fl::u16 numLeds) {
    return static_cast<fl::u16>(ceil_constexpr(static_cast<float>(numLeds) / totalTurns));
}

constexpr fl::u16 calculateCorkscrewHeight(float totalTurns, fl::u16 numLeds) {
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
    
    Gap() = default;
    Gap(float g) : num_leds(0), gap(g) {} // Backwards compatibility constructor
    Gap(int n, float g) : num_leds(n), gap(g) {} // New constructor with num_leds
    
    // Rule of 5 for POD data
    Gap(const Gap &other) = default;
    Gap &operator=(const Gap &other) = default;
    Gap(Gap &&other) noexcept = default;
    Gap &operator=(Gap &&other) noexcept = default;
};



// Maps a Corkscrew defined by the input to a cylindrical mapping for rendering
// a densly wrapped LED corkscrew.
class Corkscrew {
  public:
    
    // Pixel storage variants - can hold either external span or owned vector
    using PixelStorage = fl::Variant<fl::span<CRGB>, fl::vector<CRGB, fl::allocator_psram<CRGB>>>;

    // Iterator class moved from CorkscrewState
    class iterator {
      public:
        using value_type = vec2f;
        using difference_type = fl::i32;
        using pointer = vec2f *;
        using reference = vec2f &;

        iterator(const Corkscrew *corkscrew, fl::size position)
            : corkscrew_(corkscrew), position_(position) {}

        vec2f operator*() const;

        iterator &operator++() {
            ++position_;
            return *this;
        }

        iterator operator++(int) {
            iterator temp = *this;
            ++position_;
            return temp;
        }

        iterator &operator--() {
            --position_;
            return *this;
        }

        iterator operator--(int) {
            iterator temp = *this;
            --position_;
            return temp;
        }

        bool operator==(const iterator &other) const {
            return position_ == other.position_;
        }

        bool operator!=(const iterator &other) const {
            return position_ != other.position_;
        }

        difference_type operator-(const iterator &other) const {
            return static_cast<difference_type>(position_) -
                   static_cast<difference_type>(other.position_);
        }

      private:
        const Corkscrew *corkscrew_;
        fl::size position_;
    };

    // Constructors that integrate input parameters directly
    // Primary constructor with default values for invert and gapParams
    Corkscrew(float totalTurns, fl::u16 numLeds, bool invert = false, const Gap& gapParams = Gap());
    
    // Constructor with external pixel buffer - these pixels will be drawn to directly
    Corkscrew(float totalTurns, fl::span<CRGB> dstPixels, bool invert = false, const Gap& gapParams = Gap());
    
    
    Corkscrew(const Corkscrew &) = default;
    Corkscrew(Corkscrew &&) = default;


    // Caching control
    void setCachingEnabled(bool enabled);

    // Essential API - Core functionality
    fl::u16 cylinderWidth() const { return mWidth; }
    fl::u16 cylinderHeight() const { return mHeight; }

    // Enhanced surface handling with shared_ptr
    // Note: Input surface will be created on first call
    fl::shared_ptr<fl::Grid<CRGB>>& getOrCreateInputSurface();
    
    // Draw like a regular rectangle surface - access input surface directly
    fl::Grid<CRGB>& surface();

    
    // Draw the corkscrew by reading from the internal surface and populating LED pixels
    void draw(bool use_multi_sampling = true);

    // Pixel storage access - works with both external and owned pixels
    // This represents the pixels that will be drawn after draw() is called
    CRGB* rawData();
    
    // Returns span of pixels that will be written to when draw() is called
    fl::span<CRGB> data();
    
    fl::size pixelCount() const;
    // Create and return a fully constructed ScreenMap for this corkscrew
    // Each LED index will be mapped to its exact position on the cylindrical surface
    fl::ScreenMap toScreenMap(float diameter = 0.5f) const;

    // STL-style container interface
    fl::size size() const;
    iterator begin() { return iterator(this, 0); }
    iterator end() { return iterator(this, size()); }

    // Non-essential API - Lower level access
    vec2f at_no_wrap(fl::u16 i) const;
    vec2f at_exact(fl::u16 i) const;
    Tile2x2_u8_wrap at_wrap(float i) const;

    // Clear all buffers and free memory
    void clear();
    
    // Fill the input surface with a color
    void fillInputSurface(const CRGB& color);


  private:
    // For internal use. Splats the pixel on the surface which
    // extends past the width. This extended Tile2x2 is designed
    // to be wrapped around with a Tile2x2_u8_wrap.
    Tile2x2_u8 at_splat_extrapolate(float i) const;

    // Read from fl::Grid<CRGB> object and populate our internal rectangular buffer
    // by sampling from the XY coordinates mapped to each corkscrew LED position
    // use_multi_sampling = true will use multi-sampling to sample from the source grid,
    // this will give a little bit better accuracy and the screenmap will be more accurate.
    void readFrom(const fl::Grid<CRGB>& source_grid, bool use_multi_sampling = true);
    
    // Read from rectangular buffer using multi-sampling and store in target grid
    // Uses Tile2x2_u8_wrap for sub-pixel accurate sampling with proper blending
    void readFromMulti(const fl::Grid<CRGB>& target_grid) const;
    
    // Initialize the rectangular buffer if not already done
    void initializeBuffer() const;
    
    // Initialize the cache if not already done and caching is enabled
    void initializeCache() const;
    
    // Calculate the tile at position i without using cache
    Tile2x2_u8_wrap calculateTileAtWrap(float i) const;

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
