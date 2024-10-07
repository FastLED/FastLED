#include "bytestreammemory.h"
#include <string.h>
#include "fl_math.h"

FASTLED_NAMESPACE_BEGIN

ByteStreamMemory::ByteStreamMemory(uint32_t size_buffer)
    : mBuffer(size_buffer) {}

ByteStreamMemory::~ByteStreamMemory() = default;

bool ByteStreamMemory::available() const {
    return !mBuffer.empty();
}

size_t ByteStreamMemory::read(uint8_t *dst, size_t bytesToRead) {
    if (!available() || dst == nullptr) {
        return 0;
    }

    size_t actualBytesToRead = min(bytesToRead, mBuffer.size());
    size_t bytesRead = 0;

    while (bytesRead < actualBytesToRead) {
        dst[bytesRead] = mBuffer.pop_front();
        bytesRead++;
    }

    return bytesRead;
}

size_t ByteStreamMemory::write(const uint8_t* src, size_t n) {
    if (src == nullptr || mBuffer.capacity() == 0) {
        return 0;
    }

    size_t written = 0;
    for (size_t i = 0; i < n; ++i) {
        if (mBuffer.full()) {
            break;
        }
        mBuffer.push_back(src[i]);
        ++written;
    }
    return written;
}

FASTLED_NAMESPACE_END
