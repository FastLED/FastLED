#if defined(__IMXRT1062__) // Teensy 4.0/4.1 only.

#define FASTLED_INTERNAL
#include "fl/fastled.h"

#include "clockless_objectfled.h"
#include "third_party/object_fled/src/ObjectFLED.h"
#include "third_party/object_fled/src/ObjectFLEDPinValidation.h"
#include "fl/warn.h"
#include "fl/stl/cstring.h"  // for fl::memset()

namespace {

typedef fl::FixedVector<uint8_t, 50> PinList50;

// Lightweight strip tracking
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
// ObjectFLEDRegistry Implementation (non-template)
// ============================================================================

void ObjectFLEDRegistry::registerGroup(void* groupPtr, void (*flushFunc)(void*)) {
    GroupEntry entry{groupPtr, flushFunc};
    if (!contains(entry)) {
        mGroups.push_back(entry);
    }
}

void ObjectFLEDRegistry::flushAll() {
    for (auto& entry : mGroups) {
        entry.flushFunc(entry.groupPtr);
    }
    mGroups.clear();
}

void ObjectFLEDRegistry::flushAllExcept(void* exceptPtr) {
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

bool ObjectFLEDRegistry::contains(const GroupEntry& entry) {
    for (const auto& e : mGroups) {
        if (e == entry) return true;
    }
    return false;
}

// ============================================================================
// ObjectFLEDGroupBase Implementation (concrete, non-template)
// ============================================================================

ObjectFLEDGroupBase::ObjectFLEDGroupBase(const ObjectFLEDTimingConfig& timing)
    : mTiming(timing)
    , mObjectFLED(nullptr)
    , mStripsData(new fl::vector<StripInfo>())
    , mPrevStripsData(new fl::vector<StripInfo>())
    , mMaxBytesPerStrip(0)
    , mDrawn(false)
    , mStripsChanged(false) {
}

ObjectFLEDGroupBase::~ObjectFLEDGroupBase() {
    delete static_cast<fl::ObjectFLED*>(mObjectFLED);
    delete static_cast<fl::vector<StripInfo>*>(mStripsData);
    delete static_cast<fl::vector<StripInfo>*>(mPrevStripsData);
}

void ObjectFLEDGroupBase::onQueuingStart() {
    auto& strips = *static_cast<fl::vector<StripInfo>*>(mStripsData);
    auto& prevStrips = *static_cast<fl::vector<StripInfo>*>(mPrevStripsData);
    strips.swap(prevStrips);
    strips.clear();
    mDrawn = false;
}

void ObjectFLEDGroupBase::addStrip(uint8_t pin, PixelIterator& pixel_iterator) {
    auto& strips = *static_cast<fl::vector<StripInfo>*>(mStripsData);

    // Validate pin before adding
    auto validation = objectfled::validate_teensy4_pin(pin);
    if (!validation.valid) {
        FL_WARN("================================================================================");
        FL_WARN("FASTLED ERROR: Strip on pin " << (int)pin << " is INVALID and has been disabled");
        FL_WARN(validation.error_message);
        FL_WARN("================================================================================");
        return;
    }

    // Check for warnings (pin is valid but may have issues)
    if (validation.error_message != nullptr) {
        FL_WARN("================================================================================");
        FL_WARN("FASTLED WARNING: Strip on pin " << (int)pin << " may have issues");
        FL_WARN(validation.error_message);
        FL_WARN("================================================================================");
    }

    // Check for duplicate pin
    for (const auto& strip : strips) {
        if (strip.pin == pin) {
            FL_WARN("================================================================================");
            FL_WARN("FASTLED ERROR: Pin " << (int)pin << " is already in use - strip disabled");
            FL_WARN("================================================================================");
            return;
        }
    }

    // Add strip metadata
    StripInfo info;
    info.pin = pin;
    info.numLeds = pixel_iterator.size();
    info.isRgbw = pixel_iterator.get_rgbw().active();
    info.numBytes = info.numLeds * (info.isRgbw ? 4 : 3);
    info.offsetBytes = 0;
    info.bytesWritten = 0;
    strips.push_back(info);

    // Finalize strip list
    onQueuingDone();

    // Write pixels directly to ObjectFLED's frameBufferLocal
    writePixels(pin, pixel_iterator);
}

void ObjectFLEDGroupBase::flush() {
    auto& strips = *static_cast<fl::vector<StripInfo>*>(mStripsData);

    if (mDrawn || strips.size() == 0) {
        return;  // Already drawn or no data
    }

    mDrawn = true;

    if (mStripsChanged || !mObjectFLED) {
        rebuildObjectFLED();
    }

    // TRANSMIT to hardware!
    static_cast<fl::ObjectFLED*>(mObjectFLED)->show();
}

void ObjectFLEDGroupBase::onQueuingDone() {
    auto& strips = *static_cast<fl::vector<StripInfo>*>(mStripsData);
    auto& prevStrips = *static_cast<fl::vector<StripInfo>*>(mPrevStripsData);

    // Calculate max bytes per strip
    mMaxBytesPerStrip = 0;
    for (const auto& strip : strips) {
        if (strip.numBytes > mMaxBytesPerStrip) {
            mMaxBytesPerStrip = strip.numBytes;
        }
    }

    // Assign offsets for each strip (interleaved layout)
    uint16_t offset = 0;
    for (auto& strip : strips) {
        strip.offsetBytes = offset;
        strip.bytesWritten = 0;
        offset += mMaxBytesPerStrip;
    }

    // Check if strip configuration changed
    mStripsChanged = (strips.size() != prevStrips.size());
    if (!mStripsChanged) {
        for (size_t i = 0; i < strips.size(); i++) {
            if (strips[i].pin != prevStrips[i].pin ||
                strips[i].numLeds != prevStrips[i].numLeds ||
                strips[i].isRgbw != prevStrips[i].isRgbw) {
                mStripsChanged = true;
                break;
            }
        }
    }
}

void ObjectFLEDGroupBase::rebuildObjectFLED() {
    auto& strips = *static_cast<fl::vector<StripInfo>*>(mStripsData);

    delete static_cast<fl::ObjectFLED*>(mObjectFLED);
    mObjectFLED = nullptr;

    // Build pin list
    PinList50 pinList;
    for (const auto& strip : strips) {
        pinList.push_back(strip.pin);
    }

    // Check if any strip uses RGBW
    bool hasRgbw = false;
    for (const auto& strip : strips) {
        if (strip.isRgbw) {
            hasRgbw = true;
            break;
        }
    }

    // Total LEDs = max_bytes_per_strip * num_strips / bytes_per_led
    int bytesPerLed = hasRgbw ? 4 : 3;
    int totalLeds = (mMaxBytesPerStrip / bytesPerLed) * strips.size();

    #ifdef FASTLED_DEBUG_OBJECTFLED
    FL_WARN("ObjectFLEDGroupBase: totalLeds=" << totalLeds << " maxBytes=" << mMaxBytesPerStrip);
    #endif

    // Pass nullptr so ObjectFLED allocates frameBufferLocal internally
    auto* objectfled = new fl::ObjectFLED(
        totalLeds,
        nullptr,  // No intermediate buffer - write directly to frameBufferLocal!
        hasRgbw ? CORDER_RGBW : CORDER_RGB,
        pinList.size(),
        pinList.data(),
        0  // No serpentine
    );

    // Configure timing at runtime
    objectfled->begin(mTiming.T1, mTiming.T2, mTiming.T3, mTiming.RESET);

    mObjectFLED = objectfled;

    // Clear frameBufferLocal to zeros (for padding)
    int totalBytes = mMaxBytesPerStrip * strips.size();
    fl::memset(objectfled->frameBufferLocal, 0, totalBytes);
}

void ObjectFLEDGroupBase::writePixels(uint8_t pin, PixelIterator& pixel_iterator) {
    auto& strips = *static_cast<fl::vector<StripInfo>*>(mStripsData);
    auto* objectfled = static_cast<fl::ObjectFLED*>(mObjectFLED);

    if (!objectfled) {
        FL_WARN("ObjectFLEDGroupBase::writePixels: mObjectFLED not initialized");
        return;
    }

    // Find the strip info for this pin
    StripInfo* stripInfo = nullptr;
    for (auto& strip : strips) {
        if (strip.pin == pin) {
            stripInfo = &strip;
            break;
        }
    }

    if (!stripInfo) {
        FL_WARN("ObjectFLEDGroupBase::writePixels: strip not found for pin " << (int)pin);
        return;
    }

    // Write directly to ObjectFLED's frameBufferLocal (saves a frame buffer!)
    uint8_t* dest = objectfled->frameBufferLocal + stripInfo->offsetBytes;
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

    stripInfo->bytesWritten = bytesWritten;
}

} // namespace fl

#endif // defined(__IMXRT1062__)
