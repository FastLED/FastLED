#pragma once

/**
 * @file corkscrew.h
 * @brief Corkscrew projection utilities
 *
 * You want to draw on a rectangular surface, and have it map to a GOD DAMN
 * CORKSCREW! Well guess what, this is the file for you.
 *
 * Corkscrew projection maps from Corkscrew angle height, (θ, h)  to Cylindrical
 * cartesian (w, h) space, where w = one turn of the Corkscrew. The corkscrew at
 * (0) will map to the first index in the cylinder map at (0, 0). The last value
 * is probly not at the max pixel value at (width - 1, height - 1), but could
 * be.
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
#include "fl/math_macros.h"
#include "fl/pair.h"
#include "fl/tile2x2.h"
#include "fl/vector.h"

namespace fl {

/**
 * Generates a mapping from corkscrew to cylindrical coordinates
 * @param input The input parameters defining the corkscrew.
 * @return The resulting cylindrical mapping.
 */
struct CorkscrewInput {
    float totalLength = 100;   // Total length of the corkscrew in centimeters,
                               // set to dense 144 strips.
    float totalHeight = 23.25; // Total height of the corkscrew in centimeters
                               // for 144 densly wrapped up over 19 turns
    float totalTurns = 19.f;   // Default to 19 turns
    float offsetCircumference = 0; // Optional offset for gap accounting
    uint16_t numLeds = 144;        // Default to dense 144 leds.
    bool invert = false;           // If true, reverse the mapping order
    CorkscrewInput() = default;
    CorkscrewInput(float total_length, float height, float total_turns,
                   uint16_t leds, float offset = 0,
                   bool invertMapping = false)
        : totalLength(total_length), totalHeight(height),
          totalTurns(total_turns), offsetCircumference(offset), numLeds(leds),
          invert(invertMapping) {}
};

struct CorkscrewState {
    uint16_t width = 0;  // Width of cylindrical map (circumference of one turn)
    uint16_t height = 0; // Height of cylindrical map (total vertical segments)
    fl::vector<fl::vec2f, fl::allocator_psram<fl::vec2f>>
        mapping; // Full precision mapping from corkscrew to cylindrical
    CorkscrewState() = default;

    class iterator {
      public:
        using value_type = vec2f;
        using difference_type = int32_t;
        using pointer = vec2f *;
        using reference = vec2f &;

        iterator(CorkscrewState *owner, size_t position)
            : owner_(owner), position_(position) {}

        vec2f &operator*() const { return owner_->mapping[position_]; }

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
        CorkscrewState *owner_;
        size_t position_;
    };

    iterator begin() { return iterator(this, 0); }

    iterator end() { return iterator(this, mapping.size()); }
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

    iterator begin() { return mState.begin(); }

    iterator end() { return mState.end(); }

    /// For testing

    static State generateState(const Input &input);

    State &access() { return mState; }

    const State &access() const { return mState; }

    int16_t cylinder_width() const { return mState.width; }
    int16_t cylinder_height() const { return mState.height; }

  private:
    // For internal use. Splats the pixel on the surface which
    // extends past the width. This extended Tile2x2 is designed
    // to be wrapped around with a Tile2x2_u8_wrap.
    Tile2x2_u8 at_splat_extrapolate(float i) const;

    Input mInput; // The input parameters defining the corkscrew
    State mState; // The resulting cylindrical mapping
};

} // namespace fl
