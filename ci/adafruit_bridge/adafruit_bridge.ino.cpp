


#define FASTLED_USE_ADAFRUIT_NEOPIXEL
#include "FastLED.h"

// Test the non-template Adafruit bridge
#define DATA_PIN 2
#define NUM_LEDS 144

// Test LED arrays with different RGB orders
CRGB leds_grb[NUM_LEDS];
CRGB leds_rgb[NUM_LEDS]; 
CRGB leds_bgr[NUM_LEDS];

void setup() {
    Serial.begin(115200);
    
    // Test template controllers  
    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds_grb, NUM_LEDS);
    FastLED.addLeds<WS2812, DATA_PIN+1, RGB>(leds_rgb, NUM_LEDS);
    FastLED.addLeds<WS2812, DATA_PIN+2, BGR>(leds_bgr, NUM_LEDS);
    
    // Fill with test patterns
    fill_rainbow(leds_grb, NUM_LEDS, 0, 255/NUM_LEDS);
    fill_solid(leds_rgb, NUM_LEDS, CRGB::Red);
    fill_solid(leds_bgr, NUM_LEDS, CRGB::Blue);
    
    FastLED.show();
    Serial.println("Adafruit bridge test initialized");
}

void loop() {
    static uint8_t hue = 0;
    
    // Update colors
    fill_rainbow(leds_grb, NUM_LEDS, hue, 255/NUM_LEDS);
    fill_solid(leds_rgb, NUM_LEDS, CHSV(hue, 255, 255));
    fill_solid(leds_bgr, NUM_LEDS, CHSV(hue + 128, 255, 255));
    
    FastLED.show();
    
    hue += 2;
    delay(50);
}
