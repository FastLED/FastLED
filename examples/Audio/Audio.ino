/// @file    Audio.ino
/// @brief   Audio reactive 180-LED circle with 3 segments responding to bass/mid/treble
/// @example Audio.ino

// @filter: (mem is high) and ((platform is teensy) or (platform is esp32))

#include <FastLED.h>

#if !SKETCH_HAS_LOTS_OF_MEMORY
void setup() {}
void loop() {}
#else

#include "fl/ui.h"

#define NUM_LEDS 180
#define LEDS_PER_SEGMENT 60
#define DATA_PIN 3

fl::CRGB leds[NUM_LEDS];
fl::UIAudio audio_ui("Audio Input");

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(128);
}

void loop() {
    fadeToBlackBy(leds, NUM_LEDS, 30);

    // Lazily registers with FastLED on first call, cached thereafter
    auto audio = audio_ui.processor();

    float bass = audio->getVibeBass();
    float mid = audio->getVibeMid();
    float treb = audio->getVibeTreb();

    // Segment 1 (LEDs 0-59): Bass - red/orange
    uint8_t bassVal = fl::clamp(bass * 200.0f, 0.0f, 255.0f);
    fill_solid(leds, LEDS_PER_SEGMENT, CHSV(0, 255, bassVal));

    // Segment 2 (LEDs 60-119): Mid - green
    uint8_t midVal = fl::clamp(mid * 200.0f, 0.0f, 255.0f);
    fill_solid(leds + LEDS_PER_SEGMENT, LEDS_PER_SEGMENT, CHSV(96, 255, midVal));

    // Segment 3 (LEDs 120-179): Treble - blue
    uint8_t trebVal = fl::clamp(treb * 200.0f, 0.0f, 255.0f);
    fill_solid(leds + 2 * LEDS_PER_SEGMENT, LEDS_PER_SEGMENT, CHSV(160, 255, trebVal));

    FastLED.show();
}

#endif
