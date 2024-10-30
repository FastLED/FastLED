
#include <string.h>

#include "frame.h"
#include "crgb.h"
#include "namespace.h"
#include "ref.h"
#include "allocator.h"


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

void Frame::draw(CRGB* leds, uint8_t* alpha) const {
    if (mRgb) {
        memcpy(leds, mRgb.get(), mPixelsCount * sizeof(CRGB));
    }
    if (alpha && mAlpha) {
        memcpy(alpha, mAlpha.get(), mPixelsCount);
    }
}

void Frame::interpolate(const Frame& frame1, const Frame& frame2, uint8_t amountofFrame2, CRGB* pixels, uint8_t* alpha) {
    if (frame1.size() != frame2.size()) {
        return;  // Frames must have the same size
    }

    const CRGB* rgbFirst = frame1.rgb();
    const CRGB* rgbSecond = frame2.rgb();

    if (!rgbFirst || !rgbSecond) {
        // Error, why are we getting null pointers?
        return;
    }

    for (size_t i = 0; i < frame2.size(); ++i) {
        pixels[i] = CRGB::blend(rgbFirst[i], rgbSecond[i], amountofFrame2);
    }
    // We will eventually do something with alpha.
}

void Frame::interpolate(const Frame& frame1, const Frame& frame2, uint8_t progress) {
    if (frame1.size() != frame2.size() || frame1.size() != mPixelsCount) {
        return;  // Frames must have the same size
    }
    interpolate(frame1, frame2, progress, mRgb.get(), mAlpha.get());
}

FASTLED_NAMESPACE_END
