#pragma once

#include "fl/stdint.h"

#include "fl/namespace.h"
#include "fl/memory.h"

#include "fl/bytestream.h"
#include "fl/circular_buffer.h"
#include "fl/int.h"

namespace fl {

FASTLED_SMART_PTR(ByteStreamMemory);

class ByteStreamMemory : public ByteStream {
  public:
    ByteStreamMemory(fl::u32 size_buffer);
    ~ByteStreamMemory() override;
    bool available(fl::size n) const override;
    fl::size read(fl::u8 *dst, fl::size bytesToRead) override;
    void clear() { mReadBuffer.clear(); }
    const char *path() const override { return "ByteStreamMemory"; }
    fl::size write(const fl::u8 *src, fl::size n);
    fl::size writeCRGB(const CRGB *src, fl::size n);

  private:
    CircularBuffer<fl::u8> mReadBuffer;
};

} // namespace fl
