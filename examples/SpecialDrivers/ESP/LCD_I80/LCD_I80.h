/// @file LCD_I80.h
/// @brief ESP32-S2/S3 LCD I80 parallel driver demo
///
/// This example demonstrates the LCD I80 driver for ESP32-S2/S3 parallel output.
///
/// Key features:
/// - Standard FastLED API
/// - Automatic chipset timing (WS2812, WS2811, SK6812, etc.)
/// - Up to 16 parallel strips with rectangular buffer optimization
///
/// Hardware Requirements:
/// - ESP32-S2 or ESP32-S3 (both have LCD/I80 peripheral)
/// - PSRAM recommended for >500 LEDs per strip
/// - Up to 16 WS28xx LED strips
///
/// Notes:
/// - Uses LCD I80 peripheral (parallel interface)
/// - Works on both ESP32-S2 and ESP32-S3 (identical API)
/// - All strips on same platform share the same bulk driver instance
/// - Serial output works with LCD driver

#pragma once

// Use LCD I80 driver
#define FASTLED_ESP32_LCD_DRIVER

#include <FastLED.h>

// Reduce LED count in QEMU since it doesn't have PSRAM
#ifdef FASTLED_ESP32_IS_QEMU
#define NUM_LEDS 16
#else
#define NUM_LEDS 256
#endif

// GPIO pins for LED strips
#define PIN1  3
#define PIN2  45
#define PIN3  21
#define PIN4  6

// LED arrays
CRGB leds1[NUM_LEDS];
CRGB leds2[NUM_LEDS];
CRGB leds3[NUM_LEDS];
CRGB leds4[NUM_LEDS];

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("LCD_I80 Driver Demo");
    Serial.println("===================");
    Serial.println("Using LCD I80 parallel driver");

    // Standard FastLED API - driver auto-selected based on platform and define
    FastLED.addLeds<WS2812, PIN1>(leds1, NUM_LEDS);
    FastLED.addLeds<WS2812, PIN2>(leds2, NUM_LEDS);
    FastLED.addLeds<WS2812, PIN3>(leds3, NUM_LEDS);
    FastLED.addLeds<WS2812, PIN4>(leds4, NUM_LEDS);

    Serial.println("Ready!");
}

void loop() {
    static uint8_t hue = 0;

    EVERY_N_MILLIS(1000) {
        Serial.println("Loop!");
    }

    // Rainbow on strip 1
    fill_rainbow(leds1, NUM_LEDS, hue, 7);

    // Solid color on strip 2
    fill_solid(leds2, NUM_LEDS, CHSV(hue, 255, 255));

    // Chase pattern on strip 3
    fill_solid(leds3, NUM_LEDS, CRGB::Black);
    leds3[beatsin16(60, 0, NUM_LEDS-1)] = CRGB::White;

    // Pulse on strip 4
    fill_solid(leds4, NUM_LEDS, CHSV(hue + 128, 255, beatsin8(60)));

    FastLED.show();
    hue++;
}
