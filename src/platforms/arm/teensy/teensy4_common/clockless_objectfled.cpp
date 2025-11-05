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
#include "fl/chipsets/led_timing.h"  // For TIMING structs
#include "pixel_iterator.h"
#include "cpixel_ledcontroller.h"

#include "clockless_objectfled.h"

namespace { // anonymous namespace

typedef fl::FixedVector<uint8_t, 50> PinList50;

// Lightweight strip tracking (replaces RectangularDrawBuffer to save memory)
struct StripInfo {
    uint8_t pin;
    uint16_t numLeds;
    uint16_t numBytes;      // numLeds * (3 or 4)
    uint16_t offsetBytes;   // Offset into frameBufferLocal
    uint16_t bytesWritten;  // Bytes written so far (for padding check)
    bool isRgbw;
};

} // anonymous namespace

namespace fl {

// ============================================================================
// ObjectFLED Registry (tracks all active chipset groups)
// ============================================================================

/// Track all active chipset groups across all chipset types
class ObjectFLEDRegistry {
public:
    static ObjectFLEDRegistry& getInstance() {
        return fl::Singleton<ObjectFLEDRegistry>::instance();
    }

    // Register a group for tracking
    void registerGroup(void* groupPtr, void (*flushFunc)(void*)) {
        GroupEntry entry{groupPtr, flushFunc};
        if (!contains(entry)) {
            mGroups.push_back(entry);
        }
    }

    // Flush all pending groups
    void flushAll() {
        for (auto& entry : mGroups) {
            entry.flushFunc(entry.groupPtr);
        }
        mGroups.clear();
    }

    // Flush all groups except the specified one
    void flushAllExcept(void* exceptPtr) {
        for (auto& entry : mGroups) {
            if (entry.groupPtr != exceptPtr) {
                entry.flushFunc(entry.groupPtr);
            }
        }
        // Remove flushed entries, keep the exception
        auto newEnd = fl::remove_if(mGroups.begin(), mGroups.end(),
            [exceptPtr](const GroupEntry& e) { return e.groupPtr != exceptPtr; });
        mGroups.erase(newEnd, mGroups.end());
    }

private:
    struct GroupEntry {
        void* groupPtr;
        void (*flushFunc)(void*);

        bool operator==(const GroupEntry& other) const {
            return groupPtr == other.groupPtr;
        }
    };

    fl::vector<GroupEntry> mGroups;

    bool contains(const GroupEntry& entry) {
        for (const auto& e : mGroups) {
            if (e == entry) return true;
        }
        return false;
    }
};

// ============================================================================
// Templated ObjectFLED Group (one instance per chipset type)
// ============================================================================

/// Templated singleton - one instance per chipset type
/// Automatically instantiated on first access via fl::Singleton<T>
template <typename TIMING>
class ObjectFLEDGroup {
public:
    static ObjectFLEDGroup& getInstance() {
        return fl::Singleton<ObjectFLEDGroup<TIMING>>::instance();
    }

    ObjectFLEDGroup() : mMaxBytesPerStrip(0) {
        // Auto-register with global registry on construction
        ObjectFLEDRegistry::getInstance().registerGroup(
            this,
            [](void* ptr) {
                static_cast<ObjectFLEDGroup*>(ptr)->flush();
            }
        );
    }

    ~ObjectFLEDGroup() {
        mObjectFLED.reset();
    }

    // Called by proxy in beginShowLeds()
    void onQueuingStart() {
        mStrips.swap(mPrevStrips);
        mStrips.clear();
        mDrawn = false;
    }

    // Called by proxy in showPixels()
    void addStrip(uint8_t pin, PixelIterator& pixel_iterator) {
        // Add strip metadata
        StripInfo info;
        info.pin = pin;
        info.numLeds = pixel_iterator.size();
        info.isRgbw = pixel_iterator.get_rgbw().active();
        info.numBytes = info.numLeds * (info.isRgbw ? 4 : 3);
        info.offsetBytes = 0;
        info.bytesWritten = 0;
        mStrips.push_back(info);

        // Finalize strip list
        onQueuingDone();

        // Write pixels directly to ObjectFLED's frameBufferLocal
        writePixels(info, pixel_iterator);
    }

    // Called by registry when chipset changes or frame ends
    void flush() {
        if (mDrawn || mStrips.size() == 0) {
            return;  // Already drawn or no data
        }

        mDrawn = true;

        if (mStripsChanged || !mObjectFLED.get()) {
            rebuildObjectFLED();
        }

        // TRANSMIT to hardware!
        mObjectFLED->show();
    }

private:
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

    void rebuildObjectFLED() {
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
        FL_WARN("ObjectFLEDGroup<TIMING>: totalLeds=" << totalLeds << " maxBytes=" << mMaxBytesPerStrip);
        #endif

        // Pass nullptr so ObjectFLED allocates frameBufferLocal internally
        mObjectFLED.reset(new fl::ObjectFLED(
            totalLeds,
            nullptr,  // No intermediate buffer - write directly to frameBufferLocal!
            hasRgbw ? CORDER_RGBW : CORDER_RGB,
            pinList.size(),
            pinList.data(),
            0  // No serpentine
        ));

        // Use template-based begin<TIMING>() - extracts timing at compile-time
        // Uses TIMING::RESET as the default latch delay
        mObjectFLED->begin<TIMING>();

        // Clear frameBufferLocal to zeros (for padding)
        int totalBytes = mMaxBytesPerStrip * mStrips.size();
        fl::memset(mObjectFLED->frameBufferLocal, 0, totalBytes);
    }

    void writePixels(StripInfo& stripInfo, PixelIterator& pixel_iterator) {
        if (!mObjectFLED) {
            FL_WARN("ObjectFLEDGroup::writePixels: mObjectFLED not initialized");
            return;
        }

        // Write directly to ObjectFLED's frameBufferLocal (saves a frame buffer!)
        uint8_t* dest = mObjectFLED->frameBufferLocal + stripInfo.offsetBytes;
        uint16_t bytesWritten = 0;

        const Rgbw rgbw = pixel_iterator.get_rgbw();

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

        stripInfo.bytesWritten = bytesWritten;
    }

    fl::unique_ptr<fl::ObjectFLED> mObjectFLED;
    fl::vector<StripInfo> mStrips;
    fl::vector<StripInfo> mPrevStrips;
    uint16_t mMaxBytesPerStrip;
    bool mDrawn = false;
    bool mStripsChanged = false;
};

// ============================================================================
// Proxy Controller Implementation
// ============================================================================

template <typename TIMING, int DATA_PIN, EOrder RGB_ORDER>
ClocklessController_ObjectFLED_Proxy<TIMING, DATA_PIN, RGB_ORDER>::ClocklessController_ObjectFLED_Proxy()
    : Base() {
    // Latch delay is automatically determined from TIMING::RESET
}

template <typename TIMING, int DATA_PIN, EOrder RGB_ORDER>
void *ClocklessController_ObjectFLED_Proxy<TIMING, DATA_PIN, RGB_ORDER>::beginShowLeds(int nleds) {
    void *data = Base::beginShowLeds(nleds);

    // Auto-grab the singleton for THIS chipset type
    ObjectFLEDGroup<TIMING>& group = ObjectFLEDGroup<TIMING>::getInstance();

    // Flush any pending groups of DIFFERENT chipset types
    ObjectFLEDRegistry::getInstance().flushAllExcept(&group);

    // Start queuing for this group
    group.onQueuingStart();

    return data;
}

template <typename TIMING, int DATA_PIN, EOrder RGB_ORDER>
void ClocklessController_ObjectFLED_Proxy<TIMING, DATA_PIN, RGB_ORDER>::showPixels(
    PixelController<RGB_ORDER> &pixels) {
    // Auto-grab the singleton for THIS chipset type
    ObjectFLEDGroup<TIMING>& group = ObjectFLEDGroup<TIMING>::getInstance();

    // Add this strip to the group
    auto pixel_iterator = pixels.as_iterator(this->getRgbw());
    group.addStrip(DATA_PIN, pixel_iterator);
}

template <typename TIMING, int DATA_PIN, EOrder RGB_ORDER>
void ClocklessController_ObjectFLED_Proxy<TIMING, DATA_PIN, RGB_ORDER>::endShowLeds(void *data) {
    Base::endShowLeds(data);

    // DON'T flush here - let chipset change detection or frame end handle it
    // This is handled by the next controller's beginShowLeds() or FastLED.show() end
}

// ============================================================================
// Explicit Template Instantiations (for common chipsets)
// ============================================================================

// Instantiate for WS2812
template class ClocklessController_ObjectFLED_Proxy<TIMING_WS2812_800KHZ, 0, GRB>;
template class ClocklessController_ObjectFLED_Proxy<TIMING_WS2812_800KHZ, 1, GRB>;
template class ClocklessController_ObjectFLED_Proxy<TIMING_WS2812_800KHZ, 2, GRB>;
template class ClocklessController_ObjectFLED_Proxy<TIMING_WS2812_800KHZ, 3, GRB>;
template class ClocklessController_ObjectFLED_Proxy<TIMING_WS2812_800KHZ, 4, GRB>;
template class ClocklessController_ObjectFLED_Proxy<TIMING_WS2812_800KHZ, 5, GRB>;
template class ClocklessController_ObjectFLED_Proxy<TIMING_WS2812_800KHZ, 6, GRB>;
template class ClocklessController_ObjectFLED_Proxy<TIMING_WS2812_800KHZ, 7, GRB>;

// Instantiate for SK6812
template class ClocklessController_ObjectFLED_Proxy<TIMING_SK6812, 0, GRB>;
template class ClocklessController_ObjectFLED_Proxy<TIMING_SK6812, 1, GRB>;
template class ClocklessController_ObjectFLED_Proxy<TIMING_SK6812, 2, GRB>;
template class ClocklessController_ObjectFLED_Proxy<TIMING_SK6812, 3, GRB>;

// Instantiate for WS2811
template class ClocklessController_ObjectFLED_Proxy<TIMING_WS2811_400KHZ, 0, GRB>;
template class ClocklessController_ObjectFLED_Proxy<TIMING_WS2811_400KHZ, 1, GRB>;
template class ClocklessController_ObjectFLED_Proxy<TIMING_WS2811_400KHZ, 2, GRB>;

// Instantiate for WS2813
template class ClocklessController_ObjectFLED_Proxy<TIMING_WS2813, 0, GRB>;
template class ClocklessController_ObjectFLED_Proxy<TIMING_WS2813, 1, GRB>;

} // namespace fl

#endif // defined(__IMXRT1062__)
