#pragma once

#include <stdint.h>

#include "crgb.h"
#include "fixed_vector.h"
#include "namespace.h"
#include "singleton.h"
#include "pixelset.h"
#include "scoped_ptr.h"

#if defined(NUM_LEDS_PER_STRIP)
#error "you can't set this yourself, yet."
#else
#define NUM_LEDS_PER_STRIP 256
#endif


FASTLED_NAMESPACE_BEGIN

class YvezI2SImpl;

// Note that this is a work in progress. The API will change and things right
// now are overly strict and hard to compile on purpose as the driver is undergoing
// development. I don't want to be dealing with memory errors because of all the
// the raw pointers so types are strict and the API is strict. This will change
// in the future.
class YvezI2S {
  public:
    // Note that this is a WS2812 driver ONLY.
    typedef FixedVector<int, 6> Pins;
    // CRGBArray6Strips data is a strict type to enforce correctness during driver bringup.
    typedef CRGBArray<NUM_LEDS_PER_STRIP * 6> CRGBArray6Strips;
    static Pins DefaultPins() {
        return Pins({9,10,12,8,18,17});  // S3 only at the moment.
    }

    // Safe to initialize in static memory because the driver will be instantiated
    // on first call to showPixels().
    YvezI2S(CRGBArray6Strips* leds, int clock_pin, int latch_pin,
            const Pins &pins = DefaultPins());

    void showPixels();

    // Disable copy constructor and assignment operator
    YvezI2S() = delete;
    YvezI2S(YvezI2S &&) = delete;
    YvezI2S &operator=(const YvezI2S &) = delete;
    ~YvezI2S();

  private:
    scoped_ptr<YvezI2SImpl> mDriver;
    Pins mPins;
    int mClockPin;
    int mLatchPin;
    CRGBArray6Strips* mLeds;
};



FASTLED_NAMESPACE_END
