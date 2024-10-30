#include "fx/video/stream_buffered.h"

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

VideoStream::VideoStream(size_t pixelsPerFrame, size_t nFramesInBuffer, float fpsVideo)
    : mPixelsPerFrame(pixelsPerFrame),
      mInterpolator(FrameInterpolatorRef::New(nFramesInBuffer, fpsVideo)) {
}

void VideoStream::begin(uint32_t now, FileHandleRef h) {
    end();
    // Removed setStartTime call
    mStream = DataStreamRef::New(mPixelsPerFrame);
    mStream->begin(h);
}

void VideoStream::beginStream(uint32_t now, ByteStreamRef bs) {
    end();
    mStream = DataStreamRef::New(mPixelsPerFrame);
    // Removed setStartTime call
    mStream->beginStream(bs);
}

void VideoStream::end() {
    mInterpolator->clear();
    // Removed resetFrameCounter and setStartTime calls
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

bool VideoStream::draw(uint32_t now, CRGB* leds, uint8_t* alpha) {
    if (!mStream) {
        return false;
    }
    mInterpolator->draw(now, leds, alpha);
    return true;
}

void VideoStream::updateBufferIfNecessary(uint32_t now) {
    // get the number of frames according to the time elapsed
    uint32_t precise_timestamp;
    // At most, update one frame. That way if the user forgets to call draw and
    // then sends a really old timestamp, we don't update the buffer too much.
    bool needs_refresh = mInterpolator->needsRefresh(now, &precise_timestamp);
    if (!needs_refresh) {
        return;
    }
    // if we dropped frames (because of time manipulation) just set
    // the frame counter to the current frame number + 1
    // read the frame from the stream
    FrameRef frame;
    if (mInterpolator->full()) {
        if (!mInterpolator->popOldest(&frame)) {
            return;  // Something went wrong
        }
    } else {
        frame = FrameRef::New(mPixelsPerFrame, false);
    }
    if (mStream->readFrame(frame.get())) {
        if (mInterpolator->pushNewest(frame, now)) {
            // we have a new frame
            mInterpolator->incrementFrameCounter();
        }
    } else {
        // Something went wrong so put the frame back in the buffer.
        mInterpolator->push_front(frame, frame->getTimestamp());
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
