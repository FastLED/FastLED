
#if defined(__IMXRT1062__) // Teensy 4.0/4.1 only.

#include "third_party/object_fled/src/ObjectFLED.h"

#include "crgb.h"
#include "eorder.h"
#include "fl/map.h"
#include "fl/singleton.h"
#include "fl/vector.h"
#include "fl/warn.h"
#include "pixel_iterator.h"
#include "cpixel_ledcontroller.h"

#include "clockless_objectfled.h"

namespace { // anonymous namespace

typedef fl::FixedVector<uint8_t, 42> PinList42;

struct Info {
    CRGB *buffer = nullptr;
    uint16_t numLeds = 0;
};


// Maps multiple pins and CRGB strips to a single ObjectFLED object.
class ObjectFLEDGroup {
  public:
    typedef fl::SortedHeapMap<uint8_t, Info> ObjectMap;

    fl::scoped_ptr<ObjectFLED> mObjectFLED;
    fl::HeapVector<CRGB> mAllLedsBuffer;
    ObjectMap mObjects;
    bool mDrawn = false;
    bool mNeedsValidation = false;

    static ObjectFLEDGroup &getInstance() {
        return fl::Singleton<ObjectFLEDGroup>::instance();
    }

    ObjectFLEDGroup() = default;
    ~ObjectFLEDGroup() { mObjectFLED.reset(); }

    void onNewFrame() { mDrawn = false; }

    void addObject(uint8_t pin, CRGB *leds, uint16_t numLeds) {
        Info newInfo = {leds, numLeds};
        if (mObjects.has(pin)) {
            Info &info = mObjects.at(pin);
            if (info.numLeds == numLeds && info.buffer == leds) {
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
            maxLed = max(maxLed, it->second.numLeds);
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
        mAllLedsBuffer.resize(totalLeds);
        CRGB *curr = &mAllLedsBuffer.front();
        for (auto it = mObjects.begin(); it != mObjects.end(); ++it) {
            CRGB *src = it->second.buffer;
            size_t nBytes = it->second.numLeds * sizeof(CRGB);
            memcpy(curr, src, nBytes);
            uint32_t maxLedSegment = getMaxLedInStrip();
            if (it->second.numLeds < maxLedSegment) {
                // Fill the rest with black.
                memset(curr + it->second.numLeds, 0,
                       (maxLedSegment - it->second.numLeds) * sizeof(CRGB));
            }
            curr += maxLedSegment;
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
            mObjectFLED.reset(new ObjectFLED(totalLeds, &mAllLedsBuffer.front(),
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
    mBuffer.resize(numLeds);
    uint8_t r, g, b;
    for (uint16_t i = 0; pixel_iterator.has(1); i++) {
        pixel_iterator.loadAndScaleRGB(&r, &g, &b);
        mBuffer[i] = CRGB(r, g, b);
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
