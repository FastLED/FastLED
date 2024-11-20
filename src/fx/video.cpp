#include "fx/video.h"

#include "crgb.h"
#include "fx/detail/data_stream.h"
#include "fx/frame.h"
#include "fx/video/frame_interpolator.h"
#include "fl/dbg.h"

#define DBG FASTLED_DBG


#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_REF(DataStream);
FASTLED_SMART_REF(FrameInterpolator);
FASTLED_SMART_REF(Frame);

class VideoImpl : public Referent {
  public:
    // frameHistoryCount is the number of frames to keep in the buffer after
    // draw. This allows for time based effects like syncing video speed to
    // audio triggers.
    VideoImpl(size_t pixelsPerFrame, float fpsVideo,
              size_t frameHistoryCount = 0);
    ~VideoImpl();
    // Api
    void begin(FileHandleRef h);
    void beginStream(ByteStreamRef s);
    bool draw(uint32_t now, CRGB *leds, uint8_t *alpha = nullptr);
    void end();
    bool rewind();
    // internal use
    bool draw(uint32_t now, Frame *frame);
    bool full() const;
    FrameRef popOldest();

  private:
    bool updateBufferIfNecessary(uint32_t prev, uint32_t now);
    uint32_t mPixelsPerFrame = 0;
    DataStreamRef mStream;
    uint32_t mPrevNow = 0;
    FrameInterpolatorRef mFrameInterpolator;
};

VideoImpl::VideoImpl(size_t pixelsPerFrame, float fpsVideo,
                     size_t nFramesInBuffer)
    : mPixelsPerFrame(pixelsPerFrame),
      mFrameInterpolator(
          // todo: the max number of frames should be 1 or 0.
          FrameInterpolatorRef::New(MAX(0, nFramesInBuffer), fpsVideo)) {}

VideoImpl::~VideoImpl() { end(); }

void VideoImpl::begin(FileHandleRef h) {
    end();
    // Removed setStartTime call
    mStream = DataStreamRef::New(mPixelsPerFrame);
    mStream->begin(h);
    mPrevNow = 0;
}

void VideoImpl::beginStream(ByteStreamRef bs) {
    end();
    mStream = DataStreamRef::New(mPixelsPerFrame);
    // Removed setStartTime call
    mStream->beginStream(bs);
    mPrevNow = 0;
}

void VideoImpl::end() {
    mFrameInterpolator->clear();
    // Removed resetFrameCounter and setStartTime calls
    mStream.reset();
}

bool VideoImpl::full() const { return mFrameInterpolator->getFrames()->full(); }

FrameRef VideoImpl::popOldest() {
    FrameRef frame;
    mFrameInterpolator->pop_back(&frame);
    return frame;
}

bool VideoImpl::draw(uint32_t now, Frame *frame) {
    if (!mStream) {
        return false;
    }
    updateBufferIfNecessary(mPrevNow, now);
    mPrevNow = now;
    if (!frame) {
        return false;
    }
    bool ok = mFrameInterpolator->draw(now, frame);
    return ok;
}

bool VideoImpl::draw(uint32_t now, CRGB *leds, uint8_t *alpha) {
    if (!mStream) {
        return false;
    }
    bool ok = updateBufferIfNecessary(mPrevNow, now);
    mPrevNow = now;
    if (!ok) {
        // paint black
        memset(leds, 0, mPixelsPerFrame * sizeof(CRGB));
        if (alpha) {
            memset(alpha, 0, mPixelsPerFrame);
        }
        return false;
    }
    mFrameInterpolator->draw(now, leds, alpha);
    return true;
}

bool VideoImpl::updateBufferIfNecessary(uint32_t prev, uint32_t now) {
    const bool forward = now >= prev;
    // At most, update one frame. That way if the user forgets to call draw and
    // then sends a really old timestamp, we don't update the buffer too much.
    uint32_t currFrameNumber = 0;
    uint32_t nextFrameNumber = 0;
    while (mFrameInterpolator->needsFrame(now, &currFrameNumber, &nextFrameNumber)) {
        if (mFrameInterpolator->empty()) {
            // we are missing the first frame
            FrameRef frame = FrameRef::New(mPixelsPerFrame, false);
            if (!mStream->readFrame(frame.get())) {
                if (!mStream->rewind()) {
                    FASTLED_DBG("readFrame (1) failed");
                    return false;
                }
                if (!mStream->readFrame(frame.get())) {
                    FASTLED_DBG("readFrame (2) failed");
                    return false;
                }
            }
            uint32_t timestamp = mFrameInterpolator->get_exact_timestamp_ms(currFrameNumber);
            frame->setFrameNumberAndTime(currFrameNumber, timestamp);
            mFrameInterpolator->push_front(frame, currFrameNumber, timestamp);
            continue;
        }

        uint32_t newest_frame_number = 0;
        bool has_newest = mFrameInterpolator->get_newest_frame_number(&newest_frame_number);
        if (!has_newest) {
            DBG("get_newest_frame_number failed");
            return false;
        }

        FrameRef frame;
        if (full()) {
            frame = popOldest();
        } else {
            // We will continue allocating until the buffer fills up with frames, then we start
            // recycling.
            frame = FrameRef::New(mPixelsPerFrame, false);
        }
        if (!mStream->readFrame(frame.get())) {
            if (!mStream->rewind()) {
                DBG("readFrame (3) failed");
                return false;
            }
            if (!mStream->readFrame(frame.get())) {
                DBG("readFrame (4) failed");
                return false;
            }
        }
        uint32_t next_frame = newest_frame_number + 1;
        uint32_t next_timestamp = mFrameInterpolator->get_exact_timestamp_ms(next_frame);
        frame->setFrameNumberAndTime(next_frame, next_timestamp);
        bool ok = mFrameInterpolator->push_front(frame, next_frame, next_timestamp);
        if (!ok) {
            DBG("pushNewest failed");
            return false;
        }
    }
    return true;
}

bool VideoImpl::rewind() {
    if (!mStream || !mStream->rewind()) {
        return false;
    }
    mFrameInterpolator->clear();
    return true;
}

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
    mImpl.reset();
    mImpl = VideoImplRef::New(pixelsPerFrame, fps, frameHistoryCount);
    mImpl->begin(h);
}



void Video::beginStream(ByteStreamRef bs, size_t pixelsPerFrame, float fps,
                        size_t frameHistoryCount) {
    mImpl.reset();
    mImpl = VideoImplRef::New(pixelsPerFrame, fps, frameHistoryCount);
    mImpl->beginStream(bs);
}

bool Video::draw(uint32_t now, CRGB *leds, uint8_t *alpha) {
    if (!mTimeScale) {
        mTimeScale = TimeScaleRef::New(now);
    }
    mTimeScale->update(now);
    now = mTimeScale->time();
    if (!mImpl) {
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

void Video::setScale(float timeScale) { mTimeScale->setScale(timeScale); }

float Video::scale() const { return mTimeScale->scale(); }

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
