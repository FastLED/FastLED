/// Massive parallel output example using the ObjectFLED driver on Teensy 4.0/4.1.
///
/// ObjectFLED (Bus::FLEX_IO slot 0) drives the strips in parallel via DMA,
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
#include "fl/log/log.h"


// Hardware configuration
#define PIN_STRIP1 3
#define PIN_STRIP2 1
#define PIN_STRIP3 4

#define NUM_LEDS 100
fl::CRGB strip1[NUM_LEDS];
fl::CRGB strip2[NUM_LEDS];
fl::CRGB strip3[NUM_LEDS];

void wait_for_serial(uint32_t timeout = 3000) {
    uint32_t start = millis();
    while (!Serial && (millis() - start) < timeout) {}
}

void print_startup_info() {
    Serial.println("\n*********************************************");
    Serial.println("* TeensyMassiveParallel - ObjectFLED        *");
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

    // Route every clockless strip through ObjectFLED (Bus::FLEX_IO slot 0
    // on Teensy 4.x), which drives all strips in parallel via DMA.
    FastLED.setExclusiveDriver<fl::Bus::FLEX_IO, 0>();

    // Per-strip color correction is set on each controller.
    FastLED.addLeds<WS2812B, PIN_STRIP1, GRB>(strip1, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<WS2812B, PIN_STRIP2, GRB>(strip2, NUM_LEDS).setCorrection(TypicalSMD5050);
    FastLED.addLeds<WS2812B, PIN_STRIP3, GRB>(strip3, NUM_LEDS).setCorrection(UncorrectedColor);

    FastLED.setBrightness(8);
    print_startup_info();
}

void fill_all(fl::CRGB color) {
    for (int i = 0; i < NUM_LEDS; i++) {
        strip1[i] = color;
        strip2[i] = color;
        strip3[i] = color;
    }
}

void fill_strip(fl::CRGB* strip, fl::CRGB color) {
    for (int i = 0; i < NUM_LEDS; i++) {
        strip[i] = color;
    }
}

void blink_all(fl::CRGB color, int times, int delay_ms = 250) {
    for (int i = 0; i < times; ++i) {
        fill_all(color);
        FastLED.show();
        delay(delay_ms);
        fill_all(fl::CRGB::Black);
        FastLED.show();
        delay(delay_ms);
    }
}

void chase_pattern() {
    // Chase different colors across the three strips
    fill_strip(strip1, fl::CRGB::Red);
    fill_strip(strip2, fl::CRGB::Black);
    fill_strip(strip3, fl::CRGB::Black);
    FastLED.show();
    delay(300);

    fill_strip(strip1, fl::CRGB::Black);
    fill_strip(strip2, fl::CRGB::Green);
    fill_strip(strip3, fl::CRGB::Black);
    FastLED.show();
    delay(300);

    fill_strip(strip1, fl::CRGB::Black);
    fill_strip(strip2, fl::CRGB::Black);
    fill_strip(strip3, fl::CRGB::Blue);
    FastLED.show();
    delay(300);
}

void loop() {
    // Blink all strips simultaneously
    blink_all(fl::CRGB::White, 1, 200);

    // Chase pattern across strips
    chase_pattern();

    delay(500);
}

#endif //  __IMXRT1062__
