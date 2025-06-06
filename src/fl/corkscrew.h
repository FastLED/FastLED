#pragma once

/**
 * @file corkscrew.h
 * @brief Corkscrew projection utilities
 *
 * Corkscrew projection maps from Corkscrew (θ, h) to Cylindrical cartesian (w,
 * h) space, where w = one turn of the Corkscrew. The corkscrew at (0,0) will
 * map to (0,0) in cylindrical space.
 *
 * The projection:
 * - Super samples cylindrical space?
 * - θ is normalized to [0, 1] or mapped to [0, W-1] for grid projection
 * - Uses 2x2 super sampling for better visual quality
 * - Works with XYPathRenderer's "Splat Rendering" for sub-pixel rendering
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
#include "fl/tile2x2.h"
#include "fl/vector.h"

namespace fl {

/**
 * Generates a mapping from corkscrew to cylindrical coordinates
 * @param input The input parameters defining the corkscrew.
 * @return The resulting cylindrical mapping.
 */
struct CorkscrewInput {
    float totalHeight = 23.25; // Total height of the corkscrew in centimeters
                               // for 144 densly wrapped up over 19 turns
    float totalTurns = 19.f;   // Default to 19 turns
    float offsetCircumference = 0; // Optional offset for gap accounting
    uint16_t numLeds = 144;        // Default to dense 144 leds.
    bool invert = false;           // If true, reverse the mapping order
    CorkscrewInput() = default;
    CorkscrewInput(float height, float total_turns, float offset = 0,
                   uint16_t leds = 144, bool invertMapping = false)
        : totalHeight(height), totalTurns(total_turns),
          offsetCircumference(offset), numLeds(leds), invert(invertMapping) {}
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

        bool operator==(const iterator &other) const {
            return position_ == other.position_;
        }

        bool operator!=(const iterator &other) const {
            return position_ != other.position_;
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

    vec2f at(uint16_t i) const;
    // This is a splatted pixel. This is will look way better than
    // using at(), because it uses 2x2 neighboor sampling.
    Tile2x2_u8 at_splat(uint16_t i) const;
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
    Input mInput; // The input parameters defining the corkscrew
    State mState; // The resulting cylindrical mapping
};

} // namespace fl
