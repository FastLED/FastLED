/*
Festival Stick is a dense corkscrew of LEDs that is wrapped around one end of
a wooden walking stick commonly found on amazon.A0

The UI screenmap projects this cork screw into polar coordinates, so that the
LEDs are mapped to a sprial, with the inner portion of the spiral being the top,
the outer most portion being the bottom.

*/

#include "fl/assert.h"
#include "fl/corkscrew.h"
#include "fl/screenmap.h"
#include "fl/warn.h"
#include "fl/sstream.h"
#include "fl/leds.h"
#include "noise.h"
#include <FastLED.h>
// #include "vec3.h"

using namespace fl;

// Power management settings
#define VOLTS 5
#define MAX_AMPS 1

#define PIN_DATA 9
#define PIN_CLOCK 7

// Pin could have been tied to ground, instead it's tied to another pin.
#define PIN_BUTTON 1
#define PIN_GRND 2

#define NUM_LEDS 288
#define CORKSCREW_TOTAL_HEIGHT                                                 \
    23.25f // Total height of the corkscrew in centimeters for 144 densly
           // wrapped up over 19 turns
#define CORKSCREW_TURNS 19 // Default to 19 turns
// #define CM_BETWEEN_LEDS 1.0 // 1cm between LEDs
// #define CM_LED_DIAMETER 0.5 // 0.5cm LED diameter

#define CORKSCREW_WIDTH 16
#define CORKSCREW_HEIGHT 19


UITitle festivalStickTitle("Festival Stick");
UIDescription festivalStickDescription(
    "Take a wooden walking stick, wrap dense LEDs around it like a corkscrew. "
    "Super simple but very awesome looking."
    "This assumes the dense 144 LEDs / meter.");

UISlider ledsScale("Leds scale", 0.1f, 0.1f, 1.0f, 0.01f);
UIButton button("Button");

CRGB leds[NUM_LEDS];

// Tested on a 288 led (2x 144 max density led strip) with 19 turns
// with 23.25cm height, 19 turns, and ~15.5 LEDs per turn.
Corkscrew::Input
    corkscrewInput(CORKSCREW_TOTAL_HEIGHT,
                   CORKSCREW_TURNS * 2.0f * PI, // Default to 19 turns
                   0,        // offset to account for gaps between segments
                   NUM_LEDS, // Default to dense 144 leds.
    );

// Corkscrew::Output corkscrewMap = fl::Corkscrew::generateMap(corkscrewInput);
Corkscrew corkscrew(corkscrewInput);

// Used only for the fl::ScreenMap generation.
struct corkscrew_args {
    int num_leds = NUM_LEDS;
    float leds_per_turn = 15.5;
    float width_cm = 1.0;
};

fl::ScreenMap makeScreenMap(corkscrew_args args = corkscrew_args()) {
    // Create a ScreenMap for the corkscrew
    fl::vector<vec2f> points(args.num_leds);

    int num_leds = args.num_leds;
    float leds_per_turn = args.leds_per_turn;
    float width_cm = args.width_cm;

    const float circumference = leds_per_turn;
    const float radius = circumference / (2.0 * PI);      // radius in mm
    const float angle_per_led = 2.0 * PI / leds_per_turn; // degrees per LED
    const float height_per_turn_cm = width_cm; // 10cm height per turn
    const float height_per_led = height_per_turn_cm / leds_per_turn *
                                 1.3; // this is the changing height per led.

    for (int i = 0; i < num_leds; i++) {
        float angle = i * angle_per_led;            // angle in radians
        float r = radius + 10 + i * height_per_led; // height in cm

        // Calculate the x, y coordinates for the corkscrew
        float x = r * cos(angle); // x coordinate
        float y = r * sin(angle); // y coordinate

        // Store the 2D coordinates in the vector
        points[i] = vec2f(x, y);
    }

    FASTLED_WARN("Creating ScreenMap with:\n" << points);

    // Create a ScreenMap from the points
    fl::ScreenMap screenMap(points.data(), num_leds, .5);
    return screenMap;
}

// Create a corkscrew with:
// - 30cm total length (300mm)
// - 5cm width (50mm)
// - 2mm LED inner diameter
// - 24 LEDs per turn
// fl::ScreenMap screenMap = makeCorkScrew(NUM_LEDS,
// 300.0f, 50.0f, 2.0f, 24.0f);

// fl::vector<vec3f> mapCorkScrew = makeCorkScrew(args);
fl::ScreenMap screenMap;

CLEDController *addController() {
    CLEDController *controller =
        &FastLED.addLeds<APA102HD, PIN_DATA, PIN_CLOCK, BGR>(leds, NUM_LEDS);
    return controller;
}

void setup() {
    pinMode(PIN_GRND, OUTPUT);
    digitalWrite(PIN_GRND, LOW); // Set ground pin to low
    button.addRealButton(Button(PIN_BUTTON));
    corkscrew_args args = corkscrew_args();
    screenMap = makeScreenMap(args);
    // screenMap = ScreenMap::Circle(NUM_LEDS, 1.5f, 0.5f, 1.0f);
    auto controller = addController();
    // Set the screen map for the controller
    controller->setScreenMap(screenMap);

    // Set power management. This allows this festival stick to conformatable
    // run on any USB battery that can output at least 1A at 5V.
    // Keep in mind that this sketch is designed to use APA102HD mode, which
    // will result in even lowwer run power consumption, since the power mode
    // does not take into account the APA102HD gamma correction. However it is
    // still a correct upper bound that will match the ledset exactly when the
    // display tries to go full white.
    FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS, MAX_AMPS * 1000);
    button.onChanged([](UIButton &but) {
        // This function is called when the button is pressed
        // If the button is pressed, show the generative pattern
        if (but.isPressed()) {
            FASTLED_WARN("Button pressed");
        } else {
            FASTLED_WARN("NOT Button pressed");
        }
    });
}

void printOutput(const Corkscrew::Output& output) {
    fl::sstream stream;
    stream << "Corkscrew Output:\n";
    stream << "Width: " << output.width << "\n";
    stream << "Height: " << output.height << "\n";
    // stream << "Mapping: \n";
    // for (const auto &point : output.mapping) {
    //     stream << point << "\n";
    // }
    FASTLED_WARN(stream.str());
}



LedsXY<CORKSCREW_WIDTH, CORKSCREW_HEIGHT> frameBuffer;

void loop() {
    uint32_t now = millis();
    fl::clear(leds);
    fl::clear(frameBuffer);

    static int w = 0;

    EVERY_N_MILLIS(300) {
        // Update the corkscrew mapping every second
        w = (w + 1) % CORKSCREW_WIDTH;
    }


    // draw a blue line down the middle
    for (int i = 0; i < CORKSCREW_HEIGHT; ++i) {
        frameBuffer.at(w % CORKSCREW_WIDTH, i) = CRGB::Blue;
        frameBuffer.at((w + 1) % CORKSCREW_WIDTH, i) = CRGB::Blue;
        frameBuffer.at((w - 1 + CORKSCREW_WIDTH) % CORKSCREW_WIDTH, i) = CRGB::Blue;
        frameBuffer.at((w + 2) % CORKSCREW_WIDTH, i) = CRGB::Blue;
        frameBuffer.at((w - 2 + CORKSCREW_WIDTH) % CORKSCREW_WIDTH, i) = CRGB::Blue;
    }


    // printOutput(corkscrewMap);

    for (int i = 0; i < NUM_LEDS; ++i) {
        // Get the position in the frame buffer
        vec2<int16_t> pos = corkscrew.at(i);
        // Draw the tile to the frame buffer
        CRGB c = frameBuffer.at(pos.x, pos.y);
        leds[i] = c;

        FASTLED_WARN_IF(i < 16, "LED " << i << " at position: "
                        << pos.x << ", " << pos.y
                        << " with color: " << c);
            
    }
    
    FastLED.show();
}
