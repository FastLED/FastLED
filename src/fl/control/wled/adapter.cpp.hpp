// ok no header

// This is FastLED's own translation unit, not user code. `FastLED.h` emits
// user-facing `#warning`s (EOL boards, removed build flags) that are all
// guarded on `!defined(FASTLED_INTERNAL)`, so without this the library warns
// at itself and the user sees the same notice twice per build. Matches the
// convention already used by bitswap/crgb/cled_controller/FastLED.cpp.hpp.
#define FASTLED_INTERNAL

#include "fl/fx/wled/adapter.h"
#include "FastLED.h"  // ok include - WLED adapter needs global FastLED object
#include "fl/stl/memory.h"
#include "fl/stl/noexcept.h"

namespace fl {

// FastLEDAdapter implementation

FastLEDAdapter::FastLEDAdapter(u8 controllerIndex)
    FL_NO_EXCEPT : mControllerIndex(controllerIndex)
    , mSegmentStart(0)
    , mSegmentEnd(0)
    , mHasSegment(false)
{
    // Initialize segment end to the number of LEDs in the controller
    CLEDController& controller = FastLED[mControllerIndex];
    mSegmentEnd = controller.size();
}

fl::span<CRGB> FastLEDAdapter::getLEDs() FL_NO_EXCEPT {
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

size_t FastLEDAdapter::getNumLEDs() const FL_NO_EXCEPT {
    if (mHasSegment) {
        return mSegmentEnd - mSegmentStart;
    }

    CLEDController& controller = FastLED[mControllerIndex];
    return controller.size();
}

void FastLEDAdapter::show() FL_NO_EXCEPT {
    FastLED.show();
}

void FastLEDAdapter::show(u8 brightness) FL_NO_EXCEPT {
    FastLED.show(brightness);
}

void FastLEDAdapter::clear(bool writeToStrip) FL_NO_EXCEPT {
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

void FastLEDAdapter::setBrightness(u8 brightness) FL_NO_EXCEPT {
    FastLED.setBrightness(brightness);
}

u8 FastLEDAdapter::getBrightness() const FL_NO_EXCEPT {
    return FastLED.getBrightness();
}

void FastLEDAdapter::setCorrection(CRGB correction) FL_NO_EXCEPT {
    FastLED.setCorrection(correction);
}

void FastLEDAdapter::setTemperature(CRGB temperature) FL_NO_EXCEPT {
    FastLED.setTemperature(temperature);
}

void FastLEDAdapter::delay(unsigned long ms) FL_NO_EXCEPT {
    FastLED.delay(ms);
}

void FastLEDAdapter::setMaxRefreshRate(u16 fps) FL_NO_EXCEPT {
    FastLED.setMaxRefreshRate(fps);
}

u16 FastLEDAdapter::getMaxRefreshRate() const FL_NO_EXCEPT {
    // CFastLED doesn't expose the max refresh rate setting
    // Return 0 to indicate no limit
    return 0;
}

void FastLEDAdapter::setSegment(size_t start, size_t end) FL_NO_EXCEPT {
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

void FastLEDAdapter::clearSegment() FL_NO_EXCEPT {
    CLEDController& controller = FastLED[mControllerIndex];
    mSegmentEnd = controller.size();
    mSegmentStart = 0;
    mHasSegment = false;
}

// Helper function implementation
fl::shared_ptr<IFastLED> createFastLEDController(u8 controllerIndex) FL_NO_EXCEPT {
    return fl::make_shared<FastLEDAdapter>(controllerIndex);
}

} // namespace fl
