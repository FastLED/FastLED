#pragma once

#include <stdint.h>

#include "crgb.h"
#include "fixed_vector.h"
#include "namespace.h"
#include "singleton.h"
#include "pixelset.h"


#include "third_party/yvez/I2SClocklessLedDriver.h"



#ifndef NUM_LEDS_PER_STRIP
#warning "NUM_LEDS_PER_STRIP not defined, using default 256"
#define NUM_LEDS_PER_STRIP 256
#endif

FASTLED_NAMESPACE_BEGIN

class YvezI2S {
  public:
    typedef FixedVector<int, 6> Pins;
    // CRGBArray6Strips data is a strict type to enforce correctness during driver bringup.
    typedef CRGBArray<NUM_LEDS_PER_STRIP * 6> CRGBArray6Strips;
    static Pins DefaultPins() {
        return Pins({9,10,12,8,18,17});  // S3 only at the moment.
    }

    YvezI2S(CRGBArray6Strips* leds, int clock_pin, int latch_pin,
            const Pins &pins = DefaultPins()) : mPins(pins){
        mDriver.initled(leds->get(), mPins.data(), clock_pin, latch_pin);
    }

    void showPixels() { mDriver.showPixels(); }

    // Disable copy constructor and assignment operator
    YvezI2S() = delete;
    YvezI2S(YvezI2S &&) = delete;
    YvezI2S &operator=(const YvezI2S &) = delete;

  private:
    I2SClocklessVirtualLedDriver mDriver;
    Pins mPins;
};



FASTLED_NAMESPACE_END
