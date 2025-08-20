/*
Basic cork screw test.

This test is forward mapping, in which we test that
the corkscrew is mapped to cylinder cartesian coordinates.

Most of the time, you'll want the reverse mapping, that is
drawing to a rectangular grid, and then mapping that to a corkscrew.

However, to make sure the above mapping works correctly, we have
to test that the forward mapping works correctly first.

NEW: ScreenMap Support
=====================
You can now create a ScreenMap directly from a Corkscrew, which maps
each LED index to its exact position on the cylindrical surface.
This is useful for web interfaces and visualization:

Example usage:
```cpp
// Create a corkscrew
Corkscrew corkscrew(totalTurns, numLeds);

// Create ScreenMap with 0.5cm LED diameter
fl::ScreenMap screenMap = corkscrew.toScreenMap(0.5f);

// Use with FastLED controller for web visualization
controller->setScreenMap(screenMap);
```

NEW: Rectangular Buffer Support
===============================
You can now draw into a rectangular fl::Leds grid and read that 
into the corkscrew's internal buffer for display:

Example usage:
```cpp
// Create a rectangular grid to draw into
CRGB grid_buffer[width * height];
fl::Leds grid(grid_buffer, width, height);

// Draw your 2D patterns into the grid
grid(x, y) = CRGB::Red;  // etc.

// Draw patterns on the corkscrew's surface and map to LEDs
auto& surface = corkscrew.surface();
// Draw your patterns on the surface, then call draw() to map to LEDs
corkscrew.draw();

// Access the LED pixel data
auto ledData = corkscrew.data();  // Or corkscrew.rawData() for pointer
// The LED data now contains the corkscrew mapping of your patterns
```
*/

#include "fl/assert.h"
#include "fl/corkscrew.h"
#include "fl/grid.h"
#include "fl/leds.h"
#include "fl/screenmap.h"
#include "fl/sstream.h"
#include "fl/warn.h"
#include "noise.h"
#include <FastLED.h>
// #include "vec3.h"

using namespace fl;

#define PIN_DATA 9

#define NUM_LEDS 288
#define CORKSCREW_TURNS 19       // Default to 19 turns

// #define CM_BETWEEN_LEDS 1.0 // 1cm between LEDs
// #define CM_LED_DIAMETER 0.5 // 0.5cm LED diameter

UITitle festivalStickTitle("Corkscrew");
UIDescription festivalStickDescription(
    "Tests the ability to map a cork screw onto a 2D cylindrical surface");

UISlider speed("Speed", 0.1f, 0.01f, 1.0f, 0.01f);

UICheckbox allWhite("All White", false);
UICheckbox splatRendering("Splat Rendering", true);
UICheckbox cachingEnabled("Enable Tile Caching", true);

// CRGB leds[NUM_LEDS];

// Tested on a 288 led (2x 144 max density led strip) with 19 turns
// Auto-calculates optimal grid dimensions from turns and LEDs
Corkscrew corkscrew(CORKSCREW_TURNS, // 19 turns
                    NUM_LEDS);        // 288 leds

// Create a corkscrew with:
// - 30cm total length (300mm)
// - 5cm width (50mm)
// - 2mm LED inner diameter
// - 24 LEDs per turn
// fl::ScreenMap screenMap = makeCorkScrew(NUM_LEDS,
// 300.0f, 50.0f, 2.0f, 24.0f);

// fl::vector<vec3f> mapCorkScrew = makeCorkScrew(args);
fl::ScreenMap screenMap;
fl::Grid<CRGB> frameBuffer;

void setup() {
    int width = corkscrew.cylinderWidth();
    int height = corkscrew.cylinderHeight();

    frameBuffer.reset(width, height);
    XYMap xyMap = XYMap::constructRectangularGrid(width, height, 0);

    CRGB *leds = frameBuffer.data();
    size_t num_leds = frameBuffer.size();

    CLEDController *controller =
        &FastLED.addLeds<WS2812, 3, BGR>(leds, num_leds);

    // NEW: Create ScreenMap directly from Corkscrew using toScreenMap()
    // This maps each LED index to its exact position on the cylindrical surface
    fl::ScreenMap corkscrewScreenMap = corkscrew.toScreenMap(0.2f);
    
    // Alternative: Create ScreenMap from rectangular XYMap (old way)
    // fl::ScreenMap screenMap = xyMap.toScreenMap();
    // screenMap.setDiameter(.2f);

    // Set the corkscrew screen map for the controller
    // This allows the web interface to display the actual corkscrew shape
    controller->setScreenMap(corkscrewScreenMap);
    
    // Initialize caching based on UI setting
    corkscrew.setCachingEnabled(cachingEnabled.value());
}

void loop() {
    fl::clear(frameBuffer);
    static float pos = 0;
    pos += speed.value();
    if (pos > corkscrew.size() - 1) {
        pos = 0; // Reset to the beginning
    }

    // Update caching setting if it changed
    static bool lastCachingState = cachingEnabled.value();
    if (lastCachingState != cachingEnabled.value()) {
        corkscrew.setCachingEnabled(cachingEnabled.value());
        lastCachingState = cachingEnabled.value();
    }

    if (allWhite) {
        for (size_t i = 0; i < frameBuffer.size(); ++i) {
            frameBuffer.data()[i] = CRGB(8, 8, 8);
        }
    }

    if (splatRendering) {
        Tile2x2_u8_wrap pos_tile = corkscrew.at_wrap(pos);
        const CRGB color = CRGB::Blue;
        // Draw each pixel in the 2x2 tile using the new wrapping API
        for (int dx = 0; dx < 2; ++dx) {
            for (int dy = 0; dy < 2; ++dy) {
                auto data = pos_tile.at(dx, dy);
                vec2<u16> wrapped_pos = data.first; // Already wrapped position
                uint8_t alpha = data.second;      // Alpha value

                if (alpha > 0) { // Only draw if there's some alpha
                    CRGB c = color;
                    c.nscale8(alpha); // Scale the color by the alpha value
                    frameBuffer.at(wrapped_pos.x, wrapped_pos.y) = c;
                }
            }
        }
    } else {
        // None splat rendering, looks aweful.
        vec2f pos_vec2f = corkscrew.at_no_wrap(pos);
        vec2<u16> pos_i16 = vec2<u16>(round(pos_vec2f.x), round(pos_vec2f.y));
        // Now map the cork screw position to the cylindrical buffer that we
        // will draw.
        frameBuffer.at(pos_i16.x, pos_i16.y) =
            CRGB::Blue; // Draw a blue pixel at (w, h)
    }
    FastLED.show();
}
