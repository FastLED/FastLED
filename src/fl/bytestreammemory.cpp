#include <string.h>

#include "fl/bytestreammemory.h"

#include "fl/math_macros.h"

#include "namespace.h"
#include "fl/dbg.h"

#define DBG FASTLED_DBG

namespace fl {

ByteStreamMemory::ByteStreamMemory(uint32_t size_buffer)
    : mReadBuffer(size_buffer) {}

ByteStreamMemory::~ByteStreamMemory() = default;

bool ByteStreamMemory::available(size_t n) const {
    return mReadBuffer.size() >= n;
}

size_t ByteStreamMemory::read(uint8_t *dst, size_t bytesToRead) {
    if (!available(bytesToRead) || dst == nullptr) {
        DBG("ByteStreamMemory::read: !available(bytesToRead): " << bytesToRead << " mReadBuffer.size(): " << mReadBuffer.size());
        return 0;
    }

    size_t actualBytesToRead = MIN(bytesToRead, mReadBuffer.size());
    size_t bytesRead = 0;

    while (bytesRead < actualBytesToRead) {
        uint8_t& b = dst[bytesRead];
        mReadBuffer.pop_front(&b);
        bytesRead++;
    }

    if (bytesRead == 0) {
        DBG("ByteStreamMemory::read: bytesRead == 0");
    }

    return bytesRead;
}

size_t ByteStreamMemory::write(const uint8_t* src, size_t n) {
    if (src == nullptr || mReadBuffer.capacity() == 0) {
        FASTLED_DBG_IF(src == nullptr, "ByteStreamMemory::write: src == nullptr");
        FASTLED_DBG_IF(mReadBuffer.capacity() == 0, "ByteStreamMemory::write: mReadBuffer.capacity() == 0");
        return 0;
    }

    size_t written = 0;
    for (size_t i = 0; i < n; ++i) {
        if (mReadBuffer.full()) {
            FASTLED_DBG("ByteStreamMemory::write: mReadBuffer.full(): " << mReadBuffer.size());
            break;
        }
        mReadBuffer.push_back(src[i]);
        ++written;
    }
    return written;
}

size_t ByteStreamMemory::writeCRGB(const CRGB* src, size_t n) {
    size_t bytes_written = write(reinterpret_cast<const uint8_t*>(src), n * 3);
    size_t pixels_written = bytes_written / 3;
    return pixels_written;
}

}  // namespace fl
