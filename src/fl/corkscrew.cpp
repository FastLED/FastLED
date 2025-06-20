#include "fl/corkscrew.h"
#include "fl/algorithm.h"
#include "fl/assert.h"
#include "fl/math.h"
#include "fl/splat.h"
#include "fl/warn.h"
#include "fl/tile2x2.h"
#include "fl/math_macros.h"
#include "fl/unused.h"
#include "fl/map_range.h"
#include "fl/leds.h"
#include "fl/grid.h"

#define TWO_PI (PI * 2.0)

namespace fl {

namespace {

// New helper function to calculate individual LED position
vec2f calculateLedPositionExtended(uint16_t ledIndex, uint16_t numLeds, float totalTurns, float offsetCircumference, uint16_t width, uint16_t height) {
    // Calculate position along the corkscrew (0.0 to 1.0)
    FL_UNUSED(totalTurns);
    FL_UNUSED(offsetCircumference);
    FL_UNUSED(height);

    const float ledProgress = static_cast<float>(ledIndex) / static_cast<float>(numLeds - 1);
    
    // Calculate row (turn) using integer division
    const uint16_t row = ledIndex / width;
    
    // Calculate remainder as position within the row
    const uint16_t remainder = ledIndex % width;
    
    // Convert remainder to alpha value (0.0 to 1.0) against width
    const float alpha = static_cast<float>(remainder) / static_cast<float>(width);
    
    // Width position uses original calculation
    const float width_pos = ledProgress * numLeds;
    
    // Height is the row number plus the alpha interpolation
    const float height_pos = static_cast<float>(row) + alpha;
    
    return vec2f(width_pos, height_pos);
}

void generateState(const Corkscrew::Input &input, CorkscrewState *output) {
    output->width = input.calculateWidth();
    output->height = input.calculateHeight();
    // No longer pre-calculating mapping - positions computed on-the-fly
}
}  // namespace


Corkscrew::Corkscrew(const Corkscrew::Input &input) : mInput(input) {
    fl::generateState(mInput, &mState);
}

vec2f Corkscrew::at_exact(uint16_t i) const {
    if (i >= mInput.numLeds) {
        // Handle out-of-bounds access, possibly by returning a default value
        return vec2f(0, 0);
    }
    
    // Compute position on-the-fly
    vec2f position = calculateLedPositionExtended(i, mInput.numLeds, mInput.totalTurns, 
                                         mInput.offsetCircumference, mState.width, mState.height);
    
    // // Apply inversion if requested
    // if (mInput.invert) {
    //     uint16_t invertedIndex = mInput.numLeds - 1 - i;
    //     position = calculateLedPositionExtended(invertedIndex, mInput.numLeds, mInput.totalTurns, 
    //                                    mInput.offsetCircumference, mState.width, mState.height);
    // }

    // now wrap the x-position
    //position.x = fmodf(position.x, static_cast<float>(mState.width));
    
    return position;
}


Tile2x2_u8 Corkscrew::at_splat_extrapolate(float i) const {
    if (i >= mInput.numLeds) {
        // Handle out-of-bounds access, possibly by returning a default
        // Tile2x2_u8
        FASTLED_ASSERT(false, "Out of bounds access in Corkscrew at_splat: "
                                  << i << " size: " << mInput.numLeds);
        return Tile2x2_u8();
    }
    
    // Use the splat function to convert the vec2f to a Tile2x2_u8
    float i_floor = floorf(i);
    float i_ceil = ceilf(i);
    if (ALMOST_EQUAL_FLOAT(i_floor, i_ceil)) {
        // If the index is the same, just return the splat of that index
        vec2f position = at_exact(static_cast<uint16_t>(i_floor));
        return splat(position);
    } else {
        // Interpolate between the two points and return the splat of the result
        vec2f pos1 = at_exact(static_cast<uint16_t>(i_floor));
        vec2f pos2 = at_exact(static_cast<uint16_t>(i_ceil));
        float t = i - i_floor;
        vec2f interpolated_pos = map_range(t, 0.0f, 1.0f, pos1, pos2);
        return splat(interpolated_pos);
    }
}

size_t Corkscrew::size() const { return mInput.numLeds; }

Corkscrew::State Corkscrew::generateState(const Corkscrew::Input &input) {
    CorkscrewState output;
    fl::generateState(input, &output);
    return output;
}

Tile2x2_u8_wrap Corkscrew::at_wrap(float i) const {
    // This is a splatted pixel, but wrapped around the cylinder.
    // This is useful for rendering the corkscrew in a cylindrical way.
    Tile2x2_u8 tile = at_splat_extrapolate(i);
    Tile2x2_u8_wrap::Entry data[2][2];
    vec2i16 origin = tile.origin();
    for (uint8_t x = 0; x < 2; x++) {
        for (uint8_t y = 0; y < 2; y++) {
            // For each pixel in the tile, map it to the cylinder so that each subcomponent
            // is mapped to the correct position on the cylinder.
            vec2i16 pos = origin + vec2i16(x, y);
            // now wrap the x-position
            pos.x = fmodf(pos.x, static_cast<float>(mState.width));
            data[x][y] = {pos, tile.at(x, y)};
        }
    }
    return Tile2x2_u8_wrap(data);
}

// New rectangular buffer functionality implementation

void Corkscrew::initializeBuffer() const {
    if (!mBufferInitialized) {
        size_t buffer_size = static_cast<size_t>(mState.width) * static_cast<size_t>(mState.height);
        mCorkscrewLeds.resize(buffer_size, CRGB::Black);
        mBufferInitialized = true;
    }
}

fl::vector<CRGB>& Corkscrew::getBuffer() {
    initializeBuffer();
    return mCorkscrewLeds;
}

const fl::vector<CRGB>& Corkscrew::getBuffer() const {
    initializeBuffer();
    return mCorkscrewLeds;
}

CRGB* Corkscrew::data() {
    initializeBuffer();
    return mCorkscrewLeds.data();
}

const CRGB* Corkscrew::data() const {
    initializeBuffer();
    return mCorkscrewLeds.data();
}

void Corkscrew::readFrom(const fl::Grid<CRGB>& source_grid) {
    // Initialize the buffer if not already done
    initializeBuffer();
    
    // Clear buffer first
    clearBuffer();
    
    // Iterate through each LED in the corkscrew
    for (size_t led_idx = 0; led_idx < mInput.numLeds; ++led_idx) {
        // Get the rectangular coordinates for this corkscrew LED
        vec2f rect_pos = at_exact(static_cast<uint16_t>(led_idx));
        
        // Convert to integer coordinates for indexing
        vec2i16 coord(static_cast<int16_t>(rect_pos.x + 0.5f), 
                      static_cast<int16_t>(rect_pos.y + 0.5f));
        
        // Clamp coordinates to grid bounds
        coord.x = MAX(0, MIN(coord.x, static_cast<int16_t>(source_grid.width()) - 1));
        coord.y = MAX(0, MIN(coord.y, static_cast<int16_t>(source_grid.height()) - 1));
        
        // Sample from the source fl::Grid using its at() method
        CRGB sampled_color = source_grid.at(coord.x, coord.y);
        
        // Store in our rectangular buffer using row-major indexing
        size_t buffer_idx = static_cast<size_t>(coord.y) * static_cast<size_t>(mState.width) + static_cast<size_t>(coord.x);
        
        if (buffer_idx < mCorkscrewLeds.size()) {
            mCorkscrewLeds[buffer_idx] = sampled_color;
        }
    }
}

void Corkscrew::clearBuffer() {
    initializeBuffer();
    for (size_t i = 0; i < mCorkscrewLeds.size(); ++i) {
        mCorkscrewLeds[i] = CRGB::Black;
    }
}

void Corkscrew::fillBuffer(const CRGB& color) {
    initializeBuffer();
    for (size_t i = 0; i < mCorkscrewLeds.size(); ++i) {
        mCorkscrewLeds[i] = color;
    }
}

void Corkscrew::readFromMulti(const fl::Grid<CRGB>& target_grid) const {
    // Ensure buffer is initialized
    initializeBuffer();
    
    // Iterate through each LED in the corkscrew
    for (size_t led_idx = 0; led_idx < mInput.numLeds; ++led_idx) {
        // Get the wrapped tile for this LED position
        Tile2x2_u8_wrap tile = at_wrap(static_cast<float>(led_idx));
        
        // Accumulate color from the 4 sample points with their weights
        uint32_t r_accum = 0, g_accum = 0, b_accum = 0;
        uint32_t total_weight = 0;
        
        // Sample from each of the 4 corners of the tile
        for (uint8_t x = 0; x < 2; x++) {
            for (uint8_t y = 0; y < 2; y++) {
                const auto& entry = tile.at(x, y);
                vec2i16 pos = entry.first;   // position is the first element of the pair
                uint8_t weight = entry.second; // weight is the second element of the pair
                
                // Bounds check for the rectangular buffer
                if (pos.x >= 0 && pos.x < static_cast<int16_t>(mState.width) && 
                    pos.y >= 0 && pos.y < static_cast<int16_t>(mState.height)) {
                    
                    // Calculate buffer index using row-major ordering
                    size_t buffer_idx = static_cast<size_t>(pos.y) * static_cast<size_t>(mState.width) + static_cast<size_t>(pos.x);
                    
                    if (buffer_idx < mCorkscrewLeds.size()) {
                        CRGB sample_color = mCorkscrewLeds[buffer_idx];
                        
                        // Accumulate weighted color components
                        r_accum += static_cast<uint32_t>(sample_color.r) * weight;
                        g_accum += static_cast<uint32_t>(sample_color.g) * weight;
                        b_accum += static_cast<uint32_t>(sample_color.b) * weight;
                        total_weight += weight;
                    }
                }
            }
        }
        
        // Calculate final color by dividing by total weight
        CRGB final_color = CRGB::Black;
        if (total_weight > 0) {
            final_color.r = static_cast<uint8_t>(r_accum / total_weight);
            final_color.g = static_cast<uint8_t>(g_accum / total_weight);
            final_color.b = static_cast<uint8_t>(b_accum / total_weight);
        }
        
        // Store the result in the target grid (using linear indexing)
        if (led_idx < target_grid.size()) {
            mCorkscrewLeds[led_idx] = final_color;
        }
    }
}

// Iterator implementation
vec2f CorkscrewState::iterator::operator*() const {
    return corkscrew_->at_exact(static_cast<uint16_t>(position_));
}

} // namespace fl