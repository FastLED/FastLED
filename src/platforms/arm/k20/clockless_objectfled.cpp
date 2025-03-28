
#if defined(__IMXRT1062__) // Teensy 4.0/4.1 only.


#define FASTLED_INTERNAL
#include "FastLED.h"

#include "third_party/object_fled/src/ObjectFLED.h"

#include "crgb.h"
#include "eorder.h"
#include "fl/map.h"
#include "fl/singleton.h"
#include "fl/vector.h"
#include "fl/warn.h"
#include "fl/math_macros.h"
#include "fl/rectangular_draw_buffer.h"
#include "pixel_iterator.h"
#include "cpixel_ledcontroller.h"

#include "clockless_objectfled.h"

namespace { // anonymous namespace

typedef fl::FixedVector<uint8_t, 50> PinList50;


static float gOverclock = 1.0f;
static float gPrevOverclock = 1.0f;
static int gLatchDelayUs = -1;


// Maps multiple pins and CRGB strips to a single ObjectFLED object.
class ObjectFLEDGroup {
  public:


    fl::scoped_ptr<fl::ObjectFLED> mObjectFLED;
    fl::RectangularDrawBuffer mRectDrawBuffer;
    bool mDrawn = false;


    static ObjectFLEDGroup &getInstance() {
        return fl::Singleton<ObjectFLEDGroup>::instance();
    }

    ObjectFLEDGroup() = default;
    ~ObjectFLEDGroup() { mObjectFLED.reset(); }

    void onQueuingStart() {
        mRectDrawBuffer.onQueuingStart();
        mDrawn = false;
    }

    void onQueuingDone() {
        mRectDrawBuffer.onQueuingDone();
    }

    void addObject(uint8_t pin, uint16_t numLeds, bool is_rgbw) {
        mRectDrawBuffer.queue(fl::DrawItem(pin, numLeds, is_rgbw));
    }

    void showPixelsOnceThisFrame() {
        if (mDrawn) {
            return;
        }
        mDrawn = true;
        if (mRectDrawBuffer.mAllLedsBufferUint8Size == 0) {
            return;
        }

        bool draw_list_changed = mRectDrawBuffer.mDrawListChangedThisFrame;
        bool needs_validation = draw_list_changed || !mObjectFLED.get() || gOverclock != gPrevOverclock;
        if (needs_validation) {
            gPrevOverclock = gOverclock;
            mObjectFLED.reset();
            PinList50 pinList;
            for (auto it = mRectDrawBuffer.mDrawList.begin(); it != mRectDrawBuffer.mDrawList.end(); ++it) {
                pinList.push_back(it->mPin);
            }
            int totalLeds = mRectDrawBuffer.getTotalBytes() / 3;  // Always work in RGB, even when in RGBW mode.
            FASTLED_WARN("ObjectFLEDGroup::showPixelsOnceThisFrame: totalLeds = " <<  totalLeds);
            mObjectFLED.reset(new fl::ObjectFLED(totalLeds, mRectDrawBuffer.mAllLedsBufferUint8.get(),
                                                 CORDER_RGB, pinList.size(),
                                                 pinList.data()));
            if (gLatchDelayUs >= 0) {
                mObjectFLED->begin(gOverclock, gLatchDelayUs);
            } else {
                mObjectFLED->begin(gOverclock);
            }
        }
        mObjectFLED->show();
    }
};

} // anonymous namespace


namespace fl {

void ObjectFled::SetOverclock(float overclock) {
    gOverclock = overclock;
}

void ObjectFled::SetLatchDelay(uint16_t latch_delay_us) {
    gLatchDelayUs = latch_delay_us;
}

void ObjectFled::beginShowLeds(int datapin, int nleds) {
    ObjectFLEDGroup &group = ObjectFLEDGroup::getInstance();
    group.onQueuingStart();
    group.addObject(datapin, nleds, false);
}

void ObjectFled::showPixels(uint8_t data_pin, PixelIterator& pixel_iterator) {
    ObjectFLEDGroup &group = ObjectFLEDGroup::getInstance();
    group.onQueuingDone();
    const Rgbw rgbw = pixel_iterator.get_rgbw();

    fl::Slice<uint8_t> strip_pixels = group.mRectDrawBuffer.getLedsBufferBytesForPin(data_pin, true);
    if (rgbw.active()) {
        uint8_t r, g, b, w;
        while (pixel_iterator.has(1)) {
            FASTLED_ASSERT(strip_pixels.size() >= 4, "ObjectFled::showPixels: buffer overflow");
            pixel_iterator.loadAndScaleRGBW(&r, &g, &b, &w);
            strip_pixels[0] = r;
            strip_pixels[1] = g;
            strip_pixels[2] = b;
            strip_pixels[3] = w;
            strip_pixels.pop_front();
            strip_pixels.pop_front();
            strip_pixels.pop_front();
            strip_pixels.pop_front();
            pixel_iterator.advanceData();
            pixel_iterator.stepDithering();
        }
    } else {
        uint8_t r, g, b;
        while (pixel_iterator.has(1)) {
            FASTLED_ASSERT(strip_pixels.size() >= 3, "ObjectFled::showPixels: buffer overflow");
            pixel_iterator.loadAndScaleRGB(&r, &g, &b);
            strip_pixels[0] = r;
            strip_pixels[1] = g;
            strip_pixels[2] = b;
            strip_pixels.pop_front();
            strip_pixels.pop_front();
            strip_pixels.pop_front();
            pixel_iterator.advanceData();
            pixel_iterator.stepDithering();
        }
    }

}

void ObjectFled::endShowLeds() {
    // First one to call this draws everything, every other call this frame
    // is ignored.
    ObjectFLEDGroup::getInstance().showPixelsOnceThisFrame();
}

} // namespace fl

#endif // defined(__IMXRT1062__)
