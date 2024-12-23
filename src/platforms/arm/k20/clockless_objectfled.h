/// WORK IN PROGRESS!!! BETA CODE`

#pragma once

#include "FastLED.h"  // TODO, remove this from the header.

#include "third_party/object_fled/src/ObjectFLED.h"

#include "crgb.h"
#include "eorder.h"
#include "fl/map.h"
#include "fl/singleton.h"
#include "fl/vector.h"
#include "pixel_iterator.h"
#include "cpixel_ledcontroller.h"

namespace fl {

class PinList42 : public fl::FixedVector<uint8_t, 42> {};

struct Info {
    CRGB *buffer = nullptr;
    uint16_t numLeds = 0;
};

// Maps multiple pins and CRGB strips to a single ObjectFLED object.
class ObjectFLEDGroup {
  public:
    typedef fl::SortedHeapMap<uint8_t, Info> ObjectMap;

    fl::scoped_ptr<ObjectFLED> mObjectFLED;
    fl::scoped_array<CRGB> mAllLedsBuffer;
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
        bool ok = mObjects.insert(pin, newInfo, &result);
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

    void showPixels() {
        if (mDrawn) {
            return;
        }
        if (!mObjectFLED.get() || mNeedsValidation) {
            mObjectFLED.reset();
            uint32_t totalLeds = getTotalLeds();
            uint32_t maxLedSegment = getMaxLedInStrip();
            mAllLedsBuffer.reset();
            mAllLedsBuffer.reset(new CRGB[totalLeds]);

            CRGB* curr = mAllLedsBuffer.get();
            // copy leds in the buffer
            for (auto it = mObjects.begin(); it != mObjects.end(); ++it) {
                //memcpy(mBuffer.get() + it->second.numLeds, it->second.buffer,
                //       it->second.numLeds * sizeof(CRGB));
                CRGB* src = it->second.buffer;
                size_t nBytes = it->second.numLeds * sizeof(CRGB);
                memcpy(curr, src, nBytes);
                if (it->second.numLeds < maxLedSegment) {
                    // Fill the rest with black.
                    memset(curr + it->second.numLeds, 0,
                           (maxLedSegment - it->second.numLeds) * sizeof(CRGB));
                }
                curr += maxLedSegment;
            }

            PinList42 pinList;
            for (auto it = mObjects.begin(); it != mObjects.end(); ++it) {
                pinList.push_back(it->first);
            }
            mObjectFLED.reset(new ObjectFLED(totalLeds, mAllLedsBuffer.get(),
                                             CORDER_RGB, pinList.size(),
                                             pinList.data()));
            mObjectFLED->begin();
            mNeedsValidation = false;
        }
        mObjectFLED->show();
        mDrawn = true;
    }
};

// TODO: RGBW support, should be pretty easy except the fact that ObjectFLED either
// supports RGBW on all pixels, or none.
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class ClocklessController_ObjectFLED_WS2812
    : public CPixelLEDController<RGB_ORDER> {
  private:
    // -- Verify that the pin is valid
    // static_assert(FastPin<DATA_PIN>::validpin(), "Invalid pin specified");

    // -- The actual controller object for ESP32
    // RmtController5 mRMTController;

  public:
    ClocklessController_ObjectFLED_WS2812()
        : CPixelLEDController<RGB_ORDER>() {}

    void init() override {}
    virtual uint16_t getMaxRefreshRate() const { return 800; }

  protected:
    // Wait until the last draw is complete, if necessary.
    virtual void *beginShowLeds() override {
        ObjectFLEDGroup &group = ObjectFLEDGroup::getInstance();
        group.onNewFrame();
        void *data = CPixelLEDController<RGB_ORDER>::beginShowLeds();
        return data;
    }

    // Prepares data for the draw.
    virtual void showPixels(PixelController<RGB_ORDER> &pixels) override {
        ObjectFLEDGroup &group = ObjectFLEDGroup::getInstance();
        PixelIterator iterator = pixels.as_iterator(this->getRgbw());
        int numLeds = pixels.size();
        mBuffer.resize(numLeds);
        uint8_t r, g, b;
        for (uint16_t i = 0; pixels.has(1); i++) {
            pixels.loadAndScaleRGB(&r, &g, &b);
            mBuffer[i] = CRGB(r, g, b);
            pixels.advanceData();
            pixels.stepDithering();
        }
        group.addObject(DATA_PIN, mBuffer.data(), numLeds);
    }

    // Send the data to the strip
    virtual void endShowLeds(void *data) override {
        CPixelLEDController<RGB_ORDER>::endShowLeds(data);
        ObjectFLEDGroup::getInstance().showPixels();
    }

    fl::HeapVector<CRGB> mBuffer;
};

} // namespace fl