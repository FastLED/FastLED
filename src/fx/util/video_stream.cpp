
#include "video_stream.h"
#include "namespace.h"
#include "fx/storage/filebuffer.h"
#include <limits>


FASTLED_NAMESPACE_BEGIN

VideoStream::VideoStream(int bytes_per_frame) : mBytesPerFrame(bytes_per_frame), mUsingByteStream(false) {
}

VideoStream::~VideoStream() {
    Close();
}

bool VideoStream::begin(FileHandlePtr h) {
    Close();
    mFileHandle = h;
    mFileBuffer = RefPtr<FileBuffer>::FromHeap(new FileBuffer(h));
    mUsingByteStream = false;
    return mFileBuffer->available();
}

bool VideoStream::beginStream(ByteStreamPtr s) {
    Close();
    mByteStream = s;
    mUsingByteStream = true;
    return mByteStream->available();
}

void VideoStream::Close() {
    if (!mUsingByteStream && mFileBuffer) {
        mFileBuffer->close();
        mFileBuffer.reset();
    }
    mByteStream.reset();
    mFileHandle.reset();
}

int32_t VideoStream::BytesPerFrame() {
    return mBytesPerFrame;
}

bool VideoStream::ReadPixel(CRGB* dst) {
    if (mUsingByteStream) {
        return mByteStream->read(&dst->r, 1) && mByteStream->read(&dst->g, 1) && mByteStream->read(&dst->b, 1);
    } else {
        return mFileBuffer->read(&dst->r, 1) && mFileBuffer->read(&dst->g, 1) && mFileBuffer->read(&dst->b, 1);
    }
}

bool VideoStream::available() const {
    if (mUsingByteStream) {
        return mByteStream->available();
    } else {
        return mFileBuffer->available();
    }
}

int32_t VideoStream::FramesRemaining() const {
    if (mBytesPerFrame == 0) return 0;
    int32_t bytes_left = BytesRemaining();
    return (bytes_left > 0) ? (bytes_left / mBytesPerFrame) : 0;
}

int32_t VideoStream::FramesDisplayed() const {
    if (mUsingByteStream) {
        // ByteStream doesn't have a concept of total size, so we can't calculate this
        return -1;
    } else {
        int32_t bytes_played = mFileBuffer->FileSize() - mFileBuffer->BytesLeft();
        return bytes_played / mBytesPerFrame;
    }
}

int32_t VideoStream::BytesRemaining() const {
    if (mUsingByteStream) {
        return std::numeric_limits<int32_t>::max();
    } else {
        return mFileBuffer->BytesLeft();
    }
}

int32_t VideoStream::BytesRemainingInFrame() const {
    return BytesRemaining() % mBytesPerFrame;
}

bool VideoStream::Rewind() {
    if (mUsingByteStream) {
        // ByteStream doesn't support rewinding
        return false;
    } else {
        mFileBuffer->RewindToStart();
        return true;
    }
}

VideoStream::Type VideoStream::getType() const {
    return mUsingByteStream ? Type::kStreaming : Type::kFile;
}

size_t VideoStream::ReadBytes(uint8_t* dst, size_t len) {
    uint16_t bytesRead = 0;
    if (mUsingByteStream) {
        while (bytesRead < len && mByteStream->available()) {
            // use pop_front()
            if (mByteStream->read(dst + bytesRead, 1)) {
                bytesRead++;
            } else {
                break;
            }
        }
    } else {
        while (bytesRead < len && mFileBuffer->available()) {
            if (mFileBuffer->read(dst + bytesRead, 1)) {
                bytesRead++;
            } else {
                break;
            }
        }
    }
    return bytesRead;
}

FASTLED_NAMESPACE_END
