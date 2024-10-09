/// @brief Implements a video playback effect for 2D LED grids.

#pragma once

#include "FastLED.h"
#include "fx/detail/data_stream.h"
#include "fx/fx2d.h"
#include "fx/video/frame_interpolator.h"
#include "fx/video/stream_buffered.h"
#include "ptr.h"

FASTLED_NAMESPACE_BEGIN

DECLARE_SMART_PTR(Video);
DECLARE_SMART_PTR(VideoFx);

class Video : public FxGrid {
  public:
    Video(XYMap xymap) : FxGrid(xymap) {}

    void lazyInit() override {
        if (!mInitialized) {
            mInitialized = true;
            // Initialize video stream here if needed
        }
    }

    bool begin(FileHandlePtr fileHandle) {
        const uint8_t bytes_per_frame = getXYMap().getTotal() * 3;
        mDataStream = DataStreamPtr::New(bytes_per_frame);
        return mDataStream->begin(fileHandle);
    }

    bool beginStream(ByteStreamPtr byteStream) {
        const uint8_t bytes_per_frame = getXYMap().getTotal() * 3;
        mDataStream = DataStreamPtr::New(bytes_per_frame);
        return mDataStream->beginStream(byteStream);
    }

    void close() { mDataStream->Close(); }

    void draw(DrawContext context) override {
        if (!mDataStream) {
            return;
        }
        DataStream::Type type = mDataStream->getType();
        if (type == DataStream::kStreaming) {
            if (!mDataStream->FramesRemaining()) {
                return; // can't rewind streaming video
            }
        }

        if (!mDataStream->FramesRemaining()) {
            mDataStream->Rewind();
        }

        if (!mDataStream->available() &&
            mDataStream->getType() == DataStream::kStreaming) {
            // If we're streaming and we're out of data then bail.
            return;
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
    DataStreamPtr mDataStream;
    bool mInitialized = false;
};

class VideoFx : public FxGrid {
  public:
    VideoFx(XYMap xymap): FxGrid(xymap) {}

    void begin(FxGridPtr fx, uint16_t nFrameHistory, float fps = -1) {
        mDelegate = fx;
        mDelegate->getXYMap().setRectangularGrid();
        float _fps = fps < 0 ? 30 : fps;
        mDelegate->hasFixedFrameRate(&_fps);
        mVideoStream =
            VideoStreamPtr::New(mDelegate->getNumLeds(), nFrameHistory, _fps);
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
        #if 0
        if (mVideoStream->needsRefresh(context.now)) {
            FramePtr frame;

            if (mVideoStream->full()) {
                frame = mVideoStream->popOldest();
            } else {
                frame = FramePtr::New(mDelegate->getNumLeds(),
                                      mDelegate->hasAlphaChannel());
            }
            if (!frame) {
                return; // Something went wrong.
            }

            DrawContext delegateContext = context;
            delegateContext.leds = frame->rgb();
            delegateContext.alpha_channel = frame->alpha();
            mDelegate->draw(delegateContext);
            frame->setTimestamp(context.now);
            mVideoStream->pushNewest(frame);
        }
        #endif



        //mVideoStream->draw(
    }

    const char *fxName(int) const override { return "video_fx"; }

  private:
    Ptr<FxGrid> mDelegate;
    bool mInitialized = false;
    VideoStreamPtr mVideoStream;
};

FASTLED_NAMESPACE_END
