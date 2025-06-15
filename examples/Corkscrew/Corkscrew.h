/*
Basic cork screw test.

This test is forward mapping, in which we test that
the corkscrew is mapped to cylinder cartesian coordinates.

Most of the time, you'll want the reverse mapping, that is
drawing to a rectangular grid, and then mapping that to a corkscrew.

However, to make sure the above mapping works correctly, we have
to test that the forward mapping works correctly first.

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
#define CORKSCREW_TOTAL_LENGTH 100
#define CORKSCREW_TOTAL_HEIGHT 23.25 // when height = 0, it's a circle.
                                     // wrapped up over 19 turns
#define CORKSCREW_TURNS 19           // Default to 19 turns

// #define CM_BETWEEN_LEDS 1.0 // 1cm between LEDs
// #define CM_LED_DIAMETER 0.5 // 0.5cm LED diameter

UITitle festivalStickTitle("Corkscrew");
UIDescription festivalStickDescription(
    "Tests the ability to map a cork screw onto a 2D cylindrical surface");

UISlider speed("Speed", 0.1f, 0.01f, 1.0f, 0.01f);

UICheckbox allWhite("All White", false);
UICheckbox splatRendering("Splat Rendering", true);

// CRGB leds[NUM_LEDS];

// Tested on a 288 led (2x 144 max density led strip) with 19 turns
// with 23.25cm height, 19 turns, and ~15.5 LEDs per turn.
Corkscrew::Input corkscrewInput(CORKSCREW_TOTAL_LENGTH, CORKSCREW_TOTAL_HEIGHT,
                                CORKSCREW_TURNS, // Default to 19 turns
                                NUM_LEDS,        // Default to dense 144 leds.
                                0 // offset to account for gaps between segments
);

// Corkscrew::State corkscrewMap = fl::Corkscrew::generateMap(corkscrewInput);
Corkscrew corkscrew(corkscrewInput);

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
    int width = corkscrew.cylinder_width();
    int height = corkscrew.cylinder_height();

    frameBuffer.reset(width, height);
    XYMap xyMap = XYMap::constructRectangularGrid(width, height, 0);

    CRGB *leds = frameBuffer.data();
    size_t num_leds = frameBuffer.size();

    CLEDController *controller =
        &FastLED.addLeds<WS2812, 3, BGR>(leds, num_leds);

    fl::ScreenMap screenMap = xyMap.toScreenMap();
    screenMap.setDiameter(.2f);

    // Set the screen map for the controller
    controller->setScreenMap(screenMap);
}

void loop() {
    fl::clear(frameBuffer);
    static float pos = 0;
    pos += speed.value();
    if (pos > corkscrew.size() - 1) {
        pos = 0; // Reset to the beginning
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
                vec2i16 wrapped_pos = data.first; // Already wrapped position
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
        vec2f pos_vec2f = corkscrew.at_exact(pos);
        vec2i16 pos_i16 = vec2i16(round(pos_vec2f.x), round(pos_vec2f.y));
        // Now map the cork screw position to the cylindrical buffer that we
        // will draw.
        frameBuffer.at(pos_i16.x, pos_i16.y) =
            CRGB::Blue; // Draw a blue pixel at (w, h)
    }
    FastLED.show();
}
