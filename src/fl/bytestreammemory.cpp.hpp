#include "fl/int.h"

#include "fl/bytestreammemory.h"
#include "fl/stl/bit_cast.h"
#include "fl/math_macros.h"
#include "fl/warn.h"

namespace fl {

ByteStreamMemory::ByteStreamMemory(fl::u32 size_buffer)
    : mReadBuffer(size_buffer) {}

ByteStreamMemory::~ByteStreamMemory() = default;

bool ByteStreamMemory::available(fl::size n) const {
    return mReadBuffer.size() >= n;
}

fl::size ByteStreamMemory::read(fl::u8 *dst, fl::size bytesToRead) {
    if (dst == nullptr) {
        FASTLED_WARN("ByteStreamMemory::read: dst == nullptr");
        return 0;
    }

    // Read up to the requested amount, whatever is available
    fl::size actualBytesToRead = FL_MIN(bytesToRead, mReadBuffer.size());
    fl::size bytesRead = 0;

    while (bytesRead < actualBytesToRead) {
        fl::u8 &b = dst[bytesRead];
        mReadBuffer.pop_front(&b);
        bytesRead++;
    }

    return bytesRead;
}

fl::size ByteStreamMemory::write(const fl::u8 *src, fl::size n) {
    if (src == nullptr || mReadBuffer.capacity() == 0) {
        FASTLED_WARN_IF(src == nullptr,
                        "ByteStreamMemory::write: src == nullptr");
        FASTLED_WARN_IF(mReadBuffer.capacity() == 0,
                        "ByteStreamMemory::write: mReadBuffer.capacity() == 0");
        return 0;
    }

    fl::size written = 0;
    for (fl::size i = 0; i < n; ++i) {
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

fl::size ByteStreamMemory::writeCRGB(const CRGB *src, fl::size n) {
    fl::size bytes_written = write(fl::bit_cast<const fl::u8 *>(src), n * 3);
    fl::size pixels_written = bytes_written / 3;
    return pixels_written;
}

} // namespace fl
