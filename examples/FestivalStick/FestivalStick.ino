
#include "fl/screenmap.h"
#include "noise.h"
#include <FastLED.h>

#define NUM_LEDS 288
#define DATA_PIN 6
#define CM_BETWEEN_LEDS 1.0 // 1cm between LEDs
#define CM_LED_DIAMETER 0.5 // 0.5cm LED diameter

CRGB leds[NUM_LEDS];
fl::ScreenMap circleMap =
    fl::ScreenMap::Circle(NUM_LEDS, CM_BETWEEN_LEDS, CM_LED_DIAMETER);

void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS)
        .setScreenMap(circleMap);
    FastLED.setBrightness(64);
}

void loop() {
    uint32_t now = millis();

    // Example of using the circle map (optional visual effect)
    for (int i = 0; i < NUM_LEDS; i++) {
        // Get the 2D position of this LED
        float anim = fl::map_range(i, 0, NUM_LEDS - 1, 0.0, 1.0);
        float angle = anim * 2.0 * PI + (now / 1000.0f);
        float x = cos(angle) * circleMap.getBounds().x / 2.0f;
        float y = sin(angle) * circleMap.getBounds().y / 2.0f;
        // Set the color based on the position
        CRGB c = CHSV((angle * 180.0 / PI), 255, 255);
        leds[i] = c;
    }

    FastLED.show();
}
