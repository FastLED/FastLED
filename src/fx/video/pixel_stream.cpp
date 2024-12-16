
#include "fx/video/pixel_stream.h"
#include "fl/namespace.h"
#include "fl/dbg.h"

#ifndef INT32_MAX
#define INT32_MAX 0x7fffffff
#endif

#define DBG FASTLED_DBG

using fl::FileHandlePtr;
using fl::ByteStreamPtr;

namespace fl {

PixelStream::PixelStream(int bytes_per_frame) : mbytesPerFrame(bytes_per_frame), mUsingByteStream(false) {
}

PixelStream::~PixelStream() {
    close();
}

bool PixelStream::begin(FileHandlePtr h) {
    close();
    mFileHandle = h;
    mUsingByteStream = false;
    return mFileHandle->available();
}

bool PixelStream::beginStream(ByteStreamPtr s) {
    close();
    mByteStream = s;
    mUsingByteStream = true;
    return mByteStream->available(mbytesPerFrame);
}

void PixelStream::close() {
    if (!mUsingByteStream && mFileHandle) {
        mFileHandle.reset();
    }
    mByteStream.reset();
    mFileHandle.reset();
}

int32_t PixelStream::bytesPerFrame() {
    return mbytesPerFrame;
}

bool PixelStream::readPixel(CRGB* dst) {
    if (mUsingByteStream) {
        return mByteStream->read(&dst->r, 1) && mByteStream->read(&dst->g, 1) && mByteStream->read(&dst->b, 1);
    } else {
        return mFileHandle->read(&dst->r, 1) && mFileHandle->read(&dst->g, 1) && mFileHandle->read(&dst->b, 1);
    }
}

bool PixelStream::available() const {
    if (mUsingByteStream) {
        return mByteStream->available(mbytesPerFrame);
    } else {
        return mFileHandle->available();
    }
}

bool PixelStream::atEnd() const {
    if (mUsingByteStream) {
        return false;
    } else {
        return !mFileHandle->available();
    }
}

bool PixelStream::readFrame(Frame* frame) {
    if (!frame) {
        return false;
    }
    if (!mUsingByteStream) {
        if (!framesRemaining()) {
            return false;
        }
        size_t n = mFileHandle->readCRGB(frame->rgb(), mbytesPerFrame / 3);
        DBG("pos: " << mFileHandle->pos());
        return n*3 == size_t(mbytesPerFrame);
    }
    size_t n = mByteStream->readCRGB(frame->rgb(), mbytesPerFrame / 3);
    return n*3 == size_t(mbytesPerFrame);
}

bool PixelStream::hasFrame(uint32_t frameNumber) {
    if (mUsingByteStream) {
        // ByteStream doesn't support seeking
        DBG("Not implemented and therefore always returns true");
        return true;
    } else {
        size_t total_bytes = mFileHandle->size();
        return frameNumber * mbytesPerFrame < total_bytes;
    }
}

bool PixelStream::readFrameAt(uint32_t frameNumber, Frame* frame) {
    // DBG("read frame at " << frameNumber);
    if (mUsingByteStream) {
        // ByteStream doesn't support seeking
        FASTLED_DBG("ByteStream doesn't support seeking");
        return false;
    } else {
        // DBG("mbytesPerFrame: " << mbytesPerFrame);
        mFileHandle->seek(frameNumber * mbytesPerFrame);
        if (mFileHandle->bytesLeft() == 0) {
            return false;
        }
        size_t read = mFileHandle->readCRGB(frame->rgb(), mbytesPerFrame / 3) * 3;
        // DBG("read: " << read);
        // DBG("pos: " << mFileHandle->Position());

        bool ok = int(read) == mbytesPerFrame;
        if (!ok) {
            DBG("readFrameAt failed - read: " << read << ", mbytesPerFrame: " << mbytesPerFrame << ", frame:" << frameNumber << ", left: " << mFileHandle->bytesLeft());
        }
        return ok;
    }
}

int32_t PixelStream::framesRemaining() const {
    if (mbytesPerFrame == 0) return 0;
    int32_t bytes_left = bytesRemaining();
    if (bytes_left <= 0) {
        return 0;
    }
    return bytes_left / mbytesPerFrame;
}

int32_t PixelStream::framesDisplayed() const {
    if (mUsingByteStream) {
        // ByteStream doesn't have a concept of total size, so we can't calculate this
        return -1;
    } else {
        int32_t bytes_played = mFileHandle->pos();
        return bytes_played / mbytesPerFrame;
    }
}

int32_t PixelStream::bytesRemaining() const {
    if (mUsingByteStream) {
        return INT32_MAX;
    } else {
        return mFileHandle->bytesLeft();
    }
}

int32_t PixelStream::bytesRemainingInFrame() const {
    return bytesRemaining() % mbytesPerFrame;
}

bool PixelStream::rewind() {
    if (mUsingByteStream) {
        // ByteStream doesn't support rewinding
        return false;
    } else {
        mFileHandle->seek(0);
        return true;
    }
}

PixelStream::Type PixelStream::getType() const {
    return mUsingByteStream ? Type::kStreaming : Type::kFile;
}

size_t PixelStream::readBytes(uint8_t* dst, size_t len) {
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
        while (bytesRead < len && mFileHandle->available()) {
            if (mFileHandle->read(dst + bytesRead, 1)) {
                bytesRead++;
            } else {
                break;
            }
        }
    }
    return bytesRead;
}

}  // namespace fl
