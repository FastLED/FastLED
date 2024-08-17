// https://github.com/FastLED/FastLED/issues/1653


#define FASTLED_ALL_PINS_HARDWARE_SPI
#include <FastLED.h>

#if defined(__AVR_ATmega328P__)
# define DATA_PIN 11
# define CLOCK_PIN 13
#elif defined(__AVR_ATmega2560__)
# define DATA_PIN 51
# define CLOCK_PIN 52
#elif defined(__AVR_ATmega32U4__)
# define DATA_PIN 17
# define CLOCK_PIN 21
#elif defined(__AVR_ATmega1284P__)
# define DATA_PIN 11
# define CLOCK_PIN 13
#elif defined(__AVR_ATmega1280__)
# define DATA_PIN 51
# define CLOCK_PIN 52
// teensy 3.0
#elif defined(__MK20DX128__)
# define DATA_PIN 2
# define CLOCK_PIN 14
// teensy 3.1
#elif defined(__MK20DX256__)
# define DATA_PIN 2
# define CLOCK_PIN 14
// teensy 3.2
#elif defined(__MK64FX512__)
# define DATA_PIN 2
# define CLOCK_PIN 14
// teensy 4.1
#elif defined(__IMXRT1062__)
# define DATA_PIN 2
# define CLOCK_PIN 14
// esp32
#elif defined(ESP32)
# define DATA_PIN 23
# define CLOCK_PIN 18
// esp8266
#elif defined(ESP8266)
# define DATA_PIN 13
# define CLOCK_PIN 14
// esp32-c3
#elif defined(ESP32C3)
# define DATA_PIN 23
# define CLOCK_PIN 18
#else
#define DATA_PIN 40  // Data pin for the APA102 LED
#define CLOCK_PIN 39  // Clock pin for the APA102 LED
#endif

#define NUM_LEDS 1 // The number of LEDs in your APA102 strip

// Define the array of leds
CRGB leds[NUM_LEDS];

void setup()
{
    // Initialize the LED stripThe sketch is:
    FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR>(leds, NUM_LEDS);
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