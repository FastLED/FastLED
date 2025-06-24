#include <FastLED.h>
#include <iostream>

#define NUM_LEDS_PER_STRIP 60  // Number of LEDs in each strip
#define NUM_STRIPS 3           // Number of parallel strips

// Define the pins for each strip
#define STRIP_1_PIN 13
#define STRIP_2_PIN 12
#define STRIP_3_PIN 14
#define STRIP_4_PIN 14

// Create separate LED arrays for each strip
CRGB leds1[NUM_LEDS_PER_STRIP];
CRGB leds2[NUM_LEDS_PER_STRIP];
CRGB leds3[NUM_LEDS_PER_STRIP];
CRGB leds4[NUM_LEDS_PER_STRIP];

void setup() {
  Serial.begin(9600);
  //Initialize FastLED for multiple strips
  FastLED.addLeds<WS2812B, STRIP_1_PIN, GRB>(leds1, NUM_LEDS_PER_STRIP);
  FastLED.addLeds<WS2812B, STRIP_2_PIN, GRB>(leds2, NUM_LEDS_PER_STRIP);
  FastLED.addLeds<WS2812B, STRIP_3_PIN, GRB>(leds3, NUM_LEDS_PER_STRIP);
  FastLED.addLeds<WS2812B, STRIP_4_PIN, GRB>(leds3, NUM_LEDS_PER_STRIP);
  Serial.println("Setup");
  std::cout << "Setup" << std::endl;
  
  FastLED.setBrightness(64);  // Set initial brightness (0-255)
}

void loop() {
  // Create a simple rainbow pattern
  static uint8_t hue = 0;
  
  // Update each strip
  for(int i = 0; i < NUM_LEDS_PER_STRIP; i++) {
    leds1[i] = CHSV(hue + (i * 4), 255, 255);
    leds2[i] = CHSV(hue + (i * 4), 255, 255);
    leds3[i] = CHSV(hue + (i * 4), 255, 255);
    leds4[i] = CHSV(hue + (i * 4), 255, 255);
  }
  
  FastLED.show();
  
  EVERY_N_MILLISECONDS(20) {
    hue++;
  }
  EVERY_N_SECONDS(1) {
    // Serial.println("Alive");
    std::cout << "Alive" << std::endl;
  }
}
