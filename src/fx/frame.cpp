
#include "frame.h"
#include "crgb.h"
#include "namespace.h"
#include "ptr.h"
#include "allocator.h"
#include <string.h>

FASTLED_NAMESPACE_BEGIN


Frame::Frame(int pixels_count, bool has_alpha)
    : mPixelsCount(pixels_count), mRgb() {
    mRgb.reset(reinterpret_cast<CRGB *>(LargeBlockAllocate(pixels_count * sizeof(CRGB))));
    memset(mRgb.get(), 0, pixels_count * sizeof(CRGB));
    if (has_alpha) {
        mAlpha.reset(reinterpret_cast<uint8_t *>(LargeBlockAllocate(pixels_count)));
        memset(mAlpha.get(), 0, pixels_count);
    }
}

Frame::~Frame() {
    if (mRgb) {
        LargeBlockDeallocate(mRgb.release());
    }
    if (mAlpha) {
        LargeBlockDeallocate(mAlpha.release());
    }
}

FASTLED_NAMESPACE_END
