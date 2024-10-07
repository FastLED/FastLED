#include "bytestreammemory.h"
#include <string.h>
#include "crgb.h"

FASTLED_NAMESPACE_BEGIN

ByteStreamMemory::ByteStreamMemory(uint32_t size_buffer)
    : mBuffer(new uint8_t[size_buffer]), mSize(size_buffer), mPosition(0) {}

ByteStreamMemory::~ByteStreamMemory() {
    delete[] mBuffer;
}

bool ByteStreamMemory::available() const {
    return mPosition < mSize;
}

size_t ByteStreamMemory::read(uint8_t *dst, size_t bytesToRead) {
    if (!available()) {
        return 0;
    }

    size_t remainingBytes = mSize - mPosition;
    size_t actualBytesToRead = (bytesToRead < remainingBytes) ? bytesToRead : remainingBytes;

    memcpy(dst, mBuffer + mPosition, actualBytesToRead);
    mPosition += actualBytesToRead;

    return actualBytesToRead;
}


FASTLED_NAMESPACE_END
