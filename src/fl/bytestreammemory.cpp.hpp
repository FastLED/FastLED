#include <string.h>
#include "fl/int.h"

#include "fl/bytestreammemory.h"
#include "fl/math_macros.h"
#include "fl/namespace.h"
#include "fl/warn.h"

namespace fl {

ByteStreamMemory::ByteStreamMemory(fl::u32 size_buffer)
    : mReadBuffer(size_buffer) {}

ByteStreamMemory::~ByteStreamMemory() = default;

bool ByteStreamMemory::available(fl::sz n) const {
    return mReadBuffer.size() >= n;
}

fl::sz ByteStreamMemory::read(fl::u8 *dst, fl::sz bytesToRead) {
    if (!available(bytesToRead) || dst == nullptr) {
        FASTLED_WARN("ByteStreamMemory::read: !available(bytesToRead): "
                     << bytesToRead
                     << " mReadBuffer.size(): " << mReadBuffer.size());
        return 0;
    }

    fl::sz actualBytesToRead = MIN(bytesToRead, mReadBuffer.size());
    fl::sz bytesRead = 0;

    while (bytesRead < actualBytesToRead) {
        fl::u8 &b = dst[bytesRead];
        mReadBuffer.pop_front(&b);
        bytesRead++;
    }

    if (bytesRead == 0) {
        FASTLED_WARN("ByteStreamMemory::read: bytesRead == 0");
    }

    return bytesRead;
}

fl::sz ByteStreamMemory::write(const fl::u8 *src, fl::sz n) {
    if (src == nullptr || mReadBuffer.capacity() == 0) {
        FASTLED_WARN_IF(src == nullptr,
                        "ByteStreamMemory::write: src == nullptr");
        FASTLED_WARN_IF(mReadBuffer.capacity() == 0,
                        "ByteStreamMemory::write: mReadBuffer.capacity() == 0");
        return 0;
    }

    fl::sz written = 0;
    for (fl::sz i = 0; i < n; ++i) {
        if (mReadBuffer.full()) {
            FASTLED_WARN("ByteStreamMemory::write: mReadBuffer.full(): "
                         << mReadBuffer.size());
            break;
        }
        mReadBuffer.push_back(src[i]);
        ++written;
    }
    return written;
}

fl::sz ByteStreamMemory::writeCRGB(const CRGB *src, fl::sz n) {
    fl::sz bytes_written = write(reinterpret_cast<const fl::u8 *>(src), n * 3);
    fl::sz pixels_written = bytes_written / 3;
    return pixels_written;
}

} // namespace fl
