#pragma once

/**
 * @file corkscrew.h
 * @brief Corkscrew projection utilities
 *
 * Provides math to project a cork screw onto a rectangular surface.
 * This allows drawing into a normal rectangle, and then have it mapped
 * correctly to the corkscrew.
 * 
 * Construction:
 * Take a stick and wrap as dense as possible. Measure the number of turns.
 * That's it.
 *
 * Corkscrew will provide you a width and height for the rectangular grid. There
 * are constexpr functions to calculate the width and height so you can statically
 * allocate the CRGB buffer to draw into.
 *
 *
 *
 * Inputs:
 * - Total Height of the Corkscrew in centimeters
 * - Total angle of the Corkscrew (number of veritcal segments × 2π)
 * - Optional offset circumference (default 0)
 *   - Allows pixel-perfect corkscrew with gaps via circumference offsetting
 *   - Example (accounting for gaps):
 *     - segment 0: offset circumference = 0, circumference = 100
 *     - segment 1: offset circumference = 100.5, circumference = 100
 *     - segment 2: offset circumference = 101, circumference = 100
 *
 * Outputs:
 * - Width and Height of the cylindrical map
 *   - Width is the circumference of one turn
 *   - Height is the total number of vertical segments
 * - Vector of vec2f {width, height} mapping corkscrew (r,c) to cylindrical
 * {w,h}
 */

#include "fl/allocator.h"
#include "fl/geometry.h"
#include "fl/math.h"
#include "fl/math_macros.h"
#include "fl/pair.h"
#include "fl/tile2x2.h"
#include "fl/vector.h"
#include "crgb.h"

namespace fl {

// Forward declaration
class Leds;

// Simple constexpr functions for compile-time corkscrew dimension calculation
constexpr uint16_t calculateCorkscrewWidth(float totalTurns, uint16_t numLeds) {
    return static_cast<uint16_t>(ceil_constexpr(static_cast<float>(numLeds) / totalTurns));
}

constexpr uint16_t calculateCorkscrewHeight(float totalTurns, uint16_t numLeds) {
    return (calculateCorkscrewWidth(totalTurns, numLeds) * static_cast<int>(ceil_constexpr(totalTurns)) > numLeds) ?
        static_cast<uint16_t>(ceil_constexpr(static_cast<float>(numLeds) / static_cast<float>(calculateCorkscrewWidth(totalTurns, numLeds)))) :
        static_cast<uint16_t>(ceil_constexpr(totalTurns));
}

/**
 * Generates a mapping from corkscrew to cylindrical coordinates
 * @param input The input parameters defining the corkscrew.
 * @return The resulting cylindrical mapping.
 */
struct CorkscrewInput {
    float totalTurns = 19.f;   // Default to 19 turns
    float offsetCircumference = 0; // Optional offset for gap accounting
    uint16_t numLeds = 144;        // Default to dense 144 leds.
    bool invert = false;           // If true, reverse the mapping order
    
    CorkscrewInput() = default;
    
    // Constructor with turns and LEDs
    CorkscrewInput(float total_turns, uint16_t leds, float offset = 0,
                   bool invertMapping = false)
        : totalTurns(total_turns), offsetCircumference(offset), numLeds(leds),
          invert(invertMapping) {}
    
    // Calculate optimal width and height based on number of turns and LEDs
    uint16_t calculateWidth() const {
        // Width = LEDs per turn
        float ledsPerTurn = static_cast<float>(numLeds) / totalTurns;
        return static_cast<uint16_t>(fl::ceil(ledsPerTurn));
    }
    
    uint16_t calculateHeight() const {
        // Calculate optimal height to minimize empty pixels
        uint16_t width = calculateWidth();
        uint16_t height_from_turns = static_cast<uint16_t>(fl::ceil(totalTurns));
        
        // If the grid would have more pixels than LEDs, adjust height to better match
        if (width * height_from_turns > numLeds) {
            // Calculate height that better matches LED count
            return static_cast<uint16_t>(fl::ceil(static_cast<float>(numLeds) / static_cast<float>(width)));
        }
        
        return height_from_turns;
    }
};

struct CorkscrewState {
    uint16_t width = 0;  // Width of cylindrical map (circumference of one turn)
    uint16_t height = 0; // Height of cylindrical map (total vertical segments)
    // Removed mapping vector - positions now computed on-the-fly
    CorkscrewState() = default;

    class iterator {
      public:
        using value_type = vec2f;
        using difference_type = int32_t;
        using pointer = vec2f *;
        using reference = vec2f &;

        iterator(const class Corkscrew *corkscrew, size_t position)
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
        const class Corkscrew *corkscrew_;
        size_t position_;
    };
};

// Maps a Corkscrew defined by the input to a cylindrical mapping for rendering
// a densly wrapped LED corkscrew.
class Corkscrew {
  public:
    using Input = CorkscrewInput;
    using State = CorkscrewState;
    using iterator = CorkscrewState::iterator;

    Corkscrew(const Input &input);
    Corkscrew(const Corkscrew &) = default;
    Corkscrew(Corkscrew &&) = default;

    vec2f at_exact(uint16_t i) const;

    // This is the future api.
    Tile2x2_u8_wrap at_wrap(float i) const;
    size_t size() const;
    iterator begin() { return iterator(this, 0); }
    iterator end() { return iterator(this, size()); }

    int16_t cylinder_width() const { return mState.width; }
    int16_t cylinder_height() const { return mState.height; }

    // New functionality: rectangular buffer and drawing
    // Get access to the rectangular buffer (lazily initialized)
    fl::vector<CRGB>& getBuffer();
    const fl::vector<CRGB>& getBuffer() const;
    
    // Read from fl::Leds object and populate our internal rectangular buffer
    // by sampling from the XY coordinates mapped to each corkscrew LED position
    void readFrom(const fl::Leds& source_leds);
    
    // Clear the rectangular buffer 
    void clearBuffer();
    
    // Fill the rectangular buffer with a color
    void fillBuffer(const CRGB& color);

    /// For testing
    static State generateState(const Input &input);

  private:
    // For internal use. Splats the pixel on the surface which
    // extends past the width. This extended Tile2x2 is designed
    // to be wrapped around with a Tile2x2_u8_wrap.
    Tile2x2_u8 at_splat_extrapolate(float i) const;
    
    // Initialize the rectangular buffer if not already done
    void initializeBuffer() const;

    Input mInput; // The input parameters defining the corkscrew
    State mState; // The resulting cylindrical mapping
    
    // Rectangular buffer for drawing (lazily initialized)
    mutable fl::vector<CRGB> mRectangularBuffer;
    mutable bool mBufferInitialized = false;
};

} // namespace fl
