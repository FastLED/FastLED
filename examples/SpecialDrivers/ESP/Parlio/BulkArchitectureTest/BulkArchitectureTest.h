#pragma once

#ifdef ESP32

#define FASTLED_USES_ESP32P4_PARLIO  // Enable PARLIO driver for ESP32-P4/S3

#include "FastLED.h"

// ===== Test Configuration =====

// Test phases (cycled in loop)
enum TestPhase {
    TEST_SINGLE_STRIP = 0,      // Test 1: Single WS2812 strip
    TEST_MULTI_STRIP_BATCH,     // Test 2: Multiple WS2812 strips (batching)
    TEST_CHIPSET_CHANGE,        // Test 3: Mixed WS2812 + SK6812 (different timings)
    TEST_PHASE_COUNT            // Number of test phases
};

// Pin definitions - Choose GPIO pins that support PARLIO output
#define PIN0 1
#define PIN1 2
#define PIN2 3
#define PIN3 4

// LED configuration
#define NUM_LEDS_PER_STRIP 50
#define MAX_STRIPS 4

// LED arrays - separate arrays for different test phases
CRGB leds_test1[NUM_LEDS_PER_STRIP];                          // Test 1: Single strip
CRGB leds_test2[NUM_LEDS_PER_STRIP * MAX_STRIPS];            // Test 2: Multi-strip
CRGB leds_test3_ws2812[NUM_LEDS_PER_STRIP * 2];              // Test 3: WS2812 strips
CRGB leds_test3_sk6812[NUM_LEDS_PER_STRIP];                  // Test 3: SK6812 strip

// Current test phase
TestPhase current_phase = TEST_SINGLE_STRIP;
uint32_t phase_start_time = 0;
const uint32_t PHASE_DURATION_MS = 5000;  // 5 seconds per test phase

// ===== Helper Functions =====

void clear_all_leds() {
    for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) {
        leds_test1[i] = CRGB::Black;
    }
    for (int i = 0; i < NUM_LEDS_PER_STRIP * MAX_STRIPS; i++) {
        leds_test2[i] = CRGB::Black;
    }
    for (int i = 0; i < NUM_LEDS_PER_STRIP * 2; i++) {
        leds_test3_ws2812[i] = CRGB::Black;
    }
    for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) {
        leds_test3_sk6812[i] = CRGB::Black;
    }
}

void setup_test_phase(TestPhase phase) {
    // Clear all controllers
    FastLED.clear();

    Serial.println();
    Serial.println("====================================");

    switch (phase) {
        case TEST_SINGLE_STRIP:
            Serial.println("TEST 1: Single Strip Transmission");
            Serial.println("  - 1x WS2812 strip (50 LEDs)");
            Serial.println("  - Validates basic PARLIO operation");
            Serial.println("  - Expected: Red → Green → Blue cycle");

            // Single WS2812 strip
            FastLED.addLeds<WS2812, PIN0, GRB>(leds_test1, NUM_LEDS_PER_STRIP);
            break;

        case TEST_MULTI_STRIP_BATCH:
            Serial.println("TEST 2: Multi-Strip Batching");
            Serial.println("  - 4x WS2812 strips (50 LEDs each)");
            Serial.println("  - Validates hub coordination & batching");
            Serial.println("  - Expected: Sequential rainbow on all strips");

            // Four WS2812 strips - should batch together (same chipset timing)
            FastLED.addLeds<WS2812, PIN0, GRB>(leds_test2 + (0 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP);
            FastLED.addLeds<WS2812, PIN1, GRB>(leds_test2 + (1 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP);
            FastLED.addLeds<WS2812, PIN2, GRB>(leds_test2 + (2 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP);
            FastLED.addLeds<WS2812, PIN3, GRB>(leds_test2 + (3 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP);
            break;

        case TEST_CHIPSET_CHANGE:
            Serial.println("TEST 3: Chipset Change");
            Serial.println("  - 2x WS2812 strips (50 LEDs each)");
            Serial.println("  - 1x SK6812 strip (50 LEDs)");
            Serial.println("  - Validates different timing configs");
            Serial.println("  - Expected: WS2812=Red, SK6812=Blue");

            // Mix of chipsets - should create separate groups due to different timings
            FastLED.addLeds<WS2812, PIN0, GRB>(leds_test3_ws2812 + (0 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP);
            FastLED.addLeds<WS2812, PIN1, GRB>(leds_test3_ws2812 + (1 * NUM_LEDS_PER_STRIP), NUM_LEDS_PER_STRIP);
            FastLED.addLeds<SK6812, PIN2, GRB>(leds_test3_sk6812, NUM_LEDS_PER_STRIP);
            break;

        default:
            break;
    }

    FastLED.setBrightness(32);  // Moderate brightness
    Serial.println("====================================");
    Serial.println();
}

void run_test_animation(TestPhase phase, uint32_t elapsed_ms) {
    // Animation time (0.0 - 1.0 over phase duration)
    float t = float(elapsed_ms) / float(PHASE_DURATION_MS);

    switch (phase) {
        case TEST_SINGLE_STRIP: {
            // Simple RGB cycle
            uint8_t cycle_pos = uint8_t(t * 3.0f) % 3;
            CRGB color = CRGB::Black;

            if (cycle_pos == 0) color = CRGB::Red;
            else if (cycle_pos == 1) color = CRGB::Green;
            else color = CRGB::Blue;

            for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) {
                leds_test1[i] = color;
            }
            break;
        }

        case TEST_MULTI_STRIP_BATCH: {
            // Rainbow effect across all strips
            static uint8_t hue_offset = 0;
            for (int strip = 0; strip < MAX_STRIPS; strip++) {
                for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) {
                    int idx = i + (strip * NUM_LEDS_PER_STRIP);
                    uint8_t hue = (i * 256 / NUM_LEDS_PER_STRIP) + hue_offset + (strip * 64);
                    leds_test2[idx] = CHSV(hue, 255, 255);
                }
            }
            hue_offset += 2;
            break;
        }

        case TEST_CHIPSET_CHANGE: {
            // WS2812 strips: Red with breathing
            uint8_t brightness = uint8_t(127.5f + 127.5f * sin(t * 2.0f * PI));
            for (int i = 0; i < NUM_LEDS_PER_STRIP * 2; i++) {
                leds_test3_ws2812[i] = CRGB(brightness, 0, 0);
            }

            // SK6812 strip: Blue with breathing (opposite phase)
            uint8_t brightness2 = uint8_t(127.5f + 127.5f * sin((t * 2.0f * PI) + PI));
            for (int i = 0; i < NUM_LEDS_PER_STRIP; i++) {
                leds_test3_sk6812[i] = CRGB(0, 0, brightness2);
            }
            break;
        }

        default:
            break;
    }
}

// ===== Arduino Setup/Loop =====

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("FastLED PARLIO Bulk Architecture Test");
    Serial.println("========================================");
    Serial.println();
    Serial.println("This test validates the bulk architecture:");
    Serial.println("  - Test 1: Single strip transmission");
    Serial.println("  - Test 2: Multi-strip batching");
    Serial.println("  - Test 3: Chipset changes");
    Serial.println();
    Serial.println("Each test runs for 5 seconds, then cycles to next");
    Serial.println("========================================");

    // Initialize first test phase
    clear_all_leds();
    setup_test_phase(current_phase);
    phase_start_time = millis();
}

void loop() {
    uint32_t now = millis();
    uint32_t elapsed = now - phase_start_time;

    // Check if it's time to switch test phases
    if (elapsed >= PHASE_DURATION_MS) {
        // Move to next phase
        current_phase = TestPhase((current_phase + 1) % TEST_PHASE_COUNT);
        clear_all_leds();
        setup_test_phase(current_phase);
        phase_start_time = now;
        elapsed = 0;
    }

    // Run current test animation
    run_test_animation(current_phase, elapsed);

    // Show LEDs - this triggers the bulk architecture:
    // Controller → ParlioGroup → ParlioHub → ParlioEngine
    FastLED.show();

    delay(16);  // ~60 FPS
}

#else  // ESP32

// Non-ESP32 platform - simple fallback
#include "FastLED.h"

#define NUM_LEDS 16
#define DATA_PIN 3

CRGB leds[NUM_LEDS];

void setup() {
    Serial.begin(115200);
    Serial.println("BulkArchitectureTest requires ESP32-P4 or ESP32-S3");
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
}

void loop() {
    fill_rainbow(leds, NUM_LEDS, 0, 7);
    FastLED.show();
    delay(50);
}

#endif  // ESP32
