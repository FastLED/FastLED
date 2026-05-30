// FL_AGENT_ALLOW_NEW_EXAMPLE — first example sketch for the 5-channel
// RGB + warm-W + cool-W path landed in PR #2560 (issue #2558).
//
// @filter: (memory is large)
//
// Pulls in the full Channels API plus the colorimetric RGB→RGBWW solvers;
// total code size is ~54 KB on AVR (overflows attiny85's 8 KB and uno's
// 32 KB flash). Restrict CI compile matrix to boards with >= "large"
// memory tier (Apollo3 / nRF52 / SAMD21+ / SAMD51 / STM32F4+ /
// teensy 3.5+ / teensy 4.x / ESP32 / native / web / rp2040).
//
// Add the following to platformio.ini for full colorimetric output:
//     build_flags =
//         -DFASTLED_RGBW_COLORIMETRIC=1     ; provides the underlying math
//
// Without that flag the channel falls back to 5 zero bytes per pixel + a
// one-time startup warning. The example still compiles either way; the
// 5-channel pipeline itself is always available — gc-sections drops it for
// sketches that don't configure an Rgbww channel.

#include <FastLED.h>
#include "fl/channels/channel.h"
#include "fl/channels/config.h"
#include "fl/gfx/rgbww.h"

#define NUM_LEDS 60
#define DATA_PIN  3

CRGB leds[NUM_LEDS];

void setup() {
    Serial.begin(115200);
    delay(2000);  // Upload safety delay.

    // ----- Channels API setup ------------------------------------------------
    // ChannelOptions.mWhiteCfg is a fl::variant<fl::Empty, Rgbw, Rgbww>.
    // Assigning Rgbww selects the 5-channel alternative; everything downstream
    // (encoder, dispatch, driver) routes through the RGBWW pipeline.
    fl::ChannelOptions opts;
    opts.mCorrection = TypicalSMD5050;
    opts.mWhiteCfg = fl::Rgbww(
        /* warm_cct */    2700,
        /* cool_cct */    6500,
        /* rgbww_mode */  fl::RGBWW_MODE::kRGBWWColorimetric,
        /* placement */   fl::EOrderWW::WwWcEnd     // RGB, then warm-W, cool-W
    );

    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    FastLED.add(fl::ChannelConfig(
        DATA_PIN, timing,
        fl::span<CRGB>(leds, NUM_LEDS),
        GRB,
        opts));

    FastLED.setBrightness(128);
}

void loop() {
    // Animate a slow warm <-> cool transition. The colorimetric solver
    // chooses the warm/cool blend per pixel based on the input chromaticity,
    // so writing CRGB stays the same as a normal sketch — only the wire
    // format and color science change.
    uint32_t ms = millis();
    uint8_t blend = sin8(ms / 16);  // 0..255 triangle-ish

    // Lerp between a warm white tint (255, 200, 130) and a cool one (180, 200, 255).
    CRGB warm(255, 200, 130);
    CRGB cool(180, 200, 255);
    CRGB mixed;
    mixed.r = lerp8by8(warm.r, cool.r, blend);
    mixed.g = lerp8by8(warm.g, cool.g, blend);
    mixed.b = lerp8by8(warm.b, cool.b, blend);

    fill_solid(leds, NUM_LEDS, mixed);
    FastLED.show();
    delay(20);
}
