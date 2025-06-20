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
        mRectangularBuffer.resize(buffer_size, CRGB::Black);
        mBufferInitialized = true;
    }
}

fl::vector<CRGB>& Corkscrew::getBuffer() {
    initializeBuffer();
    return mRectangularBuffer;
}

const fl::vector<CRGB>& Corkscrew::getBuffer() const {
    initializeBuffer();
    return mRectangularBuffer;
}

void Corkscrew::draw(CRGB* target_leds) const {
    if (!mBufferInitialized || mRectangularBuffer.empty()) {
        // If buffer not initialized or empty, fill target with black
        for (size_t i = 0; i < mInput.numLeds; ++i) {
            target_leds[i] = CRGB::Black;
        }
        return;
    }
    
    // Iterate through each LED in the corkscrew and sample from rectangular buffer
    for (size_t led_idx = 0; led_idx < mInput.numLeds; ++led_idx) {
        // Get the rectangular coordinates for this corkscrew LED
        vec2f rect_pos = at_exact(static_cast<uint16_t>(led_idx));
        
        // Convert to integer coordinates for sampling (using simple rounding)
        int x = static_cast<int>(rect_pos.x + 0.5f);
        int y = static_cast<int>(rect_pos.y + 0.5f);
        
        // Clamp coordinates to buffer bounds
        x = MAX(0, MIN(x, static_cast<int>(mState.width) - 1));
        y = MAX(0, MIN(y, static_cast<int>(mState.height) - 1));
        
        // Sample from rectangular buffer using row-major indexing
        size_t buffer_idx = static_cast<size_t>(y) * static_cast<size_t>(mState.width) + static_cast<size_t>(x);
        
        if (buffer_idx < mRectangularBuffer.size()) {
            target_leds[led_idx] = mRectangularBuffer[buffer_idx];
        } else {
            target_leds[led_idx] = CRGB::Black;
        }
    }
}

void Corkscrew::clearBuffer() {
    initializeBuffer();
    for (size_t i = 0; i < mRectangularBuffer.size(); ++i) {
        mRectangularBuffer[i] = CRGB::Black;
    }
}

void Corkscrew::fillBuffer(const CRGB& color) {
    initializeBuffer();
    for (size_t i = 0; i < mRectangularBuffer.size(); ++i) {
        mRectangularBuffer[i] = color;
    }
}

// Iterator implementation
vec2f CorkscrewState::iterator::operator*() const {
    return corkscrew_->at_exact(static_cast<uint16_t>(position_));
}

} // namespace fl