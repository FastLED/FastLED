
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
#include "pixel_iterator.h"
#include "cpixel_ledcontroller.h"

#include "clockless_objectfled.h"

namespace { // anonymous namespace

typedef fl::FixedVector<uint8_t, 50> PinList50;

typedef uint8_t Pin;

static float gOverclock = 1.0f;
static float gPrevOverclock = 1.0f;
static int gLatchDelayUs = -1;

struct Info {
    uint8_t pin = 0;
    uint16_t numLeds = 0;
    bool isRgbw = false;
    bool operator!=(const Info &other) const {
        return pin != other.pin || numLeds != other.numLeds || isRgbw != other.isRgbw;
    }
};



// Maps multiple pins and CRGB strips to a single ObjectFLED object.
class ObjectFLEDGroup {
  public:
    typedef fl::HeapVector<Info> DrawList;

    fl::scoped_ptr<ObjectFLED> mObjectFLED;
    fl::HeapVector<uint8_t> mAllLedsBufferUint8;
    DrawList mObjects;
    DrawList mPrevObjects;
    bool mDrawn = false;
    bool mOnPreDrawCalled = false;

    static ObjectFLEDGroup &getInstance() {
        return fl::Singleton<ObjectFLEDGroup>::instance();
    }

    ObjectFLEDGroup() = default;
    ~ObjectFLEDGroup() { mObjectFLED.reset(); }

    void onNewFrame() {
        if (!mDrawn) {
            return;
        }
        mDrawn = false;
        mOnPreDrawCalled = false;
        mObjects.swap(mPrevObjects);
        mObjects.clear();
        if (!mAllLedsBufferUint8.empty()) {
            memset(&mAllLedsBufferUint8.front(), 0, mAllLedsBufferUint8.size());
            mAllLedsBufferUint8.clear();
        }
    }

    void onPreDraw() {
        if (mOnPreDrawCalled) {
            return;
        }
        // iterator through the current draw objects and calculate the total number of leds
        // that will be drawn this frame.
        uint32_t totalLeds = 0;
        for (auto it = mObjects.begin(); it != mObjects.end(); ++it) {
            totalLeds += it->numLeds;
        }
        if (totalLeds == 0) {
            return;
        }
        // Always assume RGB data. RGBW data will be converted to RGB data.
        mAllLedsBufferUint8.reserve(totalLeds * 3);
    }

    void addObject(Pin pin, uint16_t numLeds, bool is_rgbw) {
        if (is_rgbw) {
            numLeds = Rgbw::size_as_rgb(numLeds);
        }
        Info newInfo = {pin, numLeds, is_rgbw};
        mObjects.push_back(newInfo);
    }

    uint32_t getMaxLedInStrip() const {
        uint32_t maxLed = 0;
        for (auto it = mObjects.begin(); it != mObjects.end(); ++it) {
            maxLed = MAX(maxLed, it->numLeds);
        }
        return maxLed;
    }

    uint32_t getTotalLeds() const {
        uint32_t numStrips = mObjects.size();
        uint32_t maxLed = getMaxLedInStrip();
        return numStrips * maxLed;
    }

    void showPixelsOnceThisFrame() {
        if (mDrawn) {
            return;
        }
        uint32_t totalLeds = getTotalLeds();
        if (totalLeds == 0) {
            return;
        }
        
        bool needs_validation = mPrevObjects != mObjects || !mObjectFLED.get() || gOverclock != gPrevOverclock;
        if (needs_validation) {
            gPrevOverclock = gOverclock;
            mObjectFLED.reset();
            PinList50 pinList;
            for (auto it = mObjects.begin(); it != mObjects.end(); ++it) {
                pinList.push_back(it->pin);
            }
            mObjectFLED.reset(new ObjectFLED(totalLeds, &mAllLedsBufferUint8.front(),
                                             CORDER_RGB, pinList.size(),
                                             pinList.data()));
            if (gLatchDelayUs >= 0) {
                mObjectFLED->begin(gOverclock, gLatchDelayUs);
            } else {
                mObjectFLED->begin(gOverclock);
            }
        }
        mObjectFLED->show();
        mDrawn = true;
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
    group.onNewFrame();
}

void ObjectFled::showPixels(uint8_t data_pin, PixelIterator& pixel_iterator) {
    ObjectFLEDGroup &group = ObjectFLEDGroup::getInstance();
    group.onPreDraw();
    const Rgbw rgbw = pixel_iterator.get_rgbw();
    int numLeds = pixel_iterator.size();
    fl::HeapVector<uint8_t>& all_pixels = group.mAllLedsBufferUint8;
    if (rgbw.active()) {
        uint8_t r, g, b, w;
        while (pixel_iterator.has(1)) {
            pixel_iterator.loadAndScaleRGBW(&r, &g, &b, &w);
            all_pixels.push_back(r);
            all_pixels.push_back(g);
            all_pixels.push_back(b);
            all_pixels.push_back(w);
            pixel_iterator.advanceData();
            pixel_iterator.stepDithering();
        }
    } else {
        uint8_t r, g, b;
        while (pixel_iterator.has(1)) {
            pixel_iterator.loadAndScaleRGB(&r, &g, &b);
            all_pixels.push_back(r);
            all_pixels.push_back(g);
            all_pixels.push_back(b);
            pixel_iterator.advanceData();
            pixel_iterator.stepDithering();
        }
    }
    group.addObject(data_pin, numLeds, rgbw.active());
}

void ObjectFled::endShowLeds() {
    // First one to call this draws everything, every other call this frame
    // is ignored.
    ObjectFLEDGroup::getInstance().showPixelsOnceThisFrame();
}

} // namespace fl

#endif // defined(__IMXRT1062__)
