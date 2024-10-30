/// @brief Implements a video playback effect for 2D LED grids.

#pragma once

#include "FastLED.h"
#include "fx/detail/data_stream.h"
#include "fx/fx2d.h"
#include "fx/video/frame_interpolator.h"
#include "ref.h"

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_REF(Video);
FASTLED_SMART_REF(VideoFx);

class Video : public FxGrid {
  public:
    Video(XYMap xymap) : FxGrid(xymap) {}

    void lazyInit() override {
        if (!mInitialized) {
            mInitialized = true;
            // Initialize video stream here if needed
        }
    }

    bool begin(FileHandleRef fileHandle) {
        const uint8_t bytes_per_frame = getXYMap().getTotal() * 3;
        mDataStream = DataStreamRef::New(bytes_per_frame);
        return mDataStream->begin(fileHandle);
    }

    bool beginStream(ByteStreamRef byteStream) {
        const uint8_t bytes_per_frame = getXYMap().getTotal() * 3;
        mDataStream = DataStreamRef::New(bytes_per_frame);
        return mDataStream->beginStream(byteStream);
    }

    void close() { mDataStream->Close(); }

    void draw(DrawContext context) override {
        if (!mDataStream || !mDataStream->FramesRemaining()) {
            if (mDataStream && mDataStream->getType() != DataStream::kStreaming) {
                mDataStream->Rewind();
            } else {
                return; // Can't draw or rewind
            }
        }

        if (!mDataStream->available()) {
            return; // No data available
        }

        for (uint16_t w = 0; w < mXyMap.getWidth(); w++) {
            for (uint16_t h = 0; h < mXyMap.getHeight(); h++) {
                CRGB pixel;
                if (mDataStream->ReadPixel(&pixel)) {
                    context.leds[mXyMap.mapToIndex(w, h)] = pixel;
                } else {
                    context.leds[mXyMap.mapToIndex(w, h)] = CRGB::Black;
                }
            }
        }
    }



    const char *fxName(int) const override { return "video"; }

  private:
    DataStreamRef mDataStream;
    bool mInitialized = false;
};

// Converts a FxGrid to a video effect. This primarily allows for
// fixed frame rates and frame interpolation.
class VideoFx : public FxGrid {
  public:
    VideoFx(XYMap xymap) : FxGrid(xymap) {}

    void begin(uint32_t now, FxGridRef fx,uint16_t nFrameHistory, float fps = -1) {
        mDelegate = fx;
        if (!mDelegate) {
            return; // Early return if delegate is null
        }
        mDelegate->getXYMap().setRectangularGrid();
        mFps = fps < 0 ? 30 : fps;
        mDelegate->hasFixedFrameRate(&mFps);
        mFrameInterpolator = FrameInterpolatorRef::New(nFrameHistory, mFps);
        mFrameInterpolator->reset(now);
    }

    void lazyInit() override {
        if (!mInitialized) {
            mInitialized = true;
            mDelegate->lazyInit();
        }
    }

    void draw(DrawContext context) override {
        if (!mDelegate) {
            return;
        }

        uint32_t precise_timestamp;
        if (mFrameInterpolator->needsRefresh(context.now, &precise_timestamp)) {
            FrameRef frame;
            bool wasFullBeforePop = mFrameInterpolator->full();
            if (wasFullBeforePop) {
                if (!mFrameInterpolator->popOldest(&frame)) {
                    return; // Failed to pop, something went wrong
                }
                if (mFrameInterpolator->full()) {
                    return; // Still full after popping, something went wrong
                }
            } else {
                frame = FrameRef::New(mDelegate->getNumLeds(), mDelegate->hasAlphaChannel());
            }

            if (!frame) {
                return; // Something went wrong.
            }

            DrawContext delegateContext = context;
            delegateContext.leds = frame->rgb();
            delegateContext.alpha_channel = frame->alpha();
            delegateContext.now = precise_timestamp;
            mDelegate->draw(delegateContext);

            mFrameInterpolator->pushNewest(frame, precise_timestamp);
            mFrameInterpolator->incrementFrameCounter();
        }

        mFrameInterpolator->draw(context.now, context.leds, context.alpha_channel);
    }

    const char *fxName(int) const override { return "video_fx"; }

  private:
    Ref<FxGrid> mDelegate;
    bool mInitialized = false;
    FrameInterpolatorRef mFrameInterpolator;
    float mFps;
};

FASTLED_NAMESPACE_END
