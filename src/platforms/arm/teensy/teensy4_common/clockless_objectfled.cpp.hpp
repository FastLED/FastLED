// IWYU pragma: private

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X) // Teensy 4.0/4.1 only.

#define FASTLED_INTERNAL
#include "fl/system/fastled.h"

#include "platforms/arm/teensy/teensy4_common/clockless_objectfled.h"
// IWYU pragma: begin_keep
#include "third_party/object_fled/src/ObjectFLED.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "third_party/object_fled/src/ObjectFLEDPinValidation.h"
// IWYU pragma: end_keep
#include "fl/log/log.h"
#include "fl/stl/cstring.h"  // for fl::memset()

namespace {

typedef fl::FixedVector<fl::u8, 50> PinList50;

} // anonymous namespace

namespace fl {

// ============================================================================
// ObjectFLEDRegistry Implementation (non-template)
// ============================================================================

ObjectFLEDRegistry::ObjectFLEDRegistry() FL_NO_EXCEPT {
    EngineEvents::addListener(this);
}

ObjectFLEDRegistry::~ObjectFLEDRegistry() FL_NO_EXCEPT {
    EngineEvents::removeListener(this);
}

void ObjectFLEDRegistry::registerGroup(void* groupPtr, void (*flushFunc)(void*)) {
    GroupEntry entry{groupPtr, flushFunc};
    if (!contains(entry)) {
        mGroups.push_back(entry);
    }
}

void ObjectFLEDRegistry::flushAll() {
    fl::vector<GroupEntry> groups = mGroups;
    mGroups.clear();
    for (auto& entry : groups) {
        entry.flushFunc(entry.groupPtr);
    }
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

void ObjectFLEDRegistry::onEndFrame() FL_NO_EXCEPT {
    flushAll();
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
    delete static_cast<fl::ObjectFLED*>(mObjectFLED);  // ok bare allocation
}

void ObjectFLEDGroupBase::onQueuingStart() {
    const bool started = mRectDrawBuffer.onQueuingStart();
    if (started) {
        mPendingStrips.clear();
    }
    mDrawn = false;
}

void ObjectFLEDGroupBase::addStrip(u8 pin, PixelIterator& pixel_iterator) {
    // Validate pin before adding
    auto validation = objectfled::validate_teensy4_pin(pin);
    if (!validation.valid) {
        FL_WARN_F("================================================================================");
        FL_WARN_F("FASTLED ERROR: Strip on pin %s is INVALID and has been disabled", (int)pin);
        FL_WARN_F("%s", validation.error_message);
        FL_WARN_F("================================================================================");
        return;
    }

    // Check for warnings (pin is valid but may have issues)
    if (validation.error_message != nullptr) {
        FL_WARN_F("================================================================================");
        FL_WARN_F("FASTLED WARNING: Strip on pin %s may have issues", (int)pin);
        FL_WARN_F("%s", validation.error_message);
        FL_WARN_F("================================================================================");
    }

    // Check for duplicate pin in current draw list
    for (const auto& item : mRectDrawBuffer.mDrawList) {
        if (item.mPin == pin) {
            FL_WARN_F("================================================================================");
            FL_WARN_F("FASTLED ERROR: Pin %s is already in use - strip disabled", (int)pin);
            FL_WARN_F("================================================================================");
            return;
        }
    }

    // Queue strip into rectangular draw buffer
    const u16 numLeds = pixel_iterator.size();
    const bool isRgbw = pixel_iterator.get_rgbw().active();
    mRectDrawBuffer.queue(DrawItem(pin, numLeds, isRgbw));

    PendingStrip pending;
    pending.pin = pin;
    pending.bytes.resize(numLeds * (isRgbw ? 4 : 3));

    fl::span<u8> strip_bytes(pending.bytes.data(), pending.bytes.size());
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

    mPendingStrips.push_back(pending);
}

// autoresearch-runtime-output-lint: begin
void ObjectFLEDGroupBase::flush() {
    if (mDrawn || mRectDrawBuffer.mDrawList.size() == 0) {
        return;  // Already drawn or no data
    }

    mDrawn = true;

    mRectDrawBuffer.onQueuingDone();
    for (const auto& pending : mPendingStrips) {
        fl::span<u8> strip_bytes =
                mRectDrawBuffer.getLedsBufferBytesForPin(pending.pin, true);
        const size_t copy_size = pending.bytes.size() < strip_bytes.size()
                ? pending.bytes.size()
                : strip_bytes.size();
        if (copy_size > 0) {
            fl::memcpy(strip_bytes.data(), pending.bytes.data(), copy_size);
        }
    }
    mPendingStrips.clear();

    bool drawListChanged = mRectDrawBuffer.mDrawListChangedThisFrame;
    if (drawListChanged || !mObjectFLED) {
        rebuildObjectFLED();
    }

    // Copy pixel data from RectangularDrawBuffer into ObjectFLED's frameBufferLocal
    auto* objectfled = static_cast<fl::ObjectFLED*>(mObjectFLED);
    bool hasRgbw = false;
    for (const auto& item : mRectDrawBuffer.mDrawList) {
        if (item.mIsRgbw) {
            hasRgbw = true;
            break;
        }
    }
    u32 numStrips = 0;
    u32 bytesPerStrip = 0;
    u32 totalBytes = mRectDrawBuffer.getTotalBytes();
    mRectDrawBuffer.getBlockInfo(&numStrips, &bytesPerStrip, &totalBytes);
    const u32 frameBytes =
            objectFledFrameBytesForRectangularBlock(numStrips, bytesPerStrip, hasRgbw);
    if (frameBytes > 0) {
        fl::memset(objectfled->frameBufferLocal, 0, frameBytes);
    }
    const u32 copyBytes = totalBytes < frameBytes ? totalBytes : frameBytes;
    if (copyBytes > 0) {
        fl::memcpy(objectfled->frameBufferLocal,
                    mRectDrawBuffer.mAllLedsBufferUint8.get(),
                    copyBytes);
    }

    // Transmit the already packed RectangularDrawBuffer bytes.
    objectfled->showRawFrameBuffer();
}

void ObjectFLEDGroupBase::rebuildObjectFLED() {
    delete static_cast<fl::ObjectFLED*>(mObjectFLED);  // ok bare allocation
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

    const u32 frame_bytes =
            objectFledFrameBytesForRectangularBlock(num_strips, bytes_per_strip, hasRgbw);
    int totalLeds = static_cast<int>(
            objectFledTotalLedsForRectangularBlock(num_strips, bytes_per_strip, hasRgbw));

    // Pass nullptr so ObjectFLED allocates frameBufferLocal internally
    auto* objectfled = new fl::ObjectFLED(  // ok bare allocation
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
    fl::memset(objectfled->frameBufferLocal, 0, frame_bytes);
}
// autoresearch-runtime-output-lint: end

} // namespace fl

#endif // defined(FL_IS_TEENSY_4X)
