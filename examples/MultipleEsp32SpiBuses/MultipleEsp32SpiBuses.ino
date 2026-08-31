// @filter: (platform is esp32)

#include <FastLED.h>

#if !defined(ESP32)
#error "This example requires ESP32"
#endif

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
