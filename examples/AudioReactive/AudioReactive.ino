// @filter: (mem is large)

#include "FastLED.h"

#define NUM_LEDS 60
#define LED_PIN 2

CRGB leds[NUM_LEDS];
fl::UIAudioReactive audio("Audio");

int bandToLeds(float level, int maxLeds) {
    int count = static_cast<int>(level * maxLeds);
    if (count < 0) {
        count = 0;
    }
    if (count > maxLeds) {
        count = maxLeds;
    }
    return count;
}

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
}

void loop() {
    float volume = audio.getVolume();
    float bass = audio.getBass();
    float mid = audio.getMid();
    float treble = audio.getTreble();
    bool beat = audio.isBeat();

    fadeToBlackBy(leds, NUM_LEDS, 10);

    int center = NUM_LEDS / 2;

    if (beat) {
        fill_solid(leds, NUM_LEDS, CRGB::White);
    } else {
        int bassLeds = bandToLeds(bass, center);
        int midLeds = bandToLeds(mid, center);
        int trebleLeds = bandToLeds(treble, center);

        for (int i = 0; i < bassLeds; ++i) {
            leds[center + i] += CHSV(0, 255, 150);
            leds[center - 1 - i] += CHSV(0, 255, 150);
        }
        for (int i = 0; i < midLeds; ++i) {
            leds[center + i] += CHSV(96, 255, 100);
            leds[center - 1 - i] += CHSV(96, 255, 100);
        }
        for (int i = 0; i < trebleLeds; ++i) {
            leds[center + i] += CHSV(160, 255, 50);
            leds[center - 1 - i] += CHSV(160, 255, 50);
        }
    }

    if (volume > 1.0f) {
        volume = 1.0f;
    }
    FastLED.setBrightness(static_cast<uint8_t>(volume * 255.0f));
    FastLED.show();
    delay(16);
}
