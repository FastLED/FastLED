/// @file    HelloWorld.ino
/// @brief   A simple "Hello World" example that cycles through colors to spell out a greeting
/// @example HelloWorld.ino

#include <FastLED.h>

// How many leds in your strip?
#define NUM_LEDS 10

// For led chips like WS2812, which have a data line, ground, and power, you just
// need to define DATA_PIN.  For led chipsets that are SPI based (four wires - data, clock,
// ground, and power), like the LPD8806 define both DATA_PIN and CLOCK_PIN
#define DATA_PIN 3
#define CLOCK_PIN 13

// Define the array of leds
CRGB leds[NUM_LEDS];

// Color palette for our "Hello World" greeting
CRGB hello_colors[] = {
  CRGB::Red,    // H
  CRGB::Orange, // E
  CRGB::Yellow, // L
  CRGB::Green,  // L
  CRGB::Blue,   // O
  CRGB::Purple, // W
  CRGB::Pink,   // O
  CRGB::Cyan,   // R
  CRGB::White,  // L
  CRGB::Magenta // D
};

void setup() { 
    Serial.begin(115200);
    Serial.println("Hello World - FastLED Example");
    
    // Initialize FastLED
    // Uncomment/edit one of the following lines for your leds arrangement.
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);  // GRB ordering is assumed
    
    // Set brightness (0-255)
    FastLED.setBrightness(50);
    
    Serial.println("FastLED initialized. Starting Hello World pattern...");
}

void loop() { 
    // "Hello World" pattern - light up LEDs one by one in sequence
    for(int i = 0; i < NUM_LEDS; i++) {
        // Clear all LEDs
        FastLED.clear();
        
        // Light up LEDs from 0 to current position with their respective colors
        for(int j = 0; j <= i; j++) {
            leds[j] = hello_colors[j];
        }
        
        FastLED.show();
        delay(300);
    }
    
    // Hold the full pattern for a moment
    delay(1000);
    
    // Fade out effect
    for(int brightness = 255; brightness >= 0; brightness -= 5) {
        FastLED.setBrightness(brightness);
        FastLED.show();
        delay(20);
    }
    
    // Reset brightness and pause before repeating
    FastLED.setBrightness(50);
    delay(500);
    
    Serial.println("Hello World cycle complete!");
}