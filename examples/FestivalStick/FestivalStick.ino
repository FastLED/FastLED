
#include "fl/screenmap.h"
#include "noise.h"
#include <FastLED.h>
#include "fl/warn.h"
#include "fl/assert.h"

#define NUM_LEDS 288
#define DATA_PIN 6
#define CM_BETWEEN_LEDS 1.0 // 1cm between LEDs
#define CM_LED_DIAMETER 0.5 // 0.5cm LED diameter

CRGB leds[NUM_LEDS];


// fl::vector<vec3f> 

fl::ScreenMap makeCorkScrew(int num_leds, float mm_led_whole_length, float mm_led_whole_width, float mm_led_inner_diameter,
                            float leds_per_turn) {
    
    float circumference = leds_per_turn;
    float angle_per_led = 2.0 * PI / leds_per_turn; // degrees per LED

    // float radius = 

    for (int i = 0; i < num_leds; i++) {
        float angle = i * angle_per_led; // angle in radians
        float height = (i / leds_per_turn) * mm_led_whole_length; // height along the corkscrew
        float radius = mm_led_whole_width / 2.0f; // radius of the corkscrew

        // Calculate x, y coordinates based on the angle and height
        float x = radius * cos(angle);
        float y = radius * sin(angle);

        // Create a vec2f for the LED position
        fl::vec2f pos(x, height);
        
        // Set the position in the screen map
        screenMap.set(i, pos);
    }
}

// Create a corkscrew with:
// - 30cm total length (300mm)
// - 5cm width (50mm)
// - 2mm LED inner diameter
// - 24 LEDs per turn
fl::ScreenMap screenMap = makeCorkScrew(NUM_LEDS, 300.0f, 50.0f, 2.0f, 24.0f);

void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS)
        .setScreenMap(screenMap);
    FastLED.setBrightness(64);
}

void loop() {
    uint32_t now = millis();

    // Create a moving pattern that travels up the corkscrew
    for (int i = 0; i < NUM_LEDS; i++) {
        // Get the 2D position of this LED from the screen map
        fl::vec2f pos = screenMap[i];
        
        // Create a wave pattern that moves up the corkscrew
        float wave = sin(pos.y * 0.2 - (now / 500.0));
        wave = (wave + 1.0) / 2.0; // Normalize to 0-1 range
        
        // Create a hue that changes with position and time
        uint8_t hue = int32_t((pos.x * 10 + pos.y * 5 + now / 20)) % 256;
        
        // Set brightness based on the wave pattern
        uint8_t val = 128 + 127 * wave;
        
        // Set the color
        leds[i] = CHSV(hue, 240, val);
    }

    FastLED.show();
}
