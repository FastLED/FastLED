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
    bool available(size_t n) const override;
    size_t read(fl::u8 *dst, size_t bytesToRead) override;
    void clear() { mReadBuffer.clear(); }
    const char *path() const override { return "ByteStreamMemory"; }
    size_t write(const fl::u8 *src, size_t n);
    size_t writeCRGB(const CRGB *src, size_t n);

  private:
    CircularBuffer<fl::u8> mReadBuffer;
};

} // namespace fl
