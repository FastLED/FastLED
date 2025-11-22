

#if defined(ESP32)

#include "esp_log.h"

// use gcc intialize constructor
// to set log level to ESP_LOG_VERBOSE
// before setup() is called
__attribute__((constructor))
void on_startup() {
  esp_log_level_set("*", ESP_LOG_VERBOSE);        // set all components to ERROR level
}

#endif  // ESP32

// Testing PARLIO driver on ESP32-C6
// Simple Blink test for WS2812 LEDs via PARLIO

#include <Arduino.h>
#include <FastLED.h>

#define NUM_LEDS 10
#define DATA_PIN 8

CRGB leds[NUM_LEDS];

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== ESP32-C6 PARLIO Driver Test ===");
    Serial.printf("Testing with %d LEDs on pin %d\n", NUM_LEDS, DATA_PIN);

    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
    Serial.println("FastLED initialized - PARLIO should be active on ESP32-C6");
}

void loop() {
    // Red blink
    Serial.println("Setting all LEDs to RED");
    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.show();
    delay(500);

    // Turn off
    Serial.println("Turning LEDs OFF");
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(500);

    // Green blink
    Serial.println("Setting all LEDs to GREEN");
    fill_solid(leds, NUM_LEDS, CRGB::Green);
    FastLED.show();
    delay(500);

    // Turn off
    Serial.println("Turning LEDs OFF");
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(500);

    // Blue blink
    Serial.println("Setting all LEDs to BLUE");
    fill_solid(leds, NUM_LEDS, CRGB::Blue);
    FastLED.show();
    delay(500);

    // Turn off
    Serial.println("Turning LEDs OFF");
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(500);
}
