/// FastLED mapping of the ObjectFLED driver for Teensy 4.0/4.1.
///
/// This mode will support upto 42 parallel strips of WS2812 LEDS! ~7x that of OctoWS2811!
///
/// Caveats: This driver is a memory hog! In order to map the driver into the way that
///          FastLED works, we need to have multiple frame buffers. ObjectFLED
///          has it's own buffer which must be rectangular (i.e. all strips must be the same
///          number of LEDS). Since FastLED is flexible in this regard, we need to convert
///          the FastLED data into the rectangular ObjectFLED buffer. This is done by copying
///          the data, which means extending the buffer size to the LARGEST strip. The number of
///          of buffers in total is 3-4. This will be reduced in the future, but at the time of
///          this writing (2024-Dec-23rd), this is the case, and will be reduced in the future.
///
/// @author Kurt Funderburg
/// @reddit: reddit.com/u/Tiny_Structure_7
/// @author: Zach Vorhies (FastLED code)
/// @reddit: reddit.com/u/ZachVorhies


#pragma once

#include "cpixel_ledcontroller.h"
#include "pixel_iterator.h"
#include "fl/vector.h"

namespace fl {

class ObjectFled {
  public:
    void beginShowLeds();
    void showPixels(uint8_t data_pin, PixelIterator& pixel_iterator);
    void endShowLeds();
    fl::HeapVector<uint8_t> mBuffer;
};

// TODO: RGBW support, should be pretty easy except the fact that ObjectFLED
// either supports RGBW on all pixels strips, or none.
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class ClocklessController_ObjectFLED_WS2812
    : public CPixelLEDController<RGB_ORDER> {
  private:
    typedef CPixelLEDController<RGB_ORDER> Super;
    ObjectFled mObjectFled;

  public:
    ClocklessController_ObjectFLED_WS2812() : Super() {}
    void init() override {}
    virtual uint16_t getMaxRefreshRate() const { return 800; }

  protected:
    // Wait until the last draw is complete, if necessary.
    virtual void *beginShowLeds(int nleds) override {
        void *data = Super::beginShowLeds(nleds);
        mObjectFled.beginShowLeds();
        return data;
    }

    // Prepares data for the draw.
    virtual void showPixels(PixelController<RGB_ORDER> &pixels) override {
        auto pixel_iterator = pixels.as_iterator(this->getRgbw());
        mObjectFled.showPixels(DATA_PIN, pixel_iterator);
    }

    // Send the data to the strip
    virtual void endShowLeds(void *data) override {
        Super::endShowLeds(data);
        mObjectFled.endShowLeds();
    }
};

} // namespace fl