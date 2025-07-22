/// @file    NetTest.ino
/// @brief   Network functionality test for FastLED WASM including fetch API
/// @author  FastLED Community
///
/// This sketch tests network functionality in FastLED WASM builds, specifically
/// the fetch API for making HTTP requests. It demonstrates multiple fetch
/// scenarios and provides visual feedback via LEDs.
///
/// For WASM: 
/// 1. Install FastLED: `pip install fastled`
/// 2. cd into this examples directory
/// 3. Run: `fastled NetTest.ino`
/// 4. Open the web page and check the browser console for fetch results
///
/// For other platforms:
/// Uses mock fetch responses for testing the API without network connectivity

#include "fl/warn.h"
#include <FastLED.h>
#include "platforms/wasm/js_fetch.h"

using namespace fl;

#define NUM_LEDS 10
#define DATA_PIN 2

CRGB leds[NUM_LEDS];

void setup() {
    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);

    // Set all LEDs to dark red
    fill_solid(leds, NUM_LEDS, CRGB(64, 0, 0)); // Dark red
    FastLED.show();

    FL_WARN("NetTest started - 10 LEDs set to dark red");
    FL_WARN("Will fetch http://fastled.io every 5 seconds");
}

void loop() {
    EVERY_N_SECONDS(5) {
        fl::fetch.get("http://fastled.io")
            .response([](const fl::string &content) {
                if (content.length() >= 100) {
                    FL_WARN("First 100 characters: " << content.substr(0, 100));
                } else {
                    FL_WARN("Response (" << content.length()
                                         << " chars): " << content);
                }
            });
    }

    // Keep LEDs dark red
    FastLED.show();
    delay(10);
}
