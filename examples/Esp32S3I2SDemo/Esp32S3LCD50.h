/// @file Esp32S3LcdParallel.h
/// @brief ESP32-S2/S3 LCD parallel driver demo (alternative to I2S driver)
///
/// This demonstrates the LCD_CAM peripheral parallel driver for ESP32-S2/S3.
///
/// Supported platforms:
/// - ESP32-S2: LCD peripheral with I80 interface
/// - ESP32-S3: LCD_CAM peripheral with I80 interface
///
/// Key features:
/// - Memory-efficient: 3-word-per-bit encoding (6 bytes per bit)
/// - Automatic PCLK optimization per chipset
/// - PSRAM recommended for >500 LEDs per strip
/// - All 16 lanes must use the same chipset type
///
/// To use this driver instead of I2S:
///   #define USE_LCD_PARALLEL
///   #include "Esp32S3I2SDemo.h"

#pragma once

#ifdef ESP32

#include <FastLED.h>
#include "platforms/esp/32/clockless_lcd_i80_esp32.h"

#define NUMSTRIPS 16
#define NUM_LEDS_PER_STRIP 256

// GPIO pin assignments for 16 data lanes
const int PINS[NUMSTRIPS] = {
    1,   // B0 - Safe pin (avoids USB-JTAG conflict with GPIO19)
    45,  // B1
    21,  // B2
    6,   // B3
    7,   // B4
    8,   // G0
    9,   // G1
    10,  // G2
    11,  // G3
    12,  // G4
    13,  // G5
    14,  // R0
    15,  // R1
    16,  // R2
    17,  // R3
    18   // R4
};

// LED data arrays (one per strip)
CRGB leds[NUMSTRIPS][NUM_LEDS_PER_STRIP];

// I80 LCD driver instance (template-bound to WS2812 timing)
fl::LcdI80Driver<fl::WS2812ChipsetTiming> lcd_driver;

void setup_lcd() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("FastLED ESP32-S3 LCD Parallel Driver Demo");
    Serial.println("==========================================");

    // This is used so that you can see if PSRAM is enabled
    log_d("Total heap: %d", ESP.getHeapSize());
    log_d("Free heap: %d", ESP.getFreeHeap());
    log_d("Total PSRAM: %d", ESP.getPsramSize());  // If this prints out 0, then PSRAM is not enabled
    log_d("Free PSRAM: %d", ESP.getFreePsram());

    log_d("waiting 6 seconds before startup");
    delay(6000);  // Long delay for easier flashing during development

    // Configure driver
    fl::LcdDriverConfig config;
    config.num_lanes = NUMSTRIPS;

    // Assign GPIO pins
    for (int i = 0; i < NUMSTRIPS; i++) {
        config.gpio_pins[i] = PINS[i];
    }

    // Optional configuration
    config.latch_us = 300;      // Reset time (300 µs typical for WS2812)
    config.use_psram = true;    // Use PSRAM for buffers (recommended)

    // Initialize driver
    if (!lcd_driver.begin(config, NUM_LEDS_PER_STRIP)) {
        Serial.println("ERROR: Failed to initialize LCD driver!");
        while (1) {
            delay(1000);
        }
    }

    // Attach LED arrays to driver
    CRGB* strip_ptrs[NUMSTRIPS];
    for (int i = 0; i < NUMSTRIPS; i++) {
        strip_ptrs[i] = leds[i];
    }
    lcd_driver.attachStrips(strip_ptrs);

    // Print diagnostic information
    Serial.println("\nDriver Configuration:");
    Serial.printf("  Chipset: %s\n", fl::WS2812ChipsetTiming::name());
    Serial.printf("  PCLK: %u Hz (%u MHz)\n",
                  lcd_driver.getPclkHz(), lcd_driver.getPclkHz() / 1000000);
    Serial.printf("  Slot duration: %u ns\n",
                  lcd_driver.getPclkHz() > 0 ? 1000000000UL / lcd_driver.getPclkHz() : 0);
    Serial.printf("  Slots per bit: %u\n", lcd_driver.getSlotsPerBit());
    Serial.printf("  Buffer size: %u bytes (%u KB)\n",
                  lcd_driver.getBufferSize(), lcd_driver.getBufferSize() / 1024);
    Serial.printf("  Estimated frame time: %u µs\n", lcd_driver.getFrameTimeUs());

    uint32_t T1, T2, T3;
    lcd_driver.getActualTiming(T1, T2, T3);
    Serial.printf("\nTiming (actual):\n");
    Serial.printf("  T1: %u ns (target: %u ns)\n", T1, fl::WS2812ChipsetTiming::T1());
    Serial.printf("  T1+T2: %u ns (target: %u ns)\n", T1 + T2,
                  fl::WS2812ChipsetTiming::T1() + fl::WS2812ChipsetTiming::T2());
    Serial.printf("  T3: %u ns (target: %u ns)\n", T3, fl::WS2812ChipsetTiming::T3());

    float err_T1, err_T2, err_T3;
    lcd_driver.getTimingError(err_T1, err_T2, err_T3);
    Serial.printf("\nTiming errors:\n");
    Serial.printf("  T1: %.1f%%\n", err_T1 * 100.0f);
    Serial.printf("  T1+T2: %.1f%%\n", err_T2 * 100.0f);
    Serial.printf("  T3: %.1f%%\n", err_T3 * 100.0f);

    Serial.println("\nStarting animation...\n");
}

void setup() {
    setup_lcd();
}

void fill_rainbow_lcd(CRGB leds[][NUM_LEDS_PER_STRIP]) {
    static int s_offset = 0;
    for (int j = 0; j < NUMSTRIPS; j++) {
        for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) {
            leds[j][i] = CHSV((i + s_offset) % 256, 255, 255);
        }
    }
    s_offset++;
}

void loop() {
    static uint32_t frame_count = 0;
    static uint32_t last_fps_print = 0;

    fill_rainbow_lcd(leds);

    // Update display
    lcd_driver.show();
    lcd_driver.wait();  // Wait for transfer to complete

    frame_count++;

    // Print FPS every second
    uint32_t now = millis();
    if (now - last_fps_print >= 1000) {
        float fps = frame_count * 1000.0f / (now - last_fps_print);
        Serial.printf("FPS: %.1f (frame %u)\n", fps, frame_count);
        last_fps_print = now;
        frame_count = 0;
    }
}

#else  // ESP32

// Non-ESP32 platform - provide minimal example for compilation testing
#include "FastLED.h"

#define NUM_LEDS 16
#define DATA_PIN 3

CRGB leds[NUM_LEDS];

void setup() {
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
}

void loop() {
    fill_rainbow(leds, NUM_LEDS, 0, 7);
    FastLED.show();
    delay(50);
}

#endif  // ESP32
