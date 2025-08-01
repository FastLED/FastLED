/// @file    HelloWorld.ino
/// @brief   A simple "Hello World" example for FastLED
/// @example HelloWorld.ino

#include <FastLED.h>

///////////////////////////////////////////////////////////////////////////////////////////
//
// A simple "Hello World" program for FastLED that displays a colorful pattern
// representing the classic greeting. This example shows the basic structure
// of a FastLED program and demonstrates how to set up LEDs and create patterns.
//

// How many leds are in the strip?
#define NUM_LEDS 30

// Data pin for LED strip
#define DATA_PIN 3

// This is an array of leds. One item for each led in your strip.
CRGB leds[NUM_LEDS];

// This function sets up the leds and tells the controller about them
void setup() {
    // Sanity check delay - allows reprogramming if accidentally blowing power w/leds
    delay(2000);
    
    // Initialize serial communication for debugging
    Serial.begin(115200);
    Serial.println("Hello World - FastLED Example!");
    
    // Configure the LED strip (WS2811 is a common type)
    FastLED.addLeds<WS2811, DATA_PIN, RGB>(leds, NUM_LEDS);
    
    // Set initial brightness (0-255)
    FastLED.setBrightness(64);
}

// This function runs over and over, creating our "Hello World" pattern
void loop() {
    // Clear all LEDs
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    
    // Create a "Hello World" pattern using different colors
    // H - Red
    leds[0] = CRGB::Red;
    leds[1] = CRGB::Red;
    
    // E - Orange  
    leds[2] = CRGB::Orange;
    leds[3] = CRGB::Orange;
    
    // L - Yellow
    leds[4] = CRGB::Yellow;
    leds[5] = CRGB::Yellow;
    
    // L - Green
    leds[6] = CRGB::Green;
    leds[7] = CRGB::Green;
    
    // O - Blue
    leds[8] = CRGB::Blue;
    leds[9] = CRGB::Blue;
    
    // Space
    leds[10] = CRGB::Black;
    leds[11] = CRGB::Black;
    
    // W - Purple
    leds[12] = CRGB::Purple;
    leds[13] = CRGB::Purple;
    leds[14] = CRGB::Purple;
    
    // O - Cyan
    leds[15] = CRGB::Cyan;
    leds[16] = CRGB::Cyan;
    
    // R - Pink
    leds[17] = CRGB::DeepPink;
    leds[18] = CRGB::DeepPink;
    
    // L - White
    leds[19] = CRGB::White;
    leds[20] = CRGB::White;
    
    // D - Gold
    leds[21] = CRGB::Gold;
    leds[22] = CRGB::Gold;
    
    // Show the pattern
    FastLED.show();
    
    // Hold the pattern for 2 seconds
    delay(2000);
    
    // Create a simple rainbow wave effect as a bonus
    for(int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CHSV((i * 255 / NUM_LEDS) + millis() / 10, 255, 255);
    }
    FastLED.show();
    delay(50);
}