#include "fx/video/stream_buffered.h"

FASTLED_NAMESPACE_BEGIN

VideoStream::VideoStream(size_t pixelsPerFrame, size_t nFramesInBuffer, float fpsVideo)
    : mPixelsPerFrame(pixelsPerFrame),
      mInterpolator(FrameInterpolatorPtr::New(nFramesInBuffer)) {
    mMicrosSecondsPerFrame = static_cast<uint64_t>(1000000.0f / fpsVideo);
}

void VideoStream::begin(uint32_t now, FileHandlePtr h) {
    end();
    mStartTime = now;
    mStream = DataStreamPtr::New(mPixelsPerFrame);
    mStartTime = now;
    mStream->begin(h);
}

void VideoStream::beginStream(uint32_t now, ByteStreamPtr bs) {
    end();
    mStream = DataStreamPtr::New(mPixelsPerFrame);
    mStartTime = now;
    mStream->beginStream(bs);
}

void VideoStream::end() {
    mInterpolator->clear();
    mFrameCounter = 0;
    mStartTime = 0;
    mStream.reset();
}

bool VideoStream::draw(uint32_t now, Frame* frame) {
    if (!mStream) {
        return false;
    }
    updateBufferIfNecessary(now);
    if (!frame) {
        return false;
    }
    return mInterpolator->draw(now, frame);
}

void VideoStream::updateBufferIfNecessary(uint32_t now) {
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

bool VideoStream::Rewind() {
    if (!mStream || !mStream->Rewind()) {
        return false;
    }
    mInterpolator->clear();
    return true;
}

FASTLED_NAMESPACE_END
