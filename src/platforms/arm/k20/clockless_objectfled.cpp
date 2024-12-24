
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

typedef fl::FixedVector<uint8_t, 42> PinList42;

struct Info {
    uint8_t *buffer = nullptr;
    uint16_t numLeds = 0;
};


// Maps multiple pins and CRGB strips to a single ObjectFLED object.
class ObjectFLEDGroup {
  public:
    typedef fl::SortedHeapMap<uint8_t, Info> ObjectMap;

    fl::scoped_ptr<ObjectFLED> mObjectFLED;
    fl::HeapVector<uint8_t> mAllLedsBufferUint8;
    ObjectMap mObjects;
    bool mDrawn = false;
    bool mNeedsValidation = false;

    static ObjectFLEDGroup &getInstance() {
        return fl::Singleton<ObjectFLEDGroup>::instance();
    }

    ObjectFLEDGroup() = default;
    ~ObjectFLEDGroup() { mObjectFLED.reset(); }

    void onNewFrame() { mDrawn = false; }

    void addObject(uint8_t pin, uint8_t *led_data, uint16_t numLeds) {
        Info newInfo = {led_data, numLeds};
        if (mObjects.has(pin)) {
            Info &info = mObjects.at(pin);
            if (info.numLeds == numLeds && info.buffer == led_data) {
                return;
            }
        }
        fl::InsertResult result;
        mObjects.insert(pin, newInfo, &result);
        mNeedsValidation = true;
    }

    void removeObject(uint8_t pin) {
        mObjects.erase(pin);
        mNeedsValidation = true;
    }

    uint32_t getMaxLedInStrip() const {
        uint32_t maxLed = 0;
        for (auto it = mObjects.begin(); it != mObjects.end(); ++it) {
            maxLed = MAX(maxLed, it->second.numLeds);
        }
        return maxLed;
    }

    uint32_t getTotalLeds() const {
        uint32_t numStrips = mObjects.size();
        uint32_t maxLed = getMaxLedInStrip();
        return numStrips * maxLed;
    }

    void copy_data_to_buffer() {
        uint32_t totalLeds = getTotalLeds();
        if (totalLeds == 0) {
            return;
        }
        const int bytes_per_led = 3;
        uint32_t maxLedSegment = getMaxLedInStrip();
        mAllLedsBufferUint8.resize(totalLeds * bytes_per_led);
        uint8_t *curr = &mAllLedsBufferUint8.front();
        for (auto it = mObjects.begin(); it != mObjects.end(); ++it) {
            uint8_t *src = it->second.buffer;
            memset(curr, 0, maxLedSegment * bytes_per_led);
            size_t nBytes = it->second.numLeds * bytes_per_led;
            memcpy(curr, src, nBytes);
            curr += maxLedSegment * bytes_per_led;
        }
    }

    void showPixelsOnceThisFrame() {
        if (mDrawn) {
            return;
        }
        uint32_t totalLeds = getTotalLeds();
        if (totalLeds == 0) {
            return;
        }
        copy_data_to_buffer();
        if (!mObjectFLED.get() || mNeedsValidation) {
            mObjectFLED.reset();
            PinList42 pinList;
            for (auto it = mObjects.begin(); it != mObjects.end(); ++it) {
                pinList.push_back(it->first);
            }
            mObjectFLED.reset(new ObjectFLED(totalLeds, &mAllLedsBufferUint8.front(),
                                             CORDER_RGB, pinList.size(),
                                             pinList.data()));
            mObjectFLED->begin();
            mNeedsValidation = false;
        }
        mObjectFLED->show();
        mDrawn = true;
    }
};

} // anonymous namespace


namespace fl {

void ObjectFled::beginShowLeds() {
    ObjectFLEDGroup &group = ObjectFLEDGroup::getInstance();
    group.onNewFrame();
}

void ObjectFled::showPixels(uint8_t data_pin, PixelIterator& pixel_iterator) {
    ObjectFLEDGroup &group = ObjectFLEDGroup::getInstance();
    int numLeds = pixel_iterator.size();
    const int bytesPerLed = 3;
    mBuffer.resize(numLeds * bytesPerLed);
    uint8_t r, g, b;
    for (int i = 0; pixel_iterator.has(1); ++i) {
        pixel_iterator.loadAndScaleRGB(&r, &g, &b);
        mBuffer[i * bytesPerLed + 0] = r;
        mBuffer[i * bytesPerLed + 1] = g;
        mBuffer[i * bytesPerLed + 2] = b;
        pixel_iterator.advanceData();
        pixel_iterator.stepDithering();
    }
    group.addObject(data_pin, mBuffer.data(), numLeds);
}

void ObjectFled::endShowLeds() {
    // First one to call this draws everything, every other call this frame
    // is ignored.
    ObjectFLEDGroup::getInstance().showPixelsOnceThisFrame();
}

} // namespace fl

#endif // defined(__IMXRT1062__)
