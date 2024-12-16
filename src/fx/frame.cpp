
#include <string.h>

#include "frame.h"
#include "crgb.h"
#include "fl/namespace.h"
#include "fl/ptr.h"
#include "fl/dbg.h"
#include "fl/allocator.h"


namespace fl {


Frame::Frame(int pixels_count)
    : mPixelsCount(pixels_count), mRgb() {
    mRgb.reset(reinterpret_cast<CRGB *>(LargeBlockAllocate(pixels_count * sizeof(CRGB))));
    memset(mRgb.get(), 0, pixels_count * sizeof(CRGB));
}

Frame::~Frame() {
    if (mRgb) {
        LargeBlockDeallocate(mRgb.release());
    }
}

void Frame::draw(CRGB* leds) const {
    if (mRgb) {
        memcpy(leds, mRgb.get(), mPixelsCount * sizeof(CRGB));
    }
}


void Frame::interpolate(const Frame& frame1, const Frame& frame2, uint8_t amountofFrame2, CRGB* pixels) {
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

void Frame::interpolate(const Frame& frame1, const Frame& frame2, uint8_t amountOfFrame2) {
    if (frame1.size() != frame2.size() || frame1.size() != mPixelsCount) {
        FASTLED_DBG("Frames must have the same size");
        return;  // Frames must have the same size
    }
    interpolate(frame1, frame2, amountOfFrame2, mRgb.get());
}

}  // namespace fl
