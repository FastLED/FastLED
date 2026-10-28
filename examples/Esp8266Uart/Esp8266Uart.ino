// @filter: (platform is esp8266)

// ESP8266 UART Driver Test Example
// Demonstrates the UART-based WS2812 driver for ESP8266. It improves stability
// under Wi-Fi load by using hardware UART instead of bit-banging.
//
// #4944: FASTLED_ESP8266_UART is retired -- the driver is selected by template
// and the header is always available, so no define is needed. The legacy
// controller path below keeps working; the channel API path is shown for
// reference.

#include <FastLED.h>

#define NUM_LEDS 150
#define DATA_PIN 2  // ESP8266 UART1 TX (GPIO2)

CRGB leds[NUM_LEDS];

void setup() {
    // Legacy controller path (no define needed now).
    // Uses hardware UART for stable timing under Wi-Fi load.
    FastLED.addLeds<fl::UARTController_ESP8266, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(50);

    // Channel API alternative -- select the UART engine by template:
    //   fl::ChannelConfigOf<fl::ClocklessChipset> cfg{clockless, span<CRGB>(leds, NUM_LEDS), GRB};
    //   FastLED.add<fl::Bus::UART, fl::ClocklessChipset>(cfg);
}

void loop() {
    // Rainbow animation
    fill_rainbow(leds, NUM_LEDS, millis() / 10);
    FastLED.show();
    delay(10);
}
