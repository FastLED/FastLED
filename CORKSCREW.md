# Corkscrew Pipeline: computeTile and multiSample Integration

## Overview

The FastLED corkscrew allows the user to write to a regular rectangular buffer and have it displayd on a dense corkscrew of LEDs.

Dense 144LED @ 3.28 are cheap and readily avialable. They are cheap, have high density.

## Pipeline Components

### 1. User Paints to XY Grid

Users create patterns on a 2D rectangular grid (`fl::Grid<CRGB>`) using standard XY coordinates.

```cpp
// Grid dimensions calculated from corkscrew parameters
uint16_t width = input.calculateWidth();   // LEDs per turn
uint16_t height = input.calculateHeight(); // Total vertical segments
fl::Grid<CRGB> sourceGrid(width, height);
```

### 2. LED Projection: Corkscrew → XY Grid

Each LED in the corkscrew has a corresponding floating-point position on the XY grid:

```cpp
// From corkscrew.cpp - calculateLedPositionExtended
vec2f calculateLedPosition(uint16_t ledIndex, uint16_t numLeds, uint16_t width) {
    const float ledProgress = static_cast<float>(ledIndex) / static_cast<float>(numLeds - 1);
    const uint16_t row = ledIndex / width;  // Which turn (vertical position)
    const uint16_t remainder = ledIndex % width;  // Position within turn
    const float alpha = static_cast<float>(remainder) / static_cast<float>(width);
    
    const float width_pos = ledProgress * numLeds;
    const float height_pos = static_cast<float>(row) + alpha;
    
    return vec2f(width_pos, height_pos);
}
```

### 3. computeTile: Splat Pixel Rendering

The `splat` function implements the "computeTile" concept by converting floating-point positions to `Tile2x2_u8` structures representing neighbor intensities:

```cpp
// From splat.cpp
Tile2x2_u8 splat(vec2f xy) {
    // 1) Get integer cell indices
    int16_t cx = static_cast<int16_t>(floorf(xy.x));
    int16_t cy = static_cast<int16_t>(floorf(xy.y));
    
    // 2) Calculate fractional offsets [0..1)
    float fx = xy.x - cx;
    float fy = xy.y - cy;
    
    // 3) Compute bilinear weights for 4 neighbors
    float w_ll = (1 - fx) * (1 - fy); // lower-left
    float w_lr = fx * (1 - fy);       // lower-right  
    float w_ul = (1 - fx) * fy;       // upper-left
    float w_ur = fx * fy;             // upper-right
    
    // 4) Build Tile2x2_u8 with weights as intensities [0..255]
    Tile2x2_u8 out(vec2<int16_t>(cx, cy));
    out.lower_left()  = to_uint8(w_ll);
    out.lower_right() = to_uint8(w_lr);
    out.upper_left()  = to_uint8(w_ul);
    out.upper_right() = to_uint8(w_ur);
    
    return out;
}
```

### 4. Tile2x2_u8: Neighbor Intensity Representation

The `Tile2x2_u8` structure represents the sampling strength from the four nearest neighbors:

```cpp
class Tile2x2_u8 {
    uint8_t mTile[2][2];        // 4 neighbor intensities [0..255]
    vec2<int16_t> mOrigin;      // Base grid coordinate (cx, cy)
    
    // Access methods for the 4 neighbors:
    uint8_t& lower_left();   // (0,0) - weight for pixel at (cx, cy)
    uint8_t& lower_right();  // (1,0) - weight for pixel at (cx+1, cy)  
    uint8_t& upper_left();   // (0,1) - weight for pixel at (cx, cy+1)
    uint8_t& upper_right();  // (1,1) - weight for pixel at (cx+1, cy+1)
};
```

### 5. Cylindrical Wrapping with Tile2x2_u8_wrap

For corkscrew mapping, the tile needs cylindrical wrapping:

```cpp
// From corkscrew.cpp - at_wrap()
Tile2x2_u8_wrap Corkscrew::at_wrap(float i) const {
    Tile2x2_u8 tile = at_splat_extrapolate(i);  // Get base tile
    Tile2x2_u8_wrap::Entry data[2][2];
    vec2i16 origin = tile.origin();
    
    for (uint8_t x = 0; x < 2; x++) {
        for (uint8_t y = 0; y < 2; y++) {
            vec2i16 pos = origin + vec2i16(x, y);
            // Apply cylindrical wrapping to x-coordinate
            pos.x = fmodf(pos.x, static_cast<float>(mState.width));
            data[x][y] = {pos, tile.at(x, y)};  // {position, intensity}
        }
    }
    return Tile2x2_u8_wrap(data);
}
```

### 6. multiSample: Weighted Color Sampling

The `readFromMulti` method implements the "multiSample" concept by using tile intensities to determine sampling strength:

```cpp
// From corkscrew.cpp - readFromMulti()
void Corkscrew::readFromMulti(const fl::Grid<CRGB>& source_grid) const {
    for (size_t led_idx = 0; led_idx < mInput.numLeds; ++led_idx) {
        // Get wrapped tile for this LED position
        Tile2x2_u8_wrap tile = at_wrap(static_cast<float>(led_idx));
        
        uint32_t r_accum = 0, g_accum = 0, b_accum = 0;
        uint32_t total_weight = 0;
        
        // Sample from each of the 4 neighbors
        for (uint8_t x = 0; x < 2; x++) {
            for (uint8_t y = 0; y < 2; y++) {
                const auto& entry = tile.at(x, y);
                vec2i16 pos = entry.first;      // Grid position
                uint8_t weight = entry.second;  // Sampling intensity [0..255]
                
                if (inBounds(source_grid, pos)) {
                    CRGB sample_color = source_grid.at(pos.x, pos.y);
                    
                    // Weighted accumulation
                    r_accum += static_cast<uint32_t>(sample_color.r) * weight;
                    g_accum += static_cast<uint32_t>(sample_color.g) * weight;
                    b_accum += static_cast<uint32_t>(sample_color.b) * weight;
                    total_weight += weight;
                }
            }
        }
        
        // Final color = weighted average
        CRGB final_color = CRGB::Black;
        if (total_weight > 0) {
            final_color.r = static_cast<uint8_t>(r_accum / total_weight);
            final_color.g = static_cast<uint8_t>(g_accum / total_weight);
            final_color.b = static_cast<uint8_t>(b_accum / total_weight);
        }
        
        mCorkscrewLeds[led_idx] = final_color;
    }
}
```

## Complete Pipeline Flow

```
1. User draws → XY Grid (CRGB values at integer coordinates)
                    ↓
2. LED projection → vec2f position on grid (floating-point)
                    ↓  
3. computeTile    → Tile2x2_u8 (4 neighbor intensities)
   (splat)           ↓
4. Wrap for       → Tile2x2_u8_wrap (cylindrical coordinates + intensities)
   cylinder          ↓
5. multiSample    → Weighted sampling from 4 neighbors
   (readFromMulti)   ↓
6. Final LED color → CRGB value for corkscrew LED
```

## Key Insights

### Sub-Pixel Accuracy
The system achieves sub-pixel accuracy by:
- Using floating-point LED positions on the grid
- Converting to bilinear weights for 4 nearest neighbors
- Performing weighted color sampling instead of nearest-neighbor

### Cylindrical Mapping
- X-coordinates wrap around the cylinder circumference
- Y-coordinates represent vertical position along the helix
- Width = LEDs per turn, Height = total vertical segments

### Anti-Aliasing
The weighted sampling naturally provides anti-aliasing:
- Sharp grid patterns become smoothly interpolated on the corkscrew
- Reduces visual artifacts from the discrete→continuous→discrete mapping

## Performance Characteristics

- **Memory**: O(W×H) for grid (O(N) for corkscrew LEDs where O(N) <= O(WxH) == O(WxW))
- **Computation**: O(N) with 4 samples per LED (constant factor)
- **Quality**: Sub-pixel accurate with built-in anti-aliasing


## Future Work

Often led strips are soldered together. These leaves a gap between the other
leds on the strip. This gab should be accounted for to maximize spatial accuracy with rendering straight lines (e.g. text).
