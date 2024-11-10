/// @brief Implements a video playback effect for 2D LED grids.

#pragma once

#include "FastLED.h"
#include "fx/detail/data_stream.h"
#include "fx/fx2d.h"
#include "fx/video/frame_interpolator.h"
#include "ref.h"

FASTLED_NAMESPACE_BEGIN



FASTLED_SMART_REF(VideoFx);


class VideoFx : public FxGrid {
  public:
    VideoFx(XYMap xymap, Video video) : FxGrid(xymap) {}
    void draw(DrawContext context) override {
        if (!mFrame) {
            mFrame = FrameRef::New(mXyMap.getTotal(), false);
        }
        bool ok = mVideo.draw(context.now, mFrame.get());
        if (!ok) {
            mVideo.rewind();
            ok = mVideo.draw(context.now, mFrame.get());
            if (!ok) {
                return; // Can't draw or rewind
            }
        }
        if (!mFrame) {
            return; // Can't draw without a frame
        }

        const CRGB* src_pixels = mFrame->rgb();
        CRGB* dst_pixels = context.leds;
        size_t dst_pos = 0;
        for (uint16_t w = 0; w < mXyMap.getWidth(); w++) {
            for (uint16_t h = 0; h < mXyMap.getHeight(); h++) {
                const size_t index = mXyMap.mapToIndex(w, h);
                if (index < mFrame->size()) {
                    dst_pixels[dst_pos++] = src_pixels[index];
                }
            }
        }
    }



    const char *fxName(int) const override { return "video"; }

  private:
    Video mVideo;
    FrameRef mFrame;
};

#if 0

FASTLED_SMART_REF(VideoFx);

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
        if (mFrameInterpolator->needsFrame(context.now, &precise_timestamp)) {
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

#endif

FASTLED_NAMESPACE_END
