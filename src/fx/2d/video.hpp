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
    Video(XYMap xymap) : FxGrid(xymap) {}

    void lazyInit() override {
        if (!mInitialized) {
            mInitialized = true;
            // Initialize video stream here if needed
        }
    }

    void draw(DrawContext context) override {
        if (!mVideoStream) {
            return;
        }
        VideoStream::Type type = mVideoStream->getType();
        if (type == VideoStream::kStreaming) {
            if (!mVideoStream->FramesRemaining()) {
                return;  // can't rewind streaming video
            }
        }
        if (!mVideoStream->FramesRemaining()) {
            mVideoStream->Rewind();
        }

        for (uint16_t i = 0; i < mXyMap.getTotal(); ++i) {
            CRGB pixel;
            if (mVideoStream->ReadPixel(&pixel)) {
                context.leds[i] = pixel;
            } else {
                context.leds[i] = CRGB::Black;
            }
        }
    }

    bool begin(FileHandlePtr fileHandle) {
        const uint8_t bytes_per_frame = getXYMap().getTotal() * 3;
        mVideoStream = RefPtr<VideoStream>::FromHeap(
            new VideoStream(bytes_per_frame));
        return mVideoStream->begin(fileHandle);
    }

    bool beginStream(ByteStreamPtr byteStream) {
        const uint8_t bytes_per_frame = getXYMap().getTotal() * 3;
        mVideoStream = RefPtr<VideoStream>::FromHeap(
            new VideoStream(bytes_per_frame));
        return mVideoStream->beginStream(byteStream);
    }

    void close() {
        mVideoStream->Close();
    }

    const char* fxName(int) const override { return "video"; }

private:
    VideoStreamPtr mVideoStream;
    bool mInitialized = false;

    FX_PROTECTED_DESTRUCTOR(Video) = default;
};

FASTLED_NAMESPACE_END
