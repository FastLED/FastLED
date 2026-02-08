#include "fl/fx/wled/adapter.h"
#include "FastLED.h"  // ok include - WLED adapter needs global FastLED object
#include "fl/stl/memory.h"

namespace fl {

// FastLEDAdapter implementation

FastLEDAdapter::FastLEDAdapter(u8 controllerIndex)
    : mControllerIndex(controllerIndex)
    , mSegmentStart(0)
    , mSegmentEnd(0)
    , mHasSegment(false)
{
    // Initialize segment end to the number of LEDs in the controller
    CLEDController& controller = FastLED[mControllerIndex];
    mSegmentEnd = controller.size();
}

fl::span<CRGB> FastLEDAdapter::getLEDs() {
    CLEDController& controller = FastLED[mControllerIndex];
    CRGB* leds = controller.leds();
    if (!leds) {
        return fl::span<CRGB>();
    }

    if (mHasSegment) {
        return fl::span<CRGB>(leds + mSegmentStart, mSegmentEnd - mSegmentStart);
    }
    return fl::span<CRGB>(leds, controller.size());
}

size_t FastLEDAdapter::getNumLEDs() const {
    if (mHasSegment) {
        return mSegmentEnd - mSegmentStart;
    }

    CLEDController& controller = FastLED[mControllerIndex];
    return controller.size();
}

void FastLEDAdapter::show() {
    FastLED.show();
}

void FastLEDAdapter::show(u8 brightness) {
    FastLED.show(brightness);
}

void FastLEDAdapter::clear(bool writeToStrip) {
    CLEDController& controller = FastLED[mControllerIndex];
    CRGB* leds = controller.leds();
    if (!leds) {
        return;
    }

    if (mHasSegment) {
        // Clear only the segment
        for (size_t i = mSegmentStart; i < mSegmentEnd; i++) {
            leds[i] = CRGB::Black;
        }
    } else {
        // Clear all LEDs
        size_t numLeds = controller.size();
        for (size_t i = 0; i < numLeds; i++) {
            leds[i] = CRGB::Black;
        }
    }

    if (writeToStrip) {
        FastLED.show();
    }
}

void FastLEDAdapter::setBrightness(u8 brightness) {
    FastLED.setBrightness(brightness);
}

u8 FastLEDAdapter::getBrightness() const {
    return FastLED.getBrightness();
}

void FastLEDAdapter::setCorrection(CRGB correction) {
    FastLED.setCorrection(correction);
}

void FastLEDAdapter::setTemperature(CRGB temperature) {
    FastLED.setTemperature(temperature);
}

void FastLEDAdapter::delay(unsigned long ms) {
    FastLED.delay(ms);
}

void FastLEDAdapter::setMaxRefreshRate(u16 fps) {
    FastLED.setMaxRefreshRate(fps);
}

u16 FastLEDAdapter::getMaxRefreshRate() const {
    // CFastLED doesn't expose the max refresh rate setting
    // Return 0 to indicate no limit
    return 0;
}

void FastLEDAdapter::setSegment(size_t start, size_t end) {
    CLEDController& controller = FastLED[mControllerIndex];
    size_t numLeds = controller.size();

    // Validate bounds
    if (start >= numLeds) {
        start = numLeds > 0 ? numLeds - 1 : 0;
    }
    if (end > numLeds) {
        end = numLeds;
    }
    if (end <= start) {
        end = start + 1;
        if (end > numLeds) {
            end = numLeds;
            start = end > 0 ? end - 1 : 0;
        }
    }

    mSegmentStart = start;
    mSegmentEnd = end;
    mHasSegment = true;
}

void FastLEDAdapter::clearSegment() {
    CLEDController& controller = FastLED[mControllerIndex];
    mSegmentEnd = controller.size();
    mSegmentStart = 0;
    mHasSegment = false;
}

// Helper function implementation
fl::shared_ptr<IFastLED> createFastLEDController(u8 controllerIndex) {
    return fl::make_shared<FastLEDAdapter>(controllerIndex);
}

} // namespace fl
