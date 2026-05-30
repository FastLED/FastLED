// ok standalone
//
// Minimal FastLED sketch for fbuild native-QEMU smoke test.
// Must print FBUILD-QEMU-TEST-OK once early init completes so the QEMU
// runner's halt-on-success matcher can exit the emulator cleanly.
//
// Extension: .cpp (not .ino). PlatformIO handles both identically for
// src/ files. The .cpp extension keeps this test fixture out of
// arduino-lint-action's "Sketch(es) found outside examples and extras
// folders" check (rule LD003), which fires on .ino files anywhere in
// the library tree.
//
// `// ok standalone` above suppresses FastLED's TestPathStructureChecker
// and HeadersExistChecker: this file is a *fixture* compiled inside the
// standalone `tests/fbuild_qemu_smoke/` PlatformIO project (see
// `ci/tests/test_fbuild_qemu.py`), NOT a unit-test that mirrors a `src/`
// header — there is no `src/fbuild_qemu_smoke/...` for it to pair with.

#include <Arduino.h>  // ok include
#include <FastLED.h>  // ok include

#define NUM_LEDS 1
#define PIN_DATA 5

CRGB leds[NUM_LEDS];

void setup() {
    Serial.begin(115200);
    Serial.println("FBUILD-QEMU-TEST-BOOT");

    FastLED.addLeds<WS2812, PIN_DATA, GRB>(leds, NUM_LEDS);
    leds[0] = CRGB::Red;
    FastLED.show();

    Serial.println("FBUILD-QEMU-TEST-OK");
}

void loop() {
    delay(1000);
}
