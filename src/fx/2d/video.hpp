/// @brief Implements a video playback effect for 2D LED grids.

#pragma once

#include "FastLED.h"
#include "fx/fx2d.h"
#include "fx/video/stream.h"
#include "ptr.h"

FASTLED_NAMESPACE_BEGIN

FX_PTR(Video);
FX_PTR(VideoFx);

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

        if (!mVideoStream->available() && mVideoStream->getType() == VideoStream::kStreaming) {
            // If we're streaming and we're out of data then bail.
            return;
        }

        for (uint16_t w = 0; w < mXyMap.getWidth(); w++) {
            for (uint16_t h = 0; h < mXyMap.getHeight(); h++) {
                CRGB pixel;
                if (mVideoStream->ReadPixel(&pixel)) {
                    context.leds[mXyMap.mapToIndex(w, h)] = pixel;
                } else {
                    context.leds[mXyMap.mapToIndex(w, h)] = CRGB::Black;
                }
            }
        }
    }

    bool begin(FileHandlePtr fileHandle) {
        const uint8_t bytes_per_frame = getXYMap().getTotal() * 3;
        mVideoStream = Ptr<VideoStream>::FromHeap(
            new VideoStream(bytes_per_frame));
        return mVideoStream->begin(fileHandle);
    }

    bool beginStream(ByteStreamPtr byteStream) {
        const uint8_t bytes_per_frame = getXYMap().getTotal() * 3;
        mVideoStream = Ptr<VideoStream>::FromHeap(
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
};



class VideoFx : public FxGrid {
public:
    VideoFx(XYMap xymap, Ptr<FxGrid> fx) : FxGrid(xymap), mDelegate(fx) {
        // Turn off re-mapping of the delegate's XYMap, similar to ScaleUp
        mDelegate->getXYMap().setRectangularGrid();
    }

    void lazyInit() override {
        if (!mInitialized) {
            mInitialized = true;
            mDelegate->lazyInit();
        }
    }

    void draw(DrawContext context) override {
        if (!mSurface) {
            mSurface.reset(new CRGB[mDelegate->getNumLeds()]);
        }
        DrawContext delegateContext = context;
        delegateContext.leds = mSurface.get();
        mDelegate->draw(delegateContext);
        getXYMap().mapPixels(mSurface.get(), context.leds);
    }

    const char* fxName(int) const override { return "video_fx"; }

private:
    Ptr<FxGrid> mDelegate;
    scoped_array<CRGB> mSurface;
    bool mInitialized = false;
};

FASTLED_NAMESPACE_END
