#pragma once

#ifdef ESP32

#define FASTLED_USES_ESP32P4_PARLIO  // Enable PARLIO driver for ESP32-P4/S3

#include "FastLED.h"

// Configuration: 4 parallel LED strips with 256 LEDs each
// PARLIO driver will use 4-bit width for parallel transmission
#define NUM_STRIPS 4
#define NUM_LEDS_PER_STRIP 256
#define NUM_LEDS (NUM_LEDS_PER_STRIP * NUM_STRIPS)

// Pin definitions - Choose GPIO pins that support PARLIO output
// These are example pins; adjust based on your ESP32-P4/S3 board layout
#define PIN0 1
#define PIN1 2
#define PIN2 3
#define PIN3 4

// LED array - All strips stored in a single contiguous array
CRGB leds[NUM_LEDS];

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("FastLED ESP32-P4 PARLIO Driver Demo");
    Serial.println("====================================");
    Serial.println("Features:");
    Serial.println("  - Hardware DMA transmission (near-zero CPU usage)");
    Serial.println("  - Parallel output to 4 strips simultaneously");
    Serial.println("  - Internal 3.2 MHz clock (no external clock needed)");
    Serial.println("  - WS2812 waveform generation in hardware");
    Serial.println();

    // Just use FastLED.addLeds like normal!
    // The PARLIO driver automatically:
    //   - Generates precise WS2812 timing waveforms (T0H/T0L, T1H/T1L)
    //   - Selects optimal bit width (4-bit for 4 strips)
    //   - Handles DMA chunking for large LED counts
    //   - Uses internal 3.2 MHz clock for 800kHz WS2812 data rate
    FastLED.addLeds<WS2812, PIN0, GRB>(leds + (0 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, PIN1, GRB>(leds + (1 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, PIN2, GRB>(leds + (2 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812, PIN3, GRB>(leds + (3 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP);

    FastLED.setBrightness(32);  // Moderate brightness for demo

    Serial.println("Setup complete!");
    Serial.print("Total LEDs: ");
    Serial.println(NUM_LEDS);
    Serial.print("Expected frame rate: ~");
    // Frame time = num_leds × 30μs (3 bytes × 10μs per byte)
    Serial.print(1000000 / (NUM_LEDS_PER_STRIP * 30));
    Serial.println(" FPS");
    Serial.println("\nRunning rainbow animation...");
}

void fill_rainbow_all_strips(CRGB* all_leds) {
    static int s_offset = 0;
    for (int j = 0; j < NUM_STRIPS; j++) {
        for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) {
            int idx = i + NUM_LEDS_PER_STRIP * j;
            all_leds[idx] = CHSV((i + s_offset) & 0xFF, 255, 255);
        }
    }
    s_offset++;
}

void loop() {
    fill_rainbow_all_strips(leds);
    FastLED.show();  // Magic happens here!
}

#else  // ESP32

// Non-ESP32 platform
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
