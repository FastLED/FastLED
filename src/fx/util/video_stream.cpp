
#include "video_stream.h"
#include "namespace.h"
#include "fx/storage/filebuffer.h"


FASTLED_NAMESPACE_BEGIN

VideoStream::VideoStream(int bytes_per_frame) : mBytesPerFrame(bytes_per_frame) {
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

int32_t VideoStream::BytesPerFrame() {
    return mBytesPerFrame;
}

bool VideoStream::ReadPixel(CRGB* dst) {
    return mFileBuffer->Read(&dst->r) && mFileBuffer->Read(&dst->g) && mFileBuffer->Read(&dst->b);
}

int32_t VideoStream::FramesRemaining() const {
    if (mBytesPerFrame == 0) return 0;
    int32_t bytes_left = mFileBuffer->BytesLeft();
    return (bytes_left > 0) ? (bytes_left / mBytesPerFrame) : 0;
}

int32_t VideoStream::FramesDisplayed() const {
    int32_t bytes_played = mFileBuffer->FileSize() - mFileBuffer->BytesLeft();
    return bytes_played / mBytesPerFrame;
}

int32_t VideoStream::BytesRemaining() const {
    return mFileBuffer->BytesLeft();
}

int32_t VideoStream::BytesRemainingInFrame() const {
    return BytesRemaining() % mBytesPerFrame;
}

void VideoStream::Rewind() {
    mFileBuffer->RewindToStart();
}

uint16_t VideoStream::ReadBytes(uint8_t* dst, uint16_t len) {
    uint16_t bytesRead = 0;
    while (bytesRead < len && mFileBuffer->available()) {
        if (mFileBuffer->Read(dst + bytesRead)) {
            bytesRead++;
        } else {
            break;
        }
    }
    return bytesRead;
}

FASTLED_NAMESPACE_END
