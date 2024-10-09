#include "fx/video/stream_buffered.h"

FASTLED_NAMESPACE_BEGIN

VideoStreamBuffered::VideoStreamBuffered(VideoStreamPtr stream, size_t nFramesInBuffer, float fpsVideo)
    : mStream(stream),
      mNFrames(nFramesInBuffer),
      mInterpolator(FrameInterpolatorPtr::New(nFramesInBuffer)),
      mLastDrawTime(0) {
    mMicrosSecondsPerFrame = static_cast<uint64_t>(1000000.0f / fpsVideo);
}

bool VideoStreamBuffered::draw(uint32_t now, Frame* frame) {
    if (!frame) {
        return false;
    }
    // TODO Implement the buffered video stream
    return true;
}

bool VideoStreamBuffered::Rewind() {
    if (!mStream || !mStream->Rewind()) {
        return false;
    }
    mInterpolator->clear();
    mLastDrawTime = 0;
    return true;
}

FASTLED_NAMESPACE_END
