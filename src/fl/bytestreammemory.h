#pragma once

#include "fl/stdint.h"

#include "fl/namespace.h"
#include "fl/ptr.h"

#include "fl/bytestream.h"
#include "fl/circular_buffer.h"
#include "fl/int.h"

namespace fl {

FASTLED_SMART_PTR(ByteStreamMemory);

class ByteStreamMemory : public ByteStream {
  public:
    ByteStreamMemory(fl::u32 size_buffer);
    ~ByteStreamMemory() override;
    bool available(fl::sz n) const override;
    fl::sz read(fl::u8 *dst, fl::sz bytesToRead) override;
    void clear() { mReadBuffer.clear(); }
    const char *path() const override { return "ByteStreamMemory"; }
    fl::sz write(const fl::u8 *src, fl::sz n);
    fl::sz writeCRGB(const CRGB *src, fl::sz n);

  private:
    CircularBuffer<fl::u8> mReadBuffer;
};

} // namespace fl
