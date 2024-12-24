
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
    bool isRgbw = false;
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

    void addObject(uint8_t pin, uint8_t *led_data, uint16_t numLeds, bool is_rgbw) {
        Info newInfo = {led_data, numLeds, is_rgbw};
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

    void copy_data_to_buffer(bool has_rgbw) {
        uint32_t totalLeds = getTotalLeds();
        if (totalLeds == 0) {
            return;
        }
        const int bytes_per_led = has_rgbw ? 4 : 3;
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

    bool allRgbOrRgbw(bool* has_rgbw) {
        bool has_rgb = false;
        *has_rgbw = false;
        for (auto it = mObjects.begin(); it != mObjects.end(); ++it) {
            if (it->second.isRgbw) {
                *has_rgbw = true;
            } else {
                has_rgb = true;
            }
        }
        if (has_rgb && *has_rgbw) {
            return false;
        }
        return true;
    }

    void showPixelsOnceThisFrame() {
        if (mDrawn) {
            return;
        }
        uint32_t totalLeds = getTotalLeds();
        if (totalLeds == 0) {
            return;
        }
        bool has_rgbw = false;
        const bool all_same_format = allRgbOrRgbw(&has_rgbw);
        if (!all_same_format) {
            // Actually, because we are doing RGBW emulation, we can actually mix RGB and RGBW strips.
            // Come back here and fix this. It will take a little code re-work but is totally
            // doable.
            FASTLED_WARN("ObjectFLEDGroup: Cannot mix RGB and RGBW strips in the same ObjectFLED object, no strips will be drawn.");
            return;
        }
        copy_data_to_buffer(has_rgbw);
        if (!mObjectFLED.get() || mNeedsValidation) {
            mObjectFLED.reset();
            PinList42 pinList;
            for (auto it = mObjects.begin(); it != mObjects.end(); ++it) {
                pinList.push_back(it->first);
            }

            if (has_rgbw) {
                // Although the ObjectFLED controller can handle RGBW data, the FastLED has more options.
                // Therefore, we pretend the RGBW data is actually RGB data.
                //
                // The ObjectFLED controller expects the raw pixel byte data in multiples of 3.
                // In the case of src data not a multiple of 3, then we need to
                // add pad bytes so that the delegate controller doesn't walk off the end
                // of the array and invoke a buffer overflow panic.
                totalLeds = (totalLeds * 4 + 2) / 3; // Round up to nearest multiple of 3
                size_t extra = totalLeds % 3 ? 1 : 0;
                totalLeds += extra;
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
    const Rgbw rgbw = pixel_iterator.get_rgbw();
    int numLeds = pixel_iterator.size();
    const int bytesPerLed = rgbw.active() ? 4 : 3;
    mBuffer.resize(numLeds * bytesPerLed);
    if (rgbw.active()) {
        uint8_t r, g, b, w;
        for (int i = 0; pixel_iterator.has(1); ++i) {
            pixel_iterator.loadAndScaleRGBW(&r, &g, &b, &w);
            mBuffer[i * bytesPerLed + 0] = r;
            mBuffer[i * bytesPerLed + 1] = g;
            mBuffer[i * bytesPerLed + 2] = b;
            mBuffer[i * bytesPerLed + 3] = w;
            pixel_iterator.advanceData();
            pixel_iterator.stepDithering();
        }
    } else {
        uint8_t r, g, b;
        for (int i = 0; pixel_iterator.has(1); ++i) {
            pixel_iterator.loadAndScaleRGB(&r, &g, &b);
            mBuffer[i * bytesPerLed + 0] = r;
            mBuffer[i * bytesPerLed + 1] = g;
            mBuffer[i * bytesPerLed + 2] = b;
            pixel_iterator.advanceData();
            pixel_iterator.stepDithering();
        }
    }

    group.addObject(data_pin, mBuffer.data(), numLeds, rgbw.active());
}

void ObjectFled::endShowLeds() {
    // First one to call this draws everything, every other call this frame
    // is ignored.
    ObjectFLEDGroup::getInstance().showPixelsOnceThisFrame();
}

} // namespace fl

#endif // defined(__IMXRT1062__)
