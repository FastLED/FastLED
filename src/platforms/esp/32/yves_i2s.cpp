


#if !defined(ESP32)

// do nothing

#else


#include "sdkconfig.h"

#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32)

// #define NBIS2SERIALPINS 6 // the number of virtual pins here mavimum 6x8=48
// strips #define NUM_LEDS_PER_STRIP 256 #define NUM_LEDS (NUM_LEDS_PER_STRIP *
// NBIS2SERIALPINS * 8) #define NUM_STRIPS (NBIS2SERIALPINS * 8)

// the number of virtual pins here mavimum 6x8=48 strips
#define NBIS2SERIALPINS 6
#define NUM_LEDS_PER_STRIP 256
#define NUM_LEDS (NUM_LEDS_PER_STRIP * NBIS2SERIALPINS * 8)
#define NUM_STRIPS (NBIS2SERIALPINS * 8)

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

#include "platforms/esp/32/yves_i2s.h"

#include "crgb.h"
#include "namespace.h"
#include "allocator.h"
#include "warn.h"

FASTLED_NAMESPACE_BEGIN

class YvesI2SImpl : public fl::I2SClocklessVirtualLedDriver {};

YvesI2S::YvesI2S(const fl::FixedVector<int, 6> &pins, int clock_pin,
                 int latch_pin) {
    mPins = pins;
    mClockPin = clock_pin;
    mLatchPin = latch_pin;
}

YvesI2S::~YvesI2S() {
    mLeds.reset();
}

Slice<CRGB> YvesI2S::leds() {
    if (!mLeds) {
        mLeds.reset(LargeBlockAllocator<CRGB>::Alloc(NUM_LEDS));
    }
    return Slice<CRGB>(mLeds.get(), NUM_LEDS);
}

Slice<CRGB> YvesI2S::initOnce() {
    Slice<CRGB> leds = this->leds();
    if (!mDriver) {
        if (mPins.size() != 6) {
            FASTLED_WARN("YvesI2S requires 6 pins");
            return leds;
        }
        mDriver.reset(new YvesI2SImpl());
        mDriver->initled(leds.begin(), mPins.data(), mClockPin, mLatchPin);
    }
    return leds;
}

void YvesI2S::showPixels() {
    initOnce();
    mDriver->showPixels();
}

FASTLED_NAMESPACE_END


#endif  // ESP32S3 || ESP32
#endif  // ESP32