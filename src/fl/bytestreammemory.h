#pragma once

#include <stddef.h>
#include <stdint.h>

#include "fl/namespace.h"
#include "fl/ptr.h"

#include "fl/bytestream.h"
#include "fl/circular_buffer.h"

namespace fl {

FASTLED_SMART_PTR(ByteStreamMemory);

class ByteStreamMemory : public ByteStream {
  public:
    ByteStreamMemory(uint32_t size_buffer);
    ~ByteStreamMemory() override;
    bool available(size_t n) const override;
    size_t read(uint8_t *dst, size_t bytesToRead) override;
    void clear() {
        mReadBuffer.clear();
    }
    const char *path() const override { return "ByteStreamMemory"; }
    size_t write(const uint8_t* src, size_t n);
    size_t writeCRGB(const CRGB* src, size_t n);

  private:
    CircularBuffer<uint8_t> mReadBuffer;
};


}  // namespace fl
