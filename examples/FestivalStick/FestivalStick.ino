
#include "fl/assert.h"
#include "fl/screenmap.h"
#include "fl/warn.h"
#include "noise.h"
#include <FastLED.h>
// #include "vec3.h"

#define PIN_DATA MOSI
#define PIN_CLOCK SCK

#define NUM_LEDS 288
#define DATA_PIN 6
// #define CM_BETWEEN_LEDS 1.0 // 1cm between LEDs
// #define CM_LED_DIAMETER 0.5 // 0.5cm LED diameter

CRGB leds[NUM_LEDS];

// using sketch::vec3f; // Use vec3f for 3D coordinates
using fl::vec3f;

// fl::vector<vec3f>
struct corkscrew_args {
    int num_leds = NUM_LEDS;
    float leds_per_turn = 12.5;
    float width_cm = 1.0;
    float led_diameter_cm = 0.2; // 0.2cm LED diameter
};

fl::vector<vec3f> makeCorkScrew(corkscrew_args args = corkscrew_args()) {
    // int num_leds, float leds_per_turn, float width_cm
    int num_leds = args.num_leds;
    float leds_per_turn = args.leds_per_turn;
    float width_cm = args.width_cm;
    float led_diameter_cm = args.led_diameter_cm; // 0.2cm LED diameter

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
        float y = radius * sin(angle); // y coordinate
        float z = height;              // z coordinate

        // Store the 3D coordinates in the vector
        vec3f led_position(x, y, z);
        // screenMap.set(i, led_position);
        out.push_back(led_position);
    }
    return out;
}

// Create a corkscrew with:
// - 30cm total length (300mm)
// - 5cm width (50mm)
// - 2mm LED inner diameter
// - 24 LEDs per turn
// fl::ScreenMap screenMap = makeCorkScrew(NUM_LEDS,
// 300.0f, 50.0f, 2.0f, 24.0f);

fl::vector<vec3f> mapCorkScrew = makeCorkScrew(corkscrew_args());

void setup() {
    FastLED.addLeds<APA102HD, PIN_DATA, PIN_CLOCK, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(64);
}

void loop() {
    uint32_t now = millis();
    fl::clear(leds);

    for (int i = 0; i < NUM_LEDS; i++) {
        // Get the 3D position of this LED from the corkscrew map
        fl::vec3f pos = mapCorkScrew[i];

        // Create a wave pattern that moves up the corkscrew
        float wave = sin(pos.z * 0.2 - (now / 500.0));
        wave = (wave + 1.0) / 2.0; // Normalize to 0-1 range

        // Create a hue that changes with position and time
        uint8_t hue = int32_t((pos.x * 10 + pos.y * 5 + now / 20)) % 256;

        // Set brightness based on the wave pattern
        uint8_t val = 128 + 127 * wave;

        // Set the color
        leds[i] = CHSV(hue, 240, val);
    }

    // Create a moving pattern that travels up the corkscrew
    // for (int i = 0; i < NUM_LEDS; i++) {
    //     // Get the 2D position of this LED from the screen map
    //     fl::vec2f pos = screenMap[i];

    //     // Create a wave pattern that moves up the corkscrew
    //     float wave = sin(pos.y * 0.2 - (now / 500.0));
    //     wave = (wave + 1.0) / 2.0; // Normalize to 0-1 range

    //     // Create a hue that changes with position and time
    //     uint8_t hue = int32_t((pos.x * 10 + pos.y * 5 + now / 20)) % 256;

    //     // Set brightness based on the wave pattern
    //     uint8_t val = 128 + 127 * wave;

    //     // Set the color
    //     leds[i] = CHSV(hue, 240, val);
    // }

    FastLED.show();
}
