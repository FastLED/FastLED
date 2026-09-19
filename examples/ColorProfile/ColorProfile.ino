// FL_AGENT_ALLOW_NEW_EXAMPLE -- the first example of the colour-profile API
// (ChannelOptions::setColorProfile), which had none. It is also the probe
// P9 (#4043) measures the colour pipeline's flash/RAM budget with: build it
// with and without -DFASTLED_EXAMPLE_NO_COLOR_PROFILE=1 and the difference is
// what binding a profile costs on that board.
//
// @filter: (memory is large)
//
// The Channels API alone is beyond an Uno's 32 KB. On small boards the colour
// pipeline is compiled out entirely (FL_COLOR_PROFILE_RUNTIME, and the
// shared-ownership parts under FL_COLOR_PIPELINE_SHARED), so they pay nothing
// for it.
//
// What binding a profile does: every pixel goes through a streaming colour
// pipeline -- decode the source (linear sRGB here), map it into the strip's
// own gamut, solve the drives each emitter needs -- so a colour written as
// CRGB lands at the same colour on the strip, rather than at whatever the
// strip's primaries happen to make of those numbers. Brightness is applied
// inside the pipeline as a linear flux scale, so dimming does not shift hue.

#include <FastLED.h>
#include "fl/channels/channel.h"
#include "fl/channels/config.h"
#include "fl/gfx/colorimetric_response.h"

#define NUM_LEDS 60
#ifndef PIN_DATA
#define PIN_DATA FL_PIN_CLOCKLESS_1  // a sketch or build flag still overrides
#endif

CRGB leds[NUM_LEDS];

void setup() {
    fl::ChannelOptions opts;
#if !defined(FASTLED_EXAMPLE_NO_COLOR_PROFILE)
    // A profile describes the strip: its primaries' chromaticities and
    // relative luminances. `WS2812B` is a placeholder until measured profiles
    // land (P10); pass your own `fl::EmitterProfile` for a characterised part.
    opts.setColorProfile(fl::profiles::WS2812B);
    // Optional: the pipeline's own temporal dither, for smoother fades at low
    // brightness (off by default once a profile is bound).
    opts.mDitherMode = BINARY_DITHER;
#endif
    FastLED.add(fl::ChannelConfig(
        fl::makeClockless<fl::TIMING_WS2812_800KHZ>(PIN_DATA),
        fl::span<CRGB>(leds, NUM_LEDS), GRB, opts));
    FastLED.setBrightness(64);
}

void loop() {
    // A slow hue sweep: with a profile bound, equal steps here are equal
    // steps of colour on the strip.
    const uint8_t hue = static_cast<uint8_t>(millis() / 32);
    fill_rainbow(leds, NUM_LEDS, hue, 255 / NUM_LEDS);
    FastLED.show();
    delay(16);
}
