#include "fx/video.h"

#ifdef __EMSCRIPTEN__
#define DEBUG_IO_STREAM 1
#else
#define DEBUG_IO_STREAM 0
#endif

#if DEBUG_IO_STREAM
#include <iostream> // ok include
using namespace std;
#define DBG(X) (X)
#else
#define DBG(X)
#endif

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

VideoImpl::VideoImpl(size_t pixelsPerFrame, float fpsVideo, size_t nFramesInBuffer)
    : mPixelsPerFrame(pixelsPerFrame),
      mInterpolator(
          FrameInterpolatorRef::New(MAX(1, nFramesInBuffer), fpsVideo)) {}

VideoImpl::~VideoImpl() { end(); }

void VideoImpl::begin(FileHandleRef h) {
    end();
    // Removed setStartTime call
    mStream = DataStreamRef::New(mPixelsPerFrame);
    mStream->begin(h);
}

void VideoImpl::beginStream(ByteStreamRef bs) {
    end();
    mStream = DataStreamRef::New(mPixelsPerFrame);
    // Removed setStartTime call
    mStream->beginStream(bs);
}

void VideoImpl::end() {
    mInterpolator->clear();
    // Removed resetFrameCounter and setStartTime calls
    mStream.reset();
}

void VideoImpl::pushNewest(FrameRef frame) {
    mInterpolator->push_front(frame, frame->getTimestamp());
}

bool VideoImpl::full() const {
    return mInterpolator->getFrames()->full();
}

FrameRef VideoImpl::popOldest() {
    FrameRef frame;
    mInterpolator->pop_back(&frame);
    return frame;
}

bool VideoImpl::draw(uint32_t now, Frame *frame) {
    if (!mStream) {
        return false;
    }
    updateBufferIfNecessary(now);
    if (!frame) {
        return false;
    }
    return mInterpolator->draw(now, frame);
}

bool VideoImpl::draw(uint32_t now, CRGB *leds, uint8_t *alpha) {
    if (!mStream) {
        return false;
    }
    updateBufferIfNecessary(now);
    mInterpolator->draw(now, leds, alpha);
    return true;
}

void VideoImpl::updateBufferIfNecessary(uint32_t now) {
    // get the number of frames according to the time elapsed
    uint32_t precise_timestamp;
    // At most, update one frame. That way if the user forgets to call draw and
    // then sends a really old timestamp, we don't update the buffer too much.
    bool needs_frame = mInterpolator->needsFrame(now, &precise_timestamp);
    DBG(cout << "needs_frame: " << needs_frame << endl);
    if (!needs_frame) {
        return;
    }
    // if we dropped frames (because of time manipulation) just set
    // the frame counter to the current frame number + 1
    // read the frame from the stream
    FrameRef frame;
    if (mInterpolator->full()) {
        DBG(cout << "popOldest" << endl);
        if (!mInterpolator->popOldest(&frame)) {
            DBG(cout << "popOldest failed" << endl);
            return; // Something went wrong
        }
    } else {
        DBG(cout << "New Frame" << endl);
        frame = FrameRef::New(mPixelsPerFrame, false);
    }
    if (mStream->readFrame(frame.get())) {
        DBG(cout << "readFrame successful" << endl);
        if (mInterpolator->pushNewest(frame, now)) {
            // we have a new frame
            mInterpolator->incrementFrameCounter();
        }
    } else {
        DBG(cout << "readFrame failed" << endl);
        // Something went wrong so put the frame back in the buffer.
        mInterpolator->push_front(frame, frame->getTimestamp());
    }
}

bool VideoImpl::rewind() {
    if (!mStream || !mStream->rewind()) {
        return false;
    }
    mInterpolator->clear();
    return true;
}

Video::Video() = default;
Video::~Video() = default;

void Video::begin(FileHandleRef h, size_t pixelsPerFrame, float fps, size_t frameHistoryCount) {
    mImpl.reset();
    mImpl = VideoImplRef::New(pixelsPerFrame, fps, frameHistoryCount);
    mImpl->begin(h);
}

void Video::beginStream(ByteStreamRef bs, size_t pixelsPerFrame, float fps, size_t frameHistoryCount) {
    mImpl.reset();
    mImpl = VideoImplRef::New(pixelsPerFrame, fps, frameHistoryCount);
    mImpl->beginStream(bs);
}

bool Video::draw(uint32_t now, CRGB *leds, uint8_t *alpha) {
    if (!mImpl) {
        return false;
    }
    return mImpl->draw(now, leds, alpha);
}

void Video::end() {
    if (mImpl) {
        mImpl->end();
    }
}

bool Video::rewind() {
    if (!mImpl) {
        return false;
    }
    return mImpl->rewind();
}

FASTLED_NAMESPACE_END
