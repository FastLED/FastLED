/// @file Esp32P4ParlioDriver.h
/// @brief ESP32-P4 PARLIO parallel driver demo - Multi-strip LED output
///
/// This example demonstrates the ESP32-P4 Parallel IO peripheral for driving
/// multiple WS28xx LED strips simultaneously with hardware timing and DMA.
///
/// Key features:
/// - Drive 8 or 16 LED strips in parallel
/// - Hardware timing (no CPU bit-banging)
/// - DMA-based transmission (minimal CPU overhead)
/// - 120+ FPS for 256-pixel strips
///
/// Hardware Requirements:
/// - ESP32-P4 (PARLIO TX peripheral)
/// - Up to 8 or 16 WS28xx LED strips
/// - Shared ground between all strips
///
/// Notes:
/// - All strips must have the same number of LEDs
/// - Supports WS2812, WS2812B, WS2811, SK6812, etc.
/// - Clock frequency configurable (default 12 MHz)

#pragma once

#include <FastLED.h>

#if defined(CONFIG_IDF_TARGET_ESP32P4)
#include "platforms/esp/32/clockless_parlio_esp32p4.h"
#endif

// Configuration
#define NUM_STRIPS 8
#define NUM_LEDS 256

// GPIO pins for LED strips (data lanes)
#define PIN0  1
#define PIN1  2
#define PIN2  3
#define PIN3  4
#define PIN4  5
#define PIN5  6
#define PIN6  7
#define PIN7  8

// Clock pin for PARLIO
#define CLK_PIN  9

// LED arrays for each strip
CRGB leds[NUM_STRIPS][NUM_LEDS];

#if defined(CONFIG_IDF_TARGET_ESP32P4)

// PARLIO driver instance
fl::ParlioLedDriver<NUM_STRIPS, fl::WS2812ChipsetTiming> parlio_driver;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("FastLED ESP32-P4 PARLIO Driver Demo");
    Serial.println("====================================");
    Serial.printf("Num strips: %d\n", NUM_STRIPS);
    Serial.printf("LEDs per strip: %d\n", NUM_LEDS);

    // Configure PARLIO driver
    fl::ParlioDriverConfig config;
    config.clk_gpio = CLK_PIN;
    config.num_lanes = NUM_STRIPS;
    config.clock_freq_hz = 12000000;  // 12 MHz
    config.data_gpios[0] = PIN0;
    config.data_gpios[1] = PIN1;
    config.data_gpios[2] = PIN2;
    config.data_gpios[3] = PIN3;
    config.data_gpios[4] = PIN4;
    config.data_gpios[5] = PIN5;
    config.data_gpios[6] = PIN6;
    config.data_gpios[7] = PIN7;

    // Initialize driver
    if (!parlio_driver.begin(config, NUM_LEDS)) {
        Serial.println("ERROR: Failed to initialize PARLIO driver!");
        while (1) {
            delay(1000);
        }
    }

    // Set LED strip pointers for each channel
    for (int i = 0; i < NUM_STRIPS; i++) {
        parlio_driver.set_strip(i, leds[i]);
    }

    Serial.println("\nReady!");
}

void loop() {
    static uint8_t hue = 0;

    EVERY_N_MILLIS(1000) {
        Serial.println("Loop!");
    }

    // Update each strip with different patterns
    for (int strip = 0; strip < NUM_STRIPS; strip++) {
        switch (strip % 4) {
            case 0:
                // Rainbow
                fill_rainbow(leds[strip], NUM_LEDS, hue + (strip * 32), 7);
                break;

            case 1:
                // Solid color
                fill_solid(leds[strip], NUM_LEDS, CHSV(hue + (strip * 32), 255, 255));
                break;

            case 2:
                // Chase pattern
                fill_solid(leds[strip], NUM_LEDS, CRGB::Black);
                leds[strip][beatsin16(60 + strip * 10, 0, NUM_LEDS-1)] = CRGB::White;
                break;

            case 3:
                // Pulse
                fill_solid(leds[strip], NUM_LEDS, CHSV(hue + (strip * 32), 255, beatsin8(60 + strip * 10)));
                break;
        }
    }

    // Show all strips simultaneously
    parlio_driver.show<fl::GRB>();
    parlio_driver.wait();

    hue++;
}

#else

// Fallback for non-ESP32-P4 platforms
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("ERROR: This example requires ESP32-P4!");
    Serial.println("PARLIO peripheral not available on this platform.");
}

void loop() {
    delay(1000);
}

#endif
