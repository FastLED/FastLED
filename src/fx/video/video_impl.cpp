


#include "video_impl.h"

#include "fl/dbg.h"
#include "fl/math_macros.h"
#include "fl/namespace.h"


#define DBG FASTLED_DBG

using namespace fl;

namespace fl {

VideoImpl::VideoImpl(size_t pixelsPerFrame, float fpsVideo,
                     size_t nFramesInBuffer)
    : mPixelsPerFrame(pixelsPerFrame),
      mFrameInterpolator(
          FrameInterpolatorPtr::New(MAX(1, nFramesInBuffer), fpsVideo)) {}

void VideoImpl::pause(uint32_t now) {
    if (!mTimeScale) {
        mTimeScale = TimeScalePtr::New(now);
    }
    mTimeScale->pause(now);
}
void VideoImpl::resume(uint32_t now) {
    if (!mTimeScale) {
        mTimeScale = TimeScalePtr::New(now);
    }
    mTimeScale->resume(now);
}

void VideoImpl::setTimeScale(float timeScale) {
    if (!mTimeScale) {
        mTimeScale = TimeScalePtr::New(0, timeScale);
    }
    mTimeScale->setScale(timeScale);
}

bool VideoImpl::needsFrame(uint32_t now) const {
    uint32_t f1, f2;
    bool out = mFrameInterpolator->needsFrame(now, &f1, &f2);
    return out;
}

VideoImpl::~VideoImpl() { end(); }

void VideoImpl::begin(FileHandlePtr h) {
    end();
    // Removed setStartTime call
    mStream = PixelStreamPtr::New(mPixelsPerFrame * kSizeRGB8);
    mStream->begin(h);
    mPrevNow = 0;
}

void VideoImpl::beginStream(ByteStreamPtr bs) {
    end();
    mStream = PixelStreamPtr::New(mPixelsPerFrame * kSizeRGB8);
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

bool VideoImpl::draw(uint32_t now, CRGB *leds) {
    //DBG("draw with now = " << now);
    if (!mTimeScale) {
        mTimeScale = TimeScalePtr::New(now);
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
        return false;
    }
    mFrameInterpolator->draw(now, leds);
    return true;
}


bool VideoImpl::updateBufferFromStream(uint32_t now) {
    if (!mStream) {
        FASTLED_DBG("no stream");
        return false;
    }
    if (mStream->atEnd()) {
        return false;
    }

    uint32_t currFrameNumber = 0;
    uint32_t nextFrameNumber = 0;
    bool needs_frame = mFrameInterpolator->needsFrame(now, &currFrameNumber, &nextFrameNumber);
    if (!needs_frame) {
        return true;
    }

    if (mFrameInterpolator->capacity() == 0) {
        DBG("capacity == 0");
        return false;
    }

    const bool has_current_frame = mFrameInterpolator->has(currFrameNumber);
    const bool has_next_frame = mFrameInterpolator->has(nextFrameNumber);

    fl::FixedVector<uint32_t, 2> frame_numbers;
    if (!has_current_frame) {
        frame_numbers.push_back(currFrameNumber);
    }
    size_t capacity = mFrameInterpolator->capacity();
    if (capacity > 1 && !has_next_frame) {
        frame_numbers.push_back(nextFrameNumber);
    }

    for (size_t i = 0; i < frame_numbers.size(); ++i) {
        FramePtr recycled_frame;
        if (mFrameInterpolator->full()) {
            uint32_t frame_to_erase = 0;
            bool ok = mFrameInterpolator->get_oldest_frame_number(&frame_to_erase);
            if (!ok) {
                DBG("get_oldest_frame_number failed");
                return false;
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
            recycled_frame = FramePtr::New(mPixelsPerFrame);
        }

        if (!mStream->readFrame(recycled_frame.get())) {
            if (mStream->atEnd()) {
                if (!mStream->rewind()) {
                    DBG("rewind failed");
                    return false;
                }
                mTimeScale->reset(now);
                frame_to_fetch = 0;
                if (!mStream->readFrameAt(frame_to_fetch, recycled_frame.get())) {
                    DBG("readFrameAt failed");
                    return false;
                }
            } else {
                DBG("We failed for some other reason");
                return false;
            }
        }
        bool ok = mFrameInterpolator->insert(frame_to_fetch, recycled_frame);
        if (!ok) {
            DBG("insert failed");
            return false;
        }
    }
    return true;
}

bool VideoImpl::updateBufferFromFile(uint32_t now, bool forward) {
    uint32_t currFrameNumber = 0;
    uint32_t nextFrameNumber = 0;
    bool needs_frame = mFrameInterpolator->needsFrame(now, &currFrameNumber, &nextFrameNumber);
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

    fl::FixedVector<uint32_t, 2> frame_numbers;
    if (!mFrameInterpolator->has(currFrameNumber)) {
        frame_numbers.push_back(currFrameNumber);
    }
    if (mFrameInterpolator->capacity() > 1 && !mFrameInterpolator->has(nextFrameNumber)) {
        frame_numbers.push_back(nextFrameNumber);
    }

    for (size_t i = 0; i < frame_numbers.size(); ++i) {
        FramePtr recycled_frame;
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
            recycled_frame = FramePtr::New(mPixelsPerFrame);
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
        
        bool ok = mFrameInterpolator->insert(frame_to_fetch, recycled_frame);
        if (!ok) {
            DBG("insert failed");
            return false;
        }
    }
    return true;
}

bool VideoImpl::updateBufferIfNecessary(uint32_t prev, uint32_t now) {
    const bool forward = now >= prev;

    PixelStream::Type type = mStream->getType();
    switch (type) {
        case PixelStream::kFile:
            return updateBufferFromFile(now, forward);
        case PixelStream::kStreaming:
            return updateBufferFromStream(now);
        default:
            DBG("Unknown type: " << type);
            return false;
    }
}

bool VideoImpl::rewind() {
    if (!mStream || !mStream->rewind()) {
        return false;
    }
    mFrameInterpolator->clear();
    return true;
}

}  // namespace fl
