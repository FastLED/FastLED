// @filter: (mem is large) and ((platform is teensy) or (platform is esp32))

// Audio Reactive LEDs using FastLED.add(Config)
// Demonstrates the auto-pumped audio pipeline: mic → processor → callbacks → LEDs

#include "FastLED.h"

#if defined(FL_IS_TEENSY)
// Keep fbuild's library scanner aware of PJRC Audio sources for Teensy.
#include <Audio.h>
#endif

#define NUM_LEDS 60
#define LED_PIN 2

// I2S pins for INMP441 microphone (adjust for your board)
#define I2S_WS  7
#define I2S_SD  8
#define I2S_CLK 4

CRGB leds[NUM_LEDS];
fl::shared_ptr<fl::audio::Processor> audio;

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(128);

    auto config = fl::audio::Config::CreateInmp441(I2S_WS, I2S_SD, I2S_CLK, fl::audio::AudioChannel::Right);
    audio = FastLED.add(config);
    audio->setGain(2.0f);  // Boost input by 2x

    // Flash white on every beat
    audio->onBeat([] {
        fill_solid(leds, NUM_LEDS, CRGB::White);
    });

    // Map bass level to hue
    audio->onBass([](float level) {
        uint8_t hue = static_cast<uint8_t>(level * 160);
        fill_solid(leds, NUM_LEDS, CHSV(hue, 255, 255));
    });

    // Dim to black on silence
    audio->onSilenceStart([] {
        fill_solid(leds, NUM_LEDS, CRGB::Black);
    });
}

void loop() {
    fadeToBlackBy(leds, NUM_LEDS, 20);
    FastLED.show();
}
