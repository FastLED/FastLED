
#pragma once

// Takes multiple CRGB arrays[] of different sizes and generates one CRGB
// array of size MAX(WIDTH) * NUM_OF_STRIPS that contains them all.

// This allows the flexible LED array usage in FastLED for block renderers.

// Needed by controllers that require a compact, rectangular buffer of pixel
// data. Namely, ObjectFLED and the I2S controllers. This class handles using
// multiple independent strips of LEDs, each with their own buffer of pixel
// data. The strips are not necessarily contiguous in memory. One or more
// DrawItems containing the pin number and number are queued up. When the
// queue-ing is done, the buffers are compacted into the rectangular buffer.
// Data access is achieved through a span<u8> representing the pixel data
// for that pin.

#include "fl/stdint.h"

#include "fl/int.h"
#include "fl/map.h"
#include "fl/namespace.h"
#include "fl/scoped_array.h"
#include "fl/span.h"
#include "fl/vector.h"

namespace fl {

struct DrawItem {
    DrawItem() = default;
    DrawItem(u8 pin, u16 numLeds, bool is_rgbw);
    
    // Rule of 5 for POD data
    DrawItem(const DrawItem &other) = default;
    DrawItem &operator=(const DrawItem &other) = default;
    DrawItem(DrawItem &&other) noexcept = default;
    DrawItem &operator=(DrawItem &&other) noexcept = default;
    
    u8 mPin = 0;
    u32 mNumBytes = 0;
    bool mIsRgbw = false;
    bool operator!=(const DrawItem &other) const {
        return mPin != other.mPin || mNumBytes != other.mNumBytes ||
               mIsRgbw != other.mIsRgbw;
    }
};


class RectangularDrawBuffer {
  public:


    RectangularDrawBuffer() = default;
    ~RectangularDrawBuffer() = default;

    fl::span<u8> getLedsBufferBytesForPin(u8 pin,
                                               bool clear_first = true);

    // Safe to call multiple times before calling queue() once. Returns true on
    // the first call, false after.
    bool onQueuingStart();
    void queue(const DrawItem &item);

    // Compiles the RectangularBuffer if necessary.
    // Safe to call multiple times before calling onQueueingStart() again.
    // Returns true on the first call, false after.
    bool onQueuingDone();

    // Valid after onQueueDone:
    u32 getMaxBytesInStrip() const;
    u32 getTotalBytes() const;
    void getBlockInfo(u32 *num_strips, u32 *bytes_per_strip,
                      u32 *total_bytes) const;

// protected:
    typedef fl::HeapVector<DrawItem> DrawList;
    // We manually manage the memory for the buffer of all LEDs so that it can
    // go into psram on ESP32S3, which is managed by fl::PSRamAllocator.
    scoped_array<u8> mAllLedsBufferUint8;
    u32 mAllLedsBufferUint8Size = 0;
    fl::FixedMap<u8, fl::span<u8>, 50> mPinToLedSegment;
    DrawList mDrawList;
    DrawList mPrevDrawList;
    bool mDrawListChangedThisFrame = false;

    enum QueueState { IDLE, QUEUEING, QUEUE_DONE };
    QueueState mQueueState = IDLE;
};

} // namespace fl
