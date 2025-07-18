
#include "fl/rectangular_draw_buffer.h"
#include "fl/allocator.h"
#include "fl/namespace.h"
#include "rgbw.h"

namespace fl {

DrawItem::DrawItem(u8 pin, u16 numLeds, bool is_rgbw)
    : mPin(pin), mIsRgbw(is_rgbw) {
    if (is_rgbw) {
        numLeds = Rgbw::size_as_rgb(numLeds);
    }
    mNumBytes = numLeds * 3;
}

span<u8>
RectangularDrawBuffer::getLedsBufferBytesForPin(u8 pin, bool clear_first) {
    auto it = mPinToLedSegment.find(pin);
    if (it == mPinToLedSegment.end()) {
        FASTLED_ASSERT(false, "Pin not found in RectangularDrawBuffer");
        return fl::span<u8>();
    }
    fl::span<u8> slice = it->second;
    if (clear_first) {
        memset(slice.data(), 0, slice.size() * sizeof(slice[0]));
    }
    return slice;
}

bool RectangularDrawBuffer::onQueuingStart() {
    if (mQueueState == QUEUEING) {
        return false;
    }
    mQueueState = QUEUEING;
    mPinToLedSegment.clear();
    mDrawList.swap(mPrevDrawList);
    mDrawList.clear();
    if (mAllLedsBufferUint8Size > 0) {
        memset(mAllLedsBufferUint8.get(), 0, mAllLedsBufferUint8Size);
    }
    return true;
}

void RectangularDrawBuffer::queue(const DrawItem &item) {
    mDrawList.push_back(item);
}

bool RectangularDrawBuffer::onQueuingDone() {
    if (mQueueState == QUEUE_DONE) {
        return false;
    }
    mQueueState = QUEUE_DONE;
    mDrawListChangedThisFrame = mDrawList != mPrevDrawList;
    // iterator through the current draw objects and calculate the total
    // number of bytes (representing RGB or RGBW) that will be drawn this frame.
    u32 total_bytes = 0;
    u32 max_bytes_in_strip = 0;
    u32 num_strips = 0;
    getBlockInfo(&num_strips, &max_bytes_in_strip, &total_bytes);
    if (total_bytes > mAllLedsBufferUint8Size) {
        u8 *old_ptr = mAllLedsBufferUint8.release();
        fl::PSRamAllocator<u8>::Free(old_ptr);
        u8 *ptr = fl::PSRamAllocator<u8>::Alloc(total_bytes);
        mAllLedsBufferUint8.reset(ptr);
    }
    mAllLedsBufferUint8Size = total_bytes;
    u32 offset = 0;
    for (auto it = mDrawList.begin(); it != mDrawList.end(); ++it) {
        u8 pin = it->mPin;
        span<u8> slice(mAllLedsBufferUint8.get() + offset,
                            max_bytes_in_strip);
        mPinToLedSegment[pin] = slice;
        offset += max_bytes_in_strip;
    }
    return true;
}

u32 RectangularDrawBuffer::getMaxBytesInStrip() const {
    u32 max_bytes = 0;
    for (auto it = mDrawList.begin(); it != mDrawList.end(); ++it) {
        max_bytes = MAX(max_bytes, it->mNumBytes);
    }
    return max_bytes;
}

u32 RectangularDrawBuffer::getTotalBytes() const {
    u32 num_strips = mDrawList.size();
    u32 max_bytes = getMaxBytesInStrip();
    return num_strips * max_bytes;
}

void RectangularDrawBuffer::getBlockInfo(u32 *num_strips,
                                         u32 *bytes_per_strip,
                                         u32 *total_bytes) const {
    *num_strips = mDrawList.size();
    *bytes_per_strip = getMaxBytesInStrip();
    *total_bytes = (*num_strips) * (*bytes_per_strip);
}

} // namespace fl
