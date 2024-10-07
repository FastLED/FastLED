/// @brief Implements a video playback effect for 2D LED grids.

#pragma once

#include "FastLED.h"
#include "fx/fx2d.h"
#include "fx/util/video_stream.h"
#include "ptr.h"

FASTLED_NAMESPACE_BEGIN

FX_PTR(Video);

class Video : public FxGrid {
public:
    Video(XYMap xymap) : FxGrid(xymap), mVideoStream(xymap.getTotal()) {}

    void lazyInit() override {
        if (!mInitialized) {
            mInitialized = true;
            // Initialize video stream here if needed
        }
    }

    void draw(DrawContext context) override {
        if (!mVideoStream.FramesRemaining()) {
            mVideoStream.Rewind();
        }

        for (uint16_t i = 0; i < mXyMap.getTotal(); ++i) {
            CRGB pixel;
            if (mVideoStream.ReadPixel(&pixel)) {
                context.leds[i] = pixel;
            } else {
                context.leds[i] = CRGB::Black;
            }
        }
    }

    bool begin(FileHandlePtr fileHandle) {
        return mVideoStream.begin(fileHandle);
    }

    void close() {
        mVideoStream.Close();
    }

    const char* fxName(int) const override { return "video"; }

private:
    VideoStream mVideoStream;
    bool mInitialized = false;

    FX_PROTECTED_DESTRUCTOR(Video);
};

FASTLED_NAMESPACE_END
