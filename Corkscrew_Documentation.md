# FastLED Corkscrew Documentation

## Basic Theory

The FastLED Corkscrew system is a mathematical mapping utility that projects helical (spiral) LED arrangements onto 2D rectangular coordinate systems. This allows developers to draw patterns using familiar 2D graphics techniques while ensuring the results display correctly on physically wrapped LED strips.

### Core Concept

When LEDs are wrapped around a cylindrical object in a spiral pattern (like a corkscrew), they form a 3D helical structure. The Corkscrew class provides:

1. **Forward Mapping**: Convert LED index → 2D cylindrical coordinates
2. **Reverse Mapping**: Draw on 2D grid → Map to LED positions
3. **Cylindrical Projection**: Handle wraparound at cylinder edges
4. **Sub-pixel Rendering**: Smooth interpolation between LED positions

### Mathematical Foundation

The corkscrew mapping is based on cylindrical coordinates where:
- **Width**: Circumference of one complete turn (LEDs per turn)
- **Height**: Total number of vertical segments (turns)
- **Mapping Function**: Projects spiral position to rectangular grid

```cpp
// Core calculation for LED position
float ledProgress = static_cast<float>(ledIndex) / static_cast<float>(numLeds - 1);
uint16_t row = ledIndex / width;                    // Which turn
uint16_t remainder = ledIndex % width;              // Position within turn
float alpha = static_cast<float>(remainder) / static_cast<float>(width);
float width_pos = ledProgress * numLeds;
float height_pos = static_cast<float>(row) + alpha;
```

### Key Parameters

- **Total Turns**: Number of complete spiral rotations
- **Number of LEDs**: Total LEDs in the strip
- **Offset Circumference**: Accounts for gaps between segments
- **Width/Height**: Auto-calculated optimal rectangular dimensions

### Coordinate Systems

1. **LED Index Space**: Linear (0 to numLeds-1)
2. **Cylindrical Space**: 2D coordinates (x, y) on unwrapped cylinder
3. **Wrapped Space**: Cylindrical coordinates with x-axis wrapping
4. **Physical Space**: Actual 3D positions on the helical structure

## Use Cases

### 1. Festival Sticks and Wearable LEDs

**Problem**: Traditional LED strips arranged in spirals display 2D patterns incorrectly due to the helical geometry.

**Solution**: Draw on a rectangular canvas, then use Corkscrew to map the pattern to the actual LED positions.

```cpp
// Festival stick with 288 LEDs in 19.25 turns
Corkscrew::Input input(19.25f, 288, 0);
Corkscrew corkscrew(input);

// Draw on rectangular grid
fl::Grid<CRGB> canvas(corkscrew.cylinder_width(), corkscrew.cylinder_height());
// ... draw patterns on canvas ...

// Map to corkscrew LEDs
corkscrew.readFrom(canvas);
```

### 2. Web Visualization and Control Interfaces

**Problem**: Web interfaces need to display the actual 3D shape of LED installations.

**Solution**: Use `toScreenMap()` to create accurate position mappings for web display.

```cpp
// Create ScreenMap for web interface
fl::ScreenMap screenMap = corkscrew.toScreenMap(0.5f); // 0.5cm LED diameter
controller->setScreenMap(screenMap);
```

### 3. Cylindrical Noise Patterns

**Problem**: Standard Perlin noise doesn't wrap properly around cylindrical structures.

**Solution**: Generate noise using cylindrical coordinates for seamless wrapping.

```cpp
// Generate cylindrical noise that wraps properly
for(int x = 0; x < width; x++) {
    for(int y = 0; y < height; y++) {
        float angle = (float(x) / float(width)) * 2.0f * PI;
        float noise_x_cyl = cos(angle) * cylinder_radius;
        float noise_y_cyl = sin(angle) * cylinder_radius;
        uint8_t noise_val = inoise8(noise_x_cyl, noise_y_cyl, time_offset);
        // Map noise to color and store in grid
    }
}
```

### 4. Sub-pixel Animation and Smooth Movement

**Problem**: Moving objects along the corkscrew need smooth interpolation between LED positions.

**Solution**: Use `at_wrap()` with floating-point positions for multi-sampling.

```cpp
// Smooth movement with sub-pixel rendering
float position = 0.0f;
position += speed * deltaTime;

Tile2x2_u8_wrap tile = corkscrew.at_wrap(position);
// Draw with alpha blending across 2x2 tile
```

### 5. Pattern Scaling and Aspect Ratio Correction

**Problem**: Patterns designed for rectangular displays don't scale properly to helical arrangements.

**Solution**: Use auto-calculated dimensions and constexpr functions for compile-time optimization.

```cpp
// Compile-time dimension calculation
constexpr uint16_t WIDTH = fl::calculateCorkscrewWidth(19.0f, 288);
constexpr uint16_t HEIGHT = fl::calculateCorkscrewHeight(19.0f, 288);
CRGB frameBuffer[WIDTH * HEIGHT];
```

## Examples

### Example 1: Basic Corkscrew Setup

```cpp
#include "fl/corkscrew.h"
#include "fl/grid.h"
#include <FastLED.h>

#define NUM_LEDS 288
#define CORKSCREW_TURNS 19.25f
#define PIN_DATA 3

// Create corkscrew configuration
Corkscrew::Input corkscrewInput(CORKSCREW_TURNS, NUM_LEDS, 0);
Corkscrew corkscrew(corkscrewInput);

void setup() {
    // Initialize LED controller with corkscrew's internal buffer
    FastLED.addLeds<WS2812, PIN_DATA, BGR>(corkscrew.data(), NUM_LEDS);
    
    // Create ScreenMap for web interface
    fl::ScreenMap screenMap = corkscrew.toScreenMap(0.2f);
    FastLED.setScreenMap(screenMap);
}

void loop() {
    // Clear the corkscrew buffer
    corkscrew.clearBuffer();
    
    // Draw a simple moving dot
    static float position = 0.0f;
    position += 0.1f;
    if (position >= NUM_LEDS) position = 0.0f;
    
    // Use sub-pixel rendering for smooth movement
    Tile2x2_u8_wrap tile = corkscrew.at_wrap(position);
    for (int dx = 0; dx < 2; dx++) {
        for (int dy = 0; dy < 2; dy++) {
            auto data = tile.at(dx, dy);
            vec2i16 pos = data.first;
            uint8_t alpha = data.second;
            
            if (alpha > 0) {
                CRGB color = CRGB::Blue;
                color.nscale8(alpha);
                // Direct access to corkscrew buffer
                corkscrew.data()[pos.y * corkscrew.cylinder_width() + pos.x] = color;
            }
        }
    }
    
    FastLED.show();
}
```

### Example 2: FestivalStick with Noise Patterns

```cpp
#include "fl/corkscrew.h"
#include "fl/grid.h"
#include "noise.h"

#define NUM_LEDS 288
#define CORKSCREW_TURNS 19.25f

// Runtime corkscrew
Corkscrew::Input corkscrewInput(CORKSCREW_TURNS, NUM_LEDS, 0);
Corkscrew corkscrew(corkscrewInput);

// Compile-time dimensions for frame buffer
constexpr uint16_t WIDTH = fl::calculateCorkscrewWidth(CORKSCREW_TURNS, NUM_LEDS);
constexpr uint16_t HEIGHT = fl::calculateCorkscrewHeight(CORKSCREW_TURNS, NUM_LEDS);
fl::Grid<CRGB> frameBuffer(WIDTH, HEIGHT);

void setup() {
    FastLED.addLeds<APA102HD, PIN_DATA, PIN_CLOCK, BGR>(corkscrew.data(), NUM_LEDS);
    
    // Create accurate corkscrew ScreenMap
    fl::ScreenMap corkscrewScreenMap = corkscrew.toScreenMap(0.2f);
    FastLED.setScreenMap(corkscrewScreenMap);
}

void generateCylindricalNoise() {
    uint32_t time_offset = millis() * 4;
    
    for (int x = 0; x < WIDTH; x++) {
        for (int y = 0; y < HEIGHT; y++) {
            // Convert to cylindrical coordinates
            float angle = (float(x) / float(WIDTH)) * 2.0f * PI;
            float cylinder_radius = 30.0f; // noise scale
            
            // Generate noise using cylindrical coordinates
            float noise_x = cos(angle) * cylinder_radius;
            float noise_y = sin(angle) * cylinder_radius;
            float noise_z = float(y) * 30.0f;
            
            uint8_t noise_val = inoise8(noise_x, noise_y, noise_z + time_offset);
            
            // Map to color palette
            CRGB color = ColorFromPalette(PartyColors_p, noise_val, noise_val);
            frameBuffer.at(x, y) = color;
        }
    }
}

void loop() {
    frameBuffer.clear();
    
    // Generate noise pattern
    generateCylindricalNoise();
    
    // Map frame buffer to corkscrew LEDs with multi-sampling
    corkscrew.readFrom(frameBuffer, true); // true = use multi-sampling
    
    FastLED.show();
}
```

### Example 3: Interactive Position Control

```cpp
#include "fl/corkscrew.h"

UISlider positionSlider("Position", 0.0f, 0.0f, 1.0f, 0.001f);
UISlider speedSlider("Speed", 0.1f, 0.01f, 1.0f, 0.01f);
UICheckbox autoAdvance("Auto Advance", true);
UICheckbox useSplatRendering("Splat Rendering", true);

Corkscrew::Input input(19.25f, 288, 0);
Corkscrew corkscrew(input);
fl::Grid<CRGB> frameBuffer(corkscrew.cylinder_width(), corkscrew.cylinder_height());

void drawMovingDot(float position) {
    frameBuffer.clear();
    
    if (useSplatRendering.value()) {
        // Sub-pixel accurate rendering
        Tile2x2_u8_wrap tile = corkscrew.at_wrap(position);
        
        for (int dx = 0; dx < 2; dx++) {
            for (int dy = 0; dy < 2; dy++) {
                auto data = tile.at(dx, dy);
                vec2i16 wrapped_pos = data.first;
                uint8_t alpha = data.second;
                
                if (alpha > 0) {
                    CRGB color = CRGB::Red;
                    color.nscale8(alpha);
                    frameBuffer.at(wrapped_pos.x, wrapped_pos.y) = color;
                }
            }
        }
    } else {
        // Simple pixel rendering
        vec2f pos = corkscrew.at_exact(static_cast<uint16_t>(position));
        frameBuffer.at(static_cast<int>(pos.x), static_cast<int>(pos.y)) = CRGB::Red;
    }
}

void loop() {
    static float currentPosition = 0.0f;
    static uint32_t lastTime = 0;
    uint32_t now = millis();
    
    if (autoAdvance.value()) {
        float deltaTime = (now - lastTime) / 1000.0f;
        currentPosition += speedSlider.value() * deltaTime * NUM_LEDS;
        currentPosition = fmodf(currentPosition, NUM_LEDS);
        lastTime = now;
    } else {
        currentPosition = positionSlider.value() * (NUM_LEDS - 1);
    }
    
    drawMovingDot(currentPosition);
    corkscrew.readFrom(frameBuffer);
    FastLED.show();
}
```

### Example 4: Constexpr Compile-Time Optimization

```cpp
// Compile-time corkscrew dimension calculation
constexpr float TURNS = 19.25f;
constexpr uint16_t LEDS = 288;

// These are computed at compile time
constexpr uint16_t BUFFER_WIDTH = fl::calculateCorkscrewWidth(TURNS, LEDS);
constexpr uint16_t BUFFER_HEIGHT = fl::calculateCorkscrewHeight(TURNS, LEDS);
constexpr size_t BUFFER_SIZE = BUFFER_WIDTH * BUFFER_HEIGHT;

// Static allocation with compile-time sizes
CRGB staticFrameBuffer[BUFFER_SIZE];
fl::Grid<CRGB> frameBuffer(staticFrameBuffer, BUFFER_WIDTH, BUFFER_HEIGHT);

// Verify dimensions match runtime calculation
void setup() {
    Corkscrew::Input input(TURNS, LEDS, 0);
    Corkscrew corkscrew(input);
    
    // These should match the constexpr values
    assert(corkscrew.cylinder_width() == BUFFER_WIDTH);
    assert(corkscrew.cylinder_height() == BUFFER_HEIGHT);
}
```

### Example 5: Advanced Multi-Sampling

```cpp
void drawWithMultiSampling() {
    // Create high-resolution source pattern
    const int SCALE = 4;
    fl::Grid<CRGB> highResBuffer(WIDTH * SCALE, HEIGHT * SCALE);
    
    // Draw detailed pattern in high resolution
    for (int y = 0; y < HEIGHT * SCALE; y++) {
        for (int x = 0; x < WIDTH * SCALE; x++) {
            // Draw fine-grained pattern
            float distance = sqrt(pow(x - WIDTH*SCALE/2, 2) + pow(y - HEIGHT*SCALE/2, 2));
            uint8_t brightness = 255 - (uint8_t)min(255.0f, distance * 2);
            highResBuffer.at(x, y) = CHSV(0, 255, brightness);
        }
    }
    
    // Downsample to corkscrew dimensions with multi-sampling
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            uint32_t r = 0, g = 0, b = 0;
            
            // Sample SCALE x SCALE pixels and average
            for (int sy = 0; sy < SCALE; sy++) {
                for (int sx = 0; sx < SCALE; sx++) {
                    CRGB sample = highResBuffer.at(x * SCALE + sx, y * SCALE + sy);
                    r += sample.r;
                    g += sample.g;
                    b += sample.b;
                }
            }
            
            // Average and store
            uint32_t samples = SCALE * SCALE;
            frameBuffer.at(x, y) = CRGB(r / samples, g / samples, b / samples);
        }
    }
    
    // Map to corkscrew with additional multi-sampling
    corkscrew.readFrom(frameBuffer, true);
}
```

## Technical Notes

### Performance Considerations
- Use constexpr functions for compile-time dimension calculation
- Enable multi-sampling only when needed (smooth animations)
- Consider memory usage with large corkscrew configurations
- Buffer is lazily initialized - no memory overhead until first use

### Coordinate System Gotchas
- `at_no_wrap()` returns unwrapped coordinates (can exceed width)
- `at_exact()` returns wrapped coordinates (x wrapped to [0, width))
- `at_wrap()` returns sub-pixel tiles for smooth rendering
- Buffer indexing: `buffer[y * width + x]` for direct access

### Best Practices
- Use `toScreenMap()` for accurate web visualization
- Implement cylindrical noise for seamless patterns
- Use sub-pixel rendering for smooth animations
- Test with various turn counts and LED densities
- Consider gap compensation with offset circumference