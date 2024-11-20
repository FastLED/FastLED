#pragma once

#include <stdint.h>

#include "crgb.h"
#include "fixed_vector.h"
#include "namespace.h"
#include "singleton.h"

#include "third_party/yvez/I2SClocklessLedDriver.h"

#ifndef NUM_LEDS_PER_STRIP
#warning "NUM_LEDS_PER_STRIP not defined, using default 256"
#define NUM_LEDS_PER_STRIP 256
#endif

FASTLED_NAMESPACE_BEGIN

class YvezI2S {
  public:
    typedef FixedVector<uint8_t, 16> Pins;
    typedef CRGB CRGBArray[NUM_LEDS_PER_STRIP * 16];
    static Pins DefaultPins() {
        return Pins({0,2,4,5,12,13,14,15,16,18,19,21,22,23,25,26});
    }

    YvezI2S(const CRGBArray& leds, int clock_pin, int latch_pin,
            const Pins &pins = DefaultPins());

    void showPixels();
    void waitForDrawComplete();

    // Disable copy constructor and assignment operator
    YvezI2S() = delete;
    YvezI2S(YvezI2S &&) = delete;
    YvezI2S &operator=(const YvezI2S &) = delete;

  private:
    I2SClocklessLedDriver driver;
};

inline YvezI2S::YvezI2S(const CRGBArray& leds, int clock_pin, int latch_pin,
            const Pins &pins) {
    
}



FASTLED_NAMESPACE_END
