#include "fx/video/stream_buffered.h"

FASTLED_NAMESPACE_BEGIN

VideoStreamBuffered::VideoStreamBuffered(VideoStreamPtr stream, size_t nframes)
    : mStream(stream), mNFrames(nframes), mFrames(nframes) {
}

bool VideoStreamBuffered::draw(Frame* frame) {
    if (!frame) {
        return false;
    }
    if (mFrames.empty()) {
        return false;
    }
    FramePtr f;
    bool needs_recycle = false;
    if (mFrames.full()) {
        needs_recycle = true;
        if (!mFrames.pop_front(&f)) {
            return false;
        }
    } else {
        f = mFrames.front();
        if (!f) {
            return false;
        }
    }
    frame->copy(*f);
    if (needs_recycle) {
        mFrames.push_back(f);
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
