#ifndef FASTLED_RGBW_COLORIMETRIC
#define FASTLED_RGBW_COLORIMETRIC 1
#endif

#include <FastLED.h>

#define DATA_PIN 3
#define LED_TYPE SK6812
#define COLOR_ORDER GRB
#define NUM_LEDS 60
#define BRIGHTNESS 128

CRGB leds[NUM_LEDS];

void setup() {
    Serial.begin(115200);

    fl::shared_ptr<const fl::color::DiodeProfile> profile =
        fl::color::make_diode_profile(fl::color::kRgbwDefaultProfile);

    fl::Rgbw rgbw(
        fl::kRGBWDefaultColorTemp,
        fl::RGBW_MODE::kRGBWColorimetric,
        fl::EOrderW::W3,
        profile);

    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setRgbw(rgbw);

    FastLED.setBrightness(BRIGHTNESS);
}

void loop() {
    const uint32_t now = millis();

    for (int i = 0; i < NUM_LEDS; ++i) {
        const uint8_t wave = sin8((i * 255 / NUM_LEDS) + (now / 8));

        if (i < NUM_LEDS / 3) {
            leds[i] = CHSV(wave, 255, 255);
        } else if (i < (2 * NUM_LEDS) / 3) {
            leds[i] = CRGB(wave, wave, wave);
        } else {
            const uint8_t g = 120 + ((uint16_t)wave * 80 / 255);
            const uint8_t b = 40 + ((uint16_t)wave * 80 / 255);
            leds[i] = CRGB(255, g, b);
        }
    }

    FastLED.show();
    FastLED.delay(1000 / 60);
}
