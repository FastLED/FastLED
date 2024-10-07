
#include "video_stream.h"
#include "namespace.h"
#include "fx/storage/filebuffer.h"


FASTLED_NAMESPACE_BEGIN

VideoStream::VideoStream(int pixels_per_frame) : mPixelsPerFrame(pixels_per_frame) {
}

VideoStream::~VideoStream() {
}

bool VideoStream::begin(FileHandlePtr h) {
    mFileHandle = h;
    mFileBuffer = RefPtr<FileBuffer>::FromHeap(new FileBuffer(h));
    return mFileBuffer->available();
}

void VideoStream::Close() {
    mFileBuffer->close();
    mFileBuffer.reset();
}

int32_t VideoStream::PixelsPerFrame() {
    return mPixelsPerFrame;
}

bool VideoStream::ReadPixel(CRGB* dst) {
    return mFileBuffer->Read(&dst->r) && mFileBuffer->Read(&dst->g) && mFileBuffer->Read(&dst->b);
}

int32_t VideoStream::FramesRemaining() const {
    if (mPixelsPerFrame == 0) return 0;
    int32_t pixels_left = mFileBuffer->BytesLeft() / 3;
    return (pixels_left > 0) ? (pixels_left / mPixelsPerFrame) : 0;
}

int32_t VideoStream::FramesDisplayed() const {
    int32_t bytes_played = mFileBuffer->FileSize() - mFileBuffer->BytesLeft();
    int32_t pixels_played = bytes_played / 3;
    return pixels_played / mPixelsPerFrame;
}

int32_t VideoStream::BytesRemaining() const {
    return mFileBuffer->BytesLeft();
}

int32_t VideoStream::PixelsRemainingInFrame() const {
    return (BytesRemaining() / 3) % mPixelsPerFrame;
}

void VideoStream::Rewind() {
    mFileBuffer->RewindToStart();
}

FASTLED_NAMESPACE_END
