/// @file LCD_RGB.h
/// @brief ESP32-P4 LCD RGB parallel driver demo
///
/// This example demonstrates the LCD RGB driver for ESP32-P4 parallel output.
///
/// Key features:
/// - Standard FastLED API
/// - Automatic chipset timing (WS2812, WS2811, SK6812, etc.)
/// - RGB LCD peripheral for high-performance parallel output
/// - Up to 16 parallel strips
///
/// Hardware Requirements:
/// - ESP32-P4 (has RGB LCD peripheral)
/// - PSRAM recommended for >500 LEDs per strip
/// - Up to 16 WS28xx LED strips
///
/// Notes:
/// - Uses LCD RGB peripheral (different from I80 interface)
/// - ESP32-P4 specific
/// - All strips share the same bulk driver instance

#pragma once

// Use LCD RGB driver (ESP32-P4 only)
#define FASTLED_ESP32_LCD_RGB_DRIVER

#include <FastLED.h>

// Reduce LED count in QEMU since it doesn't have PSRAM
#ifdef FASTLED_ESP32_IS_QEMU
#define NUM_LEDS 16
#else
#define NUM_LEDS 256
#endif

// GPIO pins for LED strips (P4-specific pins)
#define PIN1  10
#define PIN2  11
#define PIN3  12
#define PIN4  13

// LED arrays
CRGB leds1[NUM_LEDS];
CRGB leds2[NUM_LEDS];
CRGB leds3[NUM_LEDS];
CRGB leds4[NUM_LEDS];

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("LCD_RGB Driver Demo (ESP32-P4)");
    Serial.println("==============================");
    Serial.println("Using LCD RGB parallel driver");

    // Standard FastLED API - driver auto-selected for ESP32-P4
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
