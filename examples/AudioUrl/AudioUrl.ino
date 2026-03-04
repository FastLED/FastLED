// AudioUrl - Demonstrates loading audio from a URL in the WASM UI.
//
// When running in the FastLED web compiler, the browser will automatically
// load and play the specified MP3 URL on page load. Audio samples drive
// a simple volume-to-brightness visualization.
//
// Usage:
//   pip install fastled
//   cd examples/AudioUrl
//   fastled

#include <FastLED.h>
#include "fl/ui.h"
#include "fl/stl/url.h"

#define NUM_LEDS 64
#define LED_PIN 2

CRGB leds[NUM_LEDS];

// Provide an MP3 URL -- the WASM frontend will auto-load and auto-play it.
fl::UIAudio audio("Audio",
    fl::url("https://www.soundhelix.com/examples/mp3/SoundHelix-Song-1.mp3"));

void setup() {
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(128);
}

void loop() {
    // Drain all available audio samples, keeping track of peak volume.
    int32_t peak = 0;
    while (fl::AudioSample sample = audio.next()) {
        for (size_t i = 0; i < sample.pcm().size(); ++i) {
            int32_t v = abs(sample.pcm()[i]);
            if (v > peak) {
                peak = v;
            }
        }
    }

    // Map peak amplitude to brightness (0-255).
    uint8_t brightness = map(peak, 0, 32768, 0, 255);
    fill_solid(leds, NUM_LEDS, CHSV(160, 255, brightness));

    FastLED.show();
}
