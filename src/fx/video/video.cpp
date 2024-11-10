#include "fx/video.h"

#include "fx/video/frame_interpolator.h"
#include "fx/detail/data_stream.h"
#include "fx/frame.h"
#include "crgb.h"

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

FASTLED_SMART_REF(DataStream);
FASTLED_SMART_REF(FrameInterpolator);
FASTLED_SMART_REF(Frame);


class VideoImpl : public Referent {
public:
    // frameHistoryCount is the number of frames to keep in the buffer after draw. This
    // allows for time based effects like syncing video speed to audio triggers.
    VideoImpl(size_t pixelsPerFrame, float fpsVideo, size_t frameHistoryCount = 0);
    ~VideoImpl();
    // Api
    void begin(FileHandleRef h);
    void beginStream(ByteStreamRef s);
    bool draw(uint32_t now, CRGB* leds, uint8_t* alpha = nullptr);
    void end();
    bool rewind();
    // internal use
    bool draw(uint32_t now, Frame* frame);
    bool full() const;
    FrameRef popOldest();
    void pushNewest(FrameRef frame);

private:
    bool updateBufferIfNecessary(uint32_t now);
    uint32_t mPixelsPerFrame = 0;
    DataStreamRef mStream;
    FrameInterpolatorRef mInterpolator;
};

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
    bool ok = updateBufferIfNecessary(now);
    if (!ok) {
        // paint black
        memset(leds, 0, mPixelsPerFrame * sizeof(CRGB));
        if (alpha) {
            memset(alpha, 0, mPixelsPerFrame);
        }
        return false;
    }
    mInterpolator->draw(now, leds, alpha);
    return true;
}

bool VideoImpl::updateBufferIfNecessary(uint32_t now) {
    // get the number of frames according to the time elapsed
    uint32_t precise_timestamp;
    // At most, update one frame. That way if the user forgets to call draw and
    // then sends a really old timestamp, we don't update the buffer too much.
    bool needs_frame = mInterpolator->needsFrame(now, &precise_timestamp);
    DBG(cout << "needs_frame: " << needs_frame << endl);
    if (!needs_frame) {
        return true;
    }
    // if we dropped frames (because of time manipulation) just set
    // the frame counter to the current frame number + 1
    // read the frame from the stream
    FrameRef frame;
    if (mInterpolator->full()) {
        DBG(cout << "popOldest" << endl);
        if (!mInterpolator->popOldest(&frame)) {
            DBG(cout << "popOldest failed" << endl);
            return false;
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
        return true;
    } else {
        DBG(cout << "readFrame failed" << endl);
        // Something went wrong so put the frame back in the buffer.
        mInterpolator->push_front(frame, frame->getTimestamp());
        return false;
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
Video::Video(const Video&) = default;

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
    bool ok = mImpl->draw(now, leds, alpha);
    if (!ok) {
        // Interpret not being able to draw as a finished signal.
        mFinished = true;
    }
    return ok;
}

void Video::end() {
    if (mImpl) {
        mImpl->end();
    }
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

FASTLED_NAMESPACE_END
