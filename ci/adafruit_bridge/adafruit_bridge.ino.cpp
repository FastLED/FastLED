


#define FASTLED_USE_ADAFRUIT_NEOPIXEL 1
#include "Adafruit_NeoPixel.h"
#include "FastLED.h"

// Test the simplified Adafruit bridge with WS2812 strips
#define DATA_PIN 2
#define NUM_LEDS 144

// Test different RGB orders
CRGB leds_grb[NUM_LEDS];
CRGB leds_rgb[NUM_LEDS];
CRGB leds_bgr[NUM_LEDS];

void setup() {
    Serial.begin(115200);
    
    // Test simplified template signature - no T1,T2,T3 parameters
    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds_grb, NUM_LEDS);
    FastLED.addLeds<WS2812, DATA_PIN+1, RGB>(leds_rgb, NUM_LEDS);
    FastLED.addLeds<WS2812, DATA_PIN+2, BGR>(leds_bgr, NUM_LEDS);
    
    // Fill with test pattern
    fill_rainbow(leds_grb, NUM_LEDS, 0, 255/NUM_LEDS);
    fill_solid(leds_rgb, NUM_LEDS, CRGB::Red);
    fill_solid(leds_bgr, NUM_LEDS, CRGB::Blue);
    
    FastLED.show();
    Serial.println("Adafruit bridge test initialized");
}

void loop() {
    static uint8_t hue = 0;
    
    // Cycle through colors
    fill_rainbow(leds_grb, NUM_LEDS, hue, 255/NUM_LEDS);
    FastLED.show();
    
    hue++;
    delay(50);
}
