// @filter: (platform is esp8266)

// ESP8266 UART Driver Test Example
// Demonstrates the opt-in UART-based WS2812 driver for ESP8266
// This driver improves stability under Wi-Fi load by using hardware UART
// instead of bit-banging.

#define FASTLED_ESP8266_UART  // Enable UART driver before including FastLED
#include <FastLED.h>

#define NUM_LEDS 150
#define DATA_PIN 2  // ESP8266 UART1 TX (GPIO2)

CRGB leds[NUM_LEDS];

void setup() {
    // Initialize UART-based LED controller
    // Uses hardware UART for stable timing under Wi-Fi load
    FastLED.addLeds<fl::UARTController_ESP8266, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(50);
}

void loop() {
    // Rainbow animation
    fill_rainbow(leds, NUM_LEDS, millis() / 10);
    FastLED.show();
    delay(10);
}
