#if defined(ESP32) && defined(ENABLE_ESP32_I2S_YVEZ_DRIVER) && !ENABLE_ESP32_I2S_YVEZ_DRIVER


#include "platforms/esp/32/yves_i2s.h"

#define FASTLED_YVEZ_INTERNAL
#include "third_party/yves/I2SClocklessLedDriver.h"
#include "namespace.h"
#include "warn.h"

FASTLED_NAMESPACE_BEGIN

static_assert(NUM_LEDS_PER_STRIP == 256, "Only 256 supported");

// Use an empty subclass to avoid having to include the implementation in the header.
class YvesI2SImpl : public fl::I2SClocklessVirtualLedDriver {};

namespace {
    int g_instance_count = 0;
}


YvesI2S::YvesI2S(CRGBArray6Strips* leds, int clock_pin, int latch_pin,
        const Pins &pins) : mPins(pins), mClockPin(clock_pin), mLatchPin(latch_pin), mLeds(leds) {
    ++g_instance_count;
}

YvesI2S::~YvesI2S() {
    --g_instance_count;
    mDriver.reset();
}

void YvesI2S::showPixels() {
    if (!mDriver) {
        if (g_instance_count > 1) {
            FASTLED_WARN("Only one instance of YvesI2S is supported at the moment.");
            return;
        }
        mDriver->initled(mLeds->get(), mPins.data(), mClockPin, mLatchPin);
    }
    mDriver->showPixels();
}

FASTLED_NAMESPACE_END

#endif // ESP32
