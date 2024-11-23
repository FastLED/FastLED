#include "fx/video.h"

#include "crgb.h"
#include "fx/detail/data_stream.h"
#include "fx/frame.h"
#include "fx/video/frame_interpolator.h"
#include "fl/dbg.h"
#include "fl/str.h"
#include "warn.h"
#include "math_macros.h"
#include "fx/video/video_impl.h"


#define DBG FASTLED_DBG


#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_REF(DataStream);
FASTLED_SMART_REF(FrameInterpolator);
FASTLED_SMART_REF(Frame);


Video::Video() = default;
Video::Video(FileHandleRef h, size_t pixelsPerFrame, float fps, size_t frameHistoryCount) {
    begin(h, pixelsPerFrame, fps, frameHistoryCount);
}
Video::Video(ByteStreamRef s, size_t pixelsPerFrame, float fps, size_t frameHistoryCount) {
    beginStream(s, pixelsPerFrame, fps, frameHistoryCount);
}
Video::~Video() = default;
Video::Video(const Video &) = default;
Video &Video::operator=(const Video &) = default;

void Video::begin(FileHandleRef h, size_t pixelsPerFrame, float fps,
                  size_t frameHistoryCount) {
    mError.clear();
    mImpl.reset();
    mImpl = VideoImplRef::New(pixelsPerFrame, fps, frameHistoryCount);
    mImpl->begin(h);
}



void Video::beginStream(ByteStreamRef bs, size_t pixelsPerFrame, float fps,
                        size_t frameHistoryCount) {
    mError.clear();
    mImpl.reset();
    mImpl = VideoImplRef::New(pixelsPerFrame, fps, frameHistoryCount);
    mImpl->beginStream(bs);
}

bool Video::draw(uint32_t now, CRGB *leds, uint8_t *alpha) {

    if (!mImpl) {
        FASTLED_WARN_IF(!mError.empty(), mError.c_str());
        return false;
    }
    bool ok = mImpl->draw(now, leds, alpha);
    if (!ok) {
        // Interpret not being able to draw as a finished signal.
        mFinished = true;
    }
    return ok;
}

bool Video::draw(uint32_t now, Frame *frame) {
    if (!mImpl) {
        return false;
    }
    return mImpl->draw(now, frame);
}

void Video::end() {
    if (mImpl) {
        mImpl->end();
    }
}

void Video::setTimeScale(float timeScale) {
    // mTimeScale->setScale(timeScale);
    mImpl->setTimeScale(timeScale);
}

float Video::timeScale() const { return mImpl->timeScale(); }

fl::Str Video::error() const {
    return mError;
}

bool Video::finished() {
    if (!mImpl) {
        return true;
    }
    return mFinished;
}

bool Video::rewind() {
    if (!mImpl) {
        return false;
    }
    return mImpl->rewind();
}

VideoFx::VideoFx(Video video, XYMap xymap) : FxGrid(xymap), mVideo(video) {}

void VideoFx::draw(DrawContext context) {
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

    const CRGB *src_pixels = mFrame->rgb();
    CRGB *dst_pixels = context.leds;
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

const char * VideoFx::fxName(int) const { return "video"; }

FASTLED_NAMESPACE_END
