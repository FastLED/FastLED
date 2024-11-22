


#if !defined(ESP32) || !ENABLE_ESP32_I2S_YVES_DRIVER 
// do nothing

#else

#include "esp32-hal.h"

#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32

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

FASTLED_NAMESPACE_BEGIN

class YvesI2SImpl : public fl::I2SClocklessVirtualLedDriver {};

YvesI2S::YvesI2S(CRGB *leds, const FixedVector<int, 6> &pins, int clock_pin,
                 int latch_pin) {
    // driver.initled(leds, Pins, CLOCK_PIN, LATCH_PIN);
    for (int i = 0; i < pins.size(); i++) {
        // mPins[i] = pins[i];
        mPins.push_back(pins[i]);
    }
    mLeds = leds;
    mClockPin = clock_pin;
    mLatchPin = latch_pin;
}

YvesI2S::~YvesI2S() {
    // delete mDriver;
}

void YvesI2S::initOnce() {
    if (!mDriver) {
        mDriver.reset(new YvesI2SImpl());
        mDriver->initled(mLeds, mPins.data(), mClockPin, mLatchPin);
    }
}

void YvesI2S::showPixels() {
    initOnce();
    mDriver->showPixels();
}

FASTLED_NAMESPACE_END


#endif  // ESP32S3 || ESP32
#endif  // ESP32