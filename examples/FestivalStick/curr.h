/*
Basic cork screw test.

This test is forward mapping, in which we test that
the corkscrew is mapped to cylinder cartesian coordinates.

Most of the time, you'll want the reverse mapping, that is
drawing to a rectangular grid, and then mapping that to a corkscrew.

However, to make sure the above mapping works correctly, we have
to test that the forward mapping works correctly first.

*/

#include "FastLED.h"

#include "fl/assert.h"
#include "fl/corkscrew.h"
#include "fl/grid.h"
#include "fl/leds.h"
#include "fl/screenmap.h"
#include "fl/sstream.h"
#include "fl/warn.h"
#include "noise.h"

// #include "vec3.h"

using namespace fl;



#ifndef PIN_DATA
#define PIN_DATA 1  // Universally available pin
#endif

#ifndef PIN_CLOCK
#define PIN_CLOCK 2  // Universally available pin
#endif


#ifdef TEST
#define NUM_LEDS  4
#define CORKSCREW_TURNS 2 // Default to 19 turns
#else
#define NUM_LEDS  288
#define CORKSCREW_TURNS 19.0 // Default to 19 turns
#endif

// #define CM_BETWEEN_LEDS 1.0 // 1cm between LEDs
// #define CM_LED_DIAMETER 0.5 // 0.5cm LED diameter

UITitle festivalStickTitle("Corkscrew");
UIDescription festivalStickDescription(
    "Tests the ability to map a cork screw onto a 2D cylindrical surface");

UISlider speed("Speed", 0.1f, 0.01f, 1.0f, 0.01f);
UISlider positionCoarse("Position Coarse (10x)", 0.0f, 0.0f, 1.0f, 0.01f);
UISlider positionFine("Position Fine (1x)", 0.0f, 0.0f, 0.1f, 0.001f);
UISlider positionExtraFine("Position Extra Fine (0.1x)", 0.0f, 0.0f, 0.01f, 0.0001f);

UICheckbox autoAdvance("Auto Advance", true);
UICheckbox allWhite("All White", false);
UICheckbox splatRendering("Splat Rendering", true);

// Option 1: Runtime Corkscrew (flexible, configurable at runtime)
Corkscrew::Input corkscrewInput(CORKSCREW_TURNS, NUM_LEDS, 0);
Corkscrew corkscrew(corkscrewInput);

// Simple position tracking - one variable for both modes
static float currentPosition = 0.0f;
static uint32_t lastUpdateTime = 0;

// Option 2: Constexpr dimensions for compile-time array sizing
constexpr uint16_t CORKSCREW_WIDTH =
    fl::calculateCorkscrewWidth(CORKSCREW_TURNS, NUM_LEDS);
constexpr uint16_t CORKSCREW_HEIGHT =
    fl::calculateCorkscrewHeight(CORKSCREW_TURNS, NUM_LEDS);

// Now you can use these for array initialization:
// CRGB frameBuffer[CORKSCREW_WIDTH * CORKSCREW_HEIGHT];  // Compile-time sized
// array

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
    // Use constexpr dimensions (computed at compile time)
    constexpr int width = CORKSCREW_WIDTH;   // = 16
    constexpr int height = CORKSCREW_HEIGHT; // = 18

    // Or use runtime corkscrew for dynamic sizing
    // int width = corkscrew.cylinder_width();
    // int height = corkscrew.cylinder_height();

    frameBuffer.reset(width, height);
    XYMap xyMap = XYMap::constructRectangularGrid(width, height, 0);

    CRGB *leds = frameBuffer.data();
    size_t num_leds = frameBuffer.size();

    CLEDController *controller =
        &FastLED.addLeds<APA102HD, PIN_DATA, PIN_CLOCK, BGR>(leds, NUM_LEDS);

    // CLEDController *controller =
    //     &FastLED.addLeds<WS2812, 3, BGR>(leds, num_leds);

    fl::ScreenMap screenMap = xyMap.toScreenMap();
    screenMap.setDiameter(.2f);

    // Set the screen map for the controller
    controller->setScreenMap(screenMap);
}

float get_position(uint32_t now) {
    if (autoAdvance.value()) {
        // Check if auto-advance was just enabled
        // Auto-advance mode: increment smoothly from current position
        float elapsedSeconds = (now - lastUpdateTime) / 1000.0f;
        float increment = elapsedSeconds * speed.value() *
                          0.3f; // Make it 1/20th the original speed
        currentPosition = fmodf(currentPosition + increment, 1.0f);
        lastUpdateTime = now;
        return currentPosition;
    } else {
        // Manual mode: use the dual slider control
        float combinedPosition = positionCoarse.value() + positionFine.value() + positionExtraFine.value();
        // Clamp to ensure we don't exceed 1.0
        if (combinedPosition > 1.0f)
            combinedPosition = 1.0f;
        return combinedPosition;
    }
}


void draw(float pos) {
    if (allWhite) {
        for (size_t i = 0; i < frameBuffer.size(); ++i) {
            frameBuffer.data()[i] = CRGB(8, 8, 8);
        }
    }

    if (splatRendering) {
        Tile2x2_u8_wrap pos_tile = corkscrew.at_wrap(pos);
        FL_WARN("pos_tile: " << pos_tile);
        const CRGB color = CRGB::Blue;
        // Draw each pixel in the 2x2 tile using the new wrapping API
        for (int dx = 0; dx < 2; ++dx) {
            for (int dy = 0; dy < 2; ++dy) {
                Tile2x2_u8_wrap::Data data = pos_tile.at(dx, dy);
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
        FL_WARN("pos_vec2f: " << pos_vec2f);
        vec2i16 pos_i16 = vec2i16(pos_vec2f.x, pos_vec2f.y);
        // Now map the cork screw position to the cylindrical buffer that we
        // will draw.
        frameBuffer.at(pos_i16.x, pos_i16.y) =
            CRGB::Blue; // Draw a blue pixel at (w, h)
    }
}

void loop() {
    uint32_t now = millis();
    fl::clear(frameBuffer);

    // Update the corkscrew mapping with auto-advance or manual position control
    float combinedPosition = get_position(now);
    float pos = combinedPosition * (corkscrew.size() - 1);

    FL_WARN("pos: " << pos);
    draw(pos);
    FastLED.show();
}
