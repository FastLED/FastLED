// https://github.com/FastLED/FastLED/issues/1653


#define FASTLED_ALL_PINS_HARDWARE_SPI
#include <FastLED.h>

#define LED_DI 40  // Data pin for the APA102 LED
#define LED_CI 39  // Clock pin for the APA102 LED
#define NUM_LEDS 1 // The number of LEDs in your APA102 strip

// Define the array of leds
CRGB leds[NUM_LEDS];

void setup()
{
    // Initialize the LED stripThe sketch is:
    FastLED.addLeds<APA102, LED_DI, LED_CI, BGR>(leds, NUM_LEDS);
    // set master brightness control
    FastLED.setBrightness(64);
}

void loop()
{
    static uint8_t hue = 0;
    // Set the first led to the current hue
    leds[0] = CHSV(hue, 255, 255);
    // Show the leds
    FastLED.show();
    // Increase the hue
    hue++;
    // Wait a little bit before the next loop
    delay(20); // This delay controls the speed at which hues change, lower value will cause faster hue rotation.
}