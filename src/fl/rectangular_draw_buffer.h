
#include "fl/math_macros.h"
#include "fl/ptr.h"
#include "fl/scoped_ptr.h"
#include "fl/vector.h"
#include "rgbw.h"
#include "test.h"
#include "fl/slice.h"
#include "fl/map.h"

#include "fl/namespace.h"

namespace fl {

struct DrawItem {
    DrawItem() = default;
    DrawItem(uint8_t pin, uint16_t numLeds, bool is_rgbw): mPin(pin), mIsRgbw(is_rgbw) {
        if (is_rgbw) {
            numLeds = Rgbw::size_as_rgb(numLeds);
        } else {
            numLeds = numLeds;
        }
        mNumBytes = numLeds * 3;
    }
    uint8_t mPin = 0;
    uint32_t mNumBytes = 0;
    bool mIsRgbw = false;
    bool operator!=(const DrawItem &other) const {
        return mPin != other.mPin ||
               mNumBytes != other.mNumBytes ||
               mIsRgbw != other.mIsRgbw;
    }
};

// Maps multiple pins and CRGB strips to a single Rectanguarl Draw Buffer object.
// This is needed for the ObjectFLED and I2S controllers for ESP32S3.
class RectangularDrawBuffer {
  public:
    typedef fl::HeapVector<DrawItem> DrawList;

    fl::HeapVector<uint8_t> mAllLedsBufferUint8;
    fl::SortedHeapMap<uint8_t, fl::Slice<uint8_t>> mPinToLedSegment;
    DrawList mDrawList;
    DrawList mPrevDrawList;
    
    enum QueueState {
        IDLE,
        QUEUEING,
        QUEUE_DONE
    };
    QueueState mQueueState = IDLE;

    RectangularDrawBuffer() = default;
    ~RectangularDrawBuffer() = default;

    fl::Slice<uint8_t> getLedsBufferBytesForPin(uint8_t pin, bool clear_first) {
        auto it = mPinToLedSegment.find(pin);
        if (it == mPinToLedSegment.end()) {
            FASTLED_ASSERT(false, "Pin not found in RectangularDrawBuffer");
            return fl::Slice<uint8_t>();
        }
        fl::Slice<uint8_t> slice = it->second;
        if (clear_first) {
            memset(slice.data(), 0, slice.size());
        }
        return slice;
    }

    void onQueuingStart() {
        if (mQueueState == QUEUEING) {
            return;
        }
        mQueueState = QUEUEING;
        mPinToLedSegment.clear();
        mDrawList.swap(mPrevDrawList);
        mDrawList.clear();
        if (!mAllLedsBufferUint8.empty()) {
            memset(&mAllLedsBufferUint8.front(), 0, mAllLedsBufferUint8.size());
            mAllLedsBufferUint8.clear();
        }
    }


    void queue(const DrawItem& item) {
        mDrawList.push_back(item);
    }

    // Expected to allow the caller to call this multiple times before getLedsBufferBytesForPin(...).
    void onQueuingDone() {
        if (mQueueState == QUEUE_DONE) {
            return;
        }
        mQueueState = QUEUE_DONE;
        // iterator through the current draw objects and calculate the total
        // number of bytes (representing RGB or RGBW) that will be drawn this frame.
        uint32_t total_bytes = 0;
        uint32_t max_bytes_in_strip = 0;
        uint32_t num_strips = 0;
        getBlockInfo(&num_strips, &max_bytes_in_strip, &total_bytes);
        mAllLedsBufferUint8.resize(total_bytes);
        memset(&mAllLedsBufferUint8.front(), 0, mAllLedsBufferUint8.size());
        uint32_t offset = 0;
        for (auto it = mDrawList.begin(); it != mDrawList.end(); ++it) {
            uint8_t pin = it->mPin;
            fl::Slice<uint8_t> slice(&mAllLedsBufferUint8.front() + offset, max_bytes_in_strip);
            mPinToLedSegment[pin] = slice;
            offset += max_bytes_in_strip;
        }
    }

    uint32_t getMaxBytesInStrip() const {
        uint32_t max_bytes = 0;
        for (auto it = mDrawList.begin(); it != mDrawList.end(); ++it) {
            max_bytes = MAX(max_bytes, it->mNumBytes);
        }
        return max_bytes;
    }

    uint32_t getTotalBytes() const {
        uint32_t num_strips = mDrawList.size();
        uint32_t max_bytes = getMaxBytesInStrip();
        return num_strips * max_bytes;
    }

    void getBlockInfo(uint32_t* num_strips, uint32_t* bytes_per_strip, uint32_t* total_bytes) const {
        *num_strips = mDrawList.size();
        *bytes_per_strip = getMaxBytesInStrip();
        *total_bytes = (*num_strips) * (*bytes_per_strip);
    }
};

}