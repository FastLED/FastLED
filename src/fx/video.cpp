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

bool VideoImpl::draw(uint32_t now, Frame *frame) {
    //DBG("draw with now = " << now);
    if (!mStream) {
        DBG("no stream");
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
    //DBG("draw with now = " << now);
    if (!mStream) {
        DBG("no stream");
        return false;
    }
    bool ok = updateBufferIfNecessary(mPrevNow, now);
    mPrevNow = now;
    if (!ok) {
        DBG("updateBufferIfNecessary failed");
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
    bool needs_frame = mFrameInterpolator->needsFrame(now, &currFrameNumber, &nextFrameNumber);
    //DBG("needs_frame: " << needs_frame);
    //DBG("currFrameNumber: " << currFrameNumber << " nextFrameNumber: " << nextFrameNumber);
    //DBG("has curr frame: " << mFrameInterpolator->has(currFrameNumber));
    //DBG("has next frame: " << mFrameInterpolator->has(nextFrameNumber));
    if (!needs_frame) {
        return true;
    }
    bool has_curr_frame = mFrameInterpolator->has(currFrameNumber);
    bool has_next_frame = mFrameInterpolator->has(nextFrameNumber);
    if (has_curr_frame && has_next_frame) {
        return true;
    }
    if (mFrameInterpolator->capacity() == 0) {
        DBG("capacity == 0");
        return false;
    }

    FixedVector<uint32_t, 2> frame_numbers;
    if (!mFrameInterpolator->has(currFrameNumber)) {
        frame_numbers.push_back(currFrameNumber);
    }
    if (mFrameInterpolator->capacity() > 1 && !mFrameInterpolator->has(nextFrameNumber)) {
        frame_numbers.push_back(nextFrameNumber);
    }

    for (size_t i = 0; i < frame_numbers.size(); ++i) {
        FrameRef recycled_frame;
        if (mFrameInterpolator->full()) {
            uint32_t frame_to_erase = 0;
            bool ok = false;
            if (forward) {
                ok = mFrameInterpolator->get_oldest_frame_number(&frame_to_erase);
                if (!ok) {
                    DBG("get_oldest_frame_number failed");
                    return false;
                }
            } else {
                ok = mFrameInterpolator->get_newest_frame_number(&frame_to_erase);
                if (!ok) {
                    DBG("get_newest_frame_number failed");
                    return false;
                }
            }
            recycled_frame = mFrameInterpolator->erase(frame_to_erase);
            if (!recycled_frame) {
                DBG("erase failed for frame: " << frame_to_erase);
                return false;
            }
        }
        uint32_t frame_to_fetch = frame_numbers[i];
        if (!recycled_frame) {
            // Happens when we are not full and we need to allocate a new frame.
            recycled_frame = FrameRef::New(mPixelsPerFrame, false);
        }
        if (!mStream->readFrame(recycled_frame.get())) {
            if (!forward) {
                // nothing more we can do, we can't go negative.
                return false;
            }
            if (mStream->atEnd()) {
                DBG("Can't go rewind until the TimeScale is migrated to VideoImpl");
                return false;
            }
            DBG("We failed for some other reason");
            return false;
        }
        uint32_t timestamp = mFrameInterpolator->get_exact_timestamp_ms(frame_to_fetch);
        recycled_frame->setFrameNumberAndTime(frame_to_fetch, timestamp);
        DBG("inserting frame: " << frame_to_fetch);
        bool ok = mFrameInterpolator->insert(frame_to_fetch, recycled_frame);
        if (!ok) {
            DBG("insert failed");
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
