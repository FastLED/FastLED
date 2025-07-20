#include "fx/video.h"

#include "crgb.h"
#include "fl/bytestreammemory.h"
#include "fl/dbg.h"
#include "fl/math_macros.h"
#include "fl/str.h"
#include "fl/warn.h"
#include "fx/frame.h"
#include "fx/video/frame_interpolator.h"
#include "fx/video/pixel_stream.h"
#include "fx/video/video_impl.h"

#define DBG FASTLED_DBG

using fl::ByteStreamPtr;
using fl::FileHandlePtr;

#include "fl/namespace.h"

namespace fl {

FASTLED_SMART_PTR(PixelStream);
FASTLED_SMART_PTR(FrameInterpolator);
FASTLED_SMART_PTR(Frame);

Video::Video() : Fx1d(0) {}

Video::Video(size_t pixelsPerFrame, float fps, size_t frame_history_count)
    : Fx1d(pixelsPerFrame) {
    mImpl = fl::make_shared<VideoImpl>(pixelsPerFrame, fps, frame_history_count);
}

void Video::setFade(fl::u32 fadeInTime, fl::u32 fadeOutTime) {
    mImpl->setFade(fadeInTime, fadeOutTime);
}

void Video::pause(fl::u32 now) { mImpl->pause(now); }

void Video::resume(fl::u32 now) { mImpl->resume(now); }

Video::~Video() = default;
Video::Video(const Video &) = default;
Video &Video::operator=(const Video &) = default;

bool Video::begin(FileHandlePtr handle) {
    if (!mImpl) {
        FASTLED_WARN("Video::begin: mImpl is null, manually constructed videos "
                     "must include full parameters.");
        return false;
    }
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

bool Video::draw(fl::u32 now, CRGB *leds) {
    if (!mImpl) {
        FASTLED_WARN_IF(!mError.empty(), mError.c_str());
        return false;
    }
    bool ok = mImpl->draw(now, leds);
    if (!ok) {
        // Interpret not being able to draw as a finished signal.
        mFinished = true;
    }
    return ok;
}

void Video::draw(DrawContext context) {
    if (!mImpl) {
        FASTLED_WARN_IF(!mError.empty(), mError.c_str());
        return;
    }
    mImpl->draw(context.now, context.leds);
}

int32_t Video::durationMicros() const {
    if (!mImpl) {
        return -1;
    }
    return mImpl->durationMicros();
}

Str Video::fxName() const { return "Video"; }

bool Video::draw(fl::u32 now, Frame *frame) {
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

Str Video::error() const { return mError; }

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

VideoFxWrapper::VideoFxWrapper(fl::shared_ptr<Fx> fx) : Fx1d(fx->getNumLeds()), mFx(fx) {
    if (!mFx->hasFixedFrameRate(&mFps)) {
        FASTLED_WARN("VideoFxWrapper: Fx does not have a fixed frame rate, "
                     "assuming 30fps.");
        mFps = 30.0f;
    }
    mVideo = fl::make_shared<VideoImpl>(mFx->getNumLeds(), mFps, 2);
    mByteStream = fl::make_shared<ByteStreamMemory>(mFx->getNumLeds() * sizeof(CRGB));
    mVideo->beginStream(mByteStream);
}

VideoFxWrapper::~VideoFxWrapper() = default;

Str VideoFxWrapper::fxName() const {
    Str out = "video_fx_wrapper: ";
    out.append(mFx->fxName());
    return out;
}

void VideoFxWrapper::draw(DrawContext context) {
    if (mVideo->needsFrame(context.now)) {
        mFx->draw(context); // use the leds in the context as a tmp buffer.
        mByteStream->writeCRGB(
            context.leds,
            mFx->getNumLeds()); // now write the leds to the byte stream.
    }
    bool ok = mVideo->draw(context.now, context.leds);
    if (!ok) {
        FASTLED_WARN("VideoFxWrapper: draw failed.");
    }
}

void VideoFxWrapper::setFade(fl::u32 fadeInTime, fl::u32 fadeOutTime) {
    mVideo->setFade(fadeInTime, fadeOutTime);
}

} // namespace fl
