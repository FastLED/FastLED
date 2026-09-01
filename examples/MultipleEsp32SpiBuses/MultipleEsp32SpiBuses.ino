/// @file    MultipleEsp32SpiBuses.ino
/// @brief   Drive two APA102 strips from two different ESP32 SPI buses
/// @example MultipleEsp32SpiBuses.ino
///
/// Demonstrates selecting the SPI host per controller, so two clocked strips
/// can run on independent hardware buses instead of sharing one.

#include <FastLED.h>

// The CI example matrix compiles every sketch for every board, so an
// ESP32-only sketch has to compile away to nothing elsewhere rather than
// `#error` out -- the same shape ParallelSPI.ino uses. Without this, `uno all`
// fails on this file and reports a build error that looks like a toolchain
// problem rather than an example that was never meant for AVR.
#if defined(ESP32)

CRGB spi2Leds[8];
CRGB spi3Leds[8];

void setup() {
    FastLED.addLeds<APA102, 11, 12, RGB, DATA_RATE_MHZ(20)>(
        spi2Leds, 8, fl::Esp32SpiBus::SPI2);
    FastLED.addLeds<APA102, 13, 14, RGB, DATA_RATE_MHZ(20)>(
        spi3Leds, 8, fl::Esp32SpiBus::SPI3);
}

void loop() {
    FastLED.show();
}

#else
void setup() {}
void loop() {}
#endif // defined(ESP32)
