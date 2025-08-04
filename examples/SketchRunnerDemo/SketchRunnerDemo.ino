// Example Arduino sketch for testing the FastLED sketch runner
// This demonstrates a simple LED animation that can be run via the sketch runner

#include "FastLED.h"

#define NUM_LEDS 10
#define DATA_PIN 2

CRGB leds[NUM_LEDS];
static int frame_count = 0;

void setup() {
    printf("SKETCH: FastLED Sketch Runner Demo\n");
    printf("SKETCH: Setting up %d LEDs on pin %d\n", NUM_LEDS, DATA_PIN);
    
    // Initialize FastLED
    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(128);
    
    // Clear all LEDs
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    
    printf("SKETCH: Setup complete\n");
}

void loop() {
    frame_count++;
    printf("SKETCH: Running animation frame %d\n", frame_count);
    
    // Simple animation: cycle through colors
    CRGB color;
    switch (frame_count % 3) {
        case 0: color = CRGB::Red; break;
        case 1: color = CRGB::Green; break;
        case 2: color = CRGB::Blue; break;
    }
    
    // Set all LEDs to the current color
    fill_solid(leds, NUM_LEDS, color);
    
    printf("SKETCH: Setting %d LEDs to color RGB(%d,%d,%d)\n", 
           NUM_LEDS, color.r, color.g, color.b);
    
    // Update the LED strip
    FastLED.show();
    
    printf("SKETCH: Frame %d complete\n", frame_count);
}
