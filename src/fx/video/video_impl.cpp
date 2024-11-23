


#include "video_impl.h"

#include "fl/dbg.h"
#include "math_macros.h"
#include "namespace.h"


#define DBG FASTLED_DBG

FASTLED_NAMESPACE_BEGIN

VideoImpl::VideoImpl(size_t pixelsPerFrame, float fpsVideo,
                     size_t nFramesInBuffer)
    : mPixelsPerFrame(pixelsPerFrame),
      mFrameInterpolator(
          // todo: the max number of frames should be 1 or 0.
          FrameInterpolatorRef::New(MAX(0, nFramesInBuffer), fpsVideo)) {}


void VideoImpl::setTimeScale(float timeScale) {
    if (!mTimeScale) {
        mTimeScale = TimeScaleRef::New(0, timeScale);
    }
    mTimeScale->setScale(timeScale);
}


VideoImpl::~VideoImpl() { end(); }

void VideoImpl::begin(FileHandleRef h) {
    end();
    // Removed setStartTime call
    mStream = PixelStreamRef::New(mPixelsPerFrame * kSizeRGB8);
    mStream->begin(h);
    mPrevNow = 0;
}

void VideoImpl::beginStream(ByteStreamRef bs) {
    end();
    mStream = PixelStreamRef::New(mPixelsPerFrame * kSizeRGB8);
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
    if (!mTimeScale) {
        mTimeScale = TimeScaleRef::New(now);
    }
    now = mTimeScale->update(now);
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

       do {  // only to use break
            if (!mStream->readFrameAt(frame_to_fetch, recycled_frame.get())) {
                if (!forward) {
                    // nothing more we can do, we can't go negative.
                    return false;
                }
                if (mStream->atEnd()) {
                    if (!mStream->rewind()) {  // Is this still 
                        DBG("rewind failed");
                        return false;
                    }
                    mTimeScale->reset(now);
                    frame_to_fetch = 0;
                    if (!mStream->readFrameAt(frame_to_fetch, recycled_frame.get())) {
                        DBG("readFrameAt failed");
                        return false;
                    }
                    break;  // we have the frame, so we can break out of the loop
                }
                DBG("We failed for some other reason");
                return false;
            }
            break;
        } while (false);

        uint32_t timestamp = mFrameInterpolator->get_exact_timestamp_ms(frame_to_fetch);
        recycled_frame->setFrameNumberAndTime(frame_to_fetch, timestamp);
        // DBG("inserting frame: " << frame_to_fetch);
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

FASTLED_NAMESPACE_END
