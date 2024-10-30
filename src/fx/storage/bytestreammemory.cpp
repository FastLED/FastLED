#include <string.h>

#include "bytestreammemory.h"

#include "math_macros.h"

FASTLED_NAMESPACE_BEGIN

ByteStreamMemory::ByteStreamMemory(uint32_t size_buffer)
    : mBuffer(size_buffer) {}

ByteStreamMemory::~ByteStreamMemory() = default;

bool ByteStreamMemory::available(size_t n) const {
    return mBuffer.size() >= n;
}

size_t ByteStreamMemory::read(uint8_t *dst, size_t bytesToRead) {
    if (!available(bytesToRead) || dst == nullptr) {
        return 0;
    }

    size_t actualBytesToRead = MIN(bytesToRead, mBuffer.size());
    size_t bytesRead = 0;

    while (bytesRead < actualBytesToRead) {
        mBuffer.pop_front(&dst[bytesRead]);
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
