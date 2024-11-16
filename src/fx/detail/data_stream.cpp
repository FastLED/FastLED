
#include "fx/detail/data_stream.h"
#include "namespace.h"
#include "fx/storage/filebuffer.h"

#ifndef INT32_MAX
#define INT32_MAX 0x7fffffff
#endif



FASTLED_NAMESPACE_BEGIN

DataStream::DataStream(int bytes_per_frame) : mbytesPerFrame(bytes_per_frame), mUsingByteStream(false) {
}

DataStream::~DataStream() {
    close();
}

bool DataStream::begin(FileHandleRef h) {
    close();
    mFileHandle = h;
    mFileBuffer = FileBufferRef::New(h);
    mUsingByteStream = false;
    return mFileBuffer->available();
}

bool DataStream::beginStream(ByteStreamRef s) {
    close();
    mByteStream = s;
    mUsingByteStream = true;
    return mByteStream->available(mbytesPerFrame);
}

void DataStream::close() {
    if (!mUsingByteStream && mFileBuffer) {
        mFileBuffer.reset();
    }
    mByteStream.reset();
    mFileHandle.reset();
}

int32_t DataStream::bytesPerFrame() {
    return mbytesPerFrame;
}

bool DataStream::readPixel(CRGB* dst) {
    if (mUsingByteStream) {
        return mByteStream->read(&dst->r, 1) && mByteStream->read(&dst->g, 1) && mByteStream->read(&dst->b, 1);
    } else {
        return mFileBuffer->read(&dst->r, 1) && mFileBuffer->read(&dst->g, 1) && mFileBuffer->read(&dst->b, 1);
    }
}

bool DataStream::available() const {
    if (mUsingByteStream) {
        return mByteStream->available(mbytesPerFrame);
    } else {
        return mFileBuffer->available();
    }
}

bool DataStream::atEnd() const {
    if (mUsingByteStream) {
        return false;
    } else {
        return !mFileBuffer->available();
    }
}

bool DataStream::readFrame(Frame* frame) {
    // returns true if a frame was read.
    if (!framesRemaining() || !frame) {
        return false;
    }
    if (mUsingByteStream) {
        mByteStream->read(frame->rgb(), mbytesPerFrame);
    } else {
        mFileBuffer->read(frame->rgb(), mbytesPerFrame);
    }
    return true;
}

int32_t DataStream::framesRemaining() const {
    if (mbytesPerFrame == 0) return 0;
    int32_t bytes_left = bytesRemaining();
    if (bytes_left <= 0) {
        return 0;
    }
    return bytes_left / mbytesPerFrame;
}

int32_t DataStream::framesDisplayed() const {
    if (mUsingByteStream) {
        // ByteStream doesn't have a concept of total size, so we can't calculate this
        return -1;
    } else {
        int32_t bytes_played = mFileBuffer->FileSize() - mFileBuffer->BytesLeft();
        return bytes_played / mbytesPerFrame;
    }
}

int32_t DataStream::bytesRemaining() const {
    if (mUsingByteStream) {
        return INT32_MAX;
    } else {
        return mFileBuffer->BytesLeft();
    }
}

int32_t DataStream::bytesRemainingInFrame() const {
    return bytesRemaining() % mbytesPerFrame;
}

bool DataStream::rewind() {
    if (mUsingByteStream) {
        // ByteStream doesn't support rewinding
        return false;
    } else {
        mFileBuffer->rewindToStart();
        return true;
    }
}

DataStream::Type DataStream::getType() const {
    return mUsingByteStream ? Type::kStreaming : Type::kFile;
}

size_t DataStream::readBytes(uint8_t* dst, size_t len) {
    uint16_t bytesRead = 0;
    if (mUsingByteStream) {
        while (bytesRead < len && mByteStream->available(len)) {
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
