#include "fx/video.h"

#include "crgb.h"
#include "fx/video/pixel_stream.h"
#include "fx/frame.h"
#include "fx/video/frame_interpolator.h"
#include "fl/dbg.h"
#include "fl/str.h"
#include "fl/warn.h"
#include "fl/math_macros.h"
#include "fx/video/video_impl.h"


#define DBG FASTLED_DBG

using fl::FileHandlePtr;
using fl::ByteStreamPtr;

#include "namespace.h"

namespace fl {

FASTLED_SMART_PTR(PixelStream);
FASTLED_SMART_PTR(FrameInterpolator);
FASTLED_SMART_PTR(Frame);


Video::Video() {
    setError("Video must be constructed with a pixel count and fps.");
}


Video::Video(size_t pixelsPerFrame, float fps, size_t frame_history_count) {
    mImpl = VideoImplPtr::New(pixelsPerFrame, fps, frame_history_count);
}

void Video::pause(uint32_t now) {
    if (mImpl) {
        mImpl->pause(now);
    }
}

void Video::resume(uint32_t now) {
    if (mImpl) {
        mImpl->resume(now);
    }
}

Video::~Video() = default;
Video::Video(const Video &) = default;
Video &Video::operator=(const Video &) = default;

bool Video::begin(FileHandlePtr handle) {
    if (!handle) {
        mError = "FileHandle is null";
        FASTLED_DBG(mError.c_str());
        return false;
    }
    if (mError.size()) {
        FASTLED_DBG(mError.c_str());
        return false;
    }
    mError.clear();
    mImpl->begin(handle);
    return true;
}

bool Video::beginStream(ByteStreamPtr bs) {
    if (!bs) {
        mError = "FileHandle is null";
        FASTLED_DBG(mError.c_str());
        return false;
    }
    if (mError.size()) {
        FASTLED_DBG(mError.c_str());
        return false;
    }
    mError.clear();
    mImpl->beginStream(bs);
    return true;
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
    if (!mImpl) {
        return;
    }
    mImpl->setTimeScale(timeScale);
}

float Video::timeScale() const {
    if (!mImpl) {
        return 1.0f;
    }
    return mImpl->timeScale();
}

fl::Str Video::error() const {
    return mError;
}

size_t Video::pixelsPerFrame() const {
    if (!mImpl) {
        return 0;
    }
    return mImpl->pixelsPerFrame();
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

VideoFx::VideoFx(Video video): Fx1d(video.pixelsPerFrame()), mVideo(video) {}

void VideoFx::draw(DrawContext context) {
    mVideo.draw(context.now, context.leds);
}

fl::Str VideoFx::fxName() const {
    return "VideoFx";
}

void VideoFx::pause(uint32_t now) {
    if (mVideo) {
        mVideo.pause(now);
    }
}

void VideoFx::resume(uint32_t now) {
    if (mVideo) {
        mVideo.resume(now);
    }
}

}  // namespace fl
