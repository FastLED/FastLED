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

// IWYU pragma: private

#include "cpixel_ledcontroller.h"
#include "pixel_iterator.h"
#include "fl/stl/vector.h"
#include "fl/stl/noexcept.h"

#ifndef FASTLED_OBJECTFLED_LATCH_DELAY
#define FASTLED_OBJECTFLED_LATCH_DELAY 300  // WS2812-5VB
#endif

namespace fl {

class ObjectFled {
  public:
    static void SetOverclock(float overclock) FL_NO_EXCEPT;
    static void SetLatchDelay(u16 latchDelayUs) FL_NO_EXCEPT;
    void beginShowLeds(int data_pin, int nleds) FL_NO_EXCEPT;
    void showPixels(u8 data_pin, PixelIterator& pixel_iterator) FL_NO_EXCEPT;
    void endShowLeds() FL_NO_EXCEPT;
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
    ClocklessController_ObjectFLED_WS2812(float overclock = 1.0f, int latchDelayUs = FASTLED_OBJECTFLED_LATCH_DELAY) FL_NO_EXCEPT : Base() {
        // Warning - overwrites previous overclock value.
        // Warning latchDelayUs is GLOBAL!
        ObjectFled::SetOverclock(overclock) FL_NO_EXCEPT;
        if (latchDelayUs >= 0) {
            ObjectFled::SetLatchDelay(latchDelayUs) FL_NO_EXCEPT;
        }
    }
    void init() override {}
    virtual u16 getMaxRefreshRate() const { return 800; }

  protected:
    // Wait until the last draw is complete, if necessary.
    virtual void *beginShowLeds(int nleds) override {
        void *data = Base::beginShowLeds(nleds);
        mObjectFled.beginShowLeds(DATA_PIN, nleds);
        return data;
    }

    // Prepares data for the draw.
    virtual void showPixels(PixelController<RGB_ORDER> &pixels) FL_NO_EXCEPT override {
        auto pixel_iterator = pixels.as_iterator(this->getRgbw());
        mObjectFled.showPixels(DATA_PIN, pixel_iterator);
    }

    // Send the data to the strip
    virtual void endShowLeds(void *data) FL_NO_EXCEPT override {
        Base::endShowLeds(data);
        mObjectFled.endShowLeds();
    }
};

} // namespace fl