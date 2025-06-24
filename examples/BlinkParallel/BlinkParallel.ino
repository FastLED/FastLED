
/// @file    BlinkParallel.ino
/// @brief   Shows parallel usage of WS2812 strips. Blinks once for red, twice for green, thrice for blue.
/// @example BlinkParallel.ino

#include "FastLED.h"

// How many leds in your strip?
#define NUM_LEDS 256

// Demo of driving multiple WS2812 strips on different pins

// Define the array of leds
CRGB leds[NUM_LEDS];  // Yes, they all share a buffer.

void setup() {
    Serial.begin(115200);
    //FastLED.addLeds<WS2812, 5>(leds, NUM_LEDS);  // GRB ordering is assumed
    FastLED.addLeds<WS2812, 1>(leds, NUM_LEDS);  // GRB ordering is assumed
    FastLED.addLeds<WS2812, 2>(leds, NUM_LEDS);  // GRB ordering is assumed
    FastLED.addLeds<WS2812, 3>(leds, NUM_LEDS);  // GRB ordering is assumed
    FastLED.addLeds<WS2812, 4>(leds, NUM_LEDS);  // GRB ordering is assumed
    delay(1000);
}

void fill(CRGB color) {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = color;
  }
}

void blink(CRGB color, int times) {
  for (int i = 0; i < times; i++) {
    fill(color);
    FastLED.show();
    delay(500);
    fill(CRGB::Black);
    FastLED.show();
    delay(500);
  }
}

void loop() { 
  // Turn the LED on, then pause
  blink(CRGB(8,0,0), 1);  // blink once for red
  blink(CRGB(0,8,0), 2);  // blink twice for green
  blink(CRGB(0,0,8), 3);  // blink thrice for blue

  delay(50);



  // now benchmark
  uint32_t start = millis();
  fill(CRGB(8,8,8));
  FastLED.show();
  uint32_t diff = millis() - start;

  Serial.print("Time to fill and show for non blocking (ms): ");
  Serial.println(diff);

  delay(50);

  start = millis();
  fill(CRGB(8,8,8));
  FastLED.show();
  FastLED.show();

  diff = millis() - start;
  Serial.print("Time to fill and show for 2nd blocking (ms): ");
  Serial.println(diff);
}