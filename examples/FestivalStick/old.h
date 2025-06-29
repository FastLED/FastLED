/*
Festival Stick is a dense corkscrew of LEDs that is wrapped around one end of
a wooden walking stick commonly found on amazon.A0

The UI screenmap projects this cork screw into polar coordinates, so that the LEDs are
mapped to a sprial, with the inner portion of the spiral being the top, the outer
most portion being the bottom.

*/



#include "fl/assert.h"
#include "fl/screenmap.h"
#include "fl/warn.h"
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
// #define CM_BETWEEN_LEDS 1.0 // 1cm between LEDs
// #define CM_LED_DIAMETER 0.5 // 0.5cm LED diameter

UITitle festivalStickTitle("Festival Stick - Classic Version");
UIDescription festivalStickDescription(
    "Take a wooden walking stick, wrap dense LEDs around it like a corkscrew. Super simple but very awesome looking. "
    "This classic version uses 3D Perlin noise to create organic, flowing patterns around the cylindrical surface. "
    "Assumes dense 144 LEDs/meter (288 total LEDs).");

// UIHelp festivalStickHelp("Festival Stick - Classic Guide");

// UIHelp technicalHelp("Technical Details - Classic Festival Stick");
// UIHelp usageHelp("Usage Guide - Classic Festival Stick");  
// UIHelp physicalBuildHelp("Building Your Festival Stick");


UISlider ledsScale("Leds scale", 0.1f, 0.1f, 1.0f, 0.01f);
UIButton button("Button");

// Adding a brightness slider
UISlider brightness("Brightness", 16, 0, 255, 1); // Brightness from 0 to 255

CRGB leds[NUM_LEDS];


// fl::vector<vec3f>
struct corkscrew_args {
    int num_leds = NUM_LEDS;
    float leds_per_turn = 15.5;
    float width_cm = 1.0;
};

fl::vector<vec3f> makeCorkScrew(corkscrew_args args = corkscrew_args()) {
    // int num_leds, float leds_per_turn, float width_cm
    int num_leds = args.num_leds;
    float leds_per_turn = args.leds_per_turn;
    float width_cm = args.width_cm;

    const float circumference = leds_per_turn;
    const float radius = circumference / (2.0 * PI);      // radius in mm
    const float angle_per_led = 2.0 * PI / leds_per_turn; // degrees per LED
    const float total_angle_radians = angle_per_led * num_leds;
    const float total_turns = total_angle_radians / (2.0 * PI); // total turns
    const float height_per_turn_cm = width_cm; // 10cm height per turn
    const float height_per_led =
        height_per_turn_cm /
        leds_per_turn; // this is the changing height per led.
    const float total_height =
        height_per_turn_cm * total_turns; // total height of the corkscrew
    fl::vector<vec3f> out;
    for (int i = 0; i < num_leds; i++) {
        float angle = i * angle_per_led; // angle in radians
        float height = (i / leds_per_turn) * height_per_turn_cm; // height in cm

        // Calculate the x, y, z coordinates for the corkscrew
        float x = radius * cos(angle); // x coordinate
        float z = radius * sin(angle); // y coordinate
        float y = height;              // z coordinate

        // Store the 3D coordinates in the vector
        vec3f led_position(x, y, z);
        // screenMap.set(i, led_position);
        out.push_back(led_position);
    }
    return out;
}


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
    const float height_per_led =
        height_per_turn_cm /
        leds_per_turn * 1.3; // this is the changing height per led.



    for (int i = 0; i < num_leds; i++) {
        float angle = i * angle_per_led; // angle in radians
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

corkscrew_args args = corkscrew_args();
fl::vector<vec3f> mapCorkScrew = makeCorkScrew(args);
fl::ScreenMap screenMap;


CLEDController* addController() {
    CLEDController* controller = &FastLED.addLeds<APA102HD, PIN_DATA, PIN_CLOCK, BGR>(leds, NUM_LEDS);
    return controller;
}

void setup() {
    pinMode(PIN_GRND, OUTPUT);
    digitalWrite(PIN_GRND, LOW); // Set ground pin to low
    button.addRealButton(Button(PIN_BUTTON));
    screenMap = makeScreenMap(args);
    //screenMap = ScreenMap::Circle(NUM_LEDS, 1.5f, 0.5f, 1.0f);
    auto controller = addController();
    // Set the screen map for the controller
    controller->setScreenMap(screenMap);
    
    // Set power management. This allows this festival stick to conformatable
    // run on any USB battery that can output at least 1A at 5V.
    // Keep in mind that this sketch is designed to use APA102HD mode, which will
    // result in even lowwer run power consumption, since the power mode does not take
    // into account the APA102HD gamma correction. However it is still a correct upper bound
    // that will match the ledset exactly when the display tries to go full white.
    FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS, MAX_AMPS * 1000);
    // set brightness 8
    FastLED.setBrightness(brightness.as_int());
    button.onChanged([](UIButton& but) {
        // This function is called when the button is pressed
        // If the button is pressed, show the generative pattern
        if (but.isPressed()) {
            FASTLED_WARN("Button pressed");
        } else {
            FASTLED_WARN("NOT Button pressed");
        }
    });

}


void showGenerative(uint32_t now) {
    // This function is called to show the generative pattern
    for (int i = 0; i < NUM_LEDS; i++) {
        // Get the 2D position of this LED from the screen map
        fl::vec3f pos = mapCorkScrew[i];
        float x = pos.x;
        float y = pos.y;
        float z = pos.z;

        x*= 20.0f * ledsScale.value();
        y*= 20.0f * ledsScale.value();
        z*= 20.0f * ledsScale.value();

        uint16_t noise_value = inoise16(x,y,z, now / 100);
        // Normalize the noise value to 0-255
        uint8_t brightness = map(noise_value, 0, 65535, 0, 255);
        // Create a hue that changes with position and time
        uint8_t sat = int32_t((x * 10 + y * 5 + now / 5)) % 256;
        // Set the color
        leds[i] = CHSV(170, sat, fl::clamp(255- sat, 64, 255));
    }
}

void loop() {
    uint32_t now = millis();
    fl::clear(leds);
    showGenerative(now);
    FastLED.show();
}
