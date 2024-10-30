#pragma once

#include <stddef.h>
#include <stdint.h>

#include "namespace.h"
#include "ref.h"

#include "bytestream.h"
#include "fx/detail/circular_buffer.h"

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_REF(ByteStreamMemory);

class ByteStreamMemory : public ByteStream {
  public:
    ByteStreamMemory(uint32_t size_buffer);
    ~ByteStreamMemory() override;
    bool available(size_t n) const override;
    size_t read(uint8_t *dst, size_t bytesToRead) override;
    void clear() {
        mBuffer.clear();
    }
    const char *path() const override { return "ByteStreamMemory"; }
    size_t write(const uint8_t* src, size_t n);

  private:
    CircularBuffer<uint8_t> mBuffer;
};

FASTLED_NAMESPACE_END
