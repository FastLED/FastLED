
// This is an prototype example for the ObjectFLED library for massive pins on
// teensy40/41.

#if !defined(__IMXRT1062__) // Teensy 4.0/4.1 only.
void setup() {}
void loop() {}
#else

#include "platforms/arm/k20/clockless_objectfled.h"

#include <FastLED.h>
#include "fl/warn.h"

#include <iostream>

using namespace fl;


ClocklessController_ObjectFLED_WS2812<3, GRB> driver;
#define NUM_LEDS (22 * 22)
CRGB leds[NUM_LEDS];


void setup() {
    Serial.begin(115200);
    while(!Serial){}  //wait until the connection to the PC is established
    Serial.println("Start");
    Serial.print("*********************************************\n");
    Serial.print("* TeensyParallel.ino                        *\n");
    Serial.print("*********************************************\n");
    Serial.printf(
        "CPU speed: %d MHz   Temp: %.1f C  %.1f F   Serial baud: %.1f MHz\n",
        F_CPU_ACTUAL / 1000000, tempmonGetTemp(),
        tempmonGetTemp() * 9.0 / 5.0 + 32, 800000 * 1.6 / 1000000.0);

    // static CLEDController &addLeds(CLEDController *pLed, struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0);

    // FastLED.addController(&leds);
    //FastLED.addLeds(&leds, 3);
    CFastLED::addLeds(&driver, leds, NUM_LEDS);

    // blank all leds/ all channels

    // Second parallel pin group Teensy 4.0: 10,12,11,13,6,9,32,8,7
    // Third parallel pin group Teensy 4.0: 37, 36, 35, 34, 39, 38, 28, 31, 30
    // from FastLED docs
    //  FastLED.addLeds<NUM_CHANNELS, WS2812, 37, GRB>(leds2, PIX_PER_ROW *
    //  NUM_ROWS * NUM_PLANES / NUM_CHANNELS);  //4-strip parallel
    //  FastLED.setBrightness(5);
    //  FastLED.setCorrection(0xB0E0FF);
    /*
    fill_solid(blankLeds[0][0], 7 * 8 * 8, 0x0);
    fill_solid(testCube[0][0], NUM_PLANES * NUM_ROWS * PIX_PER_ROW, background);
    leds.begin(1.6, 72); // 1.6 ocervlock factor, 72uS LED latch delay
    leds.setBrightness(6);
    leds.setBalance(0xDAE0FF);

    blanks.begin(); // default timing 800KHz LED clk, 75uS latch
    blanks.show();
    while (Serial.read() == -1)
        ;
        */
} // setup()

void fill(CRGB color) {
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = color;
    }
}

void blink(CRGB color, int delay_ms = 250) {
    fill(color);
    FastLED.show();
    delay(delay_ms);
    fill(CRGB::Black);
    FastLED.show();
    delay(delay_ms);
}

void loop() {
    // leds.fill_solid(CRGB::Red);
    // leds.show();
    EVERY_N_SECONDS(1) {
        // Serial.println("loop");
        FASTLED_WARN("loop");
        // std::cout << "loop" << std::endl;
        // Serial.println("loop");
    }
    blink(CRGB::Red);
    //blink(CRGB::Green);
    //blink(CRGB::Blue);
    //delay(500);
    /*
    fill(CRGB::Red);
    FastLED.show();
    delay(250);
    fill(CRGB::Green);
    FastLED.show();
    delay(250);
    fill(CRGB::Blue);
    FastLED.show();
    delay(250);
    delay(500);
    */
} // loop()

#endif //  __IMXRT1062__
