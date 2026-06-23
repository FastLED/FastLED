
#include "fl/video/pixel_stream.h"
#include "fl/log/log.h"
#include "fl/stl/limits.h"
#include "fl/stl/noexcept.h"

#define DBG FL_DBG

namespace fl {
namespace video {

// FLED v1 header layout (12 bytes). See
// https://github.com/zackees/ledmapper/blob/main/docs/fled-format.md
namespace {
constexpr fl::u8 kFledMagic[4] = {'F', 'L', 'E', 'D'};
constexpr fl::u8 kFledVersionV1 = 1;
constexpr fl::u8 kFledPixelFormatRgb8 = 0x00;
constexpr fl::size_t kFledHeaderBytes = 12;
// Defensive cap on the embedded JSON to bound the worst-case heap
// allocation if a malformed file claims a giant json_length. Real
// screenmaps run ~500 B – ~50 KB; 1 MiB is well above that.
constexpr fl::size_t kFledMaxJsonBytes = 1u * 1024u * 1024u;
} // namespace

PixelStream::PixelStream(int bytes_per_frame)
    : mbytesPerFrame(bytes_per_frame), mType(kFile) {}

PixelStream::~PixelStream() FL_NOEXCEPT { close(); }

bool PixelStream::begin(filebuf_ptr h) {
    close();
    mHandle = h;
    mPayloadOffset = 0;
    mEmbeddedScreenMapJson.clear();
    // Probe seekability: if seek-to-start succeeds, this is a seekable file.
    mType = mHandle->seek(0, seek_dir::beg) ? kFile : kStreaming;
    if (mType == kFile) {
        // Try to detect a FLED v1 container. Anything that fails the header
        // check (wrong magic, wrong version, unsupported pixel_format,
        // truncated) falls back to legacy headerless RGB — zero behavior
        // change for existing files.
        if (mHandle->size() >= kFledHeaderBytes) {
            char hdr[kFledHeaderBytes];
            fl::size_t got = mHandle->read(hdr, kFledHeaderBytes);
            bool isFled = got == kFledHeaderBytes
                && static_cast<fl::u8>(hdr[0]) == kFledMagic[0]
                && static_cast<fl::u8>(hdr[1]) == kFledMagic[1]
                && static_cast<fl::u8>(hdr[2]) == kFledMagic[2]
                && static_cast<fl::u8>(hdr[3]) == kFledMagic[3]
                && static_cast<fl::u8>(hdr[4]) == kFledVersionV1
                && static_cast<fl::u8>(hdr[5]) == kFledPixelFormatRgb8;
            if (isFled) {
                const fl::u32 jsonLen =
                    static_cast<fl::u32>(static_cast<fl::u8>(hdr[8]))
                    | (static_cast<fl::u32>(static_cast<fl::u8>(hdr[9])) << 8)
                    | (static_cast<fl::u32>(static_cast<fl::u8>(hdr[10])) << 16)
                    | (static_cast<fl::u32>(static_cast<fl::u8>(hdr[11])) << 24);
                // Cap the JSON length against a defensive maximum BEFORE
                // doing any size arithmetic — guards against a malformed
                // file declaring a multi-gigabyte json_length that would
                // either overflow the offset calc or trigger a huge resize.
                const fl::size_t jsonLenSz = static_cast<fl::size_t>(jsonLen);
                const fl::size_t fileSize = mHandle->size();
                const bool jsonInRange = jsonLenSz <= kFledMaxJsonBytes
                    && jsonLenSz <= fileSize - kFledHeaderBytes;
                if (jsonInRange) {
                    mEmbeddedScreenMapJson.resize(static_cast<fl::size>(jsonLenSz));
                    fl::size_t jr = jsonLenSz > 0
                        ? mHandle->read(&mEmbeddedScreenMapJson[0], jsonLenSz)
                        : 0;
                    if (jr == jsonLenSz) {
                        mPayloadOffset = kFledHeaderBytes + jsonLenSz;
                        // Stream is now positioned at the first frame byte.
                        return mHandle->available();
                    }
                    // JSON slurp short-read — abandon FLED interpretation.
                    mEmbeddedScreenMapJson.clear();
                }
            }
            // Not a FLED file (or header rejected). Rewind so subsequent
            // reads see the file from byte 0 as raw RGB triplets.
            mHandle->seek(0, seek_dir::beg);
        }
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
    // Use size_t throughout so frameNumber * bytesPerFrame doesn't overflow
    // u32 for high-LED-count grids past ~1M frames.
    fl::size_t total_bytes = mHandle->size();
    fl::size_t frameBytes = static_cast<fl::size_t>(frameNumber)
        * static_cast<fl::size_t>(mbytesPerFrame);
    fl::size_t target = mPayloadOffset + frameBytes;
    return target < total_bytes;
}

bool PixelStream::readFrameAt(fl::u32 frameNumber, Frame *frame) {
    if (mType == kStreaming) {
        // Streaming handle doesn't support seeking
        FL_DBG("Streaming handle doesn't support seeking");
        return false;
    }
    fl::size_t frameBytes = static_cast<fl::size_t>(frameNumber)
        * static_cast<fl::size_t>(mbytesPerFrame);
    mHandle->seek(mPayloadOffset + frameBytes);
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
    fl::size_t pos = mHandle->pos();
    if (pos < mPayloadOffset) return 0;
    return static_cast<i32>((pos - mPayloadOffset) / mbytesPerFrame);
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
    // Rewind to the start of the payload, not the start of the file —
    // skips the FLED header on container-formatted streams.
    mHandle->seek(mPayloadOffset);
    return true;
}

PixelStream::Type PixelStream::getType() const {
    return mType;
}

bool PixelStream::hasEmbeddedScreenMap() const FL_NOEXCEPT {
    return !mEmbeddedScreenMapJson.empty();
}

const fl::string &PixelStream::embeddedScreenMapJson() const FL_NOEXCEPT {
    return mEmbeddedScreenMapJson;
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

} // namespace video
} // namespace fl
