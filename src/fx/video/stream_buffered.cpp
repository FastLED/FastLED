#include "fx/video/stream_buffered.h"

FASTLED_NAMESPACE_BEGIN

VideoStreamBuffered::VideoStreamBuffered(VideoStreamPtr stream, size_t nFramesInBuffer, float fpsVideo)
    : mStream(stream),
      mNFrames(nFramesInBuffer),
      mInterpolator(FrameInterpolatorPtr::New(nFramesInBuffer)) {
    mMicrosSecondsPerFrame = static_cast<uint64_t>(1000000.0f / fpsVideo);
}

void VideoStreamBuffered::begin(uint32_t now) {
    mInterpolator->clear();
    mFrameCounter = 0;
    mStartTime = now;
}

bool VideoStreamBuffered::draw(uint32_t now, Frame* frame) {
    fillBufferIfNecessary(now);
    if (!frame) {
        return false;
    }
    return mInterpolator->draw(now, frame);
}

void VideoStreamBuffered::fillBufferIfNecessary(uint32_t now) {
    // get the number of frames according to the time elapsed
    uint32_t elapsed = now - mStartTime;
    uint32_t elapsedMicros = elapsed * 1000;
    uint32_t frameNumber = elapsedMicros / mMicrosSecondsPerFrame;
    if (frameNumber > mFrameCounter) {
        // if we dropped frames (because of time manipulation) just set
        // the frame counter to the current frame number + 1
        // read the frame from the stream
        FramePtr frame;
        mInterpolator->pop_back(&frame);
        if (mStream->readFrame(frame.get())) {
            uint32_t frametime = (1+mFrameCounter) * mMicrosSecondsPerFrame;
            if (mInterpolator->push_front(frame, frametime)) {
                // we have a new frame
                mFrameCounter++;
            }
        } else {
            // Something went wrong so put the frame back in the buffer.
            mInterpolator->push_front(frame, frame->getTimestamp());
        }
    }
}

bool VideoStreamBuffered::Rewind() {
    if (!mStream || !mStream->Rewind()) {
        return false;
    }
    mInterpolator->clear();
    return true;
}

FASTLED_NAMESPACE_END
