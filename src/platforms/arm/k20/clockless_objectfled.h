/// WORK IN PROGRESS!!! BETA CODE`

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
    virtual void *beginShowLeds() override {
        void *data = Super::beginShowLeds();
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