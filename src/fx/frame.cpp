
#include <string.h>

#include "crgb.h"
#include "fl/allocator.h"
#include "fl/dbg.h"
#include "fl/namespace.h"
#include "fl/ptr.h"
#include "fl/warn.h"
#include "fl/xymap.h"
#include "frame.h"

using namespace fl;

namespace fl {

Frame::Frame(int pixels_count) : mPixelsCount(pixels_count), mRgb() {
    mRgb.reset(reinterpret_cast<CRGB *>(
        LargeBlockAllocate(pixels_count * sizeof(CRGB))));
    memset(mRgb.get(), 0, pixels_count * sizeof(CRGB));
}

Frame::~Frame() {
    if (mRgb) {
        LargeBlockDeallocate(mRgb.release());
    }
}

void Frame::draw(CRGB *leds, DrawMode draw_mode) const {
    if (mRgb) {
        switch (draw_mode) {
        case DRAW_MODE_OVERWRITE: {
            memcpy(leds, mRgb.get(), mPixelsCount * sizeof(CRGB));
            break;
        }
        case DRAW_MODE_BLEND_BY_BLACK: {
            for (size_t i = 0; i < mPixelsCount; ++i) {
                leds[i] = CRGB::blendAlphaMaxChannel(mRgb[i], leds[i]);
            }
            break;
        }
        }
    }
}

void Frame::drawXY(CRGB *leds, const XYMap &xyMap, DrawMode draw_mode) const {
    const uint16_t width = xyMap.getWidth();
    const uint16_t height = xyMap.getHeight();
    uint32_t count = 0;
    for (uint16_t h = 0; h < height; ++h) {
        for (uint16_t w = 0; w < width; ++w) {
            uint32_t in_idx = xyMap(w, h);
            uint32_t out_idx = count++;
            if (in_idx >= mPixelsCount) {
                FASTLED_WARN("Frame::drawXY: in index out of range: " << in_idx);
                continue;
            }
            if (out_idx >= mPixelsCount) {
                FASTLED_WARN("Frame::drawXY: out index out of range: " << out_idx);
                continue;
            }
            switch (draw_mode) {
            case DRAW_MODE_OVERWRITE: {
                leds[out_idx] = mRgb[in_idx];
                break;
            }
            case DRAW_MODE_BLEND_BY_BLACK: {
                leds[out_idx] = CRGB::blendAlphaMaxChannel(mRgb[in_idx], leds[in_idx]);
                break;
            }
            }
        }
    }
}

void Frame::clear() { memset(mRgb.get(), 0, mPixelsCount * sizeof(CRGB)); }

void Frame::interpolate(const Frame &frame1, const Frame &frame2,
                        uint8_t amountofFrame2, CRGB *pixels) {
    if (frame1.size() != frame2.size()) {
        return; // Frames must have the same size
    }

    const CRGB *rgbFirst = frame1.rgb();
    const CRGB *rgbSecond = frame2.rgb();

    if (!rgbFirst || !rgbSecond) {
        // Error, why are we getting null pointers?
        return;
    }

    for (size_t i = 0; i < frame2.size(); ++i) {
        pixels[i] = CRGB::blend(rgbFirst[i], rgbSecond[i], amountofFrame2);
    }
    // We will eventually do something with alpha.
}

void Frame::interpolate(const Frame &frame1, const Frame &frame2,
                        uint8_t amountOfFrame2) {
    if (frame1.size() != frame2.size() || frame1.size() != mPixelsCount) {
        FASTLED_DBG("Frames must have the same size");
        return; // Frames must have the same size
    }
    interpolate(frame1, frame2, amountOfFrame2, mRgb.get());
}

} // namespace fl
