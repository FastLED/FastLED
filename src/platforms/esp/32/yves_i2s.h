#pragma once

#include <stdint.h>

#include "crgb.h"
#include "fixed_vector.h"
#include "namespace.h"
#include "singleton.h"
#include "pixelset.h"
#include "scoped_ptr.h"

// #define NBIS2SERIALPINS 6 // the number of virtual pins here mavimum 6x8=48 strips
// #define NUM_LEDS_PER_STRIP 256
// #define NUM_LEDS (NUM_LEDS_PER_STRIP * NBIS2SERIALPINS * 8)
// #define NUM_STRIPS (NBIS2SERIALPINS * 8)

#ifndef NBIS2SERIALPINS
#error "NBIS2SERIALPINS must be defined"
#endif

#ifndef NUM_LEDS_PER_STRIP
#error "NUM_LEDS_PER_STRIP must be defined"
#endif

#ifndef NUM_LEDS
#error "NUM_LEDS must be defined"
#endif

#ifndef NUM_STRIPS
#error "NUM_STRIPS must be defined"
#endif

#define FASTLED_YVEZ_INTERNAL
#include "third_party/yves/I2SClocklessLedDriver.h"


FASTLED_NAMESPACE_BEGIN

class YvesI2SImpl : public fl::I2SClocklessVirtualLedDriver {};

// Note that this is a work in progress. The API will change and things right
// now are overly strict and hard to compile on purpose as the driver is undergoing
// development. I don't want to be dealing with memory errors because of all the
// the raw pointers so types are strict and the API is strict. This will change
// in the future.



class YvesI2S {
  public:
    // Safe to initialize in static memory because the driver will be instantiated
    // on first call to showPixels().
    YvesI2S(CRGB* leds, const FixedVector<int, 6>& pins, int clock_pin, int latch_pin) {
        //driver.initled(leds, Pins, CLOCK_PIN, LATCH_PIN);
        for (int i = 0; i < pins.size(); i++) {
            //mPins[i] = pins[i];
            mPins.push_back(pins[i]);
        }
        mLeds = leds;
        mClockPin = clock_pin;
        mLatchPin = latch_pin;
    }

    void initOnce() {
        if (!mDriver) {
            mDriver.reset(new YvesI2SImpl());
            mDriver->initled(mLeds, mPins.data(), mClockPin, mLatchPin);
        }
    }

    void showPixels() {
        initOnce();
        mDriver->showPixels();
    }

    // Disable copy constructor and assignment operator
    YvesI2S() = delete;
    YvesI2S(YvesI2S &&) = delete;
    YvesI2S &operator=(const YvesI2S &) = delete;
    ~YvesI2S() {}

  private:
    scoped_ptr<fl::I2SClocklessVirtualLedDriver> mDriver;
    //Pins mPins;
    int mClockPin;
    int mLatchPin;
    CRGB* mLeds;
    FixedVector<int, 6> mPins;
};



FASTLED_NAMESPACE_END
