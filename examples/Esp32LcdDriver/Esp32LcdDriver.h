/// @file Esp32LcdDriver.h
/// @brief ESP32-S2/S3 LCD parallel driver demo - Ideal FastLED API
///
/// This example demonstrates the ideal FastLED API for ESP32-S2/S3 parallel output.
/// By default, this uses the I2S driver. To use the LCD driver instead, define:
///   #define FASTLED_ESP32_LCD_DRIVER
/// before including FastLED.h
///
/// Key features:
/// - Same FastLED API as any other platform
/// - Automatic chipset timing (WS2812, WS2811, SK6812, etc.)
/// - Driver selection via compile-time define
/// - Up to 16 parallel strips with rectangular buffer optimization
///
/// Hardware Requirements:
/// - ESP32-S2 or ESP32-S3 (both have LCD/I80 peripheral)
/// - PSRAM recommended for >500 LEDs per strip
/// - Up to 16 WS28xx LED strips
///
/// Notes:
/// - Define FASTLED_ESP32_LCD_DRIVER to use LCD instead of I2S
/// - Works on both ESP32-S2 and ESP32-S3 (identical API)
/// - All strips on same platform share the same bulk driver instance
/// - Serial output works with LCD driver (not recommended with I2S)

#pragma once

// Use LCD driver instead of I2S (default)
#define FASTLED_ESP32_LCD_DRIVER

#include <FastLED.h>

#define NUM_LEDS 256

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

    Serial.println("FastLED ESP32 LCD Driver Demo");
    Serial.println("==============================");

    Serial.println("Using LCD driver");


    // Standard FastLED API - driver auto-selected based on platform and define
    FastLED.addLeds<WS2812, PIN1>(leds1, NUM_LEDS);
    FastLED.addLeds<WS2812, PIN2>(leds2, NUM_LEDS);
    FastLED.addLeds<WS2812, PIN3>(leds3, NUM_LEDS);
    FastLED.addLeds<WS2812, PIN4>(leds4, NUM_LEDS);

    Serial.println("\nReady!");
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

    // Pulse on strip 4 (SK6812)
    fill_solid(leds4, NUM_LEDS, CHSV(hue + 128, 255, beatsin8(60)));

    FastLED.show();
    hue++;
}
