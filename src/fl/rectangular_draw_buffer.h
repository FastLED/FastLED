
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

// Maps multiple pins and CRGB strips to a single Rectangular Draw Buffer
// blob. This is needed for the ObjectFLED and I2S controllers for ESP32S3, which
// use a single buffer for all leds to pins.
class RectangularDrawBuffer {
  public:
    typedef fl::HeapVector<DrawItem> DrawList;

    fl::HeapVector<uint8_t> mAllLedsBufferUint8;
    fl::SortedHeapMap<uint8_t, fl::Slice<uint8_t>> mPinToLedSegment;
    DrawList mDrawList;
    DrawList mPrevDrawList;

    enum QueueState { IDLE, QUEUEING, QUEUE_DONE };
    QueueState mQueueState = IDLE;

    RectangularDrawBuffer() = default;
    ~RectangularDrawBuffer() = default;

    fl::Slice<uint8_t> getLedsBufferBytesForPin(uint8_t pin, bool clear_first);
    void onQueuingStart();
    void queue(const DrawItem &item);

    // Expected to allow the caller to call this multiple times before
    // getLedsBufferBytesForPin(...).
    void onQueuingDone();
    uint32_t getMaxBytesInStrip() const;
    uint32_t getTotalBytes() const;
    void getBlockInfo(uint32_t *num_strips, uint32_t *bytes_per_strip,
                      uint32_t *total_bytes) const;
};

} // namespace fl