
#include "crgb.h"
#include "fl/stl/allocator.h"
#include "fl/stl/bit_cast.h"
#include "fl/dbg.h"
#include "fl/warn.h"
#include "fl/xymap.h"
#include "frame.h"

#include "fl/stl/cstring.h"
namespace fl {

Frame::Frame(int pixels_count) : mPixelsCount(pixels_count), mRgb(), mIsFromCodec(false) {
    mRgb.resize(pixels_count);
    if (pixels_count > 0) {
        fl::memset((uint8_t*)mRgb.data(), 0, pixels_count * sizeof(CRGB));
    }
}

Frame::Frame(fl::u8* pixels, fl::u16 width, fl::u16 height, PixelFormat format, fl::u32 timestamp)
    : mPixelsCount(static_cast<size_t>(width) * height), mRgb(),
      mWidth(width), mHeight(height), mFormat(format), mTimestamp(timestamp), mIsFromCodec(true) {

    mRgb.resize(mPixelsCount);

    if (pixels && width > 0 && height > 0) {
        convertPixelsToRgb(pixels, format);
    } else if (mPixelsCount > 0) {
        fl::memset((uint8_t*)mRgb.data(), 0, mPixelsCount * sizeof(CRGB));
    }
}

Frame::Frame(const Frame& other)
    : mPixelsCount(other.mPixelsCount), mRgb(),
      mWidth(other.mWidth), mHeight(other.mHeight), mFormat(other.mFormat),
      mTimestamp(other.mTimestamp), mIsFromCodec(other.mIsFromCodec) {

    mRgb.resize(mPixelsCount);
    if (!other.mRgb.empty()) {
        fl::memcpy(mRgb.data(), other.mRgb.data(), mPixelsCount * sizeof(CRGB));
    }
}

Frame::~Frame() {
    // Vector will handle memory cleanup automatically
}

void Frame::draw(CRGB *leds, DrawMode draw_mode) const {
    if (!mRgb.empty()) {
        switch (draw_mode) {
        case DRAW_MODE_OVERWRITE: {
            fl::memcpy(leds, mRgb.data(), mPixelsCount * sizeof(CRGB));
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

void Frame::clear() {
    if (mPixelsCount > 0 && !mRgb.empty()) {
        fl::memset((uint8_t*)mRgb.data(), 0, mPixelsCount * sizeof(CRGB));
    }
}

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

bool Frame::isValid() const {
    if (mIsFromCodec) {
        return mWidth > 0 && mHeight > 0 && !mRgb.empty();
    }
    return !mRgb.empty();
}

void Frame::convertPixelsToRgb(fl::u8* pixels, PixelFormat format) {
    CRGB* rgbData = mRgb.data();

    switch (format) {
        case PixelFormat::RGB888: {
            for (size_t i = 0; i < mPixelsCount; i++) {
                fl::u8* pixel = &pixels[i * 3];
                rgbData[i] = CRGB(pixel[0], pixel[1], pixel[2]);
            }
            break;
        }
        case PixelFormat::RGB565: {
            fl::u16* pixel565 = fl::bit_cast<fl::u16*>(pixels);
            for (size_t i = 0; i < mPixelsCount; i++) {
                fl::u8 r, g, b;
                rgb565ToRgb888(pixel565[i], r, g, b);
                rgbData[i] = CRGB(r, g, b);
            }
            break;
        }
        case PixelFormat::RGBA8888: {
            for (size_t i = 0; i < mPixelsCount; i++) {
                fl::u8* pixel = &pixels[i * 4];
                rgbData[i] = CRGB(pixel[0], pixel[1], pixel[2]);
                // Ignoring alpha channel for now
            }
            break;
        }
        case PixelFormat::YUV420: {
            // Basic YUV420 to RGB conversion - simplified implementation
            // For now, just use the Y (luminance) component as grayscale
            for (size_t i = 0; i < mPixelsCount; i++) {
                fl::u8 y = pixels[i];
                rgbData[i] = CRGB(y, y, y);
            }
            break;
        }
        default: {
            // Fallback: fill with black
            if (mPixelsCount > 0 && !mRgb.empty()) {
                fl::memset((uint8_t*)rgbData, 0, mPixelsCount * sizeof(CRGB));
            }
            break;
        }
    }
}


} // namespace fl
