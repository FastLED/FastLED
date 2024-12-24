/// BasicTest example to demonstrate massive parallel output with FastLED using
/// ObjectFLED for Teensy 4.0/4.1.
///
/// This mode will support upto 42 parallel strips of LEDS! ~7x that of OctoWS2811!
///
/// Caveats: This driver is a memory hog! In order to map the driver into the way that
///          FastLED works, we need to have multiple frame buffers. ObjectFLED
///          has it's own buffer which must be rectangular (i.e. all strips must be the same
///          number of LEDS). Since FastLED is flexible in this regard, we need to convert
///          the FastLED data into the rectangular ObjectFLED buffer. This is done by copying
///          the data, which means extending the buffer size to the LARGEST strip. The number of
///          of buffers in total is 3-4. This will be reduced in the future, but at the time of
///          this writing (2024-Dec-23rd), this is the case. We will reduce this in the future.
///
/// @author Kurt Funderburg
/// @reddit: reddit.com/u/Tiny_Structure_7
/// The FastLED code was written by Zach Vorhies

#if !defined(__IMXRT1062__) // Teensy 4.0/4.1 only.
void setup() {}
void loop() {}
#else

#define FASTLED_USES_OBJECTFLED

#include "FastLED.h"
#include "fl/warn.h"

#include <iostream>

using namespace fl;

#define PIN_FIRST 3
#define PIN_SECOND 1
#define IS_RGBW false

#define NUM_LEDS1 (22 * 22)
#define NUM_LEDS2 1
CRGB leds1[NUM_LEDS1];
CRGB leds2[NUM_LEDS2];

void wait_for_serial() {
    uint32_t end_timeout = millis() + 3000;
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

void dump_last_crash() {
  if (CrashReport) {
    Serial.println("CrashReport:");
    Serial.println(CrashReport);
  }
}

void setup() {
    Serial.begin(115200);
    wait_for_serial();
    dump_last_crash();
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
