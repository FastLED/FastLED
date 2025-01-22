

#include "video_impl.h"

#include "fl/assert.h"
#include "fl/math_macros.h"
#include "fl/namespace.h"
#include "fl/warn.h"

using namespace fl;

namespace fl {

VideoImpl::VideoImpl(size_t pixelsPerFrame, float fpsVideo,
                     size_t nFramesInBuffer)
    : mPixelsPerFrame(pixelsPerFrame),
      mFrameInterpolator(
          FrameInterpolatorPtr::New(MAX(1, nFramesInBuffer), fpsVideo)) {}

void VideoImpl::pause(uint32_t now) {
    if (!mTime) {
        mTime = TimeScalePtr::New(now);
    }
    mTime->pause(now);
}
void VideoImpl::resume(uint32_t now) {
    if (!mTime) {
        mTime = TimeScalePtr::New(now);
    }
    mTime->resume(now);
}

void VideoImpl::setTimeScale(float timeScale) {
    mTimeScale = timeScale;
    if (mTime) {
        mTime->setScale(timeScale);
    }
}

void VideoImpl::setFade(uint32_t fadeInTime, uint32_t fadeOutTime) {
    mFadeInTime = fadeInTime;
    mFadeOutTime = fadeOutTime;
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
    return draw(now, frame->rgb());
}

int32_t VideoImpl::durationMicros() const {
    if (!mStream) {
        return -1;
    }
    int32_t frames = mStream->framesRemaining();
    if (frames < 0) {
        return -1;  // Stream case, duration unknown
    }
    uint32_t micros_per_frame = mFrameInterpolator->getFrameTracker().microsecondsPerFrame();
    return (frames * micros_per_frame);  // Convert to milliseconds
}

bool VideoImpl::draw(uint32_t now, CRGB *leds) {
    if (!mTime) {
        mTime = TimeScalePtr::New(now);
        mTime->setScale(mTimeScale);
        mTime->reset(now);
    }
    now = mTime->update(now);
    if (!mStream) {
        FASTLED_WARN("no stream");
        return false;
    }
    bool ok = updateBufferIfNecessary(mPrevNow, now);
    mPrevNow = now;
    if (!ok) {
        FASTLED_WARN("updateBufferIfNecessary failed");
        return false;
    }
    mFrameInterpolator->draw(now, leds);

    uint32_t time = mTime->time();
    uint32_t brightness = 255;
    // Compute fade in/out brightness.
    if (mFadeInTime || mFadeOutTime) {
        brightness = 255;
        if (time <= mFadeInTime) {
            if (mFadeInTime == 0) {
                brightness = 255;
            } else {
                brightness = time * 255 / mFadeInTime;
            }
        } else if (mFadeOutTime) {
            int32_t frames_remaining = mStream->framesRemaining();
            if (frames_remaining < 0) {
                // -1 means this is a stream.
                brightness = 255;
            } else {
                FrameTracker &frame_tracker =
                    mFrameInterpolator->getFrameTracker();
                uint32_t micros_per_frame =
                    frame_tracker.microsecondsPerFrame();
                uint32_t millis_left =
                    (frames_remaining * micros_per_frame) / 1000;
                if (millis_left < mFadeOutTime) {
                    brightness = millis_left * 255 / mFadeOutTime;
                }
            }
        }
    }
    if (brightness < 255) {
        if (brightness == 0) {
            for (size_t i = 0; i < mPixelsPerFrame; ++i) {
                leds[i] = CRGB::Black;
            }
        } else {
            for (size_t i = 0; i < mPixelsPerFrame; ++i) {
                leds[i].nscale8(brightness);
            }
        }
    }
    return true;
}

bool VideoImpl::updateBufferFromStream(uint32_t now) {
    FASTLED_ASSERT(mTime, "mTime is null");
    if (!mStream) {
        FASTLED_WARN("no stream");
        return false;
    }
    if (mStream->atEnd()) {
        return false;
    }

    uint32_t currFrameNumber = 0;
    uint32_t nextFrameNumber = 0;
    bool needs_frame =
        mFrameInterpolator->needsFrame(now, &currFrameNumber, &nextFrameNumber);
    if (!needs_frame) {
        return true;
    }

    if (mFrameInterpolator->capacity() == 0) {
        FASTLED_WARN("capacity == 0");
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
            bool ok =
                mFrameInterpolator->get_oldest_frame_number(&frame_to_erase);
            if (!ok) {
                FASTLED_WARN("get_oldest_frame_number failed");
                return false;
            }
            recycled_frame = mFrameInterpolator->erase(frame_to_erase);
            if (!recycled_frame) {
                FASTLED_WARN("erase failed for frame: " << frame_to_erase);
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
                    FASTLED_WARN("rewind failed");
                    return false;
                }
                mTime->reset(now);
                frame_to_fetch = 0;
                if (!mStream->readFrameAt(frame_to_fetch,
                                          recycled_frame.get())) {
                    FASTLED_WARN("readFrameAt failed");
                    return false;
                }
            } else {
                FASTLED_WARN("We failed for some other reason");
                return false;
            }
        }
        bool ok = mFrameInterpolator->insert(frame_to_fetch, recycled_frame);
        if (!ok) {
            FASTLED_WARN("insert failed");
            return false;
        }
    }
    return true;
}

bool VideoImpl::updateBufferFromFile(uint32_t now, bool forward) {
    uint32_t currFrameNumber = 0;
    uint32_t nextFrameNumber = 0;
    bool needs_frame =
        mFrameInterpolator->needsFrame(now, &currFrameNumber, &nextFrameNumber);
    if (!needs_frame) {
        return true;
    }
    bool has_curr_frame = mFrameInterpolator->has(currFrameNumber);
    bool has_next_frame = mFrameInterpolator->has(nextFrameNumber);
    if (has_curr_frame && has_next_frame) {
        return true;
    }
    if (mFrameInterpolator->capacity() == 0) {
        FASTLED_WARN("capacity == 0");
        return false;
    }

    fl::FixedVector<uint32_t, 2> frame_numbers;
    if (!mFrameInterpolator->has(currFrameNumber)) {
        frame_numbers.push_back(currFrameNumber);
    }
    if (mFrameInterpolator->capacity() > 1 &&
        !mFrameInterpolator->has(nextFrameNumber)) {
        frame_numbers.push_back(nextFrameNumber);
    }

    for (size_t i = 0; i < frame_numbers.size(); ++i) {
        FramePtr recycled_frame;
        if (mFrameInterpolator->full()) {
            uint32_t frame_to_erase = 0;
            bool ok = false;
            if (forward) {
                ok = mFrameInterpolator->get_oldest_frame_number(
                    &frame_to_erase);
                if (!ok) {
                    FASTLED_WARN("get_oldest_frame_number failed");
                    return false;
                }
            } else {
                ok = mFrameInterpolator->get_newest_frame_number(
                    &frame_to_erase);
                if (!ok) {
                    FASTLED_WARN("get_newest_frame_number failed");
                    return false;
                }
            }
            recycled_frame = mFrameInterpolator->erase(frame_to_erase);
            if (!recycled_frame) {
                FASTLED_WARN("erase failed for frame: " << frame_to_erase);
                return false;
            }
        }
        uint32_t frame_to_fetch = frame_numbers[i];
        if (!recycled_frame) {
            // Happens when we are not full and we need to allocate a new frame.
            recycled_frame = FramePtr::New(mPixelsPerFrame);
        }

        do { // only to use break
            if (!mStream->readFrameAt(frame_to_fetch, recycled_frame.get())) {
                if (!forward) {
                    // nothing more we can do, we can't go negative.
                    return false;
                }
                if (mStream->atEnd()) {
                    if (!mStream->rewind()) { // Is this still
                        FASTLED_WARN("rewind failed");
                        return false;
                    }
                    mTime->reset(now);
                    frame_to_fetch = 0;
                    if (!mStream->readFrameAt(frame_to_fetch,
                                              recycled_frame.get())) {
                        FASTLED_WARN("readFrameAt failed");
                        return false;
                    }
                    break; // we have the frame, so we can break out of the loop
                }
                FASTLED_WARN("We failed for some other reason");
                return false;
            }
            break;
        } while (false);

        bool ok = mFrameInterpolator->insert(frame_to_fetch, recycled_frame);
        if (!ok) {
            FASTLED_WARN("insert failed");
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
        FASTLED_WARN("Unknown type: " << uint32_t(type));
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

} // namespace fl
