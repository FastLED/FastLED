/// FastLED mapping of the ObjectFLED driver for Teensy 4.0/4.1.
///
/// This driver will support upto 42 parallel strips of WS2812 LEDS! ~7x that of OctoWS2811!
/// BasicTest example to demonstrate massive parallel output with FastLED using
/// ObjectFLED for Teensy 4.0/4.1.
///
/// This mode will support upto 42 parallel strips of WS2812 LEDS! ~7x that of OctoWS2811!
///
/// The theoritical limit of Teensy 4.0, if frames per second is not a concern, is
/// more than 200k pixels. However, realistically, to run 42 strips at 550 pixels
/// each at 60fps, is 23k pixels.
///
/// @author Kurt Funderburg
/// @reddit: reddit.com/u/Tiny_Structure_7
/// The FastLED code was written by Zach Vorhies
/// @author Kurt Funderburg
/// @reddit: reddit.com/u/Tiny_Structure_7
/// @author: Zach Vorhies (FastLED code)
/// @reddit: reddit.com/u/ZachVorhies


#pragma once

#include "cpixel_ledcontroller.h"
#include "pixel_iterator.h"
#include "fl/vector.h"

#ifndef FASTLED_OBJECTFLED_LATCH_DELAY
#define FASTLED_OBJECTFLED_LATCH_DELAY 300  // WS2812-5VB
#endif

namespace fl {

class ObjectFled {
  public:
    static void SetOverclock(float overclock);
    static void SetLatchDelay(uint16_t latchDelayUs);
    void beginShowLeds(int data_pin, int nleds);
    void showPixels(uint8_t data_pin, PixelIterator& pixel_iterator);
    void endShowLeds();
};

// TODO: RGBW support, should be pretty easy except the fact that ObjectFLED
// either supports RGBW on all pixels strips, or none.
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class ClocklessController_ObjectFLED_WS2812
    : public CPixelLEDController<RGB_ORDER> {
  private:
    typedef CPixelLEDController<RGB_ORDER> Base;
    ObjectFled mObjectFled;

  public:
    ClocklessController_ObjectFLED_WS2812(float overclock = 1.0f, int latchDelayUs = FASTLED_OBJECTFLED_LATCH_DELAY): Base() {
        // Warning - overwrites previous overclock value.
        // Warning latchDelayUs is GLOBAL!
        ObjectFled::SetOverclock(overclock);
        if (latchDelayUs >= 0) {
            ObjectFled::SetLatchDelay(latchDelayUs);
        }
    }
    void init() override {}
    virtual uint16_t getMaxRefreshRate() const { return 800; }

  protected:
    // Wait until the last draw is complete, if necessary.
    virtual void *beginShowLeds(int nleds) override {
        void *data = Base::beginShowLeds(nleds);
        mObjectFled.beginShowLeds(DATA_PIN, nleds);
        return data;
    }

    // Prepares data for the draw.
    virtual void showPixels(PixelController<RGB_ORDER> &pixels) override {
        auto pixel_iterator = pixels.as_iterator(this->getRgbw());
        mObjectFled.showPixels(DATA_PIN, pixel_iterator);
    }

    // Send the data to the strip
    virtual void endShowLeds(void *data) override {
        Base::endShowLeds(data);
        mObjectFled.endShowLeds();
    }
};

} // namespace fl