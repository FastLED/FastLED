/// Massive parallel output example using BulkClockless<OFLED> for Teensy 4.0/4.1.
///
/// This example demonstrates the new BulkClockless API with OFLED (ObjectFLED) peripheral,
/// supporting up to 42 parallel strips on Teensy 4.1 or 16 strips on Teensy 4.0.
///
/// Key Features:
/// - DMA-driven multi-strip LED control (minimal CPU overhead)
/// - Per-strip color correction and temperature
/// - Automatic chipset timing (WS2812, SK6812, WS2811, etc.)
/// - Up to 23,000 pixels at 60fps (42 strips × 550 LEDs each)
///
/// @author Kurt Funderburg (original ObjectFLED)
/// @reddit: reddit.com/u/Tiny_Structure_7
/// FastLED integration by Zach Vorhies

#if !defined(__IMXRT1062__) // Teensy 4.0/4.1 only.
#include "platforms/sketch_fake.hpp"
#else

#include "FastLED.h"
#include "fl/warn.h"

using namespace fl;

// Hardware configuration
#define PIN_STRIP1 3
#define PIN_STRIP2 1
#define PIN_STRIP3 4

// All strips must have the same length in a single BulkClockless instance
#define NUM_LEDS 100
CRGB strip1[NUM_LEDS];
CRGB strip2[NUM_LEDS];
CRGB strip3[NUM_LEDS];

void wait_for_serial(uint32_t timeout = 3000) {
    uint32_t start = millis();
    while (!Serial && (millis() - start) < timeout) {}
}

void print_startup_info() {
    Serial.println("\n*********************************************");
    Serial.println("* TeensyMassiveParallel - BulkClockless     *");
    Serial.println("*********************************************");
    FL_DBG("CPU speed: " << (F_CPU_ACTUAL / 1000000) << " MHz   Temp: " << tempmonGetTemp()
           << " C  " << (tempmonGetTemp() * 9.0 / 5.0 + 32) << " F");
    Serial.print("Number of strips: 3\n");
    Serial.print("LEDs per strip: ");
    Serial.println(NUM_LEDS);
    Serial.print("Total LEDs: ");
    Serial.println(NUM_LEDS * 3);
}

void setup() {
    Serial.begin(115200);
    wait_for_serial(3000);

    // Add LED strips using the new BulkClockless API
    // All strips in a single instance must have the same length
    auto& bulk = FastLED.addBulkLeds<WS2812B, OFLED>({
        {PIN_STRIP1, strip1, NUM_LEDS, ScreenMap()},
        {PIN_STRIP2, strip2, NUM_LEDS, ScreenMap()},
        {PIN_STRIP3, strip3, NUM_LEDS, ScreenMap()}
    });

    // Optional: Set per-strip color correction
    bulk.get(PIN_STRIP1)->setCorrection(TypicalLEDStrip);
    bulk.get(PIN_STRIP2)->setCorrection(TypicalSMD5050);
    bulk.get(PIN_STRIP3)->setCorrection(UncorrectedColor);

    FastLED.setBrightness(8);
    print_startup_info();
}

void fill_all(CRGB color) {
    for (int i = 0; i < NUM_LEDS; i++) {
        strip1[i] = color;
        strip2[i] = color;
        strip3[i] = color;
    }
}

void fill_strip(CRGB* strip, CRGB color) {
    for (int i = 0; i < NUM_LEDS; i++) {
        strip[i] = color;
    }
}

void blink_all(CRGB color, int times, int delay_ms = 250) {
    for (int i = 0; i < times; ++i) {
        fill_all(color);
        FastLED.show();
        delay(delay_ms);
        fill_all(CRGB::Black);
        FastLED.show();
        delay(delay_ms);
    }
}

void chase_pattern() {
    // Chase different colors across the three strips
    fill_strip(strip1, CRGB::Red);
    fill_strip(strip2, CRGB::Black);
    fill_strip(strip3, CRGB::Black);
    FastLED.show();
    delay(300);

    fill_strip(strip1, CRGB::Black);
    fill_strip(strip2, CRGB::Green);
    fill_strip(strip3, CRGB::Black);
    FastLED.show();
    delay(300);

    fill_strip(strip1, CRGB::Black);
    fill_strip(strip2, CRGB::Black);
    fill_strip(strip3, CRGB::Blue);
    FastLED.show();
    delay(300);
}

void loop() {
    // Blink all strips simultaneously
    blink_all(CRGB::White, 1, 200);

    // Chase pattern across strips
    chase_pattern();

    delay(500);
}

#endif //  __IMXRT1062__
