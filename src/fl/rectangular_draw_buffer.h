
#pragma once

#include <stdint.h>

#include "fl/map.h"
#include "fl/scoped_ptr.h"
#include "fl/slice.h"
#include "fl/vector.h"
#include "fl/namespace.h"

namespace fl {

struct DrawItem {
    DrawItem() = default;
    DrawItem(uint8_t pin, uint16_t numLeds, bool is_rgbw);
    uint8_t mPin = 0;
    uint32_t mNumBytes = 0;
    bool mIsRgbw = false;
    bool operator!=(const DrawItem &other) const {
        return mPin != other.mPin || mNumBytes != other.mNumBytes ||
               mIsRgbw != other.mIsRgbw;
    }
};

// Needed by controllers that require a compact, rectangular buffer of pixel data.
// Namely, ObjectFLED and the I2S controllers.
// This class handles using multiple independent strips of LEDs, each with their own
// buffer of pixel data. The strips are not necessarily contiguous in memory.
// One or more DrawItems containing the pin number and number are queued
// up. When the queue-ing is done, the buffers are compacted into the rectangular buffer.
// Data access is achieved through a Slice<uint8_t> representing the pixel data for that pin.
class RectangularDrawBuffer {
  public:
    typedef fl::HeapVector<DrawItem> DrawList;
    // We manually manage the memory for the buffer of all LEDs so that it can go
    // into psram on ESP32S3, which is managed by fl::LargeBlockAllocator.
    scoped_array<uint8_t> mAllLedsBufferUint8;
    uint32_t mAllLedsBufferUint8Size = 0;
    fl::FixedMap<uint8_t, fl::Slice<uint8_t>, 50> mPinToLedSegment;
    DrawList mDrawList;
    DrawList mPrevDrawList;
    bool mDrawListChangedThisFrame = false;

    enum QueueState { IDLE, QUEUEING, QUEUE_DONE };
    QueueState mQueueState = IDLE;

    RectangularDrawBuffer() = default;
    ~RectangularDrawBuffer() = default;

    fl::Slice<uint8_t> getLedsBufferBytesForPin(uint8_t pin, bool clear_first=true);

    // Safe to call multiple times before calling queue() once. Returns true on the first call, false after.
    bool onQueuingStart();
    void queue(const DrawItem &item);

    // Safe to call multiple times before calling onQueueingStart() again. Returns true on the first call, false after.
    bool onQueuingDone();
    uint32_t getMaxBytesInStrip() const;
    uint32_t getTotalBytes() const;
    void getBlockInfo(uint32_t *num_strips, uint32_t *bytes_per_strip,
                      uint32_t *total_bytes) const;
};

} // namespace fl
