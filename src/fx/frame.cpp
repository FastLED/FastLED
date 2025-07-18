
#include <string.h>

#include "crgb.h"
#include "fl/allocator.h"
#include "fl/dbg.h"
#include "fl/namespace.h"
#include "fl/memory.h"
#include "fl/warn.h"
#include "fl/xymap.h"
#include "frame.h"

#include "fl/memfill.h"
namespace fl {

Frame::Frame(int pixels_count) : mPixelsCount(pixels_count), mRgb() {
    mRgb.resize(pixels_count);
    fl::memfill((uint8_t*)mRgb.data(), 0, pixels_count * sizeof(CRGB));
}

Frame::~Frame() {
    // Vector will handle memory cleanup automatically
}

void Frame::draw(CRGB *leds, DrawMode draw_mode) const {
    if (!mRgb.empty()) {
        switch (draw_mode) {
        case DRAW_MODE_OVERWRITE: {
            memcpy(leds, mRgb.data(), mPixelsCount * sizeof(CRGB));
            break;
        }
        case DRAW_MODE_BLEND_BY_MAX_BRIGHTNESS: {
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
    fl::u32 count = 0;
    for (uint16_t h = 0; h < height; ++h) {
        for (uint16_t w = 0; w < width; ++w) {
            fl::u32 in_idx = xyMap(w, h);
            fl::u32 out_idx = count++;
            if (in_idx >= mPixelsCount) {
                FASTLED_WARN(
                    "Frame::drawXY: in index out of range: " << in_idx);
                continue;
            }
            if (out_idx >= mPixelsCount) {
                FASTLED_WARN(
                    "Frame::drawXY: out index out of range: " << out_idx);
                continue;
            }
            switch (draw_mode) {
            case DRAW_MODE_OVERWRITE: {
                leds[out_idx] = mRgb[in_idx];
                break;
            }
            case DRAW_MODE_BLEND_BY_MAX_BRIGHTNESS: {
                leds[out_idx] =
                    CRGB::blendAlphaMaxChannel(mRgb[in_idx], leds[in_idx]);
                break;
            }
            }
        }
    }
}

void Frame::clear() { fl::memfill((uint8_t*)mRgb.data(), 0, mPixelsCount * sizeof(CRGB)); }

void Frame::interpolate(const Frame &frame1, const Frame &frame2,
                        uint8_t amountofFrame2, CRGB *pixels) {
    if (frame1.size() != frame2.size()) {
        return; // Frames must have the same size
    }

    const CRGB *rgbFirst = frame1.rgb();
    const CRGB *rgbSecond = frame2.rgb();

    if (frame1.mRgb.empty() || frame2.mRgb.empty()) {
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
    interpolate(frame1, frame2, amountOfFrame2, mRgb.data());
}

} // namespace fl
