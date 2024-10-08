#include "fx/video/stream_buffered.h"

FASTLED_NAMESPACE_BEGIN

VideoStreamBuffered::VideoStreamBuffered(VideoStreamPtr stream, size_t nframes)
    : mStream(stream), mNFrames(nframes), mFrames(nframes) {
}

bool VideoStreamBuffered::draw(uint32_t now, Frame* frame) {
    if (!frame) {
        return false;
    }
    if (mFrames.empty()) {
        return false;
    }
    TimestampedFrame tf;
    bool needs_recycle = false;
    if (mFrames.full()) {
        needs_recycle = true;
        if (!mFrames.pop_front(&tf)) {
            return false;
        }
    } else {
        tf = mFrames.front();
        if (!tf.frame) {
            return false;
        }
    }
    frame->copy(*tf.frame);
    if (needs_recycle) {
        mFrames.push_back(tf);
    }
    return true;
}

int32_t VideoStreamBuffered::FramesRemaining() const {
    return mFrames.size() + mStream->FramesRemaining();
}


bool VideoStreamBuffered::Rewind() {
    if (!mStream || !mStream->Rewind()) {
        return false;
    }
    return true;
}

void VideoStreamBuffered::fillBuffer() {

}

FASTLED_NAMESPACE_END
