
#include "fl/fx/video/pixel_stream.h"
#include "fl/dbg.h"
#include "fl/numeric_limits.h"

#define DBG FASTLED_DBG

namespace fl {

PixelStream::PixelStream(int bytes_per_frame)
    : mbytesPerFrame(bytes_per_frame), mType(kFile) {}

PixelStream::~PixelStream() { close(); }

bool PixelStream::begin(FileHandlePtr h) {
    close();
    mHandle = h;
    // Probe seekability: if seek-to-start succeeds, this is a seekable file.
    mType = mHandle->seek(0, seek_dir::beg) ? kFile : kStreaming;
    if (mType == kFile) {
        return mHandle->available();
    }
    return mHandle->available(mbytesPerFrame);
}

void PixelStream::close() {
    mHandle.reset();
}

i32 PixelStream::bytesPerFrame() { return mbytesPerFrame; }

bool PixelStream::readPixel(CRGB *dst) {
    return mHandle->read(&dst->r, 1) && mHandle->read(&dst->g, 1) &&
           mHandle->read(&dst->b, 1);
}

bool PixelStream::available() const {
    if (mType == kStreaming) {
        return mHandle->available(mbytesPerFrame);
    }
    return mHandle->available();
}

bool PixelStream::atEnd() const {
    if (mType == kStreaming) {
        return false;
    }
    return !mHandle->available();
}

bool PixelStream::readFrame(Frame *frame) {
    if (!frame) {
        return false;
    }
    if (mType == kFile && !framesRemaining()) {
        return false;
    }
    size_t n = mHandle->readRGB8(frame->rgb());
    if (mType == kFile) {
        DBG("pos: " << mHandle->pos());
    }
    return n * 3 == size_t(mbytesPerFrame);
}

bool PixelStream::hasFrame(fl::u32 frameNumber) {
    if (mType == kStreaming) {
        // Streaming handle doesn't support seeking
        DBG("Not implemented and therefore always returns true");
        return true;
    }
    size_t total_bytes = mHandle->size();
    return frameNumber * mbytesPerFrame < total_bytes;
}

bool PixelStream::readFrameAt(fl::u32 frameNumber, Frame *frame) {
    if (mType == kStreaming) {
        // Streaming handle doesn't support seeking
        FASTLED_DBG("Streaming handle doesn't support seeking");
        return false;
    }
    mHandle->seek(frameNumber * mbytesPerFrame);
    if (mHandle->bytesLeft() == 0) {
        return false;
    }
    size_t read =
        mHandle->readRGB8(frame->rgb()) * 3;

    bool ok = int(read) == mbytesPerFrame;
    if (!ok) {
        DBG("readFrameAt failed - read: "
            << read << ", mbytesPerFrame: " << mbytesPerFrame << ", frame:"
            << frameNumber << ", left: " << mHandle->bytesLeft());
    }
    return ok;
}

i32 PixelStream::framesRemaining() const {
    if (mbytesPerFrame == 0)
        return 0;
    i32 bytes_left = bytesRemaining();
    if (bytes_left <= 0) {
        return 0;
    }
    return bytes_left / mbytesPerFrame;
}

i32 PixelStream::framesDisplayed() const {
    if (mType == kStreaming) {
        return -1;
    }
    i32 bytes_played = mHandle->pos();
    return bytes_played / mbytesPerFrame;
}

i32 PixelStream::bytesRemaining() const {
    if (mType == kStreaming) {
        // Use (max)() to prevent macro expansion by Arduino.h's max macro
        return (fl::numeric_limits<i32>::max)();
    }
    return mHandle->bytesLeft();
}

i32 PixelStream::bytesRemainingInFrame() const {
    return bytesRemaining() % mbytesPerFrame;
}

bool PixelStream::rewind() {
    if (mType == kStreaming) {
        return false;
    }
    mHandle->seek(0);
    return true;
}

PixelStream::Type PixelStream::getType() const {
    return mType;
}

size_t PixelStream::readBytes(u8 *dst, size_t len) {
    u16 bytesRead = 0;
    if (mType == kStreaming) {
        while (bytesRead < len && mHandle->available(len)) {
            if (mHandle->read(dst + bytesRead, 1)) {
                bytesRead++;
            } else {
                break;
            }
        }
    } else {
        while (bytesRead < len && mHandle->available()) {
            if (mHandle->read(dst + bytesRead, 1)) {
                bytesRead++;
            } else {
                break;
            }
        }
    }
    return bytesRead;
}

} // namespace fl
