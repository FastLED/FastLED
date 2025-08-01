#include <FastLED.h>

// How many leds are in the strip
#define NUM_LEDS 60

// Data pin that led data will be written out over
#define DATA_PIN 3

// This is an array of leds.  One item for each led in your strip.
CRGB leds[NUM_LEDS];

void setup() {
  // Initialize serial communication for "Hello World"
  Serial.begin(115200);
  Serial.println("Hello World!");

  // Initialize FastLED with the LED strip configuration
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  
  // Set initial brightness
  FastLED.setBrightness(64);
}

void loop() { 
  // Create a simple "Hello World" animation
  // Fill the strip with rainbow colors
  fill_rainbow(leds, NUM_LEDS, millis() / 50);
  
  // Add a white dot that moves along the strip
  static uint8_t pos = 0;
  leds[pos] = CRGB::White;
  
  // Show the leds
  FastLED.show();
  
  // Reset previous position and move to next
  leds[pos] = CRGB::Black;
  pos = (pos + 1) % NUM_LEDS;
  
  // Small delay to control animation speed
  delay(50);
}