// IWYU pragma: private

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X) // Teensy 4.0/4.1 only.

#define FASTLED_INTERNAL
#include "fl/fastled.h"

#include "clockless_objectfled.h"
// IWYU pragma: begin_keep
#include "third_party/object_fled/src/ObjectFLED.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "third_party/object_fled/src/ObjectFLEDPinValidation.h"
// IWYU pragma: end_keep
#include "fl/warn.h"
#include "fl/stl/cstring.h"  // for fl::memset()

namespace {

typedef fl::FixedVector<fl::u8, 50> PinList50;

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
    , mDrawn(false) {
}

ObjectFLEDGroupBase::~ObjectFLEDGroupBase() {
    delete static_cast<fl::ObjectFLED*>(mObjectFLED);
}

void ObjectFLEDGroupBase::onQueuingStart() {
    mRectDrawBuffer.onQueuingStart();
    mDrawn = false;
}

void ObjectFLEDGroupBase::addStrip(u8 pin, PixelIterator& pixel_iterator) {
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

    // Check for duplicate pin in current draw list
    for (const auto& item : mRectDrawBuffer.mDrawList) {
        if (item.mPin == pin) {
            FL_WARN("================================================================================");
            FL_WARN("FASTLED ERROR: Pin " << (int)pin << " is already in use - strip disabled");
            FL_WARN("================================================================================");
            return;
        }
    }

    // Queue strip into rectangular draw buffer
    const u16 numLeds = pixel_iterator.size();
    const bool isRgbw = pixel_iterator.get_rgbw().active();
    mRectDrawBuffer.queue(DrawItem(pin, numLeds, isRgbw));

    // Finalize buffer layout so we can write pixels
    mRectDrawBuffer.onQueuingDone();

    // Write pixels into RectangularDrawBuffer
    fl::span<u8> strip_bytes = mRectDrawBuffer.getLedsBufferBytesForPin(pin, true);
    const Rgbw rgbw = pixel_iterator.get_rgbw();

    if (rgbw.active()) {
        u8 r, g, b, w;
        while (pixel_iterator.has(1)) {
            pixel_iterator.loadAndScaleRGBW(&r, &g, &b, &w);
            strip_bytes[0] = r;
            strip_bytes[1] = g;
            strip_bytes[2] = b;
            strip_bytes[3] = w;
            strip_bytes.pop_front();
            strip_bytes.pop_front();
            strip_bytes.pop_front();
            strip_bytes.pop_front();
            pixel_iterator.advanceData();
            pixel_iterator.stepDithering();
        }
    } else {
        u8 r, g, b;
        while (pixel_iterator.has(1)) {
            pixel_iterator.loadAndScaleRGB(&r, &g, &b);
            strip_bytes[0] = r;
            strip_bytes[1] = g;
            strip_bytes[2] = b;
            strip_bytes.pop_front();
            strip_bytes.pop_front();
            strip_bytes.pop_front();
            pixel_iterator.advanceData();
            pixel_iterator.stepDithering();
        }
    }
}

void ObjectFLEDGroupBase::flush() {
    if (mDrawn || mRectDrawBuffer.mDrawList.size() == 0) {
        return;  // Already drawn or no data
    }

    mDrawn = true;

    bool drawListChanged = mRectDrawBuffer.mDrawListChangedThisFrame;
    if (drawListChanged || !mObjectFLED) {
        rebuildObjectFLED();
    }

    // Copy pixel data from RectangularDrawBuffer into ObjectFLED's frameBufferLocal
    auto* objectfled = static_cast<fl::ObjectFLED*>(mObjectFLED);
    u32 totalBytes = mRectDrawBuffer.getTotalBytes();
    if (totalBytes > 0) {
        fl::memcpy(objectfled->frameBufferLocal,
                    mRectDrawBuffer.mAllLedsBufferUint8.get(),
                    totalBytes);
    }

    // TRANSMIT to hardware!
    objectfled->show();
}

void ObjectFLEDGroupBase::rebuildObjectFLED() {
    delete static_cast<fl::ObjectFLED*>(mObjectFLED);
    mObjectFLED = nullptr;

    // Build pin list from draw list
    PinList50 pinList;
    bool hasRgbw = false;
    for (const auto& item : mRectDrawBuffer.mDrawList) {
        pinList.push_back(item.mPin);
        if (item.mIsRgbw) {
            hasRgbw = true;
        }
    }

    u32 num_strips = 0;
    u32 bytes_per_strip = 0;
    u32 total_bytes = 0;
    mRectDrawBuffer.getBlockInfo(&num_strips, &bytes_per_strip, &total_bytes);

    // Total LEDs = total_bytes / bytes_per_led
    int bytesPerLed = hasRgbw ? 4 : 3;
    int totalLeds = total_bytes / bytesPerLed;

    #ifdef FASTLED_DEBUG_OBJECTFLED
    FL_WARN("ObjectFLEDGroupBase: totalLeds=" << totalLeds << " bytesPerStrip=" << bytes_per_strip);
    #endif

    // Pass nullptr so ObjectFLED allocates frameBufferLocal internally
    auto* objectfled = new fl::ObjectFLED(
        totalLeds,
        nullptr,
        hasRgbw ? CORDER_RGBW : CORDER_RGB,
        pinList.size(),
        pinList.data(),
        0  // No serpentine
    );

    // Configure timing at runtime
    objectfled->begin(mTiming.T1, mTiming.T2, mTiming.T3, mTiming.RESET);

    mObjectFLED = objectfled;

    // Clear frameBufferLocal to zeros (for padding)
    fl::memset(objectfled->frameBufferLocal, 0, total_bytes);
}

} // namespace fl

#endif // defined(FL_IS_TEENSY_4X)
