#if defined(__IMXRT1062__) // Teensy 4.0/4.1 only.


#define FASTLED_INTERNAL
#include "fl/fastled.h"

#include "third_party/object_fled/src/ObjectFLED.h"

#include "crgb.h"
#include "eorder.h"
#include "fl/map.h"
#include "fl/singleton.h"
#include "fl/vector.h"
#include "fl/warn.h"
#include "fl/math_macros.h"
#include "fl/cstring.h"  // for fl::memset()
#include "pixel_iterator.h"
#include "cpixel_ledcontroller.h"

#include "clockless_objectfled.h"

namespace { // anonymous namespace

typedef fl::FixedVector<uint8_t, 50> PinList50;


static float gOverclock = 1.0f;
static float gPrevOverclock = 1.0f;
static int gLatchDelayUs = -1;


// Lightweight strip tracking (replaces RectangularDrawBuffer to save memory)
struct StripInfo {
    uint8_t pin;
    uint16_t numLeds;
    uint16_t numBytes;      // numLeds * (3 or 4)
    uint16_t offsetBytes;   // Offset into frameBufferLocal
    uint16_t bytesWritten;  // Bytes written so far (for padding check)
    bool isRgbw;
};

// Maps multiple pins and CRGB strips to a single ObjectFLED object.
class ObjectFLEDGroup {
  public:

    fl::unique_ptr<fl::ObjectFLED> mObjectFLED;
    fl::vector<StripInfo> mStrips;
    fl::vector<StripInfo> mPrevStrips;
    uint16_t mMaxBytesPerStrip;
    bool mDrawn = false;
    bool mStripsChanged = false;


    static ObjectFLEDGroup &getInstance() {
        return fl::Singleton<ObjectFLEDGroup>::instance();
    }

    ObjectFLEDGroup() : mMaxBytesPerStrip(0) {}
    ~ObjectFLEDGroup() { mObjectFLED.reset(); }

    void onQueuingStart() {
        mStrips.swap(mPrevStrips);
        mStrips.clear();
        mDrawn = false;
    }

    void onQueuingDone() {
        // Calculate max bytes per strip
        mMaxBytesPerStrip = 0;
        for (const auto& strip : mStrips) {
            if (strip.numBytes > mMaxBytesPerStrip) {
                mMaxBytesPerStrip = strip.numBytes;
            }
        }

        // Assign offsets for each strip (interleaved layout)
        uint16_t offset = 0;
        for (auto& strip : mStrips) {
            strip.offsetBytes = offset;
            strip.bytesWritten = 0;
            offset += mMaxBytesPerStrip;
        }

        // Check if strip configuration changed
        mStripsChanged = (mStrips.size() != mPrevStrips.size());
        if (!mStripsChanged) {
            for (size_t i = 0; i < mStrips.size(); i++) {
                if (mStrips[i].pin != mPrevStrips[i].pin ||
                    mStrips[i].numLeds != mPrevStrips[i].numLeds ||
                    mStrips[i].isRgbw != mPrevStrips[i].isRgbw) {
                    mStripsChanged = true;
                    break;
                }
            }
        }
    }

    void addObject(uint8_t pin, uint16_t numLeds, bool is_rgbw) {
        StripInfo info;
        info.pin = pin;
        info.numLeds = numLeds;
        info.numBytes = numLeds * (is_rgbw ? 4 : 3);
        info.isRgbw = is_rgbw;
        info.offsetBytes = 0;  // Will be set in onQueuingDone()
        info.bytesWritten = 0;
        mStrips.push_back(info);
    }

    void showPixelsOnceThisFrame() {
        if (mDrawn) {
            return;
        }
        mDrawn = true;
        if (mStrips.size() == 0) {
            return;
        }

        bool needs_rebuild = mStripsChanged || !mObjectFLED.get() || gOverclock != gPrevOverclock;
        if (needs_rebuild) {
            gPrevOverclock = gOverclock;
            mObjectFLED.reset();

            // Build pin list
            PinList50 pinList;
            for (const auto& strip : mStrips) {
                pinList.push_back(strip.pin);
            }

            // Check if any strip uses RGBW
            bool hasRgbw = false;
            for (const auto& strip : mStrips) {
                if (strip.isRgbw) {
                    hasRgbw = true;
                    break;
                }
            }

            // Total LEDs = max_bytes_per_strip * num_strips / bytes_per_led
            int bytesPerLed = hasRgbw ? 4 : 3;
            int totalLeds = (mMaxBytesPerStrip / bytesPerLed) * mStrips.size();

            #ifdef FASTLED_DEBUG_OBJECTFLED
            FL_WARN("ObjectFLEDGroup: totalLeds=" << totalLeds << " maxBytes=" << mMaxBytesPerStrip);
            #endif

            // Pass nullptr so ObjectFLED allocates frameBufferLocal internally
            // We'll write directly to it, saving a frame buffer!
            mObjectFLED.reset(new fl::ObjectFLED(
                totalLeds,
                nullptr,  // No intermediate buffer - write directly to frameBufferLocal!
                hasRgbw ? CORDER_RGBW : CORDER_RGB,
                pinList.size(),
                pinList.data(),
                0  // No serpentine
            ));

            if (gLatchDelayUs >= 0) {
                mObjectFLED->begin(gOverclock, gLatchDelayUs);
            } else {
                mObjectFLED->begin(gOverclock);
            }

            // Clear frameBufferLocal to zeros (for padding)
            int totalBytes = mMaxBytesPerStrip * mStrips.size();
            fl::memset(mObjectFLED->frameBufferLocal, 0, totalBytes);
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

    // Find this strip's info
    StripInfo* stripInfo = nullptr;
    for (auto& strip : group.mStrips) {
        if (strip.pin == data_pin) {
            stripInfo = &strip;
            break;
        }
    }

    if (!stripInfo || !group.mObjectFLED) {
        FL_WARN("ObjectFled::showPixels: strip not found for pin " << data_pin);
        return;
    }

    // Write directly to ObjectFLED's frameBufferLocal (saves a frame buffer!)
    uint8_t* dest = group.mObjectFLED->frameBufferLocal + stripInfo->offsetBytes;
    uint16_t bytesWritten = 0;

    if (rgbw.active()) {
        uint8_t r, g, b, w;
        while (pixel_iterator.has(1)) {
            pixel_iterator.loadAndScaleRGBW(&r, &g, &b, &w);
            *dest++ = r;
            *dest++ = g;
            *dest++ = b;
            *dest++ = w;
            bytesWritten += 4;
            pixel_iterator.advanceData();
            pixel_iterator.stepDithering();
        }
    } else {
        uint8_t r, g, b;
        while (pixel_iterator.has(1)) {
            pixel_iterator.loadAndScaleRGB(&r, &g, &b);
            *dest++ = r;
            *dest++ = g;
            *dest++ = b;
            bytesWritten += 3;
            pixel_iterator.advanceData();
            pixel_iterator.stepDithering();
        }
    }

    // Pad remaining bytes with zeros if strip is shorter than max
    // (frameBufferLocal was already cleared to zeros in showPixelsOnceThisFrame)
    uint16_t bytesToPad = group.mMaxBytesPerStrip - bytesWritten;
    if (bytesToPad > 0) {
        // Already zeros from memset, but we could explicitly clear if needed
        // fl::memset(dest, 0, bytesToPad);
    }

    stripInfo->bytesWritten = bytesWritten;
}

void ObjectFled::endShowLeds() {
    // First one to call this draws everything, every other call this frame
    // is ignored.
    ObjectFLEDGroup::getInstance().showPixelsOnceThisFrame();
}

} // namespace fl

#endif // defined(__IMXRT1062__)
