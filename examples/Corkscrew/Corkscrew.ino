/*
Festival Stick is a dense corkscrew of LEDs that is wrapped around one end of
a wooden walking stick commonly found on amazon.A0

The UI screenmap projects this cork screw into polar coordinates, so that the
LEDs are mapped to a sprial, with the inner portion of the spiral being the top,
the outer most portion being the bottom.

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


#define NUM_LEDS 10
#define CORKSCREW_TOTAL_HEIGHT 0 // when height = 0, it's a circle.
                                 // wrapped up over 19 turns
#define CORKSCREW_TURNS 1        // Default to 19 turns
#define CORKSCREW_TOTAL_LENGTH 100
// #define CM_BETWEEN_LEDS 1.0 // 1cm between LEDs
// #define CM_LED_DIAMETER 0.5 // 0.5cm LED diameter

UITitle festivalStickTitle("Corkscrew");
UIDescription festivalStickDescription(
    "Tests the ability to map a cork screw onto a 2D cylindrical surface");

// CRGB leds[NUM_LEDS];

// Tested on a 288 led (2x 144 max density led strip) with 19 turns
// with 23.25cm height, 19 turns, and ~15.5 LEDs per turn.
Corkscrew::Input
    corkscrewInput(CORKSCREW_TOTAL_LENGTH,
                   CORKSCREW_TOTAL_HEIGHT,
                   CORKSCREW_TURNS, // Default to 19 turns
                   0,        // offset to account for gaps between segments
                   NUM_LEDS, // Default to dense 144 leds.
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
    //corkscrew_args args = corkscrew_args();
    // screenMap = makeScreenMap(args);
    // screenMap = ScreenMap::Circle(NUM_LEDS, 1.5f, 0.5f, 1.0f);
    // FASTLED_ASSERT(false, "debugger");

    int width = corkscrew.cylinder_width();
    int height = corkscrew.cylinder_height();

    frameBuffer.reset(width, height);
    XYMap xyMap = XYMap::constructRectangularGrid(width, height, 0);

    CRGB* leds = frameBuffer.data();
    size_t num_leds = frameBuffer.size();

    CLEDController *controller = &FastLED.addLeds<WS2812, 3, BGR>(leds, num_leds);

    // Set the screen map for the controller
    controller->setScreenMap(xyMap);
}

void loop() {
    uint32_t now = millis();
    // fl::clear(leds);
    fl::clear(frameBuffer);

    static int pos = 0;
    EVERY_N_MILLIS(100) {
        // Update the corkscrew mapping every second
        // w = (w + 1) % CORKSCREW_WIDTH;
        // frameBuffer.
        pos++;
        if (pos > corkscrew.size() - 1) {
            pos = 0; // Reset to the beginning
        }
    }

    // // draw a blue line down the middle
    // for (int i = 0; i < CORKSCREW_HEIGHT; ++i) {
    //     frameBuffer.at(w % CORKSCREW_WIDTH, i) = CRGB::Blue;
    //     frameBuffer.at((w + 1) % CORKSCREW_WIDTH, i) = CRGB::Blue;
    //     frameBuffer.at((w - 1 + CORKSCREW_WIDTH) % CORKSCREW_WIDTH, i) =
    //     CRGB::Blue; frameBuffer.at((w + 2) % CORKSCREW_WIDTH, i) =
    //     CRGB::Blue; frameBuffer.at((w - 2 + CORKSCREW_WIDTH) %
    //     CORKSCREW_WIDTH, i) = CRGB::Blue;
    // }

    // frameBuffer.at(w, h) = CRGB::Blue; // Draw a blue pixel at (w, h)


    // now select the pixel via the corkscrew mapping.

    vec2f pos_vec2f = corkscrew.at(pos);
    vec2i16 pos_i16 = vec2i16(pos_vec2f.x, pos_vec2f.y);

    FASTLED_WARN("Pos.x = " << pos_vec2f.x << ", Pos.y = " << pos_vec2f.y);

    frameBuffer.at(pos_i16.x, pos_i16.y) = CRGB::Blue; // Draw a blue pixel at (w, h)


    FastLED.show();
}
