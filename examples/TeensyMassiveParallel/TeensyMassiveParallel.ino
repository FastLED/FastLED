/// BasicTest example to demonstrate massive parallel output with FastLED using
/// ObjectFLED for Teensy 4.0/4.1.
///
/// This mode will support upto 42 parallel strips of WS2812 LEDS! ~7x that of OctoWS2811!
///
/// The theoritical limit of Teensy 4.0, if frames per second is not a concern, is
/// more than 200k pixels. However, realistically, to run 42 strips at 550 pixels
/// each at 60fps, is 23k pixels.
///
/// @author Kurt Funderburg
/// @reddit: reddit.com/u/Tiny_Structure_7
/// The FastLED code was written by Zach Vorhies

#if !defined(__IMXRT1062__) // Teensy 4.0/4.1 only.
void setup() {}
void loop() {}
#else

// As if FastLED 3.9.12, this is no longer needed for Teensy 4.0/4.1.
#define FASTLED_USES_OBJECTFLED

// Optional define to override the latch delay (microseconds)
// #define FASTLED_OBJECTFLED_LATCH_DELAY 75
#include "FastLED.h"
#include "fl/warn.h"

using namespace fl;

#define PIN_FIRST 3
#define PIN_SECOND 1
#define IS_RGBW false

#define NUM_LEDS1 (22 * 22)
#define NUM_LEDS2 1
CRGB leds1[NUM_LEDS1];
CRGB leds2[NUM_LEDS2];

void wait_for_serial(uint32_t timeout = 3000) {
    uint32_t end_timeout = millis();
    while (!Serial && end_timeout > millis()) {}
}

void print_startup_info() {
    Serial.println("Start");
    Serial.print("*********************************************\n");
    Serial.print("* TeensyParallel.ino                        *\n");
    Serial.print("*********************************************\n");
    Serial.printf(
        "CPU speed: %d MHz   Temp: %.1f C  %.1f F   Serial baud: %.1f MHz\n",
        F_CPU_ACTUAL / 1000000, tempmonGetTemp(),
        tempmonGetTemp() * 9.0 / 5.0 + 32, 800000 * 1.6 / 1000000.0);
}

void setup() {
    Serial.begin(115200);
    wait_for_serial(3000);
    CLEDController& c1 = FastLED.addLeds<WS2812, PIN_FIRST, GRB>(leds1, NUM_LEDS1);
    CLEDController& c2 = FastLED.addLeds<WS2812, PIN_SECOND, GRB>(leds2, NUM_LEDS2);
    if (IS_RGBW) {
        c1.setRgbw();
        c2.setRgbw();
    }
    FastLED.setBrightness(8);
}

void fill(CRGB color) {
    for (int i = 0; i < NUM_LEDS1; i++) {
        leds1[i] = color;
    }
    for (int i = 0; i < NUM_LEDS2; i++) {
        leds2[i] = color;
    }
}

void blink(CRGB color, int times, int delay_ms = 250) {
    for (int i = 0; i < times; ++i) {
        fill(color);
        FastLED.show();
        delay(delay_ms);
        fill(CRGB::Black);
        FastLED.show();
        delay(delay_ms);
    }
}

void loop() {
    blink(CRGB::Red, 1);
    blink(CRGB::Green, 2);
    blink(CRGB::Blue, 3);
    delay(500);
}

#endif //  __IMXRT1062__
