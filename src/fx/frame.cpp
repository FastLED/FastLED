
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

void Frame::interpolate(const Frame& frame1, const Frame& frame2, float t) {
    if (frame1.size() != frame2.size() || frame1.size() != mPixelsCount) {
        return;  // Frames must have the same size
    }

    uint8_t progress = static_cast<uint8_t>(t * 255);

    for (size_t i = 0; i < mPixelsCount; ++i) {
        mRgb[i] = CRGB::blend(frame1.rgb()[i], frame2.rgb()[i], progress);
    }

    if (mAlpha && frame1.alpha() && frame2.alpha()) {
        /*
        TODO: Handle alpha
        for (size_t i = 0; i < mPixelsCount; ++i) {
            mAlpha[i] = blend8(frame1.alpha()[i], frame2.alpha()[i], progress);
        }
        */
    }
}

FASTLED_NAMESPACE_END
